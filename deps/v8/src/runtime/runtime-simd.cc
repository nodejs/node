// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/base/macros.h"
#include "src/conversions.h"
#include "src/factory.h"
#include "src/objects-inl.h"

// Implement Single Instruction Multiple Data (SIMD) operations as defined in
// the SIMD.js draft spec:
// http://littledan.github.io/simd.html

namespace v8 {
namespace internal {

namespace {

// Functions to convert Numbers to SIMD component types.

template <typename T, typename F>
static bool CanCast(F from) {
  // A float can't represent 2^31 - 1 or 2^32 - 1 exactly, so promote the limits
  // to double. Otherwise, the limit is truncated and numbers like 2^31 or 2^32
  // get through, causing any static_cast to be undefined.
  from = trunc(from);
  return from >= static_cast<double>(std::numeric_limits<T>::min()) &&
         from <= static_cast<double>(std::numeric_limits<T>::max());
}


// Explicitly specialize for conversions to float, which always succeed.
template <>
bool CanCast<float>(int32_t from) {
  return true;
}


template <>
bool CanCast<float>(uint32_t from) {
  return true;
}


template <typename T>
static T ConvertNumber(double number);


template <>
float ConvertNumber<float>(double number) {
  return DoubleToFloat32(number);
}


template <>
int32_t ConvertNumber<int32_t>(double number) {
  return DoubleToInt32(number);
}


template <>
uint32_t ConvertNumber<uint32_t>(double number) {
  return DoubleToUint32(number);
}


template <>
int16_t ConvertNumber<int16_t>(double number) {
  return static_cast<int16_t>(DoubleToInt32(number));
}


template <>
uint16_t ConvertNumber<uint16_t>(double number) {
  return static_cast<uint16_t>(DoubleToUint32(number));
}


template <>
int8_t ConvertNumber<int8_t>(double number) {
  return static_cast<int8_t>(DoubleToInt32(number));
}


template <>
uint8_t ConvertNumber<uint8_t>(double number) {
  return static_cast<uint8_t>(DoubleToUint32(number));
}


// TODO(bbudge): Make this consistent with SIMD instruction results.
inline float RecipApprox(float a) { return 1.0f / a; }


// TODO(bbudge): Make this consistent with SIMD instruction results.
inline float RecipSqrtApprox(float a) { return 1.0f / std::sqrt(a); }


// Saturating addition for int16_t and int8_t.
template <typename T>
inline T AddSaturate(T a, T b) {
  const T max = std::numeric_limits<T>::max();
  const T min = std::numeric_limits<T>::min();
  int32_t result = a + b;
  if (result > max) return max;
  if (result < min) return min;
  return result;
}


// Saturating subtraction for int16_t and int8_t.
template <typename T>
inline T SubSaturate(T a, T b) {
  const T max = std::numeric_limits<T>::max();
  const T min = std::numeric_limits<T>::min();
  int32_t result = a - b;
  if (result > max) return max;
  if (result < min) return min;
  return result;
}


inline float Min(float a, float b) {
  if (a < b) return a;
  if (a > b) return b;
  if (a == b) return std::signbit(a) ? a : b;
  return std::numeric_limits<float>::quiet_NaN();
}


inline float Max(float a, float b) {
  if (a > b) return a;
  if (a < b) return b;
  if (a == b) return std::signbit(b) ? a : b;
  return std::numeric_limits<float>::quiet_NaN();
}


inline float MinNumber(float a, float b) {
  if (std::isnan(a)) return b;
  if (std::isnan(b)) return a;
  return Min(a, b);
}


inline float MaxNumber(float a, float b) {
  if (std::isnan(a)) return b;
  if (std::isnan(b)) return a;
  return Max(a, b);
}

}  // namespace

//-------------------------------------------------------------------

// SIMD helper functions.

RUNTIME_FUNCTION(Runtime_IsSimdValue) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  return isolate->heap()->ToBoolean(args[0]->IsSimd128Value());
}


//-------------------------------------------------------------------

// Utility macros.

// TODO(gdeepti): Fix to use ToNumber conversion once polyfill is updated.
#define CONVERT_SIMD_LANE_ARG_CHECKED(name, index, lanes)            \
  Handle<Object> name_object = args.at<Object>(index);               \
  if (!name_object->IsNumber()) {                                    \
    THROW_NEW_ERROR_RETURN_FAILURE(                                  \
        isolate, NewTypeError(MessageTemplate::kInvalidSimdIndex));  \
  }                                                                  \
  double number = name_object->Number();                             \
  if (number < 0 || number >= lanes || !IsInt32Double(number)) {     \
    THROW_NEW_ERROR_RETURN_FAILURE(                                  \
        isolate, NewRangeError(MessageTemplate::kInvalidSimdIndex)); \
  }                                                                  \
  uint32_t name = static_cast<uint32_t>(number);

#define CONVERT_SIMD_ARG_HANDLE_THROW(Type, name, index)                \
  Handle<Type> name;                                                    \
  if (args[index]->Is##Type()) {                                        \
    name = args.at<Type>(index);                                        \
  } else {                                                              \
    THROW_NEW_ERROR_RETURN_FAILURE(                                     \
        isolate, NewTypeError(MessageTemplate::kInvalidSimdOperation)); \
  }

#define SIMD_UNARY_OP(type, lane_type, lane_count, op, result) \
  static const int kLaneCount = lane_count;                    \
  DCHECK(args.length() == 1);                                  \
  CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 0);                   \
  lane_type lanes[kLaneCount];                                 \
  for (int i = 0; i < kLaneCount; i++) {                       \
    lanes[i] = op(a->get_lane(i));                             \
  }                                                            \
  Handle<type> result = isolate->factory()->New##type(lanes);

#define SIMD_BINARY_OP(type, lane_type, lane_count, op, result) \
  static const int kLaneCount = lane_count;                     \
  DCHECK(args.length() == 2);                                   \
  CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 0);                    \
  CONVERT_SIMD_ARG_HANDLE_THROW(type, b, 1);                    \
  lane_type lanes[kLaneCount];                                  \
  for (int i = 0; i < kLaneCount; i++) {                        \
    lanes[i] = op(a->get_lane(i), b->get_lane(i));              \
  }                                                             \
  Handle<type> result = isolate->factory()->New##type(lanes);

