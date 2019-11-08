// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-objects.h"
#include "src/utils/utils.h"

#include "src/base/iterator.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/code-factory.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/debug-interface.h"
#include "src/logging/counters.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/struct-inl.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/vector.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"

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
                                size_t num_imported_mutable_globals,
                                size_t num_data_segments,
                                size_t num_elem_segments) {
    SET(instance, imported_function_targets,
        reinterpret_cast<Address*>(
            calloc(num_imported_functions, sizeof(Address))));
    SET(instance, imported_mutable_globals,
        reinterpret_cast<Address*>(
            calloc(num_imported_mutable_globals, sizeof(Address))));
    SET(instance, data_segment_starts,
        reinterpret_cast<Address*>(calloc(num_data_segments, sizeof(Address))));
    SET(instance, data_segment_sizes,
        reinterpret_cast<uint32_t*>(
            calloc(num_data_segments, sizeof(uint32_t))));
    SET(instance, dropped_data_segments,
        reinterpret_cast<uint8_t*>(calloc(num_data_segments, sizeof(uint8_t))));
    SET(instance, dropped_elem_segments,
        reinterpret_cast<uint8_t*>(calloc(num_elem_segments, sizeof(uint8_t))));
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
    ::free(data_segment_starts_);
    data_segment_starts_ = nullptr;
    ::free(data_segment_sizes_);
    data_segment_sizes_ = nullptr;
    ::free(dropped_data_segments_);
    dropped_data_segments_ = nullptr;
    ::free(dropped_elem_segments_);
    dropped_elem_segments_ = nullptr;
  }
  // Resizes the indirect function table.
  void resize_indirect_function_table(Isolate* isolate,
                                      Handle<WasmInstanceObject> instance,
                                      uint32_t new_size) {
    uint32_t old_size = instance->indirect_function_table_size();
    void* new_sig_ids = nullptr;
    void* new_targets = nullptr;
    Handle<FixedArray> new_refs;
    if (indirect_function_table_sig_ids_) {
      // Reallocate the old storage.
      new_sig_ids = realloc(indirect_function_table_sig_ids_,
                            new_size * sizeof(uint32_t));
      new_targets =
          realloc(indirect_function_table_targets_, new_size * sizeof(Address));

      Handle<FixedArray> old(instance->indirect_function_table_refs(), isolate);
      new_refs = isolate->factory()->CopyFixedArrayAndGrow(
          old, static_cast<int>(new_size - old_size));
    } else {
      // Allocate new storage.
      new_sig_ids = malloc(new_size * sizeof(uint32_t));
      new_targets = malloc(new_size * sizeof(Address));
      new_refs = isolate->factory()->NewFixedArray(static_cast<int>(new_size));
    }
    // Initialize new entries.
    instance->set_indirect_function_table_size(new_size);
    SET(instance, indirect_function_table_sig_ids,
        reinterpret_cast<uint32_t*>(new_sig_ids));
    SET(instance, indirect_function_table_targets,
        reinterpret_cast<Address*>(new_targets));

    instance->set_indirect_function_table_refs(*new_refs);
    for (uint32_t j = old_size; j < new_size; j++) {
      // {WasmInstanceNativeAllocations} only manages the memory of table 0.
      // Therefore we pass the {table_index} as a constant here.
      IndirectFunctionTableEntry(instance, 0, static_cast<int>(j)).clear();
    }
  }
  uint32_t* indirect_function_table_sig_ids_ = nullptr;
  Address* indirect_function_table_targets_ = nullptr;
  Address* imported_function_targets_ = nullptr;
  Address* imported_mutable_globals_ = nullptr;
  Address* data_segment_starts_ = nullptr;
  uint32_t* data_segment_sizes_ = nullptr;
  uint8_t* dropped_data_segments_ = nullptr;
  uint8_t* dropped_elem_segments_ = nullptr;
#undef SET
};

size_t EstimateNativeAllocationsSize(const WasmModule* module) {
  size_t estimate =
      sizeof(WasmInstanceNativeAllocations) +
      (1 * kSystemPointerSize * module->num_imported_mutable_globals) +
      (2 * kSystemPointerSize * module->num_imported_functions) +
      ((kSystemPointerSize + sizeof(uint32_t) + sizeof(uint8_t)) *
       module->num_declared_data_segments);
  for (auto& table : module->tables) {
    estimate += 3 * kSystemPointerSize * table.initial_size;
  }
  return estimate;
}

WasmInstanceNativeAllocations* GetNativeAllocations(
    WasmInstanceObject instance) {
  return Managed<WasmInstanceNativeAllocations>::cast(
             instance.managed_native_allocations())
      .raw();
}

#ifdef DEBUG
bool IsBreakablePosition(wasm::NativeModule* native_module, int func_index,
                         int offset_in_func) {
  AccountingAllocator alloc;
  Zone tmp(&alloc, ZONE_NAME);
  wasm::BodyLocalDecls locals(&tmp);
  const byte* module_start = native_module->wire_bytes().begin();
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
    Isolate* isolate, std::shared_ptr<wasm::NativeModule> native_module,
    Handle<Script> script) {
  Handle<FixedArray> export_wrappers = isolate->factory()->NewFixedArray(0);
  return New(isolate, std::move(native_module), script, export_wrappers);
}

// static
Handle<WasmModuleObject> WasmModuleObject::New(
    Isolate* isolate, std::shared_ptr<wasm::NativeModule> native_module,
    Handle<Script> script, Handle<FixedArray> export_wrappers) {
  const WasmModule* module = native_module->module();
  size_t code_size_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(module);
  return New(isolate, std::move(native_module), script, export_wrappers,
             code_size_estimate);
}

// static
Handle<WasmModuleObject> WasmModuleObject::New(
    Isolate* isolate, std::shared_ptr<wasm::NativeModule> native_module,
    Handle<Script> script, Handle<FixedArray> export_wrappers,
    size_t code_size_estimate) {
  const WasmModule* module = native_module->module();

  // Use the given shared {NativeModule}, but increase its reference count by
  // allocating a new {Managed<T>} that the {WasmModuleObject} references.
  size_t memory_estimate =
      code_size_estimate +
      wasm::WasmCodeManager::EstimateNativeModuleNonCodeSize(module);
  Handle<Managed<wasm::NativeModule>> managed_native_module =
      Managed<wasm::NativeModule>::FromSharedPtr(isolate, memory_estimate,
                                                 std::move(native_module));

  Handle<WasmModuleObject> module_object = Handle<WasmModuleObject>::cast(
      isolate->factory()->NewJSObject(isolate->wasm_module_constructor()));
  module_object->set_export_wrappers(*export_wrappers);
  if (script->type() == Script::TYPE_WASM) {
    script->set_wasm_breakpoint_infos(
        ReadOnlyRoots(isolate).empty_fixed_array());
    script->set_wasm_managed_native_module(*managed_native_module);
    script->set_wasm_weak_instance_list(
        ReadOnlyRoots(isolate).empty_weak_array_list());
  }
  module_object->set_script(*script);
  module_object->set_managed_native_module(*managed_native_module);
  return module_object;
}

