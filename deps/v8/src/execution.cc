// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "execution.h"

#include "bootstrapper.h"
#include "codegen.h"
#include "deoptimizer.h"
#include "isolate-inl.h"
#include "vm-state-inl.h"

namespace v8 {
namespace internal {

StackGuard::StackGuard()
    : isolate_(NULL) {
}


void StackGuard::set_interrupt_limits(const ExecutionAccess& lock) {
  ASSERT(isolate_ != NULL);
  // Ignore attempts to interrupt when interrupts are postponed.
  if (should_postpone_interrupts(lock)) return;
  thread_local_.jslimit_ = kInterruptLimit;
  thread_local_.climit_ = kInterruptLimit;
  isolate_->heap()->SetStackLimits();
}


void StackGuard::reset_limits(const ExecutionAccess& lock) {
  ASSERT(isolate_ != NULL);
  thread_local_.jslimit_ = thread_local_.real_jslimit_;
  thread_local_.climit_ = thread_local_.real_climit_;
  isolate_->heap()->SetStackLimits();
}


MUST_USE_RESULT static MaybeHandle<Object> Invoke(
    bool is_construct,
    Handle<JSFunction> function,
    Handle<Object> receiver,
    int argc,
    Handle<Object> args[]) {
  Isolate* isolate = function->GetIsolate();

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
    Handle<GlobalObject> global = Handle<GlobalObject>::cast(receiver);
    receiver = Handle<JSObject>(global->global_receiver());
  }

  // Make sure that the global object of the context we're about to
  // make the current one is indeed a global object.
  ASSERT(function->context()->global_object()->IsGlobalObject());

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
    value =
        CALL_GENERATED_CODE(stub_entry, function_entry, func, recv, argc, argv);
  }

#ifdef VERIFY_HEAP
  value->ObjectVerify();
#endif

