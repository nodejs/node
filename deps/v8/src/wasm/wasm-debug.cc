// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-debug.h"

#include <iomanip>
#include <unordered_map>

#include "src/base/optional.h"
#include "src/codegen/assembler-inl.h"
#include "src/common/assert-scope.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/debug-scopes.h"
#include "src/debug/debug.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/baseline/liftoff-register.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "src/wasm/wasm-value.h"
#include "src/zone/accounting-allocator.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

template <bool internal, typename... Args>
Handle<String> PrintFToOneByteString(Isolate* isolate, const char* format,
                                     Args... args) {
  // Maximum length of a formatted value name ("arg#%d", "local#%d",
  // "global#%d", i32 constants, i64 constants), including null character.
  static constexpr int kMaxStrLen = 21;
  EmbeddedVector<char, kMaxStrLen> value;
  int len = SNPrintF(value, format, args...);
  CHECK(len > 0 && len < value.length());
  Vector<const uint8_t> name =
      Vector<const uint8_t>::cast(value.SubVector(0, len));
  return internal
             ? isolate->factory()->InternalizeString(name)
             : isolate->factory()->NewStringFromOneByte(name).ToHandleChecked();
}

MaybeHandle<JSObject> CreateFunctionTablesObject(
    Handle<WasmInstanceObject> instance) {
  Isolate* isolate = instance->GetIsolate();
  auto tables = handle(instance->tables(), isolate);
  if (tables->length() == 0) return MaybeHandle<JSObject>();

  const char* table_label = "table%d";
  Handle<JSObject> tables_obj = isolate->factory()->NewJSObjectWithNullProto();
  for (int table_index = 0; table_index < tables->length(); ++table_index) {
    auto func_table =
        handle(WasmTableObject::cast(tables->get(table_index)), isolate);
    if (!func_table->type().is_reference_to(HeapType::kFunc)) continue;

    Handle<String> table_name;
    if (!WasmInstanceObject::GetTableNameOrNull(isolate, instance, table_index)
             .ToHandle(&table_name)) {
      table_name =
          PrintFToOneByteString<true>(isolate, table_label, table_index);
    }

    Handle<JSObject> func_table_obj =
        isolate->factory()->NewJSObjectWithNullProto();
    JSObject::AddProperty(isolate, tables_obj, table_name, func_table_obj,
                          NONE);
    for (int i = 0; i < func_table->current_length(); ++i) {
      Handle<Object> func = WasmTableObject::Get(isolate, func_table, i);
      DCHECK(!WasmCapiFunction::IsWasmCapiFunction(*func));
      if (func->IsNull(isolate)) continue;

      Handle<String> func_name;
      Handle<JSObject> func_obj =
          isolate->factory()->NewJSObjectWithNullProto();

      if (WasmExportedFunction::IsWasmExportedFunction(*func)) {
        auto target_func = Handle<WasmExportedFunction>::cast(func);
        auto target_instance = handle(target_func->instance(), isolate);
        auto module = handle(target_instance->module_object(), isolate);
        func_name = WasmModuleObject::GetFunctionName(
            isolate, module, target_func->function_index());
      } else if (WasmJSFunction::IsWasmJSFunction(*func)) {
        auto target_func = Handle<JSFunction>::cast(func);
        func_name = JSFunction::GetName(target_func);
        if (func_name->length() == 0) {
          func_name = isolate->factory()->InternalizeUtf8String("anonymous");
        }
      }
      JSObject::AddProperty(isolate, func_obj, func_name, func, NONE);
      JSObject::AddDataElement(func_table_obj, i, func_obj, NONE);
    }
  }
  return tables_obj;
}

Handle<Object> WasmValueToValueObject(Isolate* isolate, WasmValue value) {
  Handle<ByteArray> bytes;
  switch (value.type().kind()) {
    case ValueType::kI32: {
      int32_t val = value.to_i32();
      bytes = isolate->factory()->NewByteArray(sizeof(val));
      memcpy(bytes->GetDataStartAddress(), &val, sizeof(val));
      break;
    }
    case ValueType::kI64: {
      int64_t val = value.to_i64();
      bytes = isolate->factory()->NewByteArray(sizeof(val));
      memcpy(bytes->GetDataStartAddress(), &val, sizeof(val));
      break;
    }
    case ValueType::kF32: {
      float val = value.to_f32();
      bytes = isolate->factory()->NewByteArray(sizeof(val));
      memcpy(bytes->GetDataStartAddress(), &val, sizeof(val));
      break;
    }
    case ValueType::kF64: {
      double val = value.to_f64();
      bytes = isolate->factory()->NewByteArray(sizeof(val));
      memcpy(bytes->GetDataStartAddress(), &val, sizeof(val));
      break;
    }
    case ValueType::kOptRef: {
      if (value.type().is_reference_to(HeapType::kExtern)) {
        return isolate->factory()->NewWasmValue(
            static_cast<int32_t>(HeapType::kExtern), value.to_externref());
      } else {
        // TODO(7748): Implement.
        UNIMPLEMENTED();
      }
    }
    default: {
      // TODO(7748): Implement.
      UNIMPLEMENTED();
    }
  }
  return isolate->factory()->NewWasmValue(
      static_cast<int32_t>(value.type().kind()), bytes);
}

MaybeHandle<String> GetLocalNameString(Isolate* isolate,
                                       NativeModule* native_module,
                                       int func_index, int local_index) {
  WireBytesRef name_ref =
      native_module->GetDebugInfo()->GetLocalName(func_index, local_index);
  ModuleWireBytes wire_bytes{native_module->wire_bytes()};
  // Bounds were checked during decoding.
  DCHECK(wire_bytes.BoundsCheck(name_ref));
  Vector<const char> name = wire_bytes.GetNameOrNull(name_ref);
  if (name.begin() == nullptr) return {};
  return isolate->factory()->NewStringFromUtf8(name);
}

