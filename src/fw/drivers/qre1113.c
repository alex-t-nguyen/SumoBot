#include "drivers/qre1113.h"
#include "common/assert_handler.h"
#include "drivers/adc.h"
#include <stdbool.h>
#include <stdint.h>

static bool initialized = false;

void qre1113_init(void) {
    ASSERT(!initialized);
    adc_init();
    initialized = true;
}

void qre1113_get_voltages(struct qre1113_voltages *buffer) {
    adc_channel_values_t adc_v_buffer; // Buffer for ADC voltage readings
    adc_get_channel_values(adc_v_buffer);
    buffer->front_left = adc_v_buffer[0];
    buffer->front_right = adc_v_buffer[1];
    buffer->back_left = adc_v_buffer[2];
    buffer->back_right = adc_v_buffer[3];
}
