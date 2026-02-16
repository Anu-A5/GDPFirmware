// MAXM86161.cpp
#include "MAXM86161.h"

// NOTE: reg addr's are placeholders
// Need to check the MAXM86161 datasheet and adjust
static constexpr uint8_t REG_INT_STATUS   = 0x00;
static constexpr uint8_t REG_FIFO_DATA    = 0x07;
static constexpr uint8_t REG_MODE_CFG     = 0x09;
static constexpr uint8_t REG_SPO2_CFG     = 0x0A;
static constexpr uint8_t REG_FIFO_CFG     = 0x08;
static constexpr uint8_t REG_INT_ENABLE   = 0x02;

static constexpr uint8_t INT_FULL_MASK    = 0x02; // example

int MAXM86161::write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_write(i2c, buf, sizeof(buf), addr);
}

int MAXM86161::read_reg(uint8_t reg, uint8_t &val)
{
    int ret = i2c_write_read(i2c, addr, &reg, 1, &val, 1);
    return ret;
}

int MAXM86161::read_fifo_raw(uint8_t *buf, size_t len)
{
    uint8_t reg = REG_FIFO_DATA;
    return i2c_write_read(i2c, addr, &reg, 1, buf, len);
}

int MAXM86161::init()
{
    if (!device_is_ready(i2c)) {
        return -ENODEV;
    }

    // Reset / basic config as needed
    // TODO: adjust according to datasheet

    // Enable FIFO full interrupt
    int ret = write_reg(REG_INT_ENABLE, INT_FULL_MASK);
    if (ret) return ret;

    return 0;
}

int MAXM86161::set_interrogation_rate(uint8_t reg_val)
{
    // Normally goes into SPO2 or sample rate register
    return write_reg(REG_SPO2_CFG, reg_val);
}

int MAXM86161::set_watermark(uint8_t watermark)
{
    // FIFO threshold level config
    return write_reg(REG_FIFO_CFG, watermark);
}

int MAXM86161::start()
{
    // Set mode to SpO2 / multi-LED mode
    return write_reg(REG_MODE_CFG, 0x03); 
}

int MAXM86161::stop()
{
    // Put into shutdown
    return write_reg(REG_MODE_CFG, 0x00);
}

int MAXM86161::read_interrupt_state(int &int_status)
{
    uint8_t val;
    int ret = read_reg(REG_INT_STATUS, val);
    if (ret) return ret;
    int_status = val;
    return 0;
}

int MAXM86161::read(ppg_sample *buffer)
{
    // Read as many samples as fit in FIFO
    // sample 3 bytes per channel - change?
    // 4 channels * 3 bytes = 12 bytes per sample
    constexpr int BYTES_PER_SAMPLE = 12;
    uint8_t raw[BYTES_PER_SAMPLE];

    // For ease read 1 sample per call.
    int ret = read_fifo_raw(raw, BYTES_PER_SAMPLE);
    if (ret) return ret;

    // Conv 3‑byte values to 24‑bit integers
    auto conv = [](uint8_t *p) -> uint32_t {
        return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    };

    buffer[0].red     = conv(&raw[0]);
    buffer[0].ir      = conv(&raw[3]);
    buffer[0].green   = conv(&raw[6]);
    buffer[0].ambient = conv(&raw[9]);

    return 1; // num samples read
}
