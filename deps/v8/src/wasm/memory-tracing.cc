// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/memory-tracing.h"

#include "src/utils.h"
#include "src/v8memory.h"

namespace v8 {
namespace internal {
namespace wasm {

void TraceMemoryOperation(ExecutionTier tier, const MemoryTracingInfo* info,
                          int func_index, int position, uint8_t* mem_start) {
  EmbeddedVector<char, 64> value;
  auto mem_rep = static_cast<MachineRepresentation>(info->mem_rep);
  switch (mem_rep) {
#define TRACE_TYPE(rep, str, format, ctype1, ctype2)                     \
  case MachineRepresentation::rep:                                       \
    SNPrintF(value, str ":" format,                                      \
             ReadLittleEndianValue<ctype1>(                              \
                 reinterpret_cast<Address>(mem_start) + info->address),  \
             ReadLittleEndianValue<ctype2>(                              \
                 reinterpret_cast<Address>(mem_start) + info->address)); \
    break;
    TRACE_TYPE(kWord8, " i8", "%d / %02x", uint8_t, uint8_t)
    TRACE_TYPE(kWord16, "i16", "%d / %04x", uint16_t, uint16_t)
    TRACE_TYPE(kWord32, "i32", "%d / %08x", uint32_t, uint32_t)
    TRACE_TYPE(kWord64, "i64", "%" PRId64 " / %016" PRIx64, uint64_t, uint64_t)
    TRACE_TYPE(kFloat32, "f32", "%f / %08x", float, uint32_t)
    TRACE_TYPE(kFloat64, "f64", "%f / %016" PRIx64, double, uint64_t)
#undef TRACE_TYPE
    default:
      SNPrintF(value, "???");
  }
  const char* eng = "?";
  switch (tier) {
    case ExecutionTier::kOptimized:
      eng = "turbofan";
      break;
    case ExecutionTier::kBaseline:
      eng = "liftoff";
      break;
    case ExecutionTier::kInterpreter:
      eng = "interpreter";
      break;
  }
  printf("%-11s func:%6d+0x%-6x%s %08x val: %s\n", eng, func_index, position,
         info->is_store ? " store to" : "load from", info->address,
         value.start());
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
