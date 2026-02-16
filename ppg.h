// PPG.h
#ifndef PPG_H
#define PPG_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <array>
#include "ppg_types.h"
#include "MAXM86161.h"

LOG_MODULE_DECLARE(MAXM86161, LOG_LEVEL_INF);

enum led_order {
    red, green, ir, ambient
};

template <size_t N>
struct SampleRateSetting {
    uint8_t reg_vals[N];
    float   sample_rates[N];
    float   true_sample_rates[N];
};

class PPG {
public:
    static PPG sensor;

    bool init(struct k_msgq *queue);
    void start(int sample_rate_idx);
    void stop();

    static const SampleRateSetting<16> sample_rates;

private:
    static MAXM86161 ppg;

    static void sensor_timer_handler(struct k_timer *dummy);
    static void update_sensor(struct k_work *work);

    ppg_sample data_buffer[64];

    float    t_sample_us = 0.0f;
    bool     _active = false;
    bool     _running = false;

    int      _num_samples_buffered = 0;
    float    _sample_count = 0;
    uint64_t _last_time_stamp = 0;

    bool     _sd_logging = false;
    bool     _ble_stream = true;

    static struct k_msgq *sensor_queue;
    static struct k_work  sensor_work;
    static struct k_timer sensor_timer;
};

#endif // PPG_H