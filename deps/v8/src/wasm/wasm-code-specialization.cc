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
  PatchDirectCallsHelper(WasmInstanceObject* instance, const WasmCode* code)
      : source_pos_it(ByteArray::cast(
            instance->compiled_module()->source_positions()->get(
                static_cast<int>(code->index())))),
        decoder(nullptr, nullptr) {
    uint32_t func_index = code->index();
    WasmCompiledModule* comp_mod = instance->compiled_module();
    func_bytes =
        comp_mod->shared()->module_bytes()->GetChars() +
        comp_mod->shared()->module()->functions[func_index].code.offset();
  }

  PatchDirectCallsHelper(WasmInstanceObject* instance, Code* code)
      : source_pos_it(code->SourcePositionTable()), decoder(nullptr, nullptr) {
    FixedArray* deopt_data = code->deoptimization_data();
    DCHECK_EQ(2, deopt_data->length());
    WasmSharedModuleData* shared = instance->compiled_module()->shared();
    int func_index = Smi::ToInt(deopt_data->get(1));
    func_bytes = shared->module_bytes()->GetChars() +
                 shared->module()->functions[func_index].code.offset();
  }

  SourcePositionTableIterator source_pos_it;
  Decoder decoder;
  const byte* func_bytes;
};

bool IsAtWasmDirectCallTarget(RelocIterator& it) {
  DCHECK(RelocInfo::IsCodeTarget(it.rinfo()->rmode()));
  Code* code = Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
  return code->kind() == Code::WASM_FUNCTION ||
         code->kind() == Code::WASM_TO_JS_FUNCTION ||
         code->kind() == Code::WASM_TO_WASM_FUNCTION ||
         code->kind() == Code::WASM_INTERPRETER_ENTRY ||
         code->builtin_index() == Builtins::kIllegal ||
         code->builtin_index() == Builtins::kWasmCompileLazy;
}

}  // namespace

CodeSpecialization::CodeSpecialization(Isolate* isolate, Zone* zone)
    : isolate_(isolate) {}

CodeSpecialization::~CodeSpecialization() {}

void CodeSpecialization::RelocateWasmContextReferences(Address new_context) {
  DCHECK_NOT_NULL(new_context);
  DCHECK_NULL(new_wasm_context_address);
  new_wasm_context_address = new_context;
}

void CodeSpecialization::PatchTableSize(uint32_t old_size, uint32_t new_size) {
  DCHECK(old_function_table_size == 0 && new_function_table_size == 0);
  old_function_table_size = old_size;
  new_function_table_size = new_size;
}

void CodeSpecialization::RelocateDirectCalls(
    Handle<WasmInstanceObject> instance) {
  DCHECK(relocate_direct_calls_instance.is_null());
  DCHECK(!instance.is_null());
  relocate_direct_calls_instance = instance;
}

void CodeSpecialization::RelocatePointer(Address old_ptr, Address new_ptr) {
  pointers_to_relocate.insert(std::make_pair(old_ptr, new_ptr));
}

