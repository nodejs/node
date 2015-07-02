// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PREPARSE_DATA_H_
#define V8_PREPARSE_DATA_H_

#include "src/allocation.h"
#include "src/hashmap.h"
#include "src/messages.h"
#include "src/preparse-data-format.h"

namespace v8 {
namespace internal {

class ScriptData {
 public:
  ScriptData(const byte* data, int length);
  ~ScriptData() {
    if (owns_data_) DeleteArray(data_);
  }

  const byte* data() const { return data_; }
  int length() const { return length_; }
  bool rejected() const { return rejected_; }

  void Reject() { rejected_ = true; }

  void AcquireDataOwnership() {
    DCHECK(!owns_data_);
    owns_data_ = true;
  }

  void ReleaseDataOwnership() {
    DCHECK(owns_data_);
    owns_data_ = false;
  }

 private:
  bool owns_data_ : 1;
  bool rejected_ : 1;
  const byte* data_;
  int length_;

  DISALLOW_COPY_AND_ASSIGN(ScriptData);
};

// Abstract interface for preparse data recorder.
class ParserRecorder {
 public:
  ParserRecorder() { }
  virtual ~ParserRecorder() { }

  // Logs the scope and some details of a function literal in the source.
  virtual void LogFunction(int start, int end, int literals, int properties,
                           LanguageMode language_mode, bool uses_super_property,
                           bool calls_eval) = 0;

  // Logs an error message and marks the log as containing an error.
  // Further logging will be ignored, and ExtractData will return a vector
  // representing the error only.
  virtual void LogMessage(int start, int end, MessageTemplate::Template message,
                          const char* argument_opt,
                          ParseErrorType error_type) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ParserRecorder);
};


class SingletonLogger : public ParserRecorder {
 public:
  SingletonLogger()
      : has_error_(false), start_(-1), end_(-1), error_type_(kSyntaxError) {}
  virtual ~SingletonLogger() {}

  void Reset() { has_error_ = false; }

  virtual void LogFunction(int start, int end, int literals, int properties,
                           LanguageMode language_mode, bool uses_super_property,
                           bool calls_eval) {
    DCHECK(!has_error_);
    start_ = start;
    end_ = end;
    literals_ = literals;
    properties_ = properties;
    language_mode_ = language_mode;
    uses_super_property_ = uses_super_property;
    calls_eval_ = calls_eval;
  }

  // Logs an error message and marks the log as containing an error.
  // Further logging will be ignored, and ExtractData will return a vector
  // representing the error only.
  virtual void LogMessage(int start, int end, MessageTemplate::Template message,
                          const char* argument_opt, ParseErrorType error_type) {
    if (has_error_) return;
    has_error_ = true;
    start_ = start;
    end_ = end;
    message_ = message;
    argument_opt_ = argument_opt;
    error_type_ = error_type;
  }

  bool has_error() const { return has_error_; }

  int start() const { return start_; }
  int end() const { return end_; }
  int literals() const {
    DCHECK(!has_error_);
    return literals_;
  }
  int properties() const {
    DCHECK(!has_error_);
    return properties_;
  }
  LanguageMode language_mode() const {
    DCHECK(!has_error_);
    return language_mode_;
  }
  bool uses_super_property() const {
    DCHECK(!has_error_);
    return uses_super_property_;
  }
  bool calls_eval() const {
    DCHECK(!has_error_);
    return calls_eval_;
  }
  ParseErrorType error_type() const {
    DCHECK(has_error_);
    return error_type_;
  }
  MessageTemplate::Template message() {
    DCHECK(has_error_);
    return message_;
  }
  const char* argument_opt() const {
    DCHECK(has_error_);
    return argument_opt_;
  }

 private:
  bool has_error_;
  int start_;
  int end_;
  // For function entries.
  int literals_;
  int properties_;
  LanguageMode language_mode_;
  bool uses_super_property_;
  bool calls_eval_;
  // For error messages.
  MessageTemplate::Template message_;
  const char* argument_opt_;
  ParseErrorType error_type_;
};


class CompleteParserRecorder : public ParserRecorder {
 public:
  struct Key {
    bool is_one_byte;
    Vector<const byte> literal_bytes;
  };

  CompleteParserRecorder();
  virtual ~CompleteParserRecorder() {}

  virtual void LogFunction(int start, int end, int literals, int properties,
                           LanguageMode language_mode, bool uses_super_property,
                           bool calls_eval) {
    function_store_.Add(start);
    function_store_.Add(end);
    function_store_.Add(literals);
    function_store_.Add(properties);
    function_store_.Add(language_mode);
    function_store_.Add(uses_super_property);
    function_store_.Add(calls_eval);
  }

  // Logs an error message and marks the log as containing an error.
  // Further logging will be ignored, and ExtractData will return a vector
  // representing the error only.
  virtual void LogMessage(int start, int end, MessageTemplate::Template message,
                          const char* argument_opt, ParseErrorType error_type);
  ScriptData* GetScriptData();

  bool HasError() {
    return static_cast<bool>(preamble_[PreparseDataConstants::kHasErrorOffset]);
  }
  Vector<unsigned> ErrorMessageData() {
    DCHECK(HasError());
    return function_store_.ToVector();
  }

 private:
  void WriteString(Vector<const char> str);

  Collector<unsigned> function_store_;
  unsigned preamble_[PreparseDataConstants::kHeaderSize];

#ifdef DEBUG
  int prev_start_;
#endif
};


} }  // namespace v8::internal.

#endif  // V8_PREPARSE_DATA_H_
