// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/flags/flags.h"
#include "src/wasm/code-space-access.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace wasm {

enum MemoryProtectionMode {
  kNoProtection,
  kPku,
  kMprotect,
  kPkuWithMprotectFallback
};

const char* MemoryProtectionModeToString(MemoryProtectionMode mode) {
  switch (mode) {
    case kNoProtection:
      return "NoProtection";
    case kPku:
      return "Pku";
    case kMprotect:
      return "Mprotect";
    case kPkuWithMprotectFallback:
      return "PkuWithMprotectFallback";
  }
}

class MemoryProtectionTest : public TestWithNativeContext {
 public:
  void Initialize(MemoryProtectionMode mode) {
    mode_ = mode;
    bool enable_pku = mode == kPku || mode == kPkuWithMprotectFallback;
    FLAG_wasm_memory_protection_keys = enable_pku;
    if (enable_pku) {
      GetWasmCodeManager()->InitializeMemoryProtectionKeyForTesting();
    }

    bool enable_mprotect =
        mode == kMprotect || mode == kPkuWithMprotectFallback;
    FLAG_wasm_write_protect_code_memory = enable_mprotect;
  }

  void CompileModule() {
    CHECK_NULL(native_module_);
    native_module_ = CompileNativeModule();
    code_ = native_module_->GetCode(0);
  }

  NativeModule* native_module() const { return native_module_.get(); }

  WasmCode* code() const { return code_; }

  bool code_is_protected() {
    return V8_HAS_PTHREAD_JIT_WRITE_PROTECT || uses_pku() || uses_mprotect();
  }

  void MakeCodeWritable() {
    native_module_->MakeWritable(base::AddressRegionOf(code_->instructions()));
  }

  void WriteToCode() { code_->instructions()[0] = 0; }

  void AssertCodeEventuallyProtected() {
    if (!code_is_protected()) {
      // Without protection, writing to code should always work.
      WriteToCode();
      return;
    }
    // Tier-up might be running and unprotecting the code region temporarily (if
    // using mprotect). In that case, repeatedly write to the code region to
    // make us eventually crash.
    ASSERT_DEATH_IF_SUPPORTED(
        do {
          WriteToCode();
          base::OS::Sleep(base::TimeDelta::FromMilliseconds(10));
        } while (uses_mprotect()),
        "");
  }

  bool uses_mprotect() {
    // M1 always uses MAP_JIT.
    if (V8_HAS_PTHREAD_JIT_WRITE_PROTECT) return false;
    return mode_ == kMprotect ||
           (mode_ == kPkuWithMprotectFallback && !uses_pku());
  }

  bool uses_pku() {
    // M1 always uses MAP_JIT.
    if (V8_HAS_PTHREAD_JIT_WRITE_PROTECT) return false;
    bool param_has_pku = mode_ == kPku || mode_ == kPkuWithMprotectFallback;
    return param_has_pku &&
           GetWasmCodeManager()->HasMemoryProtectionKeySupport();
  }

 private:
  std::shared_ptr<NativeModule> CompileNativeModule() {
    // Define the bytes for a module with a single empty function.
    static const byte module_bytes[] = {
        WASM_MODULE_HEADER, SECTION(Type, ENTRY_COUNT(1), SIG_ENTRY_v_v),
        SECTION(Function, ENTRY_COUNT(1), SIG_INDEX(0)),
        SECTION(Code, ENTRY_COUNT(1), ADD_COUNT(0 /* locals */, kExprEnd))};

    ModuleResult result =
        DecodeWasmModule(WasmFeatures::All(), std::begin(module_bytes),
                         std::end(module_bytes), false, kWasmOrigin,
                         isolate()->counters(), isolate()->metrics_recorder(),
                         v8::metrics::Recorder::ContextId::Empty(),
                         DecodingMethod::kSync, GetWasmEngine()->allocator());
    CHECK(result.ok());

    Handle<FixedArray> export_wrappers;
    ErrorThrower thrower(isolate(), "");
    constexpr int kNoCompilationId = 0;
    std::shared_ptr<NativeModule> native_module = CompileToNativeModule(
        isolate(), WasmFeatures::All(), &thrower, std::move(result).value(),
        ModuleWireBytes{base::ArrayVector(module_bytes)}, &export_wrappers,
        kNoCompilationId);
    CHECK(!thrower.error());
    CHECK_NOT_NULL(native_module);

    return native_module;
  }

  MemoryProtectionMode mode_;
  std::shared_ptr<NativeModule> native_module_;
  WasmCodeRefScope code_refs_;
  WasmCode* code_;
};

class ParameterizedMemoryProtectionTest
    : public MemoryProtectionTest,
      public ::testing::WithParamInterface<MemoryProtectionMode> {
 public:
  void SetUp() override { Initialize(GetParam()); }
};

std::string PrintMemoryProtectionTestParam(
    ::testing::TestParamInfo<MemoryProtectionMode> info) {
  return MemoryProtectionModeToString(info.param);
}

INSTANTIATE_TEST_SUITE_P(MemoryProtection, ParameterizedMemoryProtectionTest,
                         ::testing::Values(kNoProtection, kPku, kMprotect,
                                           kPkuWithMprotectFallback),
                         PrintMemoryProtectionTestParam);

TEST_P(ParameterizedMemoryProtectionTest, CodeNotWritableAfterCompilation) {
  CompileModule();
  AssertCodeEventuallyProtected();
}

TEST_P(ParameterizedMemoryProtectionTest, CodeWritableWithinScope) {
  CompileModule();
  CodeSpaceWriteScope write_scope(native_module());
  MakeCodeWritable();
  WriteToCode();
}

TEST_P(ParameterizedMemoryProtectionTest, CodeNotWritableAfterScope) {
  CompileModule();
  {
    CodeSpaceWriteScope write_scope(native_module());
    MakeCodeWritable();
    WriteToCode();
  }
  AssertCodeEventuallyProtected();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
