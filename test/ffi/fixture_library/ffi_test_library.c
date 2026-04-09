#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define FFI_EXPORT __declspec(dllexport)
#else
#define FFI_EXPORT
#endif

// Integer operations.

FFI_EXPORT int8_t add_i8(int8_t a, int8_t b) {
  return a + b;
}

FFI_EXPORT uint8_t add_u8(uint8_t a, uint8_t b) {
  return a + b;
}

FFI_EXPORT int16_t add_i16(int16_t a, int16_t b) {
  return a + b;
}

FFI_EXPORT uint16_t add_u16(uint16_t a, uint16_t b) {
  return a + b;
}

FFI_EXPORT int32_t add_i32(int32_t a, int32_t b) {
  return a + b;
}

FFI_EXPORT uint32_t add_u32(uint32_t a, uint32_t b) {
  return a + b;
}

FFI_EXPORT int64_t add_i64(int64_t a, int64_t b) {
  return a + b;
}

FFI_EXPORT uint64_t add_u64(uint64_t a, uint64_t b) {
  return a + b;
}

FFI_EXPORT char identity_char(char value) {
  return value;
}

FFI_EXPORT int32_t char_is_signed(void) {
  return ((char)-1) < 0;
}

// Floating point operations.

FFI_EXPORT float add_f32(float a, float b) {
  return a + b;
}

FFI_EXPORT float multiply_f32(float a, float b) {
  return a * b;
}

FFI_EXPORT double add_f64(double a, double b) {
  return a + b;
}

FFI_EXPORT double multiply_f64(double a, double b) {
  return a * b;
}

// Pointer operations.

FFI_EXPORT void* identity_pointer(void* ptr) {
  return ptr;
}

FFI_EXPORT uint64_t pointer_to_usize(void* ptr) {
  return (uint64_t)(uintptr_t)ptr;
}

FFI_EXPORT void* usize_to_pointer(uint64_t addr) {
  return (void*)(uintptr_t)addr;
}

// String operations.

FFI_EXPORT uint64_t string_length(const char* str) {
  return str ? strlen(str) : 0;
}

FFI_EXPORT char* string_concat(const char* a, const char* b) {
  if (!a || !b) {
    // NOLINTNEXTLINE (readability/null_usage)
    return NULL;
  }

  size_t len_a = strlen(a);
  size_t len_b = strlen(b);
  char* result = malloc(len_a + len_b + 1);

  if (!result) {
    // NOLINTNEXTLINE (readability/null_usage)
    return NULL;
  }

  memcpy(result, a, len_a);
  memcpy(result + len_a, b, len_b + 1);
  return result;
}

FFI_EXPORT char* string_duplicate(const char* str) {
  if (!str) {
    // NOLINTNEXTLINE (readability/null_usage)
    return NULL;
  }

  size_t len = strlen(str);
  char* result = malloc(len + 1);

  if (!result) {
    // NOLINTNEXTLINE (readability/null_usage)
    return NULL;
  }

  memcpy(result, str, len + 1);
  return result;
}

FFI_EXPORT void free_string(char* str) {
  free(str);
}

// Buffer/Array operations.

FFI_EXPORT void fill_buffer(uint8_t* buffer, uint64_t length, uint32_t value) {
  if (!buffer) {
    return;
  }

  memset(buffer, (uint8_t)value, length);
}

FFI_EXPORT uint64_t sum_buffer(const uint8_t* buffer, uint64_t length) {
  if (!buffer) {
    return 0;
  }

  size_t sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += buffer[i];
  }
  return sum;
}

FFI_EXPORT void reverse_buffer(uint8_t* buffer, uint64_t length) {
  if (!buffer || length == 0) {
    return;
  }

  for (uint64_t i = 0; i < length / 2; i++) {
    uint8_t temp = buffer[i];
    buffer[i] = buffer[length - 1 - i];
    buffer[length - 1 - i] = temp;
  }
}

// Struct operations.

typedef struct {
  int32_t x;
  int32_t y;
} Point;

typedef struct {
  float x;
  float y;
  float z;
} Point3D;

FFI_EXPORT Point make_point(int32_t x, int32_t y) {
  Point p = {x, y};
  return p;
}

FFI_EXPORT int32_t point_distance_squared(Point p1, Point p2) {
  int32_t dx = p1.x - p2.x;
  int32_t dy = p1.y - p2.y;
  return dx * dx + dy * dy;
}

FFI_EXPORT Point3D make_point3d(float x, float y, float z) {
  Point3D p = {x, y, z};
  return p;
}

