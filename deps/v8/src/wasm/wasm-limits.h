// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_LIMITS_H_
#define V8_WASM_WASM_LIMITS_H_

#include <cstddef>
#include <cstdint>
#include <limits>

namespace v8 {
namespace internal {
namespace wasm {

// The following limits are imposed by V8 on WebAssembly modules.
// The limits are agreed upon with other engines for consistency.
constexpr size_t kV8MaxWasmTypes = 1000000;
constexpr size_t kV8MaxWasmFunctions = 1000000;
constexpr size_t kV8MaxWasmImports = 100000;
constexpr size_t kV8MaxWasmExports = 100000;
constexpr size_t kV8MaxWasmGlobals = 1000000;
constexpr size_t kV8MaxWasmDataSegments = 100000;
// Don't use this limit directly, but use the value of FLAG_wasm_max_mem_pages.
constexpr size_t kV8MaxWasmMemoryPages = 16384;  // = 1 GiB
constexpr size_t kV8MaxWasmStringSize = 100000;
constexpr size_t kV8MaxWasmModuleSize = 1024 * 1024 * 1024;  // = 1 GiB
constexpr size_t kV8MaxWasmFunctionSize = 128 * 1024;
constexpr size_t kV8MaxWasmFunctionLocals = 50000;
constexpr size_t kV8MaxWasmFunctionParams = 1000;
constexpr size_t kV8MaxWasmFunctionMultiReturns = 1000;
constexpr size_t kV8MaxWasmFunctionReturns = 1;
// Don't use this limit directly, but use the value of FLAG_wasm_max_table_size.
constexpr size_t kV8MaxWasmTableSize = 10000000;
constexpr size_t kV8MaxWasmTableEntries = 10000000;
constexpr size_t kV8MaxWasmTables = 1;
constexpr size_t kV8MaxWasmMemories = 1;

constexpr size_t kSpecMaxWasmMemoryPages = 65536;
constexpr size_t kSpecMaxWasmTableSize = 0xFFFFFFFFu;

constexpr uint64_t kWasmMaxHeapOffset =
    static_cast<uint64_t>(
        std::numeric_limits<uint32_t>::max())  // maximum base value
    + std::numeric_limits<uint32_t>::max();    // maximum index value

// Limit the control stack size of the C++ wasm interpreter.
constexpr size_t kV8MaxWasmInterpretedStackSize = 64 * 1024;

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_LIMITS_H_
