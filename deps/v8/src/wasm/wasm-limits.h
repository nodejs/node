// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_LIMITS_H_
#define V8_WASM_WASM_LIMITS_H_

namespace v8 {
namespace internal {
namespace wasm {

// The following limits are imposed by V8 on WebAssembly modules.
// The limits are agreed upon with other engines for consistency.
const size_t kV8MaxWasmTypes = 1000000;
const size_t kV8MaxWasmFunctions = 1000000;
const size_t kV8MaxWasmImports = 100000;
const size_t kV8MaxWasmExports = 100000;
const size_t kV8MaxWasmGlobals = 1000000;
const size_t kV8MaxWasmDataSegments = 100000;
const size_t kV8MaxWasmMemoryPages = 16384;  // = 1 GiB
const size_t kV8MaxWasmStringSize = 100000;
const size_t kV8MaxWasmModuleSize = 1024 * 1024 * 1024;  // = 1 GiB
const size_t kV8MaxWasmFunctionSize = 128 * 1024;
const size_t kV8MaxWasmFunctionLocals = 50000;
const size_t kV8MaxWasmFunctionParams = 1000;
const size_t kV8MaxWasmFunctionMultiReturns = 1000;
const size_t kV8MaxWasmFunctionReturns = 1;
const size_t kV8MaxWasmTableSize = 10000000;
const size_t kV8MaxWasmTableEntries = 10000000;
const size_t kV8MaxWasmTables = 1;
const size_t kV8MaxWasmMemories = 1;

const size_t kSpecMaxWasmMemoryPages = 65536;
const size_t kSpecMaxWasmTableSize = 0xFFFFFFFFu;

const uint64_t kWasmMaxHeapOffset =
    static_cast<uint64_t>(
        std::numeric_limits<uint32_t>::max())  // maximum base value
    + std::numeric_limits<uint32_t>::max();    // maximum index value

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_LIMITS_H_
