// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-debug.h"

#include <iomanip>
#include <unordered_map>

#include "src/api/api-inl.h"
#include "src/api/api-natives.h"
#include "src/base/optional.h"
#include "src/base/platform/wrappers.h"
#include "src/codegen/assembler-inl.h"
#include "src/common/assert-scope.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/debug-scopes.h"
#include "src/debug/debug.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/js-collection-inl.h"
#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/baseline/liftoff-register.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "src/wasm/wasm-subtyping.h"
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

Handle<Object> WasmValueToValueObject(Isolate* isolate, WasmValue value) {
  Handle<ByteArray> bytes;
  switch (value.type().kind()) {
    case ValueType::kI32: {
      int32_t val = value.to_i32();
      bytes = isolate->factory()->NewByteArray(sizeof(val));
      base::Memcpy(bytes->GetDataStartAddress(), &val, sizeof(val));
      break;
    }
    case ValueType::kI64: {
      int64_t val = value.to_i64();
      bytes = isolate->factory()->NewByteArray(sizeof(val));
      base::Memcpy(bytes->GetDataStartAddress(), &val, sizeof(val));
      break;
    }
    case ValueType::kF32: {
      float val = value.to_f32();
      bytes = isolate->factory()->NewByteArray(sizeof(val));
      base::Memcpy(bytes->GetDataStartAddress(), &val, sizeof(val));
      break;
    }
    case ValueType::kF64: {
      double val = value.to_f64();
      bytes = isolate->factory()->NewByteArray(sizeof(val));
      base::Memcpy(bytes->GetDataStartAddress(), &val, sizeof(val));
      break;
    }
    case ValueType::kS128: {
      Simd128 s128 = value.to_s128();
      bytes = isolate->factory()->NewByteArray(kSimd128Size);
      base::Memcpy(bytes->GetDataStartAddress(), s128.bytes(), kSimd128Size);
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
  WasmName name = wire_bytes.GetNameOrNull(name_ref);
  if (name.size() == 0) return {};
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

  Handle<WasmModuleObject> module_object(instance->module_object(), isolate);
  Handle<String> module_name =
      isolate->factory()->InternalizeString(StaticCharVector("module"));
  JSObject::AddProperty(isolate, module_scope_object, module_name,
                        module_object, NONE);

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
    Handle<WasmMemoryObject> memory_object(instance->memory_object(), isolate);
    JSObject::AddProperty(isolate, module_scope_object, name, memory_object,
                          NONE);
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
      LookupIterator it(isolate, globals_obj, name, globals_obj,
                        LookupIterator::OWN_SKIP_INTERCEPTOR);
      JSObject::CreateDataProperty(&it, value_obj).Check();
    }
  }
  return module_scope_object;
}

class DebugInfoImpl {
 public:
  explicit DebugInfoImpl(NativeModule* native_module)
      : native_module_(native_module) {}

  DebugInfoImpl(const DebugInfoImpl&) = delete;
  DebugInfoImpl& operator=(const DebugInfoImpl&) = delete;

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

  const WasmFunction& GetFunctionAtAddress(Address pc) {
    FrameInspectionScope scope(this, pc);
    auto* module = native_module_->module();
    return module->functions[scope.code->index()];
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

  // If the top frame is a Wasm frame and its position is not in the list of
  // breakpoints, return that position. Return 0 otherwise.
  // This is used to generate a "dead breakpoint" in Liftoff, which is necessary
  // for OSR to find the correct return address.
  int DeadBreakpoint(int func_index, std::vector<int>& breakpoints,
                     Isolate* isolate) {
    StackTraceFrameIterator it(isolate);
    if (it.done() || !it.is_wasm()) return 0;
    WasmFrame* frame = WasmFrame::cast(it.frame());
    const auto& function = native_module_->module()->functions[func_index];
    int offset = frame->position() - function.code.offset();
    if (std::binary_search(breakpoints.begin(), breakpoints.end(), offset)) {
      return 0;
    }
    return offset;
  }

  WasmCode* RecompileLiftoffWithBreakpoints(int func_index, Vector<int> offsets,
                                            int dead_breakpoint) {
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
        for_debugging, counters, &unused_detected, offsets, &debug_sidetable,
        dead_breakpoint);
    // Liftoff compilation failure is a FATAL error. We rely on complete Liftoff
    // support for debugging.
    if (!result.succeeded()) FATAL("Liftoff compilation failed");
    DCHECK_NOT_NULL(debug_sidetable);

    WasmCode* new_code = native_module_->PublishCode(
        native_module_->AddCompiledCode(std::move(result)));

    DCHECK(new_code->is_inspectable());
    {
      base::MutexGuard lock(&debug_side_tables_mutex_);
      DCHECK_EQ(0, debug_side_tables_.count(new_code));
      debug_side_tables_.emplace(new_code, std::move(debug_sidetable));
    }

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
      int dead_breakpoint =
          DeadBreakpoint(func_index, all_breakpoints, isolate);
      new_code = RecompileLiftoffWithBreakpoints(
          func_index, VectorOf(all_breakpoints), dead_breakpoint);
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
                         Isolate* isolate, StackFrameId stepping_frame,
                         int dead_breakpoint) {
    DCHECK(!mutex_.TryLock());  // Mutex is held externally.
    WasmCode* new_code = RecompileLiftoffWithBreakpoints(
        func_index, breakpoints, dead_breakpoint);
    UpdateReturnAddresses(isolate, new_code, stepping_frame);
  }

