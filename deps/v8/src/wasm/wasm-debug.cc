// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-debug.h"

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
#include "src/utils/identity-map.h"
#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/baseline/liftoff-register.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
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
  switch (value.type().kind()) {
    case ValueType::kI32:
      if (Smi::IsValid(value.to<int32_t>()))
        return handle(Smi::FromInt(value.to<int32_t>()), isolate);
      return PrintFToOneByteString<false>(isolate, "%d", value.to<int32_t>());
    case ValueType::kI64: {
      int64_t i64 = value.to<int64_t>();
      int32_t i32 = static_cast<int32_t>(i64);
      if (i32 == i64 && Smi::IsValid(i32))
        return handle(Smi::FromIntptr(i32), isolate);
      return PrintFToOneByteString<false>(isolate, "%" PRId64, i64);
    }
    case ValueType::kF32:
      return isolate->factory()->NewNumber(value.to<float>());
    case ValueType::kF64:
      return isolate->factory()->NewNumber(value.to<double>());
    case ValueType::kAnyRef:
      return value.to_anyref();
    default:
      UNIMPLEMENTED();
      return isolate->factory()->undefined_value();
  }
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

class InterpreterHandle {
  Isolate* isolate_;
  const WasmModule* module_;
  WasmInterpreter interpreter_;
  StepAction next_step_action_ = StepNone;
  int last_step_stack_depth_ = 0;
  std::unordered_map<Address, uint32_t> activations_;

  uint32_t StartActivation(Address frame_pointer) {
    WasmInterpreter::Thread* thread = interpreter_.GetThread(0);
    uint32_t activation_id = thread->StartActivation();
    DCHECK_EQ(0, activations_.count(frame_pointer));
    activations_.insert(std::make_pair(frame_pointer, activation_id));
    return activation_id;
  }

  void FinishActivation(Address frame_pointer, uint32_t activation_id) {
    WasmInterpreter::Thread* thread = interpreter_.GetThread(0);
    thread->FinishActivation(activation_id);
    DCHECK_EQ(1, activations_.count(frame_pointer));
    activations_.erase(frame_pointer);
  }

  bool HasActivation(Address frame_pointer) {
    return activations_.count(frame_pointer);
  }

  std::pair<uint32_t, uint32_t> GetActivationFrameRange(
      WasmInterpreter::Thread* thread, Address frame_pointer) {
    DCHECK_EQ(1, activations_.count(frame_pointer));
    uint32_t activation_id = activations_.find(frame_pointer)->second;
    uint32_t num_activations = static_cast<uint32_t>(activations_.size() - 1);
    uint32_t frame_base = thread->ActivationFrameBase(activation_id);
    uint32_t frame_limit = activation_id == num_activations
                               ? thread->GetFrameCount()
                               : thread->ActivationFrameBase(activation_id + 1);
    DCHECK_LE(frame_base, frame_limit);
    DCHECK_LE(frame_limit, thread->GetFrameCount());
    return {frame_base, frame_limit};
  }

  static ModuleWireBytes GetBytes(WasmDebugInfo debug_info) {
    // Return raw pointer into heap. The WasmInterpreter will make its own copy
    // of this data anyway, and there is no heap allocation in-between.
    NativeModule* native_module =
        debug_info.wasm_instance().module_object().native_module();
    return ModuleWireBytes{native_module->wire_bytes()};
  }

 public:
  InterpreterHandle(Isolate* isolate, Handle<WasmDebugInfo> debug_info)
      : isolate_(isolate),
        module_(debug_info->wasm_instance().module_object().module()),
        interpreter_(isolate, module_, GetBytes(*debug_info),
                     handle(debug_info->wasm_instance(), isolate)) {}

  WasmInterpreter* interpreter() { return &interpreter_; }
  const WasmModule* module() const { return module_; }

  void PrepareStep(StepAction step_action) {
    next_step_action_ = step_action;
    last_step_stack_depth_ = CurrentStackDepth();
  }

  void ClearStepping() { next_step_action_ = StepNone; }

  int CurrentStackDepth() {
    DCHECK_EQ(1, interpreter()->GetThreadCount());
    return interpreter()->GetThread(0)->GetFrameCount();
  }

