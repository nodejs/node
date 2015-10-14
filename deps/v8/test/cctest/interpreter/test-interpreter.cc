// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/execution.h"
#include "src/handles.h"
#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/interpreter.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace interpreter {

class InterpreterCallable {
 public:
  InterpreterCallable(Isolate* isolate, Handle<JSFunction> function)
      : isolate_(isolate), function_(function) {}
  virtual ~InterpreterCallable() {}

  MaybeHandle<Object> operator()() {
    return Execution::Call(isolate_, function_,
                           isolate_->factory()->undefined_value(), 0, nullptr,
                           false);
  }

 private:
  Isolate* isolate_;
  Handle<JSFunction> function_;
};

class InterpreterTester {
 public:
  InterpreterTester(Isolate* isolate, Handle<BytecodeArray> bytecode)
      : isolate_(isolate), function_(GetBytecodeFunction(isolate, bytecode)) {
    i::FLAG_ignition = true;
    // Ensure handler table is generated.
    isolate->interpreter()->Initialize();
  }
  virtual ~InterpreterTester() {}

  InterpreterCallable GetCallable() {
    return InterpreterCallable(isolate_, function_);
  }

 private:
  Isolate* isolate_;
  Handle<JSFunction> function_;

  static Handle<JSFunction> GetBytecodeFunction(
      Isolate* isolate, Handle<BytecodeArray> bytecode_array) {
    Handle<JSFunction> function = v8::Utils::OpenHandle(
        *v8::Handle<v8::Function>::Cast(CompileRun("(function(){})")));
    function->ReplaceCode(*isolate->builtins()->InterpreterEntryTrampoline());
    function->shared()->set_function_data(*bytecode_array);
    return function;
  }

  DISALLOW_COPY_AND_ASSIGN(InterpreterTester);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

using v8::internal::BytecodeArray;
using v8::internal::Handle;
using v8::internal::Object;
using v8::internal::Smi;
using namespace v8::internal::interpreter;

TEST(TestInterpreterReturn) {
  InitializedHandleScope handles;
  Handle<Object> undefined_value =
      handles.main_isolate()->factory()->undefined_value();

  BytecodeArrayBuilder builder(handles.main_isolate());
  builder.set_locals_count(0);
  builder.Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  InterpreterCallable callable(tester.GetCallable());
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(undefined_value));
}


TEST(TestInterpreterLoadUndefined) {
  InitializedHandleScope handles;
  Handle<Object> undefined_value =
      handles.main_isolate()->factory()->undefined_value();

  BytecodeArrayBuilder builder(handles.main_isolate());
  builder.set_locals_count(0);
  builder.LoadUndefined().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  InterpreterCallable callable(tester.GetCallable());
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(undefined_value));
}


TEST(TestInterpreterLoadNull) {
  InitializedHandleScope handles;
  Handle<Object> null_value = handles.main_isolate()->factory()->null_value();

  BytecodeArrayBuilder builder(handles.main_isolate());
  builder.set_locals_count(0);
  builder.LoadNull().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  InterpreterCallable callable(tester.GetCallable());
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(null_value));
}


TEST(TestInterpreterLoadTheHole) {
  InitializedHandleScope handles;
  Handle<Object> the_hole_value =
      handles.main_isolate()->factory()->the_hole_value();

  BytecodeArrayBuilder builder(handles.main_isolate());
  builder.set_locals_count(0);
  builder.LoadTheHole().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  InterpreterCallable callable(tester.GetCallable());
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(the_hole_value));
}


TEST(TestInterpreterLoadTrue) {
  InitializedHandleScope handles;
  Handle<Object> true_value = handles.main_isolate()->factory()->true_value();

  BytecodeArrayBuilder builder(handles.main_isolate());
  builder.set_locals_count(0);
  builder.LoadTrue().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  InterpreterCallable callable(tester.GetCallable());
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(true_value));
}


TEST(TestInterpreterLoadFalse) {
  InitializedHandleScope handles;
  Handle<Object> false_value = handles.main_isolate()->factory()->false_value();

  BytecodeArrayBuilder builder(handles.main_isolate());
  builder.set_locals_count(0);
  builder.LoadFalse().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  InterpreterCallable callable(tester.GetCallable());
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(false_value));
}


TEST(TestInterpreterLoadLiteral) {
  InitializedHandleScope handles;
  for (int i = -128; i < 128; i++) {
    BytecodeArrayBuilder builder(handles.main_isolate());
    builder.set_locals_count(0);
    builder.LoadLiteral(Smi::FromInt(i)).Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    InterpreterCallable callable(tester.GetCallable());
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(i));
  }
}


TEST(TestInterpreterLoadStoreRegisters) {
  InitializedHandleScope handles;
  Handle<Object> true_value = handles.main_isolate()->factory()->true_value();
  for (int i = 0; i <= Register::kMaxRegisterIndex; i++) {
    BytecodeArrayBuilder builder(handles.main_isolate());
    builder.set_locals_count(i + 1);
    Register reg(i);
    builder.LoadTrue()
        .StoreAccumulatorInRegister(reg)
        .LoadFalse()
        .LoadAccumulatorWithRegister(reg)
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    InterpreterCallable callable(tester.GetCallable());
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK(return_val.is_identical_to(true_value));
  }
}
