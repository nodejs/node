// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-objects.h"
#include "src/utils.h"

#include "src/assembler-inl.h"
#include "src/base/iterator.h"
#include "src/code-factory.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/debug-interface.h"
#include "src/objects-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-memory.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-text.h"

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_wasm_instances) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_IFT(...)              \
  do {                              \
    if (false) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {

// Import a few often used types from the wasm namespace.
using WasmFunction = wasm::WasmFunction;
using WasmModule = wasm::WasmModule;

namespace {

// Manages the natively-allocated memory for a WasmInstanceObject. Since
// an instance finalizer is not guaranteed to run upon isolate shutdown,
// we must use a Managed<WasmInstanceNativeAllocations> to guarantee
// it is freed.
// Native allocations are the signature ids and targets for indirect call
// targets, as well as the call targets for imported functions.
class WasmInstanceNativeAllocations {
 public:
// Helper macro to set an internal field and the corresponding field
// on an instance.
#define SET(instance, field, value) \
  {                                 \
    auto v = value;                 \
    this->field##_ = v;             \
    instance->set_##field(v);       \
  }

  // Allocates initial native storage for a given instance.
  WasmInstanceNativeAllocations(Handle<WasmInstanceObject> instance,
                                size_t num_imported_functions,
                                size_t num_imported_mutable_globals) {
    SET(instance, imported_function_targets,
        reinterpret_cast<Address*>(
            calloc(num_imported_functions, sizeof(Address))));
    SET(instance, imported_mutable_globals,
        reinterpret_cast<Address*>(
            calloc(num_imported_mutable_globals, sizeof(Address))));
  }
  ~WasmInstanceNativeAllocations() {
    ::free(indirect_function_table_sig_ids_);
    indirect_function_table_sig_ids_ = nullptr;
    ::free(indirect_function_table_targets_);
    indirect_function_table_targets_ = nullptr;
    ::free(imported_function_targets_);
    imported_function_targets_ = nullptr;
    ::free(imported_mutable_globals_);
    imported_mutable_globals_ = nullptr;
  }
  // Resizes the indirect function table.
  void resize_indirect_function_table(Isolate* isolate,
                                      Handle<WasmInstanceObject> instance,
                                      uint32_t new_size) {
    uint32_t old_size = instance->indirect_function_table_size();
    void* new_sig_ids = nullptr;
    void* new_targets = nullptr;
    Handle<FixedArray> new_instances;
    if (indirect_function_table_sig_ids_) {
      // Reallocate the old storage.
      new_sig_ids = realloc(indirect_function_table_sig_ids_,
                            new_size * sizeof(uint32_t));
      new_targets =
          realloc(indirect_function_table_targets_, new_size * sizeof(Address));

      Handle<FixedArray> old(instance->indirect_function_table_instances(),
                             isolate);
      new_instances = isolate->factory()->CopyFixedArrayAndGrow(
          old, static_cast<int>(new_size - old_size));
    } else {
      // Allocate new storage.
      new_sig_ids = malloc(new_size * sizeof(uint32_t));
      new_targets = malloc(new_size * sizeof(Address));
      new_instances =
          isolate->factory()->NewFixedArray(static_cast<int>(new_size));
    }
    // Initialize new entries.
    instance->set_indirect_function_table_size(new_size);
    SET(instance, indirect_function_table_sig_ids,
        reinterpret_cast<uint32_t*>(new_sig_ids));
    SET(instance, indirect_function_table_targets,
        reinterpret_cast<Address*>(new_targets));

    instance->set_indirect_function_table_instances(*new_instances);
    for (uint32_t j = old_size; j < new_size; j++) {
      IndirectFunctionTableEntry(instance, static_cast<int>(j)).clear();
    }
  }
  uint32_t* indirect_function_table_sig_ids_ = nullptr;
  Address* indirect_function_table_targets_ = nullptr;
  Address* imported_function_targets_ = nullptr;
  Address* imported_mutable_globals_ = nullptr;
#undef SET
};

size_t EstimateNativeAllocationsSize(const WasmModule* module) {
  size_t estimate = sizeof(WasmInstanceNativeAllocations) +
                    (1 * kPointerSize * module->num_imported_mutable_globals) +
                    (2 * kPointerSize * module->num_imported_functions);
  for (auto& table : module->tables) {
    estimate += 3 * kPointerSize * table.initial_size;
  }
  return estimate;
}

WasmInstanceNativeAllocations* GetNativeAllocations(
    WasmInstanceObject* instance) {
  return reinterpret_cast<Managed<WasmInstanceNativeAllocations>*>(
             instance->managed_native_allocations())
      ->raw();
}

#ifdef DEBUG
bool IsBreakablePosition(wasm::NativeModule* native_module, int func_index,
                         int offset_in_func) {
  AccountingAllocator alloc;
  Zone tmp(&alloc, ZONE_NAME);
  wasm::BodyLocalDecls locals(&tmp);
  const byte* module_start = native_module->wire_bytes().start();
  const WasmFunction& func = native_module->module()->functions[func_index];
  wasm::BytecodeIterator iterator(module_start + func.code.offset(),
                                  module_start + func.code.end_offset(),
                                  &locals);
  DCHECK_LT(0, locals.encoded_size);
  for (uint32_t offset : iterator.offsets()) {
    if (offset > static_cast<uint32_t>(offset_in_func)) break;
    if (offset == static_cast<uint32_t>(offset_in_func)) return true;
  }
  return false;
}
#endif  // DEBUG

enum DispatchTableElements : int {
  kDispatchTableInstanceOffset,
  kDispatchTableIndexOffset,
  kDispatchTableFunctionTableOffset,
  // Marker:
  kDispatchTableNumElements
};

}  // namespace

// static
Handle<WasmModuleObject> WasmModuleObject::New(
    Isolate* isolate, const wasm::WasmFeatures& enabled,
    std::shared_ptr<const wasm::WasmModule> shared_module, wasm::ModuleEnv& env,
    OwnedVector<const uint8_t> wire_bytes, Handle<Script> script,
    Handle<ByteArray> asm_js_offset_table) {
  DCHECK_EQ(shared_module.get(), env.module);

  // Create a new {NativeModule} first.
  size_t native_memory_estimate =
      isolate->wasm_engine()->code_manager()->EstimateNativeModuleSize(
          env.module);
  auto native_module = isolate->wasm_engine()->code_manager()->NewNativeModule(
      isolate, enabled, native_memory_estimate,
      wasm::NativeModule::kCanAllocateMoreMemory, std::move(shared_module),
      env);
  native_module->set_wire_bytes(std::move(wire_bytes));
  native_module->SetRuntimeStubs(isolate);

  // Delegate to the shared {WasmModuleObject::New} allocator.
  Handle<WasmModuleObject> module_object =
      New(isolate, std::move(native_module), script);
  if (!asm_js_offset_table.is_null()) {
    module_object->set_asm_js_offset_table(*asm_js_offset_table);
  }
  return module_object;
}

// static
Handle<WasmModuleObject> WasmModuleObject::New(
    Isolate* isolate, std::shared_ptr<wasm::NativeModule> native_module,
    Handle<Script> script) {
  int export_wrapper_size =
      static_cast<int>(native_module->module()->num_exported_functions);
  Handle<FixedArray> export_wrappers =
      isolate->factory()->NewFixedArray(export_wrapper_size, TENURED);

  // Use the given shared {NativeModule}, but increase its reference count by
  // allocating a new {Managed<T>} that the {WasmModuleObject} references.
  size_t native_memory_estimate =
      isolate->wasm_engine()->code_manager()->EstimateNativeModuleSize(
          native_module->module());
  size_t memory_estimate =
      EstimateWasmModuleSize(native_module->module()) + native_memory_estimate;
  Handle<Managed<wasm::NativeModule>> managed_native_module =
      Managed<wasm::NativeModule>::FromSharedPtr(isolate, memory_estimate,
                                                 std::move(native_module));

  Handle<WasmModuleObject> module_object = Handle<WasmModuleObject>::cast(
      isolate->factory()->NewJSObject(isolate->wasm_module_constructor()));
  module_object->set_export_wrappers(*export_wrappers);
  if (script->type() == Script::TYPE_WASM) {
    script->set_wasm_module_object(*module_object);
  }
  module_object->set_script(*script);
  module_object->set_weak_instance_list(
      ReadOnlyRoots(isolate).empty_weak_array_list());
  module_object->set_managed_native_module(*managed_native_module);
  return module_object;
}

bool WasmModuleObject::SetBreakPoint(Handle<WasmModuleObject> module_object,
                                     int* position,
                                     Handle<BreakPoint> break_point) {
  Isolate* isolate = module_object->GetIsolate();

  // Find the function for this breakpoint.
  int func_index = module_object->GetContainingFunction(*position);
  if (func_index < 0) return false;
  const WasmFunction& func = module_object->module()->functions[func_index];
  int offset_in_func = *position - func.code.offset();

  // According to the current design, we should only be called with valid
  // breakable positions.
  DCHECK(IsBreakablePosition(module_object->native_module(), func_index,
                             offset_in_func));

  // Insert new break point into break_positions of module object.
  WasmModuleObject::AddBreakpoint(module_object, *position, break_point);

  // Iterate over all instances of this module and tell them to set this new
  // breakpoint. We do this using the weak list of all instances.
  Handle<WeakArrayList> weak_instance_list(module_object->weak_instance_list(),
                                           isolate);
  for (int i = 0; i < weak_instance_list->length(); ++i) {
    MaybeObject* maybe_instance = weak_instance_list->Get(i);
    if (maybe_instance->IsWeak()) {
      Handle<WasmInstanceObject> instance(
          WasmInstanceObject::cast(maybe_instance->GetHeapObjectAssumeWeak()),
          isolate);
      Handle<WasmDebugInfo> debug_info =
          WasmInstanceObject::GetOrCreateDebugInfo(instance);
      WasmDebugInfo::SetBreakpoint(debug_info, func_index, offset_in_func);
    }
  }

  return true;
}

namespace {

int GetBreakpointPos(Isolate* isolate, Object* break_point_info_or_undef) {
  if (break_point_info_or_undef->IsUndefined(isolate)) return kMaxInt;
  return BreakPointInfo::cast(break_point_info_or_undef)->source_position();
}

int FindBreakpointInfoInsertPos(Isolate* isolate,
                                Handle<FixedArray> breakpoint_infos,
                                int position) {
  // Find insert location via binary search, taking care of undefined values on
  // the right. Position is always greater than zero.
  DCHECK_LT(0, position);

  int left = 0;                            // inclusive
  int right = breakpoint_infos->length();  // exclusive
  while (right - left > 1) {
    int mid = left + (right - left) / 2;
    Object* mid_obj = breakpoint_infos->get(mid);
    if (GetBreakpointPos(isolate, mid_obj) <= position) {
      left = mid;
    } else {
      right = mid;
    }
  }

  int left_pos = GetBreakpointPos(isolate, breakpoint_infos->get(left));
  return left_pos < position ? left + 1 : left;
}

}  // namespace

void WasmModuleObject::AddBreakpoint(Handle<WasmModuleObject> module_object,
                                     int position,
                                     Handle<BreakPoint> break_point) {
  Isolate* isolate = module_object->GetIsolate();
  Handle<FixedArray> breakpoint_infos;
  if (module_object->has_breakpoint_infos()) {
    breakpoint_infos = handle(module_object->breakpoint_infos(), isolate);
  } else {
    breakpoint_infos = isolate->factory()->NewFixedArray(4, TENURED);
    module_object->set_breakpoint_infos(*breakpoint_infos);
  }

  int insert_pos =
      FindBreakpointInfoInsertPos(isolate, breakpoint_infos, position);

  // If a BreakPointInfo object already exists for this position, add the new
  // breakpoint object and return.
  if (insert_pos < breakpoint_infos->length() &&
      GetBreakpointPos(isolate, breakpoint_infos->get(insert_pos)) ==
          position) {
    Handle<BreakPointInfo> old_info(
        BreakPointInfo::cast(breakpoint_infos->get(insert_pos)), isolate);
    BreakPointInfo::SetBreakPoint(isolate, old_info, break_point);
    return;
  }

  // Enlarge break positions array if necessary.
  bool need_realloc = !breakpoint_infos->get(breakpoint_infos->length() - 1)
                           ->IsUndefined(isolate);
  Handle<FixedArray> new_breakpoint_infos = breakpoint_infos;
  if (need_realloc) {
    new_breakpoint_infos = isolate->factory()->NewFixedArray(
        2 * breakpoint_infos->length(), TENURED);
    module_object->set_breakpoint_infos(*new_breakpoint_infos);
    // Copy over the entries [0, insert_pos).
    for (int i = 0; i < insert_pos; ++i)
      new_breakpoint_infos->set(i, breakpoint_infos->get(i));
  }

  // Move elements [insert_pos, ...] up by one.
  for (int i = breakpoint_infos->length() - 1; i >= insert_pos; --i) {
    Object* entry = breakpoint_infos->get(i);
    if (entry->IsUndefined(isolate)) continue;
    new_breakpoint_infos->set(i + 1, entry);
  }

  // Generate new BreakpointInfo.
  Handle<BreakPointInfo> breakpoint_info =
      isolate->factory()->NewBreakPointInfo(position);
  BreakPointInfo::SetBreakPoint(isolate, breakpoint_info, break_point);

  // Now insert new position at insert_pos.
  new_breakpoint_infos->set(insert_pos, *breakpoint_info);
}

void WasmModuleObject::SetBreakpointsOnNewInstance(
    Handle<WasmModuleObject> module_object,
    Handle<WasmInstanceObject> instance) {
  if (!module_object->has_breakpoint_infos()) return;
  Isolate* isolate = module_object->GetIsolate();
  Handle<WasmDebugInfo> debug_info =
      WasmInstanceObject::GetOrCreateDebugInfo(instance);

  Handle<FixedArray> breakpoint_infos(module_object->breakpoint_infos(),
                                      isolate);
  // If the array exists, it should not be empty.
  DCHECK_LT(0, breakpoint_infos->length());

  for (int i = 0, e = breakpoint_infos->length(); i < e; ++i) {
    Handle<Object> obj(breakpoint_infos->get(i), isolate);
    if (obj->IsUndefined(isolate)) {
      for (; i < e; ++i) {
        DCHECK(breakpoint_infos->get(i)->IsUndefined(isolate));
      }
      break;
    }
    Handle<BreakPointInfo> breakpoint_info = Handle<BreakPointInfo>::cast(obj);
    int position = breakpoint_info->source_position();

    // Find the function for this breakpoint, and set the breakpoint.
    int func_index = module_object->GetContainingFunction(position);
    DCHECK_LE(0, func_index);
    const WasmFunction& func = module_object->module()->functions[func_index];
    int offset_in_func = position - func.code.offset();
    WasmDebugInfo::SetBreakpoint(debug_info, func_index, offset_in_func);
  }
}

namespace {

enum AsmJsOffsetTableEntryLayout {
  kOTEByteOffset,
  kOTECallPosition,
  kOTENumberConvPosition,
  kOTESize
};

Handle<ByteArray> GetDecodedAsmJsOffsetTable(
    Handle<WasmModuleObject> module_object, Isolate* isolate) {
  DCHECK(module_object->is_asm_js());
  Handle<ByteArray> offset_table(module_object->asm_js_offset_table(), isolate);

  // The last byte in the asm_js_offset_tables ByteArray tells whether it is
  // still encoded (0) or decoded (1).
  enum AsmJsTableType : int { Encoded = 0, Decoded = 1 };
  int table_type = offset_table->get(offset_table->length() - 1);
  DCHECK(table_type == Encoded || table_type == Decoded);
  if (table_type == Decoded) return offset_table;

  wasm::AsmJsOffsetsResult asm_offsets;
  {
    DisallowHeapAllocation no_gc;
    byte* bytes_start = offset_table->GetDataStartAddress();
    byte* bytes_end = reinterpret_cast<byte*>(
        reinterpret_cast<Address>(bytes_start) + offset_table->length() - 1);
    asm_offsets = wasm::DecodeAsmJsOffsets(bytes_start, bytes_end);
  }
  // Wasm bytes must be valid and must contain asm.js offset table.
  DCHECK(asm_offsets.ok());
  DCHECK_GE(kMaxInt, asm_offsets.val.size());
  int num_functions = static_cast<int>(asm_offsets.val.size());
  int num_imported_functions =
      static_cast<int>(module_object->module()->num_imported_functions);
  DCHECK_EQ(module_object->module()->functions.size(),
            static_cast<size_t>(num_functions) + num_imported_functions);
  int num_entries = 0;
  for (int func = 0; func < num_functions; ++func) {
    size_t new_size = asm_offsets.val[func].size();
    DCHECK_LE(new_size, static_cast<size_t>(kMaxInt) - num_entries);
    num_entries += static_cast<int>(new_size);
  }
  // One byte to encode that this is a decoded table.
  DCHECK_GE(kMaxInt,
            1 + static_cast<uint64_t>(num_entries) * kOTESize * kIntSize);
  int total_size = 1 + num_entries * kOTESize * kIntSize;
  Handle<ByteArray> decoded_table =
      isolate->factory()->NewByteArray(total_size, TENURED);
  decoded_table->set(total_size - 1, AsmJsTableType::Decoded);
  module_object->set_asm_js_offset_table(*decoded_table);

  int idx = 0;
  const std::vector<WasmFunction>& wasm_funs =
      module_object->module()->functions;
  for (int func = 0; func < num_functions; ++func) {
    std::vector<wasm::AsmJsOffsetEntry>& func_asm_offsets =
        asm_offsets.val[func];
    if (func_asm_offsets.empty()) continue;
    int func_offset = wasm_funs[num_imported_functions + func].code.offset();
    for (wasm::AsmJsOffsetEntry& e : func_asm_offsets) {
      // Byte offsets must be strictly monotonously increasing:
      DCHECK_IMPLIES(idx > 0, func_offset + e.byte_offset >
                                  decoded_table->get_int(idx - kOTESize));
      decoded_table->set_int(idx + kOTEByteOffset, func_offset + e.byte_offset);
      decoded_table->set_int(idx + kOTECallPosition, e.source_position_call);
      decoded_table->set_int(idx + kOTENumberConvPosition,
                             e.source_position_number_conversion);
      idx += kOTESize;
    }
  }
  DCHECK_EQ(total_size, idx * kIntSize + 1);
  return decoded_table;
}

}  // namespace

int WasmModuleObject::GetSourcePosition(Handle<WasmModuleObject> module_object,
                                        uint32_t func_index,
                                        uint32_t byte_offset,
                                        bool is_at_number_conversion) {
  Isolate* isolate = module_object->GetIsolate();
  const WasmModule* module = module_object->module();

  if (module->origin != wasm::kAsmJsOrigin) {
    // for non-asm.js modules, we just add the function's start offset
    // to make a module-relative position.
    return byte_offset + module_object->GetFunctionOffset(func_index);
  }

  // asm.js modules have an additional offset table that must be searched.
  Handle<ByteArray> offset_table =
      GetDecodedAsmJsOffsetTable(module_object, isolate);

  DCHECK_LT(func_index, module->functions.size());
  uint32_t func_code_offset = module->functions[func_index].code.offset();
  uint32_t total_offset = func_code_offset + byte_offset;

  // Binary search for the total byte offset.
  int left = 0;                                              // inclusive
  int right = offset_table->length() / kIntSize / kOTESize;  // exclusive
  DCHECK_LT(left, right);
  while (right - left > 1) {
    int mid = left + (right - left) / 2;
    int mid_entry = offset_table->get_int(kOTESize * mid);
    DCHECK_GE(kMaxInt, mid_entry);
    if (static_cast<uint32_t>(mid_entry) <= total_offset) {
      left = mid;
    } else {
      right = mid;
    }
  }
  // There should be an entry for each position that could show up on the stack
  // trace:
  DCHECK_EQ(total_offset, offset_table->get_int(kOTESize * left));
  int idx = is_at_number_conversion ? kOTENumberConvPosition : kOTECallPosition;
  return offset_table->get_int(kOTESize * left + idx);
}

v8::debug::WasmDisassembly WasmModuleObject::DisassembleFunction(
    int func_index) {
  DisallowHeapAllocation no_gc;

  if (func_index < 0 ||
      static_cast<uint32_t>(func_index) >= module()->functions.size())
    return {};

  Vector<const byte> wire_bytes = native_module()->wire_bytes();

  std::ostringstream disassembly_os;
  v8::debug::WasmDisassembly::OffsetTable offset_table;

  PrintWasmText(module(), wire_bytes, static_cast<uint32_t>(func_index),
                disassembly_os, &offset_table);

  return {disassembly_os.str(), std::move(offset_table)};
}

bool WasmModuleObject::GetPossibleBreakpoints(
    const v8::debug::Location& start, const v8::debug::Location& end,
    std::vector<v8::debug::BreakLocation>* locations) {
  DisallowHeapAllocation no_gc;

  const std::vector<WasmFunction>& functions = module()->functions;
  if (start.GetLineNumber() < 0 || start.GetColumnNumber() < 0 ||
      (!end.IsEmpty() &&
       (end.GetLineNumber() < 0 || end.GetColumnNumber() < 0)))
    return false;

  // start_func_index, start_offset and end_func_index is inclusive.
  // end_offset is exclusive.
  // start_offset and end_offset are module-relative byte offsets.
  uint32_t start_func_index = start.GetLineNumber();
  if (start_func_index >= functions.size()) return false;
  int start_func_len = functions[start_func_index].code.length();
  if (start.GetColumnNumber() > start_func_len) return false;
  uint32_t start_offset =
      functions[start_func_index].code.offset() + start.GetColumnNumber();
  uint32_t end_func_index;
  uint32_t end_offset;
  if (end.IsEmpty()) {
    // Default: everything till the end of the Script.
    end_func_index = static_cast<uint32_t>(functions.size() - 1);
    end_offset = functions[end_func_index].code.end_offset();
  } else {
    // If end is specified: Use it and check for valid input.
    end_func_index = static_cast<uint32_t>(end.GetLineNumber());

    // Special case: Stop before the start of the next function. Change to: Stop
    // at the end of the function before, such that we don't disassemble the
    // next function also.
    if (end.GetColumnNumber() == 0 && end_func_index > 0) {
      --end_func_index;
      end_offset = functions[end_func_index].code.end_offset();
    } else {
      if (end_func_index >= functions.size()) return false;
      end_offset =
          functions[end_func_index].code.offset() + end.GetColumnNumber();
      if (end_offset > functions[end_func_index].code.end_offset())
        return false;
    }
  }

  AccountingAllocator alloc;
  Zone tmp(&alloc, ZONE_NAME);
  const byte* module_start = native_module()->wire_bytes().start();

  for (uint32_t func_idx = start_func_index; func_idx <= end_func_index;
       ++func_idx) {
    const WasmFunction& func = functions[func_idx];
    if (func.code.length() == 0) continue;

    wasm::BodyLocalDecls locals(&tmp);
    wasm::BytecodeIterator iterator(module_start + func.code.offset(),
                                    module_start + func.code.end_offset(),
                                    &locals);
    DCHECK_LT(0u, locals.encoded_size);
    for (uint32_t offset : iterator.offsets()) {
      uint32_t total_offset = func.code.offset() + offset;
      if (total_offset >= end_offset) {
        DCHECK_EQ(end_func_index, func_idx);
        break;
      }
      if (total_offset < start_offset) continue;
      locations->emplace_back(func_idx, offset, debug::kCommonBreakLocation);
    }
  }
  return true;
}

MaybeHandle<FixedArray> WasmModuleObject::CheckBreakPoints(
    Isolate* isolate, Handle<WasmModuleObject> module_object, int position) {
  if (!module_object->has_breakpoint_infos()) return {};

  Handle<FixedArray> breakpoint_infos(module_object->breakpoint_infos(),
                                      isolate);
  int insert_pos =
      FindBreakpointInfoInsertPos(isolate, breakpoint_infos, position);
  if (insert_pos >= breakpoint_infos->length()) return {};

  Handle<Object> maybe_breakpoint_info(breakpoint_infos->get(insert_pos),
                                       isolate);
  if (maybe_breakpoint_info->IsUndefined(isolate)) return {};
  Handle<BreakPointInfo> breakpoint_info =
      Handle<BreakPointInfo>::cast(maybe_breakpoint_info);
  if (breakpoint_info->source_position() != position) return {};

  // There is no support for conditional break points. Just assume that every
  // break point always hits.
  Handle<Object> break_points(breakpoint_info->break_points(), isolate);
  if (break_points->IsFixedArray()) {
    return Handle<FixedArray>::cast(break_points);
  }
  Handle<FixedArray> break_points_hit = isolate->factory()->NewFixedArray(1);
  break_points_hit->set(0, *break_points);
  return break_points_hit;
}

MaybeHandle<String> WasmModuleObject::ExtractUtf8StringFromModuleBytes(
    Isolate* isolate, Handle<WasmModuleObject> module_object,
    wasm::WireBytesRef ref) {
  // TODO(wasm): cache strings from modules if it's a performance win.
  Vector<const uint8_t> wire_bytes =
      module_object->native_module()->wire_bytes();
  return ExtractUtf8StringFromModuleBytes(isolate, wire_bytes, ref);
}

MaybeHandle<String> WasmModuleObject::ExtractUtf8StringFromModuleBytes(
    Isolate* isolate, Vector<const uint8_t> wire_bytes,
    wasm::WireBytesRef ref) {
  Vector<const uint8_t> name_vec = wire_bytes + ref.offset();
  name_vec.Truncate(ref.length());
  // UTF8 validation happens at decode time.
  DCHECK(unibrow::Utf8::ValidateEncoding(name_vec.start(), name_vec.length()));
  return isolate->factory()->NewStringFromUtf8(
      Vector<const char>::cast(name_vec));
}

MaybeHandle<String> WasmModuleObject::GetModuleNameOrNull(
    Isolate* isolate, Handle<WasmModuleObject> module_object) {
  const WasmModule* module = module_object->module();
  if (!module->name.is_set()) return {};
  return ExtractUtf8StringFromModuleBytes(isolate, module_object, module->name);
}

MaybeHandle<String> WasmModuleObject::GetFunctionNameOrNull(
    Isolate* isolate, Handle<WasmModuleObject> module_object,
    uint32_t func_index) {
  DCHECK_LT(func_index, module_object->module()->functions.size());
  wasm::WireBytesRef name = module_object->module()->LookupFunctionName(
      wasm::ModuleWireBytes(module_object->native_module()->wire_bytes()),
      func_index);
  if (!name.is_set()) return {};
  return ExtractUtf8StringFromModuleBytes(isolate, module_object, name);
}

Handle<String> WasmModuleObject::GetFunctionName(
    Isolate* isolate, Handle<WasmModuleObject> module_object,
    uint32_t func_index) {
  MaybeHandle<String> name =
      GetFunctionNameOrNull(isolate, module_object, func_index);
  if (!name.is_null()) return name.ToHandleChecked();
  EmbeddedVector<char, 32> buffer;
  int length = SNPrintF(buffer, "wasm-function[%u]", func_index);
  return isolate->factory()
      ->NewStringFromOneByte(Vector<uint8_t>::cast(buffer.SubVector(0, length)))
      .ToHandleChecked();
}

Vector<const uint8_t> WasmModuleObject::GetRawFunctionName(
    uint32_t func_index) {
  DCHECK_GT(module()->functions.size(), func_index);
  wasm::ModuleWireBytes wire_bytes(native_module()->wire_bytes());
  wasm::WireBytesRef name_ref =
      module()->LookupFunctionName(wire_bytes, func_index);
  wasm::WasmName name = wire_bytes.GetNameOrNull(name_ref);
  return Vector<const uint8_t>::cast(name);
}

int WasmModuleObject::GetFunctionOffset(uint32_t func_index) {
  const std::vector<WasmFunction>& functions = module()->functions;
  if (static_cast<uint32_t>(func_index) >= functions.size()) return -1;
  DCHECK_GE(kMaxInt, functions[func_index].code.offset());
  return static_cast<int>(functions[func_index].code.offset());
}

int WasmModuleObject::GetContainingFunction(uint32_t byte_offset) {
  const std::vector<WasmFunction>& functions = module()->functions;

  // Binary search for a function containing the given position.
  int left = 0;                                    // inclusive
  int right = static_cast<int>(functions.size());  // exclusive
  if (right == 0) return false;
  while (right - left > 1) {
    int mid = left + (right - left) / 2;
    if (functions[mid].code.offset() <= byte_offset) {
      left = mid;
    } else {
      right = mid;
    }
  }
  // If the found function does not contains the given position, return -1.
  const WasmFunction& func = functions[left];
  if (byte_offset < func.code.offset() ||
      byte_offset >= func.code.end_offset()) {
    return -1;
  }

  return left;
}

bool WasmModuleObject::GetPositionInfo(uint32_t position,
                                       Script::PositionInfo* info) {
  if (script()->source_mapping_url()->IsString()) {
    if (module()->functions.size() == 0) return false;
    info->line = 0;
    info->column = position;
    info->line_start = module()->functions[0].code.offset();
    info->line_end = module()->functions.back().code.end_offset();
    return true;
  }
  int func_index = GetContainingFunction(position);
  if (func_index < 0) return false;

  const WasmFunction& function = module()->functions[func_index];

  info->line = func_index;
  info->column = position - function.code.offset();
  info->line_start = function.code.offset();
  info->line_end = function.code.end_offset();
  return true;
}

Handle<WasmTableObject> WasmTableObject::New(Isolate* isolate, uint32_t initial,
                                             int64_t maximum,
                                             Handle<FixedArray>* js_functions) {
  Handle<JSFunction> table_ctor(
      isolate->native_context()->wasm_table_constructor(), isolate);
  auto table_obj = Handle<WasmTableObject>::cast(
      isolate->factory()->NewJSObject(table_ctor));

  *js_functions = isolate->factory()->NewFixedArray(initial);
  Object* null = ReadOnlyRoots(isolate).null_value();
  for (int i = 0; i < static_cast<int>(initial); ++i) {
    (*js_functions)->set(i, null);
  }
  table_obj->set_functions(**js_functions);
  DCHECK_EQ(maximum, static_cast<int>(maximum));
  Handle<Object> max = isolate->factory()->NewNumber(maximum);
  table_obj->set_maximum_length(*max);

  table_obj->set_dispatch_tables(ReadOnlyRoots(isolate).empty_fixed_array());
  return Handle<WasmTableObject>::cast(table_obj);
}

void WasmTableObject::AddDispatchTable(Isolate* isolate,
                                       Handle<WasmTableObject> table_obj,
                                       Handle<WasmInstanceObject> instance,
                                       int table_index) {
  Handle<FixedArray> dispatch_tables(table_obj->dispatch_tables(), isolate);
  int old_length = dispatch_tables->length();
  DCHECK_EQ(0, old_length % kDispatchTableNumElements);

  if (instance.is_null()) return;
  // TODO(titzer): use weak cells here to avoid leaking instances.

  // Grow the dispatch table and add a new entry at the end.
  Handle<FixedArray> new_dispatch_tables =
      isolate->factory()->CopyFixedArrayAndGrow(dispatch_tables,
                                                kDispatchTableNumElements);

  new_dispatch_tables->set(old_length + kDispatchTableInstanceOffset,
                           *instance);
  new_dispatch_tables->set(old_length + kDispatchTableIndexOffset,
                           Smi::FromInt(table_index));

  table_obj->set_dispatch_tables(*new_dispatch_tables);
}

void WasmTableObject::Grow(Isolate* isolate, uint32_t count) {
  if (count == 0) return;  // Degenerate case: nothing to do.

  Handle<FixedArray> dispatch_tables(this->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);
  uint32_t old_size = functions()->length();

  // Tables are stored in the instance object, no code patching is
  // necessary. We simply have to grow the raw tables in each instance
  // that has imported this table.

  // TODO(titzer): replace the dispatch table with a weak list of all
  // the instances that import a given table.
  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    Handle<WasmInstanceObject> instance(
        WasmInstanceObject::cast(dispatch_tables->get(i)), isolate);
    DCHECK_EQ(old_size, instance->indirect_function_table_size());
    uint32_t new_size = old_size + count;
    WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(instance,
                                                                   new_size);
  }
}

void WasmTableObject::Set(Isolate* isolate, Handle<WasmTableObject> table,
                          int32_t table_index, Handle<JSFunction> function) {
  Handle<FixedArray> array(table->functions(), isolate);
  if (function.is_null()) {
    ClearDispatchTables(isolate, table, table_index);  // Degenerate case.
    array->set(table_index, ReadOnlyRoots(isolate).null_value());
    return;
  }

  // TODO(titzer): Change this to MaybeHandle<WasmExportedFunction>
  DCHECK(WasmExportedFunction::IsWasmExportedFunction(*function));
  auto exported_function = Handle<WasmExportedFunction>::cast(function);
  Handle<WasmInstanceObject> other_instance(exported_function->instance(),
                                            isolate);
  int func_index = exported_function->function_index();
  auto* wasm_function = &other_instance->module()->functions[func_index];
  DCHECK_NOT_NULL(wasm_function);
  DCHECK_NOT_NULL(wasm_function->sig);
  Address call_target = exported_function->GetWasmCallTarget();
  UpdateDispatchTables(isolate, table, table_index, wasm_function->sig,
                       handle(exported_function->instance(), isolate),
                       call_target);
  array->set(table_index, *function);
}

void WasmTableObject::UpdateDispatchTables(
    Isolate* isolate, Handle<WasmTableObject> table, int table_index,
    wasm::FunctionSig* sig, Handle<WasmInstanceObject> from_instance,
    Address call_target) {
  // We simply need to update the IFTs for each instance that imports
  // this table.
  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);

  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    Handle<WasmInstanceObject> to_instance(
        WasmInstanceObject::cast(
            dispatch_tables->get(i + kDispatchTableInstanceOffset)),
        isolate);
    // Note that {SignatureMap::Find} may return {-1} if the signature is
    // not found; it will simply never match any check.
    auto sig_id = to_instance->module()->signature_map.Find(*sig);
    IndirectFunctionTableEntry(to_instance, table_index)
        .set(sig_id, *from_instance, call_target);
  }
}