  // Returns true if exited regularly, false if a trap/exception occurred and
  // was not handled inside this activation. In the latter case, a pending
  // exception will have been set on the isolate.
  bool Execute(Handle<WasmInstanceObject> instance_object,
               Address frame_pointer, uint32_t func_index,
               Vector<WasmValue> argument_values,
               Vector<WasmValue> return_values) {
    DCHECK_GE(module()->functions.size(), func_index);
    const FunctionSig* sig = module()->functions[func_index].sig;
    DCHECK_EQ(sig->parameter_count(), argument_values.size());
    DCHECK_EQ(sig->return_count(), return_values.size());

    uint32_t activation_id = StartActivation(frame_pointer);

    WasmCodeRefScope code_ref_scope;
    WasmInterpreter::Thread* thread = interpreter_.GetThread(0);
    thread->InitFrame(&module()->functions[func_index],
                      argument_values.begin());
    bool finished = false;
    while (!finished) {
      // TODO(clemensb): Add occasional StackChecks.
      WasmInterpreter::State state = ContinueExecution(thread);
      switch (state) {
        case WasmInterpreter::State::PAUSED:
          NotifyDebugEventListeners(thread);
          break;
        case WasmInterpreter::State::FINISHED:
          // Perfect, just break the switch and exit the loop.
          finished = true;
          break;
        case WasmInterpreter::State::TRAPPED: {
          MessageTemplate message_id =
              WasmOpcodes::TrapReasonToMessageId(thread->GetTrapReason());
          Handle<JSObject> exception =
              isolate_->factory()->NewWasmRuntimeError(message_id);
          JSObject::AddProperty(isolate_, exception,
                                isolate_->factory()->wasm_uncatchable_symbol(),
                                isolate_->factory()->true_value(), NONE);
          auto result = thread->RaiseException(isolate_, exception);
          if (result == WasmInterpreter::Thread::HANDLED) break;
          // If no local handler was found, we fall-thru to {STOPPED}.
          DCHECK_EQ(WasmInterpreter::State::STOPPED, thread->state());
          V8_FALLTHROUGH;
        }
        case WasmInterpreter::State::STOPPED:
          // An exception happened, and the current activation was unwound
          // without hitting a local exception handler. All that remains to be
          // done is finish the activation and let the exception propagate.
          DCHECK_EQ(thread->ActivationFrameBase(activation_id),
                    thread->GetFrameCount());
          DCHECK(isolate_->has_pending_exception());
          FinishActivation(frame_pointer, activation_id);
          return false;
        // RUNNING should never occur here.
        case WasmInterpreter::State::RUNNING:
        default:
          UNREACHABLE();
      }
    }

    // Copy back the return value.
#ifdef DEBUG
    const int max_count = WasmFeatures::FromIsolate(isolate_).has_mv()
                              ? kV8MaxWasmFunctionMultiReturns
                              : kV8MaxWasmFunctionReturns;
#endif
    DCHECK_GE(max_count, sig->return_count());
    for (unsigned i = 0; i < sig->return_count(); ++i) {
      return_values[i] = thread->GetReturnValue(i);
    }

    FinishActivation(frame_pointer, activation_id);

    // If we do stepping and it exits wasm interpreter then debugger need to
    // prepare for it.
    if (next_step_action_ != StepNone) {
      // Enter the debugger.
      DebugScope debug_scope(isolate_->debug());

      isolate_->debug()->PrepareStep(StepOut);
    }
    ClearStepping();

    return true;
  }

  WasmInterpreter::State ContinueExecution(WasmInterpreter::Thread* thread) {
    switch (next_step_action_) {
      case StepNone:
        return thread->Run();
      case StepIn:
        return thread->Step();
      case StepOut:
        thread->AddBreakFlags(WasmInterpreter::BreakFlag::AfterReturn);
        return thread->Run();
      case StepNext: {
        int stack_depth = thread->GetFrameCount();
        if (stack_depth == last_step_stack_depth_) return thread->Step();
        thread->AddBreakFlags(stack_depth > last_step_stack_depth_
                                  ? WasmInterpreter::BreakFlag::AfterReturn
                                  : WasmInterpreter::BreakFlag::AfterCall);
        return thread->Run();
      }
      default:
        UNREACHABLE();
    }
  }

  Handle<WasmInstanceObject> GetInstanceObject() {
    StackTraceFrameIterator it(isolate_);
    WasmInterpreterEntryFrame* frame =
        WasmInterpreterEntryFrame::cast(it.frame());
    Handle<WasmInstanceObject> instance_obj(frame->wasm_instance(), isolate_);
    // Check that this is indeed the instance which is connected to this
    // interpreter.
    DCHECK_EQ(this, Managed<InterpreterHandle>::cast(
                        instance_obj->debug_info().interpreter_handle())
                        .raw());
    return instance_obj;
  }

  void NotifyDebugEventListeners(WasmInterpreter::Thread* thread) {
    // Enter the debugger.
    DebugScope debug_scope(isolate_->debug());

    // Check whether we hit a breakpoint.
    if (isolate_->debug()->break_points_active()) {
      Handle<WasmModuleObject> module_object(
          GetInstanceObject()->module_object(), isolate_);
      Handle<Script> script(module_object->script(), isolate_);
      int position = GetTopPosition(module_object);
      Handle<FixedArray> breakpoints;
      if (WasmScript::CheckBreakPoints(isolate_, script, position)
              .ToHandle(&breakpoints)) {
        // We hit one or several breakpoints. Clear stepping, notify the
        // listeners and return.
        ClearStepping();
        isolate_->debug()->OnDebugBreak(breakpoints);
        return;
      }
    }

    // We did not hit a breakpoint, so maybe this pause is related to stepping.
    bool hit_step = false;
    switch (next_step_action_) {
      case StepNone:
        break;
      case StepIn:
        hit_step = true;
        break;
      case StepOut:
        hit_step = thread->GetFrameCount() < last_step_stack_depth_;
        break;
      case StepNext: {
        hit_step = thread->GetFrameCount() == last_step_stack_depth_;
        break;
      }
      default:
        UNREACHABLE();
    }
    if (!hit_step) return;
    ClearStepping();
    isolate_->debug()->OnDebugBreak(isolate_->factory()->empty_fixed_array());
  }

  int GetTopPosition(Handle<WasmModuleObject> module_object) {
    DCHECK_EQ(1, interpreter()->GetThreadCount());
    WasmInterpreter::Thread* thread = interpreter()->GetThread(0);
    DCHECK_LT(0, thread->GetFrameCount());

    auto frame = thread->GetFrame(thread->GetFrameCount() - 1);
    return GetWasmFunctionOffset(module_object->module(),
                                 frame->function()->func_index) +
           frame->pc();
  }

  std::vector<std::pair<uint32_t, int>> GetInterpretedStack(
      Address frame_pointer) {
    DCHECK_EQ(1, interpreter()->GetThreadCount());
    WasmInterpreter::Thread* thread = interpreter()->GetThread(0);

    std::pair<uint32_t, uint32_t> frame_range =
        GetActivationFrameRange(thread, frame_pointer);

    std::vector<std::pair<uint32_t, int>> stack;
    stack.reserve(frame_range.second - frame_range.first);
    for (uint32_t fp = frame_range.first; fp < frame_range.second; ++fp) {
      auto frame = thread->GetFrame(fp);
      stack.emplace_back(frame->function()->func_index, frame->pc());
    }
    return stack;
  }