enum ReturnLocation { kAfterBreakpoint, kAfterWasmCall };

Address FindNewPC(WasmFrame* frame, WasmCode* wasm_code, int byte_offset,
                  ReturnLocation return_location) {
  Vector<const uint8_t> new_pos_table = wasm_code->source_positions();

  DCHECK_LE(0, byte_offset);

  // Find the size of the call instruction by computing the distance from the
  // source position entry to the return address.
  WasmCode* old_code = frame->wasm_code();
  int pc_offset = static_cast<int>(frame->pc() - old_code->instruction_start());
  Vector<const uint8_t> old_pos_table = old_code->source_positions();
  SourcePositionTableIterator old_it(old_pos_table);
  int call_offset = -1;
  while (!old_it.done() && old_it.code_offset() < pc_offset) {
    call_offset = old_it.code_offset();
    old_it.Advance();
  }
  DCHECK_LE(0, call_offset);
  int call_instruction_size = pc_offset - call_offset;

  // If {return_location == kAfterBreakpoint} we search for the first code
  // offset which is marked as instruction (i.e. not the breakpoint).
  // If {return_location == kAfterWasmCall} we return the last code offset
  // associated with the byte offset.
  SourcePositionTableIterator it(new_pos_table);
  while (!it.done() && it.source_position().ScriptOffset() != byte_offset) {
    it.Advance();
  }
  if (return_location == kAfterBreakpoint) {
    while (!it.is_statement()) it.Advance();
    DCHECK_EQ(byte_offset, it.source_position().ScriptOffset());
    return wasm_code->instruction_start() + it.code_offset() +
           call_instruction_size;
  }

  DCHECK_EQ(kAfterWasmCall, return_location);
  int code_offset;
  do {
    code_offset = it.code_offset();
    it.Advance();
  } while (!it.done() && it.source_position().ScriptOffset() == byte_offset);
  return wasm_code->instruction_start() + code_offset + call_instruction_size;
}

}  // namespace

void DebugSideTable::Print(std::ostream& os) const {
  os << "Debug side table (" << num_locals_ << " locals, " << entries_.size()
     << " entries):\n";
  for (auto& entry : entries_) entry.Print(os);
  os << "\n";
}

void DebugSideTable::Entry::Print(std::ostream& os) const {
  os << std::setw(6) << std::hex << pc_offset_ << std::dec << " [";
  for (auto& value : values_) {
    os << " " << value.type.name() << ":";
    switch (value.kind) {
      case kConstant:
        os << "const#" << value.i32_const;
        break;
      case kRegister:
        os << "reg#" << value.reg_code;
        break;
      case kStack:
        os << "stack#" << value.stack_offset;
        break;
    }
  }
  os << " ]\n";
}

Handle<JSObject> GetModuleScopeObject(Handle<WasmInstanceObject> instance) {
  Isolate* isolate = instance->GetIsolate();

  Handle<JSObject> module_scope_object =
      isolate->factory()->NewJSObjectWithNullProto();

  Handle<String> instance_name =
      isolate->factory()->InternalizeString(StaticCharVector("instance"));
  JSObject::AddProperty(isolate, module_scope_object, instance_name, instance,
                        NONE);

  if (instance->has_memory_object()) {
    Handle<String> name;
    // TODO(duongn): extend the logic when multiple memories are supported.
    const uint32_t memory_index = 0;
    if (!WasmInstanceObject::GetMemoryNameOrNull(isolate, instance,
                                                 memory_index)
             .ToHandle(&name)) {
      const char* label = "memory%d";
      name = PrintFToOneByteString<true>(isolate, label, memory_index);
    }
    Handle<JSArrayBuffer> memory_buffer(
        instance->memory_object().array_buffer(), isolate);
    Handle<JSTypedArray> uint8_array = isolate->factory()->NewJSTypedArray(
        kExternalUint8Array, memory_buffer, 0, memory_buffer->byte_length());
    JSObject::AddProperty(isolate, module_scope_object, name, uint8_array,
                          NONE);
  }

  Handle<JSObject> function_tables_obj;
  if (CreateFunctionTablesObject(instance).ToHandle(&function_tables_obj)) {
    Handle<String> tables_name = isolate->factory()->InternalizeString(
        StaticCharVector("function tables"));
    JSObject::AddProperty(isolate, module_scope_object, tables_name,
                          function_tables_obj, NONE);
  }

  auto& globals = instance->module()->globals;
  if (globals.size() > 0) {
    Handle<JSObject> globals_obj =
        isolate->factory()->NewJSObjectWithNullProto();
    Handle<String> globals_name =
        isolate->factory()->InternalizeString(StaticCharVector("globals"));
    JSObject::AddProperty(isolate, module_scope_object, globals_name,
                          globals_obj, NONE);

    for (uint32_t i = 0; i < globals.size(); ++i) {
      Handle<String> name;
      if (!WasmInstanceObject::GetGlobalNameOrNull(isolate, instance, i)
               .ToHandle(&name)) {
        const char* label = "global%d";
        name = PrintFToOneByteString<true>(isolate, label, i);
      }
      WasmValue value =
          WasmInstanceObject::GetGlobalValue(instance, globals[i]);
      Handle<Object> value_obj = WasmValueToValueObject(isolate, value);
      JSObject::AddProperty(isolate, globals_obj, name, value_obj, NONE);
    }
  }
  return module_scope_object;
}

class DebugInfoImpl {
 public:
  explicit DebugInfoImpl(NativeModule* native_module)
      : native_module_(native_module) {}

  int GetNumLocals(Address pc) {
    FrameInspectionScope scope(this, pc);
    if (!scope.is_inspectable()) return 0;
    return scope.debug_side_table->num_locals();
  }

