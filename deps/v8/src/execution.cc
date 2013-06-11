// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "v8.h"

#include "api.h"
#include "bootstrapper.h"
#include "codegen.h"
#include "debug.h"
#include "deoptimizer.h"
#include "isolate-inl.h"
#include "runtime-profiler.h"
#include "simulator.h"
#include "v8threads.h"
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


static Handle<Object> Invoke(bool is_construct,
                             Handle<JSFunction> function,
                             Handle<Object> receiver,
                             int argc,
                             Handle<Object> args[],
                             bool* has_pending_exception) {
  Isolate* isolate = function->GetIsolate();

  // Entering JavaScript.
  VMState<JS> state(isolate);

  // Placeholder for return value.
  MaybeObject* value = reinterpret_cast<Object*>(kZapValue);

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
  value->Verify();
#endif

  // Update the pending exception flag and return the value.
  *has_pending_exception = value->IsException();
  ASSERT(*has_pending_exception == isolate->has_pending_exception());
  if (*has_pending_exception) {
    isolate->ReportPendingMessages();
    if (isolate->pending_exception()->IsOutOfMemory()) {
      if (!isolate->ignore_out_of_memory()) {
        V8::FatalProcessOutOfMemory("JS", true);
      }
    }
#ifdef ENABLE_DEBUGGER_SUPPORT
    // Reset stepping state when script exits with uncaught exception.
    if (isolate->debugger()->IsDebuggerActive()) {
      isolate->debug()->ClearStepping();
    }
#endif  // ENABLE_DEBUGGER_SUPPORT
    return Handle<Object>();
  } else {
    isolate->clear_pending_message();
  }

  return Handle<Object>(value->ToObjectUnchecked(), isolate);
}


Handle<Object> Execution::Call(Handle<Object> callable,
                               Handle<Object> receiver,
                               int argc,
                               Handle<Object> argv[],
                               bool* pending_exception,
                               bool convert_receiver) {
  *pending_exception = false;

  if (!callable->IsJSFunction()) {
    callable = TryGetFunctionDelegate(callable, pending_exception);
    if (*pending_exception) return callable;
  }
  Handle<JSFunction> func = Handle<JSFunction>::cast(callable);

  // In non-strict mode, convert receiver.
  if (convert_receiver && !receiver->IsJSReceiver() &&
      !func->shared()->native() && func->shared()->is_classic_mode()) {
    if (receiver->IsUndefined() || receiver->IsNull()) {
      Object* global = func->context()->global_object()->global_receiver();
      // Under some circumstances, 'global' can be the JSBuiltinsObject
      // In that case, don't rewrite.  (FWIW, the same holds for
      // GetIsolate()->global_object()->global_receiver().)
      if (!global->IsJSBuiltinsObject()) {
        receiver = Handle<Object>(global, func->GetIsolate());
      }
    } else {
      receiver = ToObject(receiver, pending_exception);
    }
    if (*pending_exception) return callable;
  }

  return Invoke(false, func, receiver, argc, argv, pending_exception);
}


Handle<Object> Execution::New(Handle<JSFunction> func,
                              int argc,
                              Handle<Object> argv[],
                              bool* pending_exception) {
  return Invoke(true, func, func->GetIsolate()->global_object(), argc, argv,
                pending_exception);
}