bool CodeSpecialization::ApplyToWholeInstance(
    WasmInstanceObject* instance, ICacheFlushMode icache_flush_mode) {
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = instance->compiled_module();
  NativeModule* native_module = compiled_module->GetNativeModule();
  FixedArray* code_table = compiled_module->code_table();
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
    WasmCodeWrapper wrapper;
    if (FLAG_wasm_jit_to_native) {
      const WasmCode* wasm_function = native_module->GetCode(func_index);
      if (wasm_function->kind() != WasmCode::kFunction) {
        continue;
      }
      wrapper = WasmCodeWrapper(wasm_function);
    } else {
      Code* wasm_function = Code::cast(code_table->get(func_index));
      if (wasm_function->kind() != Code::WASM_FUNCTION) continue;
      wrapper = WasmCodeWrapper(handle(wasm_function));
    }
    changed |= ApplyToWasmCode(wrapper, icache_flush_mode);
  }

  // Patch all exported functions (JS_TO_WASM_FUNCTION).
  int reloc_mode = 0;
  // We need to patch WASM_CONTEXT_REFERENCE to put the correct address.
  if (new_wasm_context_address) {
    reloc_mode |= RelocInfo::ModeMask(RelocInfo::WASM_CONTEXT_REFERENCE);
  }
  // Patch CODE_TARGET if we shall relocate direct calls. If we patch direct
  // calls, the instance registered for that (relocate_direct_calls_instance)
  // should match the instance we currently patch (instance).
  if (!relocate_direct_calls_instance.is_null()) {
    DCHECK_EQ(instance, *relocate_direct_calls_instance);
    reloc_mode |=
        RelocInfo::ModeMask(FLAG_wasm_jit_to_native ? RelocInfo::JS_TO_WASM_CALL
                                                    : RelocInfo::CODE_TARGET);
  }
  if (!reloc_mode) return changed;
  int wrapper_index = 0;
  for (auto exp : module->export_table) {
    if (exp.kind != kExternalFunction) continue;
    Code* export_wrapper =
        Code::cast(compiled_module->export_wrappers()->get(wrapper_index));
    DCHECK_EQ(Code::JS_TO_WASM_FUNCTION, export_wrapper->kind());
    for (RelocIterator it(export_wrapper, reloc_mode); !it.done(); it.next()) {
      RelocInfo::Mode mode = it.rinfo()->rmode();
      switch (mode) {
        case RelocInfo::WASM_CONTEXT_REFERENCE:
          it.rinfo()->set_wasm_context_reference(export_wrapper->GetIsolate(),
                                                 new_wasm_context_address,
                                                 icache_flush_mode);
          break;
        case RelocInfo::JS_TO_WASM_CALL: {
          DCHECK(FLAG_wasm_jit_to_native);
          const WasmCode* new_code = native_module->GetCode(exp.index);
          it.rinfo()->set_js_to_wasm_address(
              nullptr, new_code->instructions().start(), SKIP_ICACHE_FLUSH);
        } break;
        case RelocInfo::CODE_TARGET: {
          DCHECK(!FLAG_wasm_jit_to_native);
          // Ignore calls to other builtins like ToNumber.
          if (!IsAtWasmDirectCallTarget(it)) continue;
          Code* new_code = Code::cast(code_table->get(exp.index));
          it.rinfo()->set_target_address(
              new_code->GetIsolate(), new_code->instruction_start(),
              UPDATE_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
        } break;
        default:
          UNREACHABLE();
      }
    }
    changed = true;
    ++wrapper_index;
  }
  DCHECK_EQ(module->functions.size(), func_index);
  DCHECK_EQ(compiled_module->export_wrappers()->length(), wrapper_index);
  return changed;
}

