// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
#include "codegen-inl.h"
#include "debug.h"
#include "simulator.h"
#include "v8threads.h"

namespace v8 {
namespace internal {


static Handle<Object> Invoke(bool construct,
                             Handle<JSFunction> func,
                             Handle<Object> receiver,
                             int argc,
                             Object*** args,
                             bool* has_pending_exception) {
  // Entering JavaScript.
  VMState state(JS);

  // Placeholder for return value.
  Object* value = reinterpret_cast<Object*>(kZapValue);

  typedef Object* (*JSEntryFunction)(
    byte* entry,
    Object* function,
    Object* receiver,
    int argc,
    Object*** args);

  Handle<Code> code;
  if (construct) {
    JSConstructEntryStub stub;
    code = stub.GetCode();
  } else {
    JSEntryStub stub;
    code = stub.GetCode();
  }

  // Convert calls on global objects to be calls on the global
  // receiver instead to avoid having a 'this' pointer which refers
  // directly to a global object.
  if (receiver->IsGlobalObject()) {
    Handle<GlobalObject> global = Handle<GlobalObject>::cast(receiver);
    receiver = Handle<JSObject>(global->global_receiver());
  }

  // Make sure that the global object of the context we're about to
  // make the current one is indeed a global object.
  ASSERT(func->context()->global()->IsGlobalObject());

  {
    // Save and restore context around invocation and block the
    // allocation of handles without explicit handle scopes.
    SaveContext save;
    NoHandleAllocation na;
    JSEntryFunction entry = FUNCTION_CAST<JSEntryFunction>(code->entry());

    // Call the function through the right JS entry stub.
    byte* entry_address = func->code()->entry();
    JSFunction* function = *func;
    Object* receiver_pointer = *receiver;
    value = CALL_GENERATED_CODE(entry, entry_address, function,
                                receiver_pointer, argc, args);
  }

#ifdef DEBUG
  value->Verify();
#endif

  // Update the pending exception flag and return the value.
  *has_pending_exception = value->IsException();
  ASSERT(*has_pending_exception == Top::has_pending_exception());
  if (*has_pending_exception) {
    Top::ReportPendingMessages();
    return Handle<Object>();
  } else {
    Top::clear_pending_message();
  }

  return Handle<Object>(value);
}


Handle<Object> Execution::Call(Handle<JSFunction> func,
                               Handle<Object> receiver,
                               int argc,
                               Object*** args,
                               bool* pending_exception) {
  return Invoke(false, func, receiver, argc, args, pending_exception);
}


Handle<Object> Execution::New(Handle<JSFunction> func, int argc,
                              Object*** args, bool* pending_exception) {
  return Invoke(true, func, Top::global(), argc, args, pending_exception);
}


Handle<Object> Execution::TryCall(Handle<JSFunction> func,
                                  Handle<Object> receiver,
                                  int argc,
                                  Object*** args,
                                  bool* caught_exception) {
  // Enter a try-block while executing the JavaScript code. To avoid
  // duplicate error printing it must be non-verbose.  Also, to avoid
  // creating message objects during stack overflow we shouldn't
  // capture messages.
  v8::TryCatch catcher;
  catcher.SetVerbose(false);
  catcher.SetCaptureMessage(false);

  Handle<Object> result = Invoke(false, func, receiver, argc, args,
                                 caught_exception);

  if (*caught_exception) {
    ASSERT(catcher.HasCaught());
    ASSERT(Top::has_pending_exception());
    ASSERT(Top::external_caught_exception());
    if (Top::pending_exception() == Heap::termination_exception()) {
      result = Factory::termination_exception();
    } else {
      result = v8::Utils::OpenHandle(*catcher.Exception());
    }
    Top::OptionalRescheduleException(true);
  }

  ASSERT(!Top::has_pending_exception());
  ASSERT(!Top::external_caught_exception());
  return result;
}


Handle<Object> Execution::GetFunctionDelegate(Handle<Object> object) {
  ASSERT(!object->IsJSFunction());

  // If you return a function from here, it will be called when an
  // attempt is made to call the given object as a function.

  // Regular expressions can be called as functions in both Firefox
  // and Safari so we allow it too.
  if (object->IsJSRegExp()) {
    Handle<String> exec = Factory::exec_symbol();
    return Handle<Object>(object->GetProperty(*exec));
  }

  // Objects created through the API can have an instance-call handler
  // that should be used when calling the object as a function.
  if (object->IsHeapObject() &&
      HeapObject::cast(*object)->map()->has_instance_call_handler()) {
    return Handle<JSFunction>(
        Top::global_context()->call_as_function_delegate());
  }

  return Factory::undefined_value();
}


Handle<Object> Execution::GetConstructorDelegate(Handle<Object> object) {
  ASSERT(!object->IsJSFunction());

  // If you return a function from here, it will be called when an
  // attempt is made to call the given object as a constructor.

  // Objects created through the API can have an instance-call handler
  // that should be used when calling the object as a function.
  if (object->IsHeapObject() &&
      HeapObject::cast(*object)->map()->has_instance_call_handler()) {
    return Handle<JSFunction>(
        Top::global_context()->call_as_constructor_delegate());
  }

  return Factory::undefined_value();
}


// Static state for stack guards.
StackGuard::ThreadLocal StackGuard::thread_local_;


bool StackGuard::IsStackOverflow() {
  ExecutionAccess access;
  return (thread_local_.jslimit_ != kInterruptLimit &&
          thread_local_.climit_ != kInterruptLimit);
}


void StackGuard::EnableInterrupts() {
  ExecutionAccess access;
  if (has_pending_interrupts(access)) {
    set_interrupt_limits(access);
  }
}


void StackGuard::SetStackLimit(uintptr_t limit) {
  ExecutionAccess access;
  // If the current limits are special (eg due to a pending interrupt) then
  // leave them alone.
  uintptr_t jslimit = SimulatorStack::JsLimitFromCLimit(limit);
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
  ExecutionAccess access;
  reset_limits(access);
}


bool StackGuard::IsInterrupted() {
  ExecutionAccess access;
  return thread_local_.interrupt_flags_ & INTERRUPT;
}


void StackGuard::Interrupt() {
  ExecutionAccess access;
  thread_local_.interrupt_flags_ |= INTERRUPT;
  set_interrupt_limits(access);
}


bool StackGuard::IsPreempted() {
  ExecutionAccess access;
  return thread_local_.interrupt_flags_ & PREEMPT;
}


void StackGuard::Preempt() {
  ExecutionAccess access;
  thread_local_.interrupt_flags_ |= PREEMPT;
  set_interrupt_limits(access);
}


bool StackGuard::IsTerminateExecution() {
  ExecutionAccess access;
  return thread_local_.interrupt_flags_ & TERMINATE;
}


void StackGuard::TerminateExecution() {
  ExecutionAccess access;
  thread_local_.interrupt_flags_ |= TERMINATE;
  set_interrupt_limits(access);
}


#ifdef ENABLE_DEBUGGER_SUPPORT
bool StackGuard::IsDebugBreak() {
  ExecutionAccess access;
  return thread_local_.interrupt_flags_ & DEBUGBREAK;
}


void StackGuard::DebugBreak() {
  ExecutionAccess access;
  thread_local_.interrupt_flags_ |= DEBUGBREAK;
  set_interrupt_limits(access);
}


bool StackGuard::IsDebugCommand() {
  ExecutionAccess access;
  return thread_local_.interrupt_flags_ & DEBUGCOMMAND;
}


void StackGuard::DebugCommand() {
  if (FLAG_debugger_auto_break) {
    ExecutionAccess access;
    thread_local_.interrupt_flags_ |= DEBUGCOMMAND;
    set_interrupt_limits(access);
  }
}
#endif

void StackGuard::Continue(InterruptFlag after_what) {
  ExecutionAccess access;
  thread_local_.interrupt_flags_ &= ~static_cast<int>(after_what);
  if (!should_postpone_interrupts(access) && !has_pending_interrupts(access)) {
    reset_limits(access);
  }
}


int StackGuard::ArchiveSpacePerThread() {
  return sizeof(ThreadLocal);
}


char* StackGuard::ArchiveStackGuard(char* to) {
  ExecutionAccess access;
  memcpy(to, reinterpret_cast<char*>(&thread_local_), sizeof(ThreadLocal));
  ThreadLocal blank;
  thread_local_ = blank;
  return to + sizeof(ThreadLocal);
}


char* StackGuard::RestoreStackGuard(char* from) {
  ExecutionAccess access;
  memcpy(reinterpret_cast<char*>(&thread_local_), from, sizeof(ThreadLocal));
  Heap::SetStackLimits();
  return from + sizeof(ThreadLocal);
}


static internal::Thread::LocalStorageKey stack_limit_key =
    internal::Thread::CreateThreadLocalKey();


void StackGuard::FreeThreadResources() {
  Thread::SetThreadLocal(
      stack_limit_key,
      reinterpret_cast<void*>(thread_local_.real_climit_));
}


void StackGuard::ThreadLocal::Clear() {
  real_jslimit_ = kIllegalLimit;
  jslimit_ = kIllegalLimit;
  real_climit_ = kIllegalLimit;
  climit_ = kIllegalLimit;
  nesting_ = 0;
  postpone_interrupts_nesting_ = 0;
  interrupt_flags_ = 0;
  Heap::SetStackLimits();
}


void StackGuard::ThreadLocal::Initialize() {
  if (real_climit_ == kIllegalLimit) {
    // Takes the address of the limit variable in order to find out where
    // the top of stack is right now.
    uintptr_t limit = reinterpret_cast<uintptr_t>(&limit) - kLimitSize;
    ASSERT(reinterpret_cast<uintptr_t>(&limit) > kLimitSize);
    real_jslimit_ = SimulatorStack::JsLimitFromCLimit(limit);
    jslimit_ = SimulatorStack::JsLimitFromCLimit(limit);
    real_climit_ = limit;
    climit_ = limit;
    Heap::SetStackLimits();
  }
  nesting_ = 0;
  postpone_interrupts_nesting_ = 0;
  interrupt_flags_ = 0;
}


void StackGuard::ClearThread(const ExecutionAccess& lock) {
  thread_local_.Clear();
}


void StackGuard::InitThread(const ExecutionAccess& lock) {
  thread_local_.Initialize();
  void* stored_limit = Thread::GetThreadLocal(stack_limit_key);
  // You should hold the ExecutionAccess lock when you call this.
  if (stored_limit != NULL) {
    StackGuard::SetStackLimit(reinterpret_cast<intptr_t>(stored_limit));
  }
}


// --- C a l l s   t o   n a t i v e s ---

#define RETURN_NATIVE_CALL(name, argc, argv, has_pending_exception) \
  do {                                                              \
    Object** args[argc] = argv;                                     \
    ASSERT(has_pending_exception != NULL);                          \
    return Call(Top::name##_fun(), Top::builtins(), argc, args,     \
                has_pending_exception);                             \
  } while (false)


Handle<Object> Execution::ToBoolean(Handle<Object> obj) {
  // See the similar code in runtime.js:ToBoolean.
  if (obj->IsBoolean()) return obj;
  bool result = true;
  if (obj->IsString()) {
    result = Handle<String>::cast(obj)->length() != 0;
  } else if (obj->IsNull() || obj->IsUndefined()) {
    result = false;
  } else if (obj->IsNumber()) {
    double value = obj->Number();
    result = !((value == 0) || isnan(value));
  }
  return Handle<Object>(Heap::ToBoolean(result));
}


Handle<Object> Execution::ToNumber(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_number, 1, { obj.location() }, exc);
}


Handle<Object> Execution::ToString(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_string, 1, { obj.location() }, exc);
}


