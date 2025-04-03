#ifndef LED_H
typedef enum { TEST_LED } led_enum;

typedef enum { LED_STATE_ON, LED_STATE_OFF } led_state_enum;

void led_init(void);
void set_led(led_enum led, led_state_enum state);
#endif // LED_H
