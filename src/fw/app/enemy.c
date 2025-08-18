#include "app/enemy.h"
#include "drivers/vl53l0x.h"
#include "common/assert_handler.h"
#include "common/trace.h"

static bool initialized = false;

#define RANGE_DETECT_THRESHOLD 600U // In millimeter
#define INVALID_RANGE (UINT16_MAX)
#define RANGE_CLOSE (100U) // In millimeter
#define RANGE_MID (200U) // In millimeter
#define RANGE_FAR (300U)

const char *enemy_pos_str(e__enemy_pos pos) {
    switch (pos) {
            case ENEMY_POS_NONE:
                    return "NONE";
            case ENEMY_POS_FRONT_MIDDLE:
                    return "FRONT MIDDLE";
            case ENEMY_POS_FRONT_LEFT:
                    return "FRONT LEFT";
            case ENEMY_POS_FRONT_RIGHT:
                    return "FRONT RIGHT";
            case ENEMY_POS_FRONT_MIDDLE_AND_FRONT_LEFT:
                    return "FRONT LEFT";
            case ENEMY_POS_FRONT_MIDDLE_AND_FRONT_RIGHT:
                    return "FRONT RIGHT";
            case ENEMY_POS_FRONT_ALL:
                    return "FRONT ALL";
            case ENEMY_POS_IMPOSSIBLE:
                    return "IMPOSSIBLE";
    }
    return "No valid position";
}

const char *enemy_range_str(e__enemy_range range) {
    switch (range) {
            case ENEMY_RANGE_NONE:
                    return "NONE";
            case ENEMY_RANGE_CLOSE:
                    return "CLOSE";
            case ENEMY_RANGE_MID:
                    return "MID";
            case ENEMY_RANGE_FAR:
                    return "FAR";
    }
    return "No valid range";
}
struct enemy enemy_get(void) {
    struct enemy enemy = {ENEMY_POS_NONE, ENEMY_RANGE_NONE};
    #ifdef SINGLE_RANGE
        // LAUNCHPAD only using 1 range sensor
        uint16_t range = INVALID_RANGE;
        e__vl53l0x_result vl53l0x_result = vl53l0x_read_range_single(e_VL53L0X_POS_FRONT, &range);
        
        if (vl53l0x_result != e_VL53L0X_RESULT_OK) {
            TRACE("Read range multiple failed");
            return enemy;
        }

        const uint16_t range_front_middle = range;
        
        const bool front_middle = range_front_middle < RANGE_DETECT_THRESHOLD;
        // Assign enemy position and calculate range
        if (front_middle) {
            enemy.position = ENEMY_POS_FRONT_MIDDLE;
            range = range_front_middle;
        } else {
            enemy.position = ENEMY_POS_NONE;
        }
        // Assign enemy range
        if (range < RANGE_CLOSE) {
            enemy.range = ENEMY_RANGE_CLOSE;
        } else if (range < RANGE_MID) {
            enemy.range = ENEMY_RANGE_MID;
        } else {
            enemy.range = ENEMY_RANGE_FAR;
        }
    #elif MULTI_RANGE
        // SUMOBOT uses all 3 range sensors
        t__vl53l0x_ranges ranges;
        bool fresh_values = false;
        e__vl53l0x_result vl53l0x_result = vl53l0x_read_range_multiple(ranges, &fresh_values);
            
        if (vl53l0x_result != e_VL53L0X_RESULT_OK) {
            TRACE("Read range multiple failed");
            return enemy;
        }
        const uint16_t range_front_middle = ranges[e_VL53L0X_POS_FRONT];
        const uint16_t range_front_left = ranges[e_VL53L0X_POS_FRONT_LEFT];
        const uint16_t range_front_right = ranges[e_VL53L0X_POS_FRONT_RIGHT];

        const bool front_middle = range_front_middle < RANGE_DETECT_THRESHOLD;
        const bool front_left = range_front_left < RANGE_DETECT_THRESHOLD;
        const bool front_right = range_front_right < RANGE_DETECT_THRESHOLD;
        
        // Assign enemy position and calculate range
        uint16_t range = INVALID_RANGE; 
        if (front_middle && front_left && front_right) {
                enemy.position = ENEMY_POS_FRONT_ALL;
                // Average the range measurement
                range = ((((range_front_left + range_front_middle) / 2) + range_front_right) / 2);
        } else if (front_left && front_right) {
            enemy.position = ENEMY_POS_IMPOSSIBLE;
        } else if (front_left) {
            if (front_middle) {
                enemy.position = ENEMY_POS_FRONT_MIDDLE_AND_FRONT_LEFT;
                range = (range_front_left + range_front_middle) / 2;
            } else {
                enemy.position = ENEMY_POS_FRONT_LEFT;
                range = range_front_left;
            }
        } else if (front_right) {
            if (front_middle) {
                enemy.position = ENEMY_POS_FRONT_MIDDLE_AND_FRONT_RIGHT;
                range = (range_front_right + range_front_middle) / 2;
            } else {
                enemy.position = ENEMY_POS_FRONT_RIGHT;
                range = range_front_right;
            }
        } else if (front_middle) {
            enemy.position = ENEMY_POS_FRONT_MIDDLE;
            range = range_front_middle;
        } else {
            enemy.position = ENEMY_POS_NONE;
        }
        // Assign enemy range
        if (range == INVALID_RANGE)
                return enemy;
        if (range < RANGE_CLOSE) {
            enemy.range = ENEMY_RANGE_CLOSE;
        } else if (range < RANGE_MID) {
            enemy.range = ENEMY_RANGE_MID;
        } else {
            enemy.range = ENEMY_RANGE_FAR;
        }
    #endif
    return enemy;
}

bool enemy_detected(const struct enemy *enemy) {
    return enemy->position != ENEMY_POS_NONE && enemy->position != ENEMY_POS_IMPOSSIBLE;
}

bool enemy_at_left(const struct enemy *enemy) {
    return enemy->position == ENEMY_POS_FRONT_LEFT || enemy->position == ENEMY_POS_FRONT_MIDDLE_AND_FRONT_LEFT;
}

bool enemy_at_right(const struct enemy *enemy) {
    return enemy->position == ENEMY_POS_FRONT_RIGHT || enemy->position == ENEMY_POS_FRONT_MIDDLE_AND_FRONT_RIGHT;
}

bool enemy_at_front_middle(const struct enemy *enemy) {
    return enemy->position == ENEMY_POS_FRONT_MIDDLE || enemy->position == ENEMY_POS_FRONT_ALL;
}

void enemy_init(void) {
    ASSERT(!initialized);
    e__vl53l0x_result vl53l0x_result = vl53l0x_init();
    if (vl53l0x_result != e_VL53L0X_RESULT_OK) {
            TRACE("Failed to initialize VL53L0X");
            return;
    }
    initialized = true; 
}
