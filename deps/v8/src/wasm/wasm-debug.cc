// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
#include "src/assert-scope.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/debug.h"
#include "src/factory.h"
#include "src/frames-inl.h"
#include "src/isolate.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/zone/accounting-allocator.h"

using namespace v8::internal;
using namespace v8::internal::wasm;

namespace {

// Forward declaration.
class InterpreterHandle;
InterpreterHandle* GetInterpreterHandle(WasmDebugInfo* debug_info);

class InterpreterHandle {
  AccountingAllocator allocator_;
  WasmInstance instance_;
  WasmInterpreter interpreter_;
  Isolate* isolate_;
  StepAction next_step_action_ = StepNone;
  int last_step_stack_depth_ = 0;

 public:
  // Initialize in the right order, using helper methods to make this possible.
  // WasmInterpreter has to be allocated in place, since it is not movable.
  InterpreterHandle(Isolate* isolate, WasmDebugInfo* debug_info)
      : instance_(debug_info->wasm_instance()->compiled_module()->module()),
        interpreter_(GetBytesEnv(&instance_, debug_info), &allocator_),
        isolate_(isolate) {
    if (debug_info->wasm_instance()->has_memory_buffer()) {
      JSArrayBuffer* mem_buffer = debug_info->wasm_instance()->memory_buffer();
      instance_.mem_start =
          reinterpret_cast<byte*>(mem_buffer->backing_store());
      CHECK(mem_buffer->byte_length()->ToUint32(&instance_.mem_size));
    } else {
      DCHECK_EQ(0, instance_.module->min_mem_pages);
      instance_.mem_start = nullptr;
      instance_.mem_size = 0;
    }
  }

  static ModuleBytesEnv GetBytesEnv(WasmInstance* instance,
                                    WasmDebugInfo* debug_info) {
    // Return raw pointer into heap. The WasmInterpreter will make its own copy
    // of this data anyway, and there is no heap allocation in-between.
    SeqOneByteString* bytes_str =
        debug_info->wasm_instance()->compiled_module()->module_bytes();
    Vector<const byte> bytes(bytes_str->GetChars(), bytes_str->length());
    return ModuleBytesEnv(instance->module, instance, bytes);
  }

  WasmInterpreter* interpreter() { return &interpreter_; }
  const WasmModule* module() { return instance_.module; }

  void PrepareStep(StepAction step_action) {
    next_step_action_ = step_action;
    last_step_stack_depth_ = CurrentStackDepth();
  }

  void ClearStepping() { next_step_action_ = StepNone; }

  int CurrentStackDepth() {
    DCHECK_EQ(1, interpreter()->GetThreadCount());
    return interpreter()->GetThread(0)->GetFrameCount();
  }

  void Execute(uint32_t func_index, uint8_t* arg_buffer) {
    DCHECK_GE(module()->functions.size(), func_index);
    FunctionSig* sig = module()->functions[func_index].sig;
    DCHECK_GE(kMaxInt, sig->parameter_count());
    int num_params = static_cast<int>(sig->parameter_count());
    ScopedVector<WasmVal> wasm_args(num_params);
    uint8_t* arg_buf_ptr = arg_buffer;
    for (int i = 0; i < num_params; ++i) {
      int param_size = 1 << ElementSizeLog2Of(sig->GetParam(i));
#define CASE_ARG_TYPE(type, ctype)                                  \
  case type:                                                        \
    DCHECK_EQ(param_size, sizeof(ctype));                           \
    wasm_args[i] = WasmVal(*reinterpret_cast<ctype*>(arg_buf_ptr)); \
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
      arg_buf_ptr += RoundUpToMultipleOfPowOf2(param_size, 8);
    }

    WasmInterpreter::Thread* thread = interpreter_.GetThread(0);
    // We do not support reentering an already running interpreter at the moment
    // (like INTERPRETER -> JS -> WASM -> INTERPRETER).
    DCHECK(thread->state() == WasmInterpreter::STOPPED ||
           thread->state() == WasmInterpreter::FINISHED);
    thread->Reset();
    thread->PushFrame(&module()->functions[func_index], wasm_args.start());
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
        case WasmInterpreter::State::TRAPPED:
          // TODO(clemensh): Generate appropriate JS exception.
          UNIMPLEMENTED();
          break;
        // STOPPED and RUNNING should never occur here.
        case WasmInterpreter::State::STOPPED:
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
      WasmVal ret_val = thread->GetReturnValue(0);
#define CASE_RET_TYPE(type, ctype)                                       \
  case type:                                                             \
    DCHECK_EQ(1 << ElementSizeLog2Of(sig->GetReturn(0)), sizeof(ctype)); \
    *reinterpret_cast<ctype*>(arg_buffer) = ret_val.to<ctype>();         \
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
        return WasmInterpreter::STOPPED;
    }
  }

