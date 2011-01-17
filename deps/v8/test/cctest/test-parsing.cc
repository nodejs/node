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

  uintptr_t stack_limit = i::StackGuard::real_climit();
  for (int i = 0; programs[i]; i++) {
    const char* program = programs[i];
    i::Utf8ToUC16CharacterStream stream(
        reinterpret_cast<const i::byte*>(program),
        static_cast<unsigned>(strlen(program)));
    i::CompleteParserRecorder log;
    i::V8JavaScriptScanner scanner;
    scanner.Initialize(&stream);

    v8::preparser::PreParser::PreParseResult result =
        v8::preparser::PreParser::PreParseProgram(&scanner,
                                                  &log,
                                                  true,
                                                  stack_limit);
    CHECK_EQ(v8::preparser::PreParser::kPreParseSuccess, result);
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

  i::Utf8ToUC16CharacterStream stream(reinterpret_cast<const i::byte*>(program),
                                      static_cast<unsigned>(strlen(program)));
  i::ScriptDataImpl* data =
      i::ParserApi::PreParse(&stream, NULL);
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

  i::Utf8ToUC16CharacterStream stream(reinterpret_cast<const i::byte*>(program),
                                      static_cast<unsigned>(strlen(program)));
  i::ScriptDataImpl* data =
      i::ParserApi::PartialPreParse(&stream, NULL);
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


TEST(PreParseOverflow) {
  int marker;
  i::StackGuard::SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  size_t kProgramSize = 1024 * 1024;
  i::SmartPointer<char> program(
      reinterpret_cast<char*>(malloc(kProgramSize + 1)));
  memset(*program, '(', kProgramSize);
  program[kProgramSize] = '\0';

  uintptr_t stack_limit = i::StackGuard::real_climit();

  i::Utf8ToUC16CharacterStream stream(
      reinterpret_cast<const i::byte*>(*program),
      static_cast<unsigned>(kProgramSize));
  i::CompleteParserRecorder log;
  i::V8JavaScriptScanner scanner;
  scanner.Initialize(&stream);


  v8::preparser::PreParser::PreParseResult result =
      v8::preparser::PreParser::PreParseProgram(&scanner,
                                                &log,
                                                true,
                                                stack_limit);
  CHECK_EQ(v8::preparser::PreParser::kPreParseStackOverflow, result);
}


class TestExternalResource: public v8::String::ExternalStringResource {
 public:
  explicit TestExternalResource(uint16_t* data, int length)
      : data_(data), length_(static_cast<size_t>(length)) { }

  ~TestExternalResource() { }

  const uint16_t* data() const {
    return data_;
  }

  size_t length() const {
    return length_;
  }
 private:
  uint16_t* data_;
  size_t length_;
};


#define CHECK_EQU(v1, v2) CHECK_EQ(static_cast<int>(v1), static_cast<int>(v2))

void TestCharacterStream(const char* ascii_source,
                         unsigned length,
                         unsigned start = 0,
                         unsigned end = 0) {
  if (end == 0) end = length;
  unsigned sub_length = end - start;
  i::HandleScope test_scope;
  i::SmartPointer<i::uc16> uc16_buffer(new i::uc16[length]);
  for (unsigned i = 0; i < length; i++) {
    uc16_buffer[i] = static_cast<i::uc16>(ascii_source[i]);
  }
  i::Vector<const char> ascii_vector(ascii_source, static_cast<int>(length));
  i::Handle<i::String> ascii_string(
      i::Factory::NewStringFromAscii(ascii_vector));
  TestExternalResource resource(*uc16_buffer, length);
  i::Handle<i::String> uc16_string(
      i::Factory::NewExternalStringFromTwoByte(&resource));

  i::ExternalTwoByteStringUC16CharacterStream uc16_stream(
      i::Handle<i::ExternalTwoByteString>::cast(uc16_string), start, end);
  i::GenericStringUC16CharacterStream string_stream(ascii_string, start, end);
  i::Utf8ToUC16CharacterStream utf8_stream(
      reinterpret_cast<const i::byte*>(ascii_source), end);
  utf8_stream.SeekForward(start);

  unsigned i = start;
  while (i < end) {
    // Read streams one char at a time
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
    int32_t c0 = ascii_source[i];
    int32_t c1 = uc16_stream.Advance();
    int32_t c2 = string_stream.Advance();
    int32_t c3 = utf8_stream.Advance();
    i++;
    CHECK_EQ(c0, c1);
    CHECK_EQ(c0, c2);
    CHECK_EQ(c0, c3);
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
  }
  while (i > start + sub_length / 4) {
    // Pushback, re-read, pushback again.
    int32_t c0 = ascii_source[i - 1];
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
    uc16_stream.PushBack(c0);
    string_stream.PushBack(c0);
    utf8_stream.PushBack(c0);
    i--;
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
    int32_t c1 = uc16_stream.Advance();
    int32_t c2 = string_stream.Advance();
    int32_t c3 = utf8_stream.Advance();
    i++;
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
    CHECK_EQ(c0, c1);
    CHECK_EQ(c0, c2);
    CHECK_EQ(c0, c3);
    uc16_stream.PushBack(c0);
    string_stream.PushBack(c0);
    utf8_stream.PushBack(c0);
    i--;
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
  }
  unsigned halfway = start + sub_length / 2;
  uc16_stream.SeekForward(halfway - i);
  string_stream.SeekForward(halfway - i);
  utf8_stream.SeekForward(halfway - i);
  i = halfway;
  CHECK_EQU(i, uc16_stream.pos());
  CHECK_EQU(i, string_stream.pos());
  CHECK_EQU(i, utf8_stream.pos());

  while (i < end) {
    // Read streams one char at a time
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
    int32_t c0 = ascii_source[i];
    int32_t c1 = uc16_stream.Advance();
    int32_t c2 = string_stream.Advance();
    int32_t c3 = utf8_stream.Advance();
    i++;
    CHECK_EQ(c0, c1);
    CHECK_EQ(c0, c2);
    CHECK_EQ(c0, c3);
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
  }

  int32_t c1 = uc16_stream.Advance();
  int32_t c2 = string_stream.Advance();
  int32_t c3 = utf8_stream.Advance();
  CHECK_LT(c1, 0);
  CHECK_LT(c2, 0);
  CHECK_LT(c3, 0);
}


TEST(CharacterStreams) {
  v8::HandleScope handles;
  v8::Persistent<v8::Context> context = v8::Context::New();
  v8::Context::Scope context_scope(context);

  TestCharacterStream("abc\0\n\r\x7f", 7);
  static const unsigned kBigStringSize = 4096;
  char buffer[kBigStringSize + 1];
  for (unsigned i = 0; i < kBigStringSize; i++) {
    buffer[i] = static_cast<char>(i & 0x7f);
  }
  TestCharacterStream(buffer, kBigStringSize);

  TestCharacterStream(buffer, kBigStringSize, 576, 3298);

  TestCharacterStream("\0", 1);
  TestCharacterStream("", 0);
}


TEST(Utf8CharacterStream) {
  static const unsigned kMaxUC16CharU = unibrow::Utf8::kMaxThreeByteChar;
  static const int kMaxUC16Char = static_cast<int>(kMaxUC16CharU);

  static const int kAllUtf8CharsSize =
      (unibrow::Utf8::kMaxOneByteChar + 1) +
      (unibrow::Utf8::kMaxTwoByteChar - unibrow::Utf8::kMaxOneByteChar) * 2 +
      (unibrow::Utf8::kMaxThreeByteChar - unibrow::Utf8::kMaxTwoByteChar) * 3;
  static const unsigned kAllUtf8CharsSizeU =
      static_cast<unsigned>(kAllUtf8CharsSize);

  char buffer[kAllUtf8CharsSizeU];
  unsigned cursor = 0;
  for (int i = 0; i <= kMaxUC16Char; i++) {
    cursor += unibrow::Utf8::Encode(buffer + cursor, i);
  }
  ASSERT(cursor == kAllUtf8CharsSizeU);

  i::Utf8ToUC16CharacterStream stream(reinterpret_cast<const i::byte*>(buffer),
                                      kAllUtf8CharsSizeU);
  for (int i = 0; i <= kMaxUC16Char; i++) {
    CHECK_EQU(i, stream.pos());
    int32_t c = stream.Advance();
    CHECK_EQ(i, c);
    CHECK_EQU(i + 1, stream.pos());
  }
  for (int i = kMaxUC16Char; i >= 0; i--) {
    CHECK_EQU(i + 1, stream.pos());
    stream.PushBack(i);
    CHECK_EQU(i, stream.pos());
  }
  int i = 0;
  while (stream.pos() < kMaxUC16CharU) {
    CHECK_EQU(i, stream.pos());
    unsigned progress = stream.SeekForward(12);
    i += progress;
    int32_t c = stream.Advance();
    if (i <= kMaxUC16Char) {
      CHECK_EQ(i, c);
    } else {
      CHECK_EQ(-1, c);
    }
    i += 1;
    CHECK_EQU(i, stream.pos());
  }
}

#undef CHECK_EQU

void TestStreamScanner(i::UC16CharacterStream* stream,
                       i::Token::Value* expected_tokens,
                       int skip_pos = 0,  // Zero means not skipping.
                       int skip_to = 0) {
  i::V8JavaScriptScanner scanner;
  scanner.Initialize(stream);

  int i = 0;
  do {
    i::Token::Value expected = expected_tokens[i];
    i::Token::Value actual = scanner.Next();
    CHECK_EQ(i::Token::String(expected), i::Token::String(actual));
    if (scanner.location().end_pos == skip_pos) {
      scanner.SeekForward(skip_to);
    }
    i++;
  } while (expected_tokens[i] != i::Token::ILLEGAL);
}

TEST(StreamScanner) {
  const char* str1 = "{ foo get for : */ <- \n\n /*foo*/ bib";
  i::Utf8ToUC16CharacterStream stream1(reinterpret_cast<const i::byte*>(str1),
                                       static_cast<unsigned>(strlen(str1)));
  i::Token::Value expectations1[] = {
      i::Token::LBRACE,
      i::Token::IDENTIFIER,
      i::Token::IDENTIFIER,
      i::Token::FOR,
      i::Token::COLON,
      i::Token::MUL,
      i::Token::DIV,
      i::Token::LT,
      i::Token::SUB,
      i::Token::IDENTIFIER,
      i::Token::EOS,
      i::Token::ILLEGAL
  };
  TestStreamScanner(&stream1, expectations1, 0, 0);

  const char* str2 = "case default const {THIS\nPART\nSKIPPED} do";
  i::Utf8ToUC16CharacterStream stream2(reinterpret_cast<const i::byte*>(str2),
                                       static_cast<unsigned>(strlen(str2)));
  i::Token::Value expectations2[] = {
      i::Token::CASE,
      i::Token::DEFAULT,
      i::Token::CONST,
      i::Token::LBRACE,
      // Skipped part here
      i::Token::RBRACE,
      i::Token::DO,
      i::Token::EOS,
      i::Token::ILLEGAL
  };
  ASSERT_EQ('{', str2[19]);
  ASSERT_EQ('}', str2[37]);
  TestStreamScanner(&stream2, expectations2, 20, 37);

  const char* str3 = "{}}}}";
  i::Token::Value expectations3[] = {
      i::Token::LBRACE,
      i::Token::RBRACE,
      i::Token::RBRACE,
      i::Token::RBRACE,
      i::Token::RBRACE,
      i::Token::EOS,
      i::Token::ILLEGAL
  };
  // Skip zero-four RBRACEs.
  for (int i = 0; i <= 4; i++) {
     expectations3[6 - i] = i::Token::ILLEGAL;
     expectations3[5 - i] = i::Token::EOS;
     i::Utf8ToUC16CharacterStream stream3(
         reinterpret_cast<const i::byte*>(str3),
         static_cast<unsigned>(strlen(str3)));
     TestStreamScanner(&stream3, expectations3, 1, 1 + i);
  }
}


void TestScanRegExp(const char* re_source, const char* expected) {
  i::Utf8ToUC16CharacterStream stream(
       reinterpret_cast<const i::byte*>(re_source),
       static_cast<unsigned>(strlen(re_source)));
  i::V8JavaScriptScanner scanner;
  scanner.Initialize(&stream);

  i::Token::Value start = scanner.peek();
  CHECK(start == i::Token::DIV || start == i::Token::ASSIGN_DIV);
  CHECK(scanner.ScanRegExpPattern(start == i::Token::ASSIGN_DIV));
  scanner.Next();  // Current token is now the regexp literal.
  CHECK(scanner.is_literal_ascii());
  i::Vector<const char> actual = scanner.literal_ascii_string();
  for (int i = 0; i < actual.length(); i++) {
    CHECK_NE('\0', expected[i]);
    CHECK_EQ(expected[i], actual[i]);
  }
}


TEST(RegExpScanning) {
  // RegExp token with added garbage at the end. The scanner should only
  // scan the RegExp until the terminating slash just before "flipperwald".
  TestScanRegExp("/b/flipperwald", "b");
  // Incomplete escape sequences doesn't hide the terminating slash.
  TestScanRegExp("/\\x/flipperwald", "\\x");
  TestScanRegExp("/\\u/flipperwald", "\\u");
  TestScanRegExp("/\\u1/flipperwald", "\\u1");
  TestScanRegExp("/\\u12/flipperwald", "\\u12");
  TestScanRegExp("/\\u123/flipperwald", "\\u123");
  TestScanRegExp("/\\c/flipperwald", "\\c");
  TestScanRegExp("/\\c//flipperwald", "\\c");
  // Slashes inside character classes are not terminating.
  TestScanRegExp("/[/]/flipperwald", "[/]");
  TestScanRegExp("/[\\s-/]/flipperwald", "[\\s-/]");
  // Incomplete escape sequences inside a character class doesn't hide
  // the end of the character class.
  TestScanRegExp("/[\\c/]/flipperwald", "[\\c/]");
  TestScanRegExp("/[\\c]/flipperwald", "[\\c]");
  TestScanRegExp("/[\\x]/flipperwald", "[\\x]");
  TestScanRegExp("/[\\x1]/flipperwald", "[\\x1]");
  TestScanRegExp("/[\\u]/flipperwald", "[\\u]");
  TestScanRegExp("/[\\u1]/flipperwald", "[\\u1]");
  TestScanRegExp("/[\\u12]/flipperwald", "[\\u12]");
  TestScanRegExp("/[\\u123]/flipperwald", "[\\u123]");
  // Escaped ']'s wont end the character class.
  TestScanRegExp("/[\\]/]/flipperwald", "[\\]/]");
  // Escaped slashes are not terminating.
  TestScanRegExp("/\\//flipperwald", "\\/");
  // Starting with '=' works too.
  TestScanRegExp("/=/", "=");
  TestScanRegExp("/=?/", "=?");
}
