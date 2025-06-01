#include <stdint.h>

struct qre1113_voltages {
    uint16_t front_left;
    uint16_t front_right;
    uint16_t back_left;
    uint16_t back_right;
};

void qre1113_init(void);
void qre1113_get_voltages(struct qre1113_voltages *buffer);
