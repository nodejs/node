// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-compiler.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "test/cctest/cctest.h"
#include "test/common/flag-utils.h"
#include "test/common/wasm/test-signatures.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_wasm_import_wrapper_cache {

std::shared_ptr<NativeModule> NewModule(Isolate* isolate) {
  auto module = std::make_shared<WasmModule>(kWasmOrigin);
  constexpr size_t kCodeSizeEstimate = 16384;
  auto native_module = GetWasmEngine()->NewNativeModule(
      isolate, WasmEnabledFeatures::All(), CompileTimeImports{},
      std::move(module), kCodeSizeEstimate);
  native_module->SetWireBytes({});
  return native_module;
}

TEST(CacheHit) {
  FlagScope<bool> cleanup_immediately(&v8_flags.stress_wasm_code_gc, true);
  Isolate* isolate = CcTest::InitIsolateOnce();
  auto module = NewModule(isolate);
  TestSignatures sigs;

  auto kind = ImportCallKind::kJSFunctionArityMatch;
  auto sig = sigs.i_i();
  uint32_t canonical_type_index =
      GetTypeCanonicalizer()->AddRecursiveGroup(sig);
  int expected_arity = static_cast<int>(sig->parameter_count());
  {
    WasmCodeRefScope wasm_code_ref_scope;
    WasmCode* c1 = CompileImportWrapperForTest(
        module.get(), isolate->counters(), kind, sig, canonical_type_index,
        expected_arity, kNoSuspend);

    CHECK_NOT_NULL(c1);
    CHECK_EQ(WasmCode::Kind::kWasmToJsWrapper, c1->kind());

    WasmCode* c2 = GetWasmImportWrapperCache()->MaybeGet(
        kind, canonical_type_index, expected_arity, kNoSuspend);

    CHECK_NOT_NULL(c2);
    CHECK_EQ(c1, c2);
  }
  // Ending the lifetime of the {WasmCodeRefScope} should drop the refcount
  // of the wrapper to zero, causing its cleanup at the next Wasm Code GC
  // (requested via interrupt).
  isolate->stack_guard()->HandleInterrupts();
  CHECK_NULL(GetWasmImportWrapperCache()->MaybeGet(kind, canonical_type_index,
                                                   expected_arity, kNoSuspend));
}

TEST(CacheMissSig) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  auto module = NewModule(isolate);
  TestSignatures sigs;
  WasmCodeRefScope wasm_code_ref_scope;

  auto kind = ImportCallKind::kJSFunctionArityMatch;
  auto sig1 = sigs.i_i();
  int expected_arity1 = static_cast<int>(sig1->parameter_count());
  uint32_t canonical_type_index1 =
      GetTypeCanonicalizer()->AddRecursiveGroup(sig1);
  auto sig2 = sigs.i_ii();
  int expected_arity2 = static_cast<int>(sig2->parameter_count());
  uint32_t canonical_type_index2 =
      GetTypeCanonicalizer()->AddRecursiveGroup(sig2);

  WasmCode* c1 = CompileImportWrapperForTest(module.get(), isolate->counters(),
                                             kind, sig1, canonical_type_index1,
                                             expected_arity1, kNoSuspend);

  CHECK_NOT_NULL(c1);
  CHECK_EQ(WasmCode::Kind::kWasmToJsWrapper, c1->kind());

  WasmCode* c2 = GetWasmImportWrapperCache()->MaybeGet(
      kind, canonical_type_index2, expected_arity2, kNoSuspend);

  CHECK_NULL(c2);
}

TEST(CacheMissKind) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  auto module = NewModule(isolate);
  TestSignatures sigs;
  WasmCodeRefScope wasm_code_ref_scope;

  auto kind1 = ImportCallKind::kJSFunctionArityMatch;
  auto kind2 = ImportCallKind::kJSFunctionArityMismatch;
  auto sig = sigs.i_i();
  int expected_arity = static_cast<int>(sig->parameter_count());
  uint32_t canonical_type_index =
      GetTypeCanonicalizer()->AddRecursiveGroup(sig);

  WasmCode* c1 = CompileImportWrapperForTest(module.get(), isolate->counters(),
                                             kind1, sig, canonical_type_index,
                                             expected_arity, kNoSuspend);

  CHECK_NOT_NULL(c1);
  CHECK_EQ(WasmCode::Kind::kWasmToJsWrapper, c1->kind());

  WasmCode* c2 = GetWasmImportWrapperCache()->MaybeGet(
      kind2, canonical_type_index, expected_arity, kNoSuspend);

  CHECK_NULL(c2);
}

TEST(CacheHitMissSig) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  auto module = NewModule(isolate);
  TestSignatures sigs;
  WasmCodeRefScope wasm_code_ref_scope;

  auto kind = ImportCallKind::kJSFunctionArityMatch;
  auto sig1 = sigs.i_i();
  int expected_arity1 = static_cast<int>(sig1->parameter_count());
  uint32_t canonical_type_index1 =
      GetTypeCanonicalizer()->AddRecursiveGroup(sig1);
  auto sig2 = sigs.i_ii();
  int expected_arity2 = static_cast<int>(sig2->parameter_count());
  uint32_t canonical_type_index2 =
      GetTypeCanonicalizer()->AddRecursiveGroup(sig2);

  WasmCode* c1 = CompileImportWrapperForTest(module.get(), isolate->counters(),
                                             kind, sig1, canonical_type_index1,
                                             expected_arity1, kNoSuspend);

  CHECK_NOT_NULL(c1);
  CHECK_EQ(WasmCode::Kind::kWasmToJsWrapper, c1->kind());

  WasmCode* c2 = GetWasmImportWrapperCache()->MaybeGet(
      kind, canonical_type_index2, expected_arity2, kNoSuspend);

  CHECK_NULL(c2);

  c2 = CompileImportWrapperForTest(module.get(), isolate->counters(), kind,
                                   sig2, canonical_type_index2, expected_arity2,
                                   kNoSuspend);

  CHECK_NE(c1, c2);

  WasmCode* c3 = GetWasmImportWrapperCache()->MaybeGet(
      kind, canonical_type_index1, expected_arity1, kNoSuspend);

  CHECK_NOT_NULL(c3);
  CHECK_EQ(c1, c3);

  WasmCode* c4 = GetWasmImportWrapperCache()->MaybeGet(
      kind, canonical_type_index2, expected_arity2, kNoSuspend);

  CHECK_NOT_NULL(c4);
  CHECK_EQ(c2, c4);
}

}  // namespace test_wasm_import_wrapper_cache
}  // namespace wasm
}  // namespace internal
}  // namespace v8
