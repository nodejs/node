// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/ic/ic.h"
#include "src/ic/ic-compiler.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


void PropertyICCompiler::GenerateRuntimeSetProperty(
    MacroAssembler* masm, LanguageMode language_mode) {
  // Return address is on the stack.
  DCHECK(!rbx.is(StoreDescriptor::ReceiverRegister()) &&
         !rbx.is(StoreDescriptor::NameRegister()) &&
         !rbx.is(StoreDescriptor::ValueRegister()));

  __ PopReturnAddressTo(rbx);
  __ Push(StoreDescriptor::ReceiverRegister());
  __ Push(StoreDescriptor::NameRegister());
  __ Push(StoreDescriptor::ValueRegister());
  __ Push(Smi::FromInt(language_mode));
  __ PushReturnAddressFrom(rbx);

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty);
}


#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
