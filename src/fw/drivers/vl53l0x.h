#include <stdbool.h>
#include <stdint.h>

// ------------------------------------ BEGIN REGISTER DEFINES
// --------------------------------------- //
/* Device Register Map */

/** @defgroup VL53L0X_DefineRegisters_group Define Registers
 *  @brief List of all the defined registers
 *  @{
 */
#define VL53L0X_REG_SYSRANGE_START 0x000
/** mask existing bit in #VL53L0X_REG_SYSRANGE_START*/
#define VL53L0X_REG_SYSRANGE_MODE_MASK 0x0F
/** bit 0 in #VL53L0X_REG_SYSRANGE_START write 1 toggle state in
 * continuous mode and arm next shot in single shot mode */
#define VL53L0X_REG_SYSRANGE_MODE_START_STOP 0x01
/** bit 1 write 0 in #VL53L0X_REG_SYSRANGE_START set single shot mode */
#define VL53L0X_REG_SYSRANGE_MODE_SINGLESHOT 0x00
/** bit 1 write 1 in #VL53L0X_REG_SYSRANGE_START set back-to-back
 *  operation mode */
#define VL53L0X_REG_SYSRANGE_MODE_BACKTOBACK 0x02
/** bit 2 write 1 in #VL53L0X_REG_SYSRANGE_START set timed operation
 *  mode */
#define VL53L0X_REG_SYSRANGE_MODE_TIMED 0x04
/** bit 3 write 1 in #VL53L0X_REG_SYSRANGE_START set histogram operation
 *  mode */
#define VL53L0X_REG_SYSRANGE_MODE_HISTOGRAM 0x08

#define VL53L0X_REG_SYSTEM_THRESH_HIGH 0x000C
#define VL53L0X_REG_SYSTEM_THRESH_LOW 0x000E

#define VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG 0x0001
#define VL53L0X_REG_SYSTEM_RANGE_CONFIG 0x0009
#define VL53L0X_REG_SYSTEM_INTERMEASUREMENT_PERIOD 0x0004

#define VL53L0X_REG_SYSTEM_INTERRUPT_CONFIG_GPIO 0x000A
#define VL53L0X_REG_SYSTEM_INTERRUPT_GPIO_DISABLED 0x00
#define VL53L0X_REG_SYSTEM_INTERRUPT_GPIO_LEVEL_LOW 0x01
#define VL53L0X_REG_SYSTEM_INTERRUPT_GPIO_LEVEL_HIGH 0x02
#define VL53L0X_REG_SYSTEM_INTERRUPT_GPIO_OUT_OF_WINDOW 0x03
#define VL53L0X_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY 0x04

#define VL53L0X_REG_GPIO_HV_MUX_ACTIVE_HIGH 0x0084

#define VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR 0x000B

/* Result registers */
#define VL53L0X_REG_RESULT_INTERRUPT_STATUS 0x0013
#define VL53L0X_REG_RESULT_RANGE_STATUS 0x0014

#define VL53L0X_REG_RESULT_CORE_PAGE 1
#define VL53L0X_REG_RESULT_CORE_AMBIENT_WINDOW_EVENTS_RTN 0x00BC
#define VL53L0X_REG_RESULT_CORE_RANGING_TOTAL_EVENTS_RTN 0x00C0
#define VL53L0X_REG_RESULT_CORE_AMBIENT_WINDOW_EVENTS_REF 0x00D0
#define VL53L0X_REG_RESULT_CORE_RANGING_TOTAL_EVENTS_REF 0x00D4
#define VL53L0X_REG_RESULT_PEAK_SIGNAL_RATE_REF 0x00B6

/* Algo register */

#define VL53L0X_REG_ALGO_PART_TO_PART_RANGE_OFFSET_MM 0x0028

#define VL53L0X_REG_I2C_SLAVE_DEVICE_ADDRESS 0x008a

/* Check Limit registers */
#define VL53L0X_REG_MSRC_CONFIG_CONTROL 0x0060

#define VL53L0X_REG_PRE_RANGE_CONFIG_MIN_SNR 0x0027
#define VL53L0X_REG_PRE_RANGE_CONFIG_VALID_PHASE_LOW 0x0056
#define VL53L0X_REG_PRE_RANGE_CONFIG_VALID_PHASE_HIGH 0x0057
#define VL53L0X_REG_PRE_RANGE_MIN_COUNT_RATE_RTN_LIMIT 0x0064

