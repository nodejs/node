// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution.h"

#include "src/api-inl.h"
#include "src/bootstrapper.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/debug/debug.h"
#include "src/isolate-inl.h"
#include "src/runtime-profiler.h"
#include "src/vm-state-inl.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {

void StackGuard::set_interrupt_limits(const ExecutionAccess& lock) {
  DCHECK_NOT_NULL(isolate_);
  thread_local_.set_jslimit(kInterruptLimit);
  thread_local_.set_climit(kInterruptLimit);
  isolate_->heap()->SetStackLimits();
}

void StackGuard::reset_limits(const ExecutionAccess& lock) {
  DCHECK_NOT_NULL(isolate_);
  thread_local_.set_jslimit(thread_local_.real_jslimit_);
  thread_local_.set_climit(thread_local_.real_climit_);
  isolate_->heap()->SetStackLimits();
}

namespace {

Handle<Object> NormalizeReceiver(Isolate* isolate, Handle<Object> receiver) {
  // Convert calls on global objects to be calls on the global
  // receiver instead to avoid having a 'this' pointer which refers
  // directly to a global object.
  if (receiver->IsJSGlobalObject()) {
    return handle(Handle<JSGlobalObject>::cast(receiver)->global_proxy(),
                  isolate);
  }
  return receiver;
}

struct InvokeParams {
  static InvokeParams SetUpForNew(Isolate* isolate, Handle<Object> constructor,
                                  Handle<Object> new_target, int argc,
                                  Handle<Object>* argv);

  static InvokeParams SetUpForCall(Isolate* isolate, Handle<Object> callable,
                                   Handle<Object> receiver, int argc,
                                   Handle<Object>* argv);

  static InvokeParams SetUpForTryCall(
      Isolate* isolate, Handle<Object> callable, Handle<Object> receiver,
      int argc, Handle<Object>* argv,
      Execution::MessageHandling message_handling,
      MaybeHandle<Object>* exception_out);

  static InvokeParams SetUpForRunMicrotasks(Isolate* isolate,
                                            MicrotaskQueue* microtask_queue,
                                            MaybeHandle<Object>* exception_out);

  Handle<Object> target;
  Handle<Object> receiver;
  int argc;
  Handle<Object>* argv;
  Handle<Object> new_target;

  MicrotaskQueue* microtask_queue;

  Execution::MessageHandling message_handling;
  MaybeHandle<Object>* exception_out;