void WasmTableObject::ClearDispatchTables(Isolate* isolate,
                                          Handle<WasmTableObject> table,
                                          int index) {
  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);
  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    Handle<WasmInstanceObject> target_instance(
        WasmInstanceObject::cast(
            dispatch_tables->get(i + kDispatchTableInstanceOffset)),
        isolate);
    DCHECK_LT(index, target_instance->indirect_function_table_size());
    IndirectFunctionTableEntry(target_instance, index).clear();
  }
}

namespace {
MaybeHandle<JSArrayBuffer> GrowMemoryBuffer(Isolate* isolate,
                                            Handle<JSArrayBuffer> old_buffer,
                                            uint32_t pages,
                                            uint32_t maximum_pages) {
  if (!old_buffer->is_growable()) return {};
  void* old_mem_start = old_buffer->backing_store();
  size_t old_size = old_buffer->byte_length();
  CHECK_GE(wasm::kV8MaxWasmMemoryBytes, old_size);
  CHECK_EQ(0, old_size % wasm::kWasmPageSize);
  size_t old_pages = old_size / wasm::kWasmPageSize;
  if (old_pages > maximum_pages ||            // already reached maximum
      (pages > maximum_pages - old_pages) ||  // exceeds remaining
      (pages > FLAG_wasm_max_mem_pages - old_pages)) {  // exceeds limit
    return {};
  }
  size_t new_size =
      static_cast<size_t>(old_pages + pages) * wasm::kWasmPageSize;
  CHECK_GE(wasm::kV8MaxWasmMemoryBytes, new_size);

  // Reusing the backing store from externalized buffers causes problems with
  // Blink's array buffers. The connection between the two is lost, which can
  // lead to Blink not knowing about the other reference to the buffer and
  // freeing it too early.
  if (!old_buffer->is_external() &&
      ((new_size < old_buffer->allocation_length()) || old_size == new_size)) {
    if (old_size != new_size) {
      DCHECK_NOT_NULL(old_buffer->backing_store());
      // If adjusting permissions fails, propagate error back to return
      // failure to grow.
      if (!i::SetPermissions(GetPlatformPageAllocator(), old_mem_start,
                             new_size, PageAllocator::kReadWrite)) {
        return {};
      }
      reinterpret_cast<v8::Isolate*>(isolate)
          ->AdjustAmountOfExternalAllocatedMemory(pages * wasm::kWasmPageSize);
    }
    // NOTE: We must allocate a new array buffer here because the spec
    // assumes that ArrayBuffers do not change size.
    void* backing_store = old_buffer->backing_store();
    bool is_external = old_buffer->is_external();
    // Disconnect buffer early so GC won't free it.
    i::wasm::DetachMemoryBuffer(isolate, old_buffer, false);
    Handle<JSArrayBuffer> new_buffer =
        wasm::SetupArrayBuffer(isolate, backing_store, new_size, is_external);
    return new_buffer;
  } else {
    // We couldn't reuse the old backing store, so create a new one and copy the
    // old contents in.
    Handle<JSArrayBuffer> new_buffer;
    if (!wasm::NewArrayBuffer(isolate, new_size).ToHandle(&new_buffer)) {
      return {};
    }
    wasm::WasmMemoryTracker* const memory_tracker =
        isolate->wasm_engine()->memory_tracker();
    // If the old buffer had full guard regions, we can only safely use the new
    // buffer if it also has full guard regions. Otherwise, we'd have to
    // recompile all the instances using this memory to insert bounds checks.
    if (memory_tracker->HasFullGuardRegions(old_mem_start) &&
        !memory_tracker->HasFullGuardRegions(new_buffer->backing_store())) {
      return {};
    }
    if (old_size == 0) return new_buffer;
    memcpy(new_buffer->backing_store(), old_mem_start, old_size);
    DCHECK(old_buffer.is_null() || !old_buffer->is_shared());
    constexpr bool free_memory = true;
    i::wasm::DetachMemoryBuffer(isolate, old_buffer, free_memory);
    return new_buffer;
  }
}

// May GC, because SetSpecializationMemInfoFrom may GC
void SetInstanceMemory(Handle<WasmInstanceObject> instance,
                       Handle<JSArrayBuffer> buffer) {
  instance->SetRawMemory(reinterpret_cast<byte*>(buffer->backing_store()),
                         buffer->byte_length());
#if DEBUG
  if (!FLAG_mock_arraybuffer_allocator) {
    // To flush out bugs earlier, in DEBUG mode, check that all pages of the
    // memory are accessible by reading and writing one byte on each page.
    // Don't do this if the mock ArrayBuffer allocator is enabled.
    byte* mem_start = instance->memory_start();
    size_t mem_size = instance->memory_size();
    for (size_t offset = 0; offset < mem_size; offset += wasm::kWasmPageSize) {
      byte val = mem_start[offset];
      USE(val);
      mem_start[offset] = val;
    }
  }
#endif
}

}  // namespace

