// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/counters.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 20.2.2 Function Properties of the Math Object

// ES6 section 20.2.2.18 Math.hypot ( value1, value2, ...values )
BUILTIN(MathHypot) {
  HandleScope scope(isolate);
  int const length = args.length() - 1;
  if (length == 0) return Smi::kZero;
  DCHECK_LT(0, length);
  double max = 0;
  bool one_arg_is_nan = false;
  std::vector<double> abs_values;
  abs_values.reserve(length);
  for (int i = 0; i < length; i++) {
    Handle<Object> x = args.at(i + 1);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, x,
                                       Object::ToNumber(isolate, x));
    double abs_value = std::abs(x->Number());

    if (std::isnan(abs_value)) {
      one_arg_is_nan = true;
    } else {
      abs_values.push_back(abs_value);
      if (max < abs_value) {
        max = abs_value;
      }
    }
  }

  if (max == V8_INFINITY) {
    return *isolate->factory()->NewNumber(V8_INFINITY);
  }

  if (one_arg_is_nan) {
    return ReadOnlyRoots(isolate).nan_value();
  }

  if (max == 0) {
    return Smi::kZero;
  }
  DCHECK_GT(max, 0);

  // Kahan summation to avoid rounding errors.
  // Normalize the numbers to the largest one to avoid overflow.
  double sum = 0;
  double compensation = 0;
  for (int i = 0; i < length; i++) {
    double n = abs_values[i] / max;
    double summand = n * n - compensation;
    double preliminary = sum + summand;
    compensation = (preliminary - sum) - summand;
    sum = preliminary;
  }

  return *isolate->factory()->NewNumber(std::sqrt(sum) * max);
}

}  // namespace internal
}  // namespace v8