  Handle<WasmInstanceObject> GetInstanceObject() {
    StackTraceFrameIterator it(isolate_);
    WasmInterpreterEntryFrame* frame =
        WasmInterpreterEntryFrame::cast(it.frame());
    Handle<WasmInstanceObject> instance_obj(frame->wasm_instance(), isolate_);
    DCHECK_EQ(this, GetInterpreterHandle(instance_obj->debug_info()));
    return instance_obj;
  }

  void NotifyDebugEventListeners(WasmInterpreter::Thread* thread) {
    // Enter the debugger.
    DebugScope debug_scope(isolate_->debug());
    if (debug_scope.failed()) return;

    // Postpone interrupt during breakpoint processing.
    PostponeInterruptsScope postpone(isolate_);

    // Check whether we hit a breakpoint.
    if (isolate_->debug()->break_points_active()) {
      Handle<WasmCompiledModule> compiled_module(
          GetInstanceObject()->compiled_module(), isolate_);
      int position = GetTopPosition(compiled_module);
      Handle<FixedArray> breakpoints;
      if (compiled_module->CheckBreakPoints(position).ToHandle(&breakpoints)) {
        // We hit one or several breakpoints. Clear stepping, notify the
        // listeners and return.
        ClearStepping();
        Handle<Object> hit_breakpoints_js =
            isolate_->factory()->NewJSArrayWithElements(breakpoints);
        isolate_->debug()->OnDebugBreak(hit_breakpoints_js);
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
    isolate_->debug()->OnDebugBreak(isolate_->factory()->undefined_value());
  }

  int GetTopPosition(Handle<WasmCompiledModule> compiled_module) {
    DCHECK_EQ(1, interpreter()->GetThreadCount());
    WasmInterpreter::Thread* thread = interpreter()->GetThread(0);
    DCHECK_LT(0, thread->GetFrameCount());

    wasm::InterpretedFrame frame =
        thread->GetFrame(thread->GetFrameCount() - 1);
    return compiled_module->GetFunctionOffset(frame.function()->func_index) +
           frame.pc();
  }

  std::vector<std::pair<uint32_t, int>> GetInterpretedStack(
      Address frame_pointer) {
    // TODO(clemensh): Use frame_pointer.
    USE(frame_pointer);

    DCHECK_EQ(1, interpreter()->GetThreadCount());
    WasmInterpreter::Thread* thread = interpreter()->GetThread(0);
    std::vector<std::pair<uint32_t, int>> stack(thread->GetFrameCount());
    for (int i = 0, e = thread->GetFrameCount(); i < e; ++i) {
      wasm::InterpretedFrame frame = thread->GetFrame(i);
      stack[i] = {frame.function()->func_index, frame.pc()};
    }
    return stack;
  }

  std::unique_ptr<wasm::InterpretedFrame> GetInterpretedFrame(
      Address frame_pointer, int idx) {
    // TODO(clemensh): Use frame_pointer.
    USE(frame_pointer);

    DCHECK_EQ(1, interpreter()->GetThreadCount());
    WasmInterpreter::Thread* thread = interpreter()->GetThread(0);
    return std::unique_ptr<wasm::InterpretedFrame>(
        new wasm::InterpretedFrame(thread->GetMutableFrame(idx)));
  }

  uint64_t NumInterpretedCalls() {
    DCHECK_EQ(1, interpreter()->GetThreadCount());
    return interpreter()->GetThread(0)->NumInterpretedCalls();
  }
};

InterpreterHandle* GetOrCreateInterpreterHandle(
    Isolate* isolate, Handle<WasmDebugInfo> debug_info) {
  Handle<Object> handle(debug_info->get(WasmDebugInfo::kInterpreterHandle),
                        isolate);
  if (handle->IsUndefined(isolate)) {
    InterpreterHandle* cpp_handle = new InterpreterHandle(isolate, *debug_info);
    handle = Managed<InterpreterHandle>::New(isolate, cpp_handle);
    debug_info->set(WasmDebugInfo::kInterpreterHandle, *handle);
  }

  return Handle<Managed<InterpreterHandle>>::cast(handle)->get();
}

InterpreterHandle* GetInterpreterHandle(WasmDebugInfo* debug_info) {
  Object* handle_obj = debug_info->get(WasmDebugInfo::kInterpreterHandle);
  DCHECK(!handle_obj->IsUndefined(debug_info->GetIsolate()));
  return Managed<InterpreterHandle>::cast(handle_obj)->get();
}

InterpreterHandle* GetInterpreterHandleOrNull(WasmDebugInfo* debug_info) {
  Object* handle_obj = debug_info->get(WasmDebugInfo::kInterpreterHandle);
  if (handle_obj->IsUndefined(debug_info->GetIsolate())) return nullptr;
  return Managed<InterpreterHandle>::cast(handle_obj)->get();
}

int GetNumFunctions(WasmInstanceObject* instance) {
  size_t num_functions =
      instance->compiled_module()->module()->functions.size();
  DCHECK_GE(kMaxInt, num_functions);
  return static_cast<int>(num_functions);
}

Handle<FixedArray> GetOrCreateInterpretedFunctions(
    Isolate* isolate, Handle<WasmDebugInfo> debug_info) {
  Handle<Object> obj(debug_info->get(WasmDebugInfo::kInterpretedFunctions),
                     isolate);
  if (!obj->IsUndefined(isolate)) return Handle<FixedArray>::cast(obj);

  Handle<FixedArray> new_arr = isolate->factory()->NewFixedArray(
      GetNumFunctions(debug_info->wasm_instance()));
  debug_info->set(WasmDebugInfo::kInterpretedFunctions, *new_arr);
  return new_arr;
}

void RedirectCallsitesInCode(Code* code, Code* old_target, Code* new_target) {
  DisallowHeapAllocation no_gc;
  for (RelocIterator it(code, RelocInfo::kCodeTargetMask); !it.done();
       it.next()) {
    DCHECK(RelocInfo::IsCodeTarget(it.rinfo()->rmode()));
    Code* target = Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
    if (target != old_target) continue;
    it.rinfo()->set_target_address(new_target->instruction_start());
  }
}

void RedirectCallsitesInInstance(Isolate* isolate, WasmInstanceObject* instance,
                                 Code* old_target, Code* new_target) {
  DisallowHeapAllocation no_gc;
  // Redirect all calls in wasm functions.
  FixedArray* code_table = instance->compiled_module()->ptr_to_code_table();
  for (int i = 0, e = GetNumFunctions(instance); i < e; ++i) {
    RedirectCallsitesInCode(Code::cast(code_table->get(i)), old_target,
                            new_target);
  }

  // Redirect all calls in exported functions.
  FixedArray* weak_exported_functions =
      instance->compiled_module()->ptr_to_weak_exported_functions();
  for (int i = 0, e = weak_exported_functions->length(); i != e; ++i) {
    WeakCell* weak_function = WeakCell::cast(weak_exported_functions->get(i));
    if (weak_function->cleared()) continue;
    Code* code = JSFunction::cast(weak_function->value())->code();
    RedirectCallsitesInCode(code, old_target, new_target);
  }
}

}  // namespace

Handle<WasmDebugInfo> WasmDebugInfo::New(Handle<WasmInstanceObject> instance) {
  Isolate* isolate = instance->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<FixedArray> arr = factory->NewFixedArray(kFieldCount, TENURED);
  arr->set(kWrapperTracerHeader, Smi::kZero);
  arr->set(kInstance, *instance);
  return Handle<WasmDebugInfo>::cast(arr);
}

bool WasmDebugInfo::IsDebugInfo(Object* object) {
  if (!object->IsFixedArray()) return false;
  FixedArray* arr = FixedArray::cast(object);
  if (arr->length() != kFieldCount) return false;
  if (!IsWasmInstance(arr->get(kInstance))) return false;
  Isolate* isolate = arr->GetIsolate();
  if (!arr->get(kInterpreterHandle)->IsUndefined(isolate) &&
      !arr->get(kInterpreterHandle)->IsForeign())
    return false;
  return true;
}

WasmDebugInfo* WasmDebugInfo::cast(Object* object) {
  DCHECK(IsDebugInfo(object));
  return reinterpret_cast<WasmDebugInfo*>(object);
}

WasmInstanceObject* WasmDebugInfo::wasm_instance() {
  return WasmInstanceObject::cast(get(kInstance));
}

void WasmDebugInfo::SetBreakpoint(Handle<WasmDebugInfo> debug_info,
                                  int func_index, int offset) {
  Isolate* isolate = debug_info->GetIsolate();
  InterpreterHandle* handle = GetOrCreateInterpreterHandle(isolate, debug_info);
  RedirectToInterpreter(debug_info, func_index);
  const WasmFunction* func = &handle->module()->functions[func_index];
  handle->interpreter()->SetBreakpoint(func, offset, true);
}

void WasmDebugInfo::RedirectToInterpreter(Handle<WasmDebugInfo> debug_info,
                                          int func_index) {
  Isolate* isolate = debug_info->GetIsolate();
  DCHECK_LE(0, func_index);
  DCHECK_GT(debug_info->wasm_instance()->module()->functions.size(),
            func_index);
  Handle<FixedArray> interpreted_functions =
      GetOrCreateInterpretedFunctions(isolate, debug_info);
  if (!interpreted_functions->get(func_index)->IsUndefined(isolate)) return;

  // Ensure that the interpreter is instantiated.
  GetOrCreateInterpreterHandle(isolate, debug_info);
  Handle<WasmInstanceObject> instance(debug_info->wasm_instance(), isolate);
  Handle<Code> new_code = compiler::CompileWasmInterpreterEntry(
      isolate, func_index,
      instance->compiled_module()->module()->functions[func_index].sig,
      instance);

  Handle<FixedArray> code_table = instance->compiled_module()->code_table();
  Handle<Code> old_code(Code::cast(code_table->get(func_index)), isolate);
  interpreted_functions->set(func_index, *new_code);

  RedirectCallsitesInInstance(isolate, *instance, *old_code, *new_code);
}

void WasmDebugInfo::PrepareStep(StepAction step_action) {
  GetInterpreterHandle(this)->PrepareStep(step_action);
}

void WasmDebugInfo::RunInterpreter(int func_index, uint8_t* arg_buffer) {
  DCHECK_LE(0, func_index);
  GetInterpreterHandle(this)->Execute(static_cast<uint32_t>(func_index),
                                      arg_buffer);
}

std::vector<std::pair<uint32_t, int>> WasmDebugInfo::GetInterpretedStack(
    Address frame_pointer) {
  return GetInterpreterHandle(this)->GetInterpretedStack(frame_pointer);
}

std::unique_ptr<wasm::InterpretedFrame> WasmDebugInfo::GetInterpretedFrame(
    Address frame_pointer, int idx) {
  return GetInterpreterHandle(this)->GetInterpretedFrame(frame_pointer, idx);
}

uint64_t WasmDebugInfo::NumInterpretedCalls() {
  auto handle = GetInterpreterHandleOrNull(this);
  return handle ? handle->NumInterpretedCalls() : 0;
}