FFI_EXPORT float point3d_magnitude_squared(Point3D p) {
  return p.x * p.x + p.y * p.y + p.z * p.z;
}

// Boolean operations.

FFI_EXPORT int32_t logical_and(int32_t a, int32_t b) {
  return (a && b) ? 1 : 0;
}

FFI_EXPORT int32_t logical_or(int32_t a, int32_t b) {
  return (a || b) ? 1 : 0;
}

FFI_EXPORT int32_t logical_not(int32_t a) {
  return !a ? 1 : 0;
}

// Void operations (side effects).

static int32_t global_counter = 0;

FFI_EXPORT void increment_counter(void) {
  global_counter++;
}

FFI_EXPORT int32_t get_counter(void) {
  return global_counter;
}

FFI_EXPORT void reset_counter(void) {
  global_counter = 0;
}

// Callback operations.

typedef int32_t (*IntCallback)(int32_t);
typedef int8_t (*Int8Callback)(int8_t);
typedef const char* (*PointerCallback)(void);
typedef void (*VoidCallback)(void);
typedef void (*StringCallback)(const char*);
typedef int32_t (*BinaryIntCallback)(int32_t, int32_t);

FFI_EXPORT int32_t call_int_callback(IntCallback callback, int32_t value) {
  if (!callback) {
    return -1;
  }

  return callback(value);
}

FFI_EXPORT int8_t call_int8_callback(Int8Callback callback, int8_t value) {
  if (!callback) {
    return 0;
  }

  return callback(value);
}

FFI_EXPORT int32_t call_pointer_callback_is_null(PointerCallback callback) {
  if (!callback) {
    return 1;
  }

  // NOLINTNEXTLINE (readability/null_usage)
  return callback() == NULL;
}

FFI_EXPORT void call_void_callback(VoidCallback callback) {
  if (callback) {
    callback();
  }
}

FFI_EXPORT void call_string_callback(StringCallback callback, const char* str) {
  if (callback) {
    callback(str);
  }
}

FFI_EXPORT int32_t call_binary_int_callback(BinaryIntCallback callback,
                                            int32_t a,
                                            int32_t b) {
  if (!callback) {
    return -1;
  }

  return callback(a, b);
}

// Callback that calls multiple times.
FFI_EXPORT void call_callback_multiple_times(IntCallback callback,
                                             int32_t times) {
  if (!callback) {
    return;
  }

  for (int32_t i = 0; i < times; i++) {
    callback(i);
  }
}

// Edge cases and error conditions.

FFI_EXPORT int32_t divide_i32(int32_t a, int32_t b) {
  if (b == 0) {
    return 0;
  }

  return a / b;
}

FFI_EXPORT void* allocate_memory(size_t size) {
  return malloc(size);
}

FFI_EXPORT void deallocate_memory(void* ptr) {
  free(ptr);
}

// NULL pointer handling.
FFI_EXPORT int32_t safe_strlen(const char* str) {
  return str ? (int32_t)strlen(str) : -1;
}

// Multi-parameter functions.

FFI_EXPORT int32_t
sum_five_i32(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
  return a + b + c + d + e;
}

FFI_EXPORT double sum_five_f64(
    double a, double b, double c, double d, double e) {
  return a + b + c + d + e;
}

// Mixed parameter types.

FFI_EXPORT double mixed_operation(int32_t i, float f, double d, uint32_t u) {
  return (double)i + (double)f + d + (double)u;
}

// Constant values for testing.

FFI_EXPORT const int32_t CONSTANT_I32 = 42;
FFI_EXPORT const uint64_t CONSTANT_U64 = 0xDEADBEEFCAFEBABEULL;
FFI_EXPORT const float CONSTANT_F32 = 3.14159f;
FFI_EXPORT const double CONSTANT_F64 = 2.718281828459045;
FFI_EXPORT const char* CONSTANT_STRING = "Hello from FFI addon";

// Array/pointer indexing.

FFI_EXPORT int32_t array_get_i32(const int32_t* arr, size_t index) {
  if (!arr) {
    return 0;
  }

  return arr[index];
}

FFI_EXPORT void array_set_i32(int32_t* arr, size_t index, int32_t value) {
  if (!arr) {
    return;
  }

  arr[index] = value;
}

FFI_EXPORT double array_get_f64(const double* arr, size_t index) {
  if (!arr) {
    return 0.0;
  }

  return arr[index];
}

FFI_EXPORT void array_set_f64(double* arr, size_t index, double value) {
  if (!arr) {
    return;
  }

  arr[index] = value;
}