Handle<Object> Execution::TryCall(Handle<JSFunction> func,
                                  Handle<Object> receiver,
                                  int argc,
                                  Handle<Object> args[],
                                  bool* caught_exception) {
  // Enter a try-block while executing the JavaScript code. To avoid
  // duplicate error printing it must be non-verbose.  Also, to avoid
  // creating message objects during stack overflow we shouldn't
  // capture messages.
  v8::TryCatch catcher;
  catcher.SetVerbose(false);
  catcher.SetCaptureMessage(false);
  *caught_exception = false;

  Handle<Object> result = Invoke(false, func, receiver, argc, args,
                                 caught_exception);

  Isolate* isolate = func->GetIsolate();
  if (*caught_exception) {
    ASSERT(catcher.HasCaught());
    ASSERT(isolate->has_pending_exception());
    ASSERT(isolate->external_caught_exception());
    if (isolate->is_out_of_memory() && !isolate->ignore_out_of_memory()) {
      V8::FatalProcessOutOfMemory("OOM during Execution::TryCall");
    }
    if (isolate->pending_exception() ==
        isolate->heap()->termination_exception()) {
      result = isolate->factory()->termination_exception();
    } else {
      result = v8::Utils::OpenHandle(*catcher.Exception());
    }
    isolate->OptionalRescheduleException(true);
  }

  ASSERT(!isolate->has_pending_exception());
  ASSERT(!isolate->external_caught_exception());
  return result;
}


Handle<Object> Execution::GetFunctionDelegate(Handle<Object> object) {
  ASSERT(!object->IsJSFunction());
  Isolate* isolate = Isolate::Current();
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


Handle<Object> Execution::TryGetFunctionDelegate(Handle<Object> object,
                                                 bool* has_pending_exception) {
  ASSERT(!object->IsJSFunction());
  Isolate* isolate = Isolate::Current();

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
  isolate->Throw(*error_obj);
  *has_pending_exception = true;

  return isolate->factory()->undefined_value();
}


Handle<Object> Execution::GetConstructorDelegate(Handle<Object> object) {
  ASSERT(!object->IsJSFunction());
  Isolate* isolate = Isolate::Current();

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


Handle<Object> Execution::TryGetConstructorDelegate(
    Handle<Object> object,
    bool* has_pending_exception) {
  ASSERT(!object->IsJSFunction());
  Isolate* isolate = Isolate::Current();

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
  isolate->Throw(*error_obj);
  *has_pending_exception = true;

  return isolate->factory()->undefined_value();
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


bool StackGuard::IsFullDeopt() {
  ExecutionAccess access(isolate_);
  return (thread_local_.interrupt_flags_ & FULL_DEOPT) != 0;
}


void StackGuard::FullDeopt() {
  ExecutionAccess access(isolate_);
  thread_local_.interrupt_flags_ |= FULL_DEOPT;
  set_interrupt_limits(access);
}


#ifdef ENABLE_DEBUGGER_SUPPORT
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
  if (FLAG_debugger_auto_break) {
    ExecutionAccess access(isolate_);
    thread_local_.interrupt_flags_ |= DEBUGCOMMAND;
    set_interrupt_limits(access);
  }
}
#endif

void StackGuard::Continue(InterruptFlag after_what) {
  ExecutionAccess access(isolate_);
  thread_local_.interrupt_flags_ &= ~static_cast<int>(after_what);
  if (!should_postpone_interrupts(access) && !has_pending_interrupts(access)) {
    reset_limits(access);
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

#define RETURN_NATIVE_CALL(name, args, has_pending_exception)           \
  do {                                                                  \
    Isolate* isolate = Isolate::Current();                              \
    Handle<Object> argv[] = args;                                       \
    ASSERT(has_pending_exception != NULL);                              \
    return Call(isolate->name##_fun(),                                  \
                isolate->js_builtins_object(),                          \
                ARRAY_SIZE(argv), argv,                                 \
                has_pending_exception);                                 \
  } while (false)


Handle<Object> Execution::ToNumber(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_number, { obj }, exc);
}


Handle<Object> Execution::ToString(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_string, { obj }, exc);
}


Handle<Object> Execution::ToDetailString(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_detail_string, { obj }, exc);
}


Handle<Object> Execution::ToObject(Handle<Object> obj, bool* exc) {
  if (obj->IsSpecObject()) return obj;
  RETURN_NATIVE_CALL(to_object, { obj }, exc);
}


Handle<Object> Execution::ToInteger(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_integer, { obj }, exc);
}


Handle<Object> Execution::ToUint32(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_uint32, { obj }, exc);
}


Handle<Object> Execution::ToInt32(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_int32, { obj }, exc);
}