#define SIMD_RELATIONAL_OP(type, bool_type, lane_count, a, b, op, result) \
  static const int kLaneCount = lane_count;                               \
  DCHECK(args.length() == 2);                                             \
  CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 0);                              \
  CONVERT_SIMD_ARG_HANDLE_THROW(type, b, 1);                              \
  bool lanes[kLaneCount];                                                 \
  for (int i = 0; i < kLaneCount; i++) {                                  \
    lanes[i] = a->get_lane(i) op b->get_lane(i);                          \
  }                                                                       \
  Handle<bool_type> result = isolate->factory()->New##bool_type(lanes);

//-------------------------------------------------------------------

// Common functions.

#define GET_NUMERIC_ARG(lane_type, name, index)              \
  Handle<Object> a;                                          \
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                        \
      isolate, a, Object::ToNumber(args.at<Object>(index))); \
  name = ConvertNumber<lane_type>(a->Number());

#define GET_BOOLEAN_ARG(lane_type, name, index) \
  name = args[index]->BooleanValue();

#define SIMD_ALL_TYPES(FUNCTION)                              \
  FUNCTION(Float32x4, float, 4, NewNumber, GET_NUMERIC_ARG)   \
  FUNCTION(Int32x4, int32_t, 4, NewNumber, GET_NUMERIC_ARG)   \
  FUNCTION(Uint32x4, uint32_t, 4, NewNumber, GET_NUMERIC_ARG) \
  FUNCTION(Bool32x4, bool, 4, ToBoolean, GET_BOOLEAN_ARG)     \
  FUNCTION(Int16x8, int16_t, 8, NewNumber, GET_NUMERIC_ARG)   \
  FUNCTION(Uint16x8, uint16_t, 8, NewNumber, GET_NUMERIC_ARG) \
  FUNCTION(Bool16x8, bool, 8, ToBoolean, GET_BOOLEAN_ARG)     \
  FUNCTION(Int8x16, int8_t, 16, NewNumber, GET_NUMERIC_ARG)   \
  FUNCTION(Uint8x16, uint8_t, 16, NewNumber, GET_NUMERIC_ARG) \
  FUNCTION(Bool8x16, bool, 16, ToBoolean, GET_BOOLEAN_ARG)

#define SIMD_CREATE_FUNCTION(type, lane_type, lane_count, extract, replace) \
  RUNTIME_FUNCTION(Runtime_Create##type) {                                  \
    static const int kLaneCount = lane_count;                               \
    HandleScope scope(isolate);                                             \
    DCHECK(args.length() == kLaneCount);                                    \
    lane_type lanes[kLaneCount];                                            \
    for (int i = 0; i < kLaneCount; i++) {                                  \
      replace(lane_type, lanes[i], i)                                       \
    }                                                                       \
    return *isolate->factory()->New##type(lanes);                           \
  }

#define SIMD_EXTRACT_FUNCTION(type, lane_type, lane_count, extract, replace) \
  RUNTIME_FUNCTION(Runtime_##type##ExtractLane) {                            \
    HandleScope scope(isolate);                                              \
    DCHECK(args.length() == 2);                                              \
    CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 0);                               \
    CONVERT_SIMD_LANE_ARG_CHECKED(lane, 1, lane_count);                      \
    return *isolate->factory()->extract(a->get_lane(lane));                  \
  }

#define SIMD_REPLACE_FUNCTION(type, lane_type, lane_count, extract, replace) \
  RUNTIME_FUNCTION(Runtime_##type##ReplaceLane) {                            \
    static const int kLaneCount = lane_count;                                \
    HandleScope scope(isolate);                                              \
    DCHECK(args.length() == 3);                                              \
    CONVERT_SIMD_ARG_HANDLE_THROW(type, simd, 0);                            \
    CONVERT_SIMD_LANE_ARG_CHECKED(lane, 1, kLaneCount);                      \
    lane_type lanes[kLaneCount];                                             \
    for (int i = 0; i < kLaneCount; i++) {                                   \
      lanes[i] = simd->get_lane(i);                                          \
    }                                                                        \
    replace(lane_type, lanes[lane], 2);                                      \
    Handle<type> result = isolate->factory()->New##type(lanes);              \
    return *result;                                                          \
  }

#define SIMD_CHECK_FUNCTION(type, lane_type, lane_count, extract, replace) \
  RUNTIME_FUNCTION(Runtime_##type##Check) {                                \
    HandleScope scope(isolate);                                            \
    CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 0);                             \
    return *a;                                                             \
  }

#define SIMD_SWIZZLE_FUNCTION(type, lane_type, lane_count, extract, replace) \
  RUNTIME_FUNCTION(Runtime_##type##Swizzle) {                                \
    static const int kLaneCount = lane_count;                                \
    HandleScope scope(isolate);                                              \
    DCHECK(args.length() == 1 + kLaneCount);                                 \
    CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 0);                               \
    lane_type lanes[kLaneCount];                                             \
    for (int i = 0; i < kLaneCount; i++) {                                   \
      CONVERT_SIMD_LANE_ARG_CHECKED(index, i + 1, kLaneCount);               \
      lanes[i] = a->get_lane(index);                                         \
    }                                                                        \
    Handle<type> result = isolate->factory()->New##type(lanes);              \
    return *result;                                                          \
  }

#define SIMD_SHUFFLE_FUNCTION(type, lane_type, lane_count, extract, replace) \
  RUNTIME_FUNCTION(Runtime_##type##Shuffle) {                                \
    static const int kLaneCount = lane_count;                                \
    HandleScope scope(isolate);                                              \
    DCHECK(args.length() == 2 + kLaneCount);                                 \
    CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 0);                               \
    CONVERT_SIMD_ARG_HANDLE_THROW(type, b, 1);                               \
    lane_type lanes[kLaneCount];                                             \
    for (int i = 0; i < kLaneCount; i++) {                                   \
      CONVERT_SIMD_LANE_ARG_CHECKED(index, i + 2, kLaneCount * 2);           \
      lanes[i] = index < kLaneCount ? a->get_lane(index)                     \
                                    : b->get_lane(index - kLaneCount);       \
    }                                                                        \
    Handle<type> result = isolate->factory()->New##type(lanes);              \
    return *result;                                                          \
  }

