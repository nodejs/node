// Copyright 2010 the V8 project authors. All rights reserved.
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

#ifndef V8_SCANNER_H_
#define V8_SCANNER_H_

#include "token.h"
#include "char-predicates-inl.h"
#include "scanner-base.h"

namespace v8 {
namespace internal {

// UTF16 buffer to read characters from a character stream.
class CharacterStreamUTF16Buffer: public UTF16Buffer {
 public:
  CharacterStreamUTF16Buffer();
  virtual ~CharacterStreamUTF16Buffer() {}
  void Initialize(Handle<String> data,
                  unibrow::CharacterStream* stream,
                  int start_position,
                  int end_position);
  virtual void PushBack(uc32 ch);
  virtual uc32 Advance();
  virtual void SeekForward(int pos);

 private:
  List<uc32> pushback_buffer_;
  uc32 last_;
  unibrow::CharacterStream* stream_;

  List<uc32>* pushback_buffer() { return &pushback_buffer_; }
};


// UTF16 buffer to read characters from an external string.
template <typename StringType, typename CharType>
class ExternalStringUTF16Buffer: public UTF16Buffer {
 public:
  ExternalStringUTF16Buffer();
  virtual ~ExternalStringUTF16Buffer() {}
  void Initialize(Handle<StringType> data,
                  int start_position,
                  int end_position);
  virtual void PushBack(uc32 ch);
  virtual uc32 Advance();
  virtual void SeekForward(int pos);

 private:
  const CharType* raw_data_;  // Pointer to the actual array of characters.
};


// Initializes a UTF16Buffer as input stream, using one of a number
// of strategies depending on the available character sources.
class StreamInitializer {
 public:
  UTF16Buffer* Init(Handle<String> source,
                    unibrow::CharacterStream* stream,
                    int start_position,
                    int end_position);
 private:
  // Different UTF16 buffers used to pull characters from. Based on input one of
  // these will be initialized as the actual data source.
  CharacterStreamUTF16Buffer char_stream_buffer_;
  ExternalStringUTF16Buffer<ExternalTwoByteString, uint16_t>
      two_byte_string_buffer_;
  ExternalStringUTF16Buffer<ExternalAsciiString, char> ascii_string_buffer_;

  // Used to convert the source string into a character stream when a stream
  // is not passed to the scanner.
  SafeStringInputBuffer safe_string_input_buffer_;
};

// ----------------------------------------------------------------------------
// V8JavaScriptScanner
// JavaScript scanner getting its input from either a V8 String or a unicode
// CharacterStream.

class V8JavaScriptScanner : public JavaScriptScanner {
 public:
  V8JavaScriptScanner() {}

  Token::Value NextCheckStack();

  // Initialize the Scanner to scan source.
  void Initialize(Handle<String> source, int literal_flags = kAllLiterals);
  void Initialize(Handle<String> source,
                  unibrow::CharacterStream* stream,
                  int literal_flags = kAllLiterals);
  void Initialize(Handle<String> source,
                  int start_position, int end_position,
                  int literal_flags = kAllLiterals);

 protected:
  StreamInitializer stream_initializer_;
};


class JsonScanner : public Scanner {
 public:
  JsonScanner();

  // Initialize the Scanner to scan source.
  void Initialize(Handle<String> source);

  // Returns the next token.
  Token::Value Next();

 protected:
  // Skip past JSON whitespace (only space, tab, newline and carrige-return).
  bool SkipJsonWhiteSpace();

  // Scan a single JSON token. The JSON lexical grammar is specified in the
  // ECMAScript 5 standard, section 15.12.1.1.
  // Recognizes all of the single-character tokens directly, or calls a function
  // to scan a number, string or identifier literal.
  // The only allowed whitespace characters between tokens are tab,
  // carrige-return, newline and space.
  void ScanJson();

  // A JSON number (production JSONNumber) is a subset of the valid JavaScript
  // decimal number literals.
  // It includes an optional minus sign, must have at least one
  // digit before and after a decimal point, may not have prefixed zeros (unless
  // the integer part is zero), and may include an exponent part (e.g., "e-10").
  // Hexadecimal and octal numbers are not allowed.
  Token::Value ScanJsonNumber();

  // A JSON string (production JSONString) is subset of valid JavaScript string
  // literals. The string must only be double-quoted (not single-quoted), and
  // the only allowed backslash-escapes are ", /, \, b, f, n, r, t and
  // four-digit hex escapes (uXXXX). Any other use of backslashes is invalid.
  Token::Value ScanJsonString();

  // Used to recognizes one of the literals "true", "false", or "null". These
  // are the only valid JSON identifiers (productions JSONBooleanLiteral,
  // JSONNullLiteral).
  Token::Value ScanJsonIdentifier(const char* text, Token::Value token);

  StreamInitializer stream_initializer_;
};


// ExternalStringUTF16Buffer
template <typename StringType, typename CharType>
ExternalStringUTF16Buffer<StringType, CharType>::ExternalStringUTF16Buffer()
    : raw_data_(NULL) { }


template <typename StringType, typename CharType>
void ExternalStringUTF16Buffer<StringType, CharType>::Initialize(
     Handle<StringType> data,
     int start_position,
     int end_position) {
  ASSERT(!data.is_null());
  raw_data_ = data->resource()->data();

  ASSERT(end_position <= data->length());
  if (start_position > 0) {
    SeekForward(start_position);
  }
  end_ =
      end_position != kNoEndPosition ? end_position : data->length();
}


template <typename StringType, typename CharType>
uc32 ExternalStringUTF16Buffer<StringType, CharType>::Advance() {
  if (pos_ < end_) {
    return raw_data_[pos_++];
  } else {
    // note: currently the following increment is necessary to avoid a
    // test-parser problem!
    pos_++;
    return static_cast<uc32>(-1);
  }
}


template <typename StringType, typename CharType>
void ExternalStringUTF16Buffer<StringType, CharType>::PushBack(uc32 ch) {
  pos_--;
  ASSERT(pos_ >= Scanner::kCharacterLookaheadBufferSize);
  ASSERT(raw_data_[pos_ - Scanner::kCharacterLookaheadBufferSize] == ch);
}


template <typename StringType, typename CharType>
void ExternalStringUTF16Buffer<StringType, CharType>::SeekForward(int pos) {
  pos_ = pos;
}

} }  // namespace v8::internal

#endif  // V8_SCANNER_H_
