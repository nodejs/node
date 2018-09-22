// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_map>

#include "src/assembler-inl.h"
#include "src/assert-scope.h"
#include "src/base/optional.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/debug-scopes.h"
#include "src/debug/debug.h"
#include "src/frames-inl.h"
#include "src/heap/factory.h"
#include "src/identity-map.h"
#include "src/isolate.h"
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
  // Maximum length of a formatted value name ("param#%d", "local#%d",
  // "global#%d").
  constexpr int kMaxStrLen = 18;
  EmbeddedVector<char, kMaxStrLen> value;
  int len = SNPrintF(value, format, args...);
  CHECK(len > 0 && len < value.length());
  Vector<uint8_t> name = Vector<uint8_t>::cast(value.SubVector(0, len));
  return internal
             ? isolate->factory()->InternalizeOneByteString(name)
             : isolate->factory()->NewStringFromOneByte(name).ToHandleChecked();
}

Handle<Object> WasmValueToValueObject(Isolate* isolate, WasmValue value) {
  switch (value.type()) {
    case kWasmI32:
      if (Smi::IsValid(value.to<int32_t>()))
        return handle(Smi::FromInt(value.to<int32_t>()), isolate);
      return PrintFToOneByteString<false>(isolate, "%d", value.to<int32_t>());
    case kWasmI64:
      if (Smi::IsValid(value.to<int64_t>()))
        return handle(Smi::FromIntptr(value.to<int64_t>()), isolate);
      return PrintFToOneByteString<false>(isolate, "%" PRId64,
                                          value.to<int64_t>());
    case kWasmF32:
      return isolate->factory()->NewNumber(value.to<float>());
    case kWasmF64:
      return isolate->factory()->NewNumber(value.to<double>());
    default:
      UNIMPLEMENTED();
      return isolate->factory()->undefined_value();
  }
}

MaybeHandle<String> GetLocalName(Isolate* isolate,
                                 Handle<WasmDebugInfo> debug_info,
                                 int func_index, int local_index) {
  DCHECK_LE(0, func_index);
  DCHECK_LE(0, local_index);
  if (!debug_info->has_locals_names()) {
    Handle<WasmModuleObject> module_object(
        debug_info->wasm_instance()->module_object(), isolate);
    Handle<FixedArray> locals_names = DecodeLocalNames(isolate, module_object);
    debug_info->set_locals_names(*locals_names);
  }

  Handle<FixedArray> locals_names(debug_info->locals_names(), isolate);
  if (func_index >= locals_names->length() ||
      locals_names->get(func_index)->IsUndefined(isolate)) {
    return {};
  }

  Handle<FixedArray> func_locals_names(
      FixedArray::cast(locals_names->get(func_index)), isolate);
  if (local_index >= func_locals_names->length() ||
      func_locals_names->get(local_index)->IsUndefined(isolate)) {
    return {};
  }
  return handle(String::cast(func_locals_names->get(local_index)), isolate);
}

class InterpreterHandle {
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(InterpreterHandle);
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

  static Vector<const byte> GetBytes(WasmDebugInfo* debug_info) {
    // Return raw pointer into heap. The WasmInterpreter will make its own copy
    // of this data anyway, and there is no heap allocation in-between.
    NativeModule* native_module =
        debug_info->wasm_instance()->module_object()->native_module();
    return native_module->wire_bytes();
  }

 public:
  InterpreterHandle(Isolate* isolate, Handle<WasmDebugInfo> debug_info)
      : isolate_(isolate),
        module_(debug_info->wasm_instance()->module_object()->module()),
        interpreter_(isolate, module_, GetBytes(*debug_info),
                     handle(debug_info->wasm_instance(), isolate)) {}

