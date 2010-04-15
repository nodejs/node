// Copyright 2006-2009 the V8 project authors. All rights reserved.
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

#include <stdlib.h>

#include "v8.h"

#include "token.h"
#include "scanner.h"
#include "utils.h"

#include "cctest.h"

namespace i = ::v8::internal;

TEST(KeywordMatcher) {
  struct KeywordToken {
    const char* keyword;
    i::Token::Value token;
  };

  static const KeywordToken keywords[] = {
#define KEYWORD(t, s, d) { s, i::Token::t },
#define IGNORE(t, s, d)  /* */
      TOKEN_LIST(IGNORE, KEYWORD, IGNORE)
#undef KEYWORD
      { NULL, i::Token::IDENTIFIER }
  };

  static const char* future_keywords[] = {
#define FUTURE(t, s, d) s,
      TOKEN_LIST(IGNORE, IGNORE, FUTURE)
#undef FUTURE
#undef IGNORE
      NULL
  };

  KeywordToken key_token;
  for (int i = 0; (key_token = keywords[i]).keyword != NULL; i++) {
    i::KeywordMatcher matcher;
    const char* keyword = key_token.keyword;
    int length = i::StrLength(keyword);
    for (int j = 0; j < length; j++) {
      if (key_token.token == i::Token::INSTANCEOF && j == 2) {
        // "in" is a prefix of "instanceof". It's the only keyword
        // that is a prefix of another.
        CHECK_EQ(i::Token::IN, matcher.token());
      } else {
        CHECK_EQ(i::Token::IDENTIFIER, matcher.token());
      }
      matcher.AddChar(keyword[j]);
    }
    CHECK_EQ(key_token.token, matcher.token());
    // Adding more characters will make keyword matching fail.
    matcher.AddChar('z');
    CHECK_EQ(i::Token::IDENTIFIER, matcher.token());
    // Adding a keyword later will not make it match again.
    matcher.AddChar('i');
    matcher.AddChar('f');
    CHECK_EQ(i::Token::IDENTIFIER, matcher.token());
  }

  // Future keywords are not recognized.
  const char* future_keyword;
  for (int i = 0; (future_keyword = future_keywords[i]) != NULL; i++) {
    i::KeywordMatcher matcher;
    int length = i::StrLength(future_keyword);
    for (int j = 0; j < length; j++) {
      matcher.AddChar(future_keyword[j]);
    }
    CHECK_EQ(i::Token::IDENTIFIER, matcher.token());
  }

  // Zero isn't ignored at first.
  i::KeywordMatcher bad_start;
  bad_start.AddChar(0);
  CHECK_EQ(i::Token::IDENTIFIER, bad_start.token());
  bad_start.AddChar('i');
  bad_start.AddChar('f');
  CHECK_EQ(i::Token::IDENTIFIER, bad_start.token());

  // Zero isn't ignored at end.
  i::KeywordMatcher bad_end;
  bad_end.AddChar('i');
  bad_end.AddChar('f');
  CHECK_EQ(i::Token::IF, bad_end.token());
  bad_end.AddChar(0);
  CHECK_EQ(i::Token::IDENTIFIER, bad_end.token());

  // Case isn't ignored.
  i::KeywordMatcher bad_case;
  bad_case.AddChar('i');
  bad_case.AddChar('F');
  CHECK_EQ(i::Token::IDENTIFIER, bad_case.token());

  // If we mark it as failure, continuing won't help.
  i::KeywordMatcher full_stop;
  full_stop.AddChar('i');
  CHECK_EQ(i::Token::IDENTIFIER, full_stop.token());
  full_stop.Fail();
  CHECK_EQ(i::Token::IDENTIFIER, full_stop.token());
  full_stop.AddChar('f');
  CHECK_EQ(i::Token::IDENTIFIER, full_stop.token());
}

