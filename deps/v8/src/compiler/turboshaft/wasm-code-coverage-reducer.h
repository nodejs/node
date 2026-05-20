// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_WASM_CODE_COVERAGE_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_WASM_CODE_COVERAGE_REDUCER_H_

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/operations.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <class Next>
class WasmCodeCoverageReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(WasmCodeCoverageReducer)

  // Increments a coverage counter at a given address.
  V<None> REDUCE(WasmIncCoverageCounter)(Address address) {
    V<WordPtr> counter_address =
        __ WordPtrConstant(reinterpret_cast<uintptr_t>(address));
    V<Word32> value =
        __ LoadOffHeap(counter_address, 0, MemoryRepresentation::Uint32());
    V<Word32> incremented_value = __ Word32Add(value, 1);
    __ StoreOffHeap(counter_address, incremented_value,
                    MemoryRepresentation::Uint32(), 0);
    return V<None>::Invalid();
  }
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_CODE_COVERAGE_REDUCER_H_
