// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/ast/prettyprinter.h"
#include "src/bootstrapper.h"
#include "src/conversions.h"
#include "src/debug/debug.h"
#include "src/frames-inl.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/parsing/parser.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CheckIsBootstrapping) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_ExportFromRuntime) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, container, 0);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  JSObject::NormalizeProperties(container, KEEP_INOBJECT_PROPERTIES, 10,
                                "ExportFromRuntime");
  Bootstrapper::ExportFromRuntime(isolate, container);
  JSObject::MigrateSlowToFast(container, 0, "ExportFromRuntime");
  return *container;
}


RUNTIME_FUNCTION(Runtime_ExportExperimentalFromRuntime) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, container, 0);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  JSObject::NormalizeProperties(container, KEEP_INOBJECT_PROPERTIES, 10,
                                "ExportExperimentalFromRuntime");
  Bootstrapper::ExportExperimentalFromRuntime(isolate, container);
  JSObject::MigrateSlowToFast(container, 0, "ExportExperimentalFromRuntime");
  return *container;
}


RUNTIME_FUNCTION(Runtime_InstallToContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  RUNTIME_ASSERT(array->HasFastElements());
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  Handle<Context> native_context = isolate->native_context();
  Handle<FixedArray> fixed_array(FixedArray::cast(array->elements()));
  int length = Smi::cast(array->length())->value();
  for (int i = 0; i < length; i += 2) {
    RUNTIME_ASSERT(fixed_array->get(i)->IsString());
    Handle<String> name(String::cast(fixed_array->get(i)));
    RUNTIME_ASSERT(fixed_array->get(i + 1)->IsJSObject());
    Handle<JSObject> object(JSObject::cast(fixed_array->get(i + 1)));
    int index = Context::ImportedFieldIndexForName(name);
    if (index == Context::kNotFound) {
      index = Context::IntrinsicIndexForName(name);
    }
    RUNTIME_ASSERT(index != Context::kNotFound);
    native_context->set(index, *object);
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_Throw) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  return isolate->Throw(args[0]);
}


RUNTIME_FUNCTION(Runtime_ReThrow) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  return isolate->ReThrow(args[0]);
}


RUNTIME_FUNCTION(Runtime_ThrowStackOverflow) {
  SealHandleScope shs(isolate);
  DCHECK_LE(0, args.length());
  return isolate->StackOverflow();
}


RUNTIME_FUNCTION(Runtime_UnwindAndFindExceptionHandler) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return isolate->UnwindAndFindHandler();
}


RUNTIME_FUNCTION(Runtime_PromoteScheduledException) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return isolate->PromoteScheduledException();
}


RUNTIME_FUNCTION(Runtime_ThrowReferenceError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewReferenceError(MessageTemplate::kNotDefined, name));
}


RUNTIME_FUNCTION(Runtime_NewTypeError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg0, 1);
  auto message_template =
      static_cast<MessageTemplate::Template>(template_index);
  return *isolate->factory()->NewTypeError(message_template, arg0);
}


RUNTIME_FUNCTION(Runtime_NewReferenceError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg0, 1);
  auto message_template =
      static_cast<MessageTemplate::Template>(template_index);
  return *isolate->factory()->NewReferenceError(message_template, arg0);
}


RUNTIME_FUNCTION(Runtime_NewSyntaxError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg0, 1);
  auto message_template =
      static_cast<MessageTemplate::Template>(template_index);
  return *isolate->factory()->NewSyntaxError(message_template, arg0);
}


RUNTIME_FUNCTION(Runtime_ThrowIllegalInvocation) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kIllegalInvocation));
}


RUNTIME_FUNCTION(Runtime_ThrowIteratorResultNotAnObject) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate,
      NewTypeError(MessageTemplate::kIteratorResultNotAnObject, value));
}


RUNTIME_FUNCTION(Runtime_ThrowStrongModeImplicitConversion) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kStrongImplicitConversion));
}


RUNTIME_FUNCTION(Runtime_ThrowApplyNonFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  Handle<String> type = Object::TypeOf(isolate, object);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kApplyNonFunction, object, type));
}