Handle<Object> Execution::NewDate(double time, bool* exc) {
  Isolate* isolate = Isolate::Current();
  Handle<Object> time_obj = isolate->factory()->NewNumber(time);
  RETURN_NATIVE_CALL(create_date, { time_obj }, exc);
}


#undef RETURN_NATIVE_CALL


Handle<JSRegExp> Execution::NewJSRegExp(Handle<String> pattern,
                                        Handle<String> flags,
                                        bool* exc) {
  Handle<JSFunction> function = Handle<JSFunction>(
      pattern->GetIsolate()->native_context()->regexp_function());
  Handle<Object> re_obj = RegExpImpl::CreateRegExpLiteral(
      function, pattern, flags, exc);
  if (*exc) return Handle<JSRegExp>();
  return Handle<JSRegExp>::cast(re_obj);
}


Handle<Object> Execution::CharAt(Handle<String> string, uint32_t index) {
  Isolate* isolate = string->GetIsolate();
  Factory* factory = isolate->factory();

  int int_index = static_cast<int>(index);
  if (int_index < 0 || int_index >= string->length()) {
    return factory->undefined_value();
  }

  Handle<Object> char_at = GetProperty(
      isolate, isolate->js_builtins_object(), factory->char_at_string());
  if (!char_at->IsJSFunction()) {
    return factory->undefined_value();
  }

  bool caught_exception;
  Handle<Object> index_object = factory->NewNumberFromInt(int_index);
  Handle<Object> index_arg[] = { index_object };
  Handle<Object> result = TryCall(Handle<JSFunction>::cast(char_at),
                                  string,
                                  ARRAY_SIZE(index_arg),
                                  index_arg,
                                  &caught_exception);
  if (caught_exception) {
    return factory->undefined_value();
  }
  return result;
}


Handle<JSFunction> Execution::InstantiateFunction(
    Handle<FunctionTemplateInfo> data,
    bool* exc) {
  Isolate* isolate = data->GetIsolate();
  // Fast case: see if the function has already been instantiated
  int serial_number = Smi::cast(data->serial_number())->value();
  Object* elm =
      isolate->native_context()->function_cache()->
          GetElementNoExceptionThrown(serial_number);
  if (elm->IsJSFunction()) return Handle<JSFunction>(JSFunction::cast(elm));
  // The function has not yet been instantiated in this context; do it.
  Handle<Object> args[] = { data };
  Handle<Object> result = Call(isolate->instantiate_fun(),
                               isolate->js_builtins_object(),
                               ARRAY_SIZE(args),
                               args,
                               exc);
  if (*exc) return Handle<JSFunction>::null();
  return Handle<JSFunction>::cast(result);
}


Handle<JSObject> Execution::InstantiateObject(Handle<ObjectTemplateInfo> data,
                                              bool* exc) {
  Isolate* isolate = data->GetIsolate();
  if (data->property_list()->IsUndefined() &&
      !data->constructor()->IsUndefined()) {
    // Initialization to make gcc happy.
    Object* result = NULL;
    {
      HandleScope scope(isolate);
      Handle<FunctionTemplateInfo> cons_template =
          Handle<FunctionTemplateInfo>(
              FunctionTemplateInfo::cast(data->constructor()));
      Handle<JSFunction> cons = InstantiateFunction(cons_template, exc);
      if (*exc) return Handle<JSObject>::null();
      Handle<Object> value = New(cons, 0, NULL, exc);
      if (*exc) return Handle<JSObject>::null();
      result = *value;
    }
    ASSERT(!*exc);
    return Handle<JSObject>(JSObject::cast(result));
  } else {
    Handle<Object> args[] = { data };
    Handle<Object> result = Call(isolate->instantiate_fun(),
                                 isolate->js_builtins_object(),
                                 ARRAY_SIZE(args),
                                 args,
                                 exc);
    if (*exc) return Handle<JSObject>::null();
    return Handle<JSObject>::cast(result);
  }
}


