#ifndef DIPLOMAT_RUNTIME_C_H
#define DIPLOMAT_RUNTIME_C_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// These come from `uchar.h`, which is not available on all platforms.
// Redefining them in C is no problem, however in >C++11 they are fundamental
// types, which don't like being redefined.
#if !(__cplusplus >= 201100)
// https://en.cppreference.com/w/c/string/multibyte/char16_t
typedef uint_least16_t char16_t;
// https://en.cppreference.com/w/c/string/multibyte/char32_t
typedef uint_least32_t char32_t;
#endif

static_assert(sizeof(char) == sizeof(uint8_t), "your architecture's `char` is not 8 bits");
static_assert(sizeof(char16_t) == sizeof(uint16_t), "your architecture's `char16_t` is not 16 bits");
static_assert(sizeof(char32_t) == sizeof(uint32_t), "your architecture's `char32_t` is not 32 bits");

typedef struct DiplomatWrite {
    void* context;
    char* buf;
    size_t len;
    size_t cap;
    bool grow_failed;
    void (*flush)(struct DiplomatWrite*);
    bool (*grow)(struct DiplomatWrite*, size_t);
} DiplomatWrite;

bool diplomat_is_str(const char* buf, size_t len);

#define MAKE_SLICES(name, c_ty) \
    typedef struct Diplomat##name##View { \
        const c_ty* data; \
        size_t len; \
    } Diplomat##name##View; \
    typedef struct Diplomat##name##ViewMut { \
        c_ty* data; \
        size_t len; \
    } Diplomat##name##ViewMut; \
    typedef struct Diplomat##name##Array { \
        const c_ty* data; \
        size_t len; \
    } Diplomat##name##Array;

#define MAKE_SLICES_AND_OPTIONS(name, c_ty) \
    MAKE_SLICES(name, c_ty) \
    typedef struct Option##name {union { c_ty ok; }; bool is_ok; } Option##name; \
    typedef struct Option##name##View {union { Diplomat##name##View ok; }; bool is_ok; } Option##name##View; \
    typedef struct Option##name##ViewMut {union { Diplomat##name##ViewMut ok; }; bool is_ok; } Option##name##ViewMut; \
    typedef struct Option##name##Array {union { Diplomat##name##Array ok; }; bool is_ok; } Option##name##Array; \

MAKE_SLICES_AND_OPTIONS(I8, int8_t)
MAKE_SLICES_AND_OPTIONS(U8, uint8_t)
MAKE_SLICES_AND_OPTIONS(I16, int16_t)
MAKE_SLICES_AND_OPTIONS(U16, uint16_t)
MAKE_SLICES_AND_OPTIONS(I32, int32_t)
MAKE_SLICES_AND_OPTIONS(U32, uint32_t)
MAKE_SLICES_AND_OPTIONS(I64, int64_t)
MAKE_SLICES_AND_OPTIONS(U64, uint64_t)
MAKE_SLICES_AND_OPTIONS(Isize, intptr_t)
MAKE_SLICES_AND_OPTIONS(Usize, size_t)
MAKE_SLICES_AND_OPTIONS(F32, float)
MAKE_SLICES_AND_OPTIONS(F64, double)
MAKE_SLICES_AND_OPTIONS(Bool, bool)
MAKE_SLICES_AND_OPTIONS(Char, char32_t)
MAKE_SLICES_AND_OPTIONS(String, char)
MAKE_SLICES_AND_OPTIONS(String16, char16_t)
MAKE_SLICES_AND_OPTIONS(Strings, DiplomatStringView)
MAKE_SLICES_AND_OPTIONS(Strings16, DiplomatString16View)

DiplomatWrite diplomat_simple_write(char* buf, size_t buf_size);

DiplomatWrite* diplomat_buffer_write_create(size_t cap);
char* diplomat_buffer_write_get_bytes(DiplomatWrite* t);
size_t diplomat_buffer_write_len(DiplomatWrite* t);
void diplomat_buffer_write_destroy(DiplomatWrite* t);

#endif
