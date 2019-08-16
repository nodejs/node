// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/wasm-api-tests/wasm-api-test.h"

#include <iostream>

namespace v8 {
namespace internal {
namespace wasm {

using ::wasm::Message;

namespace {

own<Trap*> FailCallback(void* env, const Val args[], Val results[]) {
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
  byte code2[] = {WASM_UNREACHABLE, WASM_I32V_1(1)};
  AddExportedFunction(CStrVector("unreachable"), code2, sizeof(code2), &sig);

  own<FuncType*> func_type = FuncType::make(
      vec<ValType*>::make(), vec<ValType*>::make(ValType::make(::wasm::I32)));
  own<Func*> cpp_callback = Func::make(store(), func_type.get(), FailCallback,
                                       reinterpret_cast<void*>(store()));
  Extern* imports[] = {cpp_callback.get()};
  Instantiate(imports);

  Func* cpp_trapping_func = GetExportedFunction(0);
  own<Trap*> cpp_trap = cpp_trapping_func->call();
  EXPECT_NE(nullptr, cpp_trap.get());
  ExpectMessage("Uncaught Error: callback abort", cpp_trap->message());

  Func* wasm_trapping_func = GetExportedFunction(1);
  own<Trap*> wasm_trap = wasm_trapping_func->call();
  EXPECT_NE(nullptr, wasm_trap.get());
  ExpectMessage("Uncaught RuntimeError: unreachable", wasm_trap->message());
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
