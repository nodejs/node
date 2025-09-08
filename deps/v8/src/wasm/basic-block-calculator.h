// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASIC_BLOCK_CALCULATOR_H_
#define V8_WASM_BASIC_BLOCK_CALCULATOR_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/base/vector.h"
#include "src/wasm/wasm-code-coverage.h"
#include "src/zone/zone-containers.h"

namespace v8::internal::wasm {

class Decoder;

class BasicBlockCalculator {
 public:
  BasicBlockCalculator(Zone* zone, const base::Vector<const uint8_t>& func_code)
      : zone_(zone),
        start_(func_code.begin()),
        end_(func_code.end()),
        basic_block_ranges_(zone) {}

  void V8_EXPORT_PRIVATE ComputeBasicBlocks();
  const base::Vector<const WasmCodeRange> GetCodeRanges() const {
    return {basic_block_ranges_.begin(), basic_block_ranges_.size()};
  }

 private:
  void StartNewBlock(Decoder* decoder);
  void EndCurrentBlock(Decoder* decoder);

  Zone* zone_;
  const uint8_t* start_;
  const uint8_t* end_;

  int current_basic_block_ = -1;
  uint32_t current_basic_block_start_pos_ = 0;
  ZoneVector<WasmCodeRange> basic_block_ranges_;
};

}  // namespace v8::internal::wasm

#endif  // V8_WASM_BASIC_BLOCK_CALCULATOR_H_
