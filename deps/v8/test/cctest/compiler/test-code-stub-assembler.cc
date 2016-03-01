// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interface-descriptors.h"
#include "src/isolate.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {


class CodeStubAssemblerTester : public CodeStubAssembler {
 public:
  CodeStubAssemblerTester(Isolate* isolate,
                          const CallInterfaceDescriptor& descriptor)
      : CodeStubAssembler(isolate, isolate->runtime_zone(), descriptor,
                          Code::STUB, "test"),
        scope_(isolate) {}

 private:
  HandleScope scope_;
  LocalContext context_;
};


TEST(SimpleSmiReturn) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  m.Return(m.SmiTag(m.Int32Constant(37)));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(37, Handle<Smi>::cast(result.ToHandleChecked())->value());
}


TEST(SimpleIntPtrReturn) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  int test;
  m.Return(m.IntPtrConstant(reinterpret_cast<intptr_t>(&test)));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(reinterpret_cast<intptr_t>(&test),
           reinterpret_cast<intptr_t>(*result.ToHandleChecked()));
}


TEST(SimpleDoubleReturn) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  m.Return(m.NumberConstant(0.5));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(0.5, Handle<HeapNumber>::cast(result.ToHandleChecked())->value());
}


TEST(SimpleCallRuntime1Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* b = m.SmiTag(m.Int32Constant(256));
  m.Return(m.CallRuntime(Runtime::kMathSqrt, context, b));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(16, Handle<Smi>::cast(result.ToHandleChecked())->value());
}


TEST(SimpleTailCallRuntime1Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* b = m.SmiTag(m.Int32Constant(256));
  m.TailCallRuntime(Runtime::kMathSqrt, context, b);
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(16, Handle<Smi>::cast(result.ToHandleChecked())->value());
}


TEST(SimpleCallRuntime2Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* a = m.SmiTag(m.Int32Constant(2));
  Node* b = m.SmiTag(m.Int32Constant(4));
  m.Return(m.CallRuntime(Runtime::kMathPow, context, a, b));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(16, Handle<Smi>::cast(result.ToHandleChecked())->value());
}


TEST(SimpleTailCallRuntime2Arg) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* a = m.SmiTag(m.Int32Constant(2));
  Node* b = m.SmiTag(m.Int32Constant(4));
  m.TailCallRuntime(Runtime::kMathPow, context, a, b);
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(16, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