bool CodeSpecialization::ApplyToWasmCode(WasmCodeWrapper code,
                                         ICacheFlushMode icache_flush_mode) {
  DisallowHeapAllocation no_gc;
  if (code.IsCodeObject()) {
    DCHECK_EQ(Code::WASM_FUNCTION, code.GetCode()->kind());
  } else {
    DCHECK_EQ(wasm::WasmCode::kFunction, code.GetWasmCode()->kind());
  }

  bool patch_table_size = old_function_table_size || new_function_table_size;
  bool reloc_direct_calls = !relocate_direct_calls_instance.is_null();
  bool reloc_pointers = pointers_to_relocate.size() > 0;

  int reloc_mode = 0;
  auto add_mode = [&reloc_mode](bool cond, RelocInfo::Mode mode) {
    if (cond) reloc_mode |= RelocInfo::ModeMask(mode);
  };
  add_mode(patch_table_size, RelocInfo::WASM_FUNCTION_TABLE_SIZE_REFERENCE);
  if (code.IsCodeObject()) {
    add_mode(reloc_direct_calls, RelocInfo::CODE_TARGET);
  } else {
    add_mode(reloc_direct_calls, RelocInfo::WASM_CALL);
  }
  add_mode(reloc_pointers, RelocInfo::WASM_GLOBAL_HANDLE);

  base::Optional<PatchDirectCallsHelper> patch_direct_calls_helper;
  bool changed = false;

  NativeModule* native_module =
      code.IsCodeObject() ? nullptr : code.GetWasmCode()->owner();

  RelocIterator it =
      code.IsCodeObject()
          ? RelocIterator(*code.GetCode(), reloc_mode)
          : RelocIterator(code.GetWasmCode()->instructions(),
                          code.GetWasmCode()->reloc_info(),
                          code.GetWasmCode()->constant_pool(), reloc_mode);
  for (; !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::CODE_TARGET: {
        DCHECK(!FLAG_wasm_jit_to_native);
        DCHECK(reloc_direct_calls);
        // Skip everything which is not a wasm call (stack checks, traps, ...).
        if (!IsAtWasmDirectCallTarget(it)) continue;
        // Iterate simultaneously over the relocation information and the source
        // position table. For each call in the reloc info, move the source
        // position iterator forward to that position to find the byte offset of
        // the respective call. Then extract the call index from the module wire
        // bytes to find the new compiled function.
        size_t offset = it.rinfo()->pc() - code.GetCode()->instruction_start();
        if (!patch_direct_calls_helper) {
          patch_direct_calls_helper.emplace(*relocate_direct_calls_instance,
                                            *code.GetCode());
        }
        int byte_pos = AdvanceSourcePositionTableIterator(
            patch_direct_calls_helper->source_pos_it, offset);
        int called_func_index = ExtractDirectCallIndex(
            patch_direct_calls_helper->decoder,
            patch_direct_calls_helper->func_bytes + byte_pos);
        FixedArray* code_table =
            relocate_direct_calls_instance->compiled_module()->code_table();
        Code* new_code = Code::cast(code_table->get(called_func_index));
        it.rinfo()->set_target_address(new_code->GetIsolate(),
                                       new_code->instruction_start(),
                                       UPDATE_WRITE_BARRIER, icache_flush_mode);
        changed = true;
      } break;
      case RelocInfo::WASM_CALL: {
        DCHECK(FLAG_wasm_jit_to_native);
        DCHECK(reloc_direct_calls);
        // Iterate simultaneously over the relocation information and the source
        // position table. For each call in the reloc info, move the source
        // position iterator forward to that position to find the byte offset of
        // the respective call. Then extract the call index from the module wire
        // bytes to find the new compiled function.
        size_t offset =
            it.rinfo()->pc() - code.GetWasmCode()->instructions().start();
        if (!patch_direct_calls_helper) {
          patch_direct_calls_helper.emplace(*relocate_direct_calls_instance,
                                            code.GetWasmCode());
        }
        int byte_pos = AdvanceSourcePositionTableIterator(
            patch_direct_calls_helper->source_pos_it, offset);
        uint32_t called_func_index = ExtractDirectCallIndex(
            patch_direct_calls_helper->decoder,
            patch_direct_calls_helper->func_bytes + byte_pos);
        const WasmCode* new_code = native_module->GetCode(called_func_index);
        it.rinfo()->set_wasm_call_address(
            isolate_, new_code->instructions().start(), icache_flush_mode);
        changed = true;
      } break;
      case RelocInfo::WASM_GLOBAL_HANDLE: {
        DCHECK(reloc_pointers);
        Address old_ptr = it.rinfo()->global_handle();
        if (pointers_to_relocate.count(old_ptr) == 1) {
          Address new_ptr = pointers_to_relocate[old_ptr];
          it.rinfo()->set_global_handle(isolate_, new_ptr, icache_flush_mode);
          changed = true;
        }
      } break;
      case RelocInfo::WASM_FUNCTION_TABLE_SIZE_REFERENCE:
        DCHECK(patch_table_size);
        it.rinfo()->update_wasm_function_table_size_reference(
            isolate_, old_function_table_size, new_function_table_size,
            icache_flush_mode);
        changed = true;
        break;
      default:
        UNREACHABLE();
    }
  }

  return changed;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
