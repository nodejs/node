// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/init/v8.h"

#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"

#include "test/cctest/cctest.h"

#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

class TestResolver : public CompilationResultResolver {
 public:
  explicit TestResolver(std::atomic<int>* pending)
      : native_module_(nullptr), pending_(pending) {}

  void OnCompilationSucceeded(i::Handle<i::WasmModuleObject> module) override {
    if (!module.is_null()) {
      native_module_ = module->shared_native_module();
      pending_->fetch_sub(1);
    }
  }

  void OnCompilationFailed(i::Handle<i::Object> error_reason) override {
    CHECK(false);
  }

  std::shared_ptr<NativeModule> native_module() { return native_module_; }

 private:
  std::shared_ptr<NativeModule> native_module_;
  std::atomic<int>* pending_;
};

// Create a valid module such that the bytes depend on {n}.
ZoneBuffer GetValidModuleBytes(Zone* zone, int n) {
  ZoneBuffer buffer(zone);
  TestSignatures sigs;
  WasmModuleBuilder builder(zone);
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.v_v());
    uint8_t code[] = {kExprI32Const, n, kExprDrop, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  builder.WriteTo(&buffer);
  return buffer;
}

}  // namespace

TEST(TestAsyncCache) {
  CcTest::InitializeVM();
  i::HandleScope internal_scope_(CcTest::i_isolate());
  AccountingAllocator allocator;
  Zone zone(&allocator, "CompilationCacheTester");

  auto bufferA = GetValidModuleBytes(&zone, 0);
  auto bufferB = GetValidModuleBytes(&zone, 1);

  std::atomic<int> pending(3);
  auto resolverA1 = std::make_shared<TestResolver>(&pending);
  auto resolverA2 = std::make_shared<TestResolver>(&pending);
  auto resolverB = std::make_shared<TestResolver>(&pending);

  CcTest::i_isolate()->wasm_engine()->AsyncCompile(
      CcTest::i_isolate(), WasmFeatures::All(), resolverA1,
      ModuleWireBytes(bufferA.begin(), bufferA.end()), true,
      "WebAssembly.compile");
  CcTest::i_isolate()->wasm_engine()->AsyncCompile(
      CcTest::i_isolate(), WasmFeatures::All(), resolverA2,
      ModuleWireBytes(bufferA.begin(), bufferA.end()), true,
      "WebAssembly.compile");
  CcTest::i_isolate()->wasm_engine()->AsyncCompile(
      CcTest::i_isolate(), WasmFeatures::All(), resolverB,
      ModuleWireBytes(bufferB.begin(), bufferB.end()), true,
      "WebAssembly.compile");

  while (pending > 0) {
    v8::platform::PumpMessageLoop(i::V8::GetCurrentPlatform(),
                                  CcTest::isolate());
  }

  CHECK_EQ(resolverA1->native_module(), resolverA2->native_module());
  CHECK_NE(resolverA1->native_module(), resolverB->native_module());
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
