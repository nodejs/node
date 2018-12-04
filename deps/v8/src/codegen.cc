// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen.h"

#include <cmath>
#include <memory>

#include "src/flags.h"

namespace v8 {
namespace internal {

#define UNARY_MATH_FUNCTION(name, generator)                          \
  static UnaryMathFunction fast_##name##_function = nullptr;          \
  double std_##name(double x) { return std::name(x); }                \
  void init_fast_##name##_function() {                                \
    if (FLAG_fast_math) fast_##name##_function = generator();         \
    if (!fast_##name##_function) fast_##name##_function = std_##name; \
  }                                                                   \
  void lazily_initialize_fast_##name() {                              \
    if (!fast_##name##_function) init_fast_##name##_function();       \
  }                                                                   \
  double fast_##name(double x) { return (*fast_##name##_function)(x); }

UNARY_MATH_FUNCTION(sqrt, CreateSqrtFunction)

#undef UNARY_MATH_FUNCTION

}  // namespace internal
}  // namespace v8
