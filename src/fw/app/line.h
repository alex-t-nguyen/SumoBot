
typedef enum {
    LINE_NONE,  // No line
    LINE_FRONT, // Line crossing both front qre1113 sensors
    LINE_BACK,  // Line crossing both back qre1113 sensors
    LINE_LEFT,  // Line crossing both left qre1113 sensors
    LINE_RIGHT, // Line crossing both right qre1113 sensors
    LINE_FRONT_LEFT, // Line crossing only front left qre1113 sensor
    LINE_FRONT_RIGHT, // Line crossing only front right qre1113 sensor
    LINE_BACK_LEFT, // Line crossing only back left qre1113 sensor
    LINE_BACK_RIGHT, // Line crossing only back right qre1113 sensor
    LINE_DIAGONAL_LEFT, // Line crossing front left and back right qre1113 sensor
    LINE_DIAGONAL_RIGHT // Line crossing front right and back left qre1113 sensor
} e__line;

void line_init(void);
e__line line_get(void);