// static
bool WasmModuleObject::SetBreakPoint(Handle<Script> script, int* position,
                                     Handle<BreakPoint> break_point) {
  Isolate* isolate = script->GetIsolate();

  // Find the function for this breakpoint.
  const WasmModule* module = script->wasm_native_module()->module();
  int func_index = GetContainingWasmFunction(module, *position);
  if (func_index < 0) return false;
  const WasmFunction& func = module->functions[func_index];
  int offset_in_func = *position - func.code.offset();

  // According to the current design, we should only be called with valid
  // breakable positions.
  DCHECK(IsBreakablePosition(script->wasm_native_module(), func_index,
                             offset_in_func));

  // Insert new break point into break_positions of module object.
  WasmModuleObject::AddBreakpointToInfo(script, *position, break_point);

  // Iterate over all instances and tell them to set this new breakpoint.
  // We do this using the weak list of all instances from the script.
  Handle<WeakArrayList> weak_instance_list(script->wasm_weak_instance_list(),
                                           isolate);
  for (int i = 0; i < weak_instance_list->length(); ++i) {
    MaybeObject maybe_instance = weak_instance_list->Get(i);
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

// static
bool WasmModuleObject::ClearBreakPoint(Handle<Script> script, int position,
                                       Handle<BreakPoint> break_point) {
  Isolate* isolate = script->GetIsolate();

  // Find the function for this breakpoint.
  const WasmModule* module = script->wasm_native_module()->module();
  int func_index = GetContainingWasmFunction(module, position);
  if (func_index < 0) return false;
  const WasmFunction& func = module->functions[func_index];
  int offset_in_func = position - func.code.offset();

  if (!WasmModuleObject::RemoveBreakpointFromInfo(script, position,
                                                  break_point)) {
    return false;
  }

  // Iterate over all instances and tell them to remove this breakpoint.
  // We do this using the weak list of all instances from the script.
  Handle<WeakArrayList> weak_instance_list(script->wasm_weak_instance_list(),
                                           isolate);
  for (int i = 0; i < weak_instance_list->length(); ++i) {
    MaybeObject maybe_instance = weak_instance_list->Get(i);
    if (maybe_instance->IsWeak()) {
      Handle<WasmInstanceObject> instance(
          WasmInstanceObject::cast(maybe_instance->GetHeapObjectAssumeWeak()),
          isolate);
      Handle<WasmDebugInfo> debug_info =
          WasmInstanceObject::GetOrCreateDebugInfo(instance);
      WasmDebugInfo::ClearBreakpoint(debug_info, func_index, offset_in_func);
    }
  }

  return true;
}

namespace {

int GetBreakpointPos(Isolate* isolate, Object break_point_info_or_undef) {
  if (break_point_info_or_undef.IsUndefined(isolate)) return kMaxInt;
  return BreakPointInfo::cast(break_point_info_or_undef).source_position();
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
    Object mid_obj = breakpoint_infos->get(mid);
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

// static
void WasmModuleObject::AddBreakpointToInfo(Handle<Script> script, int position,
                                           Handle<BreakPoint> break_point) {
  Isolate* isolate = script->GetIsolate();
  Handle<FixedArray> breakpoint_infos;
  if (script->has_wasm_breakpoint_infos()) {
    breakpoint_infos = handle(script->wasm_breakpoint_infos(), isolate);
  } else {
    breakpoint_infos =
        isolate->factory()->NewFixedArray(4, AllocationType::kOld);
    script->set_wasm_breakpoint_infos(*breakpoint_infos);
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
                           .IsUndefined(isolate);
  Handle<FixedArray> new_breakpoint_infos = breakpoint_infos;
  if (need_realloc) {
    new_breakpoint_infos = isolate->factory()->NewFixedArray(
        2 * breakpoint_infos->length(), AllocationType::kOld);
    script->set_wasm_breakpoint_infos(*new_breakpoint_infos);
    // Copy over the entries [0, insert_pos).
    for (int i = 0; i < insert_pos; ++i)
      new_breakpoint_infos->set(i, breakpoint_infos->get(i));
  }

  // Move elements [insert_pos, ...] up by one.
  for (int i = breakpoint_infos->length() - 1; i >= insert_pos; --i) {
    Object entry = breakpoint_infos->get(i);
    if (entry.IsUndefined(isolate)) continue;
    new_breakpoint_infos->set(i + 1, entry);
  }

  // Generate new BreakpointInfo.
  Handle<BreakPointInfo> breakpoint_info =
      isolate->factory()->NewBreakPointInfo(position);
  BreakPointInfo::SetBreakPoint(isolate, breakpoint_info, break_point);

  // Now insert new position at insert_pos.
  new_breakpoint_infos->set(insert_pos, *breakpoint_info);
}

// static
bool WasmModuleObject::RemoveBreakpointFromInfo(
    Handle<Script> script, int position, Handle<BreakPoint> break_point) {
  if (!script->has_wasm_breakpoint_infos()) return false;

  Isolate* isolate = script->GetIsolate();
  Handle<FixedArray> breakpoint_infos(script->wasm_breakpoint_infos(), isolate);

  int pos = FindBreakpointInfoInsertPos(isolate, breakpoint_infos, position);

  // Does a BreakPointInfo object already exist for this position?
  if (pos == breakpoint_infos->length()) return false;

  Handle<BreakPointInfo> info(BreakPointInfo::cast(breakpoint_infos->get(pos)),
                              isolate);
  BreakPointInfo::ClearBreakPoint(isolate, info, break_point);

  // Check if there are no more breakpoints at this location.
  if (info->GetBreakPointCount(isolate) == 0) {
    // Update array by moving breakpoints up one position.
    for (int i = pos; i < breakpoint_infos->length() - 1; i++) {
      Object entry = breakpoint_infos->get(i + 1);
      breakpoint_infos->set(i, entry);
      if (entry.IsUndefined(isolate)) break;
    }
    // Make sure last array element is empty as a result.
    breakpoint_infos->set_undefined(breakpoint_infos->length() - 1);
  }
  return true;
}

void WasmModuleObject::SetBreakpointsOnNewInstance(
    Handle<Script> script, Handle<WasmInstanceObject> instance) {
  if (!script->has_wasm_breakpoint_infos()) return;
  Isolate* isolate = script->GetIsolate();
  Handle<WasmDebugInfo> debug_info =
      WasmInstanceObject::GetOrCreateDebugInfo(instance);

  Handle<FixedArray> breakpoint_infos(script->wasm_breakpoint_infos(), isolate);
  // If the array exists, it should not be empty.
  DCHECK_LT(0, breakpoint_infos->length());

  for (int i = 0, e = breakpoint_infos->length(); i < e; ++i) {
    Handle<Object> obj(breakpoint_infos->get(i), isolate);
    if (obj->IsUndefined(isolate)) {
      for (; i < e; ++i) {
        DCHECK(breakpoint_infos->get(i).IsUndefined(isolate));
      }
      break;
    }
    Handle<BreakPointInfo> breakpoint_info = Handle<BreakPointInfo>::cast(obj);
    int position = breakpoint_info->source_position();

    // Find the function for this breakpoint, and set the breakpoint.
    const WasmModule* module = script->wasm_native_module()->module();
    int func_index = GetContainingWasmFunction(module, position);
    DCHECK_LE(0, func_index);
    const WasmFunction& func = module->functions[func_index];
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

  wasm::AsmJsOffsets asm_offsets;
  {
    DisallowHeapAllocation no_gc;
    byte* bytes_start = offset_table->GetDataStartAddress();
    byte* bytes_end = reinterpret_cast<byte*>(
        reinterpret_cast<Address>(bytes_start) + offset_table->length() - 1);
    asm_offsets = wasm::DecodeAsmJsOffsets(bytes_start, bytes_end).value();
  }
  // Wasm bytes must be valid and must contain asm.js offset table.
  DCHECK_GE(kMaxInt, asm_offsets.size());
  int num_functions = static_cast<int>(asm_offsets.size());
  int num_imported_functions =
      static_cast<int>(module_object->module()->num_imported_functions);
  DCHECK_EQ(module_object->module()->functions.size(),
            static_cast<size_t>(num_functions) + num_imported_functions);
  int num_entries = 0;
  for (int func = 0; func < num_functions; ++func) {
    size_t new_size = asm_offsets[func].size();
    DCHECK_LE(new_size, static_cast<size_t>(kMaxInt) - num_entries);
    num_entries += static_cast<int>(new_size);
  }
  // One byte to encode that this is a decoded table.
  DCHECK_GE(kMaxInt,
            1 + static_cast<uint64_t>(num_entries) * kOTESize * kIntSize);
  int total_size = 1 + num_entries * kOTESize * kIntSize;
  Handle<ByteArray> decoded_table =
      isolate->factory()->NewByteArray(total_size, AllocationType::kOld);
  decoded_table->set(total_size - 1, AsmJsTableType::Decoded);
  module_object->set_asm_js_offset_table(*decoded_table);

  int idx = 0;
  const std::vector<WasmFunction>& wasm_funs =
      module_object->module()->functions;
  for (int func = 0; func < num_functions; ++func) {
    std::vector<wasm::AsmJsOffsetEntry>& func_asm_offsets = asm_offsets[func];
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

  if (module->origin == wasm::kWasmOrigin) {
    // for non-asm.js modules, we just add the function's start offset
    // to make a module-relative position.
    return byte_offset + GetWasmFunctionOffset(module, func_index);
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

// static
bool WasmModuleObject::GetPossibleBreakpoints(
    wasm::NativeModule* native_module, const v8::debug::Location& start,
    const v8::debug::Location& end,
    std::vector<v8::debug::BreakLocation>* locations) {
  DisallowHeapAllocation no_gc;

  const std::vector<WasmFunction>& functions =
      native_module->module()->functions;
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
  const byte* module_start = native_module->wire_bytes().begin();

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

// static
MaybeHandle<FixedArray> WasmModuleObject::CheckBreakPoints(
    Isolate* isolate, Handle<Script> script, int position) {
  if (!script->has_wasm_breakpoint_infos()) return {};

  Handle<FixedArray> breakpoint_infos(script->wasm_breakpoint_infos(), isolate);
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
  DCHECK(unibrow::Utf8::ValidateEncoding(name_vec.begin(), name_vec.length()));
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

Handle<WasmTableObject> WasmTableObject::New(Isolate* isolate,
                                             wasm::ValueType type,
                                             uint32_t initial, bool has_maximum,
                                             uint32_t maximum,
                                             Handle<FixedArray>* entries) {
  Handle<FixedArray> backing_store = isolate->factory()->NewFixedArray(initial);
  Object null = ReadOnlyRoots(isolate).null_value();
  for (int i = 0; i < static_cast<int>(initial); ++i) {
    backing_store->set(i, null);
  }

  Handle<Object> max;
  if (has_maximum) {
    max = isolate->factory()->NewNumberFromUint(maximum);
  } else {
    max = isolate->factory()->undefined_value();
  }

  Handle<JSFunction> table_ctor(
      isolate->native_context()->wasm_table_constructor(), isolate);
  auto table_obj = Handle<WasmTableObject>::cast(
      isolate->factory()->NewJSObject(table_ctor));
  DisallowHeapAllocation no_gc;

  table_obj->set_raw_type(static_cast<int>(type));
  table_obj->set_entries(*backing_store);
  table_obj->set_maximum_length(*max);

  table_obj->set_dispatch_tables(ReadOnlyRoots(isolate).empty_fixed_array());
  if (entries != nullptr) {
    *entries = backing_store;
  }
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

int WasmTableObject::Grow(Isolate* isolate, Handle<WasmTableObject> table,
                          uint32_t count, Handle<Object> init_value) {
  uint32_t old_size = table->current_length();
  if (count == 0) return old_size;  // Degenerate case: nothing to do.

  // Check if growing by {count} is valid.
  uint32_t max_size;
  if (!table->maximum_length().ToUint32(&max_size)) {
    max_size = FLAG_wasm_max_table_size;
  }
  DCHECK_LE(old_size, max_size);
  if (max_size - old_size < count) return -1;

  uint32_t new_size = old_size + count;
  auto new_store = isolate->factory()->CopyFixedArrayAndGrow(
      handle(table->entries(), isolate), count);
  table->set_entries(*new_store, WriteBarrierMode::UPDATE_WRITE_BARRIER);

  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);
  // Tables are stored in the instance object, no code patching is
  // necessary. We simply have to grow the raw tables in each instance
  // that has imported this table.

  // TODO(titzer): replace the dispatch table with a weak list of all
  // the instances that import a given table.
  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    int table_index =
        Smi::cast(dispatch_tables->get(i + kDispatchTableIndexOffset)).value();

    Handle<WasmInstanceObject> instance(
        WasmInstanceObject::cast(dispatch_tables->get(i)), isolate);

    DCHECK_EQ(old_size, WasmInstanceObject::IndirectFunctionTableSize(
                            isolate, instance, table_index));
    WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
        instance, table_index, new_size);
  }

  for (uint32_t entry = old_size; entry < new_size; ++entry) {
    WasmTableObject::Set(isolate, table, entry, init_value);
  }
  return old_size;
}

bool WasmTableObject::IsInBounds(Isolate* isolate,
                                 Handle<WasmTableObject> table,
                                 uint32_t entry_index) {
  return (entry_index <
              static_cast<uint32_t>(std::numeric_limits<int>::max()) &&
          static_cast<int>(entry_index) < table->entries().length());
}

bool WasmTableObject::IsValidElement(Isolate* isolate,
                                     Handle<WasmTableObject> table,
                                     Handle<Object> entry) {
  // Anyref tables take everything.
  if (table->type() == wasm::kWasmAnyRef) return true;
  // FuncRef tables can store {null}, {WasmExportedFunction}, {WasmJSFunction},
  // or {WasmCapiFunction} objects.
  if (entry->IsNull(isolate)) return true;
  return WasmExportedFunction::IsWasmExportedFunction(*entry) ||
         WasmJSFunction::IsWasmJSFunction(*entry) ||
         WasmCapiFunction::IsWasmCapiFunction(*entry);
}

void WasmTableObject::Set(Isolate* isolate, Handle<WasmTableObject> table,
                          uint32_t index, Handle<Object> entry) {
  // Callers need to perform bounds checks, type check, and error handling.
  DCHECK(IsInBounds(isolate, table, index));
  DCHECK(IsValidElement(isolate, table, entry));

  Handle<FixedArray> entries(table->entries(), isolate);
  // The FixedArray is addressed with int's.
  int entry_index = static_cast<int>(index);
  if (table->type() == wasm::kWasmAnyRef) {
    entries->set(entry_index, *entry);
    return;
  }

  if (entry->IsNull(isolate)) {
    ClearDispatchTables(isolate, table, entry_index);  // Degenerate case.
    entries->set(entry_index, ReadOnlyRoots(isolate).null_value());
    return;
  }

  if (WasmExportedFunction::IsWasmExportedFunction(*entry)) {
    auto exported_function = Handle<WasmExportedFunction>::cast(entry);
    Handle<WasmInstanceObject> target_instance(exported_function->instance(),
                                               isolate);
    int func_index = exported_function->function_index();
    auto* wasm_function = &target_instance->module()->functions[func_index];
    DCHECK_NOT_NULL(wasm_function);
    DCHECK_NOT_NULL(wasm_function->sig);
    UpdateDispatchTables(isolate, table, entry_index, wasm_function->sig,
                         target_instance, func_index);
  } else if (WasmJSFunction::IsWasmJSFunction(*entry)) {
    UpdateDispatchTables(isolate, table, entry_index,
                         Handle<WasmJSFunction>::cast(entry));
  } else {
    DCHECK(WasmCapiFunction::IsWasmCapiFunction(*entry));
    UpdateDispatchTables(isolate, table, entry_index,
                         Handle<WasmCapiFunction>::cast(entry));
  }
  entries->set(entry_index, *entry);
}

Handle<Object> WasmTableObject::Get(Isolate* isolate,
                                    Handle<WasmTableObject> table,
                                    uint32_t index) {
  Handle<FixedArray> entries(table->entries(), isolate);
  // Callers need to perform bounds checks and error handling.
  DCHECK(IsInBounds(isolate, table, index));

  // The FixedArray is addressed with int's.
  int entry_index = static_cast<int>(index);

  Handle<Object> entry(entries->get(entry_index), isolate);

  // First we handle the easy anyref table case.
  if (table->type() == wasm::kWasmAnyRef) return entry;

  // Now we handle the funcref case.
  if (WasmExportedFunction::IsWasmExportedFunction(*entry) ||
      WasmJSFunction::IsWasmJSFunction(*entry) ||
      WasmCapiFunction::IsWasmCapiFunction(*entry)) {
    return entry;
  }

  if (entry->IsNull(isolate)) {
    return entry;
  }

  // {entry} is not a valid entry in the table. It has to be a placeholder
  // for lazy initialization.
  Handle<Tuple2> tuple = Handle<Tuple2>::cast(entry);
  auto instance = handle(WasmInstanceObject::cast(tuple->value1()), isolate);
  int function_index = Smi::cast(tuple->value2()).value();

  // Check if we already compiled a wrapper for the function but did not store
  // it in the table slot yet.
  entry = WasmInstanceObject::GetOrCreateWasmExternalFunction(isolate, instance,
                                                              function_index);
  entries->set(entry_index, *entry);
  return entry;
}

void WasmTableObject::Fill(Isolate* isolate, Handle<WasmTableObject> table,
                           uint32_t start, Handle<Object> entry,
                           uint32_t count) {
  // Bounds checks must be done by the caller.
  DCHECK_LE(start, table->entries().length());
  DCHECK_LE(count, table->entries().length());
  DCHECK_LE(start + count, table->entries().length());

  for (uint32_t i = 0; i < count; i++) {
    WasmTableObject::Set(isolate, table, start + i, entry);
  }
}

void WasmTableObject::UpdateDispatchTables(
    Isolate* isolate, Handle<WasmTableObject> table, int entry_index,
    wasm::FunctionSig* sig, Handle<WasmInstanceObject> target_instance,
    int target_func_index) {
  // We simply need to update the IFTs for each instance that imports
  // this table.
  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);

  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    int table_index =
        Smi::cast(dispatch_tables->get(i + kDispatchTableIndexOffset)).value();
    Handle<WasmInstanceObject> instance(
        WasmInstanceObject::cast(
            dispatch_tables->get(i + kDispatchTableInstanceOffset)),
        isolate);
    // Note that {SignatureMap::Find} may return {-1} if the signature is
    // not found; it will simply never match any check.
    auto sig_id = instance->module()->signature_map.Find(*sig);
    IndirectFunctionTableEntry(instance, table_index, entry_index)
        .Set(sig_id, target_instance, target_func_index);
  }
}

void WasmTableObject::UpdateDispatchTables(Isolate* isolate,
                                           Handle<WasmTableObject> table,
                                           int entry_index,
                                           Handle<WasmJSFunction> function) {
  // We simply need to update the IFTs for each instance that imports
  // this table.
  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);

  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    int table_index =
        Smi::cast(dispatch_tables->get(i + kDispatchTableIndexOffset)).value();
    Handle<WasmInstanceObject> instance(
        WasmInstanceObject::cast(
            dispatch_tables->get(i + kDispatchTableInstanceOffset)),
        isolate);
    WasmInstanceObject::ImportWasmJSFunctionIntoTable(
        isolate, instance, table_index, entry_index, function);
  }
}

void WasmTableObject::UpdateDispatchTables(
    Isolate* isolate, Handle<WasmTableObject> table, int entry_index,
    Handle<WasmCapiFunction> capi_function) {
  // We simply need to update the IFTs for each instance that imports
  // this table.
  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);

  // Reconstruct signature.
  // TODO(jkummerow): Unify with "SignatureHelper" in c-api.cc.
  PodArray<wasm::ValueType> serialized_sig =
      capi_function->GetSerializedSignature();
  int total_count = serialized_sig.length() - 1;
  std::unique_ptr<wasm::ValueType[]> reps(new wasm::ValueType[total_count]);
  int result_count;
  static const wasm::ValueType kMarker = wasm::kWasmStmt;
  for (int i = 0, j = 0; i <= total_count; i++) {
    if (serialized_sig.get(i) == kMarker) {
      result_count = i;
      continue;
    }
    reps[j++] = serialized_sig.get(i);
  }
  int param_count = total_count - result_count;
  wasm::FunctionSig sig(result_count, param_count, reps.get());

  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    int table_index =
        Smi::cast(dispatch_tables->get(i + kDispatchTableIndexOffset)).value();
    Handle<WasmInstanceObject> instance(
        WasmInstanceObject::cast(
            dispatch_tables->get(i + kDispatchTableInstanceOffset)),
        isolate);
    // TODO(jkummerow): Find a way to avoid recompiling wrappers.
    wasm::NativeModule* native_module =
        instance->module_object().native_module();
    Address host_address = capi_function->GetHostCallTarget();
    wasm::WasmCodeRefScope code_ref_scope;
    wasm::WasmCode* wasm_code = compiler::CompileWasmCapiCallWrapper(
        isolate->wasm_engine(), native_module, &sig, host_address);
    isolate->counters()->wasm_generated_code_size()->Increment(
        wasm_code->instructions().length());
    isolate->counters()->wasm_reloc_size()->Increment(
        wasm_code->reloc_info().length());
    Handle<Tuple2> tuple = isolate->factory()->NewTuple2(
        instance, capi_function, AllocationType::kOld);
    // Note that {SignatureMap::Find} may return {-1} if the signature is
    // not found; it will simply never match any check.
    auto sig_id = instance->module()->signature_map.Find(sig);
    IndirectFunctionTableEntry(instance, table_index, entry_index)
        .Set(sig_id, wasm_code->instruction_start(), *tuple);
  }
}

void WasmTableObject::ClearDispatchTables(Isolate* isolate,
                                          Handle<WasmTableObject> table,
                                          int index) {
  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);
  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    int table_index =
        Smi::cast(dispatch_tables->get(i + kDispatchTableIndexOffset)).value();
    Handle<WasmInstanceObject> target_instance(
        WasmInstanceObject::cast(
            dispatch_tables->get(i + kDispatchTableInstanceOffset)),
        isolate);
    DCHECK_LT(index, WasmInstanceObject::IndirectFunctionTableSize(
                         isolate, target_instance, table_index));
    IndirectFunctionTableEntry(target_instance, table_index, index).clear();
  }
}

void WasmTableObject::SetFunctionTablePlaceholder(
    Isolate* isolate, Handle<WasmTableObject> table, int entry_index,
    Handle<WasmInstanceObject> instance, int func_index) {
  // Put (instance, func_index) as a Tuple2 into the table_index.
  // The {WasmExportedFunction} will be created lazily.
  Handle<Tuple2> tuple = isolate->factory()->NewTuple2(
      instance, Handle<Smi>(Smi::FromInt(func_index), isolate),
      AllocationType::kYoung);
  table->entries().set(entry_index, *tuple);
}

void WasmTableObject::GetFunctionTableEntry(
    Isolate* isolate, Handle<WasmTableObject> table, int entry_index,
    bool* is_valid, bool* is_null, MaybeHandle<WasmInstanceObject>* instance,
    int* function_index, MaybeHandle<WasmJSFunction>* maybe_js_function) {
  DCHECK_EQ(table->type(), wasm::kWasmFuncRef);
  DCHECK_LT(entry_index, table->entries().length());
  // We initialize {is_valid} with {true}. We may change it later.
  *is_valid = true;
  Handle<Object> element(table->entries().get(entry_index), isolate);

  *is_null = element->IsNull(isolate);
  if (*is_null) return;

  if (WasmExportedFunction::IsWasmExportedFunction(*element)) {
    auto target_func = Handle<WasmExportedFunction>::cast(element);
    *instance = handle(target_func->instance(), isolate);
    *function_index = target_func->function_index();
    *maybe_js_function = MaybeHandle<WasmJSFunction>();
    return;
  }
  if (WasmJSFunction::IsWasmJSFunction(*element)) {
    *instance = MaybeHandle<WasmInstanceObject>();
    *maybe_js_function = Handle<WasmJSFunction>::cast(element);
    return;
  }
  if (element->IsTuple2()) {
    auto tuple = Handle<Tuple2>::cast(element);
    *instance = handle(WasmInstanceObject::cast(tuple->value1()), isolate);
    *function_index = Smi::cast(tuple->value2()).value();
    *maybe_js_function = MaybeHandle<WasmJSFunction>();
    return;
  }
  *is_valid = false;
}

namespace {
class IftNativeAllocations {
 public:
  IftNativeAllocations(Handle<WasmIndirectFunctionTable> table, uint32_t size)
      : sig_ids_(size), targets_(size) {
    table->set_sig_ids(sig_ids_.data());
    table->set_targets(targets_.data());
  }

  static size_t SizeInMemory(uint32_t size) {
    return size * (sizeof(Address) + sizeof(uint32_t));
  }

  void resize(Handle<WasmIndirectFunctionTable> table, uint32_t new_size) {
    DCHECK_GE(new_size, sig_ids_.size());
    DCHECK_EQ(this, Managed<IftNativeAllocations>::cast(
                        table->managed_native_allocations())
                        .raw());
    sig_ids_.resize(new_size);
    targets_.resize(new_size);
    table->set_sig_ids(sig_ids_.data());
    table->set_targets(targets_.data());
  }

 private:
  std::vector<uint32_t> sig_ids_;
  std::vector<Address> targets_;
};
}  // namespace

Handle<WasmIndirectFunctionTable> WasmIndirectFunctionTable::New(
    Isolate* isolate, uint32_t size) {
  auto refs = isolate->factory()->NewFixedArray(static_cast<int>(size));
  auto table = Handle<WasmIndirectFunctionTable>::cast(
      isolate->factory()->NewStruct(WASM_INDIRECT_FUNCTION_TABLE_TYPE));
  table->set_size(size);
  table->set_refs(*refs);
  auto native_allocations = Managed<IftNativeAllocations>::Allocate(
      isolate, IftNativeAllocations::SizeInMemory(size), table, size);
  table->set_managed_native_allocations(*native_allocations);
  for (uint32_t i = 0; i < size; ++i) {
    IndirectFunctionTableEntry(table, static_cast<int>(i)).clear();
  }
  return table;
}

void WasmIndirectFunctionTable::Resize(Isolate* isolate,
                                       Handle<WasmIndirectFunctionTable> table,
                                       uint32_t new_size) {
  uint32_t old_size = table->size();
  if (old_size >= new_size) return;  // Nothing to do.

  Managed<IftNativeAllocations>::cast(table->managed_native_allocations())
      .raw()
      ->resize(table, new_size);

  Handle<FixedArray> old_refs(table->refs(), isolate);
  Handle<FixedArray> new_refs = isolate->factory()->CopyFixedArrayAndGrow(
      old_refs, static_cast<int>(new_size - old_size));
  table->set_refs(*new_refs);
  table->set_size(new_size);
  for (uint32_t i = old_size; i < new_size; ++i) {
    IndirectFunctionTableEntry(table, static_cast<int>(i)).clear();
  }
}

namespace {

void SetInstanceMemory(Handle<WasmInstanceObject> instance,
                       Handle<JSArrayBuffer> buffer) {
  bool is_wasm_module = instance->module()->origin == wasm::kWasmOrigin;
  bool use_trap_handler =
      instance->module_object().native_module()->use_trap_handler();
  // Wasm modules compiled to use the trap handler don't have bounds checks,
  // so they must have a memory that has guard regions.
  CHECK_IMPLIES(is_wasm_module && use_trap_handler,
                buffer->GetBackingStore()->has_guard_regions());

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
    uint32_t maximum) {
  Handle<JSArrayBuffer> buffer;
  if (!maybe_buffer.ToHandle(&buffer)) {
    // If no buffer was provided, create a zero-length one.
    auto backing_store =
        BackingStore::AllocateWasmMemory(isolate, 0, 0, SharedFlag::kNotShared);
    buffer = isolate->factory()->NewJSArrayBuffer(std::move(backing_store));
  }

  Handle<JSFunction> memory_ctor(
      isolate->native_context()->wasm_memory_constructor(), isolate);

  auto memory_object = Handle<WasmMemoryObject>::cast(
      isolate->factory()->NewJSObject(memory_ctor, AllocationType::kOld));
  memory_object->set_array_buffer(*buffer);
  memory_object->set_maximum_pages(maximum);

  if (buffer->is_shared()) {
    auto backing_store = buffer->GetBackingStore();
    backing_store->AttachSharedWasmMemoryObject(isolate, memory_object);
  }

  return memory_object;
}

MaybeHandle<WasmMemoryObject> WasmMemoryObject::New(Isolate* isolate,
                                                    uint32_t initial,
                                                    uint32_t maximum,
                                                    SharedFlag shared) {
  auto heuristic_maximum = maximum;
#ifdef V8_TARGET_ARCH_32_BIT
  // TODO(wasm): use a better heuristic for reserving more than the initial
  // number of pages on 32-bit systems. Being too greedy in reserving capacity
  // limits the number of memories that can be allocated, causing OOMs in many
  // tests. For now, on 32-bit we never reserve more than initial, unless the
  // memory is shared.
  if (shared == SharedFlag::kNotShared || !FLAG_wasm_grow_shared_memory) {
    heuristic_maximum = initial;
  }
#endif

  auto backing_store = BackingStore::AllocateWasmMemory(
      isolate, initial, heuristic_maximum, shared);

  if (!backing_store) return {};

  Handle<JSArrayBuffer> buffer =
      (shared == SharedFlag::kShared)
          ? isolate->factory()->NewJSSharedArrayBuffer(std::move(backing_store))
          : isolate->factory()->NewJSArrayBuffer(std::move(backing_store));

  return New(isolate, buffer, maximum);
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

void WasmMemoryObject::update_instances(Isolate* isolate,
                                        Handle<JSArrayBuffer> buffer) {
  if (has_instances()) {
    Handle<WeakArrayList> instances(this->instances(), isolate);
    for (int i = 0; i < instances->length(); i++) {
      MaybeObject elem = instances->Get(i);
      HeapObject heap_object;
      if (elem->GetHeapObjectIfWeak(&heap_object)) {
        Handle<WasmInstanceObject> instance(
            WasmInstanceObject::cast(heap_object), isolate);
        SetInstanceMemory(instance, buffer);
      } else {
        DCHECK(elem->IsCleared());
      }
    }
  }
  set_array_buffer(*buffer);
}

// static
int32_t WasmMemoryObject::Grow(Isolate* isolate,
                               Handle<WasmMemoryObject> memory_object,
                               uint32_t pages) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "GrowMemory");
  Handle<JSArrayBuffer> old_buffer(memory_object->array_buffer(), isolate);
  // Any buffer used as an asmjs memory cannot be detached, and
  // therefore this memory cannot be grown.
  if (old_buffer->is_asmjs_memory()) return -1;

  // Checks for maximum memory size.
  uint32_t maximum_pages = wasm::max_mem_pages();
  if (memory_object->has_maximum_pages()) {
    maximum_pages = std::min(
        maximum_pages, static_cast<uint32_t>(memory_object->maximum_pages()));
  }
  CHECK_GE(wasm::max_mem_pages(), maximum_pages);
  size_t old_size = old_buffer->byte_length();
  CHECK_EQ(0, old_size % wasm::kWasmPageSize);
  size_t old_pages = old_size / wasm::kWasmPageSize;
  CHECK_GE(wasm::max_mem_pages(), old_pages);
  if ((pages > maximum_pages - old_pages) ||          // exceeds remaining
      (pages > wasm::max_mem_pages() - old_pages)) {  // exceeds limit
    return -1;
  }
  std::shared_ptr<BackingStore> backing_store = old_buffer->GetBackingStore();
  if (!backing_store) return -1;

  // Compute new size.
  size_t new_pages = old_pages + pages;
  size_t new_byte_length = new_pages * wasm::kWasmPageSize;

  // Try to handle shared memory first.
  if (old_buffer->is_shared()) {
    if (FLAG_wasm_grow_shared_memory) {
      // Shared memories can only be grown in place; no copying.
      if (backing_store->GrowWasmMemoryInPlace(isolate, pages, maximum_pages)) {
        BackingStore::BroadcastSharedWasmMemoryGrow(isolate, backing_store,
                                                    new_pages);
        // Broadcasting the update should update this memory object too.
        CHECK_NE(*old_buffer, memory_object->array_buffer());
        CHECK_EQ(new_byte_length, memory_object->array_buffer().byte_length());
        return static_cast<int32_t>(old_pages);  // success
      }
    }
    return -1;
  }

  // Try to grow non-shared memory in-place.
  if (backing_store->GrowWasmMemoryInPlace(isolate, pages, maximum_pages)) {
    // Detach old and create a new one with the grown backing store.
    old_buffer->Detach(true);
    Handle<JSArrayBuffer> new_buffer =
        isolate->factory()->NewJSArrayBuffer(std::move(backing_store));
    memory_object->update_instances(isolate, new_buffer);
    return static_cast<int32_t>(old_pages);  // success
  }
  // Try allocating a new backing store and copying.
  std::unique_ptr<BackingStore> new_backing_store =
      backing_store->CopyWasmMemory(isolate, new_pages);
  if (!new_backing_store) return -1;

  // Detach old and create a new one with the new backing store.
  old_buffer->Detach(true);
  Handle<JSArrayBuffer> new_buffer =
      isolate->factory()->NewJSArrayBuffer(std::move(new_backing_store));
  memory_object->update_instances(isolate, new_buffer);
  return static_cast<int32_t>(old_pages);  // success
}

// static
MaybeHandle<WasmGlobalObject> WasmGlobalObject::New(
    Isolate* isolate, MaybeHandle<JSArrayBuffer> maybe_untagged_buffer,
    MaybeHandle<FixedArray> maybe_tagged_buffer, wasm::ValueType type,
    int32_t offset, bool is_mutable) {
  Handle<JSFunction> global_ctor(
      isolate->native_context()->wasm_global_constructor(), isolate);
  auto global_obj = Handle<WasmGlobalObject>::cast(
      isolate->factory()->NewJSObject(global_ctor));
  {
    // Disallow GC until all fields have acceptable types.
    DisallowHeapAllocation no_gc;

    global_obj->set_flags(0);
    global_obj->set_type(type);
    global_obj->set_offset(offset);
    global_obj->set_is_mutable(is_mutable);
  }

  if (wasm::ValueTypes::IsReferenceType(type)) {
    DCHECK(maybe_untagged_buffer.is_null());
    Handle<FixedArray> tagged_buffer;
    if (!maybe_tagged_buffer.ToHandle(&tagged_buffer)) {
      // If no buffer was provided, create one.
      tagged_buffer =
          isolate->factory()->NewFixedArray(1, AllocationType::kOld);
      CHECK_EQ(offset, 0);
    }
    global_obj->set_tagged_buffer(*tagged_buffer);
  } else {
    DCHECK(maybe_tagged_buffer.is_null());
    uint32_t type_size = wasm::ValueTypes::ElementSizeInBytes(type);

    Handle<JSArrayBuffer> untagged_buffer;
    if (!maybe_untagged_buffer.ToHandle(&untagged_buffer)) {
      MaybeHandle<JSArrayBuffer> result =
          isolate->factory()->NewJSArrayBufferAndBackingStore(
              offset + type_size, InitializedFlag::kZeroInitialized);

      if (!result.ToHandle(&untagged_buffer)) return {};
    }

    // Check that the offset is in bounds.
    CHECK_LE(offset + type_size, untagged_buffer->byte_length());

    global_obj->set_untagged_buffer(*untagged_buffer);
  }

  return global_obj;
}

void IndirectFunctionTableEntry::clear() {
  if (!instance_.is_null()) {
    instance_->indirect_function_table_sig_ids()[index_] = -1;
    instance_->indirect_function_table_targets()[index_] = 0;
    instance_->indirect_function_table_refs().set(
        index_, ReadOnlyRoots(instance_->GetIsolate()).undefined_value());
  } else {
    DCHECK(!table_.is_null());
    table_->sig_ids()[index_] = -1;
    table_->targets()[index_] = 0;
    table_->refs().set(
        index_,
        ReadOnlyRoots(GetIsolateFromWritableObject(*table_)).undefined_value());
  }
}

void IndirectFunctionTableEntry::Set(int sig_id,
                                     Handle<WasmInstanceObject> target_instance,
                                     int target_func_index) {
  TRACE_IFT("IFT entry 0x%" PRIxPTR
            "[%d] = {sig_id=%d, target_instance=0x%" PRIxPTR
            ", target_func_index=%d}\n",
            instance_->ptr(), index_, sig_id, target_instance->ptr(),
            target_func_index);

  Object ref;
  Address call_target = 0;
  if (target_func_index <
      static_cast<int>(target_instance->module()->num_imported_functions)) {
    // The function in the target instance was imported. Use its imports table,
    // which contains a tuple needed by the import wrapper.
    ImportedFunctionEntry entry(target_instance, target_func_index);
    ref = entry.object_ref();
    call_target = entry.target();
  } else {
    // The function in the target instance was not imported.
    ref = *target_instance;
    call_target = target_instance->GetCallTarget(target_func_index);
  }
  Set(sig_id, call_target, ref);
}

void IndirectFunctionTableEntry::Set(int sig_id, Address call_target,
                                     Object ref) {
  if (!instance_.is_null()) {
    instance_->indirect_function_table_sig_ids()[index_] = sig_id;
    instance_->indirect_function_table_targets()[index_] = call_target;
    instance_->indirect_function_table_refs().set(index_, ref);
  } else {
    DCHECK(!table_.is_null());
    table_->sig_ids()[index_] = sig_id;
    table_->targets()[index_] = call_target;
    table_->refs().set(index_, ref);
  }
}

Object IndirectFunctionTableEntry::object_ref() const {
  return !instance_.is_null()
             ? instance_->indirect_function_table_refs().get(index_)
             : table_->refs().get(index_);
}

int IndirectFunctionTableEntry::sig_id() const {
  return !instance_.is_null()
             ? instance_->indirect_function_table_sig_ids()[index_]
             : table_->sig_ids()[index_];
}

Address IndirectFunctionTableEntry::target() const {
  return !instance_.is_null()
             ? instance_->indirect_function_table_targets()[index_]
             : table_->targets()[index_];
}

void ImportedFunctionEntry::SetWasmToJs(
    Isolate* isolate, Handle<JSReceiver> callable,
    const wasm::WasmCode* wasm_to_js_wrapper) {
  TRACE_IFT("Import callable 0x%" PRIxPTR "[%d] = {callable=0x%" PRIxPTR
            ", target=%p}\n",
            instance_->ptr(), index_, callable->ptr(),
            wasm_to_js_wrapper->instructions().begin());
  DCHECK(wasm_to_js_wrapper->kind() == wasm::WasmCode::kWasmToJsWrapper ||
         wasm_to_js_wrapper->kind() == wasm::WasmCode::kWasmToCapiWrapper);
  Handle<Tuple2> tuple =
      isolate->factory()->NewTuple2(instance_, callable, AllocationType::kOld);
  instance_->imported_function_refs().set(index_, *tuple);
  instance_->imported_function_targets()[index_] =
      wasm_to_js_wrapper->instruction_start();
}

void ImportedFunctionEntry::SetWasmToWasm(WasmInstanceObject instance,
                                          Address call_target) {
  TRACE_IFT("Import WASM 0x%" PRIxPTR "[%d] = {instance=0x%" PRIxPTR
            ", target=0x%" PRIxPTR "}\n",
            instance_->ptr(), index_, instance.ptr(), call_target);
  instance_->imported_function_refs().set(index_, instance);
  instance_->imported_function_targets()[index_] = call_target;
}

WasmInstanceObject ImportedFunctionEntry::instance() {
  // The imported reference entry is either a target instance or a tuple
  // of this instance and the target callable.
  Object value = instance_->imported_function_refs().get(index_);
  if (value.IsWasmInstanceObject()) {
    return WasmInstanceObject::cast(value);
  }
  Tuple2 tuple = Tuple2::cast(value);
  return WasmInstanceObject::cast(tuple.value1());
}

JSReceiver ImportedFunctionEntry::callable() {
  return JSReceiver::cast(Tuple2::cast(object_ref()).value2());
}

Object ImportedFunctionEntry::object_ref() {
  return instance_->imported_function_refs().get(index_);
}

Address ImportedFunctionEntry::target() {
  return instance_->imported_function_targets()[index_];
}

// static
constexpr uint16_t WasmInstanceObject::kTaggedFieldOffsets[];

// static
bool WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
    Handle<WasmInstanceObject> instance, int table_index,
    uint32_t minimum_size) {
  Isolate* isolate = instance->GetIsolate();
  if (table_index > 0) {
    DCHECK_LT(table_index, instance->indirect_function_tables().length());
    auto table =
        handle(WasmIndirectFunctionTable::cast(
                   instance->indirect_function_tables().get(table_index)),
               isolate);
    WasmIndirectFunctionTable::Resize(isolate, table, minimum_size);
    return true;
  }
  uint32_t old_size = instance->indirect_function_table_size();
  if (old_size >= minimum_size) return false;  // Nothing to do.

  HandleScope scope(isolate);
  auto native_allocations = GetNativeAllocations(*instance);
  native_allocations->resize_indirect_function_table(isolate, instance,
                                                     minimum_size);
  return true;
}

