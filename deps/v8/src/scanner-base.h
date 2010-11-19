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

// Features shared by parsing and pre-parsing scanners.

#ifndef V8_SCANNER_BASE_H_
#define V8_SCANNER_BASE_H_

#include "globals.h"
#include "checks.h"
#include "allocation.h"
#include "token.h"
#include "unicode-inl.h"
#include "char-predicates.h"
#include "utils.h"

namespace v8 {
namespace internal {

// Interface through which the scanner reads characters from the input source.
class UTF16Buffer {
 public:
  UTF16Buffer();
  virtual ~UTF16Buffer() {}

  virtual void PushBack(uc32 ch) = 0;
  // Returns a value < 0 when the buffer end is reached.
  virtual uc32 Advance() = 0;
  virtual void SeekForward(int pos) = 0;

  int pos() const { return pos_; }

 protected:
  int pos_;  // Current position in the buffer.
  int end_;  // Position where scanning should stop (EOF).
};


class ScannerConstants : AllStatic {
 public:
  typedef unibrow::Utf8InputBuffer<1024> Utf8Decoder;

  static StaticResource<Utf8Decoder>* utf8_decoder() {
    return &utf8_decoder_;
  }

  static unibrow::Predicate<IdentifierStart, 128> kIsIdentifierStart;
  static unibrow::Predicate<IdentifierPart, 128> kIsIdentifierPart;
  static unibrow::Predicate<unibrow::LineTerminator, 128> kIsLineTerminator;
  static unibrow::Predicate<unibrow::WhiteSpace, 128> kIsWhiteSpace;

  static bool IsIdentifier(unibrow::CharacterStream* buffer);

 private:
  static StaticResource<Utf8Decoder> utf8_decoder_;
};


class KeywordMatcher {
//  Incrementally recognize keywords.
//
//  Recognized keywords:
//      break case catch const* continue debugger* default delete do else
//      finally false for function if in instanceof native* new null
//      return switch this throw true try typeof var void while with
//
//  *: Actually "future reserved keywords". These are the only ones we
//     recognize, the remaining are allowed as identifiers.
//     In ES5 strict mode, we should disallow all reserved keywords.
 public:
  KeywordMatcher()
      : state_(INITIAL),
        token_(Token::IDENTIFIER),
        keyword_(NULL),
        counter_(0),
        keyword_token_(Token::ILLEGAL) {}

  Token::Value token() { return token_; }

  inline void AddChar(unibrow::uchar input) {
    if (state_ != UNMATCHABLE) {
      Step(input);
    }
  }

  void Fail() {
    token_ = Token::IDENTIFIER;
    state_ = UNMATCHABLE;
  }

 private:
  enum State {
    UNMATCHABLE,
    INITIAL,
    KEYWORD_PREFIX,
    KEYWORD_MATCHED,
    C,
    CA,
    CO,
    CON,
    D,
    DE,
    F,
    I,
    IN,
    N,
    T,
    TH,
    TR,
    V,
    W
  };

  struct FirstState {
    const char* keyword;
    State state;
    Token::Value token;
  };

  // Range of possible first characters of a keyword.
  static const unsigned int kFirstCharRangeMin = 'b';
  static const unsigned int kFirstCharRangeMax = 'w';
  static const unsigned int kFirstCharRangeLength =
      kFirstCharRangeMax - kFirstCharRangeMin + 1;
  // State map for first keyword character range.
  static FirstState first_states_[kFirstCharRangeLength];

  // If input equals keyword's character at position, continue matching keyword
  // from that position.
  inline bool MatchKeywordStart(unibrow::uchar input,
                                const char* keyword,
                                int position,
                                Token::Value token_if_match) {
    if (input == static_cast<unibrow::uchar>(keyword[position])) {
      state_ = KEYWORD_PREFIX;
      this->keyword_ = keyword;
      this->counter_ = position + 1;
      this->keyword_token_ = token_if_match;
      return true;
    }
    return false;
  }

  // If input equals match character, transition to new state and return true.
  inline bool MatchState(unibrow::uchar input, char match, State new_state) {
    if (input == static_cast<unibrow::uchar>(match)) {
      state_ = new_state;
      return true;
    }
    return false;
  }

  inline bool MatchKeyword(unibrow::uchar input,
                           char match,
                           State new_state,
                           Token::Value keyword_token) {
    if (input != static_cast<unibrow::uchar>(match)) {
      return false;
    }
    state_ = new_state;
    token_ = keyword_token;
    return true;
  }

  void Step(unibrow::uchar input);

  // Current state.
  State state_;
  // Token for currently added characters.
  Token::Value token_;

  // Matching a specific keyword string (there is only one possible valid
  // keyword with the current prefix).
  const char* keyword_;
  int counter_;
  Token::Value keyword_token_;
};


} }  // namespace v8::internal

#endif  // V8_SCANNER_BASE_H_
