#ifndef PPG_TYPES_H
#define PPG_TYPES_H

#include <zephyr/kernel.h>
#include <stdint.h>

#define ID_PPG                     0x01

// Tune these
#define FIFO_SIZE                  32      // MAXM86161 FIFO depth in samples
#define LED_NUM                    4       // red, ir, green, ambient
#define SENSOR_DATA_FIXED_LENGTH   128     // bytes available for payload
#define CONFIG_SENSOR_LATENCY_MS   40
#define CONFIG_SENSOR_CLOCK_ACCURACY 1.0f  // percent

#ifndef MIN
#define MIN(a,b) (( (a) < (b) ) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) (( (a) > (b) ) ? (a) : (b))
#endif

// One PPG sample: red, ir, green, ambient
typedef struct {
    uint32_t red;
    uint32_t ir;
    uint32_t green;
    uint32_t ambient;
} ppg_sample;

// Message format pushed into k_msgq
struct sensor_msg {
    bool sd;
    bool stream;
    struct {
        uint8_t  id;
        uint16_t size;
        uint64_t time;
        uint8_t  data[SENSOR_DATA_FIXED_LENGTH];
    } data;
};

static inline uint64_t micros(void)
{
    return k_ticks_to_us_floor64(k_uptime_ticks());
}

#endif // PPG_TYPES_H
