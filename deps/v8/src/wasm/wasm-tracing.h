// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_TRACING_H_
#define V8_WASM_WASM_TRACING_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <cstdint>

#include "src/codegen/machine-type.h"
#include "src/execution/frames.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-tier.h"

namespace v8::internal::wasm {

// This struct is created in generated code, hence use low-level types.
struct MemoryTracingInfo {
  uintptr_t offset;
  uint32_t mem_index;
  uint8_t is_store;  // 0 or 1
  uint8_t mem_rep;
  static_assert(std::is_same_v<decltype(mem_rep),
                               std::underlying_type_t<MachineRepresentation>>,
                "MachineRepresentation uses uint8_t");

  MemoryTracingInfo(uintptr_t offset, uint32_t mem_index, bool is_store,
                    MachineRepresentation rep)
      : offset(offset),
        mem_index(mem_index),
        is_store(is_store),
        mem_rep(static_cast<uint8_t>(rep)) {}
};

struct GlobalTracingInfo {
  uint32_t global_index;
  uint8_t is_store;  // 0 or 1
};

struct GlobalTraceEntry {
  int function_index;
  uint32_t global_index;
  int frame_position;
  wasm::ExecutionTier tier;
  wasm::ValueKind kind;
  bool is_store;

  uint8_t value_bytes[16];

  bool operator==(const GlobalTraceEntry& other) const {
    bool value_matches = true;
    // For references, value_bytes holds the address, which doesn't make sense
    // to compare.
    if (!is_reference(kind)) {
      value_matches =
          (memcmp(value_bytes, other.value_bytes, value_kind_size(kind)) == 0);
    }

    return (function_index == other.function_index) &&
           (global_index == other.global_index) &&
           (is_store == other.is_store) && (kind == other.kind) &&
           value_matches;
  }
};

struct MemoryTraceEntry {
  uintptr_t offset;
  int function_index;
  uint32_t mem_index;
  int frame_position;
  wasm::ExecutionTier tier;
  MachineRepresentation representation;
  bool is_store;

  uint8_t value_bytes[16];

  bool operator==(const MemoryTraceEntry& other) const {
    return (function_index == other.function_index) &&
           (mem_index == other.mem_index) && (is_store == other.is_store) &&
           (representation == other.representation) &&
           (offset == other.offset) &&
           (memcmp(value_bytes, other.value_bytes,
                   ElementSizeInBytes(representation)) == 0);
  }
};

using GlobalTrace = std::vector<GlobalTraceEntry>;
using MemoryTrace = std::vector<MemoryTraceEntry>;

struct WasmTracesForTesting {
  // Store the entry structs instead of strings for more flexibility when
  // processing the traces and the structs are smaller in size than the strings.
  GlobalTrace global_trace;
  MemoryTrace memory_trace;
  // This flag decides whether the memory and global traces are stored in the
  // above vectors or are printed to stdout.
  bool should_store_trace = false;
};

V8_EXPORT_PRIVATE wasm::WasmTracesForTesting& GetWasmTracesForTesting();
V8_EXPORT_PRIVATE void PrintMemoryTraceString(
    const wasm::MemoryTraceEntry& trace_entry,
    wasm::NativeModule* native_module, std::ostream& outs);
V8_EXPORT_PRIVATE void PrintGlobalTraceString(
    const wasm::GlobalTraceEntry& trace_entry,
    wasm::NativeModule* native_module, std::ostream& outs);

}  // namespace v8::internal::wasm

#endif  // V8_WASM_WASM_TRACING_H_