  WasmValue GetLocalValue(int local, Address pc, Address fp,
                          Address debug_break_fp) {
    FrameInspectionScope scope(this, pc);
    return GetValue(scope.debug_side_table_entry, local, fp, debug_break_fp);
  }

  int GetStackDepth(Address pc) {
    FrameInspectionScope scope(this, pc);
    if (!scope.is_inspectable()) return 0;
    int num_locals = static_cast<int>(scope.debug_side_table->num_locals());
    int value_count = scope.debug_side_table_entry->num_values();
    return value_count - num_locals;
  }

  WasmValue GetStackValue(int index, Address pc, Address fp,
                          Address debug_break_fp) {
    FrameInspectionScope scope(this, pc);
    int num_locals = static_cast<int>(scope.debug_side_table->num_locals());
    int value_count = scope.debug_side_table_entry->num_values();
    if (num_locals + index >= value_count) return {};
    return GetValue(scope.debug_side_table_entry, num_locals + index, fp,
                    debug_break_fp);
  }

  Handle<JSObject> GetLocalScopeObject(Isolate* isolate, Address pc, Address fp,
                                       Address debug_break_fp) {
    FrameInspectionScope scope(this, pc);
    Handle<JSObject> local_scope_object =
        isolate->factory()->NewJSObjectWithNullProto();

    if (!scope.is_inspectable()) return local_scope_object;

    auto* module = native_module_->module();
    auto* function = &module->functions[scope.code->index()];

    // Fill parameters and locals.
    int num_locals = static_cast<int>(scope.debug_side_table->num_locals());
    DCHECK_LE(static_cast<int>(function->sig->parameter_count()), num_locals);
    for (int i = 0; i < num_locals; ++i) {
      Handle<Name> name;
      if (!GetLocalNameString(isolate, native_module_, function->func_index, i)
               .ToHandle(&name)) {
        name = PrintFToOneByteString<true>(isolate, "var%d", i);
      }
      WasmValue value =
          GetValue(scope.debug_side_table_entry, i, fp, debug_break_fp);
      Handle<Object> value_obj = WasmValueToValueObject(isolate, value);
      // {name} can be a string representation of an element index.
      LookupIterator::Key lookup_key{isolate, name};
      LookupIterator it(isolate, local_scope_object, lookup_key,
                        local_scope_object,
                        LookupIterator::OWN_SKIP_INTERCEPTOR);
      if (it.IsFound()) continue;
      Object::AddDataProperty(&it, value_obj, NONE,
                              Just(ShouldThrow::kThrowOnError),
                              StoreOrigin::kNamed)
          .Check();
    }
    return local_scope_object;
  }

  Handle<JSObject> GetStackScopeObject(Isolate* isolate, Address pc, Address fp,
                                       Address debug_break_fp) {
    FrameInspectionScope scope(this, pc);
    Handle<JSObject> stack_scope_obj =
        isolate->factory()->NewJSObjectWithNullProto();

    if (!scope.is_inspectable()) return stack_scope_obj;

    // Fill stack values.
    // Use an object without prototype instead of an Array, for nicer displaying
    // in DevTools. For Arrays, the length field and prototype is displayed,
    // which does not make too much sense here.
    int num_locals = static_cast<int>(scope.debug_side_table->num_locals());
    int value_count = scope.debug_side_table_entry->num_values();
    for (int i = num_locals; i < value_count; ++i) {
      WasmValue value =
          GetValue(scope.debug_side_table_entry, i, fp, debug_break_fp);
      Handle<Object> value_obj = WasmValueToValueObject(isolate, value);
      JSObject::AddDataElement(stack_scope_obj,
                               static_cast<uint32_t>(i - num_locals), value_obj,
                               NONE);
    }
    return stack_scope_obj;
  }

  WireBytesRef GetLocalName(int func_index, int local_index) {
    base::MutexGuard guard(&mutex_);
    if (!local_names_) {
      local_names_ = std::make_unique<LocalNames>(
          DecodeLocalNames(native_module_->wire_bytes()));
    }
    return local_names_->GetName(func_index, local_index);
  }

  WasmCode* RecompileLiftoffWithBreakpoints(int func_index,
                                            Vector<int> offsets) {
    DCHECK(!mutex_.TryLock());  // Mutex is held externally.
    // Recompile the function with Liftoff, setting the new breakpoints.
    // Not thread-safe. The caller is responsible for locking {mutex_}.
    CompilationEnv env = native_module_->CreateCompilationEnv();
    auto* function = &native_module_->module()->functions[func_index];
    Vector<const uint8_t> wire_bytes = native_module_->wire_bytes();
    FunctionBody body{function->sig, function->code.offset(),
                      wire_bytes.begin() + function->code.offset(),
                      wire_bytes.begin() + function->code.end_offset()};
    std::unique_ptr<DebugSideTable> debug_sidetable;

    ForDebugging for_debugging = offsets.size() == 1 && offsets[0] == 0
                                     ? kForStepping
                                     : kWithBreakpoints;
    Counters* counters = nullptr;
    WasmFeatures unused_detected;
    WasmCompilationResult result = ExecuteLiftoffCompilation(
        native_module_->engine()->allocator(), &env, body, func_index,
        for_debugging, counters, &unused_detected, offsets, &debug_sidetable);
    // Liftoff compilation failure is a FATAL error. We rely on complete Liftoff
    // support for debugging.
    if (!result.succeeded()) FATAL("Liftoff compilation failed");
    DCHECK_NOT_NULL(debug_sidetable);

    WasmCode* new_code = native_module_->PublishCode(
        native_module_->AddCompiledCode(std::move(result)));

    DCHECK(new_code->is_inspectable());
    DCHECK_EQ(0, debug_side_tables_.count(new_code));
    debug_side_tables_.emplace(new_code, std::move(debug_sidetable));

    return new_code;
  }

