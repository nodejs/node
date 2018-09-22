// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_COMPILER_H_
#define V8_WASM_BASELINE_LIFTOFF_COMPILER_H_

#include "src/base/macros.h"

namespace v8 {
namespace internal {
namespace wasm {

struct WasmFeatures;
class ErrorThrower;
class WasmCode;
class WasmCompilationUnit;

class LiftoffCompilationUnit final {
 public:
  explicit LiftoffCompilationUnit(WasmCompilationUnit* wasm_unit)
      : wasm_unit_(wasm_unit) {}

  bool ExecuteCompilation(WasmFeatures* detected);
  WasmCode* FinishCompilation(ErrorThrower*);

 private:
  WasmCompilationUnit* const wasm_unit_;

  // Result of compilation:
  WasmCode* code_;

  DISALLOW_COPY_AND_ASSIGN(LiftoffCompilationUnit);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_COMPILER_H_