Handle<Object> Execution::ToDetailString(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_detail_string, 1, { obj.location() }, exc);
}


Handle<Object> Execution::ToObject(Handle<Object> obj, bool* exc) {
  if (obj->IsJSObject()) return obj;
  RETURN_NATIVE_CALL(to_object, 1, { obj.location() }, exc);
}


Handle<Object> Execution::ToInteger(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_integer, 1, { obj.location() }, exc);
}


Handle<Object> Execution::ToUint32(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_uint32, 1, { obj.location() }, exc);
}


Handle<Object> Execution::ToInt32(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_int32, 1, { obj.location() }, exc);
}


Handle<Object> Execution::NewDate(double time, bool* exc) {
  Handle<Object> time_obj = Factory::NewNumber(time);
  RETURN_NATIVE_CALL(create_date, 1, { time_obj.location() }, exc);
}


#undef RETURN_NATIVE_CALL


Handle<Object> Execution::CharAt(Handle<String> string, uint32_t index) {
  int int_index = static_cast<int>(index);
  if (int_index < 0 || int_index >= string->length()) {
    return Factory::undefined_value();
  }

  Handle<Object> char_at =
      GetProperty(Top::builtins(), Factory::char_at_symbol());
  if (!char_at->IsJSFunction()) {
    return Factory::undefined_value();
  }

  bool caught_exception;
  Handle<Object> index_object = Factory::NewNumberFromInt(int_index);
  Object** index_arg[] = { index_object.location() };
  Handle<Object> result = TryCall(Handle<JSFunction>::cast(char_at),
                                  string,
                                  ARRAY_SIZE(index_arg),
                                  index_arg,
                                  &caught_exception);
  if (caught_exception) {
    return Factory::undefined_value();
  }
  return result;
}


