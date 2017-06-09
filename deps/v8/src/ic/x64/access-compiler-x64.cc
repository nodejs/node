// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/ic/access-compiler.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void PropertyAccessCompiler::GenerateTailCall(MacroAssembler* masm,
                                              Handle<Code> code) {
  __ jmp(code, RelocInfo::CODE_TARGET);
}

void PropertyAccessCompiler::InitializePlatformSpecific(
    AccessCompilerData* data) {
  Register receiver = LoadDescriptor::ReceiverRegister();
  Register name = LoadDescriptor::NameRegister();

  // Load calling convention.
  // receiver, name, scratch1, scratch2, scratch3.
  Register load_registers[] = {receiver, name, rax, rbx, rdi};

  // Store calling convention.
  // receiver, name, scratch1, scratch2.
  Register store_registers[] = {receiver, name, rbx, rdi};

  data->Initialize(arraysize(load_registers), load_registers,
                   arraysize(store_registers), store_registers);
}

#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
