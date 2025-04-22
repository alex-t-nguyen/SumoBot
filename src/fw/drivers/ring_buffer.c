#include "drivers/ring_buffer.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct ring_buffer {
    uint8_t *buffer;
    uint8_t size;
    uint8_t w_idx;
    uint8_t r_idx;
};

// Constructor
/**
 * Initializes ring buffer and allocates memory for
 * "num_elements" elements and returns a pointer to the
 * initialized ring buffer
 */
struct ring_buffer *ring_buffer_init(uint8_t num_elements) {
    struct ring_buffer *p_buf = malloc(sizeof(struct ring_buffer));
    p_buf->buffer = calloc(
        num_elements, sizeof(uint8_t)); // Initialize memory for num_elements
                                        // each of type uint8_t
    p_buf->size = num_elements;
    p_buf->w_idx = 0;
    p_buf->r_idx = 0;
    return p_buf;
}

// Destructor
/**
 * Frees the memory allocated for the ring buffer
 */
void ring_buffer_destroy(struct ring_buffer *p_buf) { free(p_buf); }

// Mutators
/**
 * Puts an element in the ring buffer at the current
 * write index and updates the write index to the next index
 */
void ring_buffer_push(struct ring_buffer *p_buf, uint8_t c) {
    p_buf->buffer[p_buf->w_idx] = c; // Put element into buffer at current index
    p_buf->w_idx++;                  // Increment write index

    if (p_buf->w_idx == p_buf->size) // If write index is at end of buffer
        p_buf->w_idx = 0; // Set write index back to beginning of buffer
}

// Accessors
/**
 * Gets the element in the ring buffer at the current
 * read index and updates the read index to the next element
 */
uint8_t ring_buffer_pop(struct ring_buffer *p_buf) {
    const uint8_t r_val =
        p_buf->buffer[p_buf->r_idx]; // Get element at current index
    p_buf->r_idx++;                  // Increment read index

    if (p_buf->r_idx == p_buf->size) // If read index is at end of buffer
        p_buf->r_idx = 0; // Set read index back to beginning of buffer
    return r_val;         // Return read value from buffer
}

/**
 * Gets the element in the ring buffer at the current
 * read index, but doesn't update the read index
 */
uint8_t ring_buffer_peek(struct ring_buffer *p_buf) {
    return p_buf->buffer[p_buf->r_idx]; // Get element at current index
}

/**
 * Returns true if ring buffer is full, false if
 * ring buffer is not full
 */
bool ring_buffer_isfull(const struct ring_buffer *p_buf) {
    uint8_t w_idx_next = p_buf->w_idx + 1; // Get next write index
    if (w_idx_next ==
        p_buf->size)    // If the next write index is at end of buffer
        w_idx_next = 0; // Set next write index to beginning of buffer
    return w_idx_next ==
           p_buf->r_idx; // Buffer is full if next write index == read index
}

/**
 * Returns true if ring buffer is empty, false if
 * ring buffer is not empty
 */
bool ring_buffer_isempty(const struct ring_buffer *p_buf) {
    return p_buf->r_idx ==
           p_buf->w_idx; // Buffer is empty if write index == read index
}