#define VL53L0X_REG_FINAL_RANGE_CONFIG_MIN_SNR 0x0067
#define VL53L0X_REG_FINAL_RANGE_CONFIG_VALID_PHASE_LOW 0x0047
#define VL53L0X_REG_FINAL_RANGE_CONFIG_VALID_PHASE_HIGH 0x0048
#define VL53L0X_REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT 0x0044

#define VL53L0X_REG_PRE_RANGE_CONFIG_SIGMA_THRESH_HI 0x0061
#define VL53L0X_REG_PRE_RANGE_CONFIG_SIGMA_THRESH_LO 0x0062

/* PRE RANGE registers */
#define VL53L0X_REG_PRE_RANGE_CONFIG_VCSEL_PERIOD 0x0050
#define VL53L0X_REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI 0x0051
#define VL53L0X_REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_LO 0x0052

#define VL53L0X_REG_SYSTEM_HISTOGRAM_BIN 0x0081
#define VL53L0X_REG_HISTOGRAM_CONFIG_INITIAL_PHASE_SELECT 0x0033
#define VL53L0X_REG_HISTOGRAM_CONFIG_READOUT_CTRL 0x0055

#define VL53L0X_REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD 0x0070
#define VL53L0X_REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI 0x0071
#define VL53L0X_REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_LO 0x0072
#define VL53L0X_REG_CROSSTALK_COMPENSATION_PEAK_RATE_MCPS 0x0020

#define VL53L0X_REG_MSRC_CONFIG_TIMEOUT_MACROP 0x0046

#define VL53L0X_REG_SOFT_RESET_GO2_SOFT_RESET_N 0x00bf
#define VL53L0X_REG_IDENTIFICATION_MODEL_ID 0x00c0
#define VL53L0X_REG_IDENTIFICATION_REVISION_ID 0x00c2

#define VL53L0X_REG_OSC_CALIBRATE_VAL 0x00f8

#define VL53L0X_SIGMA_ESTIMATE_MAX_VALUE 65535
/* equivalent to a range sigma of 655.35mm */

#define VL53L0X_REG_GLOBAL_CONFIG_VCSEL_WIDTH 0x032
#define VL53L0X_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0 0x0B0
#define VL53L0X_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_1 0x0B1
#define VL53L0X_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_2 0x0B2
#define VL53L0X_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_3 0x0B3
#define VL53L0X_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_4 0x0B4
#define VL53L0X_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_5 0x0B5

#define VL53L0X_REG_GLOBAL_CONFIG_REF_EN_START_SELECT 0xB6
#define VL53L0X_REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD 0x4E /* 0x14E */
#define VL53L0X_REG_DYNAMIC_SPAD_REF_EN_START_OFFSET 0x4F    /* 0x14F */
#define VL53L0X_REG_POWER_MANAGEMENT_GO1_POWER_FORCE 0x80

/*
 * Speed of light in um per 1E-10 Seconds
 */

#define VL53L0X_SPEED_OF_LIGHT_IN_AIR 2997

#define VL53L0X_REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV 0x0089

#define VL53L0X_REG_ALGO_PHASECAL_LIM 0x0030 /* 0x130 */
#define VL53L0X_REG_ALGO_PHASECAL_CONFIG_TIMEOUT 0x0030

// ------------------------------------ END REGISTER DEFINES
// --------------------------------------- //
#define VL53L0X_POS_CNT                                                        \
    5 // Number of possible positions of enemey detected by range sensor
#define VL53L0X_DEFAULT_ADDRESS 0x29
#define VL53L0X_EXPECTED_DEVICE_ID 0xEE

#define RANGE_SEQUENCE_STEP_TCC (0x10)  // Target CentreCheck
#define RANGE_SEQUENCE_STEP_MSRC (0x04) // Minimum Signal Rate Check
#define RANGE_SEQUENCE_STEP_DSS (0x28)  // Dynamic SPAD selection
#define RANGE_SEQUENCE_STEP_PRE_RANGE (0x40)
#define RANGE_SEQUENCE_STEP_FINAL_RANGE (0x80)

