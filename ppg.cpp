// PPG.cpp
#include "PPG.h"

LOG_MODULE_REGISTER(MAXM86161, LOG_LEVEL_INF);

// I2C device for MAXM86161
static const struct device *const I2C2_DEV =
    DEVICE_DT_GET(DT_NODELABEL(i2c2));

PPG PPG::sensor;
MAXM86161 PPG::ppg(I2C2_DEV);

struct k_msgq *PPG::sensor_queue = nullptr;
struct k_work  PPG::sensor_work;
struct k_timer PPG::sensor_timer;

static struct sensor_msg msg_ppg;

const SampleRateSetting<16> PPG::sample_rates = {
    // reg_vals
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x0A, 0x0B,
      0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13 },
    // sample_rates (nominal)
    { 25, 50, 84, 100, 200, 400, 8, 16,
      32, 64, 128, 256, 512, 1024, 2048, 4096 },
    // true_sample_rates
    { 24.995, 50.027, 84.021, 99.902, 199.805, 399.610, 8.000, 16.000,
      32.000, 64.000, 128.000, 256.000, 512.000, 1024.000, 2048.000, 4096.000 }
};

bool PPG::init(struct k_msgq *queue)
{
    if (_active) {
        return true;
    }

    if (!device_is_ready(I2C2_DEV)) {
        LOG_ERR("I2C2 not ready");
        return false;
    }

    // can use to enable LDO via GPIO if needed
    const struct gpio_dt_spec LDO_EN = {
        .port = DEVICE_DT_GET(DT_NODELABEL(gpio0)),
        .pin = 6,
        .dt_flags = GPIO_OUTPUT_ACTIVE
    };

    if (device_is_ready(LDO_EN.port)) {
        int ret = gpio_pin_configure_dt(&LDO_EN, GPIO_OUTPUT_ACTIVE);
        if (ret != 0) {
            LOG_WRN("Failed to configure LDO_EN");
        }
        k_msleep(5);
    }

    int ret = ppg.init();
    if (ret != 0) {
        LOG_WRN("Could not init MAXM86161, ret=%d", ret);
        _active = false;
        return false;
    }

    sensor_queue = queue;

    k_work_init(&sensor_work, update_sensor);
    k_timer_init(&sensor_timer, sensor_timer_handler, NULL);

    _active = true;
    return true;
}

void PPG::sensor_timer_handler(struct k_timer *dummy)
{
    ARG_UNUSED(dummy);
    k_work_submit(&sensor_work);
}

void PPG::update_sensor(struct k_work *work)
{
    ARG_UNUSED(work);

    int int_status = 0;
    int status;

    uint64_t _time_stamp = micros();

    PPG::sensor._sample_count +=
        (_time_stamp - PPG::sensor._last_time_stamp) / PPG::sensor.t_sample_us;
    PPG::sensor._last_time_stamp = _time_stamp;

    if (PPG::sensor._sample_count <
        PPG::sensor._num_samples_buffered *
        (1.f - CONFIG_SENSOR_CLOCK_ACCURACY / 100.f)) {
        return;
    }

    status = ppg.read_interrupt_state(int_status);
    if (status != 0) {
        LOG_ERR("PPG read interrupt state failed: %d", status);
        return;
    }

    if (int_status & 0x02) { // FIFO full
        int num_samples = ppg.read(sensor.data_buffer);

        PPG::sensor._sample_count =
            MAX(0, PPG::sensor._num_samples_buffered - num_samples);

        int written = 0;
        const int _size = 4 * sizeof(uint32_t); // red, ir, green, ambient

        while (written < num_samples) {
            int to_write =
                MIN((SENSOR_DATA_FIXED_LENGTH - sizeof(uint16_t)) / _size,
                    num_samples - written);
            if (to_write <= 0) break;

            msg_ppg.sd     = PPG::sensor._sd_logging;
            msg_ppg.stream = PPG::sensor._ble_stream;

            msg_ppg.data.id   = ID_PPG;
            msg_ppg.data.size = to_write * _size + sizeof(uint16_t);
            msg_ppg.data.time =
                _time_stamp - (num_samples - written) * PPG::sensor.t_sample_us;

            if (to_write > 1) {
                uint16_t t_diff = (uint16_t)PPG::sensor.t_sample_us;
                for (int i = 0; i < to_write; i++) {
                    memcpy(&msg_ppg.data.data[i * _size],
                           &sensor.data_buffer[written + i],
                           _size);
                }
                memcpy(&msg_ppg.data.data[msg_ppg.data.size - sizeof(uint16_t)],
                       &t_diff, sizeof(uint16_t));
            } else {
                memcpy(&msg_ppg.data.data,
                       &sensor.data_buffer[written],
                       _size);
            }

            if (sensor_queue) {
                int ret = k_msgq_put(sensor_queue, &msg_ppg, K_NO_WAIT);
                if (ret) {
                    LOG_WRN("sensor msg queue full");
                }
            }

            written += to_write;
        }
    }
}

void PPG::start(int sample_rate_idx)
{
    if (!_active) return;

    t_sample_us = 1e6f / sample_rates.true_sample_rates[sample_rate_idx];

    k_timeout_t t = K_USEC((uint32_t)t_sample_us);

    _num_samples_buffered =
        MIN(MAX(1, (int)(CONFIG_SENSOR_LATENCY_MS * 1e3f / t_sample_us)),
            FIFO_SIZE / LED_NUM - 2);

    ppg.set_interrogation_rate(sample_rates.reg_vals[sample_rate_idx]);
    ppg.set_watermark(FIFO_SIZE - _num_samples_buffered * LED_NUM);
    ppg.start();

    k_timer_start(&sensor_timer, K_NO_WAIT, t);

    _running = true;
    _sample_count = 0;
    _last_time_stamp = micros();
}

void PPG::stop()
{
    if (!_active) return;

    _running = false;

    k_timer_stop(&sensor_timer);
    ppg.stop();

    _active = false;
}