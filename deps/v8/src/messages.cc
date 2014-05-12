// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8.h"

#include "api.h"
#include "execution.h"
#include "messages.h"
#include "spaces-inl.h"

namespace v8 {
namespace internal {


// If no message listeners have been registered this one is called
// by default.
void MessageHandler::DefaultMessageReport(Isolate* isolate,
                                          const MessageLocation* loc,
                                          Handle<Object> message_obj) {
  SmartArrayPointer<char> str = GetLocalizedMessage(isolate, message_obj);
  if (loc == NULL) {
    PrintF("%s\n", str.get());
  } else {
    HandleScope scope(isolate);
    Handle<Object> data(loc->script()->name(), isolate);
    SmartArrayPointer<char> data_str;
    if (data->IsString())
      data_str = Handle<String>::cast(data)->ToCString(DISALLOW_NULLS);
    PrintF("%s:%i: %s\n", data_str.get() ? data_str.get() : "<unknown>",
           loc->start_pos(), str.get());
  }
}


Handle<JSMessageObject> MessageHandler::MakeMessageObject(
    Isolate* isolate,
    const char* type,
    MessageLocation* loc,
    Vector< Handle<Object> > args,
    Handle<JSArray> stack_frames) {
  Factory* factory = isolate->factory();
  Handle<String> type_handle = factory->InternalizeUtf8String(type);
  Handle<FixedArray> arguments_elements =
      factory->NewFixedArray(args.length());
  for (int i = 0; i < args.length(); i++) {
    arguments_elements->set(i, *args[i]);
  }
  Handle<JSArray> arguments_handle =
      factory->NewJSArrayWithElements(arguments_elements);

  int start = 0;
  int end = 0;
  Handle<Object> script_handle = factory->undefined_value();
  if (loc) {
    start = loc->start_pos();
    end = loc->end_pos();
    script_handle = Script::GetWrapper(loc->script());
  }

  Handle<Object> stack_frames_handle = stack_frames.is_null()
      ? Handle<Object>::cast(factory->undefined_value())
      : Handle<Object>::cast(stack_frames);

  Handle<JSMessageObject> message =
      factory->NewJSMessageObject(type_handle,
                                  arguments_handle,
                                  start,
                                  end,
                                  script_handle,
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
    exception_object = isolate->pending_exception();
  }
  Handle<Object> exception_handle(exception_object, isolate);

  Isolate::ExceptionScope exception_scope(isolate);
  isolate->clear_pending_exception();
  isolate->set_external_caught_exception(false);

  v8::Local<v8::Message> api_message_obj = v8::Utils::MessageToLocal(message);
  v8::Local<v8::Value> api_exception_obj = v8::Utils::ToLocal(exception_handle);

  v8::NeanderArray global_listeners(isolate->factory()->message_listeners());
  int global_length = global_listeners.length();
  if (global_length == 0) {
    DefaultMessageReport(isolate, loc, message);
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


Handle<String> MessageHandler::GetMessage(Isolate* isolate,
                                          Handle<Object> data) {
  Factory* factory = isolate->factory();
  Handle<String> fmt_str =
      factory->InternalizeOneByteString(STATIC_ASCII_VECTOR("FormatMessage"));
  Handle<JSFunction> fun = Handle<JSFunction>::cast(Object::GetProperty(
          isolate->js_builtins_object(), fmt_str).ToHandleChecked());
  Handle<JSMessageObject> message = Handle<JSMessageObject>::cast(data);
  Handle<Object> argv[] = { Handle<Object>(message->type(), isolate),
                            Handle<Object>(message->arguments(), isolate) };

  MaybeHandle<Object> maybe_result = Execution::TryCall(
      fun, isolate->js_builtins_object(), ARRAY_SIZE(argv), argv);
  Handle<Object> result;
  if (!maybe_result.ToHandle(&result) || !result->IsString()) {
    return factory->InternalizeOneByteString(STATIC_ASCII_VECTOR("<error>"));
  }
  Handle<String> result_string = Handle<String>::cast(result);
  // A string that has been obtained from JS code in this way is
  // likely to be a complicated ConsString of some sort.  We flatten it
  // here to improve the efficiency of converting it to a C string and
  // other operations that are likely to take place (see GetLocalizedMessage
  // for example).
  result_string = String::Flatten(result_string);
  return result_string;
}


SmartArrayPointer<char> MessageHandler::GetLocalizedMessage(
    Isolate* isolate,
    Handle<Object> data) {
  HandleScope scope(isolate);
  return GetMessage(isolate, data)->ToCString(DISALLOW_NULLS);
}


} }  // namespace v8::internal
