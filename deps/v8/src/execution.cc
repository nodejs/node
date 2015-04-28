// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution.h"

#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/deoptimizer.h"
#include "src/isolate-inl.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

StackGuard::StackGuard()
    : isolate_(NULL) {
}


void StackGuard::set_interrupt_limits(const ExecutionAccess& lock) {
  DCHECK(isolate_ != NULL);
  thread_local_.jslimit_ = kInterruptLimit;
  thread_local_.climit_ = kInterruptLimit;
  isolate_->heap()->SetStackLimits();
}


void StackGuard::reset_limits(const ExecutionAccess& lock) {
  DCHECK(isolate_ != NULL);
  thread_local_.jslimit_ = thread_local_.real_jslimit_;
  thread_local_.climit_ = thread_local_.real_climit_;
  isolate_->heap()->SetStackLimits();
}


static void PrintDeserializedCodeInfo(Handle<JSFunction> function) {
  if (function->code() == function->shared()->code() &&
      function->shared()->deserialized()) {
    PrintF("[Running deserialized script");
    Object* script = function->shared()->script();
    if (script->IsScript()) {
      Object* name = Script::cast(script)->name();
      if (name->IsString()) {
        PrintF(": %s", String::cast(name)->ToCString().get());
      }
    }
    PrintF("]\n");
  }
}


MUST_USE_RESULT static MaybeHandle<Object> Invoke(
    bool is_construct,
    Handle<JSFunction> function,
    Handle<Object> receiver,
    int argc,
    Handle<Object> args[]) {
  Isolate* isolate = function->GetIsolate();

  // api callbacks can be called directly.
  if (!is_construct && function->shared()->IsApiFunction()) {
    SaveContext save(isolate);
    isolate->set_context(function->context());
    if (receiver->IsGlobalObject()) {
      receiver = handle(Handle<GlobalObject>::cast(receiver)->global_proxy());
    }
    DCHECK(function->context()->global_object()->IsGlobalObject());
    auto value = Builtins::InvokeApiFunction(function, receiver, argc, args);
    bool has_exception = value.is_null();
    DCHECK(has_exception == isolate->has_pending_exception());
    if (has_exception) {
      isolate->ReportPendingMessages();
      return MaybeHandle<Object>();
    } else {
      isolate->clear_pending_message();
    }
    return value;
  }

  // Entering JavaScript.
  VMState<JS> state(isolate);
  CHECK(AllowJavascriptExecution::IsAllowed(isolate));
  if (!ThrowOnJavascriptExecution::IsAllowed(isolate)) {
    isolate->ThrowIllegalOperation();
    isolate->ReportPendingMessages();
    return MaybeHandle<Object>();
  }

  // Placeholder for return value.
  Object* value = NULL;

  typedef Object* (*JSEntryFunction)(byte* entry,
                                     Object* function,
                                     Object* receiver,
                                     int argc,
                                     Object*** args);

  Handle<Code> code = is_construct
      ? isolate->factory()->js_construct_entry_code()
      : isolate->factory()->js_entry_code();

  // Convert calls on global objects to be calls on the global
  // receiver instead to avoid having a 'this' pointer which refers
  // directly to a global object.
  if (receiver->IsGlobalObject()) {
    receiver = handle(Handle<GlobalObject>::cast(receiver)->global_proxy());
  }

  // Make sure that the global object of the context we're about to
  // make the current one is indeed a global object.
  DCHECK(function->context()->global_object()->IsGlobalObject());

  {
    // Save and restore context around invocation and block the
    // allocation of handles without explicit handle scopes.
    SaveContext save(isolate);
    SealHandleScope shs(isolate);
    JSEntryFunction stub_entry = FUNCTION_CAST<JSEntryFunction>(code->entry());

    // Call the function through the right JS entry stub.
    byte* function_entry = function->code()->entry();
    JSFunction* func = *function;
    Object* recv = *receiver;
    Object*** argv = reinterpret_cast<Object***>(args);
    if (FLAG_profile_deserialization) PrintDeserializedCodeInfo(function);
    value =
        CALL_GENERATED_CODE(stub_entry, function_entry, func, recv, argc, argv);
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    value->ObjectVerify();
  }
#endif

  // Update the pending exception flag and return the value.
  bool has_exception = value->IsException();
  DCHECK(has_exception == isolate->has_pending_exception());
  if (has_exception) {
    isolate->ReportPendingMessages();
    // Reset stepping state when script exits with uncaught exception.
    if (isolate->debug()->is_active()) {
      isolate->debug()->ClearStepping();
    }
    return MaybeHandle<Object>();
  } else {
    isolate->clear_pending_message();
  }

  return Handle<Object>(value, isolate);
}