/* There are two types of SPAD: aperture and non-aperture. My understanding
 * is that aperture ones let it less light (they have a smaller opening),
 * similar to how you can change the aperture on a digital camera. Only 1/4 th
 * of the SPADs are of type non-aperture. */
#define SPAD_TYPE_APERTURE (0x01)
/* The total SPAD array is 16x16, but we can only activate a quadrant spanning
 * 44 SPADs at a time. In the ST api code they have (for some reason) selected
 * 0xB4 (180) as a starting point (lies in the middle and spans non-aperture
 * (3rd) quadrant and aperture (4th) quadrant). */
#define SPAD_START_SELECT (0xB4)
/* The total SPAD map is 16x16, but we should only activate an area of 44 SPADs
 * at a time. */
#define SPAD_MAX_COUNT (44)
/* The 44 SPADs are represented as 6 bytes where each bit represents a single
 * SPAD. 6x8 = 48, so the last four bits are unused. */
#define SPAD_MAP_ROW_COUNT (6)
#define SPAD_ROW_SIZE (8)
/* Since we start at 0xB4 (180), there are four quadrants (three aperture, one
 * aperture), and each quadrant contains 256 / 4 = 64 SPADs, and the third
 * quadrant is non-aperture, the offset to the aperture quadrant is (256 - 64 -
 * 180) = 12 */
#define SPAD_APERTURE_START_INDEX (12)

#define VL53L0X_POS_COUNT (3)

#define VL53L0X_OUT_OF_RANGE (8190)

typedef enum {
    e_VL53L0X_POS_FRONT,
    // Only using 3 sensors, and they are all on the front
    // e_VL53L0X_POS_LEFT,
    // e_VL53L0X_POS_RIGHT,
    e_VL53L0X_POS_FRONT_LEFT,
    e_VL53L0X_POS_FRONT_RIGHT,
} e__vl53l0x_pos;

typedef enum {
    e_VL53L0X_RESULT_OK,
    e_VL53L0X_RESULT_ERROR_I2C,
    e_VL53L0X_RESULT_ERROR_PWRUP,
    e_VL53L0X_RESULT_ERROR_SPAD,
    e_VL53L0X_RESULT_ERROR_MEASURE_ONGOING
} e__vl53l0x_result;

typedef enum {
    VL53L0X_CALIBRATION_TYPE_VHV,
    VL53L0X_CALIBRATION_TYPE_PHASE
} e__vl53l0x_calibration_type;

typedef enum {
    STATUS_MULTIPLE_NOT_STARTED,
    STATUS_MULTIPLE_MEASURING,
    STATUS_SINGLE_DONE,
    STATUS_MULTIPLE_DONE
} e__status_multiple;

typedef uint16_t t__vl53l0x_ranges[VL53L0X_POS_CNT];

/**
 * Initializes the sensors in the e__vl53l0x_idx enum.
 * Performs the data init and static init of ST's API init.
 * @note Each sensor must have its XSHUT pin connected.
 */
e__vl53l0x_result vl53l0x_init(void);

/**
 * Does a single range measurement (starts and polls until it's finished)
 * @param idx selects specific sensor
 * @param range contains the measured range or VL53L0X_OUT_OF_RANGE
 *        if out of range.
 * @return see vl53l0x_result_e
 * @note   Polling
 */
e__vl53l0x_result vl53l0x_read_range_single(e__vl53l0x_pos pos,
                                            uint16_t *range);

/**
 * Reads all sensors. This is faster than reading sensors individually because
 * we do the measurements in parallel. It starts measuring if no measurement is
 * ongoing and will return cached values if the measuring is not finished.
 * @param ranges contains the measured ranges (or VL53L0X_OUT_OF_RANGE
 *        if out of range).
 * @param fresh_values is true if the values are from a new measurement and
 *        false if cached values.
 * @return see vl53l0x_result_e
 * @note Blocks until range measurement is done when called the first time
 * (unless vl53l0x_start_measuring_multiple has been called)
 * @note Returns values from the last measurement if measuring is not finished
 */
e__vl53l0x_result vl53l0x_read_range_multiple(t__vl53l0x_ranges ranges,
                                              bool *fresh_values);

e__vl53l0x_result vl53lox_start_measuring_multiple(void);