void WasmInstanceObject::SetRawMemory(byte* mem_start, size_t mem_size) {
  CHECK_LE(mem_size, wasm::max_mem_bytes());
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
  return module_object().module();
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
      isolate->factory()->NewJSObject(instance_cons, AllocationType::kOld);

  Handle<WasmInstanceObject> instance(
      WasmInstanceObject::cast(*instance_object), isolate);
  instance->clear_padding();

  // Initialize the imported function arrays.
  auto module = module_object->module();
  auto num_imported_functions = module->num_imported_functions;
  auto num_imported_mutable_globals = module->num_imported_mutable_globals;
  auto num_data_segments = module->num_declared_data_segments;
  size_t native_allocations_size = EstimateNativeAllocationsSize(module);
  auto native_allocations = Managed<WasmInstanceNativeAllocations>::Allocate(
      isolate, native_allocations_size, instance, num_imported_functions,
      num_imported_mutable_globals, num_data_segments,
      module->elem_segments.size());
  instance->set_managed_native_allocations(*native_allocations);

  Handle<FixedArray> imported_function_refs =
      isolate->factory()->NewFixedArray(num_imported_functions);
  instance->set_imported_function_refs(*imported_function_refs);

  instance->SetRawMemory(nullptr, 0);
  instance->set_isolate_root(isolate->isolate_root());
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
  instance->set_jump_table_start(
      module_object->native_module()->jump_table_start());

  // Insert the new instance into the scripts weak list of instances. This list
  // is used for breakpoints affecting all instances belonging to the script.
  // TODO(mstarzinger): Allow to reuse holes in the {WeakArrayList} below.
  if (module_object->script().type() == Script::TYPE_WASM) {
    Handle<WeakArrayList> weak_instance_list(
        module_object->script().wasm_weak_instance_list(), isolate);
    weak_instance_list = WeakArrayList::AddToEnd(
        isolate, weak_instance_list, MaybeObjectHandle::Weak(instance));
    module_object->script().set_wasm_weak_instance_list(*weak_instance_list);
  }

  InitDataSegmentArrays(instance, module_object);
  InitElemSegmentArrays(instance, module_object);

  return instance;
}

