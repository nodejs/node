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

#include "scanner-base.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Keyword Matcher

KeywordMatcher::FirstState KeywordMatcher::first_states_[] = {
  { "break",  KEYWORD_PREFIX, Token::BREAK },
  { NULL,     C,              Token::ILLEGAL },
  { NULL,     D,              Token::ILLEGAL },
  { "else",   KEYWORD_PREFIX, Token::ELSE },
  { NULL,     F,              Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     I,              Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     N,              Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { "return", KEYWORD_PREFIX, Token::RETURN },
  { "switch", KEYWORD_PREFIX, Token::SWITCH },
  { NULL,     T,              Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     V,              Token::ILLEGAL },
  { NULL,     W,              Token::ILLEGAL }
};


void KeywordMatcher::Step(unibrow::uchar input) {
  switch (state_) {
    case INITIAL: {
      // matching the first character is the only state with significant fanout.
      // Match only lower-case letters in range 'b'..'w'.
      unsigned int offset = input - kFirstCharRangeMin;
      if (offset < kFirstCharRangeLength) {
        state_ = first_states_[offset].state;
        if (state_ == KEYWORD_PREFIX) {
          keyword_ = first_states_[offset].keyword;
          counter_ = 1;
          keyword_token_ = first_states_[offset].token;
        }
        return;
      }
      break;
    }
    case KEYWORD_PREFIX:
      if (static_cast<unibrow::uchar>(keyword_[counter_]) == input) {
        counter_++;
        if (keyword_[counter_] == '\0') {
          state_ = KEYWORD_MATCHED;
          token_ = keyword_token_;
        }
        return;
      }
      break;
    case KEYWORD_MATCHED:
      token_ = Token::IDENTIFIER;
      break;
    case C:
      if (MatchState(input, 'a', CA)) return;
      if (MatchState(input, 'o', CO)) return;
      break;
    case CA:
      if (MatchKeywordStart(input, "case", 2, Token::CASE)) return;
      if (MatchKeywordStart(input, "catch", 2, Token::CATCH)) return;
      break;
    case CO:
      if (MatchState(input, 'n', CON)) return;
      break;
    case CON:
      if (MatchKeywordStart(input, "const", 3, Token::CONST)) return;
      if (MatchKeywordStart(input, "continue", 3, Token::CONTINUE)) return;
      break;
    case D:
      if (MatchState(input, 'e', DE)) return;
      if (MatchKeyword(input, 'o', KEYWORD_MATCHED, Token::DO)) return;
      break;
    case DE:
      if (MatchKeywordStart(input, "debugger", 2, Token::DEBUGGER)) return;
      if (MatchKeywordStart(input, "default", 2, Token::DEFAULT)) return;
      if (MatchKeywordStart(input, "delete", 2, Token::DELETE)) return;
      break;
    case F:
      if (MatchKeywordStart(input, "false", 1, Token::FALSE_LITERAL)) return;
      if (MatchKeywordStart(input, "finally", 1, Token::FINALLY)) return;
      if (MatchKeywordStart(input, "for", 1, Token::FOR)) return;
      if (MatchKeywordStart(input, "function", 1, Token::FUNCTION)) return;
      break;
    case I:
      if (MatchKeyword(input, 'f', KEYWORD_MATCHED, Token::IF)) return;
      if (MatchKeyword(input, 'n', IN, Token::IN)) return;
      break;
    case IN:
      token_ = Token::IDENTIFIER;
      if (MatchKeywordStart(input, "instanceof", 2, Token::INSTANCEOF)) {
        return;
      }
      break;
    case N:
      if (MatchKeywordStart(input, "native", 1, Token::NATIVE)) return;
      if (MatchKeywordStart(input, "new", 1, Token::NEW)) return;
      if (MatchKeywordStart(input, "null", 1, Token::NULL_LITERAL)) return;
      break;
    case T:
      if (MatchState(input, 'h', TH)) return;
      if (MatchState(input, 'r', TR)) return;
      if (MatchKeywordStart(input, "typeof", 1, Token::TYPEOF)) return;
      break;
    case TH:
      if (MatchKeywordStart(input, "this", 2, Token::THIS)) return;
      if (MatchKeywordStart(input, "throw", 2, Token::THROW)) return;
      break;
    case TR:
      if (MatchKeywordStart(input, "true", 2, Token::TRUE_LITERAL)) return;
      if (MatchKeyword(input, 'y', KEYWORD_MATCHED, Token::TRY)) return;
      break;
    case V:
      if (MatchKeywordStart(input, "var", 1, Token::VAR)) return;
      if (MatchKeywordStart(input, "void", 1, Token::VOID)) return;
      break;
    case W:
      if (MatchKeywordStart(input, "while", 1, Token::WHILE)) return;
      if (MatchKeywordStart(input, "with", 1, Token::WITH)) return;
      break;
    case UNMATCHABLE:
      break;
  }
  // On fallthrough, it's a failure.
  state_ = UNMATCHABLE;
}

} }  // namespace v8::internal
