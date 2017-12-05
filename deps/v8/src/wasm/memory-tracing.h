// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MEMORY_TRACING_H
#define V8_MEMORY_TRACING_H

#include <cstdint>

#include "src/machine-type.h"

namespace v8 {
namespace internal {
namespace tracing {

enum ExecutionEngine { kWasmCompiled, kWasmInterpreted };

// Callback for tracing a memory operation for debugging.
// Triggered by --wasm-trace-memory.
void TraceMemoryOperation(ExecutionEngine, bool is_store, MachineRepresentation,
                          uint32_t addr, int func_index, int position,
                          uint8_t* mem_start);

}  // namespace tracing
}  // namespace internal
}  // namespace v8

#endif /* !V8_MEMORY_TRACING_H */
