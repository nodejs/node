// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-compiler.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-import-wrapper-cache-inl.h"
#include "src/wasm/wasm-module.h"

#include "test/cctest/cctest.h"
#include "test/common/wasm/test-signatures.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_wasm_import_wrapper_cache {

std::unique_ptr<NativeModule> NewModule(Isolate* isolate) {
  std::shared_ptr<WasmModule> module(new WasmModule);
  bool can_request_more = false;
  size_t size = 16384;
  auto native_module = isolate->wasm_engine()->NewNativeModule(
      isolate, kAllWasmFeatures, size, can_request_more, std::move(module));
  native_module->SetRuntimeStubs(isolate);
  return native_module;
}

TEST(CacheHit) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  auto module = NewModule(isolate);
  TestSignatures sigs;

  auto kind = compiler::WasmImportCallKind::kJSFunctionArityMatch;

  WasmCode* c1 = module->import_wrapper_cache()->GetOrCompile(
      isolate->wasm_engine(), isolate->counters(), kind, sigs.i_i());

  CHECK_NOT_NULL(c1);
  CHECK_EQ(WasmCode::Kind::kWasmToJsWrapper, c1->kind());

  WasmCode* c2 = module->import_wrapper_cache()->GetOrCompile(
      isolate->wasm_engine(), isolate->counters(), kind, sigs.i_i());

  CHECK_NOT_NULL(c2);
  CHECK_EQ(c1, c2);
}

TEST(CacheMissSig) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  auto module = NewModule(isolate);
  TestSignatures sigs;

  auto kind = compiler::WasmImportCallKind::kJSFunctionArityMatch;

  WasmCode* c1 = module->import_wrapper_cache()->GetOrCompile(
      isolate->wasm_engine(), isolate->counters(), kind, sigs.i_i());

  CHECK_NOT_NULL(c1);
  CHECK_EQ(WasmCode::Kind::kWasmToJsWrapper, c1->kind());

  WasmCode* c2 = module->import_wrapper_cache()->GetOrCompile(
      isolate->wasm_engine(), isolate->counters(), kind, sigs.i_ii());

  CHECK_NOT_NULL(c2);
  CHECK_NE(c1, c2);
}

TEST(CacheMissKind) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  auto module = NewModule(isolate);
  TestSignatures sigs;

  auto kind1 = compiler::WasmImportCallKind::kJSFunctionArityMatch;
  auto kind2 = compiler::WasmImportCallKind::kJSFunctionArityMismatch;

  WasmCode* c1 = module->import_wrapper_cache()->GetOrCompile(
      isolate->wasm_engine(), isolate->counters(), kind1, sigs.i_i());

  CHECK_NOT_NULL(c1);
  CHECK_EQ(WasmCode::Kind::kWasmToJsWrapper, c1->kind());

  WasmCode* c2 = module->import_wrapper_cache()->GetOrCompile(
      isolate->wasm_engine(), isolate->counters(), kind2, sigs.i_i());

  CHECK_NOT_NULL(c2);
  CHECK_NE(c1, c2);
}

TEST(CacheHitMissSig) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  auto module = NewModule(isolate);
  TestSignatures sigs;

  auto kind = compiler::WasmImportCallKind::kJSFunctionArityMatch;

  WasmCode* c1 = module->import_wrapper_cache()->GetOrCompile(
      isolate->wasm_engine(), isolate->counters(), kind, sigs.i_i());

  CHECK_NOT_NULL(c1);
  CHECK_EQ(WasmCode::Kind::kWasmToJsWrapper, c1->kind());

  WasmCode* c2 = module->import_wrapper_cache()->GetOrCompile(
      isolate->wasm_engine(), isolate->counters(), kind, sigs.i_ii());

  CHECK_NOT_NULL(c2);
  CHECK_NE(c1, c2);

  WasmCode* c3 = module->import_wrapper_cache()->GetOrCompile(
      isolate->wasm_engine(), isolate->counters(), kind, sigs.i_i());

  CHECK_NOT_NULL(c3);
  CHECK_EQ(c1, c3);

  WasmCode* c4 = module->import_wrapper_cache()->GetOrCompile(
      isolate->wasm_engine(), isolate->counters(), kind, sigs.i_ii());

  CHECK_NOT_NULL(c4);
  CHECK_EQ(c2, c4);
}

}  // namespace test_wasm_import_wrapper_cache
}  // namespace wasm
}  // namespace internal
}  // namespace v8