  bool is_construct;
  Execution::Target execution_target;
};

// static
InvokeParams InvokeParams::SetUpForNew(Isolate* isolate,
                                       Handle<Object> constructor,
                                       Handle<Object> new_target, int argc,
                                       Handle<Object>* argv) {
  InvokeParams params;
  params.target = constructor;
  params.receiver = isolate->factory()->undefined_value();
  params.argc = argc;
  params.argv = argv;
  params.new_target = new_target;
  params.microtask_queue = nullptr;
  params.message_handling = Execution::MessageHandling::kReport;
  params.exception_out = nullptr;
  params.is_construct = true;
  params.execution_target = Execution::Target::kCallable;
  return params;
}

// static
InvokeParams InvokeParams::SetUpForCall(Isolate* isolate,
                                        Handle<Object> callable,
                                        Handle<Object> receiver, int argc,
                                        Handle<Object>* argv) {
  InvokeParams params;
  params.target = callable;
  params.receiver = NormalizeReceiver(isolate, receiver);
  params.argc = argc;
  params.argv = argv;
  params.new_target = isolate->factory()->undefined_value();
  params.microtask_queue = nullptr;
  params.message_handling = Execution::MessageHandling::kReport;
  params.exception_out = nullptr;
  params.is_construct = false;
  params.execution_target = Execution::Target::kCallable;
  return params;
}

// static
InvokeParams InvokeParams::SetUpForTryCall(
    Isolate* isolate, Handle<Object> callable, Handle<Object> receiver,
    int argc, Handle<Object>* argv, Execution::MessageHandling message_handling,
    MaybeHandle<Object>* exception_out) {
  InvokeParams params;
  params.target = callable;
  params.receiver = NormalizeReceiver(isolate, receiver);
  params.argc = argc;
  params.argv = argv;
  params.new_target = isolate->factory()->undefined_value();
  params.microtask_queue = nullptr;
  params.message_handling = message_handling;
  params.exception_out = exception_out;
  params.is_construct = false;
  params.execution_target = Execution::Target::kCallable;
  return params;
}

// static
InvokeParams InvokeParams::SetUpForRunMicrotasks(
    Isolate* isolate, MicrotaskQueue* microtask_queue,
    MaybeHandle<Object>* exception_out) {
  auto undefined = isolate->factory()->undefined_value();
  InvokeParams params;
  params.target = undefined;
  params.receiver = undefined;
  params.argc = 0;
  params.argv = nullptr;
  params.new_target = undefined;
  params.microtask_queue = microtask_queue;
  params.message_handling = Execution::MessageHandling::kReport;
  params.exception_out = exception_out;
  params.is_construct = false;
  params.execution_target = Execution::Target::kRunMicrotasks;
  return params;
}

Handle<Code> JSEntry(Isolate* isolate, Execution::Target execution_target,
                     bool is_construct) {
  if (is_construct) {
    DCHECK_EQ(Execution::Target::kCallable, execution_target);
    return BUILTIN_CODE(isolate, JSConstructEntry);
  } else if (execution_target == Execution::Target::kCallable) {
    DCHECK(!is_construct);
    return BUILTIN_CODE(isolate, JSEntry);
  } else if (execution_target == Execution::Target::kRunMicrotasks) {
    DCHECK(!is_construct);
    return BUILTIN_CODE(isolate, JSRunMicrotasksEntry);
  }
  UNREACHABLE();
}

V8_WARN_UNUSED_RESULT MaybeHandle<Object> Invoke(Isolate* isolate,
                                                 const InvokeParams& params) {
  RuntimeCallTimerScope timer(isolate, RuntimeCallCounterId::kInvoke);
  DCHECK(!params.receiver->IsJSGlobalObject());
  DCHECK_LE(params.argc, FixedArray::kMaxLength);

#ifdef USE_SIMULATOR
  // Simulators use separate stacks for C++ and JS. JS stack overflow checks
  // are performed whenever a JS function is called. However, it can be the case
  // that the C++ stack grows faster than the JS stack, resulting in an overflow
  // there. Add a check here to make that less likely.
  StackLimitCheck check(isolate);
  if (check.HasOverflowed()) {
    isolate->StackOverflow();
    if (params.message_handling == Execution::MessageHandling::kReport) {
      isolate->ReportPendingMessages();
    }
    return MaybeHandle<Object>();
  }
#endif

  // api callbacks can be called directly, unless we want to take the detour
  // through JS to set up a frame for break-at-entry.
  if (params.target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(params.target);
    if ((!params.is_construct || function->IsConstructor()) &&
        function->shared()->IsApiFunction() &&
        !function->shared()->BreakAtEntry()) {
      SaveAndSwitchContext save(isolate, function->context());
      DCHECK(function->context()->global_object()->IsJSGlobalObject());

      Handle<Object> receiver = params.is_construct
                                    ? isolate->factory()->the_hole_value()
                                    : params.receiver;
      auto value = Builtins::InvokeApiFunction(
          isolate, params.is_construct, function, receiver, params.argc,
          params.argv, Handle<HeapObject>::cast(params.new_target));
      bool has_exception = value.is_null();
      DCHECK(has_exception == isolate->has_pending_exception());
      if (has_exception) {
        if (params.message_handling == Execution::MessageHandling::kReport) {
          isolate->ReportPendingMessages();
        }
        return MaybeHandle<Object>();
      } else {
        isolate->clear_pending_message();
      }
      return value;
    }
  }

  // Entering JavaScript.
  VMState<JS> state(isolate);
  CHECK(AllowJavascriptExecution::IsAllowed(isolate));
  if (!ThrowOnJavascriptExecution::IsAllowed(isolate)) {
    isolate->ThrowIllegalOperation();
    if (params.message_handling == Execution::MessageHandling::kReport) {
      isolate->ReportPendingMessages();
    }
    return MaybeHandle<Object>();
  }
  if (!DumpOnJavascriptExecution::IsAllowed(isolate)) {
    V8::GetCurrentPlatform()->DumpWithoutCrashing();
    return isolate->factory()->undefined_value();
  }

  // Placeholder for return value.
  Object value;

  Handle<Code> code =
      JSEntry(isolate, params.execution_target, params.is_construct);
  {
    // Save and restore context around invocation and block the
    // allocation of handles without explicit handle scopes.
    SaveContext save(isolate);
    SealHandleScope shs(isolate);

    if (FLAG_clear_exceptions_on_js_entry) isolate->clear_pending_exception();

    if (params.execution_target == Execution::Target::kCallable) {
      // clang-format off
      // {new_target}, {target}, {receiver}, return value: tagged pointers
      // {argv}: pointer to array of tagged pointers
      using JSEntryFunction = GeneratedCode<Address(
          Address root_register_value, Address new_target, Address target,
          Address receiver, intptr_t argc, Address** argv)>;
      // clang-format on
      JSEntryFunction stub_entry =
          JSEntryFunction::FromAddress(isolate, code->InstructionStart());

      Address orig_func = params.new_target->ptr();
      Address func = params.target->ptr();
      Address recv = params.receiver->ptr();
      Address** argv = reinterpret_cast<Address**>(params.argv);
      RuntimeCallTimerScope timer(isolate, RuntimeCallCounterId::kJS_Execution);
      value = Object(stub_entry.Call(isolate->isolate_data()->isolate_root(),
                                     orig_func, func, recv, params.argc, argv));
    } else {
      DCHECK_EQ(Execution::Target::kRunMicrotasks, params.execution_target);

      // clang-format off
      // return value: tagged pointers
      // {microtask_queue}: pointer to a C++ object
      using JSEntryFunction = GeneratedCode<Address(
          Address root_register_value, MicrotaskQueue* microtask_queue)>;
      // clang-format on
      JSEntryFunction stub_entry =
          JSEntryFunction::FromAddress(isolate, code->InstructionStart());

      RuntimeCallTimerScope timer(isolate, RuntimeCallCounterId::kJS_Execution);
      value = Object(stub_entry.Call(isolate->isolate_data()->isolate_root(),
                                     params.microtask_queue));
    }
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    value->ObjectVerify(isolate);
  }
#endif

  // Update the pending exception flag and return the value.
  bool has_exception = value->IsException(isolate);
  DCHECK(has_exception == isolate->has_pending_exception());
  if (has_exception) {
    if (params.message_handling == Execution::MessageHandling::kReport) {
      isolate->ReportPendingMessages();
    }
    return MaybeHandle<Object>();
  } else {
    isolate->clear_pending_message();
  }

  return Handle<Object>(value, isolate);
}

MaybeHandle<Object> InvokeWithTryCatch(Isolate* isolate,
                                       const InvokeParams& params) {
  bool is_termination = false;
  MaybeHandle<Object> maybe_result;
  if (params.exception_out != nullptr) {
    *params.exception_out = MaybeHandle<Object>();
  }
  DCHECK_IMPLIES(
      params.message_handling == Execution::MessageHandling::kKeepPending,
      params.exception_out == nullptr);
  // Enter a try-block while executing the JavaScript code. To avoid
  // duplicate error printing it must be non-verbose.  Also, to avoid
  // creating message objects during stack overflow we shouldn't
  // capture messages.
  {
    v8::TryCatch catcher(reinterpret_cast<v8::Isolate*>(isolate));
    catcher.SetVerbose(false);
    catcher.SetCaptureMessage(false);

    maybe_result = Invoke(isolate, params);

    if (maybe_result.is_null()) {
      DCHECK(isolate->has_pending_exception());
      if (isolate->pending_exception() ==
          ReadOnlyRoots(isolate).termination_exception()) {
        is_termination = true;
      } else {
        if (params.exception_out != nullptr) {
          DCHECK(catcher.HasCaught());
          DCHECK(isolate->external_caught_exception());
          *params.exception_out = v8::Utils::OpenHandle(*catcher.Exception());
        }
      }
      if (params.message_handling == Execution::MessageHandling::kReport) {
        isolate->OptionalRescheduleException(true);
      }
    }
  }

  // Re-request terminate execution interrupt to trigger later.
  if (is_termination) isolate->stack_guard()->RequestTerminateExecution();

  return maybe_result;
}

}  // namespace