  // Update the pending exception flag and return the value.
  bool has_exception = value->IsException();
  ASSERT(has_exception == isolate->has_pending_exception());
  if (has_exception) {
    isolate->ReportPendingMessages();
    // Reset stepping state when script exits with uncaught exception.
    if (isolate->debugger()->IsDebuggerActive()) {
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
      !func->shared()->native() &&
      func->shared()->strict_mode() == SLOPPY) {
    if (receiver->IsUndefined() || receiver->IsNull()) {
      Object* global = func->context()->global_object()->global_receiver();
      // Under some circumstances, 'global' can be the JSBuiltinsObject
      // In that case, don't rewrite.  (FWIW, the same holds for
      // GetIsolate()->global_object()->global_receiver().)
      if (!global->IsJSBuiltinsObject()) {
        receiver = Handle<Object>(global, func->GetIsolate());
      }
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
  return Invoke(true, func, func->GetIsolate()->global_object(), argc, argv);
}


MaybeHandle<Object> Execution::TryCall(Handle<JSFunction> func,
                                       Handle<Object> receiver,
                                       int argc,
                                       Handle<Object> args[],
                                       Handle<Object>* exception_out) {
  // Enter a try-block while executing the JavaScript code. To avoid
  // duplicate error printing it must be non-verbose.  Also, to avoid
  // creating message objects during stack overflow we shouldn't
  // capture messages.
  v8::TryCatch catcher;
  catcher.SetVerbose(false);
  catcher.SetCaptureMessage(false);

  // Get isolate now, because handle might be persistent
  // and get destroyed in the next call.
  Isolate* isolate = func->GetIsolate();
  MaybeHandle<Object> maybe_result = Invoke(false, func, receiver, argc, args);

  if (maybe_result.is_null()) {
    ASSERT(catcher.HasCaught());
    ASSERT(isolate->has_pending_exception());
    ASSERT(isolate->external_caught_exception());
    if (exception_out != NULL) {
      if (isolate->pending_exception() ==
          isolate->heap()->termination_exception()) {
        *exception_out = isolate->factory()->termination_exception();
      } else {
        *exception_out = v8::Utils::OpenHandle(*catcher.Exception());
      }
    }
    isolate->OptionalRescheduleException(true);
  }

  ASSERT(!isolate->has_pending_exception());
  ASSERT(!isolate->external_caught_exception());
  return maybe_result;
}


Handle<Object> Execution::GetFunctionDelegate(Isolate* isolate,
                                              Handle<Object> object) {
  ASSERT(!object->IsJSFunction());
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
  ASSERT(!object->IsJSFunction());

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
  i::Handle<i::Object> error_obj = isolate->factory()->NewTypeError(
      "called_non_callable", i::HandleVector<i::Object>(&object, 1));

  return isolate->Throw<Object>(error_obj);
}


Handle<Object> Execution::GetConstructorDelegate(Isolate* isolate,
                                                 Handle<Object> object) {
  ASSERT(!object->IsJSFunction());

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
  ASSERT(!object->IsJSFunction());

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
  i::Handle<i::Object> error_obj = isolate->factory()->NewTypeError(
      "called_non_callable", i::HandleVector<i::Object>(&object, 1));
  return isolate->Throw<Object>(error_obj);
}


void Execution::RunMicrotasks(Isolate* isolate) {
  ASSERT(isolate->microtask_pending());
  Execution::Call(
      isolate,
      isolate->run_microtasks(),
      isolate->factory()->undefined_value(),
      0,
      NULL).Check();
}


void Execution::EnqueueMicrotask(Isolate* isolate, Handle<Object> microtask) {
  Handle<Object> args[] = { microtask };
  Execution::Call(
      isolate,
      isolate->enqueue_microtask(),
      isolate->factory()->undefined_value(),
      1,
      args).Check();
}


bool StackGuard::IsStackOverflow() {
  ExecutionAccess access(isolate_);
  return (thread_local_.jslimit_ != kInterruptLimit &&
          thread_local_.climit_ != kInterruptLimit);
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


bool StackGuard::ShouldPostponeInterrupts() {
  ExecutionAccess access(isolate_);
  return should_postpone_interrupts(access);
}


bool StackGuard::IsInterrupted() {
  ExecutionAccess access(isolate_);
  return (thread_local_.interrupt_flags_ & INTERRUPT) != 0;
}


void StackGuard::Interrupt() {
  ExecutionAccess access(isolate_);
  thread_local_.interrupt_flags_ |= INTERRUPT;
  set_interrupt_limits(access);
}


bool StackGuard::IsPreempted() {
  ExecutionAccess access(isolate_);
  return thread_local_.interrupt_flags_ & PREEMPT;
}


void StackGuard::Preempt() {
  ExecutionAccess access(isolate_);
  thread_local_.interrupt_flags_ |= PREEMPT;
  set_interrupt_limits(access);
}


bool StackGuard::IsTerminateExecution() {
  ExecutionAccess access(isolate_);
  return (thread_local_.interrupt_flags_ & TERMINATE) != 0;
}


void StackGuard::CancelTerminateExecution() {
  ExecutionAccess access(isolate_);
  Continue(TERMINATE);
  isolate_->CancelTerminateExecution();
}


void StackGuard::TerminateExecution() {
  ExecutionAccess access(isolate_);
  thread_local_.interrupt_flags_ |= TERMINATE;
  set_interrupt_limits(access);
}


bool StackGuard::IsGCRequest() {
  ExecutionAccess access(isolate_);
  return (thread_local_.interrupt_flags_ & GC_REQUEST) != 0;
}


void StackGuard::RequestGC() {
  ExecutionAccess access(isolate_);
  thread_local_.interrupt_flags_ |= GC_REQUEST;
  if (thread_local_.postpone_interrupts_nesting_ == 0) {
    thread_local_.jslimit_ = thread_local_.climit_ = kInterruptLimit;
    isolate_->heap()->SetStackLimits();
  }
}


bool StackGuard::IsInstallCodeRequest() {
  ExecutionAccess access(isolate_);
  return (thread_local_.interrupt_flags_ & INSTALL_CODE) != 0;
}


void StackGuard::RequestInstallCode() {
  ExecutionAccess access(isolate_);
  thread_local_.interrupt_flags_ |= INSTALL_CODE;
  if (thread_local_.postpone_interrupts_nesting_ == 0) {
    thread_local_.jslimit_ = thread_local_.climit_ = kInterruptLimit;
    isolate_->heap()->SetStackLimits();
  }
}


bool StackGuard::IsFullDeopt() {
  ExecutionAccess access(isolate_);
  return (thread_local_.interrupt_flags_ & FULL_DEOPT) != 0;
}


void StackGuard::FullDeopt() {
  ExecutionAccess access(isolate_);
  thread_local_.interrupt_flags_ |= FULL_DEOPT;
  set_interrupt_limits(access);
}


bool StackGuard::IsDeoptMarkedAllocationSites() {
  ExecutionAccess access(isolate_);
  return (thread_local_.interrupt_flags_ & DEOPT_MARKED_ALLOCATION_SITES) != 0;
}


void StackGuard::DeoptMarkedAllocationSites() {
  ExecutionAccess access(isolate_);
  thread_local_.interrupt_flags_ |= DEOPT_MARKED_ALLOCATION_SITES;
  set_interrupt_limits(access);
}


bool StackGuard::IsDebugBreak() {
  ExecutionAccess access(isolate_);
  return thread_local_.interrupt_flags_ & DEBUGBREAK;
}


void StackGuard::DebugBreak() {
  ExecutionAccess access(isolate_);
  thread_local_.interrupt_flags_ |= DEBUGBREAK;
  set_interrupt_limits(access);
}


bool StackGuard::IsDebugCommand() {
  ExecutionAccess access(isolate_);
  return thread_local_.interrupt_flags_ & DEBUGCOMMAND;
}


void StackGuard::DebugCommand() {
  ExecutionAccess access(isolate_);
  thread_local_.interrupt_flags_ |= DEBUGCOMMAND;
  set_interrupt_limits(access);
}


void StackGuard::Continue(InterruptFlag after_what) {
  ExecutionAccess access(isolate_);
  thread_local_.interrupt_flags_ &= ~static_cast<int>(after_what);
  if (!should_postpone_interrupts(access) && !has_pending_interrupts(access)) {
    reset_limits(access);
  }
}


void StackGuard::RequestInterrupt(InterruptCallback callback, void* data) {
  ExecutionAccess access(isolate_);
  thread_local_.interrupt_flags_ |= API_INTERRUPT;
  thread_local_.interrupt_callback_ = callback;
  thread_local_.interrupt_callback_data_ = data;
  set_interrupt_limits(access);
}


void StackGuard::ClearInterrupt() {
  thread_local_.interrupt_callback_ = 0;
  thread_local_.interrupt_callback_data_ = 0;
  Continue(API_INTERRUPT);
}


bool StackGuard::IsAPIInterrupt() {
  ExecutionAccess access(isolate_);
  return thread_local_.interrupt_flags_ & API_INTERRUPT;
}


void StackGuard::InvokeInterruptCallback() {
  InterruptCallback callback = 0;
  void* data = 0;

  {
    ExecutionAccess access(isolate_);
    callback = thread_local_.interrupt_callback_;
    data = thread_local_.interrupt_callback_data_;
    thread_local_.interrupt_callback_ = NULL;
    thread_local_.interrupt_callback_data_ = NULL;
  }

  if (callback != NULL) {
    VMState<EXTERNAL> state(isolate_);
    HandleScope handle_scope(isolate_);
    callback(reinterpret_cast<v8::Isolate*>(isolate_), data);
  }
}


char* StackGuard::ArchiveStackGuard(char* to) {
  ExecutionAccess access(isolate_);
  OS::MemCopy(to, reinterpret_cast<char*>(&thread_local_), sizeof(ThreadLocal));
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
  OS::MemCopy(
      reinterpret_cast<char*>(&thread_local_), from, sizeof(ThreadLocal));
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
  nesting_ = 0;
  postpone_interrupts_nesting_ = 0;
  interrupt_flags_ = 0;
  interrupt_callback_ = NULL;
  interrupt_callback_data_ = NULL;
}


bool StackGuard::ThreadLocal::Initialize(Isolate* isolate) {
  bool should_set_stack_limits = false;
  if (real_climit_ == kIllegalLimit) {
    // Takes the address of the limit variable in order to find out where
    // the top of stack is right now.
    const uintptr_t kLimitSize = FLAG_stack_size * KB;
    uintptr_t limit = reinterpret_cast<uintptr_t>(&limit) - kLimitSize;
    ASSERT(reinterpret_cast<uintptr_t>(&limit) > kLimitSize);
    real_jslimit_ = SimulatorStack::JsLimitFromCLimit(isolate, limit);
    jslimit_ = SimulatorStack::JsLimitFromCLimit(isolate, limit);
    real_climit_ = limit;
    climit_ = limit;
    should_set_stack_limits = true;
  }
  nesting_ = 0;
  postpone_interrupts_nesting_ = 0;
  interrupt_flags_ = 0;
  interrupt_callback_ = NULL;
  interrupt_callback_data_ = NULL;
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
                ARRAY_SIZE(argv), argv);                                \
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
               ARRAY_SIZE(index_arg),
               index_arg).ToHandle(&result)) {
    return factory->undefined_value();
  }
  return result;
}


MaybeHandle<JSFunction> Execution::InstantiateFunction(
    Handle<FunctionTemplateInfo> data) {
  Isolate* isolate = data->GetIsolate();
  if (!data->do_not_cache()) {
    // Fast case: see if the function has already been instantiated
    int serial_number = Smi::cast(data->serial_number())->value();
    Handle<JSObject> cache(isolate->native_context()->function_cache());
    Handle<Object> elm =
        Object::GetElement(isolate, cache, serial_number).ToHandleChecked();
    if (elm->IsJSFunction()) return Handle<JSFunction>::cast(elm);
  }
  // The function has not yet been instantiated in this context; do it.
  Handle<Object> args[] = { data };
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      Call(isolate,
           isolate->instantiate_fun(),
           isolate->js_builtins_object(),
           ARRAY_SIZE(args),
           args),
      JSFunction);
  return Handle<JSFunction>::cast(result);
}


