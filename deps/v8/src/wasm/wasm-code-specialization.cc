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
    WasmCompiledModule* comp_mod = native_module->compiled_module();
    func_bytes =
        comp_mod->shared()->module_bytes()->GetChars() +
        comp_mod->shared()->module()->functions[func_index].code.offset();
  }

  SourcePositionTableIterator source_pos_it;
  Decoder decoder;
  const byte* func_bytes;
};

}  // namespace

CodeSpecialization::CodeSpecialization() {}

CodeSpecialization::~CodeSpecialization() {}

void CodeSpecialization::UpdateInstanceReferences(
    Handle<WeakCell> old_weak_instance, Handle<WeakCell> new_weak_instance) {
  DCHECK(!old_weak_instance.is_null());
  DCHECK(!new_weak_instance.is_null());
  old_weak_instance_ = old_weak_instance;
  new_weak_instance_ = new_weak_instance;
}

void CodeSpecialization::RelocateDirectCalls(NativeModule* native_module) {
  DCHECK_NULL(relocate_direct_calls_module_);
  DCHECK_NOT_NULL(native_module);
  relocate_direct_calls_module_ = native_module;
}

bool CodeSpecialization::ApplyToWholeModule(NativeModule* native_module,
                                            ICacheFlushMode icache_flush_mode) {
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = native_module->compiled_module();
  WasmSharedModuleData* shared = compiled_module->shared();
  WasmModule* module = shared->module();
  std::vector<WasmFunction>* wasm_functions = &shared->module()->functions;
  DCHECK_EQ(compiled_module->export_wrappers()->length(),
            shared->module()->num_exported_functions);

  bool changed = false;
  int func_index = module->num_imported_functions;

  // Patch all wasm functions.
  for (int num_wasm_functions = static_cast<int>(wasm_functions->size());
       func_index < num_wasm_functions; ++func_index) {
    WasmCode* wasm_function = native_module->GetCode(func_index);
    // TODO(clemensh): Get rid of this nullptr check
    if (wasm_function == nullptr ||
        wasm_function->kind() != WasmCode::kFunction) {
      continue;
    }
    changed |= ApplyToWasmCode(wasm_function, icache_flush_mode);
  }

  bool patch_wasm_weak_instances =
      !old_weak_instance_.is_identical_to(new_weak_instance_);

  // Patch all exported functions (JS_TO_WASM_FUNCTION).
  int reloc_mode = 0;
  // Patch CODE_TARGET if we shall relocate direct calls. If we patch direct
  // calls, the instance registered for that (relocate_direct_calls_module_)
  // should match the instance we currently patch (instance).
  if (relocate_direct_calls_module_ != nullptr) {
    DCHECK_EQ(native_module, relocate_direct_calls_module_);
    reloc_mode |= RelocInfo::ModeMask(RelocInfo::JS_TO_WASM_CALL);
  }
  // Instance references are simply embedded objects.
  if (patch_wasm_weak_instances) {
    reloc_mode |= RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  }
  if (!reloc_mode) return changed;
  int wrapper_index = 0;
  for (auto exp : module->export_table) {
    if (exp.kind != kExternalFunction) continue;
    Code* export_wrapper =
        Code::cast(compiled_module->export_wrappers()->get(wrapper_index++));
    if (export_wrapper->kind() != Code::JS_TO_WASM_FUNCTION) continue;
    for (RelocIterator it(export_wrapper, reloc_mode); !it.done(); it.next()) {
      RelocInfo::Mode mode = it.rinfo()->rmode();
      switch (mode) {
        case RelocInfo::JS_TO_WASM_CALL: {
          changed = true;
          const WasmCode* new_code =
              native_module->GetIndirectlyCallableCode(exp.index);
          it.rinfo()->set_js_to_wasm_address(new_code->instructions().start(),
                                             icache_flush_mode);
        } break;
        case RelocInfo::EMBEDDED_OBJECT: {
          changed = true;
          const HeapObject* old = it.rinfo()->target_object();
          if (*old_weak_instance_ == old) {
            it.rinfo()->set_target_object(
                *new_weak_instance_, WriteBarrierMode::UPDATE_WRITE_BARRIER,
                icache_flush_mode);
          }
        } break;
        default:
          UNREACHABLE();
      }
    }
  }
  DCHECK_EQ(module->functions.size(), func_index);
  DCHECK_EQ(compiled_module->export_wrappers()->length(), wrapper_index);
  return changed;
}

bool CodeSpecialization::ApplyToWasmCode(wasm::WasmCode* code,
                                         ICacheFlushMode icache_flush_mode) {
  DisallowHeapAllocation no_gc;
  DCHECK_EQ(wasm::WasmCode::kFunction, code->kind());

  bool reloc_direct_calls = relocate_direct_calls_module_ != nullptr;

  int reloc_mode = 0;
  auto add_mode = [&reloc_mode](bool cond, RelocInfo::Mode mode) {
    if (cond) reloc_mode |= RelocInfo::ModeMask(mode);
  };
  add_mode(reloc_direct_calls, RelocInfo::WASM_CALL);

  // Always patch the code table entry address which is used in Liftoff
  // prologue to jump to optimized code if existent.
  reloc_mode |= RelocInfo::ModeMask(RelocInfo::WASM_CODE_TABLE_ENTRY);

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
        size_t offset = it.rinfo()->pc() - code->instructions().start();
        if (!patch_direct_calls_helper) {
          patch_direct_calls_helper.emplace(relocate_direct_calls_module_,
                                            code);
        }
        int byte_pos = AdvanceSourcePositionTableIterator(
            patch_direct_calls_helper->source_pos_it, offset);
        uint32_t called_func_index = ExtractDirectCallIndex(
            patch_direct_calls_helper->decoder,
            patch_direct_calls_helper->func_bytes + byte_pos);
        const WasmCode* new_code = native_module->GetCode(called_func_index);
        it.rinfo()->set_wasm_call_address(new_code->instructions().start(),
                                          icache_flush_mode);
        changed = true;
      } break;
      case RelocInfo::WASM_CODE_TABLE_ENTRY: {
        DCHECK(FLAG_wasm_tier_up);
        WasmCode* const* code_table_entry =
            native_module->code_table().data() + code->index();
        it.rinfo()->set_wasm_code_table_entry(
            const_cast<Address>(
                reinterpret_cast<byte const*>(code_table_entry)),
            icache_flush_mode);
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