// static
MaybeHandle<Object> Execution::Call(Isolate* isolate, Handle<Object> callable,
                                    Handle<Object> receiver, int argc,
                                    Handle<Object> argv[]) {
  return Invoke(isolate, InvokeParams::SetUpForCall(isolate, callable, receiver,
                                                    argc, argv));
}

// static
MaybeHandle<Object> Execution::New(Isolate* isolate, Handle<Object> constructor,
                                   int argc, Handle<Object> argv[]) {
  return New(isolate, constructor, constructor, argc, argv);
}

// static
MaybeHandle<Object> Execution::New(Isolate* isolate, Handle<Object> constructor,
                                   Handle<Object> new_target, int argc,
                                   Handle<Object> argv[]) {
  return Invoke(isolate, InvokeParams::SetUpForNew(isolate, constructor,
                                                   new_target, argc, argv));
}

// static
MaybeHandle<Object> Execution::TryCall(Isolate* isolate,
                                       Handle<Object> callable,
                                       Handle<Object> receiver, int argc,
                                       Handle<Object> argv[],
                                       MessageHandling message_handling,
                                       MaybeHandle<Object>* exception_out) {
  return InvokeWithTryCatch(
      isolate,
      InvokeParams::SetUpForTryCall(isolate, callable, receiver, argc, argv,
                                    message_handling, exception_out));
}