MaybeHandle<Object> Execution::Call(Isolate* isolate,
                                    Handle<Object> callable,
                                    Handle<Object> receiver,
                                    int argc,
                                    Handle<Object> argv[],
                                    bool convert_receiver) {
  if (!callable->IsJSFunction()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, callable, TryGetFunctionDelegate(isolate, callable), Object);
  }
  Handle<JSFunction> func = Handle<JSFunction>::cast(callable);

  // In sloppy mode, convert receiver.
  if (convert_receiver && !receiver->IsJSReceiver() &&
      !func->shared()->native() && is_sloppy(func->shared()->language_mode())) {
    if (receiver->IsUndefined() || receiver->IsNull()) {
      receiver = handle(func->global_proxy());
      DCHECK(!receiver->IsJSBuiltinsObject());
    } else {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, receiver, ToObject(isolate, receiver), Object);
    }
  }

  return Invoke(false, func, receiver, argc, argv);
}


MaybeHandle<Object> Execution::New(Handle<JSFunction> func,
                                   int argc,
                                   Handle<Object> argv[]) {
  return Invoke(true, func, handle(func->global_proxy()), argc, argv);
}


MaybeHandle<Object> Execution::TryCall(Handle<JSFunction> func,
                                       Handle<Object> receiver, int argc,
                                       Handle<Object> args[],
                                       MaybeHandle<Object>* exception_out) {
  bool is_termination = false;
  Isolate* isolate = func->GetIsolate();
  MaybeHandle<Object> maybe_result;
  if (exception_out != NULL) *exception_out = MaybeHandle<Object>();
  // Enter a try-block while executing the JavaScript code. To avoid
  // duplicate error printing it must be non-verbose.  Also, to avoid
  // creating message objects during stack overflow we shouldn't
  // capture messages.
  {
    v8::TryCatch catcher;
    catcher.SetVerbose(false);
    catcher.SetCaptureMessage(false);

    maybe_result = Invoke(false, func, receiver, argc, args);

    if (maybe_result.is_null()) {
      DCHECK(catcher.HasCaught());
      DCHECK(isolate->has_pending_exception());
      DCHECK(isolate->external_caught_exception());
      if (exception_out != NULL) {
        if (isolate->pending_exception() ==
            isolate->heap()->termination_exception()) {
          is_termination = true;
        } else {
          *exception_out = v8::Utils::OpenHandle(*catcher.Exception());
        }
      }
      isolate->OptionalRescheduleException(true);
    }

    DCHECK(!isolate->has_pending_exception());
    DCHECK(!isolate->external_caught_exception());
  }
  if (is_termination) isolate->TerminateExecution();
  return maybe_result;
}


Handle<Object> Execution::GetFunctionDelegate(Isolate* isolate,
                                              Handle<Object> object) {
  DCHECK(!object->IsJSFunction());
  Factory* factory = isolate->factory();

  // If you return a function from here, it will be called when an
  // attempt is made to call the given object as a function.

  // If object is a function proxy, get its handler. Iterate if necessary.
  Object* fun = *object;
  while (fun->IsJSFunctionProxy()) {
    fun = JSFunctionProxy::cast(fun)->call_trap();
  }
  if (fun->IsJSFunction()) return Handle<Object>(fun, isolate);

  // Objects created through the API can have an instance-call handler
  // that should be used when calling the object as a function.
  if (fun->IsHeapObject() &&
      HeapObject::cast(fun)->map()->has_instance_call_handler()) {
    return Handle<JSFunction>(
        isolate->native_context()->call_as_function_delegate());
  }

  return factory->undefined_value();
}


