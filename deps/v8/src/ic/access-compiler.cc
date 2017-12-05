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
                                                       Type type) {
  AccessCompilerData* data = isolate->access_compiler_data();
  if (!data->IsInitialized()) {
    InitializePlatformSpecific(data);
  }
  switch (type) {
    case LOAD:
      return data->load_calling_convention();
    case STORE:
      return data->store_calling_convention();
  }
  UNREACHABLE();
  return data->store_calling_convention();
}


Register PropertyAccessCompiler::slot() const {
  switch (type_) {
    case LOAD:
      return LoadDescriptor::SlotRegister();
    case STORE:
      return StoreWithVectorDescriptor::SlotRegister();
  }
  UNREACHABLE();
  return StoreWithVectorDescriptor::SlotRegister();
}

Register PropertyAccessCompiler::vector() const {
  switch (type_) {
    case LOAD:
      return LoadWithVectorDescriptor::VectorRegister();
    case STORE:
      return StoreWithVectorDescriptor::VectorRegister();
  }
  UNREACHABLE();
  return StoreWithVectorDescriptor::VectorRegister();
}
}  // namespace internal
}  // namespace v8
