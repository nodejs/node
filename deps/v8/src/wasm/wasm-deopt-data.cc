// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-deopt-data.h"

#include "src/base/sanitizer/msan.h"
#include "src/objects/deoptimization-data.h"

namespace v8::internal::wasm {

std::vector<DeoptimizationLiteral>
WasmDeoptView::BuildDeoptimizationLiteralArray() {
  DCHECK(HasDeoptData());
  std::vector<DeoptimizationLiteral> deopt_literals(
      base_data_.num_deopt_literals);
  base::Vector<const uint8_t> buffer =
      deopt_data_ + sizeof(base_data_) + base_data_.translation_array_size +
      sizeof(WasmDeoptEntry) * base_data_.entry_count;
  for (uint32_t i = 0; i < base_data_.num_deopt_literals; ++i) {
    buffer += DeoptimizationLiteral::Read(buffer, deopt_literals.data() + i);
  }
  DCHECK_EQ(0, buffer.size());
  return deopt_literals;
}

base::OwnedVector<uint8_t> WasmDeoptDataProcessor::Serialize(
    int deopt_exit_start_offset, int eager_deopt_count,
    base::Vector<const uint8_t> translation_array,
    base::Vector<wasm::WasmDeoptEntry> deopt_entries,
    const ZoneDeque<DeoptimizationLiteral>& deopt_literals) {
  wasm::WasmDeoptData data;
  data.entry_count = eager_deopt_count;
  data.deopt_exit_start_offset = deopt_exit_start_offset;
  data.eager_deopt_count = eager_deopt_count;
  data.num_deopt_literals = static_cast<uint32_t>(deopt_literals.size());

  data.translation_array_size = static_cast<uint32_t>(translation_array.size());

  size_t translation_array_byte_size =
      translation_array.size() * sizeof(translation_array[0]);
  size_t deopt_entries_byte_size =
      deopt_entries.size() * sizeof(deopt_entries[0]);
  size_t deopt_literals_byte_size =
      std::accumulate(deopt_literals.begin(), deopt_literals.end(), size_t{0},
                      [](size_t acc, const DeoptimizationLiteral& literal) {
                        return acc + literal.SerializationSize();
                      });
  size_t byte_size = sizeof(data) + translation_array_byte_size +
                     deopt_entries_byte_size + deopt_literals_byte_size;
  auto result = base::OwnedVector<uint8_t>::New(byte_size);
  base::Vector<uint8_t> remaining_buffer = result.as_vector();
  std::memcpy(remaining_buffer.begin(), &data, sizeof(data));
  remaining_buffer += sizeof(data);
  std::memcpy(remaining_buffer.begin(), translation_array.data(),
              translation_array_byte_size);
  remaining_buffer += translation_array_byte_size;
  std::memcpy(remaining_buffer.begin(), deopt_entries.data(),
              deopt_entries_byte_size);
  remaining_buffer += deopt_entries_byte_size;
  for (const auto& literal : deopt_literals) {
    // We can't serialize objects. Wasm should never contain object literals as
    // it is isolate-independent.
    CHECK_NE(literal.kind(), DeoptimizationLiteralKind::kObject);
    remaining_buffer += literal.Write(remaining_buffer);
  }
  DCHECK_EQ(0, remaining_buffer.size());
  MSAN_CHECK_MEM_IS_INITIALIZED(result.begin(), result.size());
  return result;
}

}  // namespace v8::internal::wasm
