#include <zephyr/kernel.h>
#include "PPG.h"
#include "ppg_types.h"

K_MSGQ_DEFINE(ppg_queue, sizeof(struct sensor_msg), 16, 4);

void main(void)
{
    if (!PPG::sensor.init(&ppg_queue)) {
        printk("PPG init failed\n");
        return;
    }

    // e.g. 100 Hz index in table
    PPG::sensor.start(3);

    while (1) {
        struct sensor_msg msg;
        if (k_msgq_get(&ppg_queue, &msg, K_FOREVER) == 0) {
            if (msg.data.id == ID_PPG) {
                // Parse msg.data.data into ppg_sample(s)
                // First sample:
                ppg_sample s;
                memcpy(&s, msg.data.data, sizeof(ppg_sample));
                // Do something with s.red, s.ir, s.green, s.ambient
            }
        }
    }
}