Handle<WasmMemoryObject> WasmMemoryObject::New(
    Isolate* isolate, MaybeHandle<JSArrayBuffer> maybe_buffer,
    int32_t maximum) {
  // TODO(kschimpf): Do we need to add an argument that defines the
  // style of memory the user prefers (with/without trap handling), so
  // that the memory will match the style of the compiled wasm module.
  // See issue v8:7143
  Handle<JSFunction> memory_ctor(
      isolate->native_context()->wasm_memory_constructor(), isolate);
  auto memory_obj = Handle<WasmMemoryObject>::cast(
      isolate->factory()->NewJSObject(memory_ctor, TENURED));

  Handle<JSArrayBuffer> buffer;
  if (!maybe_buffer.ToHandle(&buffer)) {
    // If no buffer was provided, create a 0-length one.
    buffer = wasm::SetupArrayBuffer(isolate, nullptr, 0, false);
  }
  memory_obj->set_array_buffer(*buffer);
  memory_obj->set_maximum_pages(maximum);

  return memory_obj;
}

uint32_t WasmMemoryObject::current_pages() {
  return static_cast<uint32_t>(array_buffer()->byte_length() /
                               wasm::kWasmPageSize);
}

bool WasmMemoryObject::has_full_guard_region(Isolate* isolate) {
  const wasm::WasmMemoryTracker::AllocationData* allocation =
      isolate->wasm_engine()->memory_tracker()->FindAllocationData(
          array_buffer()->backing_store());
  CHECK_NOT_NULL(allocation);

  Address allocation_base =
      reinterpret_cast<Address>(allocation->allocation_base);
  Address buffer_start = reinterpret_cast<Address>(allocation->buffer_start);

  // Return whether the allocation covers every possible Wasm heap index.
  //
  // We always have the following relationship:
  // allocation_base <= buffer_start <= buffer_start + memory_size <=
  // allocation_base + allocation_length
  // (in other words, the buffer fits within the allocation)
  //
  // The space between buffer_start + memory_size and allocation_base +
  // allocation_length is the guard region. Here we make sure the guard region
  // is large enough for any Wasm heap offset.
  return buffer_start + wasm::kWasmMaxHeapOffset <=
         allocation_base + allocation->allocation_length;
}