MaybeHandle<JSObject> Execution::InstantiateObject(
    Handle<ObjectTemplateInfo> data) {
  Isolate* isolate = data->GetIsolate();
  Handle<Object> result;
  if (data->property_list()->IsUndefined() &&
      !data->constructor()->IsUndefined()) {
    Handle<FunctionTemplateInfo> cons_template =
        Handle<FunctionTemplateInfo>(
            FunctionTemplateInfo::cast(data->constructor()));
    Handle<JSFunction> cons;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, cons, InstantiateFunction(cons_template), JSObject);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, result, New(cons, 0, NULL), JSObject);
  } else {
    Handle<Object> args[] = { data };
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        Call(isolate,
             isolate->instantiate_fun(),
             isolate->js_builtins_object(),
             ARRAY_SIZE(args),
             args),
        JSObject);
  }
  return Handle<JSObject>::cast(result);
}


MaybeHandle<Object> Execution::ConfigureInstance(
    Isolate* isolate,
    Handle<Object> instance,
    Handle<Object> instance_template) {
  Handle<Object> args[] = { instance, instance_template };
  return Execution::Call(isolate,
                         isolate->configure_instance_fun(),
                         isolate->js_builtins_object(),
                         ARRAY_SIZE(args),
                         args);
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
              ARRAY_SIZE(args),
              args);
  Handle<Object> result;
  if (!maybe_result.ToHandle(&result) || !result->IsString()) {
    return isolate->factory()->empty_string();
  }

  return Handle<String>::cast(result);
}


