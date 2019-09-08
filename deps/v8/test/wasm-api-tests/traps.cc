// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/wasm-api-tests/wasm-api-test.h"

#include "src/execution/isolate.h"
#include "src/wasm/c-api.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-engine.h"

#include <iostream>

namespace v8 {
namespace internal {
namespace wasm {

using ::wasm::Frame;
using ::wasm::Message;

namespace {

own<Trap> FailCallback(void* env, const Val args[], Val results[]) {
  Store* store = reinterpret_cast<Store*>(env);
  Message message = Message::make(std::string("callback abort"));
  return Trap::make(store, message);
}

void ExpectMessage(const char* expected, const Message& message) {
  size_t len = strlen(expected);
  EXPECT_EQ(len, message.size());
  EXPECT_EQ(0, strncmp(expected, message.get(), len));
}

}  // namespace

TEST_F(WasmCapiTest, Traps) {
  ValueType i32_type[] = {kWasmI32};
  FunctionSig sig(1, 0, i32_type);
  uint32_t callback_index = builder()->AddImport(CStrVector("callback"), &sig);
  byte code[] = {WASM_CALL_FUNCTION0(callback_index)};
  AddExportedFunction(CStrVector("callback"), code, sizeof(code), &sig);
  // The first constant is a 4-byte dummy so that the {unreachable} trap
  // has a more interesting offset.
  byte code2[] = {WASM_I32V_3(0), WASM_UNREACHABLE, WASM_I32V_1(1)};
  AddExportedFunction(CStrVector("unreachable"), code2, sizeof(code2), &sig);

  own<FuncType> func_type =
      FuncType::make(ownvec<ValType>::make(),
                     ownvec<ValType>::make(ValType::make(::wasm::I32)));
  own<Func> cpp_callback = Func::make(store(), func_type.get(), FailCallback,
                                      reinterpret_cast<void*>(store()));
  Extern* imports[] = {cpp_callback.get()};
  Instantiate(imports);

  // Use internal machinery to parse the module to find the function offsets.
  // This makes the test more robust than hardcoding them.
  i::Isolate* isolate =
      reinterpret_cast<::wasm::StoreImpl*>(store())->i_isolate();
  ModuleResult result = DecodeWasmModule(
      kAllWasmFeatures, wire_bytes()->begin(), wire_bytes()->end(), false,
      ModuleOrigin::kWasmOrigin, isolate->counters(),
      isolate->wasm_engine()->allocator());
  ASSERT_TRUE(result.ok());
  const WasmFunction* func1 = &result.value()->functions[1];
  const WasmFunction* func2 = &result.value()->functions[2];
  const uint32_t func1_offset = func1->code.offset();
  const uint32_t func2_offset = func2->code.offset();

  Func* cpp_trapping_func = GetExportedFunction(0);
  own<Trap> cpp_trap = cpp_trapping_func->call();
  EXPECT_NE(nullptr, cpp_trap.get());
  ExpectMessage("Uncaught Error: callback abort", cpp_trap->message());
  own<Frame> frame = cpp_trap->origin();
  EXPECT_TRUE(frame->instance()->same(instance()));
  EXPECT_EQ(1u, frame->func_index());
  EXPECT_EQ(1u, frame->func_offset());
  EXPECT_EQ(func1_offset + frame->func_offset(), frame->module_offset());
  ownvec<Frame> trace = cpp_trap->trace();
  EXPECT_EQ(1u, trace.size());
  frame.reset(trace[0].release());
  EXPECT_TRUE(frame->instance()->same(instance()));
  EXPECT_EQ(1u, frame->func_index());
  EXPECT_EQ(1u, frame->func_offset());
  EXPECT_EQ(func1_offset + frame->func_offset(), frame->module_offset());

  Func* wasm_trapping_func = GetExportedFunction(1);
  own<Trap> wasm_trap = wasm_trapping_func->call();
  EXPECT_NE(nullptr, wasm_trap.get());
  ExpectMessage("Uncaught RuntimeError: unreachable", wasm_trap->message());
  frame = wasm_trap->origin();
  EXPECT_TRUE(frame->instance()->same(instance()));
  EXPECT_EQ(2u, frame->func_index());
  EXPECT_EQ(5u, frame->func_offset());
  EXPECT_EQ(func2_offset + frame->func_offset(), frame->module_offset());
  trace = wasm_trap->trace();
  EXPECT_EQ(1u, trace.size());
  frame.reset(trace[0].release());
  EXPECT_TRUE(frame->instance()->same(instance()));
  EXPECT_EQ(2u, frame->func_index());
  EXPECT_EQ(5u, frame->func_offset());
  EXPECT_EQ(func2_offset + frame->func_offset(), frame->module_offset());
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