  void SetBreakpoint(int func_index, int offset, Isolate* isolate) {
    // Put the code ref scope outside of the mutex, so we don't unnecessarily
    // hold the mutex while freeing code.
    WasmCodeRefScope wasm_code_ref_scope;

    // Hold the mutex while modifying breakpoints, to ensure consistency when
    // multiple isolates set/remove breakpoints at the same time.
    base::MutexGuard guard(&mutex_);

    // offset == 0 indicates flooding and should not happen here.
    DCHECK_NE(0, offset);

    // Get the set of previously set breakpoints, to check later whether a new
    // breakpoint was actually added.
    std::vector<int> all_breakpoints = FindAllBreakpoints(func_index);

    auto& isolate_data = per_isolate_data_[isolate];
    std::vector<int>& breakpoints =
        isolate_data.breakpoints_per_function[func_index];
    auto insertion_point =
        std::lower_bound(breakpoints.begin(), breakpoints.end(), offset);
    if (insertion_point != breakpoints.end() && *insertion_point == offset) {
      // The breakpoint is already set for this isolate.
      return;
    }
    breakpoints.insert(insertion_point, offset);

    DCHECK(std::is_sorted(all_breakpoints.begin(), all_breakpoints.end()));
    // Find the insertion position within {all_breakpoints}.
    insertion_point = std::lower_bound(all_breakpoints.begin(),
                                       all_breakpoints.end(), offset);
    bool breakpoint_exists =
        insertion_point != all_breakpoints.end() && *insertion_point == offset;
    // If the breakpoint was already set before, then we can just reuse the old
    // code. Otherwise, recompile it. In any case, rewrite this isolate's stack
    // to make sure that it uses up-to-date code containing the breakpoint.
    WasmCode* new_code;
    if (breakpoint_exists) {
      new_code = native_module_->GetCode(func_index);
    } else {
      all_breakpoints.insert(insertion_point, offset);
      new_code = RecompileLiftoffWithBreakpoints(func_index,
                                                 VectorOf(all_breakpoints));
    }
    UpdateReturnAddresses(isolate, new_code, isolate_data.stepping_frame);
  }

  std::vector<int> FindAllBreakpoints(int func_index) {
    DCHECK(!mutex_.TryLock());  // Mutex must be held externally.
    std::set<int> breakpoints;
    for (auto& data : per_isolate_data_) {
      auto it = data.second.breakpoints_per_function.find(func_index);
      if (it == data.second.breakpoints_per_function.end()) continue;
      for (int offset : it->second) breakpoints.insert(offset);
    }
    return {breakpoints.begin(), breakpoints.end()};
  }

  void UpdateBreakpoints(int func_index, Vector<int> breakpoints,
                         Isolate* isolate, StackFrameId stepping_frame) {
    DCHECK(!mutex_.TryLock());  // Mutex is held externally.
    WasmCode* new_code =
        RecompileLiftoffWithBreakpoints(func_index, breakpoints);
    UpdateReturnAddresses(isolate, new_code, stepping_frame);
  }

  void FloodWithBreakpoints(WasmFrame* frame, ReturnLocation return_location) {
    // 0 is an invalid offset used to indicate flooding.
    int offset = 0;
    WasmCodeRefScope wasm_code_ref_scope;
    DCHECK(frame->wasm_code()->is_liftoff());
    // Generate an additional source position for the current byte offset.
    base::MutexGuard guard(&mutex_);
    WasmCode* new_code = RecompileLiftoffWithBreakpoints(
        frame->function_index(), VectorOf(&offset, 1));
    UpdateReturnAddress(frame, new_code, return_location);
  }

  void PrepareStep(Isolate* isolate, StackFrameId break_frame_id) {
    StackTraceFrameIterator it(isolate, break_frame_id);
    DCHECK(!it.done());
    DCHECK(it.frame()->is_wasm());
    WasmFrame* frame = WasmFrame::cast(it.frame());
    StepAction step_action = isolate->debug()->last_step_action();

    // If we are flooding the top frame, the return location is after a
    // breakpoints. Otherwise, it's after a call.
    ReturnLocation return_location = kAfterBreakpoint;

    // If we are at a return instruction, then any stepping action is equivalent
    // to StepOut, and we need to flood the parent function.
    if (IsAtReturn(frame) || step_action == StepOut) {
      it.Advance();
      if (it.done() || !it.frame()->is_wasm()) return;
      frame = WasmFrame::cast(it.frame());
      return_location = kAfterWasmCall;
    }

    FloodWithBreakpoints(frame, return_location);

    base::MutexGuard guard(&mutex_);
    per_isolate_data_[isolate].stepping_frame = frame->id();
  }

  void ClearStepping(Isolate* isolate) {
    base::MutexGuard guard(&mutex_);
    auto it = per_isolate_data_.find(isolate);
    if (it != per_isolate_data_.end()) it->second.stepping_frame = NO_ID;
  }

  bool IsStepping(WasmFrame* frame) {
    Isolate* isolate = frame->wasm_instance().GetIsolate();
    if (isolate->debug()->last_step_action() == StepIn) return true;
    base::MutexGuard guard(&mutex_);
    auto it = per_isolate_data_.find(isolate);
    return it != per_isolate_data_.end() &&
           it->second.stepping_frame == frame->id();
  }

