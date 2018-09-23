// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MEMORY_TRACING_H_
#define V8_WASM_MEMORY_TRACING_H_

#include <cstdint>

#include "src/machine-type.h"
#include "src/wasm/wasm-tier.h"

namespace v8 {
namespace internal {
namespace wasm {

// This struct is create in generated code, hence use low-level types.
struct MemoryTracingInfo {
  uint32_t address;
  uint8_t is_store;  // 0 or 1
  uint8_t mem_rep;
  static_assert(
      std::is_same<decltype(mem_rep),
                   std::underlying_type<MachineRepresentation>::type>::value,
      "MachineRepresentation uses uint8_t");

  MemoryTracingInfo(uint32_t addr, bool is_store, MachineRepresentation rep)
      : address(addr), is_store(is_store), mem_rep(static_cast<uint8_t>(rep)) {}
};

// Callback for tracing a memory operation for debugging.
// Triggered by --wasm-trace-memory.
void TraceMemoryOperation(ExecutionTier, const MemoryTracingInfo* info,
                          int func_index, int position, uint8_t* mem_start);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MEMORY_TRACING_H_
