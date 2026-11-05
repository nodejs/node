// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_GENERATORS_WASM_MODULE_H_
#define V8_EXPERIMENTAL_REDLINE_GENERATORS_WASM_MODULE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "experimental/redline/core/random.h"

namespace redline {

// Value types of the generated modules (their binary encoding).
enum class WasmType : uint8_t {
  kI32 = 0x7F,
  kI64 = 0x7E,
  kF32 = 0x7D,
  kF64 = 0x7C,
};

struct WasmSignature {
  std::vector<WasmType> params;
  std::vector<WasmType> results;  // empty or one value
};

// A random WebAssembly module, valid by construction, and what the JavaScript
// side needs to use it.
struct WasmModule {
  std::vector<uint8_t> bytes;
  // Imported functions "m"."f0", "m"."f1", ... with their signatures.
  std::vector<WasmSignature> imports;
  // Exported functions "e0", "e1", ... with their signatures.
  std::vector<WasmSignature> functions;
  // Exported mutable globals "g0", "g1", ... with their types.
  std::vector<WasmType> globals;
  bool memory = false;  // exported as "mem"
  bool table = false;   // exported as "tab"
};

// Builds a module from general building blocks: function types, imports,
// a memory with data, a function table with an element segment, globals and
// functions whose bodies are typed expression trees (arithmetic,
// comparisons, conversions, loads and stores, calls, call_indirect, select,
// blocks, ifs and bounded loops). Calls only go to lower function indices,
// loops are counted, and memory addresses are masked, so most executions
// terminate without a trap.
WasmModule RandomWasmModule(Random& r);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_GENERATORS_WASM_MODULE_H_
