// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/assembler.h"
#include "src/base/utils/random-number-generator.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/third_party/fdlibm/fdlibm.h"

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
  uint64_t unsigned64 = double_to_uint64(x);
  uint32_t unsigned32 = static_cast<uint32_t>(unsigned64 >> 32);
  int32_t signed32 = bit_cast<int32_t, uint32_t>(unsigned32);
  return *isolate->factory()->NewNumber(signed32);
}


RUNTIME_FUNCTION(Runtime_DoubleLo) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  uint64_t unsigned64 = double_to_uint64(x);
  uint32_t unsigned32 = static_cast<uint32_t>(unsigned64);
  int32_t signed32 = bit_cast<int32_t, uint32_t>(unsigned32);
  return *isolate->factory()->NewNumber(signed32);
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
  SealHandleScope shs(isolate);
  DisallowHeapAllocation no_gc;
  DCHECK(args.length() == 2);
  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_ARG_CHECKED(JSTypedArray, result, 1);
  RUNTIME_ASSERT(result->byte_length() == Smi::FromInt(2 * sizeof(double)));
  FixedFloat64Array* array = FixedFloat64Array::cast(result->elements());
  double* y = static_cast<double*>(array->DataPtr());
  return Smi::FromInt(fdlibm::rempio2(x, y));
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
  lazily_initialize_fast_exp(isolate);
  return *isolate->factory()->NewNumber(fast_exp(x, isolate));
}


RUNTIME_FUNCTION(Runtime_MathClz32) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  isolate->counters()->math_clz32()->Increment();

  CONVERT_NUMBER_CHECKED(uint32_t, x, Uint32, args[0]);
  return *isolate->factory()->NewNumberFromUint(
      base::bits::CountLeadingZeros32(x));
}


RUNTIME_FUNCTION(Runtime_MathFloor) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  isolate->counters()->math_floor()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return *isolate->factory()->NewNumber(Floor(x));
}


// Slow version of Math.pow.  We check for fast paths for special cases.
// Used if VFP3 is not available.
RUNTIME_FUNCTION(Runtime_MathPow) {
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
  double result = power_helper(isolate, x, y);
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


RUNTIME_FUNCTION(Runtime_MathSqrt) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  isolate->counters()->math_sqrt()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  lazily_initialize_fast_sqrt(isolate);
  return *isolate->factory()->NewNumber(fast_sqrt(x, isolate));
}


RUNTIME_FUNCTION(Runtime_MathFround) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  float xf = DoubleToFloat32(x);
  return *isolate->factory()->NewNumber(xf);
}


RUNTIME_FUNCTION(Runtime_IsMinusZero) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (!obj->IsHeapNumber()) return isolate->heap()->false_value();
  HeapNumber* number = HeapNumber::cast(obj);
  return isolate->heap()->ToBoolean(IsMinusZero(number->value()));
}


RUNTIME_FUNCTION(Runtime_GenerateRandomNumbers) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  // Random numbers in the snapshot are not really that random.
  DCHECK(!isolate->bootstrapper()->IsActive());
  static const int kState0Offset = 0;
  static const int kState1Offset = 1;
  static const int kRandomBatchSize = 64;
  CONVERT_ARG_HANDLE_CHECKED(Object, maybe_typed_array, 0);
  Handle<JSTypedArray> typed_array;
  // Allocate typed array if it does not yet exist.
  if (maybe_typed_array->IsJSTypedArray()) {
    typed_array = Handle<JSTypedArray>::cast(maybe_typed_array);
  } else {
    static const int kByteLength = kRandomBatchSize * kDoubleSize;
    Handle<JSArrayBuffer> buffer =
        isolate->factory()->NewJSArrayBuffer(SharedFlag::kNotShared, TENURED);
    JSArrayBuffer::SetupAllocatingData(buffer, isolate, kByteLength, true,
                                       SharedFlag::kNotShared);
    typed_array = isolate->factory()->NewJSTypedArray(
        kExternalFloat64Array, buffer, 0, kRandomBatchSize);
  }

  DisallowHeapAllocation no_gc;
  double* array =
      reinterpret_cast<double*>(typed_array->GetBuffer()->backing_store());
  // Fetch existing state.
  uint64_t state0 = double_to_uint64(array[kState0Offset]);
  uint64_t state1 = double_to_uint64(array[kState1Offset]);
  // Initialize state if not yet initialized.
  while (state0 == 0 || state1 == 0) {
    isolate->random_number_generator()->NextBytes(&state0, sizeof(state0));
    isolate->random_number_generator()->NextBytes(&state1, sizeof(state1));
  }
  // Create random numbers.
  for (int i = kState1Offset + 1; i < kRandomBatchSize; i++) {
    // Generate random numbers using xorshift128+.
    base::RandomNumberGenerator::XorShift128(&state0, &state1);
    array[i] = base::RandomNumberGenerator::ToDouble(state0, state1);
  }
  // Persist current state.
  array[kState0Offset] = uint64_to_double(state0);
  array[kState1Offset] = uint64_to_double(state1);
  return *typed_array;
}
}  // namespace internal
}  // namespace v8
