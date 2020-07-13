// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/pending-compilation-error-handler.h"

#include "src/ast/ast-value-factory.h"
#include "src/base/logging.h"
#include "src/debug/debug.h"
#include "src/execution/isolate.h"
#include "src/execution/messages.h"
#include "src/execution/off-thread-isolate.h"
#include "src/handles/handles.h"
#include "src/heap/off-thread-factory-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

void PendingCompilationErrorHandler::MessageDetails::SetString(
    Handle<String> string, Isolate* isolate) {
  DCHECK_NE(type_, kMainThreadHandle);
  DCHECK_NE(type_, kOffThreadTransferHandle);
  type_ = kMainThreadHandle;
  arg_handle_ = string;
}

void PendingCompilationErrorHandler::MessageDetails::SetString(
    Handle<String> string, OffThreadIsolate* isolate) {
  DCHECK_NE(type_, kMainThreadHandle);
  DCHECK_NE(type_, kOffThreadTransferHandle);
  type_ = kOffThreadTransferHandle;
  arg_transfer_handle_ = isolate->TransferHandle(string);
}

template <typename LocalIsolate>
void PendingCompilationErrorHandler::MessageDetails::Prepare(
    LocalIsolate* isolate) {
  switch (type_) {
    case kAstRawString:
      return SetString(arg_->string(), isolate);
    case kNone:
    case kConstCharString:
      // We can delay allocation until ArgumentString(isolate).
      // TODO(leszeks): We don't actually have to transfer this string, since
      // it's a root.
      return;
    case kMainThreadHandle:
    case kOffThreadTransferHandle:
      UNREACHABLE();
  }
}

Handle<String> PendingCompilationErrorHandler::MessageDetails::ArgumentString(
    Isolate* isolate) const {
  switch (type_) {
    case kMainThreadHandle:
      return arg_handle_;
    case kOffThreadTransferHandle:
      return arg_transfer_handle_.ToHandle();
    case kNone:
      return isolate->factory()->undefined_string();
    case kConstCharString:
      return isolate->factory()
          ->NewStringFromUtf8(CStrVector(char_arg_), AllocationType::kOld)
          .ToHandleChecked();
    case kAstRawString:
      UNREACHABLE();
  }
}

MessageLocation PendingCompilationErrorHandler::MessageDetails::GetLocation(
    Handle<Script> script) const {
  return MessageLocation(script, start_position_, end_position_);
}

void PendingCompilationErrorHandler::ReportMessageAt(int start_position,
                                                     int end_position,
                                                     MessageTemplate message,
                                                     const char* arg) {
  if (has_pending_error_) return;
  has_pending_error_ = true;

  error_details_ = MessageDetails(start_position, end_position, message, arg);
}

void PendingCompilationErrorHandler::ReportMessageAt(int start_position,
                                                     int end_position,
                                                     MessageTemplate message,
                                                     const AstRawString* arg) {
  if (has_pending_error_) return;
  has_pending_error_ = true;

  error_details_ = MessageDetails(start_position, end_position, message, arg);
}

void PendingCompilationErrorHandler::ReportWarningAt(int start_position,
                                                     int end_position,
                                                     MessageTemplate message,
                                                     const char* arg) {
  warning_messages_.emplace_front(
      MessageDetails(start_position, end_position, message, arg));
}

template <typename LocalIsolate>
void PendingCompilationErrorHandler::PrepareWarnings(LocalIsolate* isolate) {
  DCHECK(!has_pending_error());

  for (MessageDetails& warning : warning_messages_) {
    warning.Prepare(isolate);
  }
}
template void PendingCompilationErrorHandler::PrepareWarnings(Isolate* isolate);
template void PendingCompilationErrorHandler::PrepareWarnings(
    OffThreadIsolate* isolate);

void PendingCompilationErrorHandler::ReportWarnings(
    Isolate* isolate, Handle<Script> script) const {
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

template <typename LocalIsolate>
void PendingCompilationErrorHandler::PrepareErrors(
    LocalIsolate* isolate, AstValueFactory* ast_value_factory) {
  if (stack_overflow()) return;

  DCHECK(has_pending_error());
  // Internalize ast values for throwing the pending error.
  ast_value_factory->Internalize(isolate);
  error_details_.Prepare(isolate);
}
template void PendingCompilationErrorHandler::PrepareErrors(
    Isolate* isolate, AstValueFactory* ast_value_factory);
template void PendingCompilationErrorHandler::PrepareErrors(
    OffThreadIsolate* isolate, AstValueFactory* ast_value_factory);

void PendingCompilationErrorHandler::ReportErrors(Isolate* isolate,
                                                  Handle<Script> script) const {
  if (stack_overflow()) {
    isolate->StackOverflow();
  } else {
    DCHECK(has_pending_error());
    ThrowPendingError(isolate, script);
  }
}

void PendingCompilationErrorHandler::ThrowPendingError(
    Isolate* isolate, Handle<Script> script) const {
  if (!has_pending_error_) return;

  MessageLocation location = error_details_.GetLocation(script);
  Handle<String> argument = error_details_.ArgumentString(isolate);
  isolate->debug()->OnCompileError(script);

  Factory* factory = isolate->factory();
  Handle<JSObject> error =
      factory->NewSyntaxError(error_details_.message(), argument);
  isolate->ThrowAt(error, &location);
}

Handle<String> PendingCompilationErrorHandler::FormatErrorMessageForTest(
    Isolate* isolate) {
  error_details_.Prepare(isolate);
  return MessageFormatter::Format(isolate, error_details_.message(),
                                  error_details_.ArgumentString(isolate));
}

}  // namespace internal
}  // namespace v8
