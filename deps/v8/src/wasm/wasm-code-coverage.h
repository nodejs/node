// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_CODE_COVERAGE_H_
#define V8_WASM_WASM_CODE_COVERAGE_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <stddef.h>
#include <stdint.h>

#include "src/base/platform/mutex.h"
#include "src/base/vector.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8::internal::wasm {

// Represents a range of code byte offsets in the form [start, end], inclusive
// at both ends.
// Offsets are calculated from the start of the function wire bytes, not from
// the start of the module.
struct WasmCodeRange {
  constexpr WasmCodeRange() = default;
  constexpr WasmCodeRange(int start, int end) : start(start), end(end) {}

  constexpr bool operator==(const WasmCodeRange& rhs) const = default;

  int start = kNoCodePosition;
  int end = kNoCodePosition;

 private:
  static constexpr int kNoCodePosition = -1;
};

class WasmFunctionCoverageData {
 public:
  explicit WasmFunctionCoverageData(
      base::Vector<const WasmCodeRange> code_ranges) {
    counts_ = base::OwnedVector<uint32_t>::New(code_ranges.size());
    code_ranges_ = base::OwnedCopyOf(code_ranges);
  }

  const base::Vector<const WasmCodeRange> code_ranges() const {
    return base::VectorOf(code_ranges_);
  }

  const base::Vector<uint32_t> counters() const {
    return base::VectorOf(counts_);
  }

 private:
  base::OwnedVector<const WasmCodeRange> code_ranges_;
  base::OwnedVector<uint32_t> counts_;
};

class WasmModuleCoverageData {
 public:
  explicit WasmModuleCoverageData(uint32_t declared_function_count) {
    function_data_ =
        base::OwnedVector<std::unique_ptr<WasmFunctionCoverageData>>::New(
            declared_function_count);
  }

  WasmFunctionCoverageData* InstallCoverageData(
      int declared_function_index,
      base::Vector<const WasmCodeRange> code_ranges) {
    base::MutexGuard guard{&mutex_};

    if (!function_data_[declared_function_index]) {
      function_data_[declared_function_index] =
          std::make_unique<WasmFunctionCoverageData>(code_ranges);
    } else {
      DCHECK_EQ(function_data_[declared_function_index]->code_ranges(),
                code_ranges);
    }
    return function_data_[declared_function_index].get();
  }

  size_t function_count() const { return function_data_.size(); }

  WasmFunctionCoverageData* GetFunctionCoverageData(
      size_t function_index) const {
    base::MutexGuard guard{&mutex_};

    DCHECK_LT(function_index, function_data_.size());
    return function_data_[function_index].get();
  }

 private:
  // There is a single WasmModuleCoverageData per NativeModule, that can be
  // accessed concurrently from multiple Isolates.
  mutable base::Mutex mutex_;
  base::OwnedVector<std::unique_ptr<WasmFunctionCoverageData>> function_data_;
};

template <typename FullDecoder>
class WasmCoverageInstrumentation {
 public:
  WasmCoverageInstrumentation(WasmFunctionCoverageData* coverage_data) {
    DCHECK(v8_flags.wasm_code_coverage);
    DCHECK_NOT_NULL(coverage_data);
    coverage_code_ranges_ = coverage_data->code_ranges();
    coverage_current_code_range_ = coverage_code_ranges_.begin();
    coverage_count_array_addr_ =
        reinterpret_cast<Address>(coverage_data->counters().data());
  }

  template <typename EmitCoverageIncFunc>
  void EmitCoverageInstrumentationIfReachable(
      FullDecoder* decoder, WasmOpcode opcode,
      EmitCoverageIncFunc emit_coverage_inc) {
    if (!decoder->control_at(0)->reachable()) return;

    if (coverage_current_code_range_ == coverage_code_ranges_.end()) return;

    while (coverage_current_code_range_->end < decoder->position()) {
      ++coverage_current_code_range_;
    }
    if (coverage_current_code_range_->start <= decoder->position()) {
      // Emit code coverage instrumentation.
      Address counter_addr =
          coverage_count_array_addr_ +
          (coverage_current_code_range_ - coverage_code_ranges_.begin()) *
              sizeof(uint32_t);
      emit_coverage_inc(counter_addr);
      ++coverage_current_code_range_;
    }
  }

 private:
  // A precalculated set of code ranges that define the basic blocks in the
  // current function. Note that the NativeModule can be deleted during the
  // compilation, but these code ranges are kept alive via a shared_ptr in the
  // CompilationEnv.
  base::Vector<const WasmCodeRange> coverage_code_ranges_;

  // Points to the current WasmCodeRange during compilation, it's used to decide
  // whether we are in a new basic block and we need to emit coverage
  // instrumentation.
  const WasmCodeRange* coverage_current_code_range_ = nullptr;

  // Points to the start of an array of uint32_t counters that represent the
  // coverage count for each basic block in the function. We emit
  // instrumentation code to increase the counter with {emit_inc_i32_at}.
  Address coverage_count_array_addr_ = 0;
};

}  // namespace v8::internal::wasm
#endif  // V8_WASM_WASM_CODE_COVERAGE_H_