// static
void WasmInstanceObject::InitDataSegmentArrays(
    Handle<WasmInstanceObject> instance,
    Handle<WasmModuleObject> module_object) {
  auto module = module_object->module();
  auto wire_bytes = module_object->native_module()->wire_bytes();
  auto num_data_segments = module->num_declared_data_segments;
  // The number of declared data segments will be zero if there is no DataCount
  // section. These arrays will not be allocated nor initialized in that case,
  // since they cannot be used (since the validator checks that number of
  // declared data segments when validating the memory.init and memory.drop
  // instructions).
  DCHECK(num_data_segments == 0 ||
         num_data_segments == module->data_segments.size());
  for (size_t i = 0; i < num_data_segments; ++i) {
    const wasm::WasmDataSegment& segment = module->data_segments[i];
    // Set the active segments to being already dropped, since memory.init on
    // a dropped passive segment and an active segment have the same
    // behavior.
    instance->dropped_data_segments()[i] = segment.active ? 1 : 0;

    // Initialize the pointer and size of passive segments.
    auto source_bytes = wire_bytes.SubVector(segment.source.offset(),
                                             segment.source.end_offset());
    instance->data_segment_starts()[i] =
        reinterpret_cast<Address>(source_bytes.begin());
    instance->data_segment_sizes()[i] = source_bytes.length();
  }
}