SIMD_ALL_TYPES(SIMD_CREATE_FUNCTION)
SIMD_ALL_TYPES(SIMD_EXTRACT_FUNCTION)
SIMD_ALL_TYPES(SIMD_REPLACE_FUNCTION)
SIMD_ALL_TYPES(SIMD_CHECK_FUNCTION)
SIMD_ALL_TYPES(SIMD_SWIZZLE_FUNCTION)
SIMD_ALL_TYPES(SIMD_SHUFFLE_FUNCTION)

//-------------------------------------------------------------------

// Float-only functions.

#define SIMD_ABS_FUNCTION(type, lane_type, lane_count)            \
  RUNTIME_FUNCTION(Runtime_##type##Abs) {                         \
    HandleScope scope(isolate);                                   \
    SIMD_UNARY_OP(type, lane_type, lane_count, std::abs, result); \
    return *result;                                               \
  }

#define SIMD_SQRT_FUNCTION(type, lane_type, lane_count)            \
  RUNTIME_FUNCTION(Runtime_##type##Sqrt) {                         \
    HandleScope scope(isolate);                                    \
    SIMD_UNARY_OP(type, lane_type, lane_count, std::sqrt, result); \
    return *result;                                                \
  }

#define SIMD_RECIP_APPROX_FUNCTION(type, lane_type, lane_count)      \
  RUNTIME_FUNCTION(Runtime_##type##RecipApprox) {                    \
    HandleScope scope(isolate);                                      \
    SIMD_UNARY_OP(type, lane_type, lane_count, RecipApprox, result); \
    return *result;                                                  \
  }

#define SIMD_RECIP_SQRT_APPROX_FUNCTION(type, lane_type, lane_count)     \
  RUNTIME_FUNCTION(Runtime_##type##RecipSqrtApprox) {                    \
    HandleScope scope(isolate);                                          \
    SIMD_UNARY_OP(type, lane_type, lane_count, RecipSqrtApprox, result); \
    return *result;                                                      \
  }

#define BINARY_DIV(a, b) (a) / (b)
#define SIMD_DIV_FUNCTION(type, lane_type, lane_count)               \
  RUNTIME_FUNCTION(Runtime_##type##Div) {                            \
    HandleScope scope(isolate);                                      \
    SIMD_BINARY_OP(type, lane_type, lane_count, BINARY_DIV, result); \
    return *result;                                                  \
  }

#define SIMD_MINNUM_FUNCTION(type, lane_type, lane_count)           \
  RUNTIME_FUNCTION(Runtime_##type##MinNum) {                        \
    HandleScope scope(isolate);                                     \
    SIMD_BINARY_OP(type, lane_type, lane_count, MinNumber, result); \
    return *result;                                                 \
  }

#define SIMD_MAXNUM_FUNCTION(type, lane_type, lane_count)           \
  RUNTIME_FUNCTION(Runtime_##type##MaxNum) {                        \
    HandleScope scope(isolate);                                     \
    SIMD_BINARY_OP(type, lane_type, lane_count, MaxNumber, result); \
    return *result;                                                 \
  }

SIMD_ABS_FUNCTION(Float32x4, float, 4)
SIMD_SQRT_FUNCTION(Float32x4, float, 4)
SIMD_RECIP_APPROX_FUNCTION(Float32x4, float, 4)
SIMD_RECIP_SQRT_APPROX_FUNCTION(Float32x4, float, 4)
SIMD_DIV_FUNCTION(Float32x4, float, 4)
SIMD_MINNUM_FUNCTION(Float32x4, float, 4)
SIMD_MAXNUM_FUNCTION(Float32x4, float, 4)

//-------------------------------------------------------------------

// Int-only functions.

#define SIMD_INT_TYPES(FUNCTION)    \
  FUNCTION(Int32x4, int32_t, 32, 4) \
  FUNCTION(Int16x8, int16_t, 16, 8) \
  FUNCTION(Int8x16, int8_t, 8, 16)

#define SIMD_UINT_TYPES(FUNCTION)     \
  FUNCTION(Uint32x4, uint32_t, 32, 4) \
  FUNCTION(Uint16x8, uint16_t, 16, 8) \
  FUNCTION(Uint8x16, uint8_t, 8, 16)

#define CONVERT_SHIFT_ARG_CHECKED(name, index)                          \
  Handle<Object> name_object = args.at<Object>(index);                  \
  if (!name_object->IsNumber()) {                                       \
    THROW_NEW_ERROR_RETURN_FAILURE(                                     \
        isolate, NewTypeError(MessageTemplate::kInvalidSimdOperation)); \
  }                                                                     \
  int32_t signed_shift = 0;                                             \
  args[index]->ToInt32(&signed_shift);                                  \
  uint32_t name = bit_cast<uint32_t>(signed_shift);

#define SIMD_LSL_FUNCTION(type, lane_type, lane_bits, lane_count) \
  RUNTIME_FUNCTION(Runtime_##type##ShiftLeftByScalar) {           \
    static const int kLaneCount = lane_count;                     \
    HandleScope scope(isolate);                                   \
    DCHECK(args.length() == 2);                                   \
    CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 0);                    \
    CONVERT_SHIFT_ARG_CHECKED(shift, 1);                          \
    lane_type lanes[kLaneCount] = {0};                            \
    shift &= lane_bits - 1;                                       \
    for (int i = 0; i < kLaneCount; i++) {                        \
      lanes[i] = a->get_lane(i) << shift;                         \
    }                                                             \
    Handle<type> result = isolate->factory()->New##type(lanes);   \
    return *result;                                               \
  }