// static
MaybeHandle<Object> Execution::TryRunMicrotasks(
    Isolate* isolate, MicrotaskQueue* microtask_queue,
    MaybeHandle<Object>* exception_out) {
  return InvokeWithTryCatch(
      isolate, InvokeParams::SetUpForRunMicrotasks(isolate, microtask_queue,
                                                   exception_out));
}

void StackGuard::SetStackLimit(uintptr_t limit) {
  ExecutionAccess access(isolate_);
  // If the current limits are special (e.g. due to a pending interrupt) then
  // leave them alone.
  uintptr_t jslimit = SimulatorStack::JsLimitFromCLimit(isolate_, limit);
  if (thread_local_.jslimit() == thread_local_.real_jslimit_) {
    thread_local_.set_jslimit(jslimit);
  }
  if (thread_local_.climit() == thread_local_.real_climit_) {
    thread_local_.set_climit(limit);
  }
  thread_local_.real_climit_ = limit;
  thread_local_.real_jslimit_ = jslimit;
}


void StackGuard::AdjustStackLimitForSimulator() {
  ExecutionAccess access(isolate_);
  uintptr_t climit = thread_local_.real_climit_;
  // If the current limits are special (e.g. due to a pending interrupt) then
  // leave them alone.
  uintptr_t jslimit = SimulatorStack::JsLimitFromCLimit(isolate_, climit);
  if (thread_local_.jslimit() == thread_local_.real_jslimit_) {
    thread_local_.set_jslimit(jslimit);
    isolate_->heap()->SetStackLimits();
  }
}


void StackGuard::EnableInterrupts() {
  ExecutionAccess access(isolate_);
  if (has_pending_interrupts(access)) {
    set_interrupt_limits(access);
  }
}


void StackGuard::DisableInterrupts() {
  ExecutionAccess access(isolate_);
  reset_limits(access);
}

void StackGuard::PushInterruptsScope(InterruptsScope* scope) {
  ExecutionAccess access(isolate_);
  DCHECK_NE(scope->mode_, InterruptsScope::kNoop);
  if (scope->mode_ == InterruptsScope::kPostponeInterrupts) {
    // Intercept already requested interrupts.
    int intercepted = thread_local_.interrupt_flags_ & scope->intercept_mask_;
    scope->intercepted_flags_ = intercepted;
    thread_local_.interrupt_flags_ &= ~intercepted;
  } else {
    DCHECK_EQ(scope->mode_, InterruptsScope::kRunInterrupts);
    // Restore postponed interrupts.
    int restored_flags = 0;
    for (InterruptsScope* current = thread_local_.interrupt_scopes_;
         current != nullptr; current = current->prev_) {
      restored_flags |= (current->intercepted_flags_ & scope->intercept_mask_);
      current->intercepted_flags_ &= ~scope->intercept_mask_;
    }
    thread_local_.interrupt_flags_ |= restored_flags;
  }
  if (!has_pending_interrupts(access)) reset_limits(access);
  // Add scope to the chain.
  scope->prev_ = thread_local_.interrupt_scopes_;
  thread_local_.interrupt_scopes_ = scope;
}

