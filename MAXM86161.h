// MAXM86161.h
#ifndef MAXM86161_H
#define MAXM86161_H

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <stdint.h>
#include "ppg_types.h"

class MAXM86161 {
public:
    explicit MAXM86161(const struct device *i2c_dev,
                       uint8_t i2c_addr = 0x62) :
        i2c(i2c_dev), addr(i2c_addr) {}

    int init();
    int set_interrogation_rate(uint8_t reg_val);
    int set_watermark(uint8_t watermark);
    int start();
    int stop();

    int read_interrupt_state(int &int_status);
    int read(ppg_sample *buffer);

private:
    const struct device *i2c;
    uint8_t addr;

    int write_reg(uint8_t reg, uint8_t val);
    int read_reg(uint8_t reg, uint8_t &val);
    int read_fifo_raw(uint8_t *buf, size_t len);
};

#endif // MAXM86161_H