  ~InterpreterHandle() { DCHECK_EQ(0, activations_.size()); }

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
               Address frame_pointer, uint32_t func_index, Address arg_buffer) {
    DCHECK_GE(module()->functions.size(), func_index);
    FunctionSig* sig = module()->functions[func_index].sig;
    DCHECK_GE(kMaxInt, sig->parameter_count());
    int num_params = static_cast<int>(sig->parameter_count());
    ScopedVector<WasmValue> wasm_args(num_params);
    Address arg_buf_ptr = arg_buffer;
    for (int i = 0; i < num_params; ++i) {
      uint32_t param_size = static_cast<uint32_t>(
          ValueTypes::ElementSizeInBytes(sig->GetParam(i)));
#define CASE_ARG_TYPE(type, ctype)                                    \
  case type:                                                          \
    DCHECK_EQ(param_size, sizeof(ctype));                             \
    wasm_args[i] = WasmValue(ReadUnalignedValue<ctype>(arg_buf_ptr)); \
    break;
      switch (sig->GetParam(i)) {
        CASE_ARG_TYPE(kWasmI32, uint32_t)
        CASE_ARG_TYPE(kWasmI64, uint64_t)
        CASE_ARG_TYPE(kWasmF32, float)
        CASE_ARG_TYPE(kWasmF64, double)
#undef CASE_ARG_TYPE
        default:
          UNREACHABLE();
      }
      arg_buf_ptr += param_size;
    }

    uint32_t activation_id = StartActivation(frame_pointer);

    WasmInterpreter::Thread* thread = interpreter_.GetThread(0);
    thread->InitFrame(&module()->functions[func_index], wasm_args.start());
    bool finished = false;
    while (!finished) {
      // TODO(clemensh): Add occasional StackChecks.
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
          int message_id =
              WasmOpcodes::TrapReasonToMessageId(thread->GetTrapReason());
          Handle<Object> exception = isolate_->factory()->NewWasmRuntimeError(
              static_cast<MessageTemplate::Template>(message_id));
          isolate_->Throw(*exception);
          // Handle this exception. Return without trying to read back the
          // return value.
          auto result = thread->HandleException(isolate_);
          return result == WasmInterpreter::Thread::HANDLED;
        } break;
        case WasmInterpreter::State::STOPPED:
          // An exception happened, and the current activation was unwound.
          DCHECK_EQ(thread->ActivationFrameBase(activation_id),
                    thread->GetFrameCount());
          return false;
        // RUNNING should never occur here.
        case WasmInterpreter::State::RUNNING:
        default:
          UNREACHABLE();
      }
    }

    // Copy back the return value
    DCHECK_GE(kV8MaxWasmFunctionReturns, sig->return_count());
    // TODO(wasm): Handle multi-value returns.
    DCHECK_EQ(1, kV8MaxWasmFunctionReturns);
    if (sig->return_count()) {
      WasmValue ret_val = thread->GetReturnValue(0);
#define CASE_RET_TYPE(type, ctype)                               \
  case type:                                                     \
    DCHECK_EQ(ValueTypes::ElementSizeInBytes(sig->GetReturn(0)), \
              sizeof(ctype));                                    \
    WriteUnalignedValue<ctype>(arg_buffer, ret_val.to<ctype>()); \
    break;
      switch (sig->GetReturn(0)) {
        CASE_RET_TYPE(kWasmI32, uint32_t)
        CASE_RET_TYPE(kWasmI64, uint64_t)
        CASE_RET_TYPE(kWasmF32, float)
        CASE_RET_TYPE(kWasmF64, double)
#undef CASE_RET_TYPE
        default:
          UNREACHABLE();
      }
    }

    FinishActivation(frame_pointer, activation_id);

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
                        instance_obj->debug_info()->interpreter_handle())
                        ->raw());
    return instance_obj;
  }

  void NotifyDebugEventListeners(WasmInterpreter::Thread* thread) {
    // Enter the debugger.
    DebugScope debug_scope(isolate_->debug());

    // Check whether we hit a breakpoint.
    if (isolate_->debug()->break_points_active()) {
      Handle<WasmModuleObject> module_object(
          GetInstanceObject()->module_object(), isolate_);
      int position = GetTopPosition(module_object);
      Handle<FixedArray> breakpoints;
      if (WasmModuleObject::CheckBreakPoints(isolate_, module_object, position)
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
    return module_object->GetFunctionOffset(frame->function()->func_index) +
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

  void Unwind(Address frame_pointer) {
    // Find the current activation.
    DCHECK_EQ(1, activations_.count(frame_pointer));
    // Activations must be properly stacked:
    DCHECK_EQ(activations_.size() - 1, activations_[frame_pointer]);
    uint32_t activation_id = static_cast<uint32_t>(activations_.size() - 1);

    // Unwind the frames of the current activation if not already unwound.
    WasmInterpreter::Thread* thread = interpreter()->GetThread(0);
    if (static_cast<uint32_t>(thread->GetFrameCount()) >
        thread->ActivationFrameBase(activation_id)) {
      using ExceptionResult = WasmInterpreter::Thread::ExceptionHandlingResult;
      ExceptionResult result = thread->HandleException(isolate_);
      // TODO(wasm): Handle exceptions caught in wasm land.
      CHECK_EQ(ExceptionResult::UNWOUND, result);
    }

    FinishActivation(frame_pointer, activation_id);
  }

  uint64_t NumInterpretedCalls() {
    DCHECK_EQ(1, interpreter()->GetThreadCount());
    return interpreter()->GetThread(0)->NumInterpretedCalls();
  }

  Handle<JSObject> GetGlobalScopeObject(InterpretedFrame* frame,
                                        Handle<WasmDebugInfo> debug_info) {
    Isolate* isolate = isolate_;
    Handle<WasmInstanceObject> instance(debug_info->wasm_instance(), isolate);

    // TODO(clemensh): Add globals to the global scope.
    Handle<JSObject> global_scope_object =
        isolate_->factory()->NewJSObjectWithNullProto();
    if (instance->has_memory_object()) {
      Handle<String> name = isolate_->factory()->InternalizeOneByteString(
          STATIC_CHAR_VECTOR("memory"));
      Handle<JSArrayBuffer> memory_buffer(
          instance->memory_object()->array_buffer(), isolate_);
      Handle<JSTypedArray> uint8_array = isolate_->factory()->NewJSTypedArray(
          kExternalUint8Array, memory_buffer, 0, memory_buffer->byte_length());
      JSObject::SetOwnPropertyIgnoreAttributes(global_scope_object, name,
                                               uint8_array, NONE)
          .Assert();
    }
    return global_scope_object;
  }

  Handle<JSObject> GetLocalScopeObject(InterpretedFrame* frame,
                                       Handle<WasmDebugInfo> debug_info) {
    Isolate* isolate = isolate_;

    Handle<JSObject> local_scope_object =
        isolate_->factory()->NewJSObjectWithNullProto();
    // Fill parameters and locals.
    int num_params = frame->GetParameterCount();
    int num_locals = frame->GetLocalCount();
    DCHECK_LE(num_params, num_locals);
    if (num_locals > 0) {
      Handle<JSObject> locals_obj =
          isolate_->factory()->NewJSObjectWithNullProto();
      Handle<String> locals_name =
          isolate_->factory()->InternalizeOneByteString(
              STATIC_CHAR_VECTOR("locals"));
      JSObject::SetOwnPropertyIgnoreAttributes(local_scope_object, locals_name,
                                               locals_obj, NONE)
          .Assert();
      for (int i = 0; i < num_locals; ++i) {
        MaybeHandle<String> name =
            GetLocalName(isolate, debug_info, frame->function()->func_index, i);
        if (name.is_null()) {
          // Parameters should come before locals in alphabetical ordering, so
          // we name them "args" here.
          const char* label = i < num_params ? "arg#%d" : "local#%d";
          name = PrintFToOneByteString<true>(isolate_, label, i);
        }
        WasmValue value = frame->GetLocalValue(i);
        Handle<Object> value_obj = WasmValueToValueObject(isolate_, value);
        JSObject::SetOwnPropertyIgnoreAttributes(
            locals_obj, name.ToHandleChecked(), value_obj, NONE)
            .Assert();
      }
    }

    // Fill stack values.
    int stack_count = frame->GetStackHeight();
    // Use an object without prototype instead of an Array, for nicer displaying
    // in DevTools. For Arrays, the length field and prototype is displayed,
    // which does not make too much sense here.
    Handle<JSObject> stack_obj =
        isolate_->factory()->NewJSObjectWithNullProto();
    Handle<String> stack_name = isolate_->factory()->InternalizeOneByteString(
        STATIC_CHAR_VECTOR("stack"));
    JSObject::SetOwnPropertyIgnoreAttributes(local_scope_object, stack_name,
                                             stack_obj, NONE)
        .Assert();
    for (int i = 0; i < stack_count; ++i) {
      WasmValue value = frame->GetStackValue(i);
      Handle<Object> value_obj = WasmValueToValueObject(isolate_, value);
      JSObject::SetOwnElementIgnoreAttributes(
          stack_obj, static_cast<uint32_t>(i), value_obj, NONE)
          .Assert();
    }
    return local_scope_object;
  }

  Handle<JSArray> GetScopeDetails(Address frame_pointer, int frame_index,
                                  Handle<WasmDebugInfo> debug_info) {
    auto frame = GetInterpretedFrame(frame_pointer, frame_index);

    Handle<FixedArray> global_scope =
        isolate_->factory()->NewFixedArray(ScopeIterator::kScopeDetailsSize);
    global_scope->set(ScopeIterator::kScopeDetailsTypeIndex,
                      Smi::FromInt(ScopeIterator::ScopeTypeGlobal));
    Handle<JSObject> global_scope_object =
        GetGlobalScopeObject(frame.get(), debug_info);
    global_scope->set(ScopeIterator::kScopeDetailsObjectIndex,
                      *global_scope_object);

    Handle<FixedArray> local_scope =
        isolate_->factory()->NewFixedArray(ScopeIterator::kScopeDetailsSize);
    local_scope->set(ScopeIterator::kScopeDetailsTypeIndex,
                     Smi::FromInt(ScopeIterator::ScopeTypeLocal));
    Handle<JSObject> local_scope_object =
        GetLocalScopeObject(frame.get(), debug_info);
    local_scope->set(ScopeIterator::kScopeDetailsObjectIndex,
                     *local_scope_object);

    Handle<JSArray> global_jsarr =
        isolate_->factory()->NewJSArrayWithElements(global_scope);
    Handle<JSArray> local_jsarr =
        isolate_->factory()->NewJSArrayWithElements(local_scope);
    Handle<FixedArray> all_scopes = isolate_->factory()->NewFixedArray(2);
    all_scopes->set(0, *global_jsarr);
    all_scopes->set(1, *local_jsarr);
    return isolate_->factory()->NewJSArrayWithElements(all_scopes);
  }
};

}  // namespace

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

wasm::InterpreterHandle* GetInterpreterHandle(WasmDebugInfo* debug_info) {
  Object* handle_obj = debug_info->interpreter_handle();
  DCHECK(!handle_obj->IsUndefined());
  return Managed<wasm::InterpreterHandle>::cast(handle_obj)->raw();
}

wasm::InterpreterHandle* GetInterpreterHandleOrNull(WasmDebugInfo* debug_info) {
  Object* handle_obj = debug_info->interpreter_handle();
  if (handle_obj->IsUndefined()) return nullptr;
  return Managed<wasm::InterpreterHandle>::cast(handle_obj)->raw();
}

Handle<FixedArray> GetOrCreateInterpretedFunctions(
    Isolate* isolate, Handle<WasmDebugInfo> debug_info) {
  Handle<FixedArray> arr(debug_info->interpreted_functions(), isolate);
  int num_functions = debug_info->wasm_instance()
                          ->module_object()
                          ->native_module()
                          ->num_functions();
  if (arr->length() == 0 && num_functions > 0) {
    arr = isolate->factory()->NewFixedArray(num_functions);
    debug_info->set_interpreted_functions(*arr);
  }
  DCHECK_EQ(num_functions, arr->length());
  return arr;
}

}  // namespace

