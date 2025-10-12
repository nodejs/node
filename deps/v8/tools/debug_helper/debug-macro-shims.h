// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_DEBUG_MACRO_SHIMS_H_
#define V8_TORQUE_DEBUG_MACRO_SHIMS_H_

// This file contains implementations of a few macros that are defined
// as external in Torque, so that generated debug code can work.

#include "src/numbers/integer-literal.h"
#include "src/objects/smi.h"
#include "tools/debug_helper/debug-helper-internal.h"

// For Object::ReadField<T>.
#define READ_FIELD_OR_FAIL(Type, destination, accessor, object, offset) \
  do {                                                                  \
    Type value{};                                                       \
    d::MemoryAccessResult validity =                                    \
        accessor(object - kHeapObjectTag + offset,                      \
                 reinterpret_cast<Type*>(&value), sizeof(value));       \
    if (validity != d::MemoryAccessResult::kOk) return {validity, {}};  \
    destination = value;                                                \
  } while (false)

// For TaggedField<T>::load.
#define READ_TAGGED_FIELD_OR_FAIL(destination, accessor, object, offset) \
  do {                                                                   \
    Tagged_t value{};                                                    \
    d::MemoryAccessResult validity =                                     \
        accessor(object - kHeapObjectTag + offset,                       \
                 reinterpret_cast<uint8_t*>(&value), sizeof(value));     \
    if (validity != d::MemoryAccessResult::kOk) return {validity, {}};   \
    destination = EnsureDecompressed(value, object);                     \
  } while (false)

// Process Value struct.
#define ASSIGN_OR_RETURN(dest, val)                   \
  do {                                                \
    if ((val).validity != d::MemoryAccessResult::kOk) \
      return {(val).validity, {}};                    \
    dest = (val).value;                               \
  } while (false)

namespace v8 {
namespace internal {
namespace debug_helper_internal {
namespace TorqueDebugMacroShims {
namespace CodeStubAssembler {

inline Value<bool> BoolConstant(d::MemoryAccessor accessor, bool b) {
  return {d::MemoryAccessResult::kOk, b};
}
inline Value<intptr_t> ChangeInt32ToIntPtr(d::MemoryAccessor accessor,
                                           int32_t i) {
  return {d::MemoryAccessResult::kOk, i};
}
inline Value<uintptr_t> ChangeUint32ToWord(d::MemoryAccessor accessor,
                                           uint32_t u) {
  return {d::MemoryAccessResult::kOk, u};
}
inline Value<intptr_t> IntPtrAdd(d::MemoryAccessor accessor, intptr_t a,
                                 intptr_t b) {
  return {d::MemoryAccessResult::kOk, a + b};
}
inline Value<intptr_t> IntPtrMul(d::MemoryAccessor accessor, intptr_t a,
                                 intptr_t b) {
  return {d::MemoryAccessResult::kOk, a * b};
}
inline Value<bool> IntPtrLessThan(d::MemoryAccessor accessor, intptr_t a,
                                  intptr_t b) {
  return {d::MemoryAccessResult::kOk, a < b};
}
inline Value<bool> IntPtrLessThanOrEqual(d::MemoryAccessor accessor, intptr_t a,
                                         intptr_t b) {
  return {d::MemoryAccessResult::kOk, a <= b};
}
inline Value<intptr_t> Signed(d::MemoryAccessor accessor, uintptr_t u) {
  return {d::MemoryAccessResult::kOk, static_cast<intptr_t>(u)};
}
inline Value<int32_t> SmiUntag(d::MemoryAccessor accessor, uintptr_t s_t) {
  Tagged<Smi> s(s_t);
  return {d::MemoryAccessResult::kOk, s.value()};
}
inline Value<uintptr_t> SmiFromInt32(d::MemoryAccessor accessor, int32_t i) {
  return {d::MemoryAccessResult::kOk, Smi::FromInt(i).ptr()};
}
inline Value<bool> UintPtrLessThan(d::MemoryAccessor accessor, uintptr_t a,
                                   uintptr_t b) {
  return {d::MemoryAccessResult::kOk, a < b};
}
inline Value<uint32_t> Unsigned(d::MemoryAccessor accessor, int32_t s) {
  return {d::MemoryAccessResult::kOk, static_cast<uint32_t>(s)};
}
#if V8_HOST_ARCH_64_BIT
inline Value<uintptr_t> Unsigned(d::MemoryAccessor accessor, intptr_t s) {
  return {d::MemoryAccessResult::kOk, static_cast<uintptr_t>(s)};
}
#endif
inline Value<bool> Word32Equal(d::MemoryAccessor accessor, uint32_t a,
                               uint32_t b) {
  return {d::MemoryAccessResult::kOk, a == b};
}
inline Value<bool> Word32NotEqual(d::MemoryAccessor accessor, uint32_t a,
                                  uint32_t b) {
  return {d::MemoryAccessResult::kOk, a != b};
}
// This is used in a nested call where we cannot pass Value<int32_t>.
inline int31_t ConstexprIntegerLiteralToInt31(d::MemoryAccessor accessor,
                                              const IntegerLiteral& i) {
  return i.To<int32_t>();
}
inline int32_t ConstexprIntegerLiteralToInt32(d::MemoryAccessor accessor,
                                              const IntegerLiteral& i) {
  return i.To<int32_t>();
}
inline intptr_t ConstexprIntegerLiteralToIntptr(d::MemoryAccessor accessor,
                                                const IntegerLiteral& i) {
  return i.To<intptr_t>();
}

}  // namespace CodeStubAssembler
}  // namespace TorqueDebugMacroShims
}  // namespace debug_helper_internal
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_DEBUG_MACRO_SHIMS_H_
