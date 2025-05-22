#include <stdint.h>

// Driver that emulates hardware PWM with timers
typedef enum {
    PWM_DRV8848_RIGHT1,
    PWM_DRV8848_RIGHT2,
    PWM_DRV8848_LEFT1,
    PWM_DRV8848_LEFT2
} mdrv_enum;

void pwm_init(void);
void pwm_set_duty_cycle(mdrv_enum, uint8_t);
