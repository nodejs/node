// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_FUNCTION_COMPILER_H_
#define V8_WASM_FUNCTION_COMPILER_H_

#include <memory>

#include "src/codegen/code-desc.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-tier.h"

namespace v8 {
namespace internal {

class AssemblerBuffer;
class Counters;
class OptimizedCompilationJob;

namespace wasm {

class NativeModule;
class WasmCode;
class WasmEngine;
struct WasmFunction;

class WasmInstructionBuffer final {
 public:
  WasmInstructionBuffer() = delete;
  ~WasmInstructionBuffer();
  std::unique_ptr<AssemblerBuffer> CreateView();
  std::unique_ptr<uint8_t[]> ReleaseBuffer();

  // Allocate a new {WasmInstructionBuffer}. The size is the maximum of {size}
  // and {AssemblerBase::kMinimalSize}.
  static std::unique_ptr<WasmInstructionBuffer> New(size_t size = 0);

  // Override {operator delete} to avoid implicit instantiation of {operator
  // delete} with {size_t} argument. The {size_t} argument would be incorrect.
  void operator delete(void* ptr) { ::operator delete(ptr); }

 private:
  DISALLOW_COPY_AND_ASSIGN(WasmInstructionBuffer);
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
  operator bool() const { return succeeded(); }

  CodeDesc code_desc;
  std::unique_ptr<uint8_t[]> instr_buffer;
  uint32_t frame_slot_count = 0;
  uint32_t tagged_parameter_slots = 0;
  OwnedVector<byte> source_positions;
  OwnedVector<byte> protected_instructions_data;
  int func_index = kAnonymousFuncIndex;
  ExecutionTier requested_tier;
  ExecutionTier result_tier;
  Kind kind = kFunction;
  ForDebugging for_debugging = kNoDebugging;
};

class V8_EXPORT_PRIVATE WasmCompilationUnit final {
 public:
  static ExecutionTier GetBaselineExecutionTier(const WasmModule*);

  WasmCompilationUnit(int index, ExecutionTier tier, ForDebugging for_debugging)
      : func_index_(index), tier_(tier), for_debugging_(for_debugging) {}

  WasmCompilationResult ExecuteCompilation(
      WasmEngine*, CompilationEnv*, const std::shared_ptr<WireBytesStorage>&,
      Counters*, WasmFeatures* detected);

  ExecutionTier tier() const { return tier_; }
  int func_index() const { return func_index_; }

  static void CompileWasmFunction(Isolate*, NativeModule*,
                                  WasmFeatures* detected, const WasmFunction*,
                                  ExecutionTier);

 private:
  WasmCompilationResult ExecuteFunctionCompilation(
      WasmEngine* wasm_engine, CompilationEnv* env,
      const std::shared_ptr<WireBytesStorage>& wire_bytes_storage,
      Counters* counters, WasmFeatures* detected);

  WasmCompilationResult ExecuteImportWrapperCompilation(WasmEngine* engine,
                                                        CompilationEnv* env);

  int func_index_;
  ExecutionTier tier_;
  ForDebugging for_debugging_;
};

// {WasmCompilationUnit} should be trivially copyable and small enough so we can
// efficiently pass it by value.
ASSERT_TRIVIALLY_COPYABLE(WasmCompilationUnit);
STATIC_ASSERT(sizeof(WasmCompilationUnit) <= 2 * kSystemPointerSize);

class V8_EXPORT_PRIVATE JSToWasmWrapperCompilationUnit final {
 public:
  JSToWasmWrapperCompilationUnit(Isolate* isolate, WasmEngine* wasm_engine,
                                 const FunctionSig* sig, bool is_import,
                                 const WasmFeatures& enabled_features);
  ~JSToWasmWrapperCompilationUnit();

  void Execute();
  Handle<Code> Finalize(Isolate* isolate);

  bool is_import() const { return is_import_; }
  const FunctionSig* sig() const { return sig_; }

  // Run a compilation unit synchronously.
  static Handle<Code> CompileJSToWasmWrapper(Isolate* isolate,
                                             const FunctionSig* sig,
                                             bool is_import);

 private:
  bool is_import_;
  const FunctionSig* sig_;
  std::unique_ptr<OptimizedCompilationJob> job_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_COMPILER_H_