void WasmMemoryObject::AddInstance(Isolate* isolate,
                                   Handle<WasmMemoryObject> memory,
                                   Handle<WasmInstanceObject> instance) {
  Handle<WeakArrayList> old_instances =
      memory->has_instances()
          ? Handle<WeakArrayList>(memory->instances(), isolate)
          : handle(ReadOnlyRoots(isolate->heap()).empty_weak_array_list(),
                   isolate);
  Handle<WeakArrayList> new_instances = WeakArrayList::AddToEnd(
      isolate, old_instances, MaybeObjectHandle::Weak(instance));
  memory->set_instances(*new_instances);
  Handle<JSArrayBuffer> buffer(memory->array_buffer(), isolate);
  SetInstanceMemory(instance, buffer);
}

void WasmMemoryObject::RemoveInstance(Handle<WasmMemoryObject> memory,
                                      Handle<WasmInstanceObject> instance) {
  if (memory->has_instances()) {
    memory->instances()->RemoveOne(MaybeObjectHandle::Weak(instance));
  }
}

// static
int32_t WasmMemoryObject::Grow(Isolate* isolate,
                               Handle<WasmMemoryObject> memory_object,
                               uint32_t pages) {
  Handle<JSArrayBuffer> old_buffer(memory_object->array_buffer(), isolate);
  if (!old_buffer->is_growable()) return -1;
  size_t old_size = old_buffer->byte_length();
  DCHECK_EQ(0, old_size % wasm::kWasmPageSize);
  Handle<JSArrayBuffer> new_buffer;

  uint32_t maximum_pages = FLAG_wasm_max_mem_pages;
  if (memory_object->has_maximum_pages()) {
    maximum_pages = Min(FLAG_wasm_max_mem_pages,
                        static_cast<uint32_t>(memory_object->maximum_pages()));
  }
  if (!GrowMemoryBuffer(isolate, old_buffer, pages, maximum_pages)
           .ToHandle(&new_buffer)) {
    return -1;
  }

  if (memory_object->has_instances()) {
    Handle<WeakArrayList> instances(memory_object->instances(), isolate);
    for (int i = 0; i < instances->length(); i++) {
      MaybeObject* elem = instances->Get(i);
      HeapObject* heap_object;
      if (elem->GetHeapObjectIfWeak(&heap_object)) {
        Handle<WasmInstanceObject> instance(
            WasmInstanceObject::cast(heap_object), isolate);
        SetInstanceMemory(instance, new_buffer);
      } else {
        DCHECK(elem->IsCleared());
      }
    }
  }
  memory_object->set_array_buffer(*new_buffer);
  return static_cast<uint32_t>(old_size / wasm::kWasmPageSize);
}