void WasmInstanceObject::InitElemSegmentArrays(
    Handle<WasmInstanceObject> instance,
    Handle<WasmModuleObject> module_object) {
  auto module = module_object->module();
  auto num_elem_segments = module->elem_segments.size();
  for (size_t i = 0; i < num_elem_segments; ++i) {
    const wasm::WasmElemSegment& segment = module->elem_segments[i];
    // Set the active segments to being already dropped, since table.init on
    // a dropped passive segment and an active segment have the same
    // behavior.
    instance->dropped_elem_segments()[i] = segment.active ? 1 : 0;
  }
}

Address WasmInstanceObject::GetCallTarget(uint32_t func_index) {
  wasm::NativeModule* native_module = module_object().native_module();
  if (func_index < native_module->num_imported_functions()) {
    return imported_function_targets()[func_index];
  }
  return native_module->GetCallTargetForFunction(func_index);
}

int WasmInstanceObject::IndirectFunctionTableSize(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    uint32_t table_index) {
  if (table_index == 0) {
    return instance->indirect_function_table_size();
  }
  auto table =
      handle(WasmIndirectFunctionTable::cast(
                 instance->indirect_function_tables().get(table_index)),
             isolate);
  return table->size();
}

// static
bool WasmInstanceObject::CopyTableEntries(Isolate* isolate,
                                          Handle<WasmInstanceObject> instance,
                                          uint32_t table_dst_index,
                                          uint32_t table_src_index,
                                          uint32_t dst, uint32_t src,
                                          uint32_t count) {
  // Copying 0 elements is a no-op.
  if (count == 0) return true;
  CHECK_LT(table_dst_index, instance->tables().length());
  CHECK_LT(table_src_index, instance->tables().length());
  auto table_dst = handle(
      WasmTableObject::cast(instance->tables().get(table_dst_index)), isolate);
  auto table_src = handle(
      WasmTableObject::cast(instance->tables().get(table_src_index)), isolate);
  uint32_t max_dst = static_cast<uint32_t>(table_dst->entries().length());
  uint32_t max_src = static_cast<uint32_t>(table_src->entries().length());
  bool copy_backward = src < dst;
  bool ok = ClampToBounds(dst, &count, max_dst);
  // Use & instead of && so the clamp is not short-circuited.
  ok &= ClampToBounds(src, &count, max_src);

  // If performing a partial copy when copying backward, then the first access
  // will be out-of-bounds, so no entries should be copied.
  if (copy_backward && !ok) return ok;

  // no-op
  if ((dst == src && table_dst_index == table_src_index) || count == 0) {
    return ok;
  }

  for (uint32_t i = 0; i < count; ++i) {
    uint32_t src_index = copy_backward ? (src + count - i - 1) : src + i;
    uint32_t dst_index = copy_backward ? (dst + count - i - 1) : dst + i;
    auto value = WasmTableObject::Get(isolate, table_src, src_index);
    WasmTableObject::Set(isolate, table_dst, dst_index, value);
  }
  return ok;
}

