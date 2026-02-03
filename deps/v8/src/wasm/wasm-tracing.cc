// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-tracing.h"

#include <sstream>
#include <type_traits>

#include "absl/strings/str_format.h"
#include "src/base/lazy-instance.h"
#include "src/base/macros.h"
#include "src/base/memory.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8::internal::wasm {

static thread_local base::LeakyObject<wasm::WasmTracesForTesting>
    wasm_traces_object;

wasm::WasmTracesForTesting& GetWasmTracesForTesting() {
  return *wasm_traces_object.get();
}

template <typename T, typename BitRepT = T>
static void PrintMemoryTraceValue(const uint8_t* ptr, const char* type_str,
                                  std::ostream& outs) {
  static_assert(sizeof(T) == sizeof(BitRepT));
  static_assert(sizeof(T) <= sizeof(MemoryTraceEntry::value_bytes));

  const BitRepT hex_value =
      base::ReadLittleEndianValue<BitRepT>(reinterpret_cast<Address>(ptr));

  T value = base::bit_cast<T>(hex_value);

  if constexpr (std::is_same_v<T, uint8_t>) {
    outs << absl::StreamFormat("%4s:%u / 0x%x", type_str, value, hex_value);
  } else {
    outs << absl::StreamFormat("%4s:%v / 0x%x", type_str, value, hex_value);
  }
}

void PrintMemoryTraceString(const wasm::MemoryTraceEntry& trace_entry,
                            wasm::NativeModule* native_module,
                            std::ostream& outs) {
  base::Vector<const uint8_t> wire_bytes = native_module->wire_bytes();
  const uint8_t* pc = wire_bytes.begin() + trace_entry.frame_position;
  wasm::WasmOpcode opcode = wasm::WasmOpcode(*pc);
  if (wasm::WasmOpcodes::IsPrefixOpcode(opcode)) {
    opcode = wasm::Decoder(wire_bytes)
                 .read_prefixed_opcode<wasm::Decoder::NoValidationTag>(pc)
                 .first;
  }

  outs << absl::StreamFormat(
      "%-8s func %5d:0x%-4x mem %d %-12s %s %016x val: ",
      ExecutionTierToString(trace_entry.tier), trace_entry.function_index,
      trace_entry.frame_position, trace_entry.mem_index,
      wasm::WasmOpcodes::OpcodeName(opcode),
      trace_entry.is_store ? "to  " : "from", trace_entry.offset);

  switch (trace_entry.representation) {
    case MachineRepresentation::kWord8: {
      PrintMemoryTraceValue<uint8_t>(trace_entry.value_bytes, "i8", outs);
      break;
    }
    case MachineRepresentation::kWord16:
      PrintMemoryTraceValue<uint16_t>(trace_entry.value_bytes, "i16", outs);
      break;
    case MachineRepresentation::kWord32:
      PrintMemoryTraceValue<uint32_t>(trace_entry.value_bytes, "i32", outs);
      break;
    case MachineRepresentation::kWord64:
      PrintMemoryTraceValue<uint64_t>(trace_entry.value_bytes, "i64", outs);
      break;
    case MachineRepresentation::kFloat32: {
      PrintMemoryTraceValue<float, uint32_t>(trace_entry.value_bytes, "f32",
                                             outs);
      break;
    }
    case MachineRepresentation::kFloat64: {
      PrintMemoryTraceValue<double, uint64_t>(trace_entry.value_bytes, "f64",
                                              outs);
      break;
    }
    case MachineRepresentation::kSimd128: {
      Address addr = reinterpret_cast<Address>(trace_entry.value_bytes);
      const auto a = base::ReadLittleEndianValue<uint32_t>(addr);
      const auto b = base::ReadLittleEndianValue<uint32_t>(addr + 4);
      const auto c = base::ReadLittleEndianValue<uint32_t>(addr + 8);
      const auto d = base::ReadLittleEndianValue<uint32_t>(addr + 12);
      outs << absl::StreamFormat("s128:%d %d %d %d / %08x %08x %08x %08x", a, b,
                                 c, d, a, b, c, d);
      break;
    }
    default:
      break;
  }
}

void PrintGlobalTraceString(const wasm::GlobalTraceEntry& trace_entry,
                            wasm::NativeModule* native_module,
                            std::ostream& outs) {
  const base::Vector<const uint8_t>& wire_bytes = native_module->wire_bytes();
  const uint8_t* pc = wire_bytes.begin() + trace_entry.frame_position;
  wasm::WasmOpcode opcode = wasm::WasmOpcode(*pc);
  if (wasm::WasmOpcodes::IsPrefixOpcode(opcode)) {
    opcode = wasm::Decoder(wire_bytes)
                 .read_prefixed_opcode<wasm::Decoder::NoValidationTag>(pc)
                 .first;
  }

  const wasm::WasmModule* module = native_module->module();
  wasm::CanonicalValueType type =
      module->canonical_type(module->globals[trace_entry.global_index].type);

  outs << absl::StreamFormat(
      "%-8s func %5d:0x%-4x %s %d val: %s:",
      ExecutionTierToString(trace_entry.tier), trace_entry.function_index,
      trace_entry.frame_position, wasm::WasmOpcodes::OpcodeName(opcode),
      trace_entry.global_index, type.name());

  if (type.is_numeric()) {
    wasm::WasmValue value = wasm::WasmValue(trace_entry.value_bytes, type);
    outs << value.to_string();
  } else {
    Address addr = base::ReadUnalignedValue<Address>(
        reinterpret_cast<Address>(trace_entry.value_bytes));
    outs << absl::StreamFormat("0x%x", addr);
  }
}
}  // namespace v8::internal::wasm
