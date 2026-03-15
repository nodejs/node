#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Integer operations.

int8_t add_i8(int8_t a, int8_t b) {
  return a + b;
}

uint8_t add_u8(uint8_t a, uint8_t b) {
  return a + b;
}

int16_t add_i16(int16_t a, int16_t b) {
  return a + b;
}

uint16_t add_u16(uint16_t a, uint16_t b) {
  return a + b;
}

int32_t add_i32(int32_t a, int32_t b) {
  return a + b;
}

uint32_t add_u32(uint32_t a, uint32_t b) {
  return a + b;
}

int64_t add_i64(int64_t a, int64_t b) {
  return a + b;
}

uint64_t add_u64(uint64_t a, uint64_t b) {
  return a + b;
}

// Floating point operations.

float add_f32(float a, float b) {
  return a + b;
}

float multiply_f32(float a, float b) {
  return a * b;
}

double add_f64(double a, double b) {
  return a + b;
}

double multiply_f64(double a, double b) {
  return a * b;
}

// Pointer operations.

void* identity_pointer(void* ptr) {
  return ptr;
}

uint64_t pointer_to_usize(void* ptr) {
  return (uint64_t)(uintptr_t)ptr;
}

void* usize_to_pointer(uint64_t addr) {
  return (void*)(uintptr_t)addr;
}

// String operations.

uint64_t string_length(const char* str) {
  return str ? strlen(str) : 0;
}

char* string_concat(const char* a, const char* b) {
  if (!a || !b) {
    return NULL;
  }

  size_t len_a = strlen(a);
  size_t len_b = strlen(b);
  char* result = malloc(len_a + len_b + 1);

  if (!result) {
    return NULL;
  }

  memcpy(result, a, len_a);
  memcpy(result + len_a, b, len_b + 1);
  return result;
}

char* string_duplicate(const char* str) {
  if (!str) {
    return NULL;
  }

  size_t len = strlen(str);
  char* result = malloc(len + 1);

  if (!result) {
    return NULL;
  }

  memcpy(result, str, len + 1);
  return result;
}

void free_string(char* str) {
  free(str);
}

// Buffer/Array operations.

void fill_buffer(uint8_t* buffer, uint64_t length, uint32_t value) {
  if (!buffer) {
    return;
  }

  memset(buffer, (uint8_t)value, length);
}

uint64_t sum_buffer(const uint8_t* buffer, uint64_t length) {
  if (!buffer) {
    return 0;
  }

  size_t sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += buffer[i];
  }
  return sum;
}

void reverse_buffer(uint8_t* buffer, uint64_t length) {
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

Point make_point(int32_t x, int32_t y) {
  Point p = { x, y };
  return p;
}

int32_t point_distance_squared(Point p1, Point p2) {
  int32_t dx = p1.x - p2.x;
  int32_t dy = p1.y - p2.y;
  return dx * dx + dy * dy;
}

Point3D make_point3d(float x, float y, float z) {
  Point3D p = { x, y, z };
  return p;
}

float point3d_magnitude_squared(Point3D p) {
  return p.x * p.x + p.y * p.y + p.z * p.z;
}

// Boolean operations.

int32_t logical_and(int32_t a, int32_t b) {
  return (a && b) ? 1 : 0;
}

int32_t logical_or(int32_t a, int32_t b) {
  return (a || b) ? 1 : 0;
}

int32_t logical_not(int32_t a) {
  return !a ? 1 : 0;
}

// Void operations (side effects).

static int32_t global_counter = 0;

void increment_counter(void) {
  global_counter++;
}

int32_t get_counter(void) {
  return global_counter;
}

void reset_counter(void) {
  global_counter = 0;
}

// Callback operations.

typedef int32_t (*IntCallback)(int32_t);
typedef void (*VoidCallback)(void);
typedef void (*StringCallback)(const char*);
typedef int32_t (*BinaryIntCallback)(int32_t, int32_t);

int32_t call_int_callback(IntCallback callback, int32_t value) {
  if (!callback) {
    return -1;
  }

  return callback(value);
}

void call_void_callback(VoidCallback callback) {
  if (callback) {
    callback();
  }
}

void call_string_callback(StringCallback callback, const char* str) {
  if (callback) {
    callback(str);
  }
}

int32_t call_binary_int_callback(BinaryIntCallback callback, int32_t a, int32_t b) {
  if (!callback) {
    return -1;
  }

  return callback(a, b);
}

// Callback that calls multiple times.
void call_callback_multiple_times(IntCallback callback, int32_t times) {
  if (!callback) {
    return;
  }

  for (int32_t i = 0; i < times; i++) {
    callback(i);
  }
}

// Edge cases and error conditions.

int32_t divide_i32(int32_t a, int32_t b) {
  if (b == 0) {
    return 0;
  }

  return a / b;
}

void* allocate_memory(size_t size) {
  return malloc(size);
}

void deallocate_memory(void* ptr) {
  free(ptr);
}

// Null pointer handling.
int32_t safe_strlen(const char* str) {
  return str ? (int32_t)strlen(str) : -1;
}

// Multi-parameter functions.

int32_t sum_five_i32(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
  return a + b + c + d + e;
}

double sum_five_f64(double a, double b, double c, double d, double e) {
  return a + b + c + d + e;
}

// Mixed parameter types.

double mixed_operation(int32_t i, float f, double d, uint32_t u) {
  return (double)i + (double)f + d + (double)u;
}

// Constant values for testing.

const int32_t CONSTANT_I32 = 42;
const uint64_t CONSTANT_U64 = 0xDEADBEEFCAFEBABEULL;
const float CONSTANT_F32 = 3.14159f;
const double CONSTANT_F64 = 2.718281828459045;
const char* CONSTANT_STRING = "Hello from FFI addon";

// Array/pointer indexing.

int32_t array_get_i32(const int32_t* arr, size_t index) {
  if (!arr) {
    return 0;
  }

  return arr[index];
}

void array_set_i32(int32_t* arr, size_t index, int32_t value) {
  if (!arr) {
    return;
  }

  arr[index] = value;
}

double array_get_f64(const double* arr, size_t index) {
  if (!arr) {
    return 0.0;
  }

  return arr[index];
}

void array_set_f64(double* arr, size_t index, double value) {
  if (!arr) {
    return;
  }

  arr[index] = value;
}
