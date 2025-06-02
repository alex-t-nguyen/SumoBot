#include "app/line.h"
#include "drivers/qre1113.h"
#include "common/assert_handler.h"
#include <stdbool.h>

#define LINE_DETECTED_VOLTAGE_THRESHOLD (700U);
static bool initialized = false;

void line_init(void) {
    ASSERT(!initialized);
    qre1113_init();
    initialized = true;
}

e__line line_get(void) {
    struct qre1113_voltages voltages;
    qre1113_get_voltages(&voltages);
    
    // Constants to indicate if line was detected by corresponding qre1113 sensor
    const bool front_left_detected = voltages.front_left < LINE_DETECTED_VOLTAGE_THRESHOLD;
    const bool front_right_detected = voltages.front_right < LINE_DETECTED_VOLTAGE_THRESHOLD;
    const bool back_left_detected = voltages.back_left < LINE_DETECTED_VOLTAGE_THRESHOLD;
    const bool back_right_detected = voltages.back_right < LINE_DETECTED_VOLTAGE_THRESHOLD;

    if (front_left_detected) {
        if (front_right_detected) {
            return LINE_FRONT;
        } else if (back_left_detected) {
            return LINE_LEFT;
        } else if (back_right_detected) {
            return LINE_DIAGONAL_LEFT;
        } else {
            return LINE_FRONT_LEFT;
        }
    } else if (front_right_detected) {
        if (back_right_detected) {
            return LINE_RIGHT;
        } else if (back_left_detected) {
            return LINE_DIAGONAL_RIGHT;
        } else {
            return LINE_FRONT_RIGHT;
        }
    } else if (back_left_detected) {
        if (back_right_detected) {
            return LINE_BACK;
        } else {
            return LINE_BACK_LEFT;
        }
    } else if (back_right_detected) {
            return LINE_BACK_RIGHT; 
    } else {
            return LINE_NONE;
    }
}
