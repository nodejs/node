// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-code-specialization.h"

#include "src/assembler-inl.h"
#include "src/base/optional.h"
#include "src/objects-inl.h"
#include "src/source-position-table.h"
#include "src/wasm/decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

uint32_t ExtractDirectCallIndex(wasm::Decoder& decoder, const byte* pc) {
  DCHECK_EQ(static_cast<int>(kExprCallFunction), static_cast<int>(*pc));
  decoder.Reset(pc + 1, pc + 6);
  uint32_t call_idx = decoder.consume_u32v("call index");
  DCHECK(decoder.ok());
  DCHECK_GE(kMaxInt, call_idx);
  return call_idx;
}

namespace {

int AdvanceSourcePositionTableIterator(SourcePositionTableIterator& iterator,
                                       size_t offset_l) {
  DCHECK_GE(kMaxInt, offset_l);
  int offset = static_cast<int>(offset_l);
  DCHECK(!iterator.done());
  int byte_pos;
  do {
    byte_pos = iterator.source_position().ScriptOffset();
    iterator.Advance();
  } while (!iterator.done() && iterator.code_offset() <= offset);
  return byte_pos;
}

class PatchDirectCallsHelper {
 public:
  PatchDirectCallsHelper(NativeModule* native_module, const WasmCode* code)
      : source_pos_it(code->source_positions()), decoder(nullptr, nullptr) {
    uint32_t func_index = code->index();
    WasmSharedModuleData* shared = native_module->shared_module_data();
    func_bytes = shared->module_bytes()->GetChars() +
                 shared->module()->functions[func_index].code.offset();
  }

  SourcePositionTableIterator source_pos_it;
  Decoder decoder;
  const byte* func_bytes;
};

}  // namespace

CodeSpecialization::CodeSpecialization() {}

CodeSpecialization::~CodeSpecialization() {}

void CodeSpecialization::RelocateDirectCalls(NativeModule* native_module) {
  DCHECK_NULL(relocate_direct_calls_module_);
  DCHECK_NOT_NULL(native_module);
  relocate_direct_calls_module_ = native_module;
}

bool CodeSpecialization::ApplyToWholeModule(
    NativeModule* native_module, Handle<WasmModuleObject> module_object,
    ICacheFlushMode icache_flush_mode) {
  DisallowHeapAllocation no_gc;
  WasmSharedModuleData* shared = module_object->shared();
  WasmModule* module = shared->module();
  std::vector<WasmFunction>* wasm_functions = &shared->module()->functions;
  FixedArray* export_wrappers = module_object->export_wrappers();
  DCHECK_EQ(export_wrappers->length(), module->num_exported_functions);

  bool changed = false;
  int func_index = module->num_imported_functions;

  // Patch all wasm functions.
  for (int num_wasm_functions = static_cast<int>(wasm_functions->size());
       func_index < num_wasm_functions; ++func_index) {
    WasmCode* wasm_function = native_module->code(func_index);
    // TODO(clemensh): Get rid of this nullptr check
    if (wasm_function == nullptr ||
        wasm_function->kind() != WasmCode::kFunction) {
      continue;
    }
    changed |= ApplyToWasmCode(wasm_function, icache_flush_mode);
  }

  // Patch all exported functions (JS_TO_WASM_FUNCTION).
  int reloc_mode = 0;
  // Patch CODE_TARGET if we shall relocate direct calls. If we patch direct
  // calls, the instance registered for that (relocate_direct_calls_module_)
  // should match the instance we currently patch (instance).
  if (relocate_direct_calls_module_ != nullptr) {
    DCHECK_EQ(native_module, relocate_direct_calls_module_);
    reloc_mode |= RelocInfo::ModeMask(RelocInfo::JS_TO_WASM_CALL);
  }
  if (!reloc_mode) return changed;
  int wrapper_index = 0;
  for (auto exp : module->export_table) {
    if (exp.kind != kExternalFunction) continue;
    Code* export_wrapper = Code::cast(export_wrappers->get(wrapper_index++));
    if (export_wrapper->kind() != Code::JS_TO_WASM_FUNCTION) continue;
    for (RelocIterator it(export_wrapper, reloc_mode); !it.done(); it.next()) {
      RelocInfo::Mode mode = it.rinfo()->rmode();
      switch (mode) {
        case RelocInfo::JS_TO_WASM_CALL: {
          changed = true;
          Address new_target =
              native_module->GetCallTargetForFunction(exp.index);
          it.rinfo()->set_js_to_wasm_address(new_target, icache_flush_mode);
        } break;
        default:
          UNREACHABLE();
      }
    }
  }
  DCHECK_EQ(module->functions.size(), func_index);
  DCHECK_EQ(export_wrappers->length(), wrapper_index);
  return changed;
}

bool CodeSpecialization::ApplyToWasmCode(wasm::WasmCode* code,
                                         ICacheFlushMode icache_flush_mode) {
  DisallowHeapAllocation no_gc;
  DCHECK_EQ(wasm::WasmCode::kFunction, code->kind());

  bool reloc_direct_calls = relocate_direct_calls_module_ != nullptr;

  int reloc_mode = 0;
  if (reloc_direct_calls) {
    reloc_mode |= RelocInfo::ModeMask(RelocInfo::WASM_CALL);
  }
  if (!reloc_mode) return false;

  base::Optional<PatchDirectCallsHelper> patch_direct_calls_helper;
  bool changed = false;

  NativeModule* native_module = code->native_module();

  RelocIterator it(code->instructions(), code->reloc_info(),
                   code->constant_pool(), reloc_mode);
  for (; !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::WASM_CALL: {
        DCHECK(reloc_direct_calls);
        // Iterate simultaneously over the relocation information and the source
        // position table. For each call in the reloc info, move the source
        // position iterator forward to that position to find the byte offset of
        // the respective call. Then extract the call index from the module wire
        // bytes to find the new compiled function.
        size_t offset = it.rinfo()->pc() - code->instruction_start();
        if (!patch_direct_calls_helper) {
          patch_direct_calls_helper.emplace(relocate_direct_calls_module_,
                                            code);
        }
        int byte_pos = AdvanceSourcePositionTableIterator(
            patch_direct_calls_helper->source_pos_it, offset);
        uint32_t called_func_index = ExtractDirectCallIndex(
            patch_direct_calls_helper->decoder,
            patch_direct_calls_helper->func_bytes + byte_pos);
        const WasmCode* new_code = native_module->code(called_func_index);
        it.rinfo()->set_wasm_call_address(new_code->instruction_start(),
                                          icache_flush_mode);
        changed = true;
      } break;

      default:
        UNREACHABLE();
    }
  }

  return changed;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