// static
MaybeHandle<WasmGlobalObject> WasmGlobalObject::New(
    Isolate* isolate, MaybeHandle<JSArrayBuffer> maybe_buffer,
    wasm::ValueType type, int32_t offset, bool is_mutable) {
  Handle<JSFunction> global_ctor(
      isolate->native_context()->wasm_global_constructor(), isolate);
  auto global_obj = Handle<WasmGlobalObject>::cast(
      isolate->factory()->NewJSObject(global_ctor));

  uint32_t type_size = wasm::ValueTypes::ElementSizeInBytes(type);

  Handle<JSArrayBuffer> buffer;
  if (!maybe_buffer.ToHandle(&buffer)) {
    // If no buffer was provided, create one long enough for the given type.
    buffer =
        isolate->factory()->NewJSArrayBuffer(SharedFlag::kNotShared, TENURED);

    const bool initialize = true;
    if (!JSArrayBuffer::SetupAllocatingData(buffer, isolate, type_size,
                                            initialize)) {
      return {};
    }
  }

  // Check that the offset is in bounds.
  CHECK_LE(offset + type_size, buffer->byte_length());

  global_obj->set_array_buffer(*buffer);
  global_obj->set_flags(0);
  global_obj->set_type(type);
  global_obj->set_offset(offset);
  global_obj->set_is_mutable(is_mutable);

  return global_obj;
}