void Execution::ConfigureInstance(Handle<Object> instance,
                                  Handle<Object> instance_template,
                                  bool* exc) {
  Isolate* isolate = Isolate::Current();
  Handle<Object> args[] = { instance, instance_template };
  Execution::Call(isolate->configure_instance_fun(),
                  isolate->js_builtins_object(),
                  ARRAY_SIZE(args),
                  args,
                  exc);
}


Handle<String> Execution::GetStackTraceLine(Handle<Object> recv,
                                            Handle<JSFunction> fun,
                                            Handle<Object> pos,
                                            Handle<Object> is_global) {
  Isolate* isolate = fun->GetIsolate();
  Handle<Object> args[] = { recv, fun, pos, is_global };
  bool caught_exception;
  Handle<Object> result = TryCall(isolate->get_stack_trace_line_fun(),
                                  isolate->js_builtins_object(),
                                  ARRAY_SIZE(args),
                                  args,
                                  &caught_exception);
  if (caught_exception || !result->IsString()) {
      return isolate->factory()->empty_string();
  }

  return Handle<String>::cast(result);
}


static Object* RuntimePreempt() {
  Isolate* isolate = Isolate::Current();

  // Clear the preempt request flag.
  isolate->stack_guard()->Continue(PREEMPT);

  ContextSwitcher::PreemptionReceived();

#ifdef ENABLE_DEBUGGER_SUPPORT
  if (isolate->debug()->InDebugger()) {
    // If currently in the debugger don't do any actual preemption but record
    // that preemption occoured while in the debugger.
    isolate->debug()->PreemptionWhileInDebugger();
  } else {
    // Perform preemption.
    v8::Unlocker unlocker(reinterpret_cast<v8::Isolate*>(isolate));
    Thread::YieldCPU();
  }
#else
  { // NOLINT
    // Perform preemption.
    v8::Unlocker unlocker(reinterpret_cast<v8::Isolate*>(isolate));
    Thread::YieldCPU();
  }
#endif

  return isolate->heap()->undefined_value();
}


#ifdef ENABLE_DEBUGGER_SUPPORT
Object* Execution::DebugBreakHelper() {
  Isolate* isolate = Isolate::Current();

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

  ProcessDebugMessages(debug_command_only);

  // Return to continue execution.
  return isolate->heap()->undefined_value();
}

void Execution::ProcessDebugMessages(bool debug_command_only) {
  Isolate* isolate = Isolate::Current();
  // Clear the debug command request flag.
  isolate->stack_guard()->Continue(DEBUGCOMMAND);

  StackLimitCheck check(isolate);
  if (check.HasOverflowed()) {
    return;
  }

  HandleScope scope(isolate);
  // Enter the debugger. Just continue if we fail to enter the debugger.
  EnterDebugger debugger;
  if (debugger.FailedToEnter()) {
    return;
  }

  // Notify the debug event listeners. Indicate auto continue if the break was
  // a debug command break.
  isolate->debugger()->OnDebugBreak(isolate->factory()->undefined_value(),
                                    debug_command_only);
}


#endif

MaybeObject* Execution::HandleStackGuardInterrupt(Isolate* isolate) {
  StackGuard* stack_guard = isolate->stack_guard();
  if (stack_guard->ShouldPostponeInterrupts()) {
    return isolate->heap()->undefined_value();
  }

  if (stack_guard->IsGCRequest()) {
    isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags,
                                       "StackGuard GC request");
    stack_guard->Continue(GC_REQUEST);
  }

  isolate->counters()->stack_interrupts()->Increment();
  isolate->counters()->runtime_profiler_ticks()->Increment();
  isolate->runtime_profiler()->OptimizeNow();
#ifdef ENABLE_DEBUGGER_SUPPORT
  if (stack_guard->IsDebugBreak() || stack_guard->IsDebugCommand()) {
    DebugBreakHelper();
  }
#endif
  if (stack_guard->IsPreempted()) RuntimePreempt();
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
  return isolate->heap()->undefined_value();
}


} }  // namespace v8::internal
