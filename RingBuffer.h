#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Použití:
 *
 * RING_BUFFER_DEF(uint32_t, rx_buf, 64);
 *
 * ring_buffer_init(&rx_buf);
 * ring_buffer_push(&rx_buf, value);
 * ring_buffer_pop(&rx_buf, &value);
 */

#define RING_BUFFER_DEF(type, name, size)                  \
    typedef struct {                                       \
        volatile uint32_t head;                            \
        volatile uint32_t tail;                            \
        type buffer[size];                                 \
    } name##_t;                                            \
    static name##_t name

#define ring_buffer_init(rb)         \
    do {                             \
        (rb)->head = 0;              \
        (rb)->tail = 0;              \
    } while (0)

#define ring_buffer_capacity(rb)     \
    (sizeof((rb)->buffer) / sizeof((rb)->buffer[0]))

#define ring_buffer_is_empty(rb)     \
    ((rb)->head == (rb)->tail)

#define ring_buffer_is_full(rb)                              \
    (((rb)->head + 1) % ring_buffer_capacity(rb) == (rb)->tail)

        /*(rb)->head &= 0x1F;*/

#define ring_buffer_push_no_check(rb, value)            \
    ({                                                  \
        (rb)->head++;                                   \
        (rb)->head &= (ring_buffer_capacity(rb)-1);     \
        (rb)->buffer[(rb)->head] = (value);             \
    })

#define ring_buffer_push(rb, value)                          \
    ({                                                       \
        bool ok = false;                                     \
        uint32_t next = ((rb)->head + 1) % ring_buffer_capacity(rb); \
        if (next != (rb)->tail) {                            \
            (rb)->buffer[(rb)->head] = (value);              \
            (rb)->head = next;                               \
            ok = true;                                       \
        }                                                    \
        ok;                                                  \
    })

#define ring_buffer_pop(rb, out_ptr)                         \
    ({                                                       \
        bool ok = false;                                     \
        if ((rb)->head != (rb)->tail) {                      \
            *(out_ptr) = (rb)->buffer[(rb)->tail];           \
            (rb)->tail = ((rb)->tail + 1) % ring_buffer_capacity(rb); \
            ok = true;                                       \
        }                                                    \
        ok;                                                  \
    })

#endif
