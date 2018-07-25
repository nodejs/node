// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_COMPILER_H_
#define V8_WASM_BASELINE_LIFTOFF_COMPILER_H_

#include "src/source-position-table.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/baseline/liftoff-assembler.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/function-compiler.h"

namespace v8 {
namespace internal {
namespace wasm {

class LiftoffCompilationUnit final {
 public:
  explicit LiftoffCompilationUnit(WasmCompilationUnit* wasm_unit)
      : wasm_unit_(wasm_unit), asm_(wasm_unit->isolate_) {}

  bool ExecuteCompilation();
  wasm::WasmCode* FinishCompilation(wasm::ErrorThrower*);
  void AbortCompilation();

 private:
  WasmCompilationUnit* const wasm_unit_;
  wasm::LiftoffAssembler asm_;
  int safepoint_table_offset_;
  SourcePositionTableBuilder source_position_table_builder_;
  std::unique_ptr<std::vector<trap_handler::ProtectedInstructionData>>
      protected_instructions_;

  // The {codegen_zone_} needs to survive until FinishCompilation. It's only
  // rarely used (e.g. for runtime calls), so it's only allocated when needed.
  std::unique_ptr<Zone> codegen_zone_;

  DISALLOW_COPY_AND_ASSIGN(LiftoffCompilationUnit);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_COMPILER_H_
