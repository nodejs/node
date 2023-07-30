// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_FUNCTION_COMPILER_H_
#define V8_WASM_FUNCTION_COMPILER_H_

#include <memory>

#include "src/codegen/assembler.h"
#include "src/codegen/code-desc.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-tier.h"

namespace v8 {
namespace internal {

class Counters;
class TurbofanCompilationJob;

namespace wasm {

class AssemblerBufferCache;
class NativeModule;
class WasmCode;
class WasmEngine;
struct WasmFunction;

// Stores assumptions that a Wasm compilation job made while executing,
// so they can be checked for continued validity when the job finishes.
class AssumptionsJournal {
 public:
  AssumptionsJournal() = default;

  void RecordAssumption(uint32_t func_index, WellKnownImport status) {
    imports_.push_back(std::make_pair(func_index, status));
  }

  const std::vector<std::pair<uint32_t, WellKnownImport>>& import_statuses() {
    return imports_;
  }

  bool empty() const { return imports_.empty(); }

 private:
  // This is not particularly efficient, but it's probably good enough.
  // For most compilations, this won't hold any entries. If it does
  // hold entries, their number is expected to be small, because most
  // functions don't call many imports, and many imports won't be
  // specially recognized.
  std::vector<std::pair<uint32_t, WellKnownImport>> imports_;
};

struct WasmCompilationResult {
 public:
  MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(WasmCompilationResult);

  enum Kind : int8_t {
    kFunction,
    kWasmToJsWrapper,
  };

  bool succeeded() const { return code_desc.buffer != nullptr; }
  bool failed() const { return !succeeded(); }
  explicit operator bool() const { return succeeded(); }

  CodeDesc code_desc;
  std::unique_ptr<AssemblerBuffer> instr_buffer;
  uint32_t frame_slot_count = 0;
  uint32_t tagged_parameter_slots = 0;
  base::OwnedVector<byte> source_positions;
  base::OwnedVector<byte> inlining_positions;
  base::OwnedVector<byte> protected_instructions_data;
  std::unique_ptr<AssumptionsJournal> assumptions;
  int func_index = kAnonymousFuncIndex;
  ExecutionTier requested_tier;
  ExecutionTier result_tier;
  Kind kind = kFunction;
  ForDebugging for_debugging = kNotForDebugging;
  bool frame_has_feedback_slot = false;
};

class V8_EXPORT_PRIVATE WasmCompilationUnit final {
 public:
  WasmCompilationUnit(int index, ExecutionTier tier, ForDebugging for_debugging)
      : func_index_(index), tier_(tier), for_debugging_(for_debugging) {
    DCHECK_IMPLIES(for_debugging != ForDebugging::kNotForDebugging,
                   tier_ == ExecutionTier::kLiftoff);
  }

  WasmCompilationResult ExecuteCompilation(CompilationEnv*,
                                           const WireBytesStorage*, Counters*,
                                           AssemblerBufferCache*,
                                           WasmFeatures* detected);

  ExecutionTier tier() const { return tier_; }
  ForDebugging for_debugging() const { return for_debugging_; }
  int func_index() const { return func_index_; }

  static void CompileWasmFunction(Counters*, NativeModule*,
                                  WasmFeatures* detected, const WasmFunction*,
                                  ExecutionTier);

 private:
  WasmCompilationResult ExecuteFunctionCompilation(CompilationEnv*,
                                                   const WireBytesStorage*,
                                                   Counters*,
                                                   AssemblerBufferCache*,
                                                   WasmFeatures* detected);

  WasmCompilationResult ExecuteImportWrapperCompilation(CompilationEnv*);

  int func_index_;
  ExecutionTier tier_;
  ForDebugging for_debugging_;
};

// {WasmCompilationUnit} should be trivially copyable and small enough so we can
// efficiently pass it by value.
ASSERT_TRIVIALLY_COPYABLE(WasmCompilationUnit);
static_assert(sizeof(WasmCompilationUnit) <= 2 * kSystemPointerSize);

class V8_EXPORT_PRIVATE JSToWasmWrapperCompilationUnit final {
 public:
  // A flag to mark whether the compilation unit can skip the compilation
  // and return the builtin (generic) wrapper, when available.
  enum AllowGeneric : bool { kAllowGeneric = true, kDontAllowGeneric = false };

  JSToWasmWrapperCompilationUnit(Isolate* isolate, const FunctionSig* sig,
                                 uint32_t canonical_sig_index,
                                 const wasm::WasmModule* module, bool is_import,
                                 const WasmFeatures& enabled_features,
                                 AllowGeneric allow_generic);
  ~JSToWasmWrapperCompilationUnit();

  Isolate* isolate() const { return isolate_; }

  void Execute();
  Handle<Code> Finalize();

  bool is_import() const { return is_import_; }
  const FunctionSig* sig() const { return sig_; }
  uint32_t canonical_sig_index() const { return canonical_sig_index_; }

  // Run a compilation unit synchronously.
  static Handle<Code> CompileJSToWasmWrapper(Isolate* isolate,
                                             const FunctionSig* sig,
                                             uint32_t canonical_sig_index,
                                             const WasmModule* module,
                                             bool is_import);

  // Run a compilation unit synchronously, but ask for the specific
  // wrapper.
  static Handle<Code> CompileSpecificJSToWasmWrapper(
      Isolate* isolate, const FunctionSig* sig, uint32_t canonical_sig_index,
      const WasmModule* module);

 private:
  // Wrapper compilation is bound to an isolate. Concurrent accesses to the
  // isolate (during the "Execute" phase) must be audited carefully, i.e. we
  // should only access immutable information (like the root table). The isolate
  // is guaranteed to be alive when this unit executes.
  Isolate* isolate_;
  bool is_import_;
  const FunctionSig* sig_;
  uint32_t canonical_sig_index_;
  bool use_generic_wrapper_;
  std::unique_ptr<TurbofanCompilationJob> job_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_COMPILER_H_
