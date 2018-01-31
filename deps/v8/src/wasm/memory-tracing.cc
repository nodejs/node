// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/memory-tracing.h"

#include "src/utils.h"

namespace v8 {
namespace internal {
namespace tracing {

void TraceMemoryOperation(ExecutionEngine engine, bool is_store,
                          MachineRepresentation rep, uint32_t addr,
                          int func_index, int position, uint8_t* mem_start) {
  EmbeddedVector<char, 64> value;
  switch (rep) {
#define TRACE_TYPE(rep, str, format, ctype1, ctype2)           \
  case MachineRepresentation::rep:                             \
    SNPrintF(value, str ":" format,                            \
             ReadLittleEndianValue<ctype1>(mem_start + addr),  \
             ReadLittleEndianValue<ctype2>(mem_start + addr)); \
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
  char eng_c = '?';
  switch (engine) {
    case kWasmCompiled:
      eng_c = 'C';
      break;
    case kWasmInterpreted:
      eng_c = 'I';
      break;
  }
  printf("%c %8d+0x%-6x %s @%08x %s\n", eng_c, func_index, position,
         is_store ? "store" : "read ", addr, value.start());
}

}  // namespace tracing
}  // namespace internal
}  // namespace v8