MaybeHandle<Object> Execution::TryGetFunctionDelegate(Isolate* isolate,
                                                      Handle<Object> object) {
  DCHECK(!object->IsJSFunction());

  // If object is a function proxy, get its handler. Iterate if necessary.
  Object* fun = *object;
  while (fun->IsJSFunctionProxy()) {
    fun = JSFunctionProxy::cast(fun)->call_trap();
  }
  if (fun->IsJSFunction()) return Handle<Object>(fun, isolate);

  // Objects created through the API can have an instance-call handler
  // that should be used when calling the object as a function.
  if (fun->IsHeapObject() &&
      HeapObject::cast(fun)->map()->has_instance_call_handler()) {
    return Handle<JSFunction>(
        isolate->native_context()->call_as_function_delegate());
  }

  // If the Object doesn't have an instance-call handler we should
  // throw a non-callable exception.
  THROW_NEW_ERROR(isolate, NewTypeError("called_non_callable",
                                        i::HandleVector<i::Object>(&object, 1)),
                  Object);
}


Handle<Object> Execution::GetConstructorDelegate(Isolate* isolate,
                                                 Handle<Object> object) {
  DCHECK(!object->IsJSFunction());

  // If you return a function from here, it will be called when an
  // attempt is made to call the given object as a constructor.

  // If object is a function proxies, get its handler. Iterate if necessary.
  Object* fun = *object;
  while (fun->IsJSFunctionProxy()) {
    fun = JSFunctionProxy::cast(fun)->call_trap();
  }
  if (fun->IsJSFunction()) return Handle<Object>(fun, isolate);

  // Objects created through the API can have an instance-call handler
  // that should be used when calling the object as a function.
  if (fun->IsHeapObject() &&
      HeapObject::cast(fun)->map()->has_instance_call_handler()) {
    return Handle<JSFunction>(
        isolate->native_context()->call_as_constructor_delegate());
  }

  return isolate->factory()->undefined_value();
}


MaybeHandle<Object> Execution::TryGetConstructorDelegate(
    Isolate* isolate, Handle<Object> object) {
  DCHECK(!object->IsJSFunction());

  // If you return a function from here, it will be called when an
  // attempt is made to call the given object as a constructor.

  // If object is a function proxies, get its handler. Iterate if necessary.
  Object* fun = *object;
  while (fun->IsJSFunctionProxy()) {
    fun = JSFunctionProxy::cast(fun)->call_trap();
  }
  if (fun->IsJSFunction()) return Handle<Object>(fun, isolate);

  // Objects created through the API can have an instance-call handler
  // that should be used when calling the object as a function.
  if (fun->IsHeapObject() &&
      HeapObject::cast(fun)->map()->has_instance_call_handler()) {
    return Handle<JSFunction>(
        isolate->native_context()->call_as_constructor_delegate());
  }

  // If the Object doesn't have an instance-call handler we should
  // throw a non-callable exception.
  THROW_NEW_ERROR(isolate, NewTypeError("called_non_callable",
                                        i::HandleVector<i::Object>(&object, 1)),
                  Object);
}


void StackGuard::EnableInterrupts() {
  ExecutionAccess access(isolate_);
  if (has_pending_interrupts(access)) {
    set_interrupt_limits(access);
  }
}


void StackGuard::SetStackLimit(uintptr_t limit) {
  ExecutionAccess access(isolate_);
  // If the current limits are special (e.g. due to a pending interrupt) then
  // leave them alone.
  uintptr_t jslimit = SimulatorStack::JsLimitFromCLimit(isolate_, limit);
  if (thread_local_.jslimit_ == thread_local_.real_jslimit_) {
    thread_local_.jslimit_ = jslimit;
  }
  if (thread_local_.climit_ == thread_local_.real_climit_) {
    thread_local_.climit_ = limit;
  }
  thread_local_.real_climit_ = limit;
  thread_local_.real_jslimit_ = jslimit;
}


void StackGuard::DisableInterrupts() {
  ExecutionAccess access(isolate_);
  reset_limits(access);
}