Handle<WasmDebugInfo> WasmDebugInfo::New(Handle<WasmInstanceObject> instance) {
  DCHECK(!instance->has_debug_info());
  Factory* factory = instance->GetIsolate()->factory();
  Handle<WasmDebugInfo> debug_info = Handle<WasmDebugInfo>::cast(
      factory->NewStruct(WASM_DEBUG_INFO_TYPE, TENURED));
  debug_info->set_wasm_instance(*instance);
  debug_info->set_interpreted_functions(*factory->empty_fixed_array());
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
  auto ret = interp_handle->raw()->interpreter();
  ret->SetCallIndirectTestMode();
  return ret;
}

void WasmDebugInfo::SetBreakpoint(Handle<WasmDebugInfo> debug_info,
                                  int func_index, int offset) {
  Isolate* isolate = debug_info->GetIsolate();
  auto* handle = GetOrCreateInterpreterHandle(isolate, debug_info);
  RedirectToInterpreter(debug_info, Vector<int>(&func_index, 1));
  const wasm::WasmFunction* func = &handle->module()->functions[func_index];
  handle->interpreter()->SetBreakpoint(func, offset, true);
}

void WasmDebugInfo::RedirectToInterpreter(Handle<WasmDebugInfo> debug_info,
                                          Vector<int> func_indexes) {
  Isolate* isolate = debug_info->GetIsolate();
  // Ensure that the interpreter is instantiated.
  GetOrCreateInterpreterHandle(isolate, debug_info);
  Handle<FixedArray> interpreted_functions =
      GetOrCreateInterpretedFunctions(isolate, debug_info);
  Handle<WasmInstanceObject> instance(debug_info->wasm_instance(), isolate);
  wasm::NativeModule* native_module =
      instance->module_object()->native_module();
  const wasm::WasmModule* module = instance->module();

  // We may modify js wrappers, as well as wasm functions. Hence the 2
  // modification scopes.
  CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
  wasm::NativeModuleModificationScope native_module_modification_scope(
      native_module);

  for (int func_index : func_indexes) {
    DCHECK_LE(0, func_index);
    DCHECK_GT(module->functions.size(), func_index);
    if (!interpreted_functions->get(func_index)->IsUndefined(isolate)) continue;

    MaybeHandle<Code> new_code = compiler::CompileWasmInterpreterEntry(
        isolate, func_index, module->functions[func_index].sig);
    const wasm::WasmCode* wasm_new_code = native_module->AddInterpreterEntry(
        new_code.ToHandleChecked(), func_index);
    Handle<Foreign> foreign_holder = isolate->factory()->NewForeign(
        wasm_new_code->instruction_start(), TENURED);
    interpreted_functions->set(func_index, *foreign_holder);
  }
}

