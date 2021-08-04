// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PENDING_COMPILATION_ERROR_HANDLER_H_
#define V8_PARSING_PENDING_COMPILATION_ERROR_HANDLER_H_

#include <forward_list>

#include "src/base/export-template.h"
#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/common/message-template.h"
#include "src/handles/handles.h"

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
      : has_pending_error_(false), stack_overflow_(false) {}

  PendingCompilationErrorHandler(const PendingCompilationErrorHandler&) =
      delete;
  PendingCompilationErrorHandler& operator=(
      const PendingCompilationErrorHandler&) = delete;

  void ReportMessageAt(int start_position, int end_position,
                       MessageTemplate message, const char* arg = nullptr);

  void ReportMessageAt(int start_position, int end_position,
                       MessageTemplate message, const AstRawString* arg);

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
  template <typename IsolateT>
  EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
  void PrepareErrors(IsolateT* isolate, AstValueFactory* ast_value_factory);
  V8_EXPORT_PRIVATE void ReportErrors(Isolate* isolate,
                                      Handle<Script> script) const;

  // Handle warnings detected during compilation.
  template <typename IsolateT>
  void PrepareWarnings(IsolateT* isolate);
  void ReportWarnings(Isolate* isolate, Handle<Script> script) const;

  V8_EXPORT_PRIVATE Handle<String> FormatErrorMessageForTest(Isolate* isolate);

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
          type_(kNone) {}
    MessageDetails(int start_position, int end_position,
                   MessageTemplate message, const AstRawString* arg)
        : start_position_(start_position),
          end_position_(end_position),
          message_(message),
          arg_(arg),
          type_(arg ? kAstRawString : kNone) {}
    MessageDetails(int start_position, int end_position,
                   MessageTemplate message, const char* char_arg)
        : start_position_(start_position),
          end_position_(end_position),
          message_(message),
          char_arg_(char_arg),
          type_(char_arg_ ? kConstCharString : kNone) {}

    Handle<String> ArgumentString(Isolate* isolate) const;
    MessageLocation GetLocation(Handle<Script> script) const;
    MessageTemplate message() const { return message_; }

    template <typename IsolateT>
    void Prepare(IsolateT* isolate);

   private:
    enum Type { kNone, kAstRawString, kConstCharString, kMainThreadHandle };

    void SetString(Handle<String> string, Isolate* isolate);
    void SetString(Handle<String> string, LocalIsolate* isolate);

    int start_position_;
    int end_position_;
    MessageTemplate message_;
    union {
      const AstRawString* arg_;
      const char* char_arg_;
      Handle<String> arg_handle_;
    };
    Type type_;
  };

  void ThrowPendingError(Isolate* isolate, Handle<Script> script) const;

  bool has_pending_error_;
  bool stack_overflow_;
  bool unidentifiable_error_ = false;

  MessageDetails error_details_;

  std::forward_list<MessageDetails> warning_messages_;
};

extern template void PendingCompilationErrorHandler::PrepareErrors(
    Isolate* isolate, AstValueFactory* ast_value_factory);
extern template void PendingCompilationErrorHandler::PrepareErrors(
    LocalIsolate* isolate, AstValueFactory* ast_value_factory);
extern template void PendingCompilationErrorHandler::PrepareWarnings(
    Isolate* isolate);
extern template void PendingCompilationErrorHandler::PrepareWarnings(
    LocalIsolate* isolate);

}  // namespace internal
}  // namespace v8
#endif  // V8_PARSING_PENDING_COMPILATION_ERROR_HANDLER_H_
