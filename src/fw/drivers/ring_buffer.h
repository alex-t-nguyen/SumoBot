#include <stdbool.h>
#include <stdint.h>

struct ring_buffer;

// Constructor
struct ring_buffer *ring_buffer_init(uint8_t size);

// Destructor
void ring_buffer_destroy(struct ring_buffer *p_buf);

// Mutators
void ring_buffer_push(struct ring_buffer *p_buf, uint8_t c);

// Accessors
uint8_t ring_buffer_pop(struct ring_buffer *p_buf);
uint8_t ring_buffer_peek(struct ring_buffer *p_buf);
bool ring_buffer_isfull(const struct ring_buffer *p_buf);
bool ring_buffer_isempty(const struct ring_buffer *p_buf);
