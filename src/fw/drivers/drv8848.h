#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MOTORS_RIGHT,
    MOTORS_LEFT
} motor_side_enum;

typedef enum {
    DRV8848_RIGHT1,
    DRV8848_RIGHT2,
    DRV8848_LEFT1,
    DRV8848_LEFT2
} drv8848_enum;

typedef enum {
    DRV8848_MODE_COAST,
    DRV8848_MODE_FORWARD, 
    DRV8848_MODE_REVERSE,
    DRV8848_MODE_STOP
} drv8848_mode_enum;

void drv8848_init(void);
void drv8848_set_mode(motor_side_enum side, drv8848_mode_enum mode, uint8_t duty_cycle);
void drv8848_set_pwm(drv8848_enum device, uint8_t duty_cycle);
void drv8848_enable(bool enable);
void drv8848_check_nfault(motor_side_enum side);