Handle<JSFunction> Execution::InstantiateFunction(
    Handle<FunctionTemplateInfo> data, bool* exc) {
  // Fast case: see if the function has already been instantiated
  int serial_number = Smi::cast(data->serial_number())->value();
  Object* elm =
      Top::global_context()->function_cache()->GetElement(serial_number);
  if (elm->IsJSFunction()) return Handle<JSFunction>(JSFunction::cast(elm));
  // The function has not yet been instantiated in this context; do it.
  Object** args[1] = { Handle<Object>::cast(data).location() };
  Handle<Object> result =
      Call(Top::instantiate_fun(), Top::builtins(), 1, args, exc);
  if (*exc) return Handle<JSFunction>::null();
  return Handle<JSFunction>::cast(result);
}


Handle<JSObject> Execution::InstantiateObject(Handle<ObjectTemplateInfo> data,
                                              bool* exc) {
  if (data->property_list()->IsUndefined() &&
      !data->constructor()->IsUndefined()) {
    // Initialization to make gcc happy.
    Object* result = NULL;
    {
      HandleScope scope;
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
    Object** args[1] = { Handle<Object>::cast(data).location() };
    Handle<Object> result =
        Call(Top::instantiate_fun(), Top::builtins(), 1, args, exc);
    if (*exc) return Handle<JSObject>::null();
    return Handle<JSObject>::cast(result);
  }
}


void Execution::ConfigureInstance(Handle<Object> instance,
                                  Handle<Object> instance_template,
                                  bool* exc) {
  Object** args[2] = { instance.location(), instance_template.location() };
  Execution::Call(Top::configure_instance_fun(), Top::builtins(), 2, args, exc);
}


Handle<String> Execution::GetStackTraceLine(Handle<Object> recv,
                                            Handle<JSFunction> fun,
                                            Handle<Object> pos,
                                            Handle<Object> is_global) {
  const int argc = 4;
  Object** args[argc] = { recv.location(),
                          Handle<Object>::cast(fun).location(),
                          pos.location(),
                          is_global.location() };
  bool caught_exception = false;
  Handle<Object> result = TryCall(Top::get_stack_trace_line_fun(),
                                  Top::builtins(), argc, args,
                                  &caught_exception);
  if (caught_exception || !result->IsString()) return Factory::empty_symbol();
  return Handle<String>::cast(result);
}


static Object* RuntimePreempt() {
  // Clear the preempt request flag.
  StackGuard::Continue(PREEMPT);

  ContextSwitcher::PreemptionReceived();

#ifdef ENABLE_DEBUGGER_SUPPORT
  if (Debug::InDebugger()) {
    // If currently in the debugger don't do any actual preemption but record
    // that preemption occoured while in the debugger.
    Debug::PreemptionWhileInDebugger();
  } else {
    // Perform preemption.
    v8::Unlocker unlocker;
    Thread::YieldCPU();
  }
#else
  // Perform preemption.
  v8::Unlocker unlocker;
  Thread::YieldCPU();
#endif

  return Heap::undefined_value();
}


#ifdef ENABLE_DEBUGGER_SUPPORT
Object* Execution::DebugBreakHelper() {
  // Just continue if breaks are disabled.
  if (Debug::disable_break()) {
    return Heap::undefined_value();
  }

  // Ignore debug break during bootstrapping.
  if (Bootstrapper::IsActive()) {
    return Heap::undefined_value();
  }

  {
    JavaScriptFrameIterator it;
    ASSERT(!it.done());
    Object* fun = it.frame()->function();
    if (fun && fun->IsJSFunction()) {
      // Don't stop in builtin functions.
      if (JSFunction::cast(fun)->IsBuiltin()) {
        return Heap::undefined_value();
      }
      GlobalObject* global = JSFunction::cast(fun)->context()->global();
      // Don't stop in debugger functions.
      if (Debug::IsDebugGlobal(global)) {
        return Heap::undefined_value();
      }
    }
  }

  // Collect the break state before clearing the flags.
  bool debug_command_only =
      StackGuard::IsDebugCommand() && !StackGuard::IsDebugBreak();

  // Clear the debug break request flag.
  StackGuard::Continue(DEBUGBREAK);

  ProcessDebugMesssages(debug_command_only);

  // Return to continue execution.
  return Heap::undefined_value();
}

void Execution::ProcessDebugMesssages(bool debug_command_only) {
  // Clear the debug command request flag.
  StackGuard::Continue(DEBUGCOMMAND);

  HandleScope scope;
  // Enter the debugger. Just continue if we fail to enter the debugger.
  EnterDebugger debugger;
  if (debugger.FailedToEnter()) {
    return;
  }

  // Notify the debug event listeners. Indicate auto continue if the break was
  // a debug command break.
  Debugger::OnDebugBreak(Factory::undefined_value(), debug_command_only);
}


#endif

Object* Execution::HandleStackGuardInterrupt() {
#ifdef ENABLE_DEBUGGER_SUPPORT
  if (StackGuard::IsDebugBreak() || StackGuard::IsDebugCommand()) {
    DebugBreakHelper();
  }
#endif
  if (StackGuard::IsPreempted()) RuntimePreempt();
  if (StackGuard::IsTerminateExecution()) {
    StackGuard::Continue(TERMINATE);
    return Top::TerminateExecution();
  }
  if (StackGuard::IsInterrupted()) {
    // interrupt
    StackGuard::Continue(INTERRUPT);
    return Top::StackOverflow();
  }
  return Heap::undefined_value();
}

// --- G C   E x t e n s i o n ---

const char* const GCExtension::kSource = "native function gc();";


v8::Handle<v8::FunctionTemplate> GCExtension::GetNativeFunction(
    v8::Handle<v8::String> str) {
  return v8::FunctionTemplate::New(GCExtension::GC);
}


v8::Handle<v8::Value> GCExtension::GC(const v8::Arguments& args) {
  // All allocation spaces other than NEW_SPACE have the same effect.
  Heap::CollectAllGarbage(false);
  return v8::Undefined();
}


static GCExtension gc_extension;
static v8::DeclareExtension gc_extension_declaration(&gc_extension);


// --- E x t e r n a l i z e S t r i n g   E x t e n s i o n ---


template <typename Char, typename Base>
class SimpleStringResource : public Base {
 public:
  // Takes ownership of |data|.
  SimpleStringResource(Char* data, size_t length)
      : data_(data),
        length_(length) {}

  virtual ~SimpleStringResource() { delete[] data_; }

  virtual const Char* data() const { return data_; }

  virtual size_t length() const { return length_; }

 private:
  Char* const data_;
  const size_t length_;
};


typedef SimpleStringResource<char, v8::String::ExternalAsciiStringResource>
    SimpleAsciiStringResource;
typedef SimpleStringResource<uc16, v8::String::ExternalStringResource>
    SimpleTwoByteStringResource;


const char* const ExternalizeStringExtension::kSource =
    "native function externalizeString();"
    "native function isAsciiString();";


v8::Handle<v8::FunctionTemplate> ExternalizeStringExtension::GetNativeFunction(
    v8::Handle<v8::String> str) {
  if (strcmp(*v8::String::AsciiValue(str), "externalizeString") == 0) {
    return v8::FunctionTemplate::New(ExternalizeStringExtension::Externalize);
  } else {
    ASSERT(strcmp(*v8::String::AsciiValue(str), "isAsciiString") == 0);
    return v8::FunctionTemplate::New(ExternalizeStringExtension::IsAscii);
  }
}


v8::Handle<v8::Value> ExternalizeStringExtension::Externalize(
    const v8::Arguments& args) {
  if (args.Length() < 1 || !args[0]->IsString()) {
    return v8::ThrowException(v8::String::New(
        "First parameter to externalizeString() must be a string."));
  }
  bool force_two_byte = false;
  if (args.Length() >= 2) {
    if (args[1]->IsBoolean()) {
      force_two_byte = args[1]->BooleanValue();
    } else {
      return v8::ThrowException(v8::String::New(
          "Second parameter to externalizeString() must be a boolean."));
    }
  }
  bool result = false;
  Handle<String> string = Utils::OpenHandle(*args[0].As<v8::String>());
  if (string->IsExternalString()) {
    return v8::ThrowException(v8::String::New(
        "externalizeString() can't externalize twice."));
  }
  if (string->IsAsciiRepresentation() && !force_two_byte) {
    char* data = new char[string->length()];
    String::WriteToFlat(*string, data, 0, string->length());
    SimpleAsciiStringResource* resource = new SimpleAsciiStringResource(
        data, string->length());
    result = string->MakeExternal(resource);
    if (result && !string->IsSymbol()) {
      i::ExternalStringTable::AddString(*string);
    }
  } else {
    uc16* data = new uc16[string->length()];
    String::WriteToFlat(*string, data, 0, string->length());
    SimpleTwoByteStringResource* resource = new SimpleTwoByteStringResource(
        data, string->length());
    result = string->MakeExternal(resource);
    if (result && !string->IsSymbol()) {
      i::ExternalStringTable::AddString(*string);
    }
  }
  if (!result) {
    return v8::ThrowException(v8::String::New("externalizeString() failed."));
  }
  return v8::Undefined();
}


v8::Handle<v8::Value> ExternalizeStringExtension::IsAscii(
    const v8::Arguments& args) {
  if (args.Length() != 1 || !args[0]->IsString()) {
    return v8::ThrowException(v8::String::New(
        "isAsciiString() requires a single string argument."));
  }
  return Utils::OpenHandle(*args[0].As<v8::String>())->IsAsciiRepresentation() ?
      v8::True() : v8::False();
}


static ExternalizeStringExtension externalize_extension;
static v8::DeclareExtension externalize_extension_declaration(
    &externalize_extension);

} }  // namespace v8::internal
