// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_CODE_COVERAGE_H_
#define V8_WASM_WASM_CODE_COVERAGE_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

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

}  // namespace v8::internal::wasm
//
#endif  // V8_WASM_WASM_CODE_COVERAGE_H_