void IndirectFunctionTableEntry::clear() {
  instance_->indirect_function_table_sig_ids()[index_] = -1;
  instance_->indirect_function_table_targets()[index_] = 0;
  instance_->indirect_function_table_instances()->set(
      index_, ReadOnlyRoots(instance_->GetIsolate()).undefined_value());
}

void IndirectFunctionTableEntry::set(int sig_id, WasmInstanceObject* instance,
                                     Address call_target) {
  TRACE_IFT("IFT entry %p[%d] = {sig_id=%d, instance=%p, target=%" PRIuPTR
            "}\n",
            *instance_, index_, sig_id, instance, call_target);
  instance_->indirect_function_table_sig_ids()[index_] = sig_id;
  instance_->indirect_function_table_targets()[index_] = call_target;
  instance_->indirect_function_table_instances()->set(index_, instance);
}

WasmInstanceObject* IndirectFunctionTableEntry::instance() {
  return WasmInstanceObject::cast(
      instance_->indirect_function_table_instances()->get(index_));
}

int IndirectFunctionTableEntry::sig_id() {
  return instance_->indirect_function_table_sig_ids()[index_];
}

Address IndirectFunctionTableEntry::target() {
  return instance_->indirect_function_table_targets()[index_];
}

void ImportedFunctionEntry::set_wasm_to_js(
    JSReceiver* callable, const wasm::WasmCode* wasm_to_js_wrapper) {
  TRACE_IFT("Import callable %p[%d] = {callable=%p, target=%p}\n", *instance_,
            index_, callable, wasm_to_js_wrapper->instructions().start());
  DCHECK_EQ(wasm::WasmCode::kWasmToJsWrapper, wasm_to_js_wrapper->kind());
  instance_->imported_function_instances()->set(index_, *instance_);
  instance_->imported_function_callables()->set(index_, callable);
  instance_->imported_function_targets()[index_] =
      wasm_to_js_wrapper->instruction_start();
}