  void RemoveBreakpoint(int func_index, int position, Isolate* isolate) {
    // TODO(thibaudm): Cannot remove the breakpoint we are currently paused at,
    // because the new code would be missing the call instruction and the
    // corresponding source position that we rely on for OSR.
    StackTraceFrameIterator it(isolate);
    if (!it.done() && it.is_wasm()) {
      WasmFrame* frame = WasmFrame::cast(it.frame());
      if (static_cast<int32_t>(frame->function_index()) == func_index &&
          frame->position() == position) {
        return;
      }
    }

    // Put the code ref scope outside of the mutex, so we don't unnecessarily
    // hold the mutex while freeing code.
    WasmCodeRefScope wasm_code_ref_scope;

    // Hold the mutex while modifying breakpoints, to ensure consistency when
    // multiple isolates set/remove breakpoints at the same time.
    base::MutexGuard guard(&mutex_);

    const auto& function = native_module_->module()->functions[func_index];
    int offset = position - function.code.offset();

    auto& isolate_data = per_isolate_data_[isolate];
    std::vector<int>& breakpoints =
        isolate_data.breakpoints_per_function[func_index];
    DCHECK_LT(0, offset);
    auto insertion_point =
        std::lower_bound(breakpoints.begin(), breakpoints.end(), offset);
    if (insertion_point == breakpoints.end()) return;
    if (*insertion_point != offset) return;
    breakpoints.erase(insertion_point);

    std::vector<int> remaining = FindAllBreakpoints(func_index);
    // If the breakpoint is still set in another isolate, don't remove it.
    DCHECK(std::is_sorted(remaining.begin(), remaining.end()));
    if (std::binary_search(remaining.begin(), remaining.end(), offset)) return;
    UpdateBreakpoints(func_index, VectorOf(remaining), isolate,
                      isolate_data.stepping_frame);
  }

  void RemoveDebugSideTables(Vector<WasmCode* const> codes) {
    base::MutexGuard guard(&mutex_);
    for (auto* code : codes) {
      debug_side_tables_.erase(code);
    }
  }

  DebugSideTable* GetDebugSideTableIfExists(const WasmCode* code) const {
    base::MutexGuard guard(&mutex_);
    auto it = debug_side_tables_.find(code);
    return it == debug_side_tables_.end() ? nullptr : it->second.get();
  }

  static bool HasRemovedBreakpoints(const std::vector<int>& removed,
                                    const std::vector<int>& remaining) {
    DCHECK(std::is_sorted(remaining.begin(), remaining.end()));
    for (int offset : removed) {
      // Return true if we removed a breakpoint which is not part of remaining.
      if (!std::binary_search(remaining.begin(), remaining.end(), offset)) {
        return true;
      }
    }
    return false;
  }

  void RemoveIsolate(Isolate* isolate) {
    // Put the code ref scope outside of the mutex, so we don't unnecessarily
    // hold the mutex while freeing code.
    WasmCodeRefScope wasm_code_ref_scope;

    base::MutexGuard guard(&mutex_);
    auto per_isolate_data_it = per_isolate_data_.find(isolate);
    if (per_isolate_data_it == per_isolate_data_.end()) return;
    std::unordered_map<int, std::vector<int>> removed_per_function =
        std::move(per_isolate_data_it->second.breakpoints_per_function);
    per_isolate_data_.erase(per_isolate_data_it);
    for (auto& entry : removed_per_function) {
      int func_index = entry.first;
      std::vector<int>& removed = entry.second;
      std::vector<int> remaining = FindAllBreakpoints(func_index);
      if (HasRemovedBreakpoints(removed, remaining)) {
        RecompileLiftoffWithBreakpoints(func_index, VectorOf(remaining));
      }
    }
  }

 private:
  struct FrameInspectionScope {
    FrameInspectionScope(DebugInfoImpl* debug_info, Address pc)
        : code(debug_info->native_module_->engine()->code_manager()->LookupCode(
              pc)),
          pc_offset(static_cast<int>(pc - code->instruction_start())),
          debug_side_table(
              code->is_inspectable()
                  ? debug_info->GetDebugSideTable(
                        code, debug_info->native_module_->engine()->allocator())
                  : nullptr),
          debug_side_table_entry(debug_side_table
                                     ? debug_side_table->GetEntry(pc_offset)
                                     : nullptr) {
      DCHECK_IMPLIES(code->is_inspectable(), debug_side_table_entry != nullptr);
    }

    bool is_inspectable() const { return debug_side_table_entry; }

    wasm::WasmCodeRefScope wasm_code_ref_scope;
    wasm::WasmCode* code;
    int pc_offset;
    const DebugSideTable* debug_side_table;
    const DebugSideTable::Entry* debug_side_table_entry;
  };

  const DebugSideTable* GetDebugSideTable(WasmCode* code,
                                          AccountingAllocator* allocator) {
    DCHECK(code->is_inspectable());
    {
      // Only hold the mutex temporarily. We can't hold it while generating the
      // debug side table, because compilation takes the {NativeModule} lock.
      base::MutexGuard guard(&mutex_);
      auto it = debug_side_tables_.find(code);
      if (it != debug_side_tables_.end()) return it->second.get();
    }

    // Otherwise create the debug side table now.
    auto* module = native_module_->module();
    auto* function = &module->functions[code->index()];
    ModuleWireBytes wire_bytes{native_module_->wire_bytes()};
    Vector<const byte> function_bytes = wire_bytes.GetFunctionBytes(function);
    CompilationEnv env = native_module_->CreateCompilationEnv();
    FunctionBody func_body{function->sig, 0, function_bytes.begin(),
                           function_bytes.end()};
    std::unique_ptr<DebugSideTable> debug_side_table =
        GenerateLiftoffDebugSideTable(allocator, &env, func_body,
                                      code->index());
    DebugSideTable* ret = debug_side_table.get();

    // Check cache again, maybe another thread concurrently generated a debug
    // side table already.
    {
      base::MutexGuard guard(&mutex_);
      auto& slot = debug_side_tables_[code];
      if (slot != nullptr) return slot.get();
      slot = std::move(debug_side_table);
    }

    // Print the code together with the debug table, if requested.
    code->MaybePrint();
    return ret;
  }