  int NumberOfActiveFrames(Address frame_pointer) {
    if (!HasActivation(frame_pointer)) return 0;

    DCHECK_EQ(1, interpreter()->GetThreadCount());
    WasmInterpreter::Thread* thread = interpreter()->GetThread(0);

    std::pair<uint32_t, uint32_t> frame_range =
        GetActivationFrameRange(thread, frame_pointer);

    return frame_range.second - frame_range.first;
  }

  WasmInterpreter::FramePtr GetInterpretedFrame(Address frame_pointer,
                                                int idx) {
    DCHECK_EQ(1, interpreter()->GetThreadCount());
    WasmInterpreter::Thread* thread = interpreter()->GetThread(0);

    std::pair<uint32_t, uint32_t> frame_range =
        GetActivationFrameRange(thread, frame_pointer);
    DCHECK_LE(0, idx);
    DCHECK_GT(frame_range.second - frame_range.first, idx);

    return thread->GetFrame(frame_range.first + idx);
  }

  uint64_t NumInterpretedCalls() {
    DCHECK_EQ(1, interpreter()->GetThreadCount());
    return interpreter()->GetThread(0)->NumInterpretedCalls();
  }

  Handle<JSObject> GetLocalScopeObject(InterpretedFrame* frame,
                                       Handle<WasmDebugInfo> debug_info) {
    Isolate* isolate = isolate_;

    Handle<JSObject> local_scope_object =
        isolate_->factory()->NewJSObjectWithNullProto();
    // Fill parameters and locals.
    int num_locals = frame->GetLocalCount();
    DCHECK_LE(frame->GetParameterCount(), num_locals);
    if (num_locals > 0) {
      Handle<JSObject> locals_obj =
          isolate_->factory()->NewJSObjectWithNullProto();
      Handle<String> locals_name =
          isolate_->factory()->InternalizeString(StaticCharVector("locals"));
      JSObject::AddProperty(isolate, local_scope_object, locals_name,
                            locals_obj, NONE);
      NativeModule* native_module =
          debug_info->wasm_instance().module_object().native_module();
      for (int i = 0; i < num_locals; ++i) {
        Handle<Name> name;
        if (!GetLocalNameString(isolate, native_module,
                                frame->function()->func_index, i)
                 .ToHandle(&name)) {
          name = PrintFToOneByteString<true>(isolate_, "var%d", i);
        }
        WasmValue value = frame->GetLocalValue(i);
        Handle<Object> value_obj = WasmValueToValueObject(isolate_, value);
        // {name} can be a string representation of an element index.
        LookupIterator::Key lookup_key{isolate, name};
        LookupIterator it(isolate, locals_obj, lookup_key, locals_obj,
                          LookupIterator::OWN_SKIP_INTERCEPTOR);
        if (it.IsFound()) continue;
        Object::AddDataProperty(&it, value_obj, NONE,
                                Just(ShouldThrow::kThrowOnError),
                                StoreOrigin::kNamed)
            .Check();
      }
    }

    // Fill stack values.
    int stack_count = frame->GetStackHeight();
    // Use an object without prototype instead of an Array, for nicer displaying
    // in DevTools. For Arrays, the length field and prototype is displayed,
    // which does not make too much sense here.
    Handle<JSObject> stack_obj =
        isolate_->factory()->NewJSObjectWithNullProto();
    Handle<String> stack_name =
        isolate_->factory()->InternalizeString(StaticCharVector("stack"));
    JSObject::AddProperty(isolate, local_scope_object, stack_name, stack_obj,
                          NONE);
    for (int i = 0; i < stack_count; ++i) {
      WasmValue value = frame->GetStackValue(i);
      Handle<Object> value_obj = WasmValueToValueObject(isolate_, value);
      JSObject::AddDataElement(stack_obj, static_cast<uint32_t>(i), value_obj,
                               NONE);
    }
    return local_scope_object;
  }

  Handle<JSObject> GetStackScopeObject(InterpretedFrame* frame,
                                       Handle<WasmDebugInfo> debug_info) {
    // Fill stack values.
    int stack_count = frame->GetStackHeight();
    // Use an object without prototype instead of an Array, for nicer displaying
    // in DevTools. For Arrays, the length field and prototype is displayed,
    // which does not make too much sense here.
    Handle<JSObject> stack_scope_obj =
        isolate_->factory()->NewJSObjectWithNullProto();
    for (int i = 0; i < stack_count; ++i) {
      WasmValue value = frame->GetStackValue(i);
      Handle<Object> value_obj = WasmValueToValueObject(isolate_, value);
      JSObject::AddDataElement(stack_scope_obj, static_cast<uint32_t>(i),
                               value_obj, NONE);
    }
    return stack_scope_obj;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InterpreterHandle);
};

int FindByteOffset(int pc_offset, WasmCode* wasm_code) {
  int position = 0;
  SourcePositionTableIterator iterator(wasm_code->source_positions());
  for (SourcePositionTableIterator iterator(wasm_code->source_positions());
       !iterator.done() && iterator.code_offset() < pc_offset;
       iterator.Advance()) {
    position = iterator.source_position().ScriptOffset();
  }
  return position;
}

