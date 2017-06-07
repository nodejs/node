// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/handler-compiler.h"

#include "src/assembler-inl.h"
#include "src/field-type.h"
#include "src/ic/call-optimization.h"
#include "src/ic/handler-configuration-inl.h"
#include "src/ic/ic-inl.h"
#include "src/ic/ic.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {

Handle<Code> PropertyHandlerCompiler::Find(Handle<Name> name,
                                           Handle<Map> stub_holder,
                                           Code::Kind kind) {
  Code::Flags flags = Code::ComputeHandlerFlags(kind);
  Code* code = stub_holder->LookupInCodeCache(*name, flags);
  if (code == nullptr) return Handle<Code>();
  return handle(code);
}

Handle<Code> PropertyHandlerCompiler::GetCode(Code::Kind kind,
                                              Handle<Name> name) {
  Code::Flags flags = Code::ComputeHandlerFlags(kind);

  // Create code object in the heap.
  CodeDesc desc;
  masm()->GetCode(&desc);
  Handle<Code> code = factory()->NewCode(desc, flags, masm()->CodeObject());
  if (code->IsCodeStubOrIC()) code->set_stub_key(CodeStub::NoCacheKey());
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_code_stubs) {
    char* raw_name = !name.is_null() && name->IsString()
                         ? String::cast(*name)->ToCString().get()
                         : nullptr;
    CodeTracer::Scope trace_scope(isolate()->GetCodeTracer());
    OFStream os(trace_scope.file());
    code->Disassemble(raw_name, os);
  }
#endif

  PROFILE(isolate(), CodeCreateEvent(CodeEventListener::HANDLER_TAG,
                                     AbstractCode::cast(*code), *name));

#ifdef DEBUG
  code->VerifyEmbeddedObjects();
#endif
  return code;
}


#define __ ACCESS_MASM(masm())

Register NamedLoadHandlerCompiler::FrontendHeader(Register object_reg,
                                                  Handle<Name> name,
                                                  Label* miss) {
  if (map()->IsPrimitiveMap() || map()->IsJSGlobalProxyMap()) {
    // If the receiver is a global proxy and if we get to this point then
    // the compile-time (current) native context has access to global proxy's
    // native context. Since access rights revocation is not supported at all,
    // we can generate a check that an execution-time native context is either
    // the same as compile-time native context or has the same access token.
    Handle<Context> native_context = isolate()->native_context();
    Handle<WeakCell> weak_cell(native_context->self_weak_cell(), isolate());

    bool compare_native_contexts_only = map()->IsPrimitiveMap();
    GenerateAccessCheck(weak_cell, scratch1(), scratch2(), miss,
                        compare_native_contexts_only);
  }

  // Check that the maps starting from the prototype haven't changed.
  return CheckPrototypes(object_reg, scratch1(), scratch2(), scratch3(), name,
                         miss);
}


// Frontend for store uses the name register. It has to be restored before a
// miss.
Register NamedStoreHandlerCompiler::FrontendHeader(Register object_reg,
                                                   Handle<Name> name,
                                                   Label* miss) {
  if (map()->IsJSGlobalProxyMap()) {
    Handle<Context> native_context = isolate()->native_context();
    Handle<WeakCell> weak_cell(native_context->self_weak_cell(), isolate());
    GenerateAccessCheck(weak_cell, scratch1(), scratch2(), miss, false);
  }

  return CheckPrototypes(object_reg, this->name(), scratch1(), scratch2(), name,
                         miss);
}


Register PropertyHandlerCompiler::Frontend(Handle<Name> name) {
  Label miss;
  if (IC::ShouldPushPopSlotAndVector(kind())) {
    PushVectorAndSlot();
  }
  Register reg = FrontendHeader(receiver(), name, &miss);
  FrontendFooter(name, &miss);
  // The footer consumes the vector and slot from the stack if miss occurs.
  if (IC::ShouldPushPopSlotAndVector(kind())) {
    DiscardVectorAndSlot();
  }
  return reg;
}

Handle<Code> NamedLoadHandlerCompiler::CompileLoadCallback(
    Handle<Name> name, const CallOptimization& call_optimization,
    int accessor_index, Handle<Code> slow_stub) {
  DCHECK(call_optimization.is_simple_api_call());
  if (V8_UNLIKELY(FLAG_runtime_stats)) {
    GenerateTailCall(masm(), slow_stub);
  }
  Register holder = Frontend(name);
  GenerateApiAccessorCall(masm(), call_optimization, map(), receiver(),
                          scratch2(), false, no_reg, holder, accessor_index);
  return GetCode(kind(), name);
}

Handle<Code> NamedStoreHandlerCompiler::CompileStoreViaSetter(
    Handle<JSObject> object, Handle<Name> name, int accessor_index,
    int expected_arguments) {
  Register holder = Frontend(name);
  GenerateStoreViaSetter(masm(), map(), receiver(), holder, accessor_index,
                         expected_arguments, scratch2());

  return GetCode(kind(), name);
}

Handle<Code> NamedStoreHandlerCompiler::CompileStoreCallback(
    Handle<JSObject> object, Handle<Name> name,
    const CallOptimization& call_optimization, int accessor_index,
    Handle<Code> slow_stub) {
  if (V8_UNLIKELY(FLAG_runtime_stats)) {
    GenerateTailCall(masm(), slow_stub);
  }
  Register holder = Frontend(name);
  if (Descriptor::kPassLastArgsOnStack) {
    __ LoadParameterFromStack<Descriptor>(value(), Descriptor::kValue);
  }
  GenerateApiAccessorCall(masm(), call_optimization, handle(object->map()),
                          receiver(), scratch2(), true, value(), holder,
                          accessor_index);
  return GetCode(kind(), name);
}


#undef __

}  // namespace internal
}  // namespace v8