RUNTIME_FUNCTION(Runtime_PromiseRejectEvent) {
  DCHECK(args.length() == 3);
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  CONVERT_BOOLEAN_ARG_CHECKED(debug_event, 2);
  if (debug_event) isolate->debug()->OnPromiseReject(promise, value);
  Handle<Symbol> key = isolate->factory()->promise_has_handler_symbol();
  // Do not report if we actually have a handler.
  if (JSReceiver::GetDataProperty(promise, key)->IsUndefined()) {
    isolate->ReportPromiseReject(promise, value,
                                 v8::kPromiseRejectWithNoHandler);
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_PromiseRevokeReject) {
  DCHECK(args.length() == 1);
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, promise, 0);
  Handle<Symbol> key = isolate->factory()->promise_has_handler_symbol();
  // At this point, no revocation has been issued before
  RUNTIME_ASSERT(JSReceiver::GetDataProperty(promise, key)->IsUndefined());
  isolate->ReportPromiseReject(promise, Handle<Object>(),
                               v8::kPromiseHandlerAddedAfterReject);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_StackGuard) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);

  // First check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) {
    return isolate->StackOverflow();
  }

  return isolate->stack_guard()->HandleInterrupts();
}


RUNTIME_FUNCTION(Runtime_Interrupt) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return isolate->stack_guard()->HandleInterrupts();
}


RUNTIME_FUNCTION(Runtime_AllocateInNewSpace) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_SMI_ARG_CHECKED(size, 0);
  RUNTIME_ASSERT(IsAligned(size, kPointerSize));
  RUNTIME_ASSERT(size > 0);
  RUNTIME_ASSERT(size <= Page::kMaxRegularHeapObjectSize);
  return *isolate->factory()->NewFillerObject(size, false, NEW_SPACE);
}


RUNTIME_FUNCTION(Runtime_AllocateInTargetSpace) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(size, 0);
  CONVERT_SMI_ARG_CHECKED(flags, 1);
  RUNTIME_ASSERT(IsAligned(size, kPointerSize));
  RUNTIME_ASSERT(size > 0);
  RUNTIME_ASSERT(size <= Page::kMaxRegularHeapObjectSize);
  bool double_align = AllocateDoubleAlignFlag::decode(flags);
  AllocationSpace space = AllocateTargetSpace::decode(flags);
  return *isolate->factory()->NewFillerObject(size, double_align, space);
}


// Collect the raw data for a stack trace.  Returns an array of 4
// element segments each containing a receiver, function, code and
// native code offset.
RUNTIME_FUNCTION(Runtime_CollectStackTrace) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, error_object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, caller, 1);

  if (!isolate->bootstrapper()->IsActive()) {
    // Optionally capture a more detailed stack trace for the message.
    RETURN_FAILURE_ON_EXCEPTION(
        isolate, isolate->CaptureAndSetDetailedStackTrace(error_object));
    // Capture a simple stack trace for the stack property.
    RETURN_FAILURE_ON_EXCEPTION(
        isolate, isolate->CaptureAndSetSimpleStackTrace(error_object, caller));
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_MessageGetStartPosition) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSMessageObject, message, 0);
  return Smi::FromInt(message->start_position());
}


RUNTIME_FUNCTION(Runtime_MessageGetScript) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSMessageObject, message, 0);
  return message->script();
}


RUNTIME_FUNCTION(Runtime_FormatMessageString) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, arg0, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, arg1, 2);
  CONVERT_ARG_HANDLE_CHECKED(String, arg2, 3);
  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      MessageTemplate::FormatMessage(template_index, arg0, arg1, arg2));
  isolate->native_context()->IncrementErrorsThrown();
  return *result;
}


#define CALLSITE_GET(NAME, RETURN)                          \
  RUNTIME_FUNCTION(Runtime_CallSite##NAME##RT) {            \
    HandleScope scope(isolate);                             \
    DCHECK(args.length() == 1);                             \
    CONVERT_ARG_HANDLE_CHECKED(JSObject, call_site_obj, 0); \
    Handle<String> result;                                  \
    CallSite call_site(isolate, call_site_obj);             \
    RUNTIME_ASSERT(call_site.IsValid())                     \
    return RETURN(call_site.NAME(), isolate);               \
  }

static inline Object* ReturnDereferencedHandle(Handle<Object> obj,
                                               Isolate* isolate) {
  return *obj;
}


