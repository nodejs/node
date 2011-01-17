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

// A buffered character stream based on a random access character
// source (ReadBlock can be called with pos_ pointing to any position,
// even positions before the current).
class BufferedUC16CharacterStream: public UC16CharacterStream {
 public:
  BufferedUC16CharacterStream();
  virtual ~BufferedUC16CharacterStream();

  virtual void PushBack(uc32 character);

 protected:
  static const unsigned kBufferSize = 512;
  static const unsigned kPushBackStepSize = 16;

  virtual unsigned SlowSeekForward(unsigned delta);
  virtual bool ReadBlock();
  virtual void SlowPushBack(uc16 character);

  virtual unsigned BufferSeekForward(unsigned delta) = 0;
  virtual unsigned FillBuffer(unsigned position, unsigned length) = 0;

  const uc16* pushback_limit_;
  uc16 buffer_[kBufferSize];
};


// Generic string stream.
class GenericStringUC16CharacterStream: public BufferedUC16CharacterStream {
 public:
  GenericStringUC16CharacterStream(Handle<String> data,
                                   unsigned start_position,
                                   unsigned end_position);
  virtual ~GenericStringUC16CharacterStream();

 protected:
  virtual unsigned BufferSeekForward(unsigned delta);
  virtual unsigned FillBuffer(unsigned position, unsigned length);

  Handle<String> string_;
  unsigned start_position_;
  unsigned length_;
};


// UC16 stream based on a literal UTF-8 string.
class Utf8ToUC16CharacterStream: public BufferedUC16CharacterStream {
 public:
  Utf8ToUC16CharacterStream(const byte* data, unsigned length);
  virtual ~Utf8ToUC16CharacterStream();

 protected:
  virtual unsigned BufferSeekForward(unsigned delta);
  virtual unsigned FillBuffer(unsigned char_position, unsigned length);
  void SetRawPosition(unsigned char_position);

  const byte* raw_data_;
  unsigned raw_data_length_;  // Measured in bytes, not characters.
  unsigned raw_data_pos_;
  // The character position of the character at raw_data[raw_data_pos_].
  // Not necessarily the same as pos_.
  unsigned raw_character_position_;
};


// UTF16 buffer to read characters from an external string.
class ExternalTwoByteStringUC16CharacterStream: public UC16CharacterStream {
 public:
  ExternalTwoByteStringUC16CharacterStream(Handle<ExternalTwoByteString> data,
                                           int start_position,
                                           int end_position);
  virtual ~ExternalTwoByteStringUC16CharacterStream();

  virtual void PushBack(uc32 character) {
    ASSERT(buffer_cursor_ > raw_data_);
    buffer_cursor_--;
    pos_--;
  }

 protected:
  virtual unsigned SlowSeekForward(unsigned delta) {
    // Fast case always handles seeking.
    return 0;
  }
  virtual bool ReadBlock() {
    // Entire string is read at start.
    return false;
  }
  Handle<ExternalTwoByteString> source_;
  const uc16* raw_data_;  // Pointer to the actual array of characters.
};


// ----------------------------------------------------------------------------
// V8JavaScriptScanner
// JavaScript scanner getting its input from either a V8 String or a unicode
// CharacterStream.

class V8JavaScriptScanner : public JavaScriptScanner {
 public:
  V8JavaScriptScanner();
  void Initialize(UC16CharacterStream* source);
};


class JsonScanner : public Scanner {
 public:
  JsonScanner();

  void Initialize(UC16CharacterStream* source);

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
  // carriage-return, newline and space.
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
};

} }  // namespace v8::internal

#endif  // V8_SCANNER_H_