#define SIMD_LSR_FUNCTION(type, lane_type, lane_bits, lane_count)              \
  RUNTIME_FUNCTION(Runtime_##type##ShiftRightByScalar) {                       \
    static const int kLaneCount = lane_count;                                  \
    HandleScope scope(isolate);                                                \
    DCHECK(args.length() == 2);                                                \
    CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 0);                                 \
    CONVERT_SHIFT_ARG_CHECKED(shift, 1);                                       \
    lane_type lanes[kLaneCount] = {0};                                         \
    shift &= lane_bits - 1;                                                    \
    for (int i = 0; i < kLaneCount; i++) {                                     \
      lanes[i] = static_cast<lane_type>(bit_cast<lane_type>(a->get_lane(i)) >> \
                                        shift);                                \
    }                                                                          \
    Handle<type> result = isolate->factory()->New##type(lanes);                \
    return *result;                                                            \
  }

#define SIMD_ASR_FUNCTION(type, lane_type, lane_bits, lane_count)      \
  RUNTIME_FUNCTION(Runtime_##type##ShiftRightByScalar) {               \
    static const int kLaneCount = lane_count;                          \
    HandleScope scope(isolate);                                        \
    DCHECK(args.length() == 2);                                        \
    CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 0);                         \
    CONVERT_SHIFT_ARG_CHECKED(shift, 1);                               \
    shift &= lane_bits - 1;                                            \
    lane_type lanes[kLaneCount];                                       \
    for (int i = 0; i < kLaneCount; i++) {                             \
      int64_t shifted = static_cast<int64_t>(a->get_lane(i)) >> shift; \
      lanes[i] = static_cast<lane_type>(shifted);                      \
    }                                                                  \
    Handle<type> result = isolate->factory()->New##type(lanes);        \
    return *result;                                                    \
  }

SIMD_INT_TYPES(SIMD_LSL_FUNCTION)
SIMD_UINT_TYPES(SIMD_LSL_FUNCTION)
SIMD_INT_TYPES(SIMD_ASR_FUNCTION)
SIMD_UINT_TYPES(SIMD_LSR_FUNCTION)

//-------------------------------------------------------------------

// Bool-only functions.

#define SIMD_BOOL_TYPES(FUNCTION) \
  FUNCTION(Bool32x4, 4)           \
  FUNCTION(Bool16x8, 8)           \
  FUNCTION(Bool8x16, 16)

#define SIMD_ANY_FUNCTION(type, lane_count)    \
  RUNTIME_FUNCTION(Runtime_##type##AnyTrue) {  \
    HandleScope scope(isolate);                \
    DCHECK(args.length() == 1);                \
    CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 0); \
    bool result = false;                       \
    for (int i = 0; i < lane_count; i++) {     \
      if (a->get_lane(i)) {                    \
        result = true;                         \
        break;                                 \
      }                                        \
    }                                          \
    return isolate->heap()->ToBoolean(result); \
  }

#define SIMD_ALL_FUNCTION(type, lane_count)    \
  RUNTIME_FUNCTION(Runtime_##type##AllTrue) {  \
    HandleScope scope(isolate);                \
    DCHECK(args.length() == 1);                \
    CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 0); \
    bool result = true;                        \
    for (int i = 0; i < lane_count; i++) {     \
      if (!a->get_lane(i)) {                   \
        result = false;                        \
        break;                                 \
      }                                        \
    }                                          \
    return isolate->heap()->ToBoolean(result); \
  }

SIMD_BOOL_TYPES(SIMD_ANY_FUNCTION)
SIMD_BOOL_TYPES(SIMD_ALL_FUNCTION)

//-------------------------------------------------------------------

// Small Int-only functions.

#define SIMD_SMALL_INT_TYPES(FUNCTION) \
  FUNCTION(Int16x8, int16_t, 8)        \
  FUNCTION(Uint16x8, uint16_t, 8)      \
  FUNCTION(Int8x16, int8_t, 16)        \
  FUNCTION(Uint8x16, uint8_t, 16)

#define SIMD_ADD_SATURATE_FUNCTION(type, lane_type, lane_count)       \
  RUNTIME_FUNCTION(Runtime_##type##AddSaturate) {                     \
    HandleScope scope(isolate);                                       \
    SIMD_BINARY_OP(type, lane_type, lane_count, AddSaturate, result); \
    return *result;                                                   \
  }

#define BINARY_SUB(a, b) (a) - (b)
#define SIMD_SUB_SATURATE_FUNCTION(type, lane_type, lane_count)       \
  RUNTIME_FUNCTION(Runtime_##type##SubSaturate) {                     \
    HandleScope scope(isolate);                                       \
    SIMD_BINARY_OP(type, lane_type, lane_count, SubSaturate, result); \
    return *result;                                                   \
  }

SIMD_SMALL_INT_TYPES(SIMD_ADD_SATURATE_FUNCTION)
SIMD_SMALL_INT_TYPES(SIMD_SUB_SATURATE_FUNCTION)

//-------------------------------------------------------------------

// Numeric functions.

#define SIMD_NUMERIC_TYPES(FUNCTION) \
  FUNCTION(Float32x4, float, 4)      \
  FUNCTION(Int32x4, int32_t, 4)      \
  FUNCTION(Uint32x4, uint32_t, 4)    \
  FUNCTION(Int16x8, int16_t, 8)      \
  FUNCTION(Uint16x8, uint16_t, 8)    \
  FUNCTION(Int8x16, int8_t, 16)      \
  FUNCTION(Uint8x16, uint8_t, 16)

#define BINARY_ADD(a, b) (a) + (b)
#define SIMD_ADD_FUNCTION(type, lane_type, lane_count)               \
  RUNTIME_FUNCTION(Runtime_##type##Add) {                            \
    HandleScope scope(isolate);                                      \
    SIMD_BINARY_OP(type, lane_type, lane_count, BINARY_ADD, result); \
    return *result;                                                  \
  }

