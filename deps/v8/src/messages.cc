// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "api.h"
#include "execution.h"
#include "messages.h"
#include "spaces-inl.h"

namespace v8 {
namespace internal {


// If no message listeners have been registered this one is called
// by default.
void MessageHandler::DefaultMessageReport(const MessageLocation* loc,
                                          Handle<Object> message_obj) {
  SmartArrayPointer<char> str = GetLocalizedMessage(message_obj);
  if (loc == NULL) {
    PrintF("%s\n", *str);
  } else {
    HandleScope scope;
    Handle<Object> data(loc->script()->name());
    SmartArrayPointer<char> data_str;
    if (data->IsString())
      data_str = Handle<String>::cast(data)->ToCString(DISALLOW_NULLS);
    PrintF("%s:%i: %s\n", *data_str ? *data_str : "<unknown>",
           loc->start_pos(), *str);
  }
}


Handle<JSMessageObject> MessageHandler::MakeMessageObject(
    const char* type,
    MessageLocation* loc,
    Vector< Handle<Object> > args,
    Handle<String> stack_trace,
    Handle<JSArray> stack_frames) {
  Handle<String> type_handle = FACTORY->LookupAsciiSymbol(type);
  Handle<FixedArray> arguments_elements =
      FACTORY->NewFixedArray(args.length());
  for (int i = 0; i < args.length(); i++) {
    arguments_elements->set(i, *args[i]);
  }
  Handle<JSArray> arguments_handle =
      FACTORY->NewJSArrayWithElements(arguments_elements);

  int start = 0;
  int end = 0;
  Handle<Object> script_handle = FACTORY->undefined_value();
  if (loc) {
    start = loc->start_pos();
    end = loc->end_pos();
    script_handle = GetScriptWrapper(loc->script());
  }

  Handle<Object> stack_trace_handle = stack_trace.is_null()
      ? Handle<Object>::cast(FACTORY->undefined_value())
      : Handle<Object>::cast(stack_trace);

  Handle<Object> stack_frames_handle = stack_frames.is_null()
      ? Handle<Object>::cast(FACTORY->undefined_value())
      : Handle<Object>::cast(stack_frames);

  Handle<JSMessageObject> message =
      FACTORY->NewJSMessageObject(type_handle,
                                  arguments_handle,
                                  start,
                                  end,
                                  script_handle,
                                  stack_trace_handle,
                                  stack_frames_handle);

  return message;
}


void MessageHandler::ReportMessage(Isolate* isolate,
                                   MessageLocation* loc,
                                   Handle<Object> message) {
  // We are calling into embedder's code which can throw exceptions.
  // Thus we need to save current exception state, reset it to the clean one
  // and ignore scheduled exceptions callbacks can throw.

  // We pass the exception object into the message handler callback though.
  Object* exception_object = isolate->heap()->undefined_value();
  if (isolate->has_pending_exception()) {
    isolate->pending_exception()->ToObject(&exception_object);
  }
  Handle<Object> exception_handle(exception_object);

  Isolate::ExceptionScope exception_scope(isolate);
  isolate->clear_pending_exception();
  isolate->set_external_caught_exception(false);

  v8::Local<v8::Message> api_message_obj = v8::Utils::MessageToLocal(message);
  v8::Local<v8::Value> api_exception_obj = v8::Utils::ToLocal(exception_handle);

  v8::NeanderArray global_listeners(FACTORY->message_listeners());
  int global_length = global_listeners.length();
  if (global_length == 0) {
    DefaultMessageReport(loc, message);
    if (isolate->has_scheduled_exception()) {
      isolate->clear_scheduled_exception();
    }
  } else {
    for (int i = 0; i < global_length; i++) {
      HandleScope scope(isolate);
      if (global_listeners.get(i)->IsUndefined()) continue;
      v8::NeanderObject listener(JSObject::cast(global_listeners.get(i)));
      Handle<Foreign> callback_obj(Foreign::cast(listener.get(0)));
      v8::MessageCallback callback =
          FUNCTION_CAST<v8::MessageCallback>(callback_obj->foreign_address());
      Handle<Object> callback_data(listener.get(1), isolate);
      {
        // Do not allow exceptions to propagate.
        v8::TryCatch try_catch;
        callback(api_message_obj, callback_data->IsUndefined()
                                      ? api_exception_obj
                                      : v8::Utils::ToLocal(callback_data));
      }
      if (isolate->has_scheduled_exception()) {
        isolate->clear_scheduled_exception();
      }
    }
  }
}


Handle<String> MessageHandler::GetMessage(Handle<Object> data) {
  Handle<String> fmt_str = FACTORY->LookupAsciiSymbol("FormatMessage");
  Handle<JSFunction> fun =
      Handle<JSFunction>(
          JSFunction::cast(
              Isolate::Current()->js_builtins_object()->
              GetPropertyNoExceptionThrown(*fmt_str)));
  Handle<Object> argv[] = { data };

  bool caught_exception;
  Handle<Object> result =
      Execution::TryCall(fun,
                         Isolate::Current()->js_builtins_object(),
                         ARRAY_SIZE(argv),
                         argv,
                         &caught_exception);

  if (caught_exception || !result->IsString()) {
    return FACTORY->LookupAsciiSymbol("<error>");
  }
  Handle<String> result_string = Handle<String>::cast(result);
  // A string that has been obtained from JS code in this way is
  // likely to be a complicated ConsString of some sort.  We flatten it
  // here to improve the efficiency of converting it to a C string and
  // other operations that are likely to take place (see GetLocalizedMessage
  // for example).
  FlattenString(result_string);
  return result_string;
}


SmartArrayPointer<char> MessageHandler::GetLocalizedMessage(
    Handle<Object> data) {
  HandleScope scope;
  return GetMessage(data)->ToCString(DISALLOW_NULLS);
}


} }  // namespace v8::internal