static Object* RuntimePreempt(Isolate* isolate) {
  // Clear the preempt request flag.
  isolate->stack_guard()->Continue(PREEMPT);

  if (isolate->debug()->InDebugger()) {
    // If currently in the debugger don't do any actual preemption but record
    // that preemption occoured while in the debugger.
    isolate->debug()->PreemptionWhileInDebugger();
  } else {
    // Perform preemption.
    v8::Unlocker unlocker(reinterpret_cast<v8::Isolate*>(isolate));
    Thread::YieldCPU();
  }

  return isolate->heap()->undefined_value();
}


Object* Execution::DebugBreakHelper(Isolate* isolate) {
  // Just continue if breaks are disabled.
  if (isolate->debug()->disable_break()) {
    return isolate->heap()->undefined_value();
  }

  // Ignore debug break during bootstrapping.
  if (isolate->bootstrapper()->IsActive()) {
    return isolate->heap()->undefined_value();
  }

  // Ignore debug break if debugger is not active.
  if (!isolate->debugger()->IsDebuggerActive()) {
    return isolate->heap()->undefined_value();
  }

  StackLimitCheck check(isolate);
  if (check.HasOverflowed()) {
    return isolate->heap()->undefined_value();
  }

  {
    JavaScriptFrameIterator it(isolate);
    ASSERT(!it.done());
    Object* fun = it.frame()->function();
    if (fun && fun->IsJSFunction()) {
      // Don't stop in builtin functions.
      if (JSFunction::cast(fun)->IsBuiltin()) {
        return isolate->heap()->undefined_value();
      }
      GlobalObject* global = JSFunction::cast(fun)->context()->global_object();
      // Don't stop in debugger functions.
      if (isolate->debug()->IsDebugGlobal(global)) {
        return isolate->heap()->undefined_value();
      }
    }
  }

  // Collect the break state before clearing the flags.
  bool debug_command_only =
      isolate->stack_guard()->IsDebugCommand() &&
      !isolate->stack_guard()->IsDebugBreak();

  // Clear the debug break request flag.
  isolate->stack_guard()->Continue(DEBUGBREAK);

  ProcessDebugMessages(isolate, debug_command_only);

  // Return to continue execution.
  return isolate->heap()->undefined_value();
}