void StackGuard::PushPostponeInterruptsScope(PostponeInterruptsScope* scope) {
  ExecutionAccess access(isolate_);
  // Intercept already requested interrupts.
  int intercepted = thread_local_.interrupt_flags_ & scope->intercept_mask_;
  scope->intercepted_flags_ = intercepted;
  thread_local_.interrupt_flags_ &= ~intercepted;
  if (!has_pending_interrupts(access)) reset_limits(access);
  // Add scope to the chain.
  scope->prev_ = thread_local_.postpone_interrupts_;
  thread_local_.postpone_interrupts_ = scope;
}


void StackGuard::PopPostponeInterruptsScope() {
  ExecutionAccess access(isolate_);
  PostponeInterruptsScope* top = thread_local_.postpone_interrupts_;
  // Make intercepted interrupts active.
  DCHECK((thread_local_.interrupt_flags_ & top->intercept_mask_) == 0);
  thread_local_.interrupt_flags_ |= top->intercepted_flags_;
  if (has_pending_interrupts(access)) set_interrupt_limits(access);
  // Remove scope from chain.
  thread_local_.postpone_interrupts_ = top->prev_;
}


bool StackGuard::CheckInterrupt(InterruptFlag flag) {
  ExecutionAccess access(isolate_);
  return thread_local_.interrupt_flags_ & flag;
}


void StackGuard::RequestInterrupt(InterruptFlag flag) {
  ExecutionAccess access(isolate_);
  // Check the chain of PostponeInterruptsScopes for interception.
  if (thread_local_.postpone_interrupts_ &&
      thread_local_.postpone_interrupts_->Intercept(flag)) {
    return;
  }

  // Not intercepted.  Set as active interrupt flag.
  thread_local_.interrupt_flags_ |= flag;
  set_interrupt_limits(access);
}