// static
bool WasmInstanceObject::InitTableEntries(Isolate* isolate,
                                          Handle<WasmInstanceObject> instance,
                                          uint32_t table_index,
                                          uint32_t segment_index, uint32_t dst,
                                          uint32_t src, uint32_t count) {
  // Copying 0 elements is a no-op.
  if (count == 0) return true;
  // Note that this implementation just calls through to module instantiation.
  // This is intentional, so that the runtime only depends on the object
  // methods, and not the module instantiation logic.
  return wasm::LoadElemSegment(isolate, instance, table_index, segment_index,
                               dst, src, count);
}

MaybeHandle<WasmExternalFunction> WasmInstanceObject::GetWasmExternalFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int index) {
  MaybeHandle<WasmExternalFunction> result;
  if (instance->has_wasm_external_functions()) {
    Object val = instance->wasm_external_functions().get(index);
    if (!val.IsUndefined(isolate)) {
      result = Handle<WasmExternalFunction>(WasmExternalFunction::cast(val),
                                            isolate);
    }
  }
  return result;
}

Handle<WasmExternalFunction>
WasmInstanceObject::GetOrCreateWasmExternalFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int function_index) {
  MaybeHandle<WasmExternalFunction> maybe_result =
      WasmInstanceObject::GetWasmExternalFunction(isolate, instance,
                                                  function_index);

  Handle<WasmExternalFunction> result;
  if (maybe_result.ToHandle(&result)) {
    return result;
  }

  Handle<WasmModuleObject> module_object(instance->module_object(), isolate);
  const WasmModule* module = module_object->module();
  const WasmFunction& function = module->functions[function_index];
  int wrapper_index =
      GetExportWrapperIndex(module, function.sig, function.imported);

  Handle<Object> entry =
      FixedArray::get(module_object->export_wrappers(), wrapper_index, isolate);

  Handle<Code> wrapper;
  if (entry->IsCode()) {
    wrapper = Handle<Code>::cast(entry);
  } else {
    // The wrapper may not exist yet if no function in the exports section has
    // this signature. We compile it and store the wrapper in the module for
    // later use.
    wrapper = wasm::JSToWasmWrapperCompilationUnit::CompileJSToWasmWrapper(
        isolate, function.sig, function.imported);
    module_object->export_wrappers().set(wrapper_index, *wrapper);
  }
  result = Handle<WasmExternalFunction>::cast(WasmExportedFunction::New(
      isolate, instance, function_index,
      static_cast<int>(function.sig->parameter_count()), wrapper));

  WasmInstanceObject::SetWasmExternalFunction(isolate, instance, function_index,
                                              result);
  return result;
}

void WasmInstanceObject::SetWasmExternalFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int index,
    Handle<WasmExternalFunction> val) {
  Handle<FixedArray> functions;
  if (!instance->has_wasm_external_functions()) {
    // Lazily allocate the wasm external functions array.
    functions = isolate->factory()->NewFixedArray(
        static_cast<int>(instance->module()->functions.size()));
    instance->set_wasm_external_functions(*functions);
  } else {
    functions =
        Handle<FixedArray>(instance->wasm_external_functions(), isolate);
  }
  functions->set(index, *val);
}

