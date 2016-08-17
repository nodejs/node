// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/access-compiler.h"


namespace v8 {
namespace internal {


Handle<Code> PropertyAccessCompiler::GetCodeWithFlags(Code::Flags flags,
                                                      const char* name) {
  // Create code object in the heap.
  CodeDesc desc;
  masm()->GetCode(&desc);
  Handle<Code> code = factory()->NewCode(desc, flags, masm()->CodeObject());
  if (code->IsCodeStubOrIC()) code->set_stub_key(CodeStub::NoCacheKey());
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_code_stubs) {
    CodeTracer::Scope trace_scope(isolate()->GetCodeTracer());
    OFStream os(trace_scope.file());
    code->Disassemble(name, os);
  }
#endif
  return code;
}


Handle<Code> PropertyAccessCompiler::GetCodeWithFlags(Code::Flags flags,
                                                      Handle<Name> name) {
  return (FLAG_print_code_stubs && !name.is_null() && name->IsString())
             ? GetCodeWithFlags(flags,
                                Handle<String>::cast(name)->ToCString().get())
             : GetCodeWithFlags(flags, NULL);
}


void PropertyAccessCompiler::TailCallBuiltin(MacroAssembler* masm,
                                             Builtins::Name name) {
  Handle<Code> code(masm->isolate()->builtins()->builtin(name));
  GenerateTailCall(masm, code);
}


Register* PropertyAccessCompiler::GetCallingConvention(Code::Kind kind) {
  if (kind == Code::LOAD_IC || kind == Code::KEYED_LOAD_IC) {
    return load_calling_convention();
  }
  DCHECK(kind == Code::STORE_IC || kind == Code::KEYED_STORE_IC);
  return store_calling_convention();
}


Register PropertyAccessCompiler::slot() const {
  if (kind() == Code::LOAD_IC || kind() == Code::KEYED_LOAD_IC) {
    return LoadDescriptor::SlotRegister();
  }
  DCHECK(kind() == Code::STORE_IC || kind() == Code::KEYED_STORE_IC);
  return VectorStoreICDescriptor::SlotRegister();
}


Register PropertyAccessCompiler::vector() const {
  if (kind() == Code::LOAD_IC || kind() == Code::KEYED_LOAD_IC) {
    return LoadWithVectorDescriptor::VectorRegister();
  }
  DCHECK(kind() == Code::STORE_IC || kind() == Code::KEYED_STORE_IC);
  return VectorStoreICDescriptor::VectorRegister();
}
}  // namespace internal
}  // namespace v8