void ImportedFunctionEntry::set_wasm_to_wasm(WasmInstanceObject* instance,
                                             Address call_target) {
  TRACE_IFT("Import WASM %p[%d] = {instance=%p, target=%" PRIuPTR "}\n",
            *instance_, index_, instance, call_target);
  instance_->imported_function_instances()->set(index_, instance);
  instance_->imported_function_callables()->set(
      index_, instance_->GetReadOnlyRoots().undefined_value());
  instance_->imported_function_targets()[index_] = call_target;
}

WasmInstanceObject* ImportedFunctionEntry::instance() {
  return WasmInstanceObject::cast(
      instance_->imported_function_instances()->get(index_));
}

JSReceiver* ImportedFunctionEntry::callable() {
  return JSReceiver::cast(
      instance_->imported_function_callables()->get(index_));
}

Address ImportedFunctionEntry::target() {
  return instance_->imported_function_targets()[index_];
}

bool ImportedFunctionEntry::is_js_receiver_entry() {
  return instance_->imported_function_callables()->get(index_)->IsJSReceiver();
}

bool WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
    Handle<WasmInstanceObject> instance, uint32_t minimum_size) {
  uint32_t old_size = instance->indirect_function_table_size();
  if (old_size >= minimum_size) return false;  // Nothing to do.

  Isolate* isolate = instance->GetIsolate();
  HandleScope scope(isolate);
  auto native_allocations = GetNativeAllocations(*instance);
  native_allocations->resize_indirect_function_table(isolate, instance,
                                                     minimum_size);
  return true;
}

void WasmInstanceObject::SetRawMemory(byte* mem_start, size_t mem_size) {
  CHECK_LE(mem_size, wasm::kV8MaxWasmMemoryBytes);
#if V8_HOST_ARCH_64_BIT
  uint64_t mem_mask64 = base::bits::RoundUpToPowerOfTwo64(mem_size) - 1;
  set_memory_start(mem_start);
  set_memory_size(mem_size);
  set_memory_mask(mem_mask64);
#else
  // Must handle memory > 2GiB specially.
  CHECK_LE(mem_size, size_t{kMaxUInt32});
  uint32_t mem_mask32 =
      (mem_size > 2 * size_t{GB})
          ? 0xFFFFFFFFu
          : base::bits::RoundUpToPowerOfTwo32(static_cast<uint32_t>(mem_size)) -
                1;
  set_memory_start(mem_start);
  set_memory_size(mem_size);
  set_memory_mask(mem_mask32);
#endif
}

const WasmModule* WasmInstanceObject::module() {
  return module_object()->module();
}

Handle<WasmDebugInfo> WasmInstanceObject::GetOrCreateDebugInfo(
    Handle<WasmInstanceObject> instance) {
  if (instance->has_debug_info()) {
    return handle(instance->debug_info(), instance->GetIsolate());
  }
  Handle<WasmDebugInfo> new_info = WasmDebugInfo::New(instance);
  DCHECK(instance->has_debug_info());
  return new_info;
}

