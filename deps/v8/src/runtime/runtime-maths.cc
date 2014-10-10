// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/assembler.h"
#include "src/codegen.h"
#include "src/runtime/runtime.h"
#include "src/runtime/runtime-utils.h"
#include "third_party/fdlibm/fdlibm.h"


namespace v8 {
namespace internal {

#define RUNTIME_UNARY_MATH(Name, name)                       \
  RUNTIME_FUNCTION(Runtime_Math##Name) {                     \
    HandleScope scope(isolate);                              \
    DCHECK(args.length() == 1);                              \
    isolate->counters()->math_##name()->Increment();         \
    CONVERT_DOUBLE_ARG_CHECKED(x, 0);                        \
    return *isolate->factory()->NewHeapNumber(std::name(x)); \
  }

RUNTIME_UNARY_MATH(Acos, acos)
RUNTIME_UNARY_MATH(Asin, asin)
RUNTIME_UNARY_MATH(Atan, atan)
RUNTIME_UNARY_MATH(LogRT, log)
#undef RUNTIME_UNARY_MATH


RUNTIME_FUNCTION(Runtime_DoubleHi) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  uint64_t integer = double_to_uint64(x);
  integer = (integer >> 32) & 0xFFFFFFFFu;
  return *isolate->factory()->NewNumber(static_cast<int32_t>(integer));
}


RUNTIME_FUNCTION(Runtime_DoubleLo) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return *isolate->factory()->NewNumber(
      static_cast<int32_t>(double_to_uint64(x) & 0xFFFFFFFFu));
}


RUNTIME_FUNCTION(Runtime_ConstructDouble) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_NUMBER_CHECKED(uint32_t, hi, Uint32, args[0]);
  CONVERT_NUMBER_CHECKED(uint32_t, lo, Uint32, args[1]);
  uint64_t result = (static_cast<uint64_t>(hi) << 32) | lo;
  return *isolate->factory()->NewNumber(uint64_to_double(result));
}


RUNTIME_FUNCTION(Runtime_RemPiO2) {
  HandleScope handle_scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  Factory* factory = isolate->factory();
  double y[2];
  int n = fdlibm::rempio2(x, y);
  Handle<FixedArray> array = factory->NewFixedArray(3);
  Handle<HeapNumber> y0 = factory->NewHeapNumber(y[0]);
  Handle<HeapNumber> y1 = factory->NewHeapNumber(y[1]);
  array->set(0, Smi::FromInt(n));
  array->set(1, *y0);
  array->set(2, *y1);
  return *factory->NewJSArrayWithElements(array);
}


static const double kPiDividedBy4 = 0.78539816339744830962;


RUNTIME_FUNCTION(Runtime_MathAtan2) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  isolate->counters()->math_atan2()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  double result;
  if (std::isinf(x) && std::isinf(y)) {
    // Make sure that the result in case of two infinite arguments
    // is a multiple of Pi / 4. The sign of the result is determined
    // by the first argument (x) and the sign of the second argument
    // determines the multiplier: one or three.
    int multiplier = (x < 0) ? -1 : 1;
    if (y < 0) multiplier *= 3;
    result = multiplier * kPiDividedBy4;
  } else {
    result = std::atan2(x, y);
  }
  return *isolate->factory()->NewNumber(result);
}


RUNTIME_FUNCTION(Runtime_MathExpRT) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  isolate->counters()->math_exp()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  lazily_initialize_fast_exp();
  return *isolate->factory()->NewNumber(fast_exp(x));
}


RUNTIME_FUNCTION(Runtime_MathFloorRT) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  isolate->counters()->math_floor()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return *isolate->factory()->NewNumber(Floor(x));
}


// Slow version of Math.pow.  We check for fast paths for special cases.
// Used if VFP3 is not available.
RUNTIME_FUNCTION(Runtime_MathPowSlow) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  isolate->counters()->math_pow()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);

  // If the second argument is a smi, it is much faster to call the
  // custom powi() function than the generic pow().
  if (args[1]->IsSmi()) {
    int y = args.smi_at(1);
    return *isolate->factory()->NewNumber(power_double_int(x, y));
  }

  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  double result = power_helper(x, y);
  if (std::isnan(result)) return isolate->heap()->nan_value();
  return *isolate->factory()->NewNumber(result);
}


// Fast version of Math.pow if we know that y is not an integer and y is not
// -0.5 or 0.5.  Used as slow case from full codegen.
RUNTIME_FUNCTION(Runtime_MathPowRT) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  isolate->counters()->math_pow()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  if (y == 0) {
    return Smi::FromInt(1);
  } else {
    double result = power_double_double(x, y);
    if (std::isnan(result)) return isolate->heap()->nan_value();
    return *isolate->factory()->NewNumber(result);
  }
}


RUNTIME_FUNCTION(Runtime_RoundNumber) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(input, 0);
  isolate->counters()->math_round()->Increment();

  if (!input->IsHeapNumber()) {
    DCHECK(input->IsSmi());
    return *input;
  }

  Handle<HeapNumber> number = Handle<HeapNumber>::cast(input);

  double value = number->value();
  int exponent = number->get_exponent();
  int sign = number->get_sign();

  if (exponent < -1) {
    // Number in range ]-0.5..0.5[. These always round to +/-zero.
    if (sign) return isolate->heap()->minus_zero_value();
    return Smi::FromInt(0);
  }

  // We compare with kSmiValueSize - 2 because (2^30 - 0.1) has exponent 29 and
  // should be rounded to 2^30, which is not smi (for 31-bit smis, similar
  // argument holds for 32-bit smis).
  if (!sign && exponent < kSmiValueSize - 2) {
    return Smi::FromInt(static_cast<int>(value + 0.5));
  }

  // If the magnitude is big enough, there's no place for fraction part. If we
  // try to add 0.5 to this number, 1.0 will be added instead.
  if (exponent >= 52) {
    return *number;
  }

  if (sign && value >= -0.5) return isolate->heap()->minus_zero_value();

  // Do not call NumberFromDouble() to avoid extra checks.
  return *isolate->factory()->NewNumber(Floor(value + 0.5));
}


RUNTIME_FUNCTION(Runtime_MathSqrtRT) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  isolate->counters()->math_sqrt()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return *isolate->factory()->NewNumber(fast_sqrt(x));
}


RUNTIME_FUNCTION(Runtime_MathFround) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  float xf = DoubleToFloat32(x);
  return *isolate->factory()->NewNumber(xf);
}


RUNTIME_FUNCTION(RuntimeReference_MathPow) {
  SealHandleScope shs(isolate);
  return __RT_impl_Runtime_MathPowSlow(args, isolate);
}


RUNTIME_FUNCTION(RuntimeReference_IsMinusZero) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (!obj->IsHeapNumber()) return isolate->heap()->false_value();
  HeapNumber* number = HeapNumber::cast(obj);
  return isolate->heap()->ToBoolean(IsMinusZero(number->value()));
}
}
}  // namespace v8::internal