  // Get the value of a local (including parameters) or stack value. Stack
  // values follow the locals in the same index space.
  WasmValue GetValue(const DebugSideTable::Entry* debug_side_table_entry,
                     int index, Address stack_frame_base,
                     Address debug_break_fp) const {
    ValueType type = debug_side_table_entry->value_type(index);
    if (debug_side_table_entry->is_constant(index)) {
      DCHECK(type == kWasmI32 || type == kWasmI64);
      return type == kWasmI32
                 ? WasmValue(debug_side_table_entry->i32_constant(index))
                 : WasmValue(
                       int64_t{debug_side_table_entry->i32_constant(index)});
    }

    if (debug_side_table_entry->is_register(index)) {
      LiftoffRegister reg = LiftoffRegister::from_liftoff_code(
          debug_side_table_entry->register_code(index));
      auto gp_addr = [debug_break_fp](Register reg) {
        return debug_break_fp +
               WasmDebugBreakFrameConstants::GetPushedGpRegisterOffset(
                   reg.code());
      };
      if (reg.is_gp_pair()) {
        DCHECK_EQ(kWasmI64, type);
        uint32_t low_word = ReadUnalignedValue<uint32_t>(gp_addr(reg.low_gp()));
        uint32_t high_word =
            ReadUnalignedValue<uint32_t>(gp_addr(reg.high_gp()));
        return WasmValue((uint64_t{high_word} << 32) | low_word);
      }
      if (reg.is_gp()) {
        return type == kWasmI32
                   ? WasmValue(ReadUnalignedValue<uint32_t>(gp_addr(reg.gp())))
                   : WasmValue(ReadUnalignedValue<uint64_t>(gp_addr(reg.gp())));
      }
      // TODO(clemensb/zhin): Fix this for SIMD.
      DCHECK(reg.is_fp() || reg.is_fp_pair());
      if (reg.is_fp_pair()) UNIMPLEMENTED();
      Address spilled_addr =
          debug_break_fp +
          WasmDebugBreakFrameConstants::GetPushedFpRegisterOffset(
              reg.fp().code());
      return type == kWasmF32
                 ? WasmValue(ReadUnalignedValue<float>(spilled_addr))
                 : WasmValue(ReadUnalignedValue<double>(spilled_addr));
    }

    // Otherwise load the value from the stack.
    Address stack_address =
        stack_frame_base - debug_side_table_entry->stack_offset(index);
    switch (type.kind()) {
      case ValueType::kI32:
        return WasmValue(ReadUnalignedValue<int32_t>(stack_address));
      case ValueType::kI64:
        return WasmValue(ReadUnalignedValue<int64_t>(stack_address));
      case ValueType::kF32:
        return WasmValue(ReadUnalignedValue<float>(stack_address));
      case ValueType::kF64:
        return WasmValue(ReadUnalignedValue<double>(stack_address));
      default:
        UNIMPLEMENTED();
    }
  }

  // After installing a Liftoff code object with a different set of breakpoints,
  // update return addresses on the stack so that execution resumes in the new
  // code. The frame layout itself should be independent of breakpoints.
  void UpdateReturnAddresses(Isolate* isolate, WasmCode* new_code,
                             StackFrameId stepping_frame) {
    // The first return location is after the breakpoint, others are after wasm
    // calls.
    ReturnLocation return_location = kAfterBreakpoint;
    for (StackTraceFrameIterator it(isolate); !it.done();
         it.Advance(), return_location = kAfterWasmCall) {
      // We still need the flooded function for stepping.
      if (it.frame()->id() == stepping_frame) continue;
      if (!it.is_wasm()) continue;
      WasmFrame* frame = WasmFrame::cast(it.frame());
      if (frame->native_module() != new_code->native_module()) continue;
      if (frame->function_index() != new_code->index()) continue;
      if (!frame->wasm_code()->is_liftoff()) continue;
      UpdateReturnAddress(frame, new_code, return_location);
    }
  }

  void UpdateReturnAddress(WasmFrame* frame, WasmCode* new_code,
                           ReturnLocation return_location) {
    DCHECK(new_code->is_liftoff());
    DCHECK_EQ(frame->function_index(), new_code->index());
    DCHECK_EQ(frame->native_module(), new_code->native_module());
    DCHECK(frame->wasm_code()->is_liftoff());
#ifdef DEBUG
    int old_position = frame->position();
#endif
    Address new_pc =
        FindNewPC(frame, new_code, frame->byte_offset(), return_location);
    PointerAuthentication::ReplacePC(frame->pc_address(), new_pc,
                                     kSystemPointerSize);
    // The frame position should still be the same after OSR.
    DCHECK_EQ(old_position, frame->position());
  }

  bool IsAtReturn(WasmFrame* frame) {
    DisallowHeapAllocation no_gc;
    int position = frame->position();
    NativeModule* native_module =
        frame->wasm_instance().module_object().native_module();
    uint8_t opcode = native_module->wire_bytes()[position];
    if (opcode == kExprReturn) return true;
    // Another implicit return is at the last kExprEnd in the function body.
    int func_index = frame->function_index();
    WireBytesRef code = native_module->module()->functions[func_index].code;
    return static_cast<size_t>(position) == code.end_offset() - 1;
  }

  // Isolate-specific data, for debugging modules that are shared by multiple
  // isolates.
  struct PerIsolateDebugData {
    // Keeps track of the currently set breakpoints (by offset within that
    // function).
    std::unordered_map<int, std::vector<int>> breakpoints_per_function;

    // Store the frame ID when stepping, to avoid overwriting that frame when
    // setting or removing a breakpoint.
    StackFrameId stepping_frame = NO_ID;
  };