// static
void WasmInstanceObject::ImportWasmJSFunctionIntoTable(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int table_index,
    int entry_index, Handle<WasmJSFunction> js_function) {
  // Deserialize the signature encapsulated with the {WasmJSFunction}.
  // Note that {SignatureMap::Find} may return {-1} if the signature is
  // not found; it will simply never match any check.
  Zone zone(isolate->allocator(), ZONE_NAME);
  wasm::FunctionSig* sig = js_function->GetSignature(&zone);
  auto sig_id = instance->module()->signature_map.Find(*sig);

  // Compile a wrapper for the target callable.
  Handle<JSReceiver> callable(js_function->GetCallable(), isolate);
  wasm::WasmCodeRefScope code_ref_scope;
  Address call_target = kNullAddress;
  if (sig_id >= 0) {
    wasm::NativeModule* native_module =
        instance->module_object().native_module();
    // TODO(mstarzinger): Cache and reuse wrapper code.
    const wasm::WasmFeatures enabled = native_module->enabled_features();
    auto resolved = compiler::ResolveWasmImportCall(callable, sig, enabled);
    compiler::WasmImportCallKind kind = resolved.first;
    callable = resolved.second;  // Update to ultimate target.
    DCHECK_NE(compiler::WasmImportCallKind::kLinkError, kind);
    wasm::CompilationEnv env = native_module->CreateCompilationEnv();
    wasm::WasmCompilationResult result = compiler::CompileWasmImportCallWrapper(
        isolate->wasm_engine(), &env, kind, sig, false);
    std::unique_ptr<wasm::WasmCode> wasm_code = native_module->AddCode(
        result.func_index, result.code_desc, result.frame_slot_count,
        result.tagged_parameter_slots, std::move(result.protected_instructions),
        std::move(result.source_positions), GetCodeKind(result),
        wasm::ExecutionTier::kNone);
    wasm::WasmCode* published_code =
        native_module->PublishCode(std::move(wasm_code));
    isolate->counters()->wasm_generated_code_size()->Increment(
        published_code->instructions().length());
    isolate->counters()->wasm_reloc_size()->Increment(
        published_code->reloc_info().length());
    call_target = published_code->instruction_start();
  }

  // Update the dispatch table.
  Handle<Tuple2> tuple =
      isolate->factory()->NewTuple2(instance, callable, AllocationType::kOld);
  IndirectFunctionTableEntry(instance, table_index, entry_index)
      .Set(sig_id, call_target, *tuple);
}

// static
Handle<WasmExceptionObject> WasmExceptionObject::New(
    Isolate* isolate, const wasm::FunctionSig* sig,
    Handle<HeapObject> exception_tag) {
  Handle<JSFunction> exception_cons(
      isolate->native_context()->wasm_exception_constructor(), isolate);

  // Serialize the signature.
  DCHECK_EQ(0, sig->return_count());
  DCHECK_LE(sig->parameter_count(), std::numeric_limits<int>::max());
  int sig_size = static_cast<int>(sig->parameter_count());
  Handle<PodArray<wasm::ValueType>> serialized_sig =
      PodArray<wasm::ValueType>::New(isolate, sig_size, AllocationType::kOld);
  int index = 0;  // Index into the {PodArray} above.
  for (wasm::ValueType param : sig->parameters()) {
    serialized_sig->set(index++, param);
  }

  Handle<JSObject> exception_object =
      isolate->factory()->NewJSObject(exception_cons, AllocationType::kOld);
  Handle<WasmExceptionObject> exception =
      Handle<WasmExceptionObject>::cast(exception_object);
  exception->set_serialized_signature(*serialized_sig);
  exception->set_exception_tag(*exception_tag);

  return exception;
}

bool WasmExceptionObject::IsSignatureEqual(const wasm::FunctionSig* sig) {
  DCHECK_EQ(0, sig->return_count());
  DCHECK_LE(sig->parameter_count(), std::numeric_limits<int>::max());
  int sig_size = static_cast<int>(sig->parameter_count());
  if (sig_size != serialized_signature().length()) return false;
  for (int index = 0; index < sig_size; ++index) {
    if (sig->GetParam(index) != serialized_signature().get(index)) {
      return false;
    }
  }
  return true;
}

bool WasmCapiFunction::IsSignatureEqual(const wasm::FunctionSig* sig) const {
  // TODO(jkummerow): Unify with "SignatureHelper" in c-api.cc.
  int param_count = static_cast<int>(sig->parameter_count());
  int result_count = static_cast<int>(sig->return_count());
  PodArray<wasm::ValueType> serialized_sig =
      shared().wasm_capi_function_data().serialized_signature();
  if (param_count + result_count + 1 != serialized_sig.length()) return false;
  int serialized_index = 0;
  for (int i = 0; i < result_count; i++, serialized_index++) {
    if (sig->GetReturn(i) != serialized_sig.get(serialized_index)) {
      return false;
    }
  }
  if (serialized_sig.get(serialized_index) != wasm::kWasmStmt) return false;
  serialized_index++;
  for (int i = 0; i < param_count; i++, serialized_index++) {
    if (sig->GetParam(i) != serialized_sig.get(serialized_index)) return false;
  }
  return true;
}

// static
Handle<WasmExceptionPackage> WasmExceptionPackage::New(
    Isolate* isolate, Handle<WasmExceptionTag> exception_tag, int size) {
  Handle<Object> exception = isolate->factory()->NewWasmRuntimeError(
      MessageTemplate::kWasmExceptionError);
  CHECK(!Object::SetProperty(isolate, exception,
                             isolate->factory()->wasm_exception_tag_symbol(),
                             exception_tag, StoreOrigin::kMaybeKeyed,
                             Just(ShouldThrow::kThrowOnError))
             .is_null());
  Handle<FixedArray> values = isolate->factory()->NewFixedArray(size);
  CHECK(!Object::SetProperty(isolate, exception,
                             isolate->factory()->wasm_exception_values_symbol(),
                             values, StoreOrigin::kMaybeKeyed,
                             Just(ShouldThrow::kThrowOnError))
             .is_null());
  return Handle<WasmExceptionPackage>::cast(exception);
}

// static
Handle<Object> WasmExceptionPackage::GetExceptionTag(
    Isolate* isolate, Handle<WasmExceptionPackage> exception_package) {
  Handle<Object> tag;
  if (JSReceiver::GetProperty(isolate, exception_package,
                              isolate->factory()->wasm_exception_tag_symbol())
          .ToHandle(&tag)) {
    return tag;
  }
  return ReadOnlyRoots(isolate).undefined_value_handle();
}

// static
Handle<Object> WasmExceptionPackage::GetExceptionValues(
    Isolate* isolate, Handle<WasmExceptionPackage> exception_package) {
  Handle<Object> values;
  if (JSReceiver::GetProperty(
          isolate, exception_package,
          isolate->factory()->wasm_exception_values_symbol())
          .ToHandle(&values)) {
    DCHECK(values->IsFixedArray());
    return values;
  }
  return ReadOnlyRoots(isolate).undefined_value_handle();
}

#ifdef DEBUG

namespace {

constexpr uint32_t kBytesPerExceptionValuesArrayElement = 2;

size_t ComputeEncodedElementSize(wasm::ValueType type) {
  size_t byte_size =
      static_cast<size_t>(wasm::ValueTypes::ElementSizeInBytes(type));
  DCHECK_EQ(byte_size % kBytesPerExceptionValuesArrayElement, 0);
  DCHECK_LE(1, byte_size / kBytesPerExceptionValuesArrayElement);
  return byte_size / kBytesPerExceptionValuesArrayElement;
}

}  // namespace

#endif  // DEBUG

// static
uint32_t WasmExceptionPackage::GetEncodedSize(
    const wasm::WasmException* exception) {
  const wasm::WasmExceptionSig* sig = exception->sig;
  uint32_t encoded_size = 0;
  for (size_t i = 0; i < sig->parameter_count(); ++i) {
    switch (sig->GetParam(i)) {
      case wasm::kWasmI32:
      case wasm::kWasmF32:
        DCHECK_EQ(2, ComputeEncodedElementSize(sig->GetParam(i)));
        encoded_size += 2;
        break;
      case wasm::kWasmI64:
      case wasm::kWasmF64:
        DCHECK_EQ(4, ComputeEncodedElementSize(sig->GetParam(i)));
        encoded_size += 4;
        break;
      case wasm::kWasmS128:
        DCHECK_EQ(8, ComputeEncodedElementSize(sig->GetParam(i)));
        encoded_size += 8;
        break;
      case wasm::kWasmAnyRef:
      case wasm::kWasmFuncRef:
      case wasm::kWasmExnRef:
        encoded_size += 1;
        break;
      default:
        UNREACHABLE();
    }
  }
  return encoded_size;
}

bool WasmExportedFunction::IsWasmExportedFunction(Object object) {
  if (!object.IsJSFunction()) return false;
  JSFunction js_function = JSFunction::cast(object);
  if (Code::JS_TO_WASM_FUNCTION != js_function.code().kind()) return false;
  DCHECK(js_function.shared().HasWasmExportedFunctionData());
  return true;
}

bool WasmCapiFunction::IsWasmCapiFunction(Object object) {
  if (!object.IsJSFunction()) return false;
  JSFunction js_function = JSFunction::cast(object);
  // TODO(jkummerow): Enable this when there is a JavaScript wrapper
  // able to call this function.
  // if (js_function->code()->kind() != Code::WASM_TO_CAPI_FUNCTION) {
  //   return false;
  // }
  // DCHECK(js_function->shared()->HasWasmCapiFunctionData());
  // return true;
  return js_function.shared().HasWasmCapiFunctionData();
}

