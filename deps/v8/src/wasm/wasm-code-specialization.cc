// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-code-specialization.h"

#include "src/assembler-inl.h"
#include "src/objects-inl.h"
#include "src/source-position-table.h"
#include "src/wasm/decoder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

using namespace v8::internal;
using namespace v8::internal::wasm;

namespace {

int ExtractDirectCallIndex(wasm::Decoder& decoder, const byte* pc) {
  DCHECK_EQ(static_cast<int>(kExprCallFunction), static_cast<int>(*pc));
  decoder.Reset(pc + 1, pc + 6);
  uint32_t call_idx = decoder.consume_u32v("call index");
  DCHECK(decoder.ok());
  DCHECK_GE(kMaxInt, call_idx);
  return static_cast<int>(call_idx);
}

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
  PatchDirectCallsHelper(WasmInstanceObject* instance, Code* code)
      : source_pos_it(code->SourcePositionTable()), decoder(nullptr, nullptr) {
    FixedArray* deopt_data = code->deoptimization_data();
    DCHECK_EQ(2, deopt_data->length());
    WasmCompiledModule* comp_mod = instance->compiled_module();
    int func_index = Smi::cast(deopt_data->get(1))->value();
    func_bytes = comp_mod->module_bytes()->GetChars() +
                 comp_mod->module()->functions[func_index].code_start_offset;
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
         code->kind() == Code::WASM_INTERPRETER_ENTRY ||
         code->builtin_index() == Builtins::kIllegal ||
         code->builtin_index() == Builtins::kWasmCompileLazy;
}

}  // namespace

CodeSpecialization::CodeSpecialization(Isolate* isolate, Zone* zone)
    : objects_to_relocate(isolate->heap(), ZoneAllocationPolicy(zone)) {}

CodeSpecialization::~CodeSpecialization() {}

void CodeSpecialization::RelocateMemoryReferences(Address old_start,
                                                  uint32_t old_size,
                                                  Address new_start,
                                                  uint32_t new_size) {
  DCHECK(old_mem_start == nullptr && old_mem_size == 0 &&
         new_mem_start == nullptr && new_mem_size == 0);
  DCHECK(old_start != new_start || old_size != new_size);
  old_mem_start = old_start;
  old_mem_size = old_size;
  new_mem_start = new_start;
  new_mem_size = new_size;
}

void CodeSpecialization::RelocateGlobals(Address old_start, Address new_start) {
  DCHECK(old_globals_start == 0 && new_globals_start == 0);
  DCHECK(old_start != 0 || new_start != 0);
  old_globals_start = old_start;
  new_globals_start = new_start;
}

void CodeSpecialization::PatchTableSize(uint32_t old_size, uint32_t new_size) {
  DCHECK(old_function_table_size == 0 && new_function_table_size == 0);
  DCHECK(old_size != 0 || new_size != 0);
  old_function_table_size = old_size;
  new_function_table_size = new_size;
}

void CodeSpecialization::RelocateDirectCalls(
    Handle<WasmInstanceObject> instance) {
  DCHECK(relocate_direct_calls_instance.is_null());
  DCHECK(!instance.is_null());
  relocate_direct_calls_instance = instance;
}

void CodeSpecialization::RelocateObject(Handle<Object> old_obj,
                                        Handle<Object> new_obj) {
  DCHECK(!old_obj.is_null() && !new_obj.is_null());
  has_objects_to_relocate = true;
  objects_to_relocate.Set(*old_obj, new_obj);
}