  NativeModule* const native_module_;

  // {mutex_} protects all fields below.
  mutable base::Mutex mutex_;

  // DebugSideTable per code object, lazily initialized.
  std::unordered_map<const WasmCode*, std::unique_ptr<DebugSideTable>>
      debug_side_tables_;

  // Names of locals, lazily decoded from the wire bytes.
  std::unique_ptr<LocalNames> local_names_;

  // Isolate-specific data.
  std::unordered_map<Isolate*, PerIsolateDebugData> per_isolate_data_;

  DISALLOW_COPY_AND_ASSIGN(DebugInfoImpl);
};

DebugInfo::DebugInfo(NativeModule* native_module)
    : impl_(std::make_unique<DebugInfoImpl>(native_module)) {}

DebugInfo::~DebugInfo() = default;

int DebugInfo::GetNumLocals(Address pc) { return impl_->GetNumLocals(pc); }

WasmValue DebugInfo::GetLocalValue(int local, Address pc, Address fp,
                                   Address debug_break_fp) {
  return impl_->GetLocalValue(local, pc, fp, debug_break_fp);
}

int DebugInfo::GetStackDepth(Address pc) { return impl_->GetStackDepth(pc); }

WasmValue DebugInfo::GetStackValue(int index, Address pc, Address fp,
                                   Address debug_break_fp) {
  return impl_->GetStackValue(index, pc, fp, debug_break_fp);
}

Handle<JSObject> DebugInfo::GetLocalScopeObject(Isolate* isolate, Address pc,
                                                Address fp,
                                                Address debug_break_fp) {
  return impl_->GetLocalScopeObject(isolate, pc, fp, debug_break_fp);
}

Handle<JSObject> DebugInfo::GetStackScopeObject(Isolate* isolate, Address pc,
                                                Address fp,
                                                Address debug_break_fp) {
  return impl_->GetStackScopeObject(isolate, pc, fp, debug_break_fp);
}

WireBytesRef DebugInfo::GetLocalName(int func_index, int local_index) {
  return impl_->GetLocalName(func_index, local_index);
}

void DebugInfo::SetBreakpoint(int func_index, int offset,
                              Isolate* current_isolate) {
  impl_->SetBreakpoint(func_index, offset, current_isolate);
}

void DebugInfo::PrepareStep(Isolate* isolate, StackFrameId break_frame_id) {
  impl_->PrepareStep(isolate, break_frame_id);
}

void DebugInfo::ClearStepping(Isolate* isolate) {
  impl_->ClearStepping(isolate);
}

bool DebugInfo::IsStepping(WasmFrame* frame) {
  return impl_->IsStepping(frame);
}

void DebugInfo::RemoveBreakpoint(int func_index, int offset,
                                 Isolate* current_isolate) {
  impl_->RemoveBreakpoint(func_index, offset, current_isolate);
}

void DebugInfo::RemoveDebugSideTables(Vector<WasmCode* const> code) {
  impl_->RemoveDebugSideTables(code);
}

DebugSideTable* DebugInfo::GetDebugSideTableIfExists(
    const WasmCode* code) const {
  return impl_->GetDebugSideTableIfExists(code);
}

void DebugInfo::RemoveIsolate(Isolate* isolate) {
  return impl_->RemoveIsolate(isolate);
}

}  // namespace wasm

namespace {

// Return the next breakable position at or after {offset_in_func} in function
// {func_index}, or 0 if there is none.
// Note that 0 is never a breakable position in wasm, since the first byte
// contains the locals count for the function.
int FindNextBreakablePosition(wasm::NativeModule* native_module, int func_index,
                              int offset_in_func) {
  AccountingAllocator alloc;
  Zone tmp(&alloc, ZONE_NAME);
  wasm::BodyLocalDecls locals(&tmp);
  const byte* module_start = native_module->wire_bytes().begin();
  const wasm::WasmFunction& func =
      native_module->module()->functions[func_index];
  wasm::BytecodeIterator iterator(module_start + func.code.offset(),
                                  module_start + func.code.end_offset(),
                                  &locals);
  DCHECK_LT(0, locals.encoded_size);
  if (offset_in_func < 0) return 0;
  for (; iterator.has_next(); iterator.next()) {
    if (iterator.pc_offset() < static_cast<uint32_t>(offset_in_func)) continue;
    if (!wasm::WasmOpcodes::IsBreakable(iterator.current())) continue;
    return static_cast<int>(iterator.pc_offset());
  }
  return 0;
}

}  // namespace

// static
bool WasmScript::SetBreakPoint(Handle<Script> script, int* position,
                               Handle<BreakPoint> break_point) {
  // Find the function for this breakpoint.
  const wasm::WasmModule* module = script->wasm_native_module()->module();
  int func_index = GetContainingWasmFunction(module, *position);
  if (func_index < 0) return false;
  const wasm::WasmFunction& func = module->functions[func_index];
  int offset_in_func = *position - func.code.offset();

  int breakable_offset = FindNextBreakablePosition(script->wasm_native_module(),
                                                   func_index, offset_in_func);
  if (breakable_offset == 0) return false;
  *position = func.code.offset() + breakable_offset;

  return WasmScript::SetBreakPointForFunction(script, func_index,
                                              breakable_offset, break_point);
}

// static
bool WasmScript::SetBreakPointOnFirstBreakableForFunction(
    Handle<Script> script, int func_index, Handle<BreakPoint> break_point) {
  if (func_index < 0) return false;
  int offset_in_func = 0;

  int breakable_offset = FindNextBreakablePosition(script->wasm_native_module(),
                                                   func_index, offset_in_func);
  if (breakable_offset == 0) return false;
  return WasmScript::SetBreakPointForFunction(script, func_index,
                                              breakable_offset, break_point);
}

