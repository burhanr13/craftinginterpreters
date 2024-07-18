#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define Vector(T)                                                              \
    struct {                                                                   \
        T* d;                                                                  \
        size_t cap;                                                            \
        size_t size;                                                           \
    }

#define Vec_push(v, e)                                                         \
    do {                                                                       \
        if ((v).size == (v).cap) {                                             \
            (v).cap = (v).cap ? 2 * (v).cap : 8;                               \
            (v).d = realloc((v).d, (v).cap * sizeof *(v).d);                   \
        }                                                                      \
        (v).d[(v).size++] = (e);                                               \
    } while (false)

#endif