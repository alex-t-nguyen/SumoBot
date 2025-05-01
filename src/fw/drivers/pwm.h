#include <stdint.h>

// Driver that emulates hardware PWM with timers
typedef enum {
    DRV8848_RIGHT1,
    DRV8848_RIGHT2,
    DRV8848_LEFT1,
    DRV8848_LEFT2
} mdrv_enum;

void pwm_init(void);
void pwm_set_duty_cycle(mdrv_enum, uint8_t);