// static
bool WasmScript::SetBreakPointForFunction(Handle<Script> script, int func_index,
                                          int offset,
                                          Handle<BreakPoint> break_point) {
  Isolate* isolate = script->GetIsolate();

  DCHECK_LE(0, func_index);
  DCHECK_NE(0, offset);

  // Find the function for this breakpoint.
  wasm::NativeModule* native_module = script->wasm_native_module();
  const wasm::WasmModule* module = native_module->module();
  const wasm::WasmFunction& func = module->functions[func_index];

  // Insert new break point into {wasm_breakpoint_infos} of the script.
  WasmScript::AddBreakpointToInfo(script, func.code.offset() + offset,
                                  break_point);

  native_module->GetDebugInfo()->SetBreakpoint(func_index, offset, isolate);

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
bool WasmScript::ClearBreakPoint(Handle<Script> script, int position,
                                 Handle<BreakPoint> break_point) {
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

  // Remove the breakpoint from DebugInfo and recompile.
  wasm::NativeModule* native_module = script->wasm_native_module();
  const wasm::WasmModule* module = native_module->module();
  int func_index = GetContainingWasmFunction(module, position);
  native_module->GetDebugInfo()->RemoveBreakpoint(func_index, position,
                                                  isolate);

  return true;
}

// static
bool WasmScript::ClearBreakPointById(Handle<Script> script, int breakpoint_id) {
  if (!script->has_wasm_breakpoint_infos()) {
    return false;
  }
  Isolate* isolate = script->GetIsolate();
  Handle<FixedArray> breakpoint_infos(script->wasm_breakpoint_infos(), isolate);
  // If the array exists, it should not be empty.
  DCHECK_LT(0, breakpoint_infos->length());

  for (int i = 0, e = breakpoint_infos->length(); i < e; ++i) {
    Handle<Object> obj(breakpoint_infos->get(i), isolate);
    if (obj->IsUndefined(isolate)) {
      continue;
    }
    Handle<BreakPointInfo> breakpoint_info = Handle<BreakPointInfo>::cast(obj);
    Handle<BreakPoint> breakpoint;
    if (BreakPointInfo::GetBreakPointById(isolate, breakpoint_info,
                                          breakpoint_id)
            .ToHandle(&breakpoint)) {
      DCHECK(breakpoint->id() == breakpoint_id);
      return WasmScript::ClearBreakPoint(
          script, breakpoint_info->source_position(), breakpoint);
    }
  }
  return false;
}

// static
void WasmScript::ClearAllBreakpoints(Script script) {
  script.set_wasm_breakpoint_infos(
      ReadOnlyRoots(script.GetIsolate()).empty_fixed_array());
}

// static
void WasmScript::AddBreakpointToInfo(Handle<Script> script, int position,
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
bool WasmScript::GetPossibleBreakpoints(
    wasm::NativeModule* native_module, const v8::debug::Location& start,
    const v8::debug::Location& end,
    std::vector<v8::debug::BreakLocation>* locations) {
  DisallowHeapAllocation no_gc;

  const wasm::WasmModule* module = native_module->module();
  const std::vector<wasm::WasmFunction>& functions = module->functions;

  if (start.GetLineNumber() != 0 || start.GetColumnNumber() < 0 ||
      (!end.IsEmpty() &&
       (end.GetLineNumber() != 0 || end.GetColumnNumber() < 0 ||
        end.GetColumnNumber() < start.GetColumnNumber())))
    return false;

  // start_func_index, start_offset and end_func_index is inclusive.
  // end_offset is exclusive.
  // start_offset and end_offset are module-relative byte offsets.
  // We set strict to false because offsets may be between functions.
  int start_func_index =
      GetNearestWasmFunction(module, start.GetColumnNumber());
  if (start_func_index < 0) return false;
  uint32_t start_offset = start.GetColumnNumber();
  int end_func_index;
  uint32_t end_offset;

  if (end.IsEmpty()) {
    // Default: everything till the end of the Script.
    end_func_index = static_cast<uint32_t>(functions.size() - 1);
    end_offset = functions[end_func_index].code.end_offset();
  } else {
    // If end is specified: Use it and check for valid input.
    end_offset = end.GetColumnNumber();
    end_func_index = GetNearestWasmFunction(module, end_offset);
    DCHECK_GE(end_func_index, start_func_index);
  }

  if (start_func_index == end_func_index &&
      start_offset > functions[end_func_index].code.end_offset())
    return false;
  AccountingAllocator alloc;
  Zone tmp(&alloc, ZONE_NAME);
  const byte* module_start = native_module->wire_bytes().begin();

  for (int func_idx = start_func_index; func_idx <= end_func_index;
       ++func_idx) {
    const wasm::WasmFunction& func = functions[func_idx];
    if (func.code.length() == 0) continue;

    wasm::BodyLocalDecls locals(&tmp);
    wasm::BytecodeIterator iterator(module_start + func.code.offset(),
                                    module_start + func.code.end_offset(),
                                    &locals);
    DCHECK_LT(0u, locals.encoded_size);
    for (; iterator.has_next(); iterator.next()) {
      uint32_t total_offset = func.code.offset() + iterator.pc_offset();
      if (total_offset >= end_offset) {
        DCHECK_EQ(end_func_index, func_idx);
        break;
      }
      if (total_offset < start_offset) continue;
      if (!wasm::WasmOpcodes::IsBreakable(iterator.current())) continue;
      locations->emplace_back(0, total_offset, debug::kCommonBreakLocation);
    }
  }
  return true;
}

// static
MaybeHandle<FixedArray> WasmScript::CheckBreakPoints(Isolate* isolate,
                                                     Handle<Script> script,
                                                     int position) {
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

}  // namespace internal
}  // namespace v8