Handle<WasmCapiFunction> WasmCapiFunction::New(
    Isolate* isolate, Address call_target, Handle<Foreign> embedder_data,
    Handle<PodArray<wasm::ValueType>> serialized_signature) {
  Handle<WasmCapiFunctionData> fun_data =
      Handle<WasmCapiFunctionData>::cast(isolate->factory()->NewStruct(
          WASM_CAPI_FUNCTION_DATA_TYPE, AllocationType::kOld));
  fun_data->set_call_target(call_target);
  fun_data->set_embedder_data(*embedder_data);
  fun_data->set_serialized_signature(*serialized_signature);
  // TODO(jkummerow): Install a JavaScript wrapper. For now, calling
  // these functions directly is unsupported; they can only be called
  // from Wasm code.
  fun_data->set_wrapper_code(isolate->builtins()->builtin(Builtins::kIllegal));
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfoForWasmCapiFunction(fun_data);
  return Handle<WasmCapiFunction>::cast(
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          shared, isolate->native_context()));
}

WasmInstanceObject WasmExportedFunction::instance() {
  return shared().wasm_exported_function_data().instance();
}

int WasmExportedFunction::function_index() {
  return shared().wasm_exported_function_data().function_index();
}

Handle<WasmExportedFunction> WasmExportedFunction::New(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int func_index,
    int arity, Handle<Code> export_wrapper) {
  DCHECK_EQ(Code::JS_TO_WASM_FUNCTION, export_wrapper->kind());
  int num_imported_functions = instance->module()->num_imported_functions;
  int jump_table_offset = -1;
  if (func_index >= num_imported_functions) {
    uint32_t jump_table_diff =
        instance->module_object().native_module()->GetJumpTableOffset(
            func_index);
    DCHECK_GE(kMaxInt, jump_table_diff);
    jump_table_offset = static_cast<int>(jump_table_diff);
  }
  Handle<WasmExportedFunctionData> function_data =
      Handle<WasmExportedFunctionData>::cast(isolate->factory()->NewStruct(
          WASM_EXPORTED_FUNCTION_DATA_TYPE, AllocationType::kOld));
  function_data->set_wrapper_code(*export_wrapper);
  function_data->set_instance(*instance);
  function_data->set_jump_table_offset(jump_table_offset);
  function_data->set_function_index(func_index);
  function_data->set_c_wrapper_code(Smi::zero(), SKIP_WRITE_BARRIER);
  function_data->set_wasm_call_target(Smi::zero(), SKIP_WRITE_BARRIER);
  function_data->set_packed_args_size(0);

  MaybeHandle<String> maybe_name;
  bool is_asm_js_module = instance->module_object().is_asm_js();
  if (is_asm_js_module) {
    // We can use the function name only for asm.js. For WebAssembly, the
    // function name is specified as the function_index.toString().
    maybe_name = WasmModuleObject::GetFunctionNameOrNull(
        isolate, handle(instance->module_object(), isolate), func_index);
  }
  Handle<String> name;
  if (!maybe_name.ToHandle(&name)) {
    EmbeddedVector<char, 16> buffer;
    int length = SNPrintF(buffer, "%d", func_index);
    name = isolate->factory()
               ->NewStringFromOneByte(
                   Vector<uint8_t>::cast(buffer.SubVector(0, length)))
               .ToHandleChecked();
  }
  Handle<Map> function_map;
  switch (instance->module()->origin) {
    case wasm::kWasmOrigin:
      function_map = isolate->wasm_exported_function_map();
      break;
    case wasm::kAsmJsSloppyOrigin:
      function_map = isolate->sloppy_function_map();
      break;
    case wasm::kAsmJsStrictOrigin:
      function_map = isolate->strict_function_map();
      break;
  }
  NewFunctionArgs args =
      NewFunctionArgs::ForWasm(name, function_data, function_map);
  Handle<JSFunction> js_function = isolate->factory()->NewFunction(args);
  // According to the spec, exported functions should not have a [[Construct]]
  // method. This does not apply to functions exported from asm.js however.
  DCHECK_EQ(is_asm_js_module, js_function->IsConstructor());
  js_function->shared().set_length(arity);
  js_function->shared().set_internal_formal_parameter_count(arity);
  return Handle<WasmExportedFunction>::cast(js_function);
}

Address WasmExportedFunction::GetWasmCallTarget() {
  return instance().GetCallTarget(function_index());
}

wasm::FunctionSig* WasmExportedFunction::sig() {
  return instance().module()->functions[function_index()].sig;
}

// static
bool WasmJSFunction::IsWasmJSFunction(Object object) {
  if (!object.IsJSFunction()) return false;
  JSFunction js_function = JSFunction::cast(object);
  return js_function.shared().HasWasmJSFunctionData();
}

Handle<WasmJSFunction> WasmJSFunction::New(Isolate* isolate,
                                           wasm::FunctionSig* sig,
                                           Handle<JSReceiver> callable) {
  DCHECK_LE(sig->all().size(), kMaxInt);
  int sig_size = static_cast<int>(sig->all().size());
  int return_count = static_cast<int>(sig->return_count());
  int parameter_count = static_cast<int>(sig->parameter_count());
  Handle<PodArray<wasm::ValueType>> serialized_sig =
      PodArray<wasm::ValueType>::New(isolate, sig_size, AllocationType::kOld);
  if (sig_size > 0) {
    serialized_sig->copy_in(0, sig->all().begin(), sig_size);
  }
  // TODO(mstarzinger): Think about caching and sharing the JS-to-JS wrappers
  // per signature instead of compiling a new one for every instantiation.
  Handle<Code> wrapper_code =
      compiler::CompileJSToJSWrapper(isolate, sig).ToHandleChecked();
  Handle<WasmJSFunctionData> function_data =
      Handle<WasmJSFunctionData>::cast(isolate->factory()->NewStruct(
          WASM_JS_FUNCTION_DATA_TYPE, AllocationType::kOld));
  function_data->set_serialized_return_count(return_count);
  function_data->set_serialized_parameter_count(parameter_count);
  function_data->set_serialized_signature(*serialized_sig);
  function_data->set_callable(*callable);
  function_data->set_wrapper_code(*wrapper_code);
  Handle<String> name = isolate->factory()->Function_string();
  if (callable->IsJSFunction()) {
    name = JSFunction::GetName(Handle<JSFunction>::cast(callable));
  }
  Handle<Map> function_map = isolate->wasm_exported_function_map();
  NewFunctionArgs args =
      NewFunctionArgs::ForWasm(name, function_data, function_map);
  Handle<JSFunction> js_function = isolate->factory()->NewFunction(args);
  js_function->shared().set_internal_formal_parameter_count(parameter_count);
  return Handle<WasmJSFunction>::cast(js_function);
}

JSReceiver WasmJSFunction::GetCallable() const {
  return shared().wasm_js_function_data().callable();
}

wasm::FunctionSig* WasmJSFunction::GetSignature(Zone* zone) {
  WasmJSFunctionData function_data = shared().wasm_js_function_data();
  int sig_size = function_data.serialized_signature().length();
  wasm::ValueType* types = zone->NewArray<wasm::ValueType>(sig_size);
  if (sig_size > 0) {
    function_data.serialized_signature().copy_out(0, types, sig_size);
  }
  int return_count = function_data.serialized_return_count();
  int parameter_count = function_data.serialized_parameter_count();
  return new (zone) wasm::FunctionSig(return_count, parameter_count, types);
}

bool WasmJSFunction::MatchesSignature(wasm::FunctionSig* sig) {
  DCHECK_LE(sig->all().size(), kMaxInt);
  int sig_size = static_cast<int>(sig->all().size());
  int return_count = static_cast<int>(sig->return_count());
  int parameter_count = static_cast<int>(sig->parameter_count());
  WasmJSFunctionData function_data = shared().wasm_js_function_data();
  if (return_count != function_data.serialized_return_count() ||
      parameter_count != function_data.serialized_parameter_count()) {
    return false;
  }
  if (sig_size == 0) return true;  // Prevent undefined behavior.
  const wasm::ValueType* expected = sig->all().begin();
  return function_data.serialized_signature().matches(expected, sig_size);
}

Address WasmCapiFunction::GetHostCallTarget() const {
  return shared().wasm_capi_function_data().call_target();
}

PodArray<wasm::ValueType> WasmCapiFunction::GetSerializedSignature() const {
  return shared().wasm_capi_function_data().serialized_signature();
}

bool WasmExternalFunction::IsWasmExternalFunction(Object object) {
  return WasmExportedFunction::IsWasmExportedFunction(object) ||
         WasmJSFunction::IsWasmJSFunction(object);
}

Handle<WasmExceptionTag> WasmExceptionTag::New(Isolate* isolate, int index) {
  Handle<WasmExceptionTag> result =
      Handle<WasmExceptionTag>::cast(isolate->factory()->NewStruct(
          WASM_EXCEPTION_TAG_TYPE, AllocationType::kOld));
  result->set_index(index);
  return result;
}

Handle<AsmWasmData> AsmWasmData::New(
    Isolate* isolate, std::shared_ptr<wasm::NativeModule> native_module,
    Handle<FixedArray> export_wrappers, Handle<ByteArray> asm_js_offset_table,
    Handle<HeapNumber> uses_bitset) {
  const WasmModule* module = native_module->module();
  size_t memory_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(module) +
      wasm::WasmCodeManager::EstimateNativeModuleNonCodeSize(module);
  Handle<Managed<wasm::NativeModule>> managed_native_module =
      Managed<wasm::NativeModule>::FromSharedPtr(isolate, memory_estimate,
                                                 std::move(native_module));
  Handle<AsmWasmData> result = Handle<AsmWasmData>::cast(
      isolate->factory()->NewStruct(ASM_WASM_DATA_TYPE, AllocationType::kOld));
  result->set_managed_native_module(*managed_native_module);
  result->set_export_wrappers(*export_wrappers);
  result->set_asm_js_offset_table(*asm_js_offset_table);
  result->set_uses_bitset(*uses_bitset);
  return result;
}

#undef TRACE
#undef TRACE_IFT
}  // namespace internal
}  // namespace v8
