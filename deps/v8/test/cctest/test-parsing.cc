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
#include <stdio.h>
#include <string.h>

#include "v8.h"

#include "token.h"
#include "scanner.h"
#include "parser.h"
#include "utils.h"
#include "execution.h"
#include "scanner.h"
#include "preparser.h"
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


TEST(ScanHTMLEndComments) {
  // Regression test. See:
  //    http://code.google.com/p/chromium/issues/detail?id=53548
  // Tests that --> is correctly interpreted as comment-to-end-of-line if there
  // is only whitespace before it on the line, even after a multiline-comment
  // comment. This was not the case if it occurred before the first real token
  // in the input.
  const char* tests[] = {
      // Before first real token.
      "--> is eol-comment\nvar y = 37;\n",
      "\n --> is eol-comment\nvar y = 37;\n",
      "/* precomment */ --> is eol-comment\nvar y = 37;\n",
      "\n/* precomment */ --> is eol-comment\nvar y = 37;\n",
      // After first real token.
      "var x = 42;\n--> is eol-comment\nvar y = 37;\n",
      "var x = 42;\n/* precomment */ --> is eol-comment\nvar y = 37;\n",
      NULL
  };

  // Parser/Scanner needs a stack limit.
  int marker;
  i::StackGuard::SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  for (int i = 0; tests[i]; i++) {
    v8::ScriptData* data =
        v8::ScriptData::PreCompile(tests[i], i::StrLength(tests[i]));
    CHECK(data != NULL && !data->HasError());
    delete data;
  }
}


class ScriptResource : public v8::String::ExternalAsciiStringResource {
 public:
  ScriptResource(const char* data, size_t length)
      : data_(data), length_(length) { }

  const char* data() const { return data_; }
  size_t length() const { return length_; }

 private:
  const char* data_;
  size_t length_;
};


TEST(Preparsing) {
  v8::HandleScope handles;
  v8::Persistent<v8::Context> context = v8::Context::New();
  v8::Context::Scope context_scope(context);
  int marker;
  i::StackGuard::SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  // Source containing functions that might be lazily compiled  and all types
  // of symbols (string, propertyName, regexp).
  const char* source =
      "var x = 42;"
      "function foo(a) { return function nolazy(b) { return a + b; } }"
      "function bar(a) { if (a) return function lazy(b) { return b; } }"
      "var z = {'string': 'string literal', bareword: 'propertyName', "
      "         42: 'number literal', for: 'keyword as propertyName', "
      "         f\\u006fr: 'keyword propertyname with escape'};"
      "var v = /RegExp Literal/;"
      "var w = /RegExp Literal\\u0020With Escape/gin;"
      "var y = { get getter() { return 42; }, "
      "          set setter(v) { this.value = v; }};";
  int source_length = i::StrLength(source);
  const char* error_source = "var x = y z;";
  int error_source_length = i::StrLength(error_source);

  v8::ScriptData* preparse =
      v8::ScriptData::PreCompile(source, source_length);
  CHECK(!preparse->HasError());
  bool lazy_flag = i::FLAG_lazy;
  {
    i::FLAG_lazy = true;
    ScriptResource* resource = new ScriptResource(source, source_length);
    v8::Local<v8::String> script_source = v8::String::NewExternal(resource);
    v8::Script::Compile(script_source, NULL, preparse);
  }

  {
    i::FLAG_lazy = false;

    ScriptResource* resource = new ScriptResource(source, source_length);
    v8::Local<v8::String> script_source = v8::String::NewExternal(resource);
    v8::Script::New(script_source, NULL, preparse, v8::Local<v8::String>());
  }
  delete preparse;
  i::FLAG_lazy = lazy_flag;

  // Syntax error.
  v8::ScriptData* error_preparse =
      v8::ScriptData::PreCompile(error_source, error_source_length);
  CHECK(error_preparse->HasError());
  i::ScriptDataImpl *pre_impl =
      reinterpret_cast<i::ScriptDataImpl*>(error_preparse);
  i::Scanner::Location error_location =
      pre_impl->MessageLocation();
  // Error is at "z" in source, location 10..11.
  CHECK_EQ(10, error_location.beg_pos);
  CHECK_EQ(11, error_location.end_pos);
  // Should not crash.
  const char* message = pre_impl->BuildMessage();
  i::Vector<const char*> args = pre_impl->BuildArgs();
  CHECK_GT(strlen(message), 0);
}


TEST(StandAlonePreParser) {
  int marker;
  i::StackGuard::SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  const char* programs[] = {
      "{label: 42}",
      "var x = 42;",
      "function foo(x, y) { return x + y; }",
      "native function foo(); return %ArgleBargle(glop);",
      "var x = new new Function('this.x = 42');",
      NULL
  };

  for (int i = 0; programs[i]; i++) {
    const char* program = programs[i];
    unibrow::Utf8InputBuffer<256> stream(program, strlen(program));
    i::CompleteParserRecorder log;
    i::Scanner scanner;
    scanner.Initialize(i::Handle<i::String>::null(), &stream, i::JAVASCRIPT);
    v8::preparser::PreParser<i::Scanner, i::CompleteParserRecorder> preparser;
    bool result = preparser.PreParseProgram(&scanner, &log, true);
    CHECK(result);
    i::ScriptDataImpl data(log.ExtractData());
    CHECK(!data.has_error());
  }
}


TEST(RegressChromium62639) {
  int marker;
  i::StackGuard::SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  const char* program = "var x = 'something';\n"
                        "escape: function() {}";
  // Fails parsing expecting an identifier after "function".
  // Before fix, didn't check *ok after Expect(Token::Identifier, ok),
  // and then used the invalid currently scanned literal. This always
  // failed in debug mode, and sometimes crashed in release mode.

  unibrow::Utf8InputBuffer<256> stream(program, strlen(program));
  i::ScriptDataImpl* data =
      i::ParserApi::PreParse(i::Handle<i::String>::null(), &stream, NULL);
  CHECK(data->HasError());
  delete data;
}


TEST(Regress928) {
  // Preparsing didn't consider the catch clause of a try statement
  // as with-content, which made it assume that a function inside
  // the block could be lazily compiled, and an extra, unexpected,
  // entry was added to the data.
  int marker;
  i::StackGuard::SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  const char* program =
      "try { } catch (e) { var foo = function () { /* first */ } }"
      "var bar = function () { /* second */ }";

  unibrow::Utf8InputBuffer<256> stream(program, strlen(program));
  i::ScriptDataImpl* data =
      i::ParserApi::PartialPreParse(i::Handle<i::String>::null(),
                                    &stream, NULL);
  CHECK(!data->HasError());

  data->Initialize();

  int first_function = strstr(program, "function") - program;
  int first_lbrace = first_function + strlen("function () ");
  CHECK_EQ('{', program[first_lbrace]);
  i::FunctionEntry entry1 = data->GetFunctionEntry(first_lbrace);
  CHECK(!entry1.is_valid());

  int second_function = strstr(program + first_lbrace, "function") - program;
  int second_lbrace = second_function + strlen("function () ");
  CHECK_EQ('{', program[second_lbrace]);
  i::FunctionEntry entry2 = data->GetFunctionEntry(second_lbrace);
  CHECK(entry2.is_valid());
  CHECK_EQ('}', program[entry2.end_pos() - 1]);
  delete data;
}
