// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_FUNCTION_COMPILER_H_
#define V8_WASM_FUNCTION_COMPILER_H_

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

namespace compiler {
class Pipeline;
class TurbofanWasmCompilationUnit;
}  // namespace compiler

namespace wasm {

class LiftoffCompilationUnit;
class NativeModule;
class WasmCode;
class WasmCompilationUnit;
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

  explicit WasmCompilationResult(WasmError error) : error(std::move(error)) {}

  bool succeeded() const {
    DCHECK_EQ(code_desc.buffer != nullptr, error.empty());
    return error.empty();
  }
  operator bool() const { return succeeded(); }

  CodeDesc code_desc;
  std::unique_ptr<uint8_t[]> instr_buffer;
  uint32_t frame_slot_count = 0;
  size_t safepoint_table_offset = 0;
  size_t handler_table_offset = 0;
  OwnedVector<byte> source_positions;
  OwnedVector<trap_handler::ProtectedInstructionData> protected_instructions;

  WasmError error;
};

class WasmCompilationUnit final {
 public:
  static ExecutionTier GetDefaultExecutionTier(const WasmModule*);

  // If constructing from a background thread, pass in a Counters*, and ensure
  // that the Counters live at least as long as this compilation unit (which
  // typically means to hold a std::shared_ptr<Counters>).
  // If used exclusively from a foreground thread, Isolate::counters() may be
  // used by callers to pass Counters.
  WasmCompilationUnit(WasmEngine*, int index, ExecutionTier);

  ~WasmCompilationUnit();

  WasmCompilationResult ExecuteCompilation(
      CompilationEnv*, const std::shared_ptr<WireBytesStorage>&, Counters*,
      WasmFeatures* detected);

  WasmCode* Publish(WasmCompilationResult, NativeModule*);

  ExecutionTier tier() const { return tier_; }

  static void CompileWasmFunction(Isolate*, NativeModule*,
                                  WasmFeatures* detected, const WasmFunction*,
                                  ExecutionTier);

 private:
  friend class LiftoffCompilationUnit;
  friend class compiler::TurbofanWasmCompilationUnit;

  WasmEngine* const wasm_engine_;
  const int func_index_;
  ExecutionTier tier_;
  WasmCode* result_ = nullptr;

  // LiftoffCompilationUnit, set if {tier_ == kLiftoff}.
  std::unique_ptr<LiftoffCompilationUnit> liftoff_unit_;
  // TurbofanWasmCompilationUnit, set if {tier_ == kTurbofan}.
  std::unique_ptr<compiler::TurbofanWasmCompilationUnit> turbofan_unit_;

  void SwitchTier(ExecutionTier new_tier);

  DISALLOW_COPY_AND_ASSIGN(WasmCompilationUnit);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_COMPILER_H_