void StackGuard::PopInterruptsScope() {
  ExecutionAccess access(isolate_);
  InterruptsScope* top = thread_local_.interrupt_scopes_;
  DCHECK_NE(top->mode_, InterruptsScope::kNoop);
  if (top->mode_ == InterruptsScope::kPostponeInterrupts) {
    // Make intercepted interrupts active.
    DCHECK_EQ(thread_local_.interrupt_flags_ & top->intercept_mask_, 0);
    thread_local_.interrupt_flags_ |= top->intercepted_flags_;
  } else {
    DCHECK_EQ(top->mode_, InterruptsScope::kRunInterrupts);
    // Postpone existing interupts if needed.
    if (top->prev_) {
      for (int interrupt = 1; interrupt < ALL_INTERRUPTS;
           interrupt = interrupt << 1) {
        InterruptFlag flag = static_cast<InterruptFlag>(interrupt);
        if ((thread_local_.interrupt_flags_ & flag) &&
            top->prev_->Intercept(flag)) {
          thread_local_.interrupt_flags_ &= ~flag;
        }
      }
    }
  }
  if (has_pending_interrupts(access)) set_interrupt_limits(access);
  // Remove scope from chain.
  thread_local_.interrupt_scopes_ = top->prev_;
}


bool StackGuard::CheckInterrupt(InterruptFlag flag) {
  ExecutionAccess access(isolate_);
  return thread_local_.interrupt_flags_ & flag;
}


void StackGuard::RequestInterrupt(InterruptFlag flag) {
  ExecutionAccess access(isolate_);
  // Check the chain of InterruptsScope for interception.
  if (thread_local_.interrupt_scopes_ &&
      thread_local_.interrupt_scopes_->Intercept(flag)) {
    return;
  }

  // Not intercepted.  Set as active interrupt flag.
  thread_local_.interrupt_flags_ |= flag;
  set_interrupt_limits(access);

  // If this isolate is waiting in a futex, notify it to wake up.
  isolate_->futex_wait_list_node()->NotifyWake();
}


void StackGuard::ClearInterrupt(InterruptFlag flag) {
  ExecutionAccess access(isolate_);
  // Clear the interrupt flag from the chain of InterruptsScope.
  for (InterruptsScope* current = thread_local_.interrupt_scopes_;
       current != nullptr; current = current->prev_) {
    current->intercepted_flags_ &= ~flag;
  }

  // Clear the interrupt flag from the active interrupt flags.
  thread_local_.interrupt_flags_ &= ~flag;
  if (!has_pending_interrupts(access)) reset_limits(access);
}


bool StackGuard::CheckAndClearInterrupt(InterruptFlag flag) {
  ExecutionAccess access(isolate_);
  bool result = (thread_local_.interrupt_flags_ & flag);
  thread_local_.interrupt_flags_ &= ~flag;
  if (!has_pending_interrupts(access)) reset_limits(access);
  return result;
}


char* StackGuard::ArchiveStackGuard(char* to) {
  ExecutionAccess access(isolate_);
  MemCopy(to, reinterpret_cast<char*>(&thread_local_), sizeof(ThreadLocal));
  ThreadLocal blank;

  // Set the stack limits using the old thread_local_.
  // TODO(isolates): This was the old semantics of constructing a ThreadLocal
  //                 (as the ctor called SetStackLimits, which looked at the
  //                 current thread_local_ from StackGuard)-- but is this
  //                 really what was intended?
  isolate_->heap()->SetStackLimits();
  thread_local_ = blank;

  return to + sizeof(ThreadLocal);
}


char* StackGuard::RestoreStackGuard(char* from) {
  ExecutionAccess access(isolate_);
  MemCopy(reinterpret_cast<char*>(&thread_local_), from, sizeof(ThreadLocal));
  isolate_->heap()->SetStackLimits();
  return from + sizeof(ThreadLocal);
}


void StackGuard::FreeThreadResources() {
  Isolate::PerIsolateThreadData* per_thread =
      isolate_->FindOrAllocatePerThreadDataForThisThread();
  per_thread->set_stack_limit(thread_local_.real_climit_);
}