static inline Object* ReturnPositiveNumberOrNull(int value, Isolate* isolate) {
  if (value >= 0) return *isolate->factory()->NewNumberFromInt(value);
  return isolate->heap()->null_value();
}


static inline Object* ReturnBoolean(bool value, Isolate* isolate) {
  return isolate->heap()->ToBoolean(value);
}


CALLSITE_GET(GetFileName, ReturnDereferencedHandle)
CALLSITE_GET(GetFunctionName, ReturnDereferencedHandle)
CALLSITE_GET(GetScriptNameOrSourceUrl, ReturnDereferencedHandle)
CALLSITE_GET(GetMethodName, ReturnDereferencedHandle)
CALLSITE_GET(GetLineNumber, ReturnPositiveNumberOrNull)
CALLSITE_GET(GetColumnNumber, ReturnPositiveNumberOrNull)
CALLSITE_GET(IsNative, ReturnBoolean)
CALLSITE_GET(IsToplevel, ReturnBoolean)
CALLSITE_GET(IsEval, ReturnBoolean)
CALLSITE_GET(IsConstructor, ReturnBoolean)

#undef CALLSITE_GET


RUNTIME_FUNCTION(Runtime_IS_VAR) {
  UNREACHABLE();  // implemented as macro in the parser
  return NULL;
}


RUNTIME_FUNCTION(Runtime_IncrementStatsCounter) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(String, name, 0);

  if (FLAG_native_code_counters) {
    StatsCounter(isolate, name->ToCString().get()).Increment();
  }
  return isolate->heap()->undefined_value();
}


namespace {

bool ComputeLocation(Isolate* isolate, MessageLocation* target) {
  JavaScriptFrameIterator it(isolate);
  if (!it.done()) {
    JavaScriptFrame* frame = it.frame();
    JSFunction* fun = frame->function();
    Object* script = fun->shared()->script();
    if (script->IsScript() &&
        !(Script::cast(script)->source()->IsUndefined())) {
      Handle<Script> casted_script(Script::cast(script));
      // Compute the location from the function and the relocation info of the
      // baseline code. For optimized code this will use the deoptimization
      // information to get canonical location information.
      List<FrameSummary> frames(FLAG_max_inlining_levels + 1);
      it.frame()->Summarize(&frames);
      FrameSummary& summary = frames.last();
      int pos = summary.code()->SourcePosition(summary.pc());
      *target = MessageLocation(casted_script, pos, pos + 1, handle(fun));
      return true;
    }
  }
  return false;
}


Handle<String> RenderCallSite(Isolate* isolate, Handle<Object> object) {
  MessageLocation location;
  if (ComputeLocation(isolate, &location)) {
    Zone zone;
    base::SmartPointer<ParseInfo> info(
        location.function()->shared()->is_function()
            ? new ParseInfo(&zone, location.function())
            : new ParseInfo(&zone, location.script()));
    if (Parser::ParseStatic(info.get())) {
      CallPrinter printer(isolate, location.function()->shared()->IsBuiltin());
      const char* string = printer.Print(info->literal(), location.start_pos());
      if (strlen(string) > 0) {
        return isolate->factory()->NewStringFromAsciiChecked(string);
      }
    } else {
      isolate->clear_pending_exception();
    }
  }
  return Object::TypeOf(isolate, object);
}

}  // namespace


RUNTIME_FUNCTION(Runtime_ThrowCalledNonCallable) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  Handle<String> callsite = RenderCallSite(isolate, object);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kCalledNonCallable, callsite));
}


RUNTIME_FUNCTION(Runtime_ThrowConstructedNonConstructable) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  Handle<String> callsite = RenderCallSite(isolate, object);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kNotConstructor, callsite));
}


// ES6 section 7.3.17 CreateListFromArrayLike (obj)
RUNTIME_FUNCTION(Runtime_CreateListFromArrayLike) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  Handle<FixedArray> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Object::CreateListFromArrayLike(isolate, object, ElementTypes::kAll));
  return *result;
}


RUNTIME_FUNCTION(Runtime_IncrementUseCounter) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(counter, 0);
  isolate->CountUsage(static_cast<v8::Isolate::UseCounterFeature>(counter));
  return isolate->heap()->undefined_value();
}

}  // namespace internal
}  // namespace v8
