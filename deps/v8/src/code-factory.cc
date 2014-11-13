// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/bootstrapper.h"
#include "src/code-factory.h"
#include "src/ic/ic.h"

namespace v8 {
namespace internal {

// static
Callable CodeFactory::LoadIC(Isolate* isolate, ContextualMode mode) {
  return Callable(
      LoadIC::initialize_stub(isolate, LoadICState(mode).GetExtraICState()),
      LoadDescriptor(isolate));
}


// static
Callable CodeFactory::LoadICInOptimizedCode(Isolate* isolate,
                                            ContextualMode mode) {
  if (FLAG_vector_ics) {
    return Callable(LoadIC::initialize_stub_in_optimized_code(
                        isolate, LoadICState(mode).GetExtraICState()),
                    VectorLoadICDescriptor(isolate));
  }
  return CodeFactory::LoadIC(isolate, mode);
}


// static
Callable CodeFactory::KeyedLoadIC(Isolate* isolate) {
  return Callable(KeyedLoadIC::initialize_stub(isolate),
                  LoadDescriptor(isolate));
}


// static
Callable CodeFactory::KeyedLoadICInOptimizedCode(Isolate* isolate) {
  if (FLAG_vector_ics) {
    return Callable(KeyedLoadIC::initialize_stub_in_optimized_code(isolate),
                    VectorLoadICDescriptor(isolate));
  }
  return CodeFactory::KeyedLoadIC(isolate);
}


// static
Callable CodeFactory::StoreIC(Isolate* isolate, StrictMode mode) {
  return Callable(StoreIC::initialize_stub(isolate, mode),
                  StoreDescriptor(isolate));
}


// static
Callable CodeFactory::KeyedStoreIC(Isolate* isolate, StrictMode mode) {
  Handle<Code> ic = mode == SLOPPY
                        ? isolate->builtins()->KeyedStoreIC_Initialize()
                        : isolate->builtins()->KeyedStoreIC_Initialize_Strict();
  return Callable(ic, StoreDescriptor(isolate));
}


// static
Callable CodeFactory::CompareIC(Isolate* isolate, Token::Value op) {
  Handle<Code> code = CompareIC::GetUninitialized(isolate, op);
  return Callable(code, BinaryOpDescriptor(isolate));
}


// static
Callable CodeFactory::BinaryOpIC(Isolate* isolate, Token::Value op,
                                 OverwriteMode mode) {
  BinaryOpICStub stub(isolate, op, mode);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::ToBoolean(Isolate* isolate,
                                ToBooleanStub::ResultMode mode,
                                ToBooleanStub::Types types) {
  ToBooleanStub stub(isolate, mode, types);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::ToNumber(Isolate* isolate) {
  ToNumberStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::StringAdd(Isolate* isolate, StringAddFlags flags,
                                PretenureFlag pretenure_flag) {
  StringAddStub stub(isolate, flags, pretenure_flag);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::AllocateHeapNumber(Isolate* isolate) {
  AllocateHeapNumberStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::CallFunction(Isolate* isolate, int argc,
                                   CallFunctionFlags flags) {
  CallFunctionStub stub(isolate, argc, flags);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}

}  // namespace internal
}  // namespace v8