  void FloodWithBreakpoints(WasmFrame* frame, ReturnLocation return_location) {
    // 0 is an invalid offset used to indicate flooding.
    int offset = 0;
    DCHECK(frame->wasm_code()->is_liftoff());
    // Generate an additional source position for the current byte offset.
    base::MutexGuard guard(&mutex_);
    WasmCode* new_code = RecompileLiftoffWithBreakpoints(
        frame->function_index(), VectorOf(&offset, 1), 0);
    UpdateReturnAddress(frame, new_code, return_location);

    per_isolate_data_[frame->isolate()].stepping_frame = frame->id();
  }

  bool PrepareStep(WasmFrame* frame) {
    WasmCodeRefScope wasm_code_ref_scope;
    wasm::WasmCode* code = frame->wasm_code();
    if (!code->is_liftoff()) return false;  // Cannot step in TurboFan code.
    if (IsAtReturn(frame)) return false;    // Will return after this step.
    FloodWithBreakpoints(frame, kAfterBreakpoint);
    return true;
  }

  void PrepareStepOutTo(WasmFrame* frame) {
    WasmCodeRefScope wasm_code_ref_scope;
    wasm::WasmCode* code = frame->wasm_code();
    if (!code->is_liftoff()) return;  // Cannot step out to TurboFan code.
    FloodWithBreakpoints(frame, kAfterWasmCall);
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
    int dead_breakpoint = DeadBreakpoint(func_index, remaining, isolate);
    UpdateBreakpoints(func_index, VectorOf(remaining), isolate,
                      isolate_data.stepping_frame, dead_breakpoint);
  }

  void RemoveDebugSideTables(Vector<WasmCode* const> codes) {
    base::MutexGuard guard(&debug_side_tables_mutex_);
    for (auto* code : codes) {
      debug_side_tables_.erase(code);
    }
  }