void WasmDebugInfo::PrepareStep(StepAction step_action) {
  GetInterpreterHandle(this)->PrepareStep(step_action);
}

// static
bool WasmDebugInfo::RunInterpreter(Isolate* isolate,
                                   Handle<WasmDebugInfo> debug_info,
                                   Address frame_pointer, int func_index,
                                   Address arg_buffer) {
  DCHECK_LE(0, func_index);
  auto* handle = GetOrCreateInterpreterHandle(isolate, debug_info);
  Handle<WasmInstanceObject> instance(debug_info->wasm_instance(), isolate);
  return handle->Execute(instance, frame_pointer,
                         static_cast<uint32_t>(func_index), arg_buffer);
}

std::vector<std::pair<uint32_t, int>> WasmDebugInfo::GetInterpretedStack(
    Address frame_pointer) {
  return GetInterpreterHandle(this)->GetInterpretedStack(frame_pointer);
}

wasm::WasmInterpreter::FramePtr WasmDebugInfo::GetInterpretedFrame(
    Address frame_pointer, int idx) {
  return GetInterpreterHandle(this)->GetInterpretedFrame(frame_pointer, idx);
}

void WasmDebugInfo::Unwind(Address frame_pointer) {
  return GetInterpreterHandle(this)->Unwind(frame_pointer);
}

