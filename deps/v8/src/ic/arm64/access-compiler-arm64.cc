// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM64

#include "src/ic/access-compiler.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


void PropertyAccessCompiler::GenerateTailCall(MacroAssembler* masm,
                                              Handle<Code> code) {
  __ Jump(code, RelocInfo::CODE_TARGET);
}


// TODO(all): The so-called scratch registers are significant in some cases. For
// example, PropertyAccessCompiler::keyed_store_calling_convention()[3] (x3) is
// actually
// used for KeyedStoreCompiler::transition_map(). We should verify which
// registers are actually scratch registers, and which are important. For now,
// we use the same assignments as ARM to remain on the safe side.

Register* PropertyAccessCompiler::load_calling_convention() {
  // receiver, name, scratch1, scratch2, scratch3, scratch4.
  Register receiver = LoadDescriptor::ReceiverRegister();
  Register name = LoadDescriptor::NameRegister();
  static Register registers[] = {receiver, name, x3, x0, x4, x5};
  return registers;
}


Register* PropertyAccessCompiler::store_calling_convention() {
  // receiver, value, scratch1, scratch2, scratch3.
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  DCHECK(x3.is(ElementTransitionAndStoreDescriptor::MapRegister()));
  static Register registers[] = {receiver, name, x3, x4, x5};
  return registers;
}


#undef __
}
}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM64