void Execution::ProcessDebugMessages(Isolate* isolate,
                                     bool debug_command_only) {
  // Clear the debug command request flag.
  isolate->stack_guard()->Continue(DEBUGCOMMAND);

  StackLimitCheck check(isolate);
  if (check.HasOverflowed()) {
    return;
  }

  HandleScope scope(isolate);
  // Enter the debugger. Just continue if we fail to enter the debugger.
  EnterDebugger debugger(isolate);
  if (debugger.FailedToEnter()) {
    return;
  }

  // Notify the debug event listeners. Indicate auto continue if the break was
  // a debug command break.
  isolate->debugger()->OnDebugBreak(isolate->factory()->undefined_value(),
                                    debug_command_only);
}


Object* Execution::HandleStackGuardInterrupt(Isolate* isolate) {
  StackGuard* stack_guard = isolate->stack_guard();
  if (stack_guard->ShouldPostponeInterrupts()) {
    return isolate->heap()->undefined_value();
  }

  if (stack_guard->IsAPIInterrupt()) {
    stack_guard->InvokeInterruptCallback();
    stack_guard->Continue(API_INTERRUPT);
  }

  if (stack_guard->IsGCRequest()) {
    isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags,
                                       "StackGuard GC request");
    stack_guard->Continue(GC_REQUEST);
  }

  isolate->counters()->stack_interrupts()->Increment();
  isolate->counters()->runtime_profiler_ticks()->Increment();
  if (stack_guard->IsDebugBreak() || stack_guard->IsDebugCommand()) {
    DebugBreakHelper(isolate);
  }
  if (stack_guard->IsPreempted()) RuntimePreempt(isolate);
  if (stack_guard->IsTerminateExecution()) {
    stack_guard->Continue(TERMINATE);
    return isolate->TerminateExecution();
  }
  if (stack_guard->IsInterrupted()) {
    stack_guard->Continue(INTERRUPT);
    return isolate->StackOverflow();
  }
  if (stack_guard->IsFullDeopt()) {
    stack_guard->Continue(FULL_DEOPT);
    Deoptimizer::DeoptimizeAll(isolate);
  }
  if (stack_guard->IsDeoptMarkedAllocationSites()) {
    stack_guard->Continue(DEOPT_MARKED_ALLOCATION_SITES);
    isolate->heap()->DeoptMarkedAllocationSites();
  }
  if (stack_guard->IsInstallCodeRequest()) {
    ASSERT(isolate->concurrent_recompilation_enabled());
    stack_guard->Continue(INSTALL_CODE);
    isolate->optimizing_compiler_thread()->InstallOptimizedFunctions();
  }
  isolate->runtime_profiler()->OptimizeNow();
  return isolate->heap()->undefined_value();
}

} }  // namespace v8::internal
