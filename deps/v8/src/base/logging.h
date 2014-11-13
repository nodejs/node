// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_LOGGING_H_
#define V8_BASE_LOGGING_H_

#include <stdint.h>
#include <string.h>

#include "src/base/build_config.h"

extern "C" void V8_Fatal(const char* file, int line, const char* format, ...);


// The FATAL, UNREACHABLE and UNIMPLEMENTED macros are useful during
// development, but they should not be relied on in the final product.
#ifdef DEBUG
#define FATAL(msg)                              \
  V8_Fatal(__FILE__, __LINE__, "%s", (msg))
#define UNIMPLEMENTED()                         \
  V8_Fatal(__FILE__, __LINE__, "unimplemented code")
#define UNREACHABLE()                           \
  V8_Fatal(__FILE__, __LINE__, "unreachable code")
#else
#define FATAL(msg)                              \
  V8_Fatal("", 0, "%s", (msg))
#define UNIMPLEMENTED()                         \
  V8_Fatal("", 0, "unimplemented code")
#define UNREACHABLE() ((void) 0)
#endif


// The CHECK macro checks that the given condition is true; if not, it
// prints a message to stderr and aborts.
#define CHECK(condition) do {                                       \
    if (!(condition)) {                                             \
      V8_Fatal(__FILE__, __LINE__, "CHECK(%s) failed", #condition); \
    }                                                               \
  } while (0)


// Helper function used by the CHECK_EQ function when given int
// arguments.  Should not be called directly.
inline void CheckEqualsHelper(const char* file, int line,
                              const char* expected_source, int expected,
                              const char* value_source, int value) {
  if (expected != value) {
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#   Expected: %i\n#   Found: %i",
             expected_source, value_source, expected, value);
  }
}


// Helper function used by the CHECK_EQ function when given int64_t
// arguments.  Should not be called directly.
inline void CheckEqualsHelper(const char* file, int line,
                              const char* expected_source,
                              int64_t expected,
                              const char* value_source,
                              int64_t value) {
  if (expected != value) {
    // Print int64_t values in hex, as two int32s,
    // to avoid platform-dependencies.
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#"
             "   Expected: 0x%08x%08x\n#   Found: 0x%08x%08x",
             expected_source, value_source,
             static_cast<uint32_t>(expected >> 32),
             static_cast<uint32_t>(expected),
             static_cast<uint32_t>(value >> 32),
             static_cast<uint32_t>(value));
  }
}


// Helper function used by the CHECK_NE function when given int
// arguments.  Should not be called directly.
inline void CheckNonEqualsHelper(const char* file,
                                 int line,
                                 const char* unexpected_source,
                                 int unexpected,
                                 const char* value_source,
                                 int value) {
  if (unexpected == value) {
    V8_Fatal(file, line, "CHECK_NE(%s, %s) failed\n#   Value: %i",
             unexpected_source, value_source, value);
  }
}


// Helper function used by the CHECK function when given string
// arguments.  Should not be called directly.
inline void CheckEqualsHelper(const char* file,
                              int line,
                              const char* expected_source,
                              const char* expected,
                              const char* value_source,
                              const char* value) {
  if ((expected == NULL && value != NULL) ||
      (expected != NULL && value == NULL) ||
      (expected != NULL && value != NULL && strcmp(expected, value) != 0)) {
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#   Expected: %s\n#   Found: %s",
             expected_source, value_source, expected, value);
  }
}


inline void CheckNonEqualsHelper(const char* file,
                                 int line,
                                 const char* expected_source,
                                 const char* expected,
                                 const char* value_source,
                                 const char* value) {
  if (expected == value ||
      (expected != NULL && value != NULL && strcmp(expected, value) == 0)) {
    V8_Fatal(file, line, "CHECK_NE(%s, %s) failed\n#   Value: %s",
             expected_source, value_source, value);
  }
}


// Helper function used by the CHECK function when given pointer
// arguments.  Should not be called directly.
inline void CheckEqualsHelper(const char* file,
                              int line,
                              const char* expected_source,
                              const void* expected,
                              const char* value_source,
                              const void* value) {
  if (expected != value) {
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#   Expected: %p\n#   Found: %p",
             expected_source, value_source,
             expected, value);
  }
}


inline void CheckNonEqualsHelper(const char* file,
                                 int line,
                                 const char* expected_source,
                                 const void* expected,
                                 const char* value_source,
                                 const void* value) {
  if (expected == value) {
    V8_Fatal(file, line, "CHECK_NE(%s, %s) failed\n#   Value: %p",
             expected_source, value_source, value);
  }
}


inline void CheckNonEqualsHelper(const char* file,
                              int line,
                              const char* expected_source,
                              int64_t expected,
                              const char* value_source,
                              int64_t value) {
  if (expected == value) {
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#   Expected: %f\n#   Found: %f",
             expected_source, value_source, expected, value);
  }
}


#define CHECK_EQ(expected, value) CheckEqualsHelper(__FILE__, __LINE__, \
  #expected, expected, #value, value)


#define CHECK_NE(unexpected, value) CheckNonEqualsHelper(__FILE__, __LINE__, \
  #unexpected, unexpected, #value, value)


#define CHECK_GT(a, b) CHECK((a) > (b))
#define CHECK_GE(a, b) CHECK((a) >= (b))
#define CHECK_LT(a, b) CHECK((a) < (b))
#define CHECK_LE(a, b) CHECK((a) <= (b))


namespace v8 {
namespace base {

// Exposed for making debugging easier (to see where your function is being
// called, just add a call to DumpBacktrace).
void DumpBacktrace();

} }  // namespace v8::base


// The DCHECK macro is equivalent to CHECK except that it only
// generates code in debug builds.
#ifdef DEBUG
#define DCHECK_RESULT(expr)    CHECK(expr)
#define DCHECK(condition)      CHECK(condition)
#define DCHECK_EQ(v1, v2)      CHECK_EQ(v1, v2)
#define DCHECK_NE(v1, v2)      CHECK_NE(v1, v2)
#define DCHECK_GE(v1, v2)      CHECK_GE(v1, v2)
#define DCHECK_LT(v1, v2)      CHECK_LT(v1, v2)
#define DCHECK_LE(v1, v2)      CHECK_LE(v1, v2)
#else
#define DCHECK_RESULT(expr)    (expr)
#define DCHECK(condition)      ((void) 0)
#define DCHECK_EQ(v1, v2)      ((void) 0)
#define DCHECK_NE(v1, v2)      ((void) 0)
#define DCHECK_GE(v1, v2)      ((void) 0)
#define DCHECK_LT(v1, v2)      ((void) 0)
#define DCHECK_LE(v1, v2)      ((void) 0)
#endif

#define DCHECK_NOT_NULL(p)  DCHECK_NE(NULL, p)

// "Extra checks" are lightweight checks that are enabled in some release
// builds.
#ifdef ENABLE_EXTRA_CHECKS
#define EXTRA_CHECK(condition) CHECK(condition)
#else
#define EXTRA_CHECK(condition) ((void) 0)
#endif

#endif  // V8_BASE_LOGGING_H_