bool CodeSpecialization::ApplyToWholeInstance(
    WasmInstanceObject* instance, ICacheFlushMode icache_flush_mode) {
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = instance->compiled_module();
  FixedArray* code_table = compiled_module->ptr_to_code_table();
  WasmModule* module = compiled_module->module();
  std::vector<WasmFunction>* wasm_functions =
      &compiled_module->module()->functions;
  DCHECK_EQ(wasm_functions->size() +
                compiled_module->module()->num_exported_functions,
            code_table->length());

  bool changed = false;
  int func_index = module->num_imported_functions;

  // Patch all wasm functions.
  for (int num_wasm_functions = static_cast<int>(wasm_functions->size());
       func_index < num_wasm_functions; ++func_index) {
    Code* wasm_function = Code::cast(code_table->get(func_index));
    if (wasm_function->kind() != Code::WASM_FUNCTION) continue;
    changed |= ApplyToWasmCode(wasm_function, icache_flush_mode);
  }

  // Patch all exported functions (if we shall relocate direct calls).
  if (!relocate_direct_calls_instance.is_null()) {
    // If we patch direct calls, the instance registered for that
    // (relocate_direct_calls_instance) should match the instance we currently
    // patch (instance).
    DCHECK_EQ(instance, *relocate_direct_calls_instance);
    for (auto exp : module->export_table) {
      if (exp.kind != kExternalFunction) continue;
      Code* export_wrapper = Code::cast(code_table->get(func_index));
      DCHECK_EQ(Code::JS_TO_WASM_FUNCTION, export_wrapper->kind());
      // There must be exactly one call to WASM_FUNCTION or WASM_TO_JS_FUNCTION.
      for (RelocIterator it(export_wrapper,
                            RelocInfo::ModeMask(RelocInfo::CODE_TARGET));
           ; it.next()) {
        DCHECK(!it.done());
        // Ignore calls to other builtins like ToNumber.
        if (!IsAtWasmDirectCallTarget(it)) continue;
        Code* new_code = Code::cast(code_table->get(exp.index));
        it.rinfo()->set_target_address(new_code->GetIsolate(),
                                       new_code->instruction_start(),
                                       UPDATE_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
        break;
      }
      changed = true;
      func_index++;
    }
    DCHECK_EQ(code_table->length(), func_index);
  }
  return changed;
}

bool CodeSpecialization::ApplyToWasmCode(Code* code,
                                         ICacheFlushMode icache_flush_mode) {
  DisallowHeapAllocation no_gc;
  DCHECK_EQ(Code::WASM_FUNCTION, code->kind());

  bool reloc_mem_addr = old_mem_start != new_mem_start;
  bool reloc_mem_size = old_mem_size != new_mem_size;
  bool reloc_globals = old_globals_start || new_globals_start;
  bool patch_table_size = old_function_table_size || new_function_table_size;
  bool reloc_direct_calls = !relocate_direct_calls_instance.is_null();
  bool reloc_objects = has_objects_to_relocate;

  int reloc_mode = 0;
  auto add_mode = [&reloc_mode](bool cond, RelocInfo::Mode mode) {
    if (cond) reloc_mode |= RelocInfo::ModeMask(mode);
  };
  add_mode(reloc_mem_addr, RelocInfo::WASM_MEMORY_REFERENCE);
  add_mode(reloc_mem_size, RelocInfo::WASM_MEMORY_SIZE_REFERENCE);
  add_mode(reloc_globals, RelocInfo::WASM_GLOBAL_REFERENCE);
  add_mode(patch_table_size, RelocInfo::WASM_FUNCTION_TABLE_SIZE_REFERENCE);
  add_mode(reloc_direct_calls, RelocInfo::CODE_TARGET);
  add_mode(reloc_objects, RelocInfo::EMBEDDED_OBJECT);

  std::unique_ptr<PatchDirectCallsHelper> patch_direct_calls_helper;
  bool changed = false;

  for (RelocIterator it(code, reloc_mode); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::WASM_MEMORY_REFERENCE:
        DCHECK(reloc_mem_addr);
        it.rinfo()->update_wasm_memory_reference(code->GetIsolate(),
                                                 old_mem_start, new_mem_start,
                                                 icache_flush_mode);
        changed = true;
        break;
      case RelocInfo::WASM_MEMORY_SIZE_REFERENCE:
        DCHECK(reloc_mem_size);
        it.rinfo()->update_wasm_memory_size(code->GetIsolate(), old_mem_size,
                                            new_mem_size, icache_flush_mode);
        changed = true;
        break;
      case RelocInfo::WASM_GLOBAL_REFERENCE:
        DCHECK(reloc_globals);
        it.rinfo()->update_wasm_global_reference(
            code->GetIsolate(), old_globals_start, new_globals_start,
            icache_flush_mode);
        changed = true;
        break;
      case RelocInfo::CODE_TARGET: {
        DCHECK(reloc_direct_calls);
        // Skip everything which is not a wasm call (stack checks, traps, ...).
        if (!IsAtWasmDirectCallTarget(it)) continue;
        // Iterate simultaneously over the relocation information and the source
        // position table. For each call in the reloc info, move the source
        // position iterator forward to that position to find the byte offset of
        // the respective call. Then extract the call index from the module wire
        // bytes to find the new compiled function.
        size_t offset = it.rinfo()->pc() - code->instruction_start();
        if (!patch_direct_calls_helper) {
          patch_direct_calls_helper.reset(new PatchDirectCallsHelper(
              *relocate_direct_calls_instance, code));
        }
        int byte_pos = AdvanceSourcePositionTableIterator(
            patch_direct_calls_helper->source_pos_it, offset);
        int called_func_index = ExtractDirectCallIndex(
            patch_direct_calls_helper->decoder,
            patch_direct_calls_helper->func_bytes + byte_pos);
        FixedArray* code_table =
            relocate_direct_calls_instance->compiled_module()
                ->ptr_to_code_table();
        Code* new_code = Code::cast(code_table->get(called_func_index));
        it.rinfo()->set_target_address(new_code->GetIsolate(),
                                       new_code->instruction_start(),
                                       UPDATE_WRITE_BARRIER, icache_flush_mode);
        changed = true;
      } break;
      case RelocInfo::EMBEDDED_OBJECT: {
        DCHECK(reloc_objects);
        Object* old = it.rinfo()->target_object();
        Handle<Object>* new_obj = objects_to_relocate.Find(old);
        if (new_obj) {
          it.rinfo()->set_target_object(HeapObject::cast(**new_obj),
                                        UPDATE_WRITE_BARRIER,
                                        icache_flush_mode);
          changed = true;
        }
      } break;
      case RelocInfo::WASM_FUNCTION_TABLE_SIZE_REFERENCE:
        DCHECK(patch_table_size);
        it.rinfo()->update_wasm_function_table_size_reference(
            code->GetIsolate(), old_function_table_size,
            new_function_table_size, icache_flush_mode);
        changed = true;
        break;
      default:
        UNREACHABLE();
    }
  }

  return changed;
}