// Generate a sorted and deduplicated list of byte offsets for this function's
// current positions on the stack.
std::vector<int> StackFramePositions(int func_index, Isolate* isolate) {
  std::vector<int> byte_offsets;
  WasmCodeRefScope code_ref_scope;
  for (StackTraceFrameIterator it(isolate); !it.done(); it.Advance()) {
    if (!it.is_wasm()) continue;
    WasmCompiledFrame* frame = WasmCompiledFrame::cast(it.frame());
    if (static_cast<int>(frame->function_index()) != func_index) continue;
    WasmCode* wasm_code = frame->wasm_code();
    if (!wasm_code->is_liftoff()) continue;
    int pc_offset =
        static_cast<int>(frame->pc() - wasm_code->instruction_start());
    int byte_offset = FindByteOffset(pc_offset, wasm_code);
    byte_offsets.push_back(byte_offset);
  }
  std::sort(byte_offsets.begin(), byte_offsets.end());
  auto last = std::unique(byte_offsets.begin(), byte_offsets.end());
  byte_offsets.erase(last, byte_offsets.end());
  return byte_offsets;
}

enum ReturnLocation { kAfterBreakpoint, kAfterWasmCall };

Address FindNewPC(WasmCode* wasm_code, int byte_offset,
                  ReturnLocation return_location) {
  Vector<const uint8_t> new_pos_table = wasm_code->source_positions();

  DCHECK_LE(0, byte_offset);

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
    return wasm_code->instruction_start() + it.code_offset();
  }

  DCHECK_EQ(kAfterWasmCall, return_location);
  int code_offset;
  do {
    code_offset = it.code_offset();
    it.Advance();
  } while (!it.done() && it.source_position().ScriptOffset() == byte_offset);
  return wasm_code->instruction_start() + code_offset;
}

}  // namespace