void StackGuard::ThreadLocal::Clear() {
  real_jslimit_ = kIllegalLimit;
  set_jslimit(kIllegalLimit);
  real_climit_ = kIllegalLimit;
  set_climit(kIllegalLimit);
  interrupt_scopes_ = nullptr;
  interrupt_flags_ = 0;
}


bool StackGuard::ThreadLocal::Initialize(Isolate* isolate) {
  bool should_set_stack_limits = false;
  if (real_climit_ == kIllegalLimit) {
    const uintptr_t kLimitSize = FLAG_stack_size * KB;
    DCHECK_GT(GetCurrentStackPosition(), kLimitSize);
    uintptr_t limit = GetCurrentStackPosition() - kLimitSize;
    real_jslimit_ = SimulatorStack::JsLimitFromCLimit(isolate, limit);
    set_jslimit(SimulatorStack::JsLimitFromCLimit(isolate, limit));
    real_climit_ = limit;
    set_climit(limit);
    should_set_stack_limits = true;
  }
  interrupt_scopes_ = nullptr;
  interrupt_flags_ = 0;
  return should_set_stack_limits;
}


void StackGuard::ClearThread(const ExecutionAccess& lock) {
  thread_local_.Clear();
  isolate_->heap()->SetStackLimits();
}


void StackGuard::InitThread(const ExecutionAccess& lock) {
  if (thread_local_.Initialize(isolate_)) isolate_->heap()->SetStackLimits();
  Isolate::PerIsolateThreadData* per_thread =
      isolate_->FindOrAllocatePerThreadDataForThisThread();
  uintptr_t stored_limit = per_thread->stack_limit();
  // You should hold the ExecutionAccess lock when you call this.
  if (stored_limit != 0) {
    SetStackLimit(stored_limit);
  }
}


// --- C a l l s   t o   n a t i v e s ---

Object StackGuard::HandleInterrupts() {
  TRACE_EVENT0("v8.execute", "V8.HandleInterrupts");

  if (FLAG_verify_predictable) {
    // Advance synthetic time by making a time request.
    isolate_->heap()->MonotonicallyIncreasingTimeInMs();
  }

  if (CheckAndClearInterrupt(GC_REQUEST)) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"), "V8.GCHandleGCRequest");
    isolate_->heap()->HandleGCRequest();
  }

  if (CheckAndClearInterrupt(GROW_SHARED_MEMORY)) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"),
                 "V8.WasmGrowSharedMemory");
    isolate_->wasm_engine()->memory_tracker()->UpdateSharedMemoryInstances(
        isolate_);
  }

  if (CheckAndClearInterrupt(TERMINATE_EXECUTION)) {
    TRACE_EVENT0("v8.execute", "V8.TerminateExecution");
    return isolate_->TerminateExecution();
  }

  if (CheckAndClearInterrupt(DEOPT_MARKED_ALLOCATION_SITES)) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                 "V8.GCDeoptMarkedAllocationSites");
    isolate_->heap()->DeoptMarkedAllocationSites();
  }

  if (CheckAndClearInterrupt(INSTALL_CODE)) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.InstallOptimizedFunctions");
    DCHECK(isolate_->concurrent_recompilation_enabled());
    isolate_->optimizing_compile_dispatcher()->InstallOptimizedFunctions();
  }

  if (CheckAndClearInterrupt(API_INTERRUPT)) {
    TRACE_EVENT0("v8.execute", "V8.InvokeApiInterruptCallbacks");
    // Callbacks must be invoked outside of ExecutionAccess lock.
    isolate_->InvokeApiInterruptCallbacks();
  }

  if (CheckAndClearInterrupt(LOG_WASM_CODE)) {
    TRACE_EVENT0("v8.wasm", "LogCode");
    isolate_->wasm_engine()->LogOutstandingCodesForIsolate(isolate_);
  }

  isolate_->counters()->stack_interrupts()->Increment();
  isolate_->counters()->runtime_profiler_ticks()->Increment();
  isolate_->runtime_profiler()->MarkCandidatesForOptimization();

  return ReadOnlyRoots(isolate_).undefined_value();
}

}  // namespace internal
}  // namespace v8
