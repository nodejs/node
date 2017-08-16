// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/access-compiler.h"
#include "src/assembler-inl.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

void PropertyAccessCompiler::TailCallBuiltin(MacroAssembler* masm,
                                             Builtins::Name name) {
  Handle<Code> code(masm->isolate()->builtins()->builtin(name));
  GenerateTailCall(masm, code);
}

Register* PropertyAccessCompiler::GetCallingConvention(Isolate* isolate,
                                                       Code::Kind kind) {
  AccessCompilerData* data = isolate->access_compiler_data();
  if (!data->IsInitialized()) {
    InitializePlatformSpecific(data);
  }
  if (kind == Code::LOAD_IC || kind == Code::KEYED_LOAD_IC) {
    return data->load_calling_convention();
  }
  DCHECK(kind == Code::STORE_IC || kind == Code::KEYED_STORE_IC);
  return data->store_calling_convention();
}


Register PropertyAccessCompiler::slot() const {
  if (kind() == Code::LOAD_IC || kind() == Code::KEYED_LOAD_IC) {
    return LoadDescriptor::SlotRegister();
  }
  DCHECK(kind() == Code::STORE_IC || kind() == Code::KEYED_STORE_IC);
  return StoreWithVectorDescriptor::SlotRegister();
}


Register PropertyAccessCompiler::vector() const {
  if (kind() == Code::LOAD_IC || kind() == Code::KEYED_LOAD_IC) {
    return LoadWithVectorDescriptor::VectorRegister();
  }
  DCHECK(kind() == Code::STORE_IC || kind() == Code::KEYED_STORE_IC);
  return StoreWithVectorDescriptor::VectorRegister();
}
}  // namespace internal
}  // namespace v8
