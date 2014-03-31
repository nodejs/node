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

#ifndef V8_PREPARSE_DATA_H_
#define V8_PREPARSE_DATA_H_

#include "allocation.h"
#include "hashmap.h"
#include "utils-inl.h"

namespace v8 {
namespace internal {


// Abstract interface for preparse data recorder.
class ParserRecorder {
 public:
  ParserRecorder() : should_log_symbols_(false) { }
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
                          const char* argument_opt) = 0;

  // Logs a symbol creation of a literal or identifier.
  bool ShouldLogSymbols() { return should_log_symbols_; }
  // The following functions are only callable on CompleteParserRecorder
  // and are guarded by calls to ShouldLogSymbols.
  virtual void LogOneByteSymbol(int start, Vector<const uint8_t> literal) {
    UNREACHABLE();
  }
  virtual void LogTwoByteSymbol(int start, Vector<const uint16_t> literal) {
    UNREACHABLE();
  }
  virtual void PauseRecording() { UNREACHABLE(); }
  virtual void ResumeRecording() { UNREACHABLE(); }

 protected:
  bool should_log_symbols_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ParserRecorder);
};


class SingletonLogger : public ParserRecorder {
 public:
  SingletonLogger() : has_error_(false), start_(-1), end_(-1) { }
  virtual ~SingletonLogger() { }

  void Reset() { has_error_ = false; }

  virtual void LogFunction(int start,
                           int end,
                           int literals,
                           int properties,
                           StrictMode strict_mode) {
    ASSERT(!has_error_);
    start_ = start;
    end_ = end;
    literals_ = literals;
    properties_ = properties;
    strict_mode_ = strict_mode;
  };

  // Logs an error message and marks the log as containing an error.
  // Further logging will be ignored, and ExtractData will return a vector
  // representing the error only.
  virtual void LogMessage(int start,
                          int end,
                          const char* message,
                          const char* argument_opt) {
    if (has_error_) return;
    has_error_ = true;
    start_ = start;
    end_ = end;
    message_ = message;
    argument_opt_ = argument_opt;
  }

  bool has_error() { return has_error_; }

  int start() { return start_; }
  int end() { return end_; }
  int literals() {
    ASSERT(!has_error_);
    return literals_;
  }
  int properties() {
    ASSERT(!has_error_);
    return properties_;
  }
  StrictMode strict_mode() {
    ASSERT(!has_error_);
    return strict_mode_;
  }
  const char* message() {
    ASSERT(has_error_);
    return message_;
  }
  const char* argument_opt() {
    ASSERT(has_error_);
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
                          const char* argument_opt);

  virtual void PauseRecording() {
    ASSERT(should_log_symbols_);
    should_log_symbols_ = false;
  }

  virtual void ResumeRecording() {
    ASSERT(!should_log_symbols_);
    should_log_symbols_ = !has_error();
  }

  virtual void LogOneByteSymbol(int start, Vector<const uint8_t> literal);
  virtual void LogTwoByteSymbol(int start, Vector<const uint16_t> literal);
  Vector<unsigned> ExtractData();

 private:
  bool has_error() {
    return static_cast<bool>(preamble_[PreparseDataConstants::kHasErrorOffset]);
  }

  void WriteString(Vector<const char> str);

  // For testing. Defined in test-parsing.cc.
  friend struct CompleteParserRecorderFriend;

  void LogSymbol(int start,
                 int hash,
                 bool is_one_byte,
                 Vector<const byte> literal);

  // Write a non-negative number to the symbol store.
  void WriteNumber(int number);

  Collector<unsigned> function_store_;
  unsigned preamble_[PreparseDataConstants::kHeaderSize];

#ifdef DEBUG
  int prev_start_;
#endif

  Collector<byte> literal_chars_;
  Collector<byte> symbol_store_;
  Collector<Key> symbol_keys_;
  HashMap string_table_;
  int symbol_id_;
};


} }  // namespace v8::internal.

#endif  // V8_PREPARSE_DATA_H_