void StackGuard::ClearInterrupt(InterruptFlag flag) {
  ExecutionAccess access(isolate_);
  // Clear the interrupt flag from the chain of PostponeInterruptsScopes.
  for (PostponeInterruptsScope* current = thread_local_.postpone_interrupts_;
       current != NULL;
       current = current->prev_) {
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
  jslimit_ = kIllegalLimit;
  real_climit_ = kIllegalLimit;
  climit_ = kIllegalLimit;
  postpone_interrupts_ = NULL;
  interrupt_flags_ = 0;
}


bool StackGuard::ThreadLocal::Initialize(Isolate* isolate) {
  bool should_set_stack_limits = false;
  if (real_climit_ == kIllegalLimit) {
    const uintptr_t kLimitSize = FLAG_stack_size * KB;
    DCHECK(GetCurrentStackPosition() > kLimitSize);
    uintptr_t limit = GetCurrentStackPosition() - kLimitSize;
    real_jslimit_ = SimulatorStack::JsLimitFromCLimit(isolate, limit);
    jslimit_ = SimulatorStack::JsLimitFromCLimit(isolate, limit);
    real_climit_ = limit;
    climit_ = limit;
    should_set_stack_limits = true;
  }
  postpone_interrupts_ = NULL;
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

#define RETURN_NATIVE_CALL(name, args)                                  \
  do {                                                                  \
    Handle<Object> argv[] = args;                                       \
    return Call(isolate,                                                \
                isolate->name##_fun(),                                  \
                isolate->js_builtins_object(),                          \
                arraysize(argv), argv);                                \
  } while (false)


MaybeHandle<Object> Execution::ToNumber(
    Isolate* isolate, Handle<Object> obj) {
  RETURN_NATIVE_CALL(to_number, { obj });
}


MaybeHandle<Object> Execution::ToString(
    Isolate* isolate, Handle<Object> obj) {
  RETURN_NATIVE_CALL(to_string, { obj });
}


MaybeHandle<Object> Execution::ToDetailString(
    Isolate* isolate, Handle<Object> obj) {
  RETURN_NATIVE_CALL(to_detail_string, { obj });
}


MaybeHandle<Object> Execution::ToObject(
    Isolate* isolate, Handle<Object> obj) {
  if (obj->IsSpecObject()) return obj;
  RETURN_NATIVE_CALL(to_object, { obj });
}


MaybeHandle<Object> Execution::ToInteger(
    Isolate* isolate, Handle<Object> obj) {
  RETURN_NATIVE_CALL(to_integer, { obj });
}


MaybeHandle<Object> Execution::ToUint32(
    Isolate* isolate, Handle<Object> obj) {
  RETURN_NATIVE_CALL(to_uint32, { obj });
}


MaybeHandle<Object> Execution::ToInt32(
    Isolate* isolate, Handle<Object> obj) {
  RETURN_NATIVE_CALL(to_int32, { obj });
}


MaybeHandle<Object> Execution::ToLength(
    Isolate* isolate, Handle<Object> obj) {
  RETURN_NATIVE_CALL(to_length, { obj });
}


MaybeHandle<Object> Execution::NewDate(Isolate* isolate, double time) {
  Handle<Object> time_obj = isolate->factory()->NewNumber(time);
  RETURN_NATIVE_CALL(create_date, { time_obj });
}


#undef RETURN_NATIVE_CALL


MaybeHandle<JSRegExp> Execution::NewJSRegExp(Handle<String> pattern,
                                             Handle<String> flags) {
  Isolate* isolate = pattern->GetIsolate();
  Handle<JSFunction> function = Handle<JSFunction>(
      isolate->native_context()->regexp_function());
  Handle<Object> re_obj;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, re_obj,
      RegExpImpl::CreateRegExpLiteral(function, pattern, flags),
      JSRegExp);
  return Handle<JSRegExp>::cast(re_obj);
}


Handle<Object> Execution::CharAt(Handle<String> string, uint32_t index) {
  Isolate* isolate = string->GetIsolate();
  Factory* factory = isolate->factory();

  int int_index = static_cast<int>(index);
  if (int_index < 0 || int_index >= string->length()) {
    return factory->undefined_value();
  }

  Handle<Object> char_at = Object::GetProperty(
      isolate->js_builtins_object(),
      factory->char_at_string()).ToHandleChecked();
  if (!char_at->IsJSFunction()) {
    return factory->undefined_value();
  }

  Handle<Object> index_object = factory->NewNumberFromInt(int_index);
  Handle<Object> index_arg[] = { index_object };
  Handle<Object> result;
  if (!TryCall(Handle<JSFunction>::cast(char_at),
               string,
               arraysize(index_arg),
               index_arg).ToHandle(&result)) {
    return factory->undefined_value();
  }
  return result;
}


Handle<String> Execution::GetStackTraceLine(Handle<Object> recv,
                                            Handle<JSFunction> fun,
                                            Handle<Object> pos,
                                            Handle<Object> is_global) {
  Isolate* isolate = fun->GetIsolate();
  Handle<Object> args[] = { recv, fun, pos, is_global };
  MaybeHandle<Object> maybe_result =
      TryCall(isolate->get_stack_trace_line_fun(),
              isolate->js_builtins_object(),
              arraysize(args),
              args);
  Handle<Object> result;
  if (!maybe_result.ToHandle(&result) || !result->IsString()) {
    return isolate->factory()->empty_string();
  }

  return Handle<String>::cast(result);
}


Object* StackGuard::HandleInterrupts() {
  if (CheckAndClearInterrupt(GC_REQUEST)) {
    isolate_->heap()->HandleGCRequest();
  }

  if (CheckDebugBreak() || CheckDebugCommand()) {
    isolate_->debug()->HandleDebugBreak();
  }

  if (CheckAndClearInterrupt(TERMINATE_EXECUTION)) {
    return isolate_->TerminateExecution();
  }

  if (CheckAndClearInterrupt(DEOPT_MARKED_ALLOCATION_SITES)) {
    isolate_->heap()->DeoptMarkedAllocationSites();
  }

  if (CheckAndClearInterrupt(INSTALL_CODE)) {
    DCHECK(isolate_->concurrent_recompilation_enabled());
    isolate_->optimizing_compiler_thread()->InstallOptimizedFunctions();
  }

  if (CheckAndClearInterrupt(API_INTERRUPT)) {
    // Callbacks must be invoked outside of ExecusionAccess lock.
    isolate_->InvokeApiInterruptCallbacks();
  }

  isolate_->counters()->stack_interrupts()->Increment();
  isolate_->counters()->runtime_profiler_ticks()->Increment();
  isolate_->runtime_profiler()->OptimizeNow();

  return isolate_->heap()->undefined_value();
}

} }  // namespace v8::internal