#define BINARY_SUB(a, b) (a) - (b)
#define SIMD_SUB_FUNCTION(type, lane_type, lane_count)               \
  RUNTIME_FUNCTION(Runtime_##type##Sub) {                            \
    HandleScope scope(isolate);                                      \
    SIMD_BINARY_OP(type, lane_type, lane_count, BINARY_SUB, result); \
    return *result;                                                  \
  }

#define BINARY_MUL(a, b) (a) * (b)
#define SIMD_MUL_FUNCTION(type, lane_type, lane_count)               \
  RUNTIME_FUNCTION(Runtime_##type##Mul) {                            \
    HandleScope scope(isolate);                                      \
    SIMD_BINARY_OP(type, lane_type, lane_count, BINARY_MUL, result); \
    return *result;                                                  \
  }

#define SIMD_MIN_FUNCTION(type, lane_type, lane_count)        \
  RUNTIME_FUNCTION(Runtime_##type##Min) {                     \
    HandleScope scope(isolate);                               \
    SIMD_BINARY_OP(type, lane_type, lane_count, Min, result); \
    return *result;                                           \
  }

#define SIMD_MAX_FUNCTION(type, lane_type, lane_count)        \
  RUNTIME_FUNCTION(Runtime_##type##Max) {                     \
    HandleScope scope(isolate);                               \
    SIMD_BINARY_OP(type, lane_type, lane_count, Max, result); \
    return *result;                                           \
  }

SIMD_NUMERIC_TYPES(SIMD_ADD_FUNCTION)
SIMD_NUMERIC_TYPES(SIMD_SUB_FUNCTION)
SIMD_NUMERIC_TYPES(SIMD_MUL_FUNCTION)
SIMD_NUMERIC_TYPES(SIMD_MIN_FUNCTION)
SIMD_NUMERIC_TYPES(SIMD_MAX_FUNCTION)

//-------------------------------------------------------------------

// Relational functions.

#define SIMD_RELATIONAL_TYPES(FUNCTION) \
  FUNCTION(Float32x4, Bool32x4, 4)      \
  FUNCTION(Int32x4, Bool32x4, 4)        \
  FUNCTION(Uint32x4, Bool32x4, 4)       \
  FUNCTION(Int16x8, Bool16x8, 8)        \
  FUNCTION(Uint16x8, Bool16x8, 8)       \
  FUNCTION(Int8x16, Bool8x16, 16)       \
  FUNCTION(Uint8x16, Bool8x16, 16)

#define SIMD_EQUALITY_TYPES(FUNCTION) \
  SIMD_RELATIONAL_TYPES(FUNCTION)     \
  FUNCTION(Bool32x4, Bool32x4, 4)     \
  FUNCTION(Bool16x8, Bool16x8, 8)     \
  FUNCTION(Bool8x16, Bool8x16, 16)

#define SIMD_EQUAL_FUNCTION(type, bool_type, lane_count)               \
  RUNTIME_FUNCTION(Runtime_##type##Equal) {                            \
    HandleScope scope(isolate);                                        \
    SIMD_RELATIONAL_OP(type, bool_type, lane_count, a, b, ==, result); \
    return *result;                                                    \
  }

#define SIMD_NOT_EQUAL_FUNCTION(type, bool_type, lane_count)           \
  RUNTIME_FUNCTION(Runtime_##type##NotEqual) {                         \
    HandleScope scope(isolate);                                        \
    SIMD_RELATIONAL_OP(type, bool_type, lane_count, a, b, !=, result); \
    return *result;                                                    \
  }

SIMD_EQUALITY_TYPES(SIMD_EQUAL_FUNCTION)
SIMD_EQUALITY_TYPES(SIMD_NOT_EQUAL_FUNCTION)

#define SIMD_LESS_THAN_FUNCTION(type, bool_type, lane_count)          \
  RUNTIME_FUNCTION(Runtime_##type##LessThan) {                        \
    HandleScope scope(isolate);                                       \
    SIMD_RELATIONAL_OP(type, bool_type, lane_count, a, b, <, result); \
    return *result;                                                   \
  }

#define SIMD_LESS_THAN_OR_EQUAL_FUNCTION(type, bool_type, lane_count)  \
  RUNTIME_FUNCTION(Runtime_##type##LessThanOrEqual) {                  \
    HandleScope scope(isolate);                                        \
    SIMD_RELATIONAL_OP(type, bool_type, lane_count, a, b, <=, result); \
    return *result;                                                    \
  }

#define SIMD_GREATER_THAN_FUNCTION(type, bool_type, lane_count)       \
  RUNTIME_FUNCTION(Runtime_##type##GreaterThan) {                     \
    HandleScope scope(isolate);                                       \
    SIMD_RELATIONAL_OP(type, bool_type, lane_count, a, b, >, result); \
    return *result;                                                   \
  }

#define SIMD_GREATER_THAN_OR_EQUAL_FUNCTION(type, bool_type, lane_count) \
  RUNTIME_FUNCTION(Runtime_##type##GreaterThanOrEqual) {                 \
    HandleScope scope(isolate);                                          \
    SIMD_RELATIONAL_OP(type, bool_type, lane_count, a, b, >=, result);   \
    return *result;                                                      \
  }

SIMD_RELATIONAL_TYPES(SIMD_LESS_THAN_FUNCTION)
SIMD_RELATIONAL_TYPES(SIMD_LESS_THAN_OR_EQUAL_FUNCTION)
SIMD_RELATIONAL_TYPES(SIMD_GREATER_THAN_FUNCTION)
SIMD_RELATIONAL_TYPES(SIMD_GREATER_THAN_OR_EQUAL_FUNCTION)

//-------------------------------------------------------------------

// Logical functions.

#define SIMD_LOGICAL_TYPES(FUNCTION)    \
  FUNCTION(Int32x4, int32_t, 4, _INT)   \
  FUNCTION(Uint32x4, uint32_t, 4, _INT) \
  FUNCTION(Int16x8, int16_t, 8, _INT)   \
  FUNCTION(Uint16x8, uint16_t, 8, _INT) \
  FUNCTION(Int8x16, int8_t, 16, _INT)   \
  FUNCTION(Uint8x16, uint8_t, 16, _INT) \
  FUNCTION(Bool32x4, bool, 4, _BOOL)    \
  FUNCTION(Bool16x8, bool, 8, _BOOL)    \
  FUNCTION(Bool8x16, bool, 16, _BOOL)

