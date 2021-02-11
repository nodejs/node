// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains runtime implementations of a few macros that are defined
// as external in Torque, so that generated runtime code can work.

#ifndef V8_TORQUE_RUNTIME_MACRO_SHIMS_H_
#define V8_TORQUE_RUNTIME_MACRO_SHIMS_H_

#include "src/objects/smi.h"

namespace v8 {
namespace internal {
namespace TorqueRuntimeMacroShims {
namespace CodeStubAssembler {

inline intptr_t ChangeInt32ToIntPtr(Isolate* isolate, int32_t i) { return i; }
inline uintptr_t ChangeUint32ToWord(Isolate* isolate, uint32_t u) { return u; }
inline intptr_t IntPtrAdd(Isolate* isolate, intptr_t a, intptr_t b) {
  return a + b;
}
inline intptr_t IntPtrMul(Isolate* isolate, intptr_t a, intptr_t b) {
  return a * b;
}
inline intptr_t Signed(Isolate* isolate, uintptr_t u) {
  return static_cast<intptr_t>(u);
}
inline int32_t SmiUntag(Isolate* isolate, Smi s) { return s.value(); }

}  // namespace CodeStubAssembler
}  // namespace TorqueRuntimeMacroShims
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_RUNTIME_MACRO_SHIMS_H_