uint64_t WasmDebugInfo::NumInterpretedCalls() {
  auto* handle = GetInterpreterHandleOrNull(this);
  return handle ? handle->NumInterpretedCalls() : 0;
}

// static
Handle<JSObject> WasmDebugInfo::GetScopeDetails(
    Handle<WasmDebugInfo> debug_info, Address frame_pointer, int frame_index) {
  auto* interp_handle = GetInterpreterHandle(*debug_info);
  return interp_handle->GetScopeDetails(frame_pointer, frame_index, debug_info);
}

// static
Handle<JSObject> WasmDebugInfo::GetGlobalScopeObject(
    Handle<WasmDebugInfo> debug_info, Address frame_pointer, int frame_index) {
  auto* interp_handle = GetInterpreterHandle(*debug_info);
  auto frame = interp_handle->GetInterpretedFrame(frame_pointer, frame_index);
  return interp_handle->GetGlobalScopeObject(frame.get(), debug_info);
}

// static
Handle<JSObject> WasmDebugInfo::GetLocalScopeObject(
    Handle<WasmDebugInfo> debug_info, Address frame_pointer, int frame_index) {
  auto* interp_handle = GetInterpreterHandle(*debug_info);
  auto frame = interp_handle->GetInterpretedFrame(frame_pointer, frame_index);
  return interp_handle->GetLocalScopeObject(frame.get(), debug_info);
}