#define BINARY_AND_INT(a, b) (a) & (b)
#define BINARY_AND_BOOL(a, b) (a) && (b)
#define SIMD_AND_FUNCTION(type, lane_type, lane_count, op)               \
  RUNTIME_FUNCTION(Runtime_##type##And) {                                \
    HandleScope scope(isolate);                                          \
    SIMD_BINARY_OP(type, lane_type, lane_count, BINARY_AND##op, result); \
    return *result;                                                      \
  }

#define BINARY_OR_INT(a, b) (a) | (b)
#define BINARY_OR_BOOL(a, b) (a) || (b)
#define SIMD_OR_FUNCTION(type, lane_type, lane_count, op)               \
  RUNTIME_FUNCTION(Runtime_##type##Or) {                                \
    HandleScope scope(isolate);                                         \
    SIMD_BINARY_OP(type, lane_type, lane_count, BINARY_OR##op, result); \
    return *result;                                                     \
  }

#define BINARY_XOR_INT(a, b) (a) ^ (b)
#define BINARY_XOR_BOOL(a, b) (a) != (b)
#define SIMD_XOR_FUNCTION(type, lane_type, lane_count, op)               \
  RUNTIME_FUNCTION(Runtime_##type##Xor) {                                \
    HandleScope scope(isolate);                                          \
    SIMD_BINARY_OP(type, lane_type, lane_count, BINARY_XOR##op, result); \
    return *result;                                                      \
  }

#define UNARY_NOT_INT ~
#define UNARY_NOT_BOOL !
#define SIMD_NOT_FUNCTION(type, lane_type, lane_count, op)             \
  RUNTIME_FUNCTION(Runtime_##type##Not) {                              \
    HandleScope scope(isolate);                                        \
    SIMD_UNARY_OP(type, lane_type, lane_count, UNARY_NOT##op, result); \
    return *result;                                                    \
  }

SIMD_LOGICAL_TYPES(SIMD_AND_FUNCTION)
SIMD_LOGICAL_TYPES(SIMD_OR_FUNCTION)
SIMD_LOGICAL_TYPES(SIMD_XOR_FUNCTION)
SIMD_LOGICAL_TYPES(SIMD_NOT_FUNCTION)

//-------------------------------------------------------------------

// Select functions.

#define SIMD_SELECT_TYPES(FUNCTION)         \
  FUNCTION(Float32x4, float, Bool32x4, 4)   \
  FUNCTION(Int32x4, int32_t, Bool32x4, 4)   \
  FUNCTION(Uint32x4, uint32_t, Bool32x4, 4) \
  FUNCTION(Int16x8, int16_t, Bool16x8, 8)   \
  FUNCTION(Uint16x8, uint16_t, Bool16x8, 8) \
  FUNCTION(Int8x16, int8_t, Bool8x16, 16)   \
  FUNCTION(Uint8x16, uint8_t, Bool8x16, 16)

#define SIMD_SELECT_FUNCTION(type, lane_type, bool_type, lane_count)  \
  RUNTIME_FUNCTION(Runtime_##type##Select) {                          \
    static const int kLaneCount = lane_count;                         \
    HandleScope scope(isolate);                                       \
    DCHECK(args.length() == 3);                                       \
    CONVERT_SIMD_ARG_HANDLE_THROW(bool_type, mask, 0);                \
    CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 1);                        \
    CONVERT_SIMD_ARG_HANDLE_THROW(type, b, 2);                        \
    lane_type lanes[kLaneCount];                                      \
    for (int i = 0; i < kLaneCount; i++) {                            \
      lanes[i] = mask->get_lane(i) ? a->get_lane(i) : b->get_lane(i); \
    }                                                                 \
    Handle<type> result = isolate->factory()->New##type(lanes);       \
    return *result;                                                   \
  }

SIMD_SELECT_TYPES(SIMD_SELECT_FUNCTION)

//-------------------------------------------------------------------

// Signed / unsigned functions.

#define SIMD_SIGNED_TYPES(FUNCTION) \
  FUNCTION(Float32x4, float, 4)     \
  FUNCTION(Int32x4, int32_t, 4)     \
  FUNCTION(Int16x8, int16_t, 8)     \
  FUNCTION(Int8x16, int8_t, 16)

#define SIMD_NEG_FUNCTION(type, lane_type, lane_count)     \
  RUNTIME_FUNCTION(Runtime_##type##Neg) {                  \
    HandleScope scope(isolate);                            \
    SIMD_UNARY_OP(type, lane_type, lane_count, -, result); \
    return *result;                                        \
  }

SIMD_SIGNED_TYPES(SIMD_NEG_FUNCTION)

//-------------------------------------------------------------------

// Casting functions.

#define SIMD_FROM_TYPES(FUNCTION)                   \
  FUNCTION(Float32x4, float, 4, Int32x4, int32_t)   \
  FUNCTION(Float32x4, float, 4, Uint32x4, uint32_t) \
  FUNCTION(Int32x4, int32_t, 4, Float32x4, float)   \
  FUNCTION(Int32x4, int32_t, 4, Uint32x4, uint32_t) \
  FUNCTION(Uint32x4, uint32_t, 4, Float32x4, float) \
  FUNCTION(Uint32x4, uint32_t, 4, Int32x4, int32_t) \
  FUNCTION(Int16x8, int16_t, 8, Uint16x8, uint16_t) \
  FUNCTION(Uint16x8, uint16_t, 8, Int16x8, int16_t) \
  FUNCTION(Int8x16, int8_t, 16, Uint8x16, uint8_t)  \
  FUNCTION(Uint8x16, uint8_t, 16, Int8x16, int8_t)

#define SIMD_FROM_FUNCTION(type, lane_type, lane_count, from_type, from_ctype) \
  RUNTIME_FUNCTION(Runtime_##type##From##from_type) {                          \
    static const int kLaneCount = lane_count;                                  \
    HandleScope scope(isolate);                                                \
    DCHECK(args.length() == 1);                                                \
    CONVERT_SIMD_ARG_HANDLE_THROW(from_type, a, 0);                            \
    lane_type lanes[kLaneCount];                                               \
    for (int i = 0; i < kLaneCount; i++) {                                     \
      from_ctype a_value = a->get_lane(i);                                     \
      if (a_value != a_value || !CanCast<lane_type>(a_value)) {                \
        THROW_NEW_ERROR_RETURN_FAILURE(                                        \
            isolate, NewRangeError(MessageTemplate::kInvalidSimdLaneValue));   \
      }                                                                        \
      lanes[i] = static_cast<lane_type>(a_value);                              \
    }                                                                          \
    Handle<type> result = isolate->factory()->New##type(lanes);                \
    return *result;                                                            \
  }

SIMD_FROM_TYPES(SIMD_FROM_FUNCTION)

#define SIMD_FROM_BITS_TYPES(FUNCTION)       \
  FUNCTION(Float32x4, float, 4, Int32x4)     \
  FUNCTION(Float32x4, float, 4, Uint32x4)    \
  FUNCTION(Float32x4, float, 4, Int16x8)     \
  FUNCTION(Float32x4, float, 4, Uint16x8)    \
  FUNCTION(Float32x4, float, 4, Int8x16)     \
  FUNCTION(Float32x4, float, 4, Uint8x16)    \
  FUNCTION(Int32x4, int32_t, 4, Float32x4)   \
  FUNCTION(Int32x4, int32_t, 4, Uint32x4)    \
  FUNCTION(Int32x4, int32_t, 4, Int16x8)     \
  FUNCTION(Int32x4, int32_t, 4, Uint16x8)    \
  FUNCTION(Int32x4, int32_t, 4, Int8x16)     \
  FUNCTION(Int32x4, int32_t, 4, Uint8x16)    \
  FUNCTION(Uint32x4, uint32_t, 4, Float32x4) \
  FUNCTION(Uint32x4, uint32_t, 4, Int32x4)   \
  FUNCTION(Uint32x4, uint32_t, 4, Int16x8)   \
  FUNCTION(Uint32x4, uint32_t, 4, Uint16x8)  \
  FUNCTION(Uint32x4, uint32_t, 4, Int8x16)   \
  FUNCTION(Uint32x4, uint32_t, 4, Uint8x16)  \
  FUNCTION(Int16x8, int16_t, 8, Float32x4)   \
  FUNCTION(Int16x8, int16_t, 8, Int32x4)     \
  FUNCTION(Int16x8, int16_t, 8, Uint32x4)    \
  FUNCTION(Int16x8, int16_t, 8, Uint16x8)    \
  FUNCTION(Int16x8, int16_t, 8, Int8x16)     \
  FUNCTION(Int16x8, int16_t, 8, Uint8x16)    \
  FUNCTION(Uint16x8, uint16_t, 8, Float32x4) \
  FUNCTION(Uint16x8, uint16_t, 8, Int32x4)   \
  FUNCTION(Uint16x8, uint16_t, 8, Uint32x4)  \
  FUNCTION(Uint16x8, uint16_t, 8, Int16x8)   \
  FUNCTION(Uint16x8, uint16_t, 8, Int8x16)   \
  FUNCTION(Uint16x8, uint16_t, 8, Uint8x16)  \
  FUNCTION(Int8x16, int8_t, 16, Float32x4)   \
  FUNCTION(Int8x16, int8_t, 16, Int32x4)     \
  FUNCTION(Int8x16, int8_t, 16, Uint32x4)    \
  FUNCTION(Int8x16, int8_t, 16, Int16x8)     \
  FUNCTION(Int8x16, int8_t, 16, Uint16x8)    \
  FUNCTION(Int8x16, int8_t, 16, Uint8x16)    \
  FUNCTION(Uint8x16, uint8_t, 16, Float32x4) \
  FUNCTION(Uint8x16, uint8_t, 16, Int32x4)   \
  FUNCTION(Uint8x16, uint8_t, 16, Uint32x4)  \
  FUNCTION(Uint8x16, uint8_t, 16, Int16x8)   \
  FUNCTION(Uint8x16, uint8_t, 16, Uint16x8)  \
  FUNCTION(Uint8x16, uint8_t, 16, Int8x16)

#define SIMD_FROM_BITS_FUNCTION(type, lane_type, lane_count, from_type) \
  RUNTIME_FUNCTION(Runtime_##type##From##from_type##Bits) {             \
    static const int kLaneCount = lane_count;                           \
    HandleScope scope(isolate);                                         \
    DCHECK(args.length() == 1);                                         \
    CONVERT_SIMD_ARG_HANDLE_THROW(from_type, a, 0);                     \
    lane_type lanes[kLaneCount];                                        \
    a->CopyBits(lanes);                                                 \
    Handle<type> result = isolate->factory()->New##type(lanes);         \
    return *result;                                                     \
  }

SIMD_FROM_BITS_TYPES(SIMD_FROM_BITS_FUNCTION)


//-------------------------------------------------------------------

// Load and Store functions.

#define SIMD_LOADN_STOREN_TYPES(FUNCTION) \
  FUNCTION(Float32x4, float, 4)           \
  FUNCTION(Int32x4, int32_t, 4)           \
  FUNCTION(Uint32x4, uint32_t, 4)

#define SIMD_COERCE_INDEX(name, i)                                            \
  Handle<Object> length_object, number_object;                                \
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                                         \
      isolate, length_object, Object::ToLength(isolate, args.at<Object>(i))); \
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, number_object,                  \
                                     Object::ToNumber(args.at<Object>(i)));   \
  if (number_object->Number() != length_object->Number()) {                   \
    THROW_NEW_ERROR_RETURN_FAILURE(                                           \
        isolate, NewTypeError(MessageTemplate::kInvalidSimdIndex));           \
  }                                                                           \
  int32_t name = number_object->Number();

// Common Load and Store Functions

#define SIMD_LOAD(type, lane_type, lane_count, count, result)        \
  static const int kLaneCount = lane_count;                          \
  DCHECK(args.length() == 2);                                        \
  CONVERT_SIMD_ARG_HANDLE_THROW(JSTypedArray, tarray, 0);            \
  SIMD_COERCE_INDEX(index, 1);                                       \
  size_t bpe = tarray->element_size();                               \
  uint32_t bytes = count * sizeof(lane_type);                        \
  size_t byte_length = NumberToSize(tarray->byte_length());          \
  if (index < 0 || index * bpe + bytes > byte_length) {              \
    THROW_NEW_ERROR_RETURN_FAILURE(                                  \
        isolate, NewRangeError(MessageTemplate::kInvalidSimdIndex)); \
  }                                                                  \
  size_t tarray_offset = NumberToSize(tarray->byte_offset());        \
  uint8_t* tarray_base =                                             \
      static_cast<uint8_t*>(tarray->GetBuffer()->backing_store()) +  \
      tarray_offset;                                                 \
  lane_type lanes[kLaneCount] = {0};                                 \
  memcpy(lanes, tarray_base + index * bpe, bytes);                   \
  Handle<type> result = isolate->factory()->New##type(lanes);

#define SIMD_STORE(type, lane_type, lane_count, count, a)            \
  static const int kLaneCount = lane_count;                          \
  DCHECK(args.length() == 3);                                        \
  CONVERT_SIMD_ARG_HANDLE_THROW(JSTypedArray, tarray, 0);            \
  CONVERT_SIMD_ARG_HANDLE_THROW(type, a, 2);                         \
  SIMD_COERCE_INDEX(index, 1);                                       \
  size_t bpe = tarray->element_size();                               \
  uint32_t bytes = count * sizeof(lane_type);                        \
  size_t byte_length = NumberToSize(tarray->byte_length());          \
  if (index < 0 || byte_length < index * bpe + bytes) {              \
    THROW_NEW_ERROR_RETURN_FAILURE(                                  \
        isolate, NewRangeError(MessageTemplate::kInvalidSimdIndex)); \
  }                                                                  \
  size_t tarray_offset = NumberToSize(tarray->byte_offset());        \
  uint8_t* tarray_base =                                             \
      static_cast<uint8_t*>(tarray->GetBuffer()->backing_store()) +  \
      tarray_offset;                                                 \
  lane_type lanes[kLaneCount];                                       \
  for (int i = 0; i < kLaneCount; i++) {                             \
    lanes[i] = a->get_lane(i);                                       \
  }                                                                  \
  memcpy(tarray_base + index * bpe, lanes, bytes);

#define SIMD_LOAD_FUNCTION(type, lane_type, lane_count)         \
  RUNTIME_FUNCTION(Runtime_##type##Load) {                      \
    HandleScope scope(isolate);                                 \
    SIMD_LOAD(type, lane_type, lane_count, lane_count, result); \
    return *result;                                             \
  }


#define SIMD_LOAD1_FUNCTION(type, lane_type, lane_count) \
  RUNTIME_FUNCTION(Runtime_##type##Load1) {              \
    HandleScope scope(isolate);                          \
    SIMD_LOAD(type, lane_type, lane_count, 1, result);   \
    return *result;                                      \
  }


#define SIMD_LOAD2_FUNCTION(type, lane_type, lane_count) \
  RUNTIME_FUNCTION(Runtime_##type##Load2) {              \
    HandleScope scope(isolate);                          \
    SIMD_LOAD(type, lane_type, lane_count, 2, result);   \
    return *result;                                      \
  }


#define SIMD_LOAD3_FUNCTION(type, lane_type, lane_count) \
  RUNTIME_FUNCTION(Runtime_##type##Load3) {              \
    HandleScope scope(isolate);                          \
    SIMD_LOAD(type, lane_type, lane_count, 3, result);   \
    return *result;                                      \
  }


#define SIMD_STORE_FUNCTION(type, lane_type, lane_count)    \
  RUNTIME_FUNCTION(Runtime_##type##Store) {                 \
    HandleScope scope(isolate);                             \
    SIMD_STORE(type, lane_type, lane_count, lane_count, a); \
    return *a;                                              \
  }


#define SIMD_STORE1_FUNCTION(type, lane_type, lane_count) \
  RUNTIME_FUNCTION(Runtime_##type##Store1) {              \
    HandleScope scope(isolate);                           \
    SIMD_STORE(type, lane_type, lane_count, 1, a);        \
    return *a;                                            \
  }


#define SIMD_STORE2_FUNCTION(type, lane_type, lane_count) \
  RUNTIME_FUNCTION(Runtime_##type##Store2) {              \
    HandleScope scope(isolate);                           \
    SIMD_STORE(type, lane_type, lane_count, 2, a);        \
    return *a;                                            \
  }


#define SIMD_STORE3_FUNCTION(type, lane_type, lane_count) \
  RUNTIME_FUNCTION(Runtime_##type##Store3) {              \
    HandleScope scope(isolate);                           \
    SIMD_STORE(type, lane_type, lane_count, 3, a);        \
    return *a;                                            \
  }


SIMD_NUMERIC_TYPES(SIMD_LOAD_FUNCTION)
SIMD_LOADN_STOREN_TYPES(SIMD_LOAD1_FUNCTION)
SIMD_LOADN_STOREN_TYPES(SIMD_LOAD2_FUNCTION)
SIMD_LOADN_STOREN_TYPES(SIMD_LOAD3_FUNCTION)
SIMD_NUMERIC_TYPES(SIMD_STORE_FUNCTION)
SIMD_LOADN_STOREN_TYPES(SIMD_STORE1_FUNCTION)
SIMD_LOADN_STOREN_TYPES(SIMD_STORE2_FUNCTION)
SIMD_LOADN_STOREN_TYPES(SIMD_STORE3_FUNCTION)

//-------------------------------------------------------------------

}  // namespace internal
}  // namespace v8
