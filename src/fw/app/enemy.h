#include <stdbool.h>

/*
 * A software layer that converts the range sensor driver measurements into discrete enemy positions
 * and distances to simplify the application code
 */

typedef enum {
    ENEMY_POS_NONE,
    ENEMY_POS_FRONT_MIDDLE,
    ENEMY_POS_FRONT_LEFT,
    ENEMY_POS_FRONT_RIGHT,
    ENEMY_POS_FRONT_MIDDLE_AND_FRONT_LEFT,
    ENEMY_POS_FRONT_MIDDLE_AND_FRONT_RIGHT,
    ENEMY_POS_FRONT_ALL,
    ENEMY_POS_IMPOSSIBLE
} e__enemy_pos;

typedef enum {
    ENEMY_RANGE_NONE,
    ENEMY_RANGE_CLOSE,
    ENEMY_RANGE_MID,
    ENEMY_RANGE_FAR
} e__enemy_range;

struct enemy {
    e__enemy_pos position;
    e__enemy_range range;
};

void enemy_init(void);

struct enemy enemy_get(void);

bool enemy_detected(const struct enemy *enemy);
bool enemy_at_left(const struct enemy *enemy);
bool enemy_at_right(const struct enemy *enemy);
bool enemy_at_front(const struct enemy *enemy);

const char *enemy_pos_str(e__enemy_pos pos);
const char *enemy_range_str(e__enemy_range range);


