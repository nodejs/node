// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PREPARSE_DATA_H_
#define V8_PREPARSE_DATA_H_

#include "src/allocation.h"
#include "src/hashmap.h"
#include "src/preparse-data-format.h"
#include "src/utils-inl.h"

namespace v8 {
namespace internal {

class ScriptData;


// Abstract interface for preparse data recorder.
class ParserRecorder {
 public:
  ParserRecorder() { }
  virtual ~ParserRecorder() { }

  // Logs the scope and some details of a function literal in the source.
  virtual void LogFunction(int start,
                           int end,
                           int literals,
                           int properties,
                           StrictMode strict_mode) = 0;

  // Logs an error message and marks the log as containing an error.
  // Further logging will be ignored, and ExtractData will return a vector
  // representing the error only.
  virtual void LogMessage(int start,
                          int end,
                          const char* message,
                          const char* argument_opt,
                          bool is_reference_error) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(ParserRecorder);
};


class SingletonLogger : public ParserRecorder {
 public:
  SingletonLogger()
      : has_error_(false), start_(-1), end_(-1), is_reference_error_(false) {}
  virtual ~SingletonLogger() {}

  void Reset() { has_error_ = false; }

  virtual void LogFunction(int start,
                           int end,
                           int literals,
                           int properties,
                           StrictMode strict_mode) {
    DCHECK(!has_error_);
    start_ = start;
    end_ = end;
    literals_ = literals;
    properties_ = properties;
    strict_mode_ = strict_mode;
  }

  // Logs an error message and marks the log as containing an error.
  // Further logging will be ignored, and ExtractData will return a vector
  // representing the error only.
  virtual void LogMessage(int start,
                          int end,
                          const char* message,
                          const char* argument_opt,
                          bool is_reference_error) {
    if (has_error_) return;
    has_error_ = true;
    start_ = start;
    end_ = end;
    message_ = message;
    argument_opt_ = argument_opt;
    is_reference_error_ = is_reference_error;
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
  StrictMode strict_mode() const {
    DCHECK(!has_error_);
    return strict_mode_;
  }
  int is_reference_error() const { return is_reference_error_; }
  const char* message() {
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
  StrictMode strict_mode_;
  // For error messages.
  const char* message_;
  const char* argument_opt_;
  bool is_reference_error_;
};


class CompleteParserRecorder : public ParserRecorder {
 public:
  struct Key {
    bool is_one_byte;
    Vector<const byte> literal_bytes;
  };

  CompleteParserRecorder();
  virtual ~CompleteParserRecorder() {}

  virtual void LogFunction(int start,
                           int end,
                           int literals,
                           int properties,
                           StrictMode strict_mode) {
    function_store_.Add(start);
    function_store_.Add(end);
    function_store_.Add(literals);
    function_store_.Add(properties);
    function_store_.Add(strict_mode);
  }

  // Logs an error message and marks the log as containing an error.
  // Further logging will be ignored, and ExtractData will return a vector
  // representing the error only.
  virtual void LogMessage(int start,
                          int end,
                          const char* message,
                          const char* argument_opt,
                          bool is_reference_error_);
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

  // Write a non-negative number to the symbol store.
  void WriteNumber(int number);

  Collector<unsigned> function_store_;
  unsigned preamble_[PreparseDataConstants::kHeaderSize];

#ifdef DEBUG
  int prev_start_;
#endif
};


} }  // namespace v8::internal.

#endif  // V8_PREPARSE_DATA_H_