Handle<JSObject> GetGlobalScopeObject(Handle<WasmInstanceObject> instance) {
  Isolate* isolate = instance->GetIsolate();

  Handle<JSObject> global_scope_object =
      isolate->factory()->NewJSObjectWithNullProto();
  if (instance->has_memory_object()) {
    Handle<String> name =
        isolate->factory()->InternalizeString(StaticCharVector("memory"));
    Handle<JSArrayBuffer> memory_buffer(
        instance->memory_object().array_buffer(), isolate);
    Handle<JSTypedArray> uint8_array = isolate->factory()->NewJSTypedArray(
        kExternalUint8Array, memory_buffer, 0, memory_buffer->byte_length());
    JSObject::AddProperty(isolate, global_scope_object, name, uint8_array,
                          NONE);
  }

  auto& globals = instance->module()->globals;
  if (globals.size() > 0) {
    Handle<JSObject> globals_obj =
        isolate->factory()->NewJSObjectWithNullProto();
    Handle<String> globals_name =
        isolate->factory()->InternalizeString(StaticCharVector("globals"));
    JSObject::AddProperty(isolate, global_scope_object, globals_name,
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

  return global_scope_object;
}

class DebugInfoImpl {
 public:
  explicit DebugInfoImpl(NativeModule* native_module)
      : native_module_(native_module) {}

  Handle<JSObject> GetLocalScopeObject(Isolate* isolate, Address pc, Address fp,
                                       Address debug_break_fp) {
    Handle<JSObject> local_scope_object =
        isolate->factory()->NewJSObjectWithNullProto();

    wasm::WasmCodeRefScope wasm_code_ref_scope;
    wasm::WasmCode* code =
        isolate->wasm_engine()->code_manager()->LookupCode(pc);
    // Only Liftoff code can be inspected.
    if (!code->is_liftoff()) return local_scope_object;

    auto* module = native_module_->module();
    auto* function = &module->functions[code->index()];
    auto* debug_side_table = GetDebugSideTable(code, isolate->allocator());
    int pc_offset = static_cast<int>(pc - code->instruction_start());
    auto* debug_side_table_entry = debug_side_table->GetEntry(pc_offset);
    DCHECK_NOT_NULL(debug_side_table_entry);

    // Fill parameters and locals.
    int num_locals = static_cast<int>(debug_side_table->num_locals());
    DCHECK_LE(static_cast<int>(function->sig->parameter_count()), num_locals);
    if (num_locals > 0) {
      Handle<JSObject> locals_obj =
          isolate->factory()->NewJSObjectWithNullProto();
      Handle<String> locals_name =
          isolate->factory()->InternalizeString(StaticCharVector("locals"));
      JSObject::AddProperty(isolate, local_scope_object, locals_name,
                            locals_obj, NONE);
      for (int i = 0; i < num_locals; ++i) {
        Handle<Name> name;
        if (!GetLocalNameString(isolate, native_module_, function->func_index,
                                i)
                 .ToHandle(&name)) {
          name = PrintFToOneByteString<true>(isolate, "var%d", i);
        }
        WasmValue value =
            GetValue(debug_side_table_entry, i, fp, debug_break_fp);
        Handle<Object> value_obj = WasmValueToValueObject(isolate, value);
        // {name} can be a string representation of an element index.
        LookupIterator::Key lookup_key{isolate, name};
        LookupIterator it(isolate, locals_obj, lookup_key, locals_obj,
                          LookupIterator::OWN_SKIP_INTERCEPTOR);
        if (it.IsFound()) continue;
        Object::AddDataProperty(&it, value_obj, NONE,
                                Just(ShouldThrow::kThrowOnError),
                                StoreOrigin::kNamed)
            .Check();
      }
    }

    // Fill stack values.
    // Use an object without prototype instead of an Array, for nicer displaying
    // in DevTools. For Arrays, the length field and prototype is displayed,
    // which does not make too much sense here.
    Handle<JSObject> stack_obj = isolate->factory()->NewJSObjectWithNullProto();
    Handle<String> stack_name =
        isolate->factory()->InternalizeString(StaticCharVector("stack"));
    JSObject::AddProperty(isolate, local_scope_object, stack_name, stack_obj,
                          NONE);
    int value_count = debug_side_table_entry->num_values();
    for (int i = num_locals; i < value_count; ++i) {
      WasmValue value = GetValue(debug_side_table_entry, i, fp, debug_break_fp);
      Handle<Object> value_obj = WasmValueToValueObject(isolate, value);
      JSObject::AddDataElement(stack_obj, static_cast<uint32_t>(i - num_locals),
                               value_obj, NONE);
    }
    return local_scope_object;
  }

  Handle<JSObject> GetStackScopeObject(Isolate* isolate, Address pc, Address fp,
                                       Address debug_break_fp) {
    Handle<JSObject> stack_scope_obj =
        isolate->factory()->NewJSObjectWithNullProto();
    wasm::WasmCodeRefScope wasm_code_ref_scope;

    wasm::WasmCode* code =
        isolate->wasm_engine()->code_manager()->LookupCode(pc);
    // Only Liftoff code can be inspected.
    if (!code->is_liftoff()) return stack_scope_obj;

    auto* debug_side_table = GetDebugSideTable(code, isolate->allocator());
    int pc_offset = static_cast<int>(pc - code->instruction_start());
    auto* debug_side_table_entry = debug_side_table->GetEntry(pc_offset);
    DCHECK_NOT_NULL(debug_side_table_entry);

    // Fill stack values.
    // Use an object without prototype instead of an Array, for nicer displaying
    // in DevTools. For Arrays, the length field and prototype is displayed,
    // which does not make too much sense here.
    int num_locals = static_cast<int>(debug_side_table->num_locals());
    int value_count = debug_side_table_entry->num_values();
    for (int i = num_locals; i < value_count; ++i) {
      WasmValue value = GetValue(debug_side_table_entry, i, fp, debug_break_fp);
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

  void RecompileLiftoffWithBreakpoints(int func_index, Vector<int> offsets,
                                       Isolate* current_isolate) {
    if (func_index == flooded_function_index_) {
      // We should not be flooding a function that is already flooded.
      DCHECK(!(offsets.size() == 1 && offsets[0] == 0));
      flooded_function_index_ = -1;
    }
    // Recompile the function with Liftoff, setting the new breakpoints.
    // Not thread-safe. The caller is responsible for locking {mutex_}.
    CompilationEnv env = native_module_->CreateCompilationEnv();
    auto* function = &native_module_->module()->functions[func_index];
    Vector<const uint8_t> wire_bytes = native_module_->wire_bytes();
    FunctionBody body{function->sig, function->code.offset(),
                      wire_bytes.begin() + function->code.offset(),
                      wire_bytes.begin() + function->code.end_offset()};
    std::unique_ptr<DebugSideTable> debug_sidetable;

    // Generate additional source positions for current stack frame positions.
    // These source positions are used to find return addresses in the new code.
    std::vector<int> stack_frame_positions =
        StackFramePositions(func_index, current_isolate);

    WasmCompilationResult result = ExecuteLiftoffCompilation(
        native_module_->engine()->allocator(), &env, body, func_index, nullptr,
        nullptr, offsets, &debug_sidetable, VectorOf(stack_frame_positions));
    // Liftoff compilation failure is a FATAL error. We rely on complete Liftoff
    // support for debugging.
    if (!result.succeeded()) FATAL("Liftoff compilation failed");
    DCHECK_NOT_NULL(debug_sidetable);

    WasmCodeRefScope wasm_code_ref_scope;
    WasmCode* new_code = native_module_->AddCompiledCode(std::move(result));
    bool added =
        debug_side_tables_.emplace(new_code, std::move(debug_sidetable)).second;
    DCHECK(added);
    USE(added);

    UpdateReturnAddresses(current_isolate, new_code);
  }

  void SetBreakpoint(int func_index, int offset, Isolate* current_isolate) {
    // Hold the mutex while setting the breakpoint. This guards against multiple
    // isolates setting breakpoints at the same time. We don't really support
    // that scenario yet, but concurrently compiling and installing different
    // Liftoff variants of a function would be problematic.
    base::MutexGuard guard(&mutex_);

    // offset == 0 indicates flooding and should not happen here.
    DCHECK_NE(0, offset);

    std::vector<int>& breakpoints = breakpoints_per_function_[func_index];
    auto insertion_point =
        std::lower_bound(breakpoints.begin(), breakpoints.end(), offset);
    if (insertion_point != breakpoints.end() && *insertion_point == offset) {
      // The breakpoint is already set.
      return;
    }
    breakpoints.insert(insertion_point, offset);

    // No need to recompile if the function is already flooded.
    if (func_index == flooded_function_index_) return;

    RecompileLiftoffWithBreakpoints(func_index, VectorOf(breakpoints),
                                    current_isolate);
  }

  void FloodWithBreakpoints(int func_index, Isolate* current_isolate) {
    base::MutexGuard guard(&mutex_);
    // 0 is an invalid offset used to indicate flooding.
    int offset = 0;
    RecompileLiftoffWithBreakpoints(func_index, Vector<int>(&offset, 1),
                                    current_isolate);
  }

  void PrepareStep(Isolate* isolate, StackFrameId break_frame_id) {
    StackTraceFrameIterator it(isolate, break_frame_id);
    DCHECK(!it.done());
    DCHECK(it.frame()->is_wasm_compiled());
    WasmCompiledFrame* frame = WasmCompiledFrame::cast(it.frame());
    StepAction step_action = isolate->debug()->last_step_action();

    // If we are at a return instruction, then any stepping action is equivalent
    // to StepOut, and we need to flood the parent function.
    if (IsAtReturn(frame) || step_action == StepOut) {
      it.Advance();
      if (it.done() || !it.frame()->is_wasm_compiled()) return;
      frame = WasmCompiledFrame::cast(it.frame());
    }

    if (static_cast<int>(frame->function_index()) != flooded_function_index_) {
      if (flooded_function_index_ != -1) {
        std::vector<int>& breakpoints =
            breakpoints_per_function_[flooded_function_index_];
        RecompileLiftoffWithBreakpoints(flooded_function_index_,
                                        VectorOf(breakpoints), isolate);
      }
      FloodWithBreakpoints(frame->function_index(), isolate);
      flooded_function_index_ = frame->function_index();
    }
    stepping_frame_ = frame->id();
  }

  void ClearStepping() { stepping_frame_ = NO_ID; }

  bool IsStepping(WasmCompiledFrame* frame) {
    Isolate* isolate = frame->wasm_instance().GetIsolate();
    StepAction last_step_action = isolate->debug()->last_step_action();
    return stepping_frame_ == frame->id() || last_step_action == StepIn;
  }

  void RemoveBreakpoint(int func_index, int position,
                        Isolate* current_isolate) {
    base::MutexGuard guard(&mutex_);
    const auto& function = native_module_->module()->functions[func_index];
    int offset = position - function.code.offset();

    std::vector<int>& breakpoints = breakpoints_per_function_[func_index];
    DCHECK_LT(0, offset);
    auto insertion_point =
        std::lower_bound(breakpoints.begin(), breakpoints.end(), offset);
    if (insertion_point != breakpoints.end() && *insertion_point == offset) {
      breakpoints.erase(insertion_point);
    }
    RecompileLiftoffWithBreakpoints(func_index, VectorOf(breakpoints),
                                    current_isolate);
  }

  void RemoveDebugSideTables(Vector<WasmCode* const> codes) {
    base::MutexGuard guard(&mutex_);
    for (auto* code : codes) {
      debug_side_tables_.erase(code);
    }
  }

 private:
  const DebugSideTable* GetDebugSideTable(WasmCode* code,
                                          AccountingAllocator* allocator) {
    base::MutexGuard guard(&mutex_);
    if (auto& existing_table = debug_side_tables_[code]) {
      return existing_table.get();
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
        GenerateLiftoffDebugSideTable(allocator, &env, func_body);
    DebugSideTable* ret = debug_side_table.get();

    // Install into cache and return.
    debug_side_tables_[code] = std::move(debug_side_table);
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
  // TODO(thibaudm): update other threads as well.
  void UpdateReturnAddresses(Isolate* isolate, WasmCode* new_code) {
    DCHECK(new_code->is_liftoff());
    // The first return location is after the breakpoint, others are after wasm
    // calls.
    ReturnLocation return_location = kAfterBreakpoint;
    for (StackTraceFrameIterator it(isolate); !it.done();
         it.Advance(), return_location = kAfterWasmCall) {
      // We still need the flooded function for stepping.
      if (it.frame()->id() == stepping_frame_) continue;
      if (!it.is_wasm()) continue;
      WasmCompiledFrame* frame = WasmCompiledFrame::cast(it.frame());
      if (frame->native_module() != new_code->native_module()) continue;
      if (frame->function_index() != new_code->index()) continue;
      WasmCode* old_code = frame->wasm_code();
      if (!old_code->is_liftoff()) continue;
      int pc_offset =
          static_cast<int>(frame->pc() - old_code->instruction_start());
      int position = frame->position();
      int byte_offset = FindByteOffset(pc_offset, old_code);
      Address new_pc = FindNewPC(new_code, byte_offset, return_location);
      PointerAuthentication::ReplacePC(frame->pc_address(), new_pc,
                                       kSystemPointerSize);
      USE(position);
      // The frame position should still be the same after OSR.
      DCHECK_EQ(position, frame->position());
    }
  }

  bool IsAtReturn(WasmCompiledFrame* frame) {
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

  NativeModule* const native_module_;

  // {mutex_} protects all fields below.
  mutable base::Mutex mutex_;

  // DebugSideTable per code object, lazily initialized.
  std::unordered_map<WasmCode*, std::unique_ptr<DebugSideTable>>
      debug_side_tables_;

  // Names of locals, lazily decoded from the wire bytes.
  std::unique_ptr<LocalNames> local_names_;

  // Keeps track of the currently set breakpoints (by offset within that
  // function).
  std::unordered_map<int, std::vector<int>> breakpoints_per_function_;

  // Store the frame ID when stepping, to avoid breaking in recursive calls of
  // the same function.
  StackFrameId stepping_frame_ = NO_ID;
  int flooded_function_index_ = -1;

  DISALLOW_COPY_AND_ASSIGN(DebugInfoImpl);
};

DebugInfo::DebugInfo(NativeModule* native_module)
    : impl_(std::make_unique<DebugInfoImpl>(native_module)) {}

DebugInfo::~DebugInfo() = default;

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

void DebugInfo::ClearStepping() { impl_->ClearStepping(); }

bool DebugInfo::IsStepping(WasmCompiledFrame* frame) {
  return impl_->IsStepping(frame);
}

void DebugInfo::RemoveBreakpoint(int func_index, int offset,
                                 Isolate* current_isolate) {
  impl_->RemoveBreakpoint(func_index, offset, current_isolate);
}

void DebugInfo::RemoveDebugSideTables(Vector<WasmCode* const> code) {
  impl_->RemoveDebugSideTables(code);
}

}  // namespace wasm

namespace {

wasm::InterpreterHandle* GetOrCreateInterpreterHandle(
    Isolate* isolate, Handle<WasmDebugInfo> debug_info) {
  Handle<Object> handle(debug_info->interpreter_handle(), isolate);
  if (handle->IsUndefined(isolate)) {
    // Use the maximum stack size to estimate the maximum size of the
    // interpreter. The interpreter keeps its own stack internally, and the size
    // of the stack should dominate the overall size of the interpreter. We
    // multiply by '2' to account for the growing strategy for the backing store
    // of the stack.
    size_t interpreter_size = FLAG_stack_size * KB * 2;
    handle = Managed<wasm::InterpreterHandle>::Allocate(
        isolate, interpreter_size, isolate, debug_info);
    debug_info->set_interpreter_handle(*handle);
  }

  return Handle<Managed<wasm::InterpreterHandle>>::cast(handle)->raw();
}

wasm::InterpreterHandle* GetInterpreterHandle(WasmDebugInfo debug_info) {
  Object handle_obj = debug_info.interpreter_handle();
  DCHECK(!handle_obj.IsUndefined());
  return Managed<wasm::InterpreterHandle>::cast(handle_obj).raw();
}

wasm::InterpreterHandle* GetInterpreterHandleOrNull(WasmDebugInfo debug_info) {
  Object handle_obj = debug_info.interpreter_handle();
  if (handle_obj.IsUndefined()) return nullptr;
  return Managed<wasm::InterpreterHandle>::cast(handle_obj).raw();
}

}  // namespace

Handle<WasmDebugInfo> WasmDebugInfo::New(Handle<WasmInstanceObject> instance) {
  DCHECK(!instance->has_debug_info());
  Factory* factory = instance->GetIsolate()->factory();
  Handle<Cell> stack_cell = factory->NewCell(factory->empty_fixed_array());
  Handle<WasmDebugInfo> debug_info = Handle<WasmDebugInfo>::cast(
      factory->NewStruct(WASM_DEBUG_INFO_TYPE, AllocationType::kOld));
  debug_info->set_wasm_instance(*instance);
  debug_info->set_interpreter_reference_stack(*stack_cell);
  instance->set_debug_info(*debug_info);
  return debug_info;
}

wasm::WasmInterpreter* WasmDebugInfo::SetupForTesting(
    Handle<WasmInstanceObject> instance_obj) {
  Handle<WasmDebugInfo> debug_info = WasmDebugInfo::New(instance_obj);
  Isolate* isolate = instance_obj->GetIsolate();
  // Use the maximum stack size to estimate the maximum size of the interpreter.
  // The interpreter keeps its own stack internally, and the size of the stack
  // should dominate the overall size of the interpreter. We multiply by '2' to
  // account for the growing strategy for the backing store of the stack.
  size_t interpreter_size = FLAG_stack_size * KB * 2;
  auto interp_handle = Managed<wasm::InterpreterHandle>::Allocate(
      isolate, interpreter_size, isolate, debug_info);
  debug_info->set_interpreter_handle(*interp_handle);
  return interp_handle->raw()->interpreter();
}

// static
void WasmDebugInfo::PrepareStepIn(Handle<WasmDebugInfo> debug_info,
                                  int func_index) {
  Isolate* isolate = debug_info->GetIsolate();
  auto* handle = GetOrCreateInterpreterHandle(isolate, debug_info);
  RedirectToInterpreter(debug_info, Vector<int>(&func_index, 1));
  const wasm::WasmFunction* func = &handle->module()->functions[func_index];
  handle->interpreter()->PrepareStepIn(func);
  // Debug break would be considered as a step-in inside wasm.
  handle->PrepareStep(StepAction::StepIn);
}

// static
void WasmDebugInfo::SetBreakpoint(Handle<WasmDebugInfo> debug_info,
                                  int func_index, int offset) {
  Isolate* isolate = debug_info->GetIsolate();
  auto* handle = GetOrCreateInterpreterHandle(isolate, debug_info);
  RedirectToInterpreter(debug_info, Vector<int>(&func_index, 1));
  const wasm::WasmFunction* func = &handle->module()->functions[func_index];
  handle->interpreter()->SetBreakpoint(func, offset, true);
}

// static
void WasmDebugInfo::ClearBreakpoint(Handle<WasmDebugInfo> debug_info,
                                    int func_index, int offset) {
  Isolate* isolate = debug_info->GetIsolate();
  auto* handle = GetOrCreateInterpreterHandle(isolate, debug_info);
  // TODO(leese): If there are no more breakpoints left it would be good to
  // undo redirecting to the interpreter.
  const wasm::WasmFunction* func = &handle->module()->functions[func_index];
  handle->interpreter()->SetBreakpoint(func, offset, false);
}

// static
void WasmDebugInfo::RedirectToInterpreter(Handle<WasmDebugInfo> debug_info,
                                          Vector<int> func_indexes) {
  Isolate* isolate = debug_info->GetIsolate();
  // Ensure that the interpreter is instantiated.
  GetOrCreateInterpreterHandle(isolate, debug_info);
  Handle<WasmInstanceObject> instance(debug_info->wasm_instance(), isolate);
  wasm::NativeModule* native_module = instance->module_object().native_module();
  const wasm::WasmModule* module = instance->module();

  // We may modify the wasm jump table.
  wasm::NativeModuleModificationScope native_module_modification_scope(
      native_module);

  for (int func_index : func_indexes) {
    DCHECK_LE(0, func_index);
    DCHECK_GT(module->functions.size(), func_index);
    // Note that this is just a best effort check. Multiple threads can still
    // race at redirecting the same function to the interpreter, which is OK.
    if (native_module->IsRedirectedToInterpreter(func_index)) continue;

    wasm::WasmCodeRefScope code_ref_scope;
    wasm::WasmCompilationResult result = compiler::CompileWasmInterpreterEntry(
        isolate->wasm_engine(), native_module->enabled_features(), func_index,
        module->functions[func_index].sig);
    std::unique_ptr<wasm::WasmCode> wasm_code = native_module->AddCode(
        func_index, result.code_desc, result.frame_slot_count,
        result.tagged_parameter_slots,
        result.protected_instructions_data.as_vector(),
        result.source_positions.as_vector(), wasm::WasmCode::kInterpreterEntry,
        wasm::ExecutionTier::kInterpreter);
    native_module->PublishCode(std::move(wasm_code));
    DCHECK(native_module->IsRedirectedToInterpreter(func_index));
  }
}

void WasmDebugInfo::PrepareStep(StepAction step_action) {
  GetInterpreterHandle(*this)->PrepareStep(step_action);
}

// static
bool WasmDebugInfo::RunInterpreter(Isolate* isolate,
                                   Handle<WasmDebugInfo> debug_info,
                                   Address frame_pointer, int func_index,
                                   Vector<wasm::WasmValue> argument_values,
                                   Vector<wasm::WasmValue> return_values) {
  DCHECK_LE(0, func_index);
  auto* handle = GetOrCreateInterpreterHandle(isolate, debug_info);
  Handle<WasmInstanceObject> instance(debug_info->wasm_instance(), isolate);
  return handle->Execute(instance, frame_pointer,
                         static_cast<uint32_t>(func_index), argument_values,
                         return_values);
}

std::vector<std::pair<uint32_t, int>> WasmDebugInfo::GetInterpretedStack(
    Address frame_pointer) {
  return GetInterpreterHandle(*this)->GetInterpretedStack(frame_pointer);
}

int WasmDebugInfo::NumberOfActiveFrames(Address frame_pointer) {
  return GetInterpreterHandle(*this)->NumberOfActiveFrames(frame_pointer);
}

wasm::WasmInterpreter::FramePtr WasmDebugInfo::GetInterpretedFrame(
    Address frame_pointer, int idx) {
  return GetInterpreterHandle(*this)->GetInterpretedFrame(frame_pointer, idx);
}

uint64_t WasmDebugInfo::NumInterpretedCalls() {
  auto* handle = GetInterpreterHandleOrNull(*this);
  return handle ? handle->NumInterpretedCalls() : 0;
}

// static
Handle<JSObject> WasmDebugInfo::GetLocalScopeObject(
    Handle<WasmDebugInfo> debug_info, Address frame_pointer, int frame_index) {
  auto* interp_handle = GetInterpreterHandle(*debug_info);
  auto frame = interp_handle->GetInterpretedFrame(frame_pointer, frame_index);
  return interp_handle->GetLocalScopeObject(frame.get(), debug_info);
}

// static
Handle<JSObject> WasmDebugInfo::GetStackScopeObject(
    Handle<WasmDebugInfo> debug_info, Address frame_pointer, int frame_index) {
  auto* interp_handle = GetInterpreterHandle(*debug_info);
  auto frame = interp_handle->GetInterpretedFrame(frame_pointer, frame_index);
  return interp_handle->GetStackScopeObject(frame.get(), debug_info);
}

// static
Handle<Code> WasmDebugInfo::GetCWasmEntry(Handle<WasmDebugInfo> debug_info,
                                          const wasm::FunctionSig* sig) {
  Isolate* isolate = debug_info->GetIsolate();
  DCHECK_EQ(debug_info->has_c_wasm_entries(),
            debug_info->has_c_wasm_entry_map());
  if (!debug_info->has_c_wasm_entries()) {
    auto entries = isolate->factory()->NewFixedArray(4, AllocationType::kOld);
    debug_info->set_c_wasm_entries(*entries);
    size_t map_size = 0;  // size estimate not so important here.
    auto managed_map = Managed<wasm::SignatureMap>::Allocate(isolate, map_size);
    debug_info->set_c_wasm_entry_map(*managed_map);
  }
  Handle<FixedArray> entries(debug_info->c_wasm_entries(), isolate);
  wasm::SignatureMap* map = debug_info->c_wasm_entry_map().raw();
  int32_t index = map->Find(*sig);
  if (index == -1) {
    index = static_cast<int32_t>(map->FindOrInsert(*sig));
    if (index == entries->length()) {
      entries =
          isolate->factory()->CopyFixedArrayAndGrow(entries, entries->length());
      debug_info->set_c_wasm_entries(*entries);
    }
    DCHECK(entries->get(index).IsUndefined(isolate));
    Handle<Code> new_entry_code =
        compiler::CompileCWasmEntry(isolate, sig).ToHandleChecked();
    entries->set(index, *new_entry_code);
  }
  return handle(Code::cast(entries->get(index)), isolate);
}

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

  if (FLAG_debug_in_liftoff) {
    native_module->GetDebugInfo()->SetBreakpoint(func_index, offset, isolate);
  } else {
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
        WasmDebugInfo::SetBreakpoint(debug_info, func_index, offset);
      }
    }
  }

  return true;
}

// static
bool WasmScript::ClearBreakPoint(Handle<Script> script, int position,
                                 Handle<BreakPoint> break_point) {
  Isolate* isolate = script->GetIsolate();

  // Find the function for this breakpoint.
  const wasm::WasmModule* module = script->wasm_native_module()->module();
  int func_index = GetContainingWasmFunction(module, position);
  if (func_index < 0) return false;
  const wasm::WasmFunction& func = module->functions[func_index];
  int offset_in_func = position - func.code.offset();

  if (!WasmScript::RemoveBreakpointFromInfo(script, position, break_point)) {
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
bool WasmScript::RemoveBreakpointFromInfo(Handle<Script> script, int position,
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
  return true;
}

void WasmScript::SetBreakpointsOnNewInstance(
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
    const wasm::WasmModule* module = script->wasm_native_module()->module();
    int func_index = GetContainingWasmFunction(module, position);
    DCHECK_LE(0, func_index);
    const wasm::WasmFunction& func = module->functions[func_index];
    int offset_in_func = position - func.code.offset();
    WasmDebugInfo::SetBreakpoint(debug_info, func_index, offset_in_func);
  }
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
