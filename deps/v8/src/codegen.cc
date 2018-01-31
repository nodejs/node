// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen.h"

#include <cmath>
#include <memory>

#include "src/flags.h"

namespace v8 {
namespace internal {

#define UNARY_MATH_FUNCTION(name, generator)                             \
  static UnaryMathFunctionWithIsolate fast_##name##_function = nullptr;  \
  double std_##name(double x, Isolate* isolate) { return std::name(x); } \
  void init_fast_##name##_function(Isolate* isolate) {                   \
    if (FLAG_fast_math) fast_##name##_function = generator(isolate);     \
    if (!fast_##name##_function) fast_##name##_function = std_##name;    \
  }                                                                      \
  void lazily_initialize_fast_##name(Isolate* isolate) {                 \
    if (!fast_##name##_function) init_fast_##name##_function(isolate);   \
  }                                                                      \
  double fast_##name(double x, Isolate* isolate) {                       \
    return (*fast_##name##_function)(x, isolate);                        \
  }

UNARY_MATH_FUNCTION(sqrt, CreateSqrtFunction)

#undef UNARY_MATH_FUNCTION

}  // namespace internal
}  // namespace v8
