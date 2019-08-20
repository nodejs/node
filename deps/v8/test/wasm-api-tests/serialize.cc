// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/wasm-api-tests/wasm-api-test.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

bool g_callback_called;

own<Trap*> Callback(const Val args[], Val results[]) {
  g_callback_called = true;
  return nullptr;
}

}  // namespace

TEST_F(WasmCapiTest, Serialize) {
  FunctionSig sig(0, 0, nullptr);
  uint32_t callback_index = builder()->AddImport(CStrVector("callback"), &sig);
  byte code[] = {WASM_CALL_FUNCTION0(callback_index)};
  AddExportedFunction(CStrVector("run"), code, sizeof(code), &sig);
  Compile();

  vec<byte_t> serialized = module()->serialize();
  own<Module*> deserialized = Module::deserialize(store(), serialized);

  own<FuncType*> callback_type =
      FuncType::make(vec<ValType*>::make(), vec<ValType*>::make());
  own<Func*> callback = Func::make(store(), callback_type.get(), Callback);
  Extern* imports[] = {callback.get()};

  own<Instance*> instance =
      Instance::make(store(), deserialized.get(), imports);
  vec<Extern*> exports = instance->exports();
  Func* run = exports[0]->func();
  g_callback_called = false;
  run->call();
  EXPECT_TRUE(g_callback_called);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
