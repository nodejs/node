// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/ic/ic.h"
#include "src/ic/ic-compiler.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


void PropertyICCompiler::GenerateRuntimeSetProperty(
    MacroAssembler* masm, LanguageMode language_mode) {
  // Return address is on the stack.
  DCHECK(!ebx.is(StoreDescriptor::ReceiverRegister()) &&
         !ebx.is(StoreDescriptor::NameRegister()) &&
         !ebx.is(StoreDescriptor::ValueRegister()));
  __ pop(ebx);
  __ push(StoreDescriptor::ReceiverRegister());
  __ push(StoreDescriptor::NameRegister());
  __ push(StoreDescriptor::ValueRegister());
  __ push(Immediate(Smi::FromInt(language_mode)));
  __ push(ebx);  // return address

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty);
}


#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32