// static
Handle<JSFunction> WasmDebugInfo::GetCWasmEntry(
    Handle<WasmDebugInfo> debug_info, wasm::FunctionSig* sig) {
  Isolate* isolate = debug_info->GetIsolate();
  DCHECK_EQ(debug_info->has_c_wasm_entries(),
            debug_info->has_c_wasm_entry_map());
  if (!debug_info->has_c_wasm_entries()) {
    auto entries = isolate->factory()->NewFixedArray(4, TENURED);
    debug_info->set_c_wasm_entries(*entries);
    size_t map_size = 0;  // size estimate not so important here.
    auto managed_map = Managed<wasm::SignatureMap>::Allocate(isolate, map_size);
    debug_info->set_c_wasm_entry_map(*managed_map);
  }
  Handle<FixedArray> entries(debug_info->c_wasm_entries(), isolate);
  wasm::SignatureMap* map = debug_info->c_wasm_entry_map()->raw();
  int32_t index = map->Find(*sig);
  if (index == -1) {
    index = static_cast<int32_t>(map->FindOrInsert(*sig));
    if (index == entries->length()) {
      entries = isolate->factory()->CopyFixedArrayAndGrow(
          entries, entries->length(), TENURED);
      debug_info->set_c_wasm_entries(*entries);
    }
    DCHECK(entries->get(index)->IsUndefined(isolate));
    Handle<Code> new_entry_code =
        compiler::CompileCWasmEntry(isolate, sig).ToHandleChecked();
    Handle<WasmExportedFunctionData> function_data =
        Handle<WasmExportedFunctionData>::cast(isolate->factory()->NewStruct(
            WASM_EXPORTED_FUNCTION_DATA_TYPE, TENURED));
    function_data->set_wrapper_code(*new_entry_code);
    function_data->set_instance(debug_info->wasm_instance());
    function_data->set_jump_table_offset(-1);
    function_data->set_function_index(-1);
    Handle<String> name = isolate->factory()->InternalizeOneByteString(
        STATIC_CHAR_VECTOR("c-wasm-entry"));
    NewFunctionArgs args = NewFunctionArgs::ForWasm(
        name, function_data, isolate->sloppy_function_map());
    Handle<JSFunction> new_entry = isolate->factory()->NewFunction(args);
    new_entry->set_context(debug_info->wasm_instance()->native_context());
    new_entry->shared()->set_internal_formal_parameter_count(
        compiler::CWasmEntryParameters::kNumParameters);
    entries->set(index, *new_entry);
  }
  return handle(JSFunction::cast(entries->get(index)), isolate);
}

}  // namespace internal
}  // namespace v8