Handle<WasmInstanceObject> WasmInstanceObject::New(
    Isolate* isolate, Handle<WasmModuleObject> module_object) {
  Handle<JSFunction> instance_cons(
      isolate->native_context()->wasm_instance_constructor(), isolate);
  Handle<JSObject> instance_object =
      isolate->factory()->NewJSObject(instance_cons, TENURED);

  Handle<WasmInstanceObject> instance(
      reinterpret_cast<WasmInstanceObject*>(*instance_object), isolate);

  // Initialize the imported function arrays.
  auto module = module_object->module();
  auto num_imported_functions = module->num_imported_functions;
  auto num_imported_mutable_globals = module->num_imported_mutable_globals;
  size_t native_allocations_size = EstimateNativeAllocationsSize(module);
  auto native_allocations = Managed<WasmInstanceNativeAllocations>::Allocate(
      isolate, native_allocations_size, instance, num_imported_functions,
      num_imported_mutable_globals);
  instance->set_managed_native_allocations(*native_allocations);

  Handle<FixedArray> imported_function_instances =
      isolate->factory()->NewFixedArray(num_imported_functions);
  instance->set_imported_function_instances(*imported_function_instances);

  Handle<FixedArray> imported_function_callables =
      isolate->factory()->NewFixedArray(num_imported_functions);
  instance->set_imported_function_callables(*imported_function_callables);

  Handle<Code> centry_stub = CodeFactory::CEntry(isolate);
  instance->set_centry_stub(*centry_stub);

  instance->SetRawMemory(nullptr, 0);
  instance->set_roots_array_address(
      reinterpret_cast<Address>(isolate->heap()->roots_array_start()));
  instance->set_stack_limit_address(
      isolate->stack_guard()->address_of_jslimit());
  instance->set_real_stack_limit_address(
      isolate->stack_guard()->address_of_real_jslimit());
  instance->set_globals_start(nullptr);
  instance->set_indirect_function_table_size(0);
  instance->set_indirect_function_table_sig_ids(nullptr);
  instance->set_indirect_function_table_targets(nullptr);
  instance->set_native_context(*isolate->native_context());
  instance->set_module_object(*module_object);
  instance->set_undefined_value(ReadOnlyRoots(isolate).undefined_value());
  instance->set_null_value(ReadOnlyRoots(isolate).null_value());
  instance->set_jump_table_start(
      module_object->native_module()->jump_table_start());

  // Insert the new instance into the modules weak list of instances.
  // TODO(mstarzinger): Allow to reuse holes in the {WeakArrayList} below.
  Handle<WeakArrayList> weak_instance_list(module_object->weak_instance_list(),
                                           isolate);
  weak_instance_list = WeakArrayList::AddToEnd(
      isolate, weak_instance_list, MaybeObjectHandle::Weak(instance));
  module_object->set_weak_instance_list(*weak_instance_list);

  return instance;
}

Address WasmInstanceObject::GetCallTarget(uint32_t func_index) {
  wasm::NativeModule* native_module = module_object()->native_module();
  if (func_index < native_module->num_imported_functions()) {
    return imported_function_targets()[func_index];
  }
  return native_module->GetCallTargetForFunction(func_index);
}

// static
Handle<WasmExceptionObject> WasmExceptionObject::New(
    Isolate* isolate, const wasm::FunctionSig* sig,
    Handle<HeapObject> exception_tag) {
  Handle<JSFunction> exception_cons(
      isolate->native_context()->wasm_exception_constructor(), isolate);
  Handle<JSObject> exception_object =
      isolate->factory()->NewJSObject(exception_cons, TENURED);
  Handle<WasmExceptionObject> exception =
      Handle<WasmExceptionObject>::cast(exception_object);

  // Serialize the signature.
  DCHECK_EQ(0, sig->return_count());
  DCHECK_LE(sig->parameter_count(), std::numeric_limits<int>::max());
  int sig_size = static_cast<int>(sig->parameter_count());
  Handle<PodArray<wasm::ValueType>> serialized_sig =
      PodArray<wasm::ValueType>::New(isolate, sig_size, TENURED);
  int index = 0;  // Index into the {PodArray} above.
  for (wasm::ValueType param : sig->parameters()) {
    serialized_sig->set(index++, param);
  }
  exception->set_serialized_signature(*serialized_sig);
  exception->set_exception_tag(*exception_tag);

  return exception;
}

bool WasmExceptionObject::IsSignatureEqual(const wasm::FunctionSig* sig) {
  DCHECK_EQ(0, sig->return_count());
  DCHECK_LE(sig->parameter_count(), std::numeric_limits<int>::max());
  int sig_size = static_cast<int>(sig->parameter_count());
  if (sig_size != serialized_signature()->length()) return false;
  for (int index = 0; index < sig_size; ++index) {
    if (sig->GetParam(index) != serialized_signature()->get(index)) {
      return false;
    }
  }
  return true;
}

bool WasmExportedFunction::IsWasmExportedFunction(Object* object) {
  if (!object->IsJSFunction()) return false;
  JSFunction* js_function = JSFunction::cast(object);
  if (Code::JS_TO_WASM_FUNCTION != js_function->code()->kind()) return false;
  DCHECK(js_function->shared()->HasWasmExportedFunctionData());
  return true;
}

WasmExportedFunction* WasmExportedFunction::cast(Object* object) {
  DCHECK(IsWasmExportedFunction(object));
  return reinterpret_cast<WasmExportedFunction*>(object);
}

WasmInstanceObject* WasmExportedFunction::instance() {
  return shared()->wasm_exported_function_data()->instance();
}

int WasmExportedFunction::function_index() {
  return shared()->wasm_exported_function_data()->function_index();
}

Handle<WasmExportedFunction> WasmExportedFunction::New(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    MaybeHandle<String> maybe_name, int func_index, int arity,
    Handle<Code> export_wrapper) {
  DCHECK_EQ(Code::JS_TO_WASM_FUNCTION, export_wrapper->kind());
  int num_imported_functions = instance->module()->num_imported_functions;
  int jump_table_offset = -1;
  if (func_index >= num_imported_functions) {
    ptrdiff_t jump_table_diff =
        instance->module_object()->native_module()->jump_table_offset(
            func_index);
    DCHECK(jump_table_diff >= 0 && jump_table_diff <= INT_MAX);
    jump_table_offset = static_cast<int>(jump_table_diff);
  }
  Handle<WasmExportedFunctionData> function_data =
      Handle<WasmExportedFunctionData>::cast(isolate->factory()->NewStruct(
          WASM_EXPORTED_FUNCTION_DATA_TYPE, TENURED));
  function_data->set_wrapper_code(*export_wrapper);
  function_data->set_instance(*instance);
  function_data->set_jump_table_offset(jump_table_offset);
  function_data->set_function_index(func_index);
  Handle<String> name;
  if (!maybe_name.ToHandle(&name)) {
    EmbeddedVector<char, 16> buffer;
    int length = SNPrintF(buffer, "%d", func_index);
    name = isolate->factory()
               ->NewStringFromOneByte(
                   Vector<uint8_t>::cast(buffer.SubVector(0, length)))
               .ToHandleChecked();
  }
  NewFunctionArgs args = NewFunctionArgs::ForWasm(
      name, function_data, isolate->sloppy_function_without_prototype_map());
  Handle<JSFunction> js_function = isolate->factory()->NewFunction(args);
  // According to the spec, exported functions should not have a [[Construct]]
  // method.
  DCHECK(!js_function->IsConstructor());
  js_function->shared()->set_length(arity);
  js_function->shared()->set_internal_formal_parameter_count(arity);
  return Handle<WasmExportedFunction>::cast(js_function);
}

Address WasmExportedFunction::GetWasmCallTarget() {
  return instance()->GetCallTarget(function_index());
}

#undef TRACE
#undef TRACE_IFT
}  // namespace internal
}  // namespace v8
