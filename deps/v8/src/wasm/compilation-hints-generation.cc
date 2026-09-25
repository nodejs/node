// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/compilation-hints-generation.h"

#include "src/base/strings.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"

namespace v8::internal::wasm {

void EmitCompilationHintsToBuffer(ZoneBuffer& buffer,
                                  NativeModule* native_module) {
  const WasmModule* module = native_module->module();

  // Do not emit the wasm-module prelude for now.
  // buffer.write_u32(kWasmMagic);
  // buffer.write_u32(kWasmVersion);

  // Emit the compilation-priority section.
  {
    buffer.write_u8(kUnknownSectionCode);
    size_t section_size_offset = buffer.reserve_u32v();
    size_t section_body_begin = buffer.offset();
    buffer.write_string(base::StaticCharVector(kCompilationPriorityString));
    uint32_t num_functions = 0;
    size_t num_functions_offset = buffer.reserve_u32v();
    base::MutexGuard marked_for_tierup_mutex_guard(
        &module->marked_for_tierup_mutex);
    for (uint32_t func_index = module->num_imported_functions;
         func_index < module->functions.size(); func_index++) {
      wasm::WasmCodeRefScope code_ref_scope;
      wasm::WasmCode* code = native_module->GetCode(func_index);
      if (code) {
        // No tierup when generating compilation hints.
        DCHECK(code->is_liftoff());
        // The function was compiled; emit a compilation-priority hint.
        num_functions++;
        buffer.write_u32v(func_index);
        buffer.write_u8(0);  // Offset 0 for function-level hint.

        if (module->marked_for_tierup.contains(func_index)) {
          // Function was marked for optimization; also emit optimization
          // priority.
          buffer.write_u8(2);    // Hint size.
          buffer.write_u32v(0);  // Compilation priority.
          buffer.write_u32v(0);  // Optimization priority.
        } else {
          buffer.write_u8(1);    // Hint size.
          buffer.write_u32v(0);  // Compilation priority.
        }
      }
    }

    buffer.patch_u32v(num_functions_offset, num_functions);
    buffer.patch_u32v(
        section_size_offset,
        static_cast<uint32_t>(buffer.offset() - section_body_begin));
  }

  // Emit the instruction-frequency section.
  {
    base::MutexGuard mutex(&module->type_feedback.mutex);
    buffer.write_u8(kUnknownSectionCode);
    size_t section_size_offset = buffer.reserve_u32v();
    size_t section_body_begin = buffer.offset();
    buffer.write_string(base::StaticCharVector(kInstructionFrequenciesString));

    uint32_t num_functions = 0;
    size_t num_functions_offset = buffer.reserve_u32v();

    std::unordered_map<uint32_t, wasm::FunctionTypeFeedback>& feedback =
        module->type_feedback.feedback_for_function;

    for (uint32_t func_index = module->num_imported_functions;
         func_index < module->functions.size(); func_index++) {
      auto it = feedback.find(func_index);
      if (it == feedback.end()) continue;  // No feedback.

      num_functions++;
      buffer.write_u32v(func_index);

      wasm::FunctionTypeFeedback& feedback_for_function = it->second;

      buffer.write_u32v(
          static_cast<uint32_t>(feedback_for_function.feedback_vector.size()));

      for (size_t slot_index = 0;
           slot_index < feedback_for_function.feedback_vector.size();
           slot_index++) {
        wasm::CallSiteFeedback& slot =
            feedback_for_function.feedback_vector[slot_index];
        int total_count_at_slot = 0;
        for (int call = 0; call < slot.num_cases(); call++) {
          total_count_at_slot += slot.call_count(call);
        }

        uint32_t offset =
            module->feedback_slots_to_wire_byte_offsets[func_index][slot_index];

        double relative_call_count = static_cast<double>(total_count_at_slot) /
                                     feedback_for_function.num_invocations;

        uint8_t log_relative_call_count = std::max(
            uint8_t{1},
            std::min(uint8_t{64}, static_cast<uint8_t>(
                                      std::log2(relative_call_count) + 32)));

        buffer.write_u32v(offset);
        buffer.write_u32v(1);  // Hint length.
        buffer.write_u8(log_relative_call_count);
      }

      buffer.patch_u32v(
          section_size_offset,
          static_cast<uint32_t>(buffer.offset() - section_body_begin));
      buffer.patch_u32v(num_functions_offset, num_functions);
    }
  }

  // Emit the call-targets section.
  {
    base::MutexGuard mutex(&module->type_feedback.mutex);
    buffer.write_u8(kUnknownSectionCode);
    size_t section_size_offset = buffer.reserve_u32v();
    size_t section_body_begin = buffer.offset();
    buffer.write_string(base::StaticCharVector(kCallTargetsString));

    uint32_t num_functions = 0;
    size_t num_functions_offset = buffer.reserve_u32v();

    std::unordered_map<uint32_t, wasm::FunctionTypeFeedback>& feedback =
        module->type_feedback.feedback_for_function;

    for (uint32_t func_index = module->num_imported_functions;
         func_index < module->functions.size(); func_index++) {
      auto it = feedback.find(func_index);
      if (it == feedback.end()) continue;  // No feedback.

      num_functions++;
      buffer.write_u32v(func_index);

      wasm::FunctionTypeFeedback& feedback_for_function = it->second;
      uint32_t num_hints = 0;
      size_t num_hints_offset = buffer.reserve_u32v();

      for (size_t slot_index = 0;
           slot_index < feedback_for_function.feedback_vector.size();
           slot_index++) {
        if (feedback_for_function.call_targets[slot_index] !=
                FunctionTypeFeedback::kCallIndirect &&
            feedback_for_function.call_targets[slot_index] !=
                FunctionTypeFeedback::kCallRef) {
          continue;  // Direct call, do not emit call targets.
        }
        wasm::CallSiteFeedback& slot =
            feedback_for_function.feedback_vector[slot_index];
        if (slot.num_cases() == 0) continue;
        num_hints++;
        int total_count_at_slot = 0;
        for (int call = 0; call < slot.num_cases(); call++) {
          total_count_at_slot += slot.call_count(call);
        }

        uint32_t offset =
            module->feedback_slots_to_wire_byte_offsets[func_index][slot_index];

        buffer.write_u32v(offset);

        size_t hint_size_offset = buffer.reserve_u32v();
        size_t hint_offset = buffer.offset();

        for (int call = 0; call < slot.num_cases(); call++) {
          buffer.write_u32v(slot.function_index(call));
          buffer.write_u32v(static_cast<uint32_t>(
              std::floor(static_cast<double>(slot.call_count(call)) * 100 /
                         total_count_at_slot)));
        }

        buffer.patch_u32v(hint_size_offset,
                          static_cast<uint32_t>(buffer.offset() - hint_offset));
      }

      buffer.patch_u32v(num_hints_offset, num_hints);
    }

    buffer.patch_u32v(num_functions_offset, num_functions);

    buffer.patch_u32v(
        section_size_offset,
        static_cast<uint32_t>(buffer.offset() - section_body_begin));
  }
}

void WriteCompilationHintsToFile(ZoneBuffer& buffer,
                                 NativeModule* native_module) {
  // Write compilation hints to file.
  uint32_t hash =
      static_cast<uint32_t>(GetWireBytesHash(native_module->wire_bytes()));
  base::EmbeddedVector<char, 48> filename;
  SNPrintF(filename, "compilation-hints-wasm-%08x.wasm-no-header", hash);

  base::OwnedVector<uint8_t> data = base::OwnedCopyOf(buffer);

  if (FILE* file = base::OS::FOpen(filename.begin(), "wb")) {
    PrintF("Emitting compilation hints to file '%s'\n", filename.begin());
    size_t written = fwrite(data.begin(), 1, data.size(), file);
    CHECK_EQ(written, data.size());
    base::Fclose(file);
  }
}

}  // namespace v8::internal::wasm
