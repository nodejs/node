// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/api.h"
#include "src/execution.h"
#include "src/heap/spaces-inl.h"
#include "src/messages.h"
#include "src/string-builder.h"

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
      factory->InternalizeOneByteString(STATIC_CHAR_VECTOR("$formatMessage"));
  Handle<JSFunction> fun = Handle<JSFunction>::cast(Object::GetProperty(
          isolate->js_builtins_object(), fmt_str).ToHandleChecked());
  Handle<JSMessageObject> message = Handle<JSMessageObject>::cast(data);
  Handle<Object> argv[] = { Handle<Object>(message->type(), isolate),
                            Handle<Object>(message->arguments(), isolate) };

  MaybeHandle<Object> maybe_result = Execution::TryCall(
      fun, isolate->js_builtins_object(), arraysize(argv), argv);
  Handle<Object> result;
  if (!maybe_result.ToHandle(&result) || !result->IsString()) {
    return factory->InternalizeOneByteString(STATIC_CHAR_VECTOR("<error>"));
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


Handle<Object> CallSite::GetFileName(Isolate* isolate) {
  Handle<Object> script(fun_->shared()->script(), isolate);
  if (script->IsScript()) {
    return Handle<Object>(Handle<Script>::cast(script)->name(), isolate);
  }
  return isolate->factory()->null_value();
}


Handle<Object> CallSite::GetFunctionName(Isolate* isolate) {
  Handle<String> result = JSFunction::GetDebugName(fun_);
  if (result->length() != 0) return result;
  Handle<Object> script(fun_->shared()->script(), isolate);
  if (script->IsScript() &&
      Handle<Script>::cast(script)->compilation_type() ==
          Script::COMPILATION_TYPE_EVAL) {
    return isolate->factory()->eval_string();
  }
  return isolate->factory()->null_value();
}


Handle<Object> CallSite::GetScriptNameOrSourceUrl(Isolate* isolate) {
  Handle<Object> script_obj(fun_->shared()->script(), isolate);
  if (script_obj->IsScript()) {
    Handle<Script> script = Handle<Script>::cast(script_obj);
    Object* source_url = script->source_url();
    if (source_url->IsString()) return Handle<Object>(source_url, isolate);
    return Handle<Object>(script->name(), isolate);
  }
  return isolate->factory()->null_value();
}


bool CheckMethodName(Handle<JSObject> obj, Handle<Name> name,
                     Handle<JSFunction> fun,
                     LookupIterator::Configuration config) {
  LookupIterator iter(obj, name, config);
  if (iter.state() == LookupIterator::DATA) {
    return iter.GetDataValue().is_identical_to(fun);
  } else if (iter.state() == LookupIterator::ACCESSOR) {
    Handle<Object> accessors = iter.GetAccessors();
    if (accessors->IsAccessorPair()) {
      Handle<AccessorPair> pair = Handle<AccessorPair>::cast(accessors);
      return pair->getter() == *fun || pair->setter() == *fun;
    }
  }
  return false;
}


Handle<Object> CallSite::GetMethodName(Isolate* isolate) {
  MaybeHandle<JSReceiver> maybe = Object::ToObject(isolate, receiver_);
  Handle<JSReceiver> receiver;
  if (!maybe.ToHandle(&receiver) || !receiver->IsJSObject()) {
    return isolate->factory()->null_value();
  }

  Handle<JSObject> obj = Handle<JSObject>::cast(receiver);
  Handle<Object> function_name(fun_->shared()->name(), isolate);
  if (function_name->IsName()) {
    Handle<Name> name = Handle<Name>::cast(function_name);
    if (CheckMethodName(obj, name, fun_,
                        LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR))
      return name;
  }

  HandleScope outer_scope(isolate);
  Handle<Object> result;
  for (PrototypeIterator iter(isolate, obj,
                              PrototypeIterator::START_AT_RECEIVER);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<Object> current = PrototypeIterator::GetCurrent(iter);
    if (!current->IsJSObject()) break;
    Handle<JSObject> current_obj = Handle<JSObject>::cast(current);
    if (current_obj->IsAccessCheckNeeded()) break;
    Handle<FixedArray> keys = JSObject::GetEnumPropertyKeys(current_obj, false);
    for (int i = 0; i < keys->length(); i++) {
      HandleScope inner_scope(isolate);
      if (!keys->get(i)->IsName()) continue;
      Handle<Name> name_key(Name::cast(keys->get(i)), isolate);
      if (!CheckMethodName(current_obj, name_key, fun_,
                           LookupIterator::OWN_SKIP_INTERCEPTOR))
        continue;
      // Return null in case of duplicates to avoid confusion.
      if (!result.is_null()) return isolate->factory()->null_value();
      result = inner_scope.CloseAndEscape(name_key);
    }
  }

  if (!result.is_null()) return outer_scope.CloseAndEscape(result);
  return isolate->factory()->null_value();
}


int CallSite::GetLineNumber(Isolate* isolate) {
  if (pos_ >= 0) {
    Handle<Object> script_obj(fun_->shared()->script(), isolate);
    if (script_obj->IsScript()) {
      Handle<Script> script = Handle<Script>::cast(script_obj);
      return Script::GetLineNumber(script, pos_) + 1;
    }
  }
  return -1;
}


int CallSite::GetColumnNumber(Isolate* isolate) {
  if (pos_ >= 0) {
    Handle<Object> script_obj(fun_->shared()->script(), isolate);
    if (script_obj->IsScript()) {
      Handle<Script> script = Handle<Script>::cast(script_obj);
      return Script::GetColumnNumber(script, pos_) + 1;
    }
  }
  return -1;
}


bool CallSite::IsNative(Isolate* isolate) {
  Handle<Object> script(fun_->shared()->script(), isolate);
  return script->IsScript() &&
         Handle<Script>::cast(script)->type()->value() == Script::TYPE_NATIVE;
}


bool CallSite::IsToplevel(Isolate* isolate) {
  return receiver_->IsJSGlobalProxy() || receiver_->IsNull() ||
         receiver_->IsUndefined();
}


bool CallSite::IsEval(Isolate* isolate) {
  Handle<Object> script(fun_->shared()->script(), isolate);
  return script->IsScript() &&
         Handle<Script>::cast(script)->compilation_type() ==
             Script::COMPILATION_TYPE_EVAL;
}


bool CallSite::IsConstructor(Isolate* isolate) {
  if (!receiver_->IsJSObject()) return false;
  Handle<Object> constructor =
      JSObject::GetDataProperty(Handle<JSObject>::cast(receiver_),
                                isolate->factory()->constructor_string());
  return constructor.is_identical_to(fun_);
}


MaybeHandle<String> MessageTemplate::FormatMessage(int template_index,
                                                   Handle<String> arg0,
                                                   Handle<String> arg1,
                                                   Handle<String> arg2) {
  const char* template_string;
  switch (template_index) {
#define CASE(NAME, STRING)    \
  case k##NAME:               \
    template_string = STRING; \
    break;
    MESSAGE_TEMPLATES(CASE)
#undef CASE
    case kLastMessage:
    default:
      UNREACHABLE();
      template_string = "";
      break;
  }

  Isolate* isolate = arg0->GetIsolate();
  IncrementalStringBuilder builder(isolate);

  unsigned int i = 0;
  Handle<String> args[] = {arg0, arg1, arg2};
  for (const char* c = template_string; *c != '\0'; c++) {
    if (*c == '%') {
      DCHECK(i < arraysize(args));
      builder.AppendString(args[i++]);
    } else {
      builder.AppendCharacter(*c);
    }
  }

  return builder.Finish();
}
} }  // namespace v8::internal
