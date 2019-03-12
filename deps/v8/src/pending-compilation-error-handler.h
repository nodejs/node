// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PENDING_COMPILATION_ERROR_HANDLER_H_
#define V8_PENDING_COMPILATION_ERROR_HANDLER_H_

#include <forward_list>

#include "src/base/macros.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/message-template.h"

namespace v8 {
namespace internal {

class AstRawString;
class AstValueFactory;
class Isolate;
class Script;

// Helper class for handling pending compilation errors consistently in various
// compilation phases.
class PendingCompilationErrorHandler {
 public:
  PendingCompilationErrorHandler()
      : has_pending_error_(false),
        stack_overflow_(false),
        error_type_(kSyntaxError) {}

  void ReportMessageAt(int start_position, int end_position,
                       MessageTemplate message, const char* arg = nullptr,
                       ParseErrorType error_type = kSyntaxError);

  void ReportMessageAt(int start_position, int end_position,
                       MessageTemplate message, const AstRawString* arg,
                       ParseErrorType error_type = kSyntaxError);

  void ReportWarningAt(int start_position, int end_position,
                       MessageTemplate message, const char* arg = nullptr);

  bool stack_overflow() const { return stack_overflow_; }

  void set_stack_overflow() {
    has_pending_error_ = true;
    stack_overflow_ = true;
  }

  bool has_pending_error() const { return has_pending_error_; }
  bool has_pending_warnings() const { return !warning_messages_.empty(); }

  // Handle errors detected during parsing.
  void ReportErrors(Isolate* isolate, Handle<Script> script,
                    AstValueFactory* ast_value_factory);

  // Handle warnings detected during compilation.
  void ReportWarnings(Isolate* isolate, Handle<Script> script);

  Handle<String> FormatErrorMessageForTest(Isolate* isolate) const;

  void set_unidentifiable_error() {
    has_pending_error_ = true;
    unidentifiable_error_ = true;
  }
  void clear_unidentifiable_error() {
    has_pending_error_ = false;
    unidentifiable_error_ = false;
  }
  bool has_error_unidentifiable_by_preparser() const {
    return unidentifiable_error_;
  }

 private:
  class MessageDetails {
   public:
    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(MessageDetails);
    MessageDetails()
        : start_position_(-1),
          end_position_(-1),
          message_(MessageTemplate::kNone),
          arg_(nullptr),
          char_arg_(nullptr) {}
    MessageDetails(int start_position, int end_position,
                   MessageTemplate message, const AstRawString* arg,
                   const char* char_arg)
        : start_position_(start_position),
          end_position_(end_position),
          message_(message),
          arg_(arg),
          char_arg_(char_arg) {}

    Handle<String> ArgumentString(Isolate* isolate) const;
    MessageLocation GetLocation(Handle<Script> script) const;
    MessageTemplate message() const { return message_; }

   private:
    int start_position_;
    int end_position_;
    MessageTemplate message_;
    const AstRawString* arg_;
    const char* char_arg_;
  };

  void ThrowPendingError(Isolate* isolate, Handle<Script> script);

  bool has_pending_error_;
  bool stack_overflow_;
  bool unidentifiable_error_ = false;

  MessageDetails error_details_;
  ParseErrorType error_type_;

  std::forward_list<MessageDetails> warning_messages_;

  DISALLOW_COPY_AND_ASSIGN(PendingCompilationErrorHandler);
};

}  // namespace internal
}  // namespace v8
#endif  // V8_PENDING_COMPILATION_ERROR_HANDLER_H_
