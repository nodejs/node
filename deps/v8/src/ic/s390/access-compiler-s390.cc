// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_S390

#include "src/ic/access-compiler.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void PropertyAccessCompiler::GenerateTailCall(MacroAssembler* masm,
                                              Handle<Code> code) {
  __ Jump(code, RelocInfo::CODE_TARGET);
}

Register* PropertyAccessCompiler::load_calling_convention() {
  // receiver, name, scratch1, scratch2, scratch3.
  Register receiver = LoadDescriptor::ReceiverRegister();
  Register name = LoadDescriptor::NameRegister();
  static Register registers[] = {receiver, name, r5, r2, r6};
  return registers;
}

Register* PropertyAccessCompiler::store_calling_convention() {
  // receiver, name, scratch1, scratch2.
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  static Register registers[] = {receiver, name, r5, r6};
  return registers;
}

#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
