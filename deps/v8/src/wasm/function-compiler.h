// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_FUNCTION_COMPILER_H_
#define V8_WASM_FUNCTION_COMPILER_H_

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

namespace wasm {

class NativeModule;
class WasmCode;
class WasmEngine;
struct WasmFunction;

class WasmInstructionBuffer final {
 public:
  ~WasmInstructionBuffer();
  std::unique_ptr<AssemblerBuffer> CreateView();
  std::unique_ptr<uint8_t[]> ReleaseBuffer();

  static std::unique_ptr<WasmInstructionBuffer> New();

 private:
  WasmInstructionBuffer() = delete;
  DISALLOW_COPY_AND_ASSIGN(WasmInstructionBuffer);
};

struct WasmCompilationResult {
 public:
  MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(WasmCompilationResult);

  bool succeeded() const { return code_desc.buffer != nullptr; }
  bool failed() const { return !succeeded(); }
  operator bool() const { return succeeded(); }

  CodeDesc code_desc;
  std::unique_ptr<uint8_t[]> instr_buffer;
  uint32_t frame_slot_count = 0;
  uint32_t tagged_parameter_slots = 0;
  OwnedVector<byte> source_positions;
  OwnedVector<trap_handler::ProtectedInstructionData> protected_instructions;
  int func_index;
  ExecutionTier requested_tier;
  ExecutionTier result_tier;
};

class V8_EXPORT_PRIVATE WasmCompilationUnit final {
 public:
  static ExecutionTier GetDefaultExecutionTier(const WasmModule*);

  WasmCompilationUnit(int index, ExecutionTier tier)
      : func_index_(index), tier_(tier) {}

  WasmCompilationResult ExecuteCompilation(
      WasmEngine*, CompilationEnv*, const std::shared_ptr<WireBytesStorage>&,
      Counters*, WasmFeatures* detected);

  ExecutionTier tier() const { return tier_; }
  int func_index() const { return func_index_; }

  static void CompileWasmFunction(Isolate*, NativeModule*,
                                  WasmFeatures* detected, const WasmFunction*,
                                  ExecutionTier);

 private:
  int func_index_;
  ExecutionTier tier_;
};

// {WasmCompilationUnit} should be trivially copyable and small enough so we can
// efficiently pass it by value.
ASSERT_TRIVIALLY_COPYABLE(WasmCompilationUnit);
STATIC_ASSERT(sizeof(WasmCompilationUnit) <= 2 * kSystemPointerSize);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_COMPILER_H_