  DebugSideTable* GetDebugSideTableIfExists(const WasmCode* code) const {
    base::MutexGuard guard(&debug_side_tables_mutex_);
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
        RecompileLiftoffWithBreakpoints(func_index, VectorOf(remaining), 0);
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
      base::MutexGuard guard(&debug_side_tables_mutex_);
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
      base::MutexGuard guard(&debug_side_tables_mutex_);
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
      DCHECK(reg.is_fp() || reg.is_fp_pair());
      // ifdef here to workaround unreachable code for is_fp_pair.
#ifdef V8_TARGET_ARCH_ARM
      int code = reg.is_fp_pair() ? reg.low_fp().code() : reg.fp().code();
#else
      int code = reg.fp().code();
#endif
      Address spilled_addr =
          debug_break_fp +
          WasmDebugBreakFrameConstants::GetPushedFpRegisterOffset(code);
      if (type == kWasmF32) {
        return WasmValue(ReadUnalignedValue<float>(spilled_addr));
      } else if (type == kWasmF64) {
        return WasmValue(ReadUnalignedValue<double>(spilled_addr));
      } else if (type == kWasmS128) {
        return WasmValue(Simd128(ReadUnalignedValue<int16>(spilled_addr)));
      } else {
        // All other cases should have been handled above.
        UNREACHABLE();
      }
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
      case ValueType::kS128: {
        return WasmValue(Simd128(ReadUnalignedValue<int16>(stack_address)));
      }
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
    DisallowGarbageCollection no_gc;
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

  mutable base::Mutex debug_side_tables_mutex_;

  // DebugSideTable per code object, lazily initialized.
  std::unordered_map<const WasmCode*, std::unique_ptr<DebugSideTable>>
      debug_side_tables_;

  // {mutex_} protects all fields below.
  mutable base::Mutex mutex_;

  // Names of locals, lazily decoded from the wire bytes.
  std::unique_ptr<LocalNames> local_names_;

  // Isolate-specific data.
  std::unordered_map<Isolate*, PerIsolateDebugData> per_isolate_data_;
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

const wasm::WasmFunction& DebugInfo::GetFunctionAtAddress(Address pc) {
  return impl_->GetFunctionAtAddress(pc);
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

bool DebugInfo::PrepareStep(WasmFrame* frame) {
  return impl_->PrepareStep(frame);
}

void DebugInfo::PrepareStepOutTo(WasmFrame* frame) {
  impl_->PrepareStepOutTo(frame);
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
  DisallowGarbageCollection no_gc;

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

namespace wasm {
namespace {

void SetMapValue(Isolate* isolate, Handle<JSMap> map, Handle<Object> key,
                 Handle<Object> value) {
  DCHECK(!map.is_null() && !key.is_null() && !value.is_null());
  Handle<Object> argv[] = {key, value};
  Execution::CallBuiltin(isolate, isolate->map_set(), map, arraysize(argv),
                         argv)
      .Check();
}

Handle<Object> GetMapValue(Isolate* isolate, Handle<JSMap> map,
                           Handle<Object> key) {
  DCHECK(!map.is_null() && !key.is_null());
  Handle<Object> argv[] = {key};
  return Execution::CallBuiltin(isolate, isolate->map_get(), map,
                                arraysize(argv), argv)
      .ToHandleChecked();
}

Handle<WasmInstanceObject> GetInstance(Isolate* isolate,
                                       Handle<JSObject> handler) {
  Handle<Object> instance =
      JSObject::GetProperty(isolate, handler, "instance").ToHandleChecked();
  DCHECK(instance->IsWasmInstanceObject());
  return Handle<WasmInstanceObject>::cast(instance);
}

// Populate a JSMap with name->index mappings from an ordered list of names.
Handle<JSMap> GetNameTable(Isolate* isolate,
                           const std::vector<Handle<String>>& names) {
  Factory* factory = isolate->factory();
  Handle<JSMap> name_table = factory->NewJSMap();

  for (size_t i = 0; i < names.size(); ++i) {
    SetMapValue(isolate, name_table, names[i], factory->NewNumberFromInt64(i));
  }
  return name_table;
}

// Look up a  JSMap with name->index mappings from an ordered list of names.
Handle<JSMap> GetOrCreateNameTable(
    Handle<WasmInstanceObject> instance, const char* table_name,
    Handle<JSMap> (*generate_names_callback)(Handle<WasmInstanceObject>)) {
  Isolate* isolate = instance->GetIsolate();
  Handle<Object> tables;
  Handle<Object> name_table;
  Handle<String> table_name_string =
      isolate->factory()->InternalizeUtf8String(table_name);
  Handle<Name> symbol = isolate->factory()->wasm_debug_proxy_name_tables();
  bool has_tables =
      Object::GetProperty(isolate, instance, symbol).ToHandle(&tables) &&
      !tables->IsUndefined();
  if (has_tables) {
    if (Object::GetProperty(isolate, tables, table_name_string)
            .ToHandle(&name_table)) {
      DCHECK(name_table->IsUndefined() || name_table->IsJSMap());
      if (!name_table->IsUndefined()) return Handle<JSMap>::cast(name_table);
    }
  } else {
    tables = isolate->factory()->NewJSObjectWithNullProto();
    Object::SetProperty(isolate, instance, symbol, tables).Check();
  }

  name_table = generate_names_callback(instance);
  Object::SetProperty(isolate, tables, table_name_string, name_table).Check();
  return Handle<JSMap>::cast(name_table);
}

// Look up a name in a name table. Name tables are stored under the "names"
// property of the handler and map names to index.
base::Optional<int> ResolveValueSelector(
    Isolate* isolate, Handle<Name> property, Handle<JSObject> handler,
    bool enable_index_lookup, const char* table_name = nullptr,
    Handle<JSMap> (*generate_names_callback)(Handle<WasmInstanceObject>) =
        nullptr) {
  size_t index = 0;
  if (enable_index_lookup && property->AsIntegerIndex(&index)) {
    if (index < kMaxInt) return static_cast<int>(index);
    return {};
  }

  Handle<Object> name_table =
      JSObject::GetProperty(isolate, handler, "names").ToHandleChecked();
  if (name_table->IsUndefined(isolate)) {
    name_table = GetOrCreateNameTable(GetInstance(isolate, handler), table_name,
                                      generate_names_callback);
    JSObject::AddProperty(isolate, handler, "names", name_table, DONT_ENUM);
  }
  DCHECK(name_table->IsJSMap());

  Handle<Object> object =
      GetMapValue(isolate, Handle<JSMap>::cast(name_table), property);
  if (object->IsUndefined()) return {};
  DCHECK(object->IsNumeric());
  return NumberToInt32(*object);
}

// Helper for unpacking a maybe name that makes a default with an index if
// the name is empty. If the name is not empty, it's prefixed with a $.
Handle<String> GetNameOrDefault(Isolate* isolate,
                                MaybeHandle<String> maybe_name,
                                const char* default_name_prefix, int index) {
  Handle<String> name;
  if (maybe_name.ToHandle(&name)) {
    return isolate->factory()
        ->NewConsString(isolate->factory()->NewStringFromAsciiChecked("$"),
                        name)
        .ToHandleChecked();
  }

  // Maximum length of the default names: $memory-2147483648\0
  static constexpr int kMaxStrLen = 19;
  EmbeddedVector<char, kMaxStrLen> value;
  DCHECK_LT(strlen(default_name_prefix) + /*strlen(kMinInt)*/ 11, kMaxStrLen);
  int len = SNPrintF(value, "%s%d", default_name_prefix, index);
  return isolate->factory()->InternalizeString(value.SubVector(0, len));
}

// Generate names for the locals. Names either come from the name table,
// otherwise the default $varX is used.
std::vector<Handle<String>> GetLocalNames(Handle<WasmInstanceObject> instance,
                                          Address pc) {
  wasm::NativeModule* native_module = instance->module_object().native_module();
  wasm::DebugInfo* debug_info = native_module->GetDebugInfo();
  int num_locals = debug_info->GetNumLocals(pc);
  auto* isolate = instance->GetIsolate();

  wasm::ModuleWireBytes module_wire_bytes(
      instance->module_object().native_module()->wire_bytes());
  const wasm::WasmFunction& function = debug_info->GetFunctionAtAddress(pc);

  std::vector<Handle<String>> names;
  for (int i = 0; i < num_locals; ++i) {
    wasm::WireBytesRef local_name_ref =
        debug_info->GetLocalName(function.func_index, i);
    DCHECK(module_wire_bytes.BoundsCheck(local_name_ref));
    Vector<const char> name_vec =
        module_wire_bytes.GetNameOrNull(local_name_ref);
    names.emplace_back(GetNameOrDefault(
        isolate,
        name_vec.empty() ? MaybeHandle<String>()
                         : isolate->factory()->NewStringFromUtf8(name_vec),
        "$var", i));
  }

  return names;
}

// Generate names for the globals. Names either come from the name table,
// otherwise the default $globalX is used.
Handle<JSMap> GetGlobalNames(Handle<WasmInstanceObject> instance) {
  Isolate* isolate = instance->GetIsolate();
  auto& globals = instance->module()->globals;
  Handle<JSMap> names = isolate->factory()->NewJSMap();
  for (uint32_t i = 0; i < globals.size(); ++i) {
    HandleScope scope(isolate);
    SetMapValue(isolate, names,
                GetNameOrDefault(isolate,
                                 WasmInstanceObject::GetGlobalNameOrNull(
                                     isolate, instance, i),
                                 "$global", i),
                isolate->factory()->NewNumberFromUint(i));
  }
  return names;
}

// Generate names for the functions.
Handle<JSMap> GetFunctionNames(Handle<WasmInstanceObject> instance) {
  Isolate* isolate = instance->GetIsolate();
  auto* module = instance->module();

  wasm::ModuleWireBytes wire_bytes(
      instance->module_object().native_module()->wire_bytes());

  Handle<JSMap> names = isolate->factory()->NewJSMap();
  for (auto& function : module->functions) {
    HandleScope scope(isolate);
    wasm::WireBytesRef name_ref =
        module->lazily_generated_names.LookupFunctionName(
            wire_bytes, function.func_index, VectorOf(module->export_table));
    DCHECK(wire_bytes.BoundsCheck(name_ref));
    Vector<const char> name_vec = wire_bytes.GetNameOrNull(name_ref);
    Handle<String> name = GetNameOrDefault(
        isolate,
        name_vec.empty() ? MaybeHandle<String>()
                         : isolate->factory()->NewStringFromUtf8(name_vec),
        "$func", function.func_index);
    SetMapValue(isolate, names, name,
                isolate->factory()->NewNumberFromUint(function.func_index));
  }

  return names;
}

// Generate names for the memories.
Handle<JSMap> GetMemoryNames(Handle<WasmInstanceObject> instance) {
  Isolate* isolate = instance->GetIsolate();

  Handle<JSMap> names = isolate->factory()->NewJSMap();
  uint32_t memory_count = instance->has_memory_object() ? 1 : 0;
  for (uint32_t memory_index = 0; memory_index < memory_count; ++memory_index) {
    SetMapValue(isolate, names,
                GetNameOrDefault(isolate,
                                 WasmInstanceObject::GetMemoryNameOrNull(
                                     isolate, instance, memory_index),
                                 "$memory", memory_index),
                isolate->factory()->NewNumberFromUint(memory_index));
  }

  return names;
}

// Generate names for the tables.
Handle<JSMap> GetTableNames(Handle<WasmInstanceObject> instance) {
  Isolate* isolate = instance->GetIsolate();
  auto tables = handle(instance->tables(), isolate);

  Handle<JSMap> names = isolate->factory()->NewJSMap();
  for (int table_index = 0; table_index < tables->length(); ++table_index) {
    auto func_table =
        handle(WasmTableObject::cast(tables->get(table_index)), isolate);
    if (!func_table->type().is_reference_to(wasm::HeapType::kFunc)) continue;

    SetMapValue(isolate, names,
                GetNameOrDefault(isolate,
                                 WasmInstanceObject::GetTableNameOrNull(
                                     isolate, instance, table_index),
                                 "$table", table_index),
                isolate->factory()->NewNumberFromInt(table_index));
  }
  return names;
}

Address GetPC(Isolate* isolate, Handle<JSObject> handler) {
  Handle<Object> pc =
      JSObject::GetProperty(isolate, handler, "pc").ToHandleChecked();
  DCHECK(pc->IsBigInt());
  return Handle<BigInt>::cast(pc)->AsUint64();
}

Address GetFP(Isolate* isolate, Handle<JSObject> handler) {
  Handle<Object> fp =
      JSObject::GetProperty(isolate, handler, "fp").ToHandleChecked();
  DCHECK(fp->IsBigInt());
  return Handle<BigInt>::cast(fp)->AsUint64();
}

Address GetCalleeFP(Isolate* isolate, Handle<JSObject> handler) {
  Handle<Object> callee_fp =
      JSObject::GetProperty(isolate, handler, "callee_fp").ToHandleChecked();
  DCHECK(callee_fp->IsBigInt());
  return Handle<BigInt>::cast(callee_fp)->AsUint64();
}

// Convert a WasmValue to an appropriate JS representation.
static Handle<Object> WasmValueToObject(Isolate* isolate,
                                        wasm::WasmValue value) {
  auto* factory = isolate->factory();
  switch (value.type().kind()) {
    case wasm::ValueType::kI32:
      return factory->NewNumberFromInt(value.to_i32());
    case wasm::ValueType::kI64:
      return BigInt::FromInt64(isolate, value.to_i64());
    case wasm::ValueType::kF32:
      return factory->NewNumber(value.to_f32());
    case wasm::ValueType::kF64:
      return factory->NewNumber(value.to_f64());
    case wasm::ValueType::kS128: {
      wasm::Simd128 s128 = value.to_s128();
      Handle<JSArrayBuffer> buffer;
      if (!isolate->factory()
               ->NewJSArrayBufferAndBackingStore(
                   kSimd128Size, InitializedFlag::kUninitialized)
               .ToHandle(&buffer)) {
        isolate->FatalProcessOutOfHeapMemory(
            "failed to allocate backing store");
      }

      base::Memcpy(buffer->allocation_base(), s128.bytes(),
                   buffer->byte_length());
      return isolate->factory()->NewJSTypedArray(kExternalUint8Array, buffer, 0,
                                                 buffer->byte_length());
    }
    case wasm::ValueType::kRef:
      return value.to_externref();
    default:
      break;
  }
  return factory->undefined_value();
}

base::Optional<int> HasLocalImpl(Isolate* isolate, Handle<Name> property,
                                 Handle<JSObject> handler,
                                 bool enable_index_lookup) {
  Handle<WasmInstanceObject> instance = GetInstance(isolate, handler);

  base::Optional<int> index =
      ResolveValueSelector(isolate, property, handler, enable_index_lookup);
  if (!index) return index;
  Address pc = GetPC(isolate, handler);

  wasm::DebugInfo* debug_info =
      instance->module_object().native_module()->GetDebugInfo();
  int num_locals = debug_info->GetNumLocals(pc);
  if (0 <= index && index < num_locals) return index;
  return {};
}

Handle<Object> GetLocalImpl(Isolate* isolate, Handle<Name> property,
                            Handle<JSObject> handler,
                            bool enable_index_lookup) {
  Factory* factory = isolate->factory();
  Handle<WasmInstanceObject> instance = GetInstance(isolate, handler);

  base::Optional<int> index =
      HasLocalImpl(isolate, property, handler, enable_index_lookup);
  if (!index) return factory->undefined_value();
  Address pc = GetPC(isolate, handler);
  Address fp = GetFP(isolate, handler);
  Address callee_fp = GetCalleeFP(isolate, handler);

  wasm::DebugInfo* debug_info =
      instance->module_object().native_module()->GetDebugInfo();
  wasm::WasmValue value = debug_info->GetLocalValue(*index, pc, fp, callee_fp);
  return WasmValueToObject(isolate, value);
}

base::Optional<int> HasGlobalImpl(Isolate* isolate, Handle<Name> property,
                                  Handle<JSObject> handler,
                                  bool enable_index_lookup) {
  Handle<WasmInstanceObject> instance = GetInstance(isolate, handler);
  base::Optional<int> index =
      ResolveValueSelector(isolate, property, handler, enable_index_lookup,
                           "globals", GetGlobalNames);
  if (!index) return index;

  const std::vector<wasm::WasmGlobal>& globals = instance->module()->globals;
  if (globals.size() <= kMaxInt && 0 <= *index &&
      *index < static_cast<int>(globals.size())) {
    return index;
  }
  return {};
}

Handle<Object> GetGlobalImpl(Isolate* isolate, Handle<Name> property,
                             Handle<JSObject> handler,
                             bool enable_index_lookup) {
  Handle<WasmInstanceObject> instance = GetInstance(isolate, handler);
  base::Optional<int> index =
      HasGlobalImpl(isolate, property, handler, enable_index_lookup);
  if (!index) return isolate->factory()->undefined_value();

  const std::vector<wasm::WasmGlobal>& globals = instance->module()->globals;
  return WasmValueToObject(
      isolate, WasmInstanceObject::GetGlobalValue(instance, globals[*index]));
}

base::Optional<int> HasMemoryImpl(Isolate* isolate, Handle<Name> property,
                                  Handle<JSObject> handler,
                                  bool enable_index_lookup) {
  Handle<WasmInstanceObject> instance = GetInstance(isolate, handler);
  base::Optional<int> index =
      ResolveValueSelector(isolate, property, handler, enable_index_lookup,
                           "memories", GetMemoryNames);
  if (index && *index == 0 && instance->has_memory_object()) return index;
  return {};
}

Handle<Object> GetMemoryImpl(Isolate* isolate, Handle<Name> property,
                             Handle<JSObject> handler,
                             bool enable_index_lookup) {
  Handle<WasmInstanceObject> instance = GetInstance(isolate, handler);
  base::Optional<int> index =
      HasMemoryImpl(isolate, property, handler, enable_index_lookup);
  if (index) return handle(instance->memory_object(), isolate);
  return isolate->factory()->undefined_value();
}

base::Optional<int> HasFunctionImpl(Isolate* isolate, Handle<Name> property,
                                    Handle<JSObject> handler,
                                    bool enable_index_lookup) {
  Handle<WasmInstanceObject> instance = GetInstance(isolate, handler);
  base::Optional<int> index =
      ResolveValueSelector(isolate, property, handler, enable_index_lookup,
                           "functions", GetFunctionNames);
  if (!index) return index;
  const std::vector<wasm::WasmFunction>& functions =
      instance->module()->functions;
  if (functions.size() <= kMaxInt && 0 <= *index &&
      *index < static_cast<int>(functions.size())) {
    return index;
  }
  return {};
}

Handle<Object> GetFunctionImpl(Isolate* isolate, Handle<Name> property,
                               Handle<JSObject> handler,
                               bool enable_index_lookup) {
  Handle<WasmInstanceObject> instance = GetInstance(isolate, handler);
  base::Optional<int> index =
      HasFunctionImpl(isolate, property, handler, enable_index_lookup);
  if (!index) return isolate->factory()->undefined_value();

  return WasmInstanceObject::GetOrCreateWasmExternalFunction(isolate, instance,
                                                             *index);
}

base::Optional<int> HasTableImpl(Isolate* isolate, Handle<Name> property,
                                 Handle<JSObject> handler,
                                 bool enable_index_lookup) {
  Handle<WasmInstanceObject> instance = GetInstance(isolate, handler);
  base::Optional<int> index = ResolveValueSelector(
      isolate, property, handler, enable_index_lookup, "tables", GetTableNames);
  if (!index) return index;
  Handle<FixedArray> tables(instance->tables(), isolate);
  int num_tables = tables->length();
  if (*index < 0 || *index >= num_tables) return {};

  Handle<WasmTableObject> func_table(WasmTableObject::cast(tables->get(*index)),
                                     isolate);
  if (func_table->type().is_reference_to(wasm::HeapType::kFunc)) return index;
  return {};
}

Handle<Object> GetTableImpl(Isolate* isolate, Handle<Name> property,
                            Handle<JSObject> handler,
                            bool enable_index_lookup) {
  Handle<WasmInstanceObject> instance = GetInstance(isolate, handler);
  base::Optional<int> index =
      HasTableImpl(isolate, property, handler, enable_index_lookup);
  if (!index) return isolate->factory()->undefined_value();

  Handle<WasmTableObject> func_table(
      WasmTableObject::cast(instance->tables().get(*index)), isolate);
  return func_table;
}

// Generic has trap callback for the index space proxies.
template <base::Optional<int> Impl(Isolate*, Handle<Name>, Handle<JSObject>,
                                   bool)>
void HasTrapCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_GE(args.Length(), 2);
  Isolate* isolate = reinterpret_cast<Isolate*>(args.GetIsolate());
  DCHECK(args.This()->IsObject());
  Handle<JSObject> handler =
      Handle<JSObject>::cast(Utils::OpenHandle(*args.This()));

  DCHECK(args[1]->IsName());
  Handle<Name> property = Handle<Name>::cast(Utils::OpenHandle(*args[1]));
  args.GetReturnValue().Set(Impl(isolate, property, handler, true).has_value());
}

// Generic get trap callback for the index space proxies.
template <Handle<Object> Impl(Isolate*, Handle<Name>, Handle<JSObject>, bool)>
void GetTrapCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_GE(args.Length(), 2);
  Isolate* isolate = reinterpret_cast<Isolate*>(args.GetIsolate());
  DCHECK(args.This()->IsObject());
  Handle<JSObject> handler =
      Handle<JSObject>::cast(Utils::OpenHandle(*args.This()));

  DCHECK(args[1]->IsName());
  Handle<Name> property = Handle<Name>::cast(Utils::OpenHandle(*args[1]));
  args.GetReturnValue().Set(
      Utils::ToLocal(Impl(isolate, property, handler, true)));
}

template <typename ReturnT>
ReturnT DelegateToplevelCall(Isolate* isolate, Handle<JSObject> target,
                             Handle<Name> property, const char* index_space,
                             ReturnT (*impl)(Isolate*, Handle<Name>,
                                             Handle<JSObject>, bool)) {
  Handle<Object> namespace_proxy =
      JSObject::GetProperty(isolate, target, index_space).ToHandleChecked();
  DCHECK(namespace_proxy->IsJSProxy());
  Handle<JSObject> namespace_handler(
      JSObject::cast(Handle<JSProxy>::cast(namespace_proxy)->handler()),
      isolate);
  return impl(isolate, property, namespace_handler, false);
}

template <typename ReturnT>
using DelegateCallback = ReturnT (*)(Isolate*, Handle<Name>, Handle<JSObject>,
                                     bool);

// Has trap callback for the top-level proxy.
void ToplevelHasTrapCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_GE(args.Length(), 2);
  Isolate* isolate = reinterpret_cast<Isolate*>(args.GetIsolate());
  DCHECK(args[0]->IsObject());
  Handle<JSObject> target = Handle<JSObject>::cast(Utils::OpenHandle(*args[0]));

  DCHECK(args[1]->IsName());
  Handle<Name> property = Handle<Name>::cast(Utils::OpenHandle(*args[1]));

  // First check if the property exists on the target.
  if (JSObject::HasProperty(target, property).FromMaybe(false)) {
    args.GetReturnValue().Set(true);
    return;
  }

  // All the properties in the delegates below are starting with $.
  if (!property->IsString()) {
    args.GetReturnValue().Set(false);
    return;
  }
  Handle<String> property_string = Handle<String>::cast(property);
  if (property_string->length() < 2 || property_string->Get(0) != '$') {
    args.GetReturnValue().Set(false);
    return;
  }

  // Now check the index space proxies in order if they know the property.
  constexpr std::pair<const char*, DelegateCallback<base::Optional<int>>>
      kDelegates[] = {{"memories", HasMemoryImpl},
                      {"locals", HasLocalImpl},
                      {"tables", HasTableImpl},
                      {"functions", HasFunctionImpl},
                      {"globals", HasGlobalImpl}};
  for (auto& delegate : kDelegates) {
    if (DelegateToplevelCall(isolate, target, property, delegate.first,
                             delegate.second)) {
      args.GetReturnValue().Set(true);
      return;
    }
    args.GetReturnValue().Set(false);
  }
}

// Get trap callback for the top-level proxy.
void ToplevelGetTrapCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_GE(args.Length(), 2);
  Isolate* isolate = reinterpret_cast<Isolate*>(args.GetIsolate());
  DCHECK(args[0]->IsObject());
  Handle<JSObject> target = Handle<JSObject>::cast(Utils::OpenHandle(*args[0]));

  DCHECK(args[1]->IsName());
  Handle<Name> property = Handle<Name>::cast(Utils::OpenHandle(*args[1]));

  // First, check if the property is a proper property on the target. If so,
  // return its value.
  Handle<Object> value =
      JSObject::GetProperty(isolate, target, property).ToHandleChecked();
  if (!value->IsUndefined()) {
    args.GetReturnValue().Set(Utils::ToLocal(value));
    return;
  }

  // All the properties in the delegates below are starting with $.
  if (!property->IsString()) {
    return;
  }
  Handle<String> property_string = Handle<String>::cast(property);
  if (property_string->length() < 0 || property_string->Get(0) != '$') {
    return;
  }

  // Try the index space proxies in the correct disambiguation order.
  constexpr std::pair<const char*, DelegateCallback<Handle<Object>>>
      kDelegates[] = {{"memories", GetMemoryImpl},
                      {"locals", GetLocalImpl},
                      {"tables", GetTableImpl},
                      {"functions", GetFunctionImpl},
                      {"globals", GetGlobalImpl}};
  for (auto& delegate : kDelegates) {
    value = DelegateToplevelCall(isolate, target, property, delegate.first,
                                 delegate.second);
    if (!value->IsUndefined()) {
      args.GetReturnValue().Set(Utils::ToLocal(value));
      return;
    }
  }
}

Handle<JSFunction> InstallFunc(Isolate* isolate, Handle<JSObject> object,
                               const char* str, FunctionCallback func,
                               int length) {
  auto name = isolate->factory()->NewStringFromAsciiChecked(str);
  auto templ = v8::FunctionTemplate::New(
      reinterpret_cast<v8::Isolate*>(isolate), func, {}, {}, 0,
      ConstructorBehavior::kAllow, SideEffectType::kHasNoSideEffect);
  templ->RemovePrototype();
  auto function =
      ApiNatives::InstantiateFunction(v8::Utils::OpenHandle(*templ), name)
          .ToHandleChecked();
  function->shared().set_length(length);
  JSObject::AddProperty(isolate, object, name, function, NONE);
  return function;
}

// Produce a JSProxy with a given name table and get and has trap handlers.
Handle<JSProxy> GetJSProxy(
    WasmFrame* frame, MaybeHandle<JSMap> maybe_name_table,
    void (*get_callback)(const v8::FunctionCallbackInfo<v8::Value>&),
    void (*has_callback)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  Isolate* isolate = frame->isolate();
  Factory* factory = isolate->factory();
  Handle<JSObject> target = factory->NewJSObjectWithNullProto();
  Handle<JSObject> handler = factory->NewJSObjectWithNullProto();

  // Besides the name table, the get and has traps need access to the instance
  // and frame information.
  Handle<JSMap> name_table;
  if (maybe_name_table.ToHandle(&name_table)) {
    JSObject::AddProperty(isolate, handler, "names", name_table, DONT_ENUM);
  }
  Handle<WasmInstanceObject> instance(frame->wasm_instance(), isolate);
  JSObject::AddProperty(isolate, handler, "instance", instance, DONT_ENUM);
  Handle<BigInt> pc = BigInt::FromInt64(isolate, frame->pc());
  JSObject::AddProperty(isolate, handler, "pc", pc, DONT_ENUM);
  Handle<BigInt> fp = BigInt::FromInt64(isolate, frame->fp());
  JSObject::AddProperty(isolate, handler, "fp", fp, DONT_ENUM);
  Handle<BigInt> callee_fp = BigInt::FromInt64(isolate, frame->callee_fp());
  JSObject::AddProperty(isolate, handler, "callee_fp", callee_fp, DONT_ENUM);

  InstallFunc(isolate, handler, "get", get_callback, 3);
  InstallFunc(isolate, handler, "has", has_callback, 2);

  return factory->NewJSProxy(target, handler);
}

Handle<JSObject> GetStackObject(WasmFrame* frame) {
  Isolate* isolate = frame->isolate();
  Handle<JSObject> object = isolate->factory()->NewJSObjectWithNullProto();
  wasm::DebugInfo* debug_info =
      frame->wasm_instance().module_object().native_module()->GetDebugInfo();
  int num_values = debug_info->GetStackDepth(frame->pc());
  for (int i = 0; i < num_values; ++i) {
    wasm::WasmValue value = debug_info->GetStackValue(
        i, frame->pc(), frame->fp(), frame->callee_fp());
    JSObject::AddDataElement(object, i, WasmValueToObject(isolate, value),
                             NONE);
  }
  return object;
}
}  // namespace

// This function generates the JS debug proxy for a given Wasm frame. The debug
// proxy is used when evaluating debug JS expressions on a wasm frame and let's
// the developer inspect the engine state from JS. The proxy provides the
// following interface:
//
// type WasmSimdValue = Uint8Array;
// type WasmValue = number | bigint | object | WasmSimdValue;
// type WasmFunction = (... args : WasmValue[]) = > WasmValue;
// interface WasmInterface {
//   $globalX: WasmValue;
//   $varX: WasmValue;
//   $funcX(a : WasmValue /*, ...*/) : WasmValue;
//   readonly $memoryX : WebAssembly.Memory;
//   readonly $tableX : WebAssembly.Table;
//
//   readonly instance : WebAssembly.Instance;
//   readonly module : WebAssembly.Module;
//
//   readonly memories : {[nameOrIndex:string | number] : WebAssembly.Memory};
//   readonly tables : {[nameOrIndex:string | number] : WebAssembly.Table};
//   readonly stack : WasmValue[];
//   readonly globals : {[nameOrIndex:string | number] : WasmValue};
//   readonly locals : {[nameOrIndex:string | number] : WasmValue};
//   readonly functions : {[nameOrIndex:string | number] : WasmFunction};
// }
//
// The wasm index spaces memories, tables, imports, exports, globals, locals
// functions are JSProxies that lazily produce values either by index or by
// name. A top level JSProxy is wrapped around those for top-level lookup of
// names in the disambiguation order  memory, local, table, function, global.

Handle<JSProxy> GetJSDebugProxy(WasmFrame* frame) {
  Isolate* isolate = frame->isolate();
  Factory* factory = isolate->factory();
  Handle<WasmInstanceObject> instance(frame->wasm_instance(), isolate);
  Handle<WasmModuleObject> module_object(instance->module_object(), isolate);

  // The top level proxy delegates lookups to the index space proxies.
  Handle<JSObject> handler = factory->NewJSObjectWithNullProto();
  InstallFunc(isolate, handler, "get", ToplevelGetTrapCallback, 3);
  InstallFunc(isolate, handler, "has", ToplevelHasTrapCallback, 2);

  Handle<JSObject> target = factory->NewJSObjectWithNullProto();

  // Expose "instance" and "module" on the target.
  JSObject::AddProperty(isolate, target, "instance", instance, READ_ONLY);
  JSObject::AddProperty(isolate, target, "module", module_object, READ_ONLY);

  // Generate JSMaps per index space for name->index lookup. Every index space
  // proxy is associated with its table for local name lookup.

  auto local_name_table =
      GetNameTable(isolate, GetLocalNames(instance, frame->pc()));
  auto locals =
      GetJSProxy(frame, local_name_table, GetTrapCallback<GetLocalImpl>,
                 HasTrapCallback<HasLocalImpl>);
  JSObject::AddProperty(isolate, target, "locals", locals, READ_ONLY);

  auto globals = GetJSProxy(frame, {}, GetTrapCallback<GetGlobalImpl>,
                            HasTrapCallback<HasGlobalImpl>);
  JSObject::AddProperty(isolate, target, "globals", globals, READ_ONLY);

  auto functions = GetJSProxy(frame, {}, GetTrapCallback<GetFunctionImpl>,
                              HasTrapCallback<HasFunctionImpl>);
  JSObject::AddProperty(isolate, target, "functions", functions, READ_ONLY);

  auto memories = GetJSProxy(frame, {}, GetTrapCallback<GetMemoryImpl>,
                             HasTrapCallback<HasMemoryImpl>);
  JSObject::AddProperty(isolate, target, "memories", memories, READ_ONLY);

  auto tables = GetJSProxy(frame, {}, GetTrapCallback<GetTableImpl>,
                           HasTrapCallback<HasTableImpl>);
  JSObject::AddProperty(isolate, target, "tables", tables, READ_ONLY);

  auto stack = GetStackObject(frame);
  JSObject::AddProperty(isolate, target, "stack", stack, READ_ONLY);

  return factory->NewJSProxy(target, handler);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
