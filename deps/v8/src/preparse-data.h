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

// ----------------------------------------------------------------------------
// ParserRecorder - Logging of preparser data.

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
                           LanguageMode language_mode) = 0;

  // Logs a symbol creation of a literal or identifier.
  virtual void LogAsciiSymbol(int start, Vector<const char> literal) { }
  virtual void LogUtf16Symbol(int start, Vector<const uc16> literal) { }

  // Logs an error message and marks the log as containing an error.
  // Further logging will be ignored, and ExtractData will return a vector
  // representing the error only.
  virtual void LogMessage(int start,
                          int end,
                          const char* message,
                          const char* argument_opt) = 0;

  virtual int function_position() = 0;

  virtual int symbol_position() = 0;

  virtual int symbol_ids() = 0;

  virtual Vector<unsigned> ExtractData() = 0;

  virtual void PauseRecording() = 0;

  virtual void ResumeRecording() = 0;
};


// ----------------------------------------------------------------------------
// FunctionLoggingParserRecorder - Record only function entries

class FunctionLoggingParserRecorder : public ParserRecorder {
 public:
  FunctionLoggingParserRecorder();
  virtual ~FunctionLoggingParserRecorder() {}

  virtual void LogFunction(int start,
                           int end,
                           int literals,
                           int properties,
                           LanguageMode language_mode) {
    function_store_.Add(start);
    function_store_.Add(end);
    function_store_.Add(literals);
    function_store_.Add(properties);
    function_store_.Add(language_mode);
  }

  // Logs an error message and marks the log as containing an error.
  // Further logging will be ignored, and ExtractData will return a vector
  // representing the error only.
  virtual void LogMessage(int start,
                          int end,
                          const char* message,
                          const char* argument_opt);

  virtual int function_position() { return function_store_.size(); }


  virtual Vector<unsigned> ExtractData() = 0;

  virtual void PauseRecording() {
    pause_count_++;
    is_recording_ = false;
  }

  virtual void ResumeRecording() {
    ASSERT(pause_count_ > 0);
    if (--pause_count_ == 0) is_recording_ = !has_error();
  }

 protected:
  bool has_error() {
    return static_cast<bool>(preamble_[PreparseDataConstants::kHasErrorOffset]);
  }

  bool is_recording() {
    return is_recording_;
  }

  void WriteString(Vector<const char> str);

  Collector<unsigned> function_store_;
  unsigned preamble_[PreparseDataConstants::kHeaderSize];
  bool is_recording_;
  int pause_count_;

#ifdef DEBUG
  int prev_start_;
#endif
};


// ----------------------------------------------------------------------------
// PartialParserRecorder - Record only function entries

class PartialParserRecorder : public FunctionLoggingParserRecorder {
 public:
  PartialParserRecorder() : FunctionLoggingParserRecorder() { }
  virtual void LogAsciiSymbol(int start, Vector<const char> literal) { }
  virtual void LogUtf16Symbol(int start, Vector<const uc16> literal) { }
  virtual ~PartialParserRecorder() { }
  virtual Vector<unsigned> ExtractData();
  virtual int symbol_position() { return 0; }
  virtual int symbol_ids() { return 0; }
};


// ----------------------------------------------------------------------------
// CompleteParserRecorder -  Record both function entries and symbols.

class CompleteParserRecorder: public FunctionLoggingParserRecorder {
 public:
  CompleteParserRecorder();
  virtual ~CompleteParserRecorder() { }

  virtual void LogAsciiSymbol(int start, Vector<const char> literal) {
    if (!is_recording_) return;
    int hash = vector_hash(literal);
    LogSymbol(start, hash, true, Vector<const byte>::cast(literal));
  }

  virtual void LogUtf16Symbol(int start, Vector<const uc16> literal) {
    if (!is_recording_) return;
    int hash = vector_hash(literal);
    LogSymbol(start, hash, false, Vector<const byte>::cast(literal));
  }

  virtual Vector<unsigned> ExtractData();

  virtual int symbol_position() { return symbol_store_.size(); }
  virtual int symbol_ids() { return symbol_id_; }

 private:
  struct Key {
    bool is_ascii;
    Vector<const byte> literal_bytes;
  };

  virtual void LogSymbol(int start,
                         int hash,
                         bool is_ascii,
                         Vector<const byte> literal);

  template <typename Char>
  static int vector_hash(Vector<const Char> string) {
    int hash = 0;
    for (int i = 0; i < string.length(); i++) {
      int c = static_cast<int>(string[i]);
      hash += c;
      hash += (hash << 10);
      hash ^= (hash >> 6);
    }
    return hash;
  }

  static bool vector_compare(void* a, void* b) {
    Key* string1 = reinterpret_cast<Key*>(a);
    Key* string2 = reinterpret_cast<Key*>(b);
    if (string1->is_ascii != string2->is_ascii) return false;
    int length = string1->literal_bytes.length();
    if (string2->literal_bytes.length() != length) return false;
    return memcmp(string1->literal_bytes.start(),
                  string2->literal_bytes.start(), length) == 0;
  }

  // Write a non-negative number to the symbol store.
  void WriteNumber(int number);

  Collector<byte> literal_chars_;
  Collector<byte> symbol_store_;
  Collector<Key> symbol_keys_;
  HashMap symbol_table_;
  int symbol_id_;
};


} }  // namespace v8::internal.

#endif  // V8_PREPARSE_DATA_H_
