// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/pending-compilation-error-handler.h"

#include "src/ast/ast-value-factory.h"
#include "src/debug/debug.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/messages.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

Handle<String> PendingCompilationErrorHandler::MessageDetails::ArgumentString(
    Isolate* isolate) const {
  if (arg_ != nullptr) return arg_->string();
  if (char_arg_ != nullptr) {
    return isolate->factory()
        ->NewStringFromUtf8(CStrVector(char_arg_))
        .ToHandleChecked();
  }
  return isolate->factory()->undefined_string();
}

MessageLocation PendingCompilationErrorHandler::MessageDetails::GetLocation(
    Handle<Script> script) const {
  return MessageLocation(script, start_position_, end_position_);
}

void PendingCompilationErrorHandler::ReportMessageAt(
    int start_position, int end_position, MessageTemplate::Template message,
    const char* arg, ParseErrorType error_type) {
  if (has_pending_error_) return;
  has_pending_error_ = true;

  error_details_ =
      MessageDetails(start_position, end_position, message, nullptr, arg);
  error_type_ = error_type;
}

void PendingCompilationErrorHandler::ReportMessageAt(
    int start_position, int end_position, MessageTemplate::Template message,
    const AstRawString* arg, ParseErrorType error_type) {
  if (has_pending_error_) return;
  has_pending_error_ = true;

  error_details_ =
      MessageDetails(start_position, end_position, message, arg, nullptr);
  error_type_ = error_type;
}

void PendingCompilationErrorHandler::ReportWarningAt(
    int start_position, int end_position, MessageTemplate::Template message,
    const char* arg) {
  warning_messages_.emplace_front(
      MessageDetails(start_position, end_position, message, nullptr, arg));
}

void PendingCompilationErrorHandler::ReportWarnings(Isolate* isolate,
                                                    Handle<Script> script) {
  DCHECK(!has_pending_error());

  for (const MessageDetails& warning : warning_messages_) {
    MessageLocation location = warning.GetLocation(script);
    Handle<String> argument = warning.ArgumentString(isolate);
    Handle<JSMessageObject> message =
        MessageHandler::MakeMessageObject(isolate, warning.message(), &location,
                                          argument, Handle<FixedArray>::null());
    message->set_error_level(v8::Isolate::kMessageWarning);
    MessageHandler::ReportMessage(isolate, &location, message);
  }
}

void PendingCompilationErrorHandler::ReportErrors(
    Isolate* isolate, Handle<Script> script,
    AstValueFactory* ast_value_factory) {
  if (stack_overflow()) {
    isolate->StackOverflow();
  } else {
    DCHECK(has_pending_error());
    // Internalize ast values for throwing the pending error.
    ast_value_factory->Internalize(isolate);
    ThrowPendingError(isolate, script);
  }
}

void PendingCompilationErrorHandler::ThrowPendingError(Isolate* isolate,
                                                       Handle<Script> script) {
  if (!has_pending_error_) return;

  MessageLocation location = error_details_.GetLocation(script);
  Handle<String> argument = error_details_.ArgumentString(isolate);
  isolate->debug()->OnCompileError(script);

  Factory* factory = isolate->factory();
  Handle<Object> error;
  switch (error_type_) {
    case kReferenceError:
      error = factory->NewReferenceError(error_details_.message(), argument);
      break;
    case kSyntaxError:
      error = factory->NewSyntaxError(error_details_.message(), argument);
      break;
    default:
      UNREACHABLE();
      break;
  }

  if (!error->IsJSObject()) {
    isolate->Throw(*error, &location);
    return;
  }

  Handle<JSObject> jserror = Handle<JSObject>::cast(error);

  Handle<Name> key_start_pos = factory->error_start_pos_symbol();
  JSObject::SetProperty(isolate, jserror, key_start_pos,
                        handle(Smi::FromInt(location.start_pos()), isolate),
                        LanguageMode::kSloppy)
      .Check();

  Handle<Name> key_end_pos = factory->error_end_pos_symbol();
  JSObject::SetProperty(isolate, jserror, key_end_pos,
                        handle(Smi::FromInt(location.end_pos()), isolate),
                        LanguageMode::kSloppy)
      .Check();

  Handle<Name> key_script = factory->error_script_symbol();
  JSObject::SetProperty(isolate, jserror, key_script, script,
                        LanguageMode::kSloppy)
      .Check();

  isolate->Throw(*error, &location);
}

Handle<String> PendingCompilationErrorHandler::FormatErrorMessageForTest(
    Isolate* isolate) const {
  return MessageTemplate::FormatMessage(isolate, error_details_.message(),
                                        error_details_.ArgumentString(isolate));
}

}  // namespace internal
}  // namespace v8
