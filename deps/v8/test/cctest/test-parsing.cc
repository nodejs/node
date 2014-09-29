// Copyright 2012 the V8 project authors. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/v8.h"

#include "src/ast-value-factory.h"
#include "src/compiler.h"
#include "src/execution.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/parser.h"
#include "src/preparser.h"
#include "src/rewriter.h"
#include "src/scanner-character-streams.h"
#include "src/token.h"
#include "src/utils.h"

#include "test/cctest/cctest.h"

TEST(ScanKeywords) {
  struct KeywordToken {
    const char* keyword;
    i::Token::Value token;
  };

  static const KeywordToken keywords[] = {
#define KEYWORD(t, s, d) { s, i::Token::t },
      TOKEN_LIST(IGNORE_TOKEN, KEYWORD)
#undef KEYWORD
      { NULL, i::Token::IDENTIFIER }
  };

  KeywordToken key_token;
  i::UnicodeCache unicode_cache;
  i::byte buffer[32];
  for (int i = 0; (key_token = keywords[i]).keyword != NULL; i++) {
    const i::byte* keyword =
        reinterpret_cast<const i::byte*>(key_token.keyword);
    int length = i::StrLength(key_token.keyword);
    CHECK(static_cast<int>(sizeof(buffer)) >= length);
    {
      i::Utf8ToUtf16CharacterStream stream(keyword, length);
      i::Scanner scanner(&unicode_cache);
      // The scanner should parse Harmony keywords for this test.
      scanner.SetHarmonyScoping(true);
      scanner.SetHarmonyModules(true);
      scanner.Initialize(&stream);
      CHECK_EQ(key_token.token, scanner.Next());
      CHECK_EQ(i::Token::EOS, scanner.Next());
    }
    // Removing characters will make keyword matching fail.
    {
      i::Utf8ToUtf16CharacterStream stream(keyword, length - 1);
      i::Scanner scanner(&unicode_cache);
      scanner.Initialize(&stream);
      CHECK_EQ(i::Token::IDENTIFIER, scanner.Next());
      CHECK_EQ(i::Token::EOS, scanner.Next());
    }
    // Adding characters will make keyword matching fail.
    static const char chars_to_append[] = { 'z', '0', '_' };
    for (int j = 0; j < static_cast<int>(ARRAY_SIZE(chars_to_append)); ++j) {
      i::MemMove(buffer, keyword, length);
      buffer[length] = chars_to_append[j];
      i::Utf8ToUtf16CharacterStream stream(buffer, length + 1);
      i::Scanner scanner(&unicode_cache);
      scanner.Initialize(&stream);
      CHECK_EQ(i::Token::IDENTIFIER, scanner.Next());
      CHECK_EQ(i::Token::EOS, scanner.Next());
    }
    // Replacing characters will make keyword matching fail.
    {
      i::MemMove(buffer, keyword, length);
      buffer[length - 1] = '_';
      i::Utf8ToUtf16CharacterStream stream(buffer, length);
      i::Scanner scanner(&unicode_cache);
      scanner.Initialize(&stream);
      CHECK_EQ(i::Token::IDENTIFIER, scanner.Next());
      CHECK_EQ(i::Token::EOS, scanner.Next());
    }
  }
}


TEST(ScanHTMLEndComments) {
  v8::V8::Initialize();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handles(isolate);

  // Regression test. See:
  //    http://code.google.com/p/chromium/issues/detail?id=53548
  // Tests that --> is correctly interpreted as comment-to-end-of-line if there
  // is only whitespace before it on the line (with comments considered as
  // whitespace, even a multiline-comment containing a newline).
  // This was not the case if it occurred before the first real token
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

  const char* fail_tests[] = {
      "x --> is eol-comment\nvar y = 37;\n",
      "\"\\n\" --> is eol-comment\nvar y = 37;\n",
      "x/* precomment */ --> is eol-comment\nvar y = 37;\n",
      "x/* precomment\n */ --> is eol-comment\nvar y = 37;\n",
      "var x = 42; --> is eol-comment\nvar y = 37;\n",
      "var x = 42; /* precomment\n */ --> is eol-comment\nvar y = 37;\n",
      NULL
  };

  // Parser/Scanner needs a stack limit.
  CcTest::i_isolate()->stack_guard()->SetStackLimit(GetCurrentStackPosition() -
                                                    128 * 1024);
  uintptr_t stack_limit = CcTest::i_isolate()->stack_guard()->real_climit();
  for (int i = 0; tests[i]; i++) {
    const i::byte* source =
        reinterpret_cast<const i::byte*>(tests[i]);
    i::Utf8ToUtf16CharacterStream stream(source, i::StrLength(tests[i]));
    i::CompleteParserRecorder log;
    i::Scanner scanner(CcTest::i_isolate()->unicode_cache());
    scanner.Initialize(&stream);
    i::PreParser preparser(&scanner, &log, stack_limit);
    preparser.set_allow_lazy(true);
    i::PreParser::PreParseResult result = preparser.PreParseProgram();
    CHECK_EQ(i::PreParser::kPreParseSuccess, result);
    CHECK(!log.HasError());
  }

  for (int i = 0; fail_tests[i]; i++) {
    const i::byte* source =
        reinterpret_cast<const i::byte*>(fail_tests[i]);
    i::Utf8ToUtf16CharacterStream stream(source, i::StrLength(fail_tests[i]));
    i::CompleteParserRecorder log;
    i::Scanner scanner(CcTest::i_isolate()->unicode_cache());
    scanner.Initialize(&stream);
    i::PreParser preparser(&scanner, &log, stack_limit);
    preparser.set_allow_lazy(true);
    i::PreParser::PreParseResult result = preparser.PreParseProgram();
    // Even in the case of a syntax error, kPreParseSuccess is returned.
    CHECK_EQ(i::PreParser::kPreParseSuccess, result);
    CHECK(log.HasError());
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


TEST(UsingCachedData) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handles(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  CcTest::i_isolate()->stack_guard()->SetStackLimit(GetCurrentStackPosition() -
                                                    128 * 1024);

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
      "          set setter(v) { this.value = v; }};"
      "var f = a => function (b) { return a + b; };"
      "var g = a => b => a + b;";
  int source_length = i::StrLength(source);

  // ScriptResource will be deleted when the corresponding String is GCd.
  v8::ScriptCompiler::Source script_source(v8::String::NewExternal(
      isolate, new ScriptResource(source, source_length)));
  i::FLAG_harmony_arrow_functions = true;
  i::FLAG_min_preparse_length = 0;
  v8::ScriptCompiler::Compile(isolate, &script_source,
                              v8::ScriptCompiler::kProduceParserCache);
  CHECK(script_source.GetCachedData());

  // Compile the script again, using the cached data.
  bool lazy_flag = i::FLAG_lazy;
  i::FLAG_lazy = true;
  v8::ScriptCompiler::Compile(isolate, &script_source,
                              v8::ScriptCompiler::kConsumeParserCache);
  i::FLAG_lazy = false;
  v8::ScriptCompiler::CompileUnbound(isolate, &script_source,
                                     v8::ScriptCompiler::kConsumeParserCache);
  i::FLAG_lazy = lazy_flag;
}


TEST(PreparseFunctionDataIsUsed) {
  // This tests that we actually do use the function data generated by the
  // preparser.

  // Make preparsing work for short scripts.
  i::FLAG_min_preparse_length = 0;
  i::FLAG_harmony_arrow_functions = true;

  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handles(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  CcTest::i_isolate()->stack_guard()->SetStackLimit(GetCurrentStackPosition() -
                                                    128 * 1024);

  const char* good_code[] = {
      "function this_is_lazy() { var a; } function foo() { return 25; } foo();",
      "var this_is_lazy = () => { var a; }; var foo = () => 25; foo();",
  };

  // Insert a syntax error inside the lazy function.
  const char* bad_code[] = {
      "function this_is_lazy() { if (   } function foo() { return 25; } foo();",
      "var this_is_lazy = () => { if (   }; var foo = () => 25; foo();",
  };

  for (unsigned i = 0; i < ARRAY_SIZE(good_code); i++) {
    v8::ScriptCompiler::Source good_source(v8_str(good_code[i]));
    v8::ScriptCompiler::Compile(isolate, &good_source,
                                v8::ScriptCompiler::kProduceDataToCache);

    const v8::ScriptCompiler::CachedData* cached_data =
        good_source.GetCachedData();
    CHECK(cached_data->data != NULL);
    CHECK_GT(cached_data->length, 0);

    // Now compile the erroneous code with the good preparse data. If the
    // preparse data is used, the lazy function is skipped and it should
    // compile fine.
    v8::ScriptCompiler::Source bad_source(
        v8_str(bad_code[i]), new v8::ScriptCompiler::CachedData(
                                 cached_data->data, cached_data->length));
    v8::Local<v8::Value> result =
        v8::ScriptCompiler::Compile(isolate, &bad_source)->Run();
    CHECK(result->IsInt32());
    CHECK_EQ(25, result->Int32Value());
  }
}


TEST(StandAlonePreParser) {
  v8::V8::Initialize();

  CcTest::i_isolate()->stack_guard()->SetStackLimit(GetCurrentStackPosition() -
                                                    128 * 1024);

  const char* programs[] = {
      "{label: 42}",
      "var x = 42;",
      "function foo(x, y) { return x + y; }",
      "%ArgleBargle(glop);",
      "var x = new new Function('this.x = 42');",
      "var f = (x, y) => x + y;",
      NULL
  };

  uintptr_t stack_limit = CcTest::i_isolate()->stack_guard()->real_climit();
  for (int i = 0; programs[i]; i++) {
    const char* program = programs[i];
    i::Utf8ToUtf16CharacterStream stream(
        reinterpret_cast<const i::byte*>(program),
        static_cast<unsigned>(strlen(program)));
    i::CompleteParserRecorder log;
    i::Scanner scanner(CcTest::i_isolate()->unicode_cache());
    scanner.Initialize(&stream);

    i::PreParser preparser(&scanner, &log, stack_limit);
    preparser.set_allow_lazy(true);
    preparser.set_allow_natives_syntax(true);
    preparser.set_allow_arrow_functions(true);
    i::PreParser::PreParseResult result = preparser.PreParseProgram();
    CHECK_EQ(i::PreParser::kPreParseSuccess, result);
    CHECK(!log.HasError());
  }
}


TEST(StandAlonePreParserNoNatives) {
  v8::V8::Initialize();

  CcTest::i_isolate()->stack_guard()->SetStackLimit(GetCurrentStackPosition() -
                                                    128 * 1024);

  const char* programs[] = {
      "%ArgleBargle(glop);",
      "var x = %_IsSmi(42);",
      NULL
  };

  uintptr_t stack_limit = CcTest::i_isolate()->stack_guard()->real_climit();
  for (int i = 0; programs[i]; i++) {
    const char* program = programs[i];
    i::Utf8ToUtf16CharacterStream stream(
        reinterpret_cast<const i::byte*>(program),
        static_cast<unsigned>(strlen(program)));
    i::CompleteParserRecorder log;
    i::Scanner scanner(CcTest::i_isolate()->unicode_cache());
    scanner.Initialize(&stream);

    // Preparser defaults to disallowing natives syntax.
    i::PreParser preparser(&scanner, &log, stack_limit);
    preparser.set_allow_lazy(true);
    i::PreParser::PreParseResult result = preparser.PreParseProgram();
    CHECK_EQ(i::PreParser::kPreParseSuccess, result);
    CHECK(log.HasError());
  }
}


TEST(PreparsingObjectLiterals) {
  // Regression test for a bug where the symbol stream produced by PreParser
  // didn't match what Parser wanted to consume.
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handles(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  CcTest::i_isolate()->stack_guard()->SetStackLimit(GetCurrentStackPosition() -
                                                    128 * 1024);

  {
    const char* source = "var myo = {if: \"foo\"}; myo.if;";
    v8::Local<v8::Value> result = ParserCacheCompileRun(source);
    CHECK(result->IsString());
    v8::String::Utf8Value utf8(result);
    CHECK_EQ("foo", *utf8);
  }

  {
    const char* source = "var myo = {\"bar\": \"foo\"}; myo[\"bar\"];";
    v8::Local<v8::Value> result = ParserCacheCompileRun(source);
    CHECK(result->IsString());
    v8::String::Utf8Value utf8(result);
    CHECK_EQ("foo", *utf8);
  }

  {
    const char* source = "var myo = {1: \"foo\"}; myo[1];";
    v8::Local<v8::Value> result = ParserCacheCompileRun(source);
    CHECK(result->IsString());
    v8::String::Utf8Value utf8(result);
    CHECK_EQ("foo", *utf8);
  }
}


TEST(RegressChromium62639) {
  v8::V8::Initialize();
  i::Isolate* isolate = CcTest::i_isolate();

  isolate->stack_guard()->SetStackLimit(GetCurrentStackPosition() - 128 * 1024);

  const char* program = "var x = 'something';\n"
                        "escape: function() {}";
  // Fails parsing expecting an identifier after "function".
  // Before fix, didn't check *ok after Expect(Token::Identifier, ok),
  // and then used the invalid currently scanned literal. This always
  // failed in debug mode, and sometimes crashed in release mode.

  i::Utf8ToUtf16CharacterStream stream(
      reinterpret_cast<const i::byte*>(program),
      static_cast<unsigned>(strlen(program)));
  i::CompleteParserRecorder log;
  i::Scanner scanner(CcTest::i_isolate()->unicode_cache());
  scanner.Initialize(&stream);
  i::PreParser preparser(&scanner, &log,
                         CcTest::i_isolate()->stack_guard()->real_climit());
  preparser.set_allow_lazy(true);
  i::PreParser::PreParseResult result = preparser.PreParseProgram();
  // Even in the case of a syntax error, kPreParseSuccess is returned.
  CHECK_EQ(i::PreParser::kPreParseSuccess, result);
  CHECK(log.HasError());
}


TEST(Regress928) {
  v8::V8::Initialize();
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();

  // Preparsing didn't consider the catch clause of a try statement
  // as with-content, which made it assume that a function inside
  // the block could be lazily compiled, and an extra, unexpected,
  // entry was added to the data.
  isolate->stack_guard()->SetStackLimit(GetCurrentStackPosition() - 128 * 1024);

  const char* program =
      "try { } catch (e) { var foo = function () { /* first */ } }"
      "var bar = function () { /* second */ }";

  v8::HandleScope handles(CcTest::isolate());
  i::Handle<i::String> source = factory->NewStringFromAsciiChecked(program);
  i::GenericStringUtf16CharacterStream stream(source, 0, source->length());
  i::CompleteParserRecorder log;
  i::Scanner scanner(CcTest::i_isolate()->unicode_cache());
  scanner.Initialize(&stream);
  i::PreParser preparser(&scanner, &log,
                         CcTest::i_isolate()->stack_guard()->real_climit());
  preparser.set_allow_lazy(true);
  i::PreParser::PreParseResult result = preparser.PreParseProgram();
  CHECK_EQ(i::PreParser::kPreParseSuccess, result);
  i::ScriptData* sd = log.GetScriptData();
  i::ParseData pd(sd);
  pd.Initialize();

  int first_function =
      static_cast<int>(strstr(program, "function") - program);
  int first_lbrace = first_function + i::StrLength("function () ");
  CHECK_EQ('{', program[first_lbrace]);
  i::FunctionEntry entry1 = pd.GetFunctionEntry(first_lbrace);
  CHECK(!entry1.is_valid());

  int second_function =
      static_cast<int>(strstr(program + first_lbrace, "function") - program);
  int second_lbrace =
      second_function + i::StrLength("function () ");
  CHECK_EQ('{', program[second_lbrace]);
  i::FunctionEntry entry2 = pd.GetFunctionEntry(second_lbrace);
  CHECK(entry2.is_valid());
  CHECK_EQ('}', program[entry2.end_pos() - 1]);
  delete sd;
}


TEST(PreParseOverflow) {
  v8::V8::Initialize();

  CcTest::i_isolate()->stack_guard()->SetStackLimit(GetCurrentStackPosition() -
                                                    128 * 1024);

  size_t kProgramSize = 1024 * 1024;
  i::SmartArrayPointer<char> program(i::NewArray<char>(kProgramSize + 1));
  memset(program.get(), '(', kProgramSize);
  program[kProgramSize] = '\0';

  uintptr_t stack_limit = CcTest::i_isolate()->stack_guard()->real_climit();

  i::Utf8ToUtf16CharacterStream stream(
      reinterpret_cast<const i::byte*>(program.get()),
      static_cast<unsigned>(kProgramSize));
  i::CompleteParserRecorder log;
  i::Scanner scanner(CcTest::i_isolate()->unicode_cache());
  scanner.Initialize(&stream);

  i::PreParser preparser(&scanner, &log, stack_limit);
  preparser.set_allow_lazy(true);
  preparser.set_allow_arrow_functions(true);
  i::PreParser::PreParseResult result = preparser.PreParseProgram();
  CHECK_EQ(i::PreParser::kPreParseStackOverflow, result);
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
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  i::HandleScope test_scope(isolate);
  i::SmartArrayPointer<i::uc16> uc16_buffer(new i::uc16[length]);
  for (unsigned i = 0; i < length; i++) {
    uc16_buffer[i] = static_cast<i::uc16>(ascii_source[i]);
  }
  i::Vector<const char> ascii_vector(ascii_source, static_cast<int>(length));
  i::Handle<i::String> ascii_string =
      factory->NewStringFromAscii(ascii_vector).ToHandleChecked();
  TestExternalResource resource(uc16_buffer.get(), length);
  i::Handle<i::String> uc16_string(
      factory->NewExternalStringFromTwoByte(&resource).ToHandleChecked());

  i::ExternalTwoByteStringUtf16CharacterStream uc16_stream(
      i::Handle<i::ExternalTwoByteString>::cast(uc16_string), start, end);
  i::GenericStringUtf16CharacterStream string_stream(ascii_string, start, end);
  i::Utf8ToUtf16CharacterStream utf8_stream(
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
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handles(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
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
    cursor += unibrow::Utf8::Encode(buffer + cursor,
                                    i,
                                    unibrow::Utf16::kNoPreviousCharacter);
  }
  DCHECK(cursor == kAllUtf8CharsSizeU);

  i::Utf8ToUtf16CharacterStream stream(reinterpret_cast<const i::byte*>(buffer),
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

void TestStreamScanner(i::Utf16CharacterStream* stream,
                       i::Token::Value* expected_tokens,
                       int skip_pos = 0,  // Zero means not skipping.
                       int skip_to = 0) {
  i::Scanner scanner(CcTest::i_isolate()->unicode_cache());
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
  v8::V8::Initialize();

  const char* str1 = "{ foo get for : */ <- \n\n /*foo*/ bib";
  i::Utf8ToUtf16CharacterStream stream1(reinterpret_cast<const i::byte*>(str1),
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
  i::Utf8ToUtf16CharacterStream stream2(reinterpret_cast<const i::byte*>(str2),
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
  DCHECK_EQ('{', str2[19]);
  DCHECK_EQ('}', str2[37]);
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
     i::Utf8ToUtf16CharacterStream stream3(
         reinterpret_cast<const i::byte*>(str3),
         static_cast<unsigned>(strlen(str3)));
     TestStreamScanner(&stream3, expectations3, 1, 1 + i);
  }
}


void TestScanRegExp(const char* re_source, const char* expected) {
  i::Utf8ToUtf16CharacterStream stream(
       reinterpret_cast<const i::byte*>(re_source),
       static_cast<unsigned>(strlen(re_source)));
  i::HandleScope scope(CcTest::i_isolate());
  i::Scanner scanner(CcTest::i_isolate()->unicode_cache());
  scanner.Initialize(&stream);

  i::Token::Value start = scanner.peek();
  CHECK(start == i::Token::DIV || start == i::Token::ASSIGN_DIV);
  CHECK(scanner.ScanRegExpPattern(start == i::Token::ASSIGN_DIV));
  scanner.Next();  // Current token is now the regexp literal.
  i::Zone zone(CcTest::i_isolate());
  i::AstValueFactory ast_value_factory(&zone,
                                       CcTest::i_isolate()->heap()->HashSeed());
  ast_value_factory.Internalize(CcTest::i_isolate());
  i::Handle<i::String> val =
      scanner.CurrentSymbol(&ast_value_factory)->string();
  i::DisallowHeapAllocation no_alloc;
  i::String::FlatContent content = val->GetFlatContent();
  CHECK(content.IsAscii());
  i::Vector<const uint8_t> actual = content.ToOneByteVector();
  for (int i = 0; i < actual.length(); i++) {
    CHECK_NE('\0', expected[i]);
    CHECK_EQ(expected[i], actual[i]);
  }
}


TEST(RegExpScanning) {
  v8::V8::Initialize();

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


static int Utf8LengthHelper(const char* s) {
  int len = i::StrLength(s);
  int character_length = len;
  for (int i = 0; i < len; i++) {
    unsigned char c = s[i];
    int input_offset = 0;
    int output_adjust = 0;
    if (c > 0x7f) {
      if (c < 0xc0) continue;
      if (c >= 0xf0) {
        if (c >= 0xf8) {
          // 5 and 6 byte UTF-8 sequences turn into a kBadChar for each UTF-8
          // byte.
          continue;  // Handle first UTF-8 byte.
        }
        if ((c & 7) == 0 && ((s[i + 1] & 0x30) == 0)) {
          // This 4 byte sequence could have been coded as a 3 byte sequence.
          // Record a single kBadChar for the first byte and continue.
          continue;
        }
        input_offset = 3;
        // 4 bytes of UTF-8 turn into 2 UTF-16 code units.
        character_length -= 2;
      } else if (c >= 0xe0) {
        if ((c & 0xf) == 0 && ((s[i + 1] & 0x20) == 0)) {
          // This 3 byte sequence could have been coded as a 2 byte sequence.
          // Record a single kBadChar for the first byte and continue.
          continue;
        }
        input_offset = 2;
        // 3 bytes of UTF-8 turn into 1 UTF-16 code unit.
        output_adjust = 2;
      } else {
        if ((c & 0x1e) == 0) {
          // This 2 byte sequence could have been coded as a 1 byte sequence.
          // Record a single kBadChar for the first byte and continue.
          continue;
        }
        input_offset = 1;
        // 2 bytes of UTF-8 turn into 1 UTF-16 code unit.
        output_adjust = 1;
      }
      bool bad = false;
      for (int j = 1; j <= input_offset; j++) {
        if ((s[i + j] & 0xc0) != 0x80) {
          // Bad UTF-8 sequence turns the first in the sequence into kBadChar,
          // which is a single UTF-16 code unit.
          bad = true;
          break;
        }
      }
      if (!bad) {
        i += input_offset;
        character_length -= output_adjust;
      }
    }
  }
  return character_length;
}


TEST(ScopePositions) {
  v8::internal::FLAG_harmony_scoping = true;

  // Test the parser for correctly setting the start and end positions
  // of a scope. We check the scope positions of exactly one scope
  // nested in the global scope of a program. 'inner source' is the
  // source code that determines the part of the source belonging
  // to the nested scope. 'outer_prefix' and 'outer_suffix' are
  // parts of the source that belong to the global scope.
  struct SourceData {
    const char* outer_prefix;
    const char* inner_source;
    const char* outer_suffix;
    i::ScopeType scope_type;
    i::StrictMode strict_mode;
  };

  const SourceData source_data[] = {
    { "  with ({}) ", "{ block; }", " more;", i::WITH_SCOPE, i::SLOPPY },
    { "  with ({}) ", "{ block; }", "; more;", i::WITH_SCOPE, i::SLOPPY },
    { "  with ({}) ", "{\n"
      "    block;\n"
      "  }", "\n"
      "  more;", i::WITH_SCOPE, i::SLOPPY },
    { "  with ({}) ", "statement;", " more;", i::WITH_SCOPE, i::SLOPPY },
    { "  with ({}) ", "statement", "\n"
      "  more;", i::WITH_SCOPE, i::SLOPPY },
    { "  with ({})\n"
      "    ", "statement;", "\n"
      "  more;", i::WITH_SCOPE, i::SLOPPY },
    { "  try {} catch ", "(e) { block; }", " more;",
      i::CATCH_SCOPE, i::SLOPPY },
    { "  try {} catch ", "(e) { block; }", "; more;",
      i::CATCH_SCOPE, i::SLOPPY },
    { "  try {} catch ", "(e) {\n"
      "    block;\n"
      "  }", "\n"
      "  more;", i::CATCH_SCOPE, i::SLOPPY },
    { "  try {} catch ", "(e) { block; }", " finally { block; } more;",
      i::CATCH_SCOPE, i::SLOPPY },
    { "  start;\n"
      "  ", "{ let block; }", " more;", i::BLOCK_SCOPE, i::STRICT },
    { "  start;\n"
      "  ", "{ let block; }", "; more;", i::BLOCK_SCOPE, i::STRICT },
    { "  start;\n"
      "  ", "{\n"
      "    let block;\n"
      "  }", "\n"
      "  more;", i::BLOCK_SCOPE, i::STRICT },
    { "  start;\n"
      "  function fun", "(a,b) { infunction; }", " more;",
      i::FUNCTION_SCOPE, i::SLOPPY },
    { "  start;\n"
      "  function fun", "(a,b) {\n"
      "    infunction;\n"
      "  }", "\n"
      "  more;", i::FUNCTION_SCOPE, i::SLOPPY },
    // TODO(aperez): Change to use i::ARROW_SCOPE when implemented
    { "  start;\n", "(a,b) => a + b", "; more;",
      i::FUNCTION_SCOPE, i::SLOPPY },
    { "  start;\n", "(a,b) => { return a+b; }", "\nmore;",
      i::FUNCTION_SCOPE, i::SLOPPY },
    { "  start;\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    { "  for ", "(let x = 1 ; x < 10; ++ x) { block; }", " more;",
      i::BLOCK_SCOPE, i::STRICT },
    { "  for ", "(let x = 1 ; x < 10; ++ x) { block; }", "; more;",
      i::BLOCK_SCOPE, i::STRICT },
    { "  for ", "(let x = 1 ; x < 10; ++ x) {\n"
      "    block;\n"
      "  }", "\n"
      "  more;", i::BLOCK_SCOPE, i::STRICT },
    { "  for ", "(let x = 1 ; x < 10; ++ x) statement;", " more;",
      i::BLOCK_SCOPE, i::STRICT },
    { "  for ", "(let x = 1 ; x < 10; ++ x) statement", "\n"
      "  more;", i::BLOCK_SCOPE, i::STRICT },
    { "  for ", "(let x = 1 ; x < 10; ++ x)\n"
      "    statement;", "\n"
      "  more;", i::BLOCK_SCOPE, i::STRICT },
    { "  for ", "(let x in {}) { block; }", " more;",
      i::BLOCK_SCOPE, i::STRICT },
    { "  for ", "(let x in {}) { block; }", "; more;",
      i::BLOCK_SCOPE, i::STRICT },
    { "  for ", "(let x in {}) {\n"
      "    block;\n"
      "  }", "\n"
      "  more;", i::BLOCK_SCOPE, i::STRICT },
    { "  for ", "(let x in {}) statement;", " more;",
      i::BLOCK_SCOPE, i::STRICT },
    { "  for ", "(let x in {}) statement", "\n"
      "  more;", i::BLOCK_SCOPE, i::STRICT },
    { "  for ", "(let x in {})\n"
      "    statement;", "\n"
      "  more;", i::BLOCK_SCOPE, i::STRICT },
    // Check that 6-byte and 4-byte encodings of UTF-8 strings do not throw
    // the preparser off in terms of byte offsets.
    // 6 byte encoding.
    { "  'foo\355\240\201\355\260\211';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // 4 byte encoding.
    { "  'foo\360\220\220\212';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // 3 byte encoding of \u0fff.
    { "  'foo\340\277\277';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Broken 6 byte encoding with missing last byte.
    { "  'foo\355\240\201\355\211';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Broken 3 byte encoding of \u0fff with missing last byte.
    { "  'foo\340\277';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Broken 3 byte encoding of \u0fff with missing 2 last bytes.
    { "  'foo\340';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Broken 3 byte encoding of \u00ff should be a 2 byte encoding.
    { "  'foo\340\203\277';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Broken 3 byte encoding of \u007f should be a 2 byte encoding.
    { "  'foo\340\201\277';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Unpaired lead surrogate.
    { "  'foo\355\240\201';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Unpaired lead surrogate where following code point is a 3 byte sequence.
    { "  'foo\355\240\201\340\277\277';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Unpaired lead surrogate where following code point is a 4 byte encoding
    // of a trail surrogate.
    { "  'foo\355\240\201\360\215\260\211';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Unpaired trail surrogate.
    { "  'foo\355\260\211';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // 2 byte encoding of \u00ff.
    { "  'foo\303\277';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Broken 2 byte encoding of \u00ff with missing last byte.
    { "  'foo\303';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Broken 2 byte encoding of \u007f should be a 1 byte encoding.
    { "  'foo\301\277';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Illegal 5 byte encoding.
    { "  'foo\370\277\277\277\277';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Illegal 6 byte encoding.
    { "  'foo\374\277\277\277\277\277';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Illegal 0xfe byte
    { "  'foo\376\277\277\277\277\277\277';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    // Illegal 0xff byte
    { "  'foo\377\277\277\277\277\277\277\277';\n"
      "  (function fun", "(a,b) { infunction; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    { "  'foo';\n"
      "  (function fun", "(a,b) { 'bar\355\240\201\355\260\213'; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    { "  'foo';\n"
      "  (function fun", "(a,b) { 'bar\360\220\220\214'; }", ")();",
      i::FUNCTION_SCOPE, i::SLOPPY },
    { NULL, NULL, NULL, i::EVAL_SCOPE, i::SLOPPY }
  };

  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();

  v8::HandleScope handles(CcTest::isolate());
  v8::Handle<v8::Context> context = v8::Context::New(CcTest::isolate());
  v8::Context::Scope context_scope(context);

  isolate->stack_guard()->SetStackLimit(GetCurrentStackPosition() - 128 * 1024);

  for (int i = 0; source_data[i].outer_prefix; i++) {
    int kPrefixLen = Utf8LengthHelper(source_data[i].outer_prefix);
    int kInnerLen = Utf8LengthHelper(source_data[i].inner_source);
    int kSuffixLen = Utf8LengthHelper(source_data[i].outer_suffix);
    int kPrefixByteLen = i::StrLength(source_data[i].outer_prefix);
    int kInnerByteLen = i::StrLength(source_data[i].inner_source);
    int kSuffixByteLen = i::StrLength(source_data[i].outer_suffix);
    int kProgramSize = kPrefixLen + kInnerLen + kSuffixLen;
    int kProgramByteSize = kPrefixByteLen + kInnerByteLen + kSuffixByteLen;
    i::ScopedVector<char> program(kProgramByteSize + 1);
    i::SNPrintF(program, "%s%s%s",
                         source_data[i].outer_prefix,
                         source_data[i].inner_source,
                         source_data[i].outer_suffix);

    // Parse program source.
    i::Handle<i::String> source = factory->NewStringFromUtf8(
        i::CStrVector(program.start())).ToHandleChecked();
    CHECK_EQ(source->length(), kProgramSize);
    i::Handle<i::Script> script = factory->NewScript(source);
    i::CompilationInfoWithZone info(script);
    i::Parser parser(&info);
    parser.set_allow_lazy(true);
    parser.set_allow_harmony_scoping(true);
    parser.set_allow_arrow_functions(true);
    info.MarkAsGlobal();
    info.SetStrictMode(source_data[i].strict_mode);
    parser.Parse();
    CHECK(info.function() != NULL);

    // Check scope types and positions.
    i::Scope* scope = info.function()->scope();
    CHECK(scope->is_global_scope());
    CHECK_EQ(scope->start_position(), 0);
    CHECK_EQ(scope->end_position(), kProgramSize);
    CHECK_EQ(scope->inner_scopes()->length(), 1);

    i::Scope* inner_scope = scope->inner_scopes()->at(0);
    CHECK_EQ(inner_scope->scope_type(), source_data[i].scope_type);
    CHECK_EQ(inner_scope->start_position(), kPrefixLen);
    // The end position of a token is one position after the last
    // character belonging to that token.
    CHECK_EQ(inner_scope->end_position(), kPrefixLen + kInnerLen);
  }
}


const char* ReadString(unsigned* start) {
  int length = start[0];
  char* result = i::NewArray<char>(length + 1);
  for (int i = 0; i < length; i++) {
    result[i] = start[i + 1];
  }
  result[length] = '\0';
  return result;
}


i::Handle<i::String> FormatMessage(i::Vector<unsigned> data) {
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  const char* message =
      ReadString(&data[i::PreparseDataConstants::kMessageTextPos]);
  i::Handle<i::String> format = v8::Utils::OpenHandle(
      *v8::String::NewFromUtf8(CcTest::isolate(), message));
  int arg_count = data[i::PreparseDataConstants::kMessageArgCountPos];
  const char* arg = NULL;
  i::Handle<i::JSArray> args_array;
  if (arg_count == 1) {
    // Position after text found by skipping past length field and
    // length field content words.
    int pos = i::PreparseDataConstants::kMessageTextPos + 1 +
              data[i::PreparseDataConstants::kMessageTextPos];
    arg = ReadString(&data[pos]);
    args_array = factory->NewJSArray(1);
    i::JSArray::SetElement(args_array, 0, v8::Utils::OpenHandle(*v8_str(arg)),
                           NONE, i::SLOPPY).Check();
  } else {
    CHECK_EQ(0, arg_count);
    args_array = factory->NewJSArray(0);
  }

  i::Handle<i::JSObject> builtins(isolate->js_builtins_object());
  i::Handle<i::Object> format_fun = i::Object::GetProperty(
      isolate, builtins, "FormatMessage").ToHandleChecked();
  i::Handle<i::Object> arg_handles[] = { format, args_array };
  i::Handle<i::Object> result = i::Execution::Call(
      isolate, format_fun, builtins, 2, arg_handles).ToHandleChecked();
  CHECK(result->IsString());
  i::DeleteArray(message);
  i::DeleteArray(arg);
  data.Dispose();
  return i::Handle<i::String>::cast(result);
}


enum ParserFlag {
  kAllowLazy,
  kAllowNativesSyntax,
  kAllowHarmonyScoping,
  kAllowModules,
  kAllowGenerators,
  kAllowHarmonyNumericLiterals,
  kAllowArrowFunctions
};


enum ParserSyncTestResult {
  kSuccessOrError,
  kSuccess,
  kError
};

template <typename Traits>
void SetParserFlags(i::ParserBase<Traits>* parser,
                    i::EnumSet<ParserFlag> flags) {
  parser->set_allow_lazy(flags.Contains(kAllowLazy));
  parser->set_allow_natives_syntax(flags.Contains(kAllowNativesSyntax));
  parser->set_allow_harmony_scoping(flags.Contains(kAllowHarmonyScoping));
  parser->set_allow_modules(flags.Contains(kAllowModules));
  parser->set_allow_generators(flags.Contains(kAllowGenerators));
  parser->set_allow_harmony_numeric_literals(
      flags.Contains(kAllowHarmonyNumericLiterals));
  parser->set_allow_arrow_functions(flags.Contains(kAllowArrowFunctions));
}


void TestParserSyncWithFlags(i::Handle<i::String> source,
                             i::EnumSet<ParserFlag> flags,
                             ParserSyncTestResult result) {
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();

  uintptr_t stack_limit = isolate->stack_guard()->real_climit();

  // Preparse the data.
  i::CompleteParserRecorder log;
  {
    i::Scanner scanner(isolate->unicode_cache());
    i::GenericStringUtf16CharacterStream stream(source, 0, source->length());
    i::PreParser preparser(&scanner, &log, stack_limit);
    SetParserFlags(&preparser, flags);
    scanner.Initialize(&stream);
    i::PreParser::PreParseResult result = preparser.PreParseProgram();
    CHECK_EQ(i::PreParser::kPreParseSuccess, result);
  }

  bool preparse_error = log.HasError();

  // Parse the data
  i::FunctionLiteral* function;
  {
    i::Handle<i::Script> script = factory->NewScript(source);
    i::CompilationInfoWithZone info(script);
    i::Parser parser(&info);
    SetParserFlags(&parser, flags);
    info.MarkAsGlobal();
    parser.Parse();
    function = info.function();
  }

  // Check that preparsing fails iff parsing fails.
  if (function == NULL) {
    // Extract exception from the parser.
    CHECK(isolate->has_pending_exception());
    i::Handle<i::JSObject> exception_handle(
        i::JSObject::cast(isolate->pending_exception()));
    i::Handle<i::String> message_string =
        i::Handle<i::String>::cast(i::Object::GetProperty(
            isolate, exception_handle, "message").ToHandleChecked());

    if (result == kSuccess) {
      v8::base::OS::Print(
          "Parser failed on:\n"
          "\t%s\n"
          "with error:\n"
          "\t%s\n"
          "However, we expected no error.",
          source->ToCString().get(), message_string->ToCString().get());
      CHECK(false);
    }

    if (!preparse_error) {
      v8::base::OS::Print(
          "Parser failed on:\n"
          "\t%s\n"
          "with error:\n"
          "\t%s\n"
          "However, the preparser succeeded",
          source->ToCString().get(), message_string->ToCString().get());
      CHECK(false);
    }
    // Check that preparser and parser produce the same error.
    i::Handle<i::String> preparser_message =
        FormatMessage(log.ErrorMessageData());
    if (!i::String::Equals(message_string, preparser_message)) {
      v8::base::OS::Print(
          "Expected parser and preparser to produce the same error on:\n"
          "\t%s\n"
          "However, found the following error messages\n"
          "\tparser:    %s\n"
          "\tpreparser: %s\n",
          source->ToCString().get(),
          message_string->ToCString().get(),
          preparser_message->ToCString().get());
      CHECK(false);
    }
  } else if (preparse_error) {
    v8::base::OS::Print(
        "Preparser failed on:\n"
        "\t%s\n"
        "with error:\n"
        "\t%s\n"
        "However, the parser succeeded",
        source->ToCString().get(),
        FormatMessage(log.ErrorMessageData())->ToCString().get());
    CHECK(false);
  } else if (result == kError) {
    v8::base::OS::Print(
        "Expected error on:\n"
        "\t%s\n"
        "However, parser and preparser succeeded",
        source->ToCString().get());
    CHECK(false);
  }
}


void TestParserSync(const char* source,
                    const ParserFlag* varying_flags,
                    size_t varying_flags_length,
                    ParserSyncTestResult result = kSuccessOrError,
                    const ParserFlag* always_true_flags = NULL,
                    size_t always_true_flags_length = 0) {
  i::Handle<i::String> str =
      CcTest::i_isolate()->factory()->NewStringFromAsciiChecked(source);
  for (int bits = 0; bits < (1 << varying_flags_length); bits++) {
    i::EnumSet<ParserFlag> flags;
    for (size_t flag_index = 0; flag_index < varying_flags_length;
         ++flag_index) {
      if ((bits & (1 << flag_index)) != 0) flags.Add(varying_flags[flag_index]);
    }
    for (size_t flag_index = 0; flag_index < always_true_flags_length;
         ++flag_index) {
      flags.Add(always_true_flags[flag_index]);
    }
    TestParserSyncWithFlags(str, flags, result);
  }
}


TEST(ParserSync) {
  const char* context_data[][2] = {
    { "", "" },
    { "{", "}" },
    { "if (true) ", " else {}" },
    { "if (true) {} else ", "" },
    { "if (true) ", "" },
    { "do ", " while (false)" },
    { "while (false) ", "" },
    { "for (;;) ", "" },
    { "with ({})", "" },
    { "switch (12) { case 12: ", "}" },
    { "switch (12) { default: ", "}" },
    { "switch (12) { ", "case 12: }" },
    { "label2: ", "" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "{}",
    "var x",
    "var x = 1",
    "const x",
    "const x = 1",
    ";",
    "12",
    "if (false) {} else ;",
    "if (false) {} else {}",
    "if (false) {} else 12",
    "if (false) ;"
    "if (false) {}",
    "if (false) 12",
    "do {} while (false)",
    "for (;;) ;",
    "for (;;) {}",
    "for (;;) 12",
    "continue",
    "continue label",
    "continue\nlabel",
    "break",
    "break label",
    "break\nlabel",
    // TODO(marja): activate once parsing 'return' is merged into ParserBase.
    // "return",
    // "return  12",
    // "return\n12",
    "with ({}) ;",
    "with ({}) {}",
    "with ({}) 12",
    "switch ({}) { default: }"
    "label3: "
    "throw",
    "throw  12",
    "throw\n12",
    "try {} catch(e) {}",
    "try {} finally {}",
    "try {} catch(e) {} finally {}",
    "debugger",
    NULL
  };

  const char* termination_data[] = {
    "",
    ";",
    "\n",
    ";\n",
    "\n;",
    NULL
  };

  v8::HandleScope handles(CcTest::isolate());
  v8::Handle<v8::Context> context = v8::Context::New(CcTest::isolate());
  v8::Context::Scope context_scope(context);

  CcTest::i_isolate()->stack_guard()->SetStackLimit(GetCurrentStackPosition() -
                                                    128 * 1024);

  static const ParserFlag flags1[] = {kAllowLazy, kAllowHarmonyScoping,
                                      kAllowModules, kAllowGenerators,
                                      kAllowArrowFunctions};
  for (int i = 0; context_data[i][0] != NULL; ++i) {
    for (int j = 0; statement_data[j] != NULL; ++j) {
      for (int k = 0; termination_data[k] != NULL; ++k) {
        int kPrefixLen = i::StrLength(context_data[i][0]);
        int kStatementLen = i::StrLength(statement_data[j]);
        int kTerminationLen = i::StrLength(termination_data[k]);
        int kSuffixLen = i::StrLength(context_data[i][1]);
        int kProgramSize = kPrefixLen + kStatementLen + kTerminationLen
            + kSuffixLen + i::StrLength("label: for (;;) {  }");

        // Plug the source code pieces together.
        i::ScopedVector<char> program(kProgramSize + 1);
        int length = i::SNPrintF(program,
            "label: for (;;) { %s%s%s%s }",
            context_data[i][0],
            statement_data[j],
            termination_data[k],
            context_data[i][1]);
        CHECK(length == kProgramSize);
        TestParserSync(program.start(), flags1, ARRAY_SIZE(flags1));
      }
    }
  }

  // Neither Harmony numeric literals nor our natives syntax have any
  // interaction with the flags above, so test these separately to reduce
  // the combinatorial explosion.
  static const ParserFlag flags2[] = { kAllowHarmonyNumericLiterals };
  TestParserSync("0o1234", flags2, ARRAY_SIZE(flags2));
  TestParserSync("0b1011", flags2, ARRAY_SIZE(flags2));

  static const ParserFlag flags3[] = { kAllowNativesSyntax };
  TestParserSync("%DebugPrint(123)", flags3, ARRAY_SIZE(flags3));
}


TEST(StrictOctal) {
  // Test that syntax error caused by octal literal is reported correctly as
  // such (issue 2220).
  v8::V8::Initialize();
  v8::HandleScope scope(CcTest::isolate());
  v8::Context::Scope context_scope(
      v8::Context::New(CcTest::isolate()));
  v8::TryCatch try_catch;
  const char* script =
      "\"use strict\";       \n"
      "a = function() {      \n"
      "  b = function() {    \n"
      "    01;               \n"
      "  };                  \n"
      "};                    \n";
  v8::Script::Compile(v8::String::NewFromUtf8(CcTest::isolate(), script));
  CHECK(try_catch.HasCaught());
  v8::String::Utf8Value exception(try_catch.Exception());
  CHECK_EQ("SyntaxError: Octal literals are not allowed in strict mode.",
           *exception);
}


void RunParserSyncTest(const char* context_data[][2],
                       const char* statement_data[],
                       ParserSyncTestResult result,
                       const ParserFlag* flags = NULL,
                       int flags_len = 0,
                       const ParserFlag* always_true_flags = NULL,
                       int always_true_flags_len = 0) {
  v8::HandleScope handles(CcTest::isolate());
  v8::Handle<v8::Context> context = v8::Context::New(CcTest::isolate());
  v8::Context::Scope context_scope(context);

  CcTest::i_isolate()->stack_guard()->SetStackLimit(GetCurrentStackPosition() -
                                                    128 * 1024);

  static const ParserFlag default_flags[] = {
      kAllowLazy,       kAllowHarmonyScoping, kAllowModules,
      kAllowGenerators, kAllowNativesSyntax,  kAllowArrowFunctions};
  ParserFlag* generated_flags = NULL;
  if (flags == NULL) {
    flags = default_flags;
    flags_len = ARRAY_SIZE(default_flags);
    if (always_true_flags != NULL) {
      // Remove always_true_flags from default_flags.
      CHECK(always_true_flags_len < flags_len);
      generated_flags = new ParserFlag[flags_len - always_true_flags_len];
      int flag_index = 0;
      for (int i = 0; i < flags_len; ++i) {
        bool use_flag = true;
        for (int j = 0; j < always_true_flags_len; ++j) {
          if (flags[i] == always_true_flags[j]) {
            use_flag = false;
            break;
          }
        }
        if (use_flag) generated_flags[flag_index++] = flags[i];
      }
      CHECK(flag_index == flags_len - always_true_flags_len);
      flags_len = flag_index;
      flags = generated_flags;
    }
  }
  for (int i = 0; context_data[i][0] != NULL; ++i) {
    for (int j = 0; statement_data[j] != NULL; ++j) {
      int kPrefixLen = i::StrLength(context_data[i][0]);
      int kStatementLen = i::StrLength(statement_data[j]);
      int kSuffixLen = i::StrLength(context_data[i][1]);
      int kProgramSize = kPrefixLen + kStatementLen + kSuffixLen;

      // Plug the source code pieces together.
      i::ScopedVector<char> program(kProgramSize + 1);
      int length = i::SNPrintF(program,
                               "%s%s%s",
                               context_data[i][0],
                               statement_data[j],
                               context_data[i][1]);
      CHECK(length == kProgramSize);
      TestParserSync(program.start(),
                     flags,
                     flags_len,
                     result,
                     always_true_flags,
                     always_true_flags_len);
    }
  }
  delete[] generated_flags;
}


TEST(ErrorsEvalAndArguments) {
  // Tests that both preparsing and parsing produce the right kind of errors for
  // using "eval" and "arguments" as identifiers. Without the strict mode, it's
  // ok to use "eval" or "arguments" as identifiers. With the strict mode, it
  // isn't.
  const char* context_data[][2] = {
    { "\"use strict\";", "" },
    { "var eval; function test_func() {\"use strict\"; ", "}"},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var eval;",
    "var arguments",
    "var foo, eval;",
    "var foo, arguments;",
    "try { } catch (eval) { }",
    "try { } catch (arguments) { }",
    "function eval() { }",
    "function arguments() { }",
    "function foo(eval) { }",
    "function foo(arguments) { }",
    "function foo(bar, eval) { }",
    "function foo(bar, arguments) { }",
    "(eval) => { }",
    "(arguments) => { }",
    "(foo, eval) => { }",
    "(foo, arguments) => { }",
    "eval = 1;",
    "arguments = 1;",
    "var foo = eval = 1;",
    "var foo = arguments = 1;",
    "++eval;",
    "++arguments;",
    "eval++;",
    "arguments++;",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(NoErrorsEvalAndArgumentsSloppy) {
  // Tests that both preparsing and parsing accept "eval" and "arguments" as
  // identifiers when needed.
  const char* context_data[][2] = {
    { "", "" },
    { "function test_func() {", "}"},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var eval;",
    "var arguments",
    "var foo, eval;",
    "var foo, arguments;",
    "try { } catch (eval) { }",
    "try { } catch (arguments) { }",
    "function eval() { }",
    "function arguments() { }",
    "function foo(eval) { }",
    "function foo(arguments) { }",
    "function foo(bar, eval) { }",
    "function foo(bar, arguments) { }",
    "eval = 1;",
    "arguments = 1;",
    "var foo = eval = 1;",
    "var foo = arguments = 1;",
    "++eval;",
    "++arguments;",
    "eval++;",
    "arguments++;",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
}


TEST(NoErrorsEvalAndArgumentsStrict) {
  const char* context_data[][2] = {
    { "\"use strict\";", "" },
    { "function test_func() { \"use strict\";", "}" },
    { "() => { \"use strict\"; ", "}" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "eval;",
    "arguments;",
    "var foo = eval;",
    "var foo = arguments;",
    "var foo = { eval: 1 };",
    "var foo = { arguments: 1 };",
    "var foo = { }; foo.eval = {};",
    "var foo = { }; foo.arguments = {};",
    NULL
  };

  static const ParserFlag always_flags[] = {kAllowArrowFunctions};
  RunParserSyncTest(context_data, statement_data, kSuccess, NULL, 0,
                    always_flags, ARRAY_SIZE(always_flags));
}


TEST(ErrorsFutureStrictReservedWords) {
  // Tests that both preparsing and parsing produce the right kind of errors for
  // using future strict reserved words as identifiers. Without the strict mode,
  // it's ok to use future strict reserved words as identifiers. With the strict
  // mode, it isn't.
  const char* context_data[][2] = {
    { "\"use strict\";", "" },
    { "function test_func() {\"use strict\"; ", "}"},
    { "() => { \"use strict\"; ", "}" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var interface;",
    "var foo, interface;",
    "try { } catch (interface) { }",
    "function interface() { }",
    "function foo(interface) { }",
    "function foo(bar, interface) { }",
    "interface = 1;",
    "var foo = interface = 1;",
    "++interface;",
    "interface++;",
    "var yield = 13;",
    NULL
  };

  static const ParserFlag always_flags[] = {kAllowArrowFunctions};
  RunParserSyncTest(context_data, statement_data, kError, NULL, 0, always_flags,
                    ARRAY_SIZE(always_flags));
}


TEST(NoErrorsFutureStrictReservedWords) {
  const char* context_data[][2] = {
    { "", "" },
    { "function test_func() {", "}"},
    { "() => {", "}" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var interface;",
    "var foo, interface;",
    "try { } catch (interface) { }",
    "function interface() { }",
    "function foo(interface) { }",
    "function foo(bar, interface) { }",
    "interface = 1;",
    "var foo = interface = 1;",
    "++interface;",
    "interface++;",
    "var yield = 13;",
    NULL
  };

  static const ParserFlag always_flags[] = {kAllowArrowFunctions};
  RunParserSyncTest(context_data, statement_data, kSuccess, NULL, 0,
                    always_flags, ARRAY_SIZE(always_flags));
}


TEST(ErrorsReservedWords) {
  // Tests that both preparsing and parsing produce the right kind of errors for
  // using future reserved words as identifiers. These tests don't depend on the
  // strict mode.
  const char* context_data[][2] = {
    { "", "" },
    { "\"use strict\";", "" },
    { "var eval; function test_func() {", "}"},
    { "var eval; function test_func() {\"use strict\"; ", "}"},
    { "var eval; () => {", "}"},
    { "var eval; () => {\"use strict\"; ", "}"},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var super;",
    "var foo, super;",
    "try { } catch (super) { }",
    "function super() { }",
    "function foo(super) { }",
    "function foo(bar, super) { }",
    "(super) => { }",
    "(bar, super) => { }",
    "super = 1;",
    "var foo = super = 1;",
    "++super;",
    "super++;",
    "function foo super",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(NoErrorsLetSloppyAllModes) {
  // In sloppy mode, it's okay to use "let" as identifier.
  const char* context_data[][2] = {
    { "", "" },
    { "function f() {", "}" },
    { "(function f() {", "})" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var let;",
    "var foo, let;",
    "try { } catch (let) { }",
    "function let() { }",
    "(function let() { })",
    "function foo(let) { }",
    "function foo(bar, let) { }",
    "let = 1;",
    "var foo = let = 1;",
    "let * 2;",
    "++let;",
    "let++;",
    "let: 34",
    "function let(let) { let: let(let + let(0)); }",
    "({ let: 1 })",
    "({ get let() { 1 } })",
    "let(100)",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
}


TEST(NoErrorsYieldSloppyAllModes) {
  // In sloppy mode, it's okay to use "yield" as identifier, *except* inside a
  // generator (see other test).
  const char* context_data[][2] = {
    { "", "" },
    { "function not_gen() {", "}" },
    { "(function not_gen() {", "})" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var yield;",
    "var foo, yield;",
    "try { } catch (yield) { }",
    "function yield() { }",
    "(function yield() { })",
    "function foo(yield) { }",
    "function foo(bar, yield) { }",
    "yield = 1;",
    "var foo = yield = 1;",
    "yield * 2;",
    "++yield;",
    "yield++;",
    "yield: 34",
    "function yield(yield) { yield: yield (yield + yield(0)); }",
    "({ yield: 1 })",
    "({ get yield() { 1 } })",
    "yield(100)",
    "yield[100]",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
}


TEST(NoErrorsYieldSloppyGeneratorsEnabled) {
  // In sloppy mode, it's okay to use "yield" as identifier, *except* inside a
  // generator (see next test).
  const char* context_data[][2] = {
    { "", "" },
    { "function not_gen() {", "}" },
    { "function * gen() { function not_gen() {", "} }" },
    { "(function not_gen() {", "})" },
    { "(function * gen() { (function not_gen() {", "}) })" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var yield;",
    "var foo, yield;",
    "try { } catch (yield) { }",
    "function yield() { }",
    "(function yield() { })",
    "function foo(yield) { }",
    "function foo(bar, yield) { }",
    "function * yield() { }",
    "(function * yield() { })",
    "yield = 1;",
    "var foo = yield = 1;",
    "yield * 2;",
    "++yield;",
    "yield++;",
    "yield: 34",
    "function yield(yield) { yield: yield (yield + yield(0)); }",
    "({ yield: 1 })",
    "({ get yield() { 1 } })",
    "yield(100)",
    "yield[100]",
    NULL
  };

  // This test requires kAllowGenerators to succeed.
  static const ParserFlag always_true_flags[] = { kAllowGenerators };
  RunParserSyncTest(context_data, statement_data, kSuccess, NULL, 0,
                    always_true_flags, 1);
}


TEST(ErrorsYieldStrict) {
  const char* context_data[][2] = {
    { "\"use strict\";", "" },
    { "\"use strict\"; function not_gen() {", "}" },
    { "function test_func() {\"use strict\"; ", "}"},
    { "\"use strict\"; function * gen() { function not_gen() {", "} }" },
    { "\"use strict\"; (function not_gen() {", "})" },
    { "\"use strict\"; (function * gen() { (function not_gen() {", "}) })" },
    { "() => {\"use strict\"; ", "}" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var yield;",
    "var foo, yield;",
    "try { } catch (yield) { }",
    "function yield() { }",
    "(function yield() { })",
    "function foo(yield) { }",
    "function foo(bar, yield) { }",
    "function * yield() { }",
    "(function * yield() { })",
    "yield = 1;",
    "var foo = yield = 1;",
    "++yield;",
    "yield++;",
    "yield: 34;",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(NoErrorsGenerator) {
  const char* context_data[][2] = {
    { "function * gen() {", "}" },
    { "(function * gen() {", "})" },
    { "(function * () {", "})" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    // A generator without a body is valid.
    ""
    // Valid yield expressions inside generators.
    "yield 2;",
    "yield * 2;",
    "yield * \n 2;",
    "yield yield 1;",
    "yield * yield * 1;",
    "yield 3 + (yield 4);",
    "yield * 3 + (yield * 4);",
    "(yield * 3) + (yield * 4);",
    "yield 3; yield 4;",
    "yield * 3; yield * 4;",
    "(function (yield) { })",
    "yield { yield: 12 }",
    "yield /* comment */ { yield: 12 }",
    "yield * \n { yield: 12 }",
    "yield /* comment */ * \n { yield: 12 }",
    // You can return in a generator.
    "yield 1; return",
    "yield * 1; return",
    "yield 1; return 37",
    "yield * 1; return 37",
    "yield 1; return 37; yield 'dead';",
    "yield * 1; return 37; yield * 'dead';",
    // Yield is still a valid key in object literals.
    "({ yield: 1 })",
    "({ get yield() { } })",
    // Yield without RHS.
    "yield;",
    "yield",
    "yield\n",
    "yield /* comment */"
    "yield // comment\n"
    "(yield)",
    "[yield]",
    "{yield}",
    "yield, yield",
    "yield; yield",
    "(yield) ? yield : yield",
    "(yield) \n ? yield : yield",
    // If there is a newline before the next token, we don't look for RHS.
    "yield\nfor (;;) {}",
    NULL
  };

  // This test requires kAllowGenerators to succeed.
  static const ParserFlag always_true_flags[] = {
    kAllowGenerators
  };
  RunParserSyncTest(context_data, statement_data, kSuccess, NULL, 0,
                    always_true_flags, 1);
}


TEST(ErrorsYieldGenerator) {
  const char* context_data[][2] = {
    { "function * gen() {", "}" },
    { "\"use strict\"; function * gen() {", "}" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    // Invalid yield expressions inside generators.
    "var yield;",
    "var foo, yield;",
    "try { } catch (yield) { }",
    "function yield() { }",
    // The name of the NFE is let-bound in the generator, which does not permit
    // yield to be an identifier.
    "(function yield() { })",
    "(function * yield() { })",
    // Yield isn't valid as a formal parameter for generators.
    "function * foo(yield) { }",
    "(function * foo(yield) { })",
    "yield = 1;",
    "var foo = yield = 1;",
    "++yield;",
    "yield++;",
    "yield *",
    "(yield *)",
    // Yield binds very loosely, so this parses as "yield (3 + yield 4)", which
    // is invalid.
    "yield 3 + yield 4;",
    "yield: 34",
    "yield ? 1 : 2",
    // Parses as yield (/ yield): invalid.
    "yield / yield",
    "+ yield",
    "+ yield 3",
    // Invalid (no newline allowed between yield and *).
    "yield\n*3",
    // Invalid (we see a newline, so we parse {yield:42} as a statement, not an
    // object literal, and yield is not a valid label).
    "yield\n{yield: 42}",
    "yield /* comment */\n {yield: 42}",
    "yield //comment\n {yield: 42}",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(ErrorsNameOfStrictFunction) {
  // Tests that illegal tokens as names of a strict function produce the correct
  // errors.
  const char* context_data[][2] = {
    { "function ", ""},
    { "\"use strict\"; function", ""},
    { "function * ", ""},
    { "\"use strict\"; function * ", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "eval() {\"use strict\";}",
    "arguments() {\"use strict\";}",
    "interface() {\"use strict\";}",
    "yield() {\"use strict\";}",
    // Future reserved words are always illegal
    "function super() { }",
    "function super() {\"use strict\";}",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(NoErrorsNameOfStrictFunction) {
  const char* context_data[][2] = {
    { "function ", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "eval() { }",
    "arguments() { }",
    "interface() { }",
    "yield() { }",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
}


TEST(NoErrorsNameOfStrictGenerator) {
  const char* context_data[][2] = {
    { "function * ", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "eval() { }",
    "arguments() { }",
    "interface() { }",
    "yield() { }",
    NULL
  };

  // This test requires kAllowGenerators to succeed.
  static const ParserFlag always_true_flags[] = {
    kAllowGenerators
  };
  RunParserSyncTest(context_data, statement_data, kSuccess, NULL, 0,
                    always_true_flags, 1);
}


TEST(ErrorsIllegalWordsAsLabelsSloppy) {
  // Using future reserved words as labels is always an error.
  const char* context_data[][2] = {
    { "", ""},
    { "function test_func() {", "}" },
    { "() => {", "}" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "super: while(true) { break super; }",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(ErrorsIllegalWordsAsLabelsStrict) {
  // Tests that illegal tokens as labels produce the correct errors.
  const char* context_data[][2] = {
    { "\"use strict\";", "" },
    { "function test_func() {\"use strict\"; ", "}"},
    { "() => {\"use strict\"; ", "}" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "super: while(true) { break super; }",
    "interface: while(true) { break interface; }",
    "yield: while(true) { break yield; }",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(NoErrorsIllegalWordsAsLabels) {
  // Using eval and arguments as labels is legal even in strict mode.
  const char* context_data[][2] = {
    { "", ""},
    { "function test_func() {", "}" },
    { "() => {", "}" },
    { "\"use strict\";", "" },
    { "\"use strict\"; function test_func() {", "}" },
    { "\"use strict\"; () => {", "}" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "mylabel: while(true) { break mylabel; }",
    "eval: while(true) { break eval; }",
    "arguments: while(true) { break arguments; }",
    NULL
  };

  static const ParserFlag always_flags[] = {kAllowArrowFunctions};
  RunParserSyncTest(context_data, statement_data, kSuccess, NULL, 0,
                    always_flags, ARRAY_SIZE(always_flags));
}


TEST(ErrorsParenthesizedLabels) {
  // Parenthesized identifiers shouldn't be recognized as labels.
  const char* context_data[][2] = {
    { "", ""},
    { "function test_func() {", "}" },
    { "() => {", "}" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "(mylabel): while(true) { break mylabel; }",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(NoErrorsParenthesizedDirectivePrologue) {
  // Parenthesized directive prologue shouldn't be recognized.
  const char* context_data[][2] = {
    { "", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "(\"use strict\"); var eval;",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
}


TEST(ErrorsNotAnIdentifierName) {
  const char* context_data[][2] = {
    { "", ""},
    { "\"use strict\";", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var foo = {}; foo.{;",
    "var foo = {}; foo.};",
    "var foo = {}; foo.=;",
    "var foo = {}; foo.888;",
    "var foo = {}; foo.-;",
    "var foo = {}; foo.--;",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(NoErrorsIdentifierNames) {
  // Keywords etc. are valid as property names.
  const char* context_data[][2] = {
    { "", ""},
    { "\"use strict\";", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var foo = {}; foo.if;",
    "var foo = {}; foo.yield;",
    "var foo = {}; foo.super;",
    "var foo = {}; foo.interface;",
    "var foo = {}; foo.eval;",
    "var foo = {}; foo.arguments;",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
}


TEST(DontRegressPreParserDataSizes) {
  // These tests make sure that Parser doesn't start producing less "preparse
  // data" (data which the embedder can cache).
  v8::V8::Initialize();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handles(isolate);

  CcTest::i_isolate()->stack_guard()->SetStackLimit(GetCurrentStackPosition() -
                                                    128 * 1024);

  struct TestCase {
    const char* program;
    int functions;
  } test_cases[] = {
    // No functions.
    {"var x = 42;", 0},
    // Functions.
    {"function foo() {}", 1}, {"function foo() {} function bar() {}", 2},
    // Getter / setter functions are recorded as functions if they're on the top
    // level.
    {"var x = {get foo(){} };", 1},
    // Functions insize lazy functions are not recorded.
    {"function lazy() { function a() {} function b() {} function c() {} }", 1},
    {"function lazy() { var x = {get foo(){} } }", 1},
    {NULL, 0}
  };

  for (int i = 0; test_cases[i].program; i++) {
    const char* program = test_cases[i].program;
    i::Factory* factory = CcTest::i_isolate()->factory();
    i::Handle<i::String> source =
        factory->NewStringFromUtf8(i::CStrVector(program)).ToHandleChecked();
    i::Handle<i::Script> script = factory->NewScript(source);
    i::CompilationInfoWithZone info(script);
    i::ScriptData* sd = NULL;
    info.SetCachedData(&sd, v8::ScriptCompiler::kProduceParserCache);
    i::Parser::Parse(&info, true);
    i::ParseData pd(sd);

    if (pd.FunctionCount() != test_cases[i].functions) {
      v8::base::OS::Print(
          "Expected preparse data for program:\n"
          "\t%s\n"
          "to contain %d functions, however, received %d functions.\n",
          program, test_cases[i].functions, pd.FunctionCount());
      CHECK(false);
    }
    delete sd;
  }
}


TEST(FunctionDeclaresItselfStrict) {
  // Tests that we produce the right kinds of errors when a function declares
  // itself strict (we cannot produce there errors as soon as we see the
  // offending identifiers, because we don't know at that point whether the
  // function is strict or not).
  const char* context_data[][2] = {
    {"function eval() {", "}"},
    {"function arguments() {", "}"},
    {"function yield() {", "}"},
    {"function interface() {", "}"},
    {"function foo(eval) {", "}"},
    {"function foo(arguments) {", "}"},
    {"function foo(yield) {", "}"},
    {"function foo(interface) {", "}"},
    {"function foo(bar, eval) {", "}"},
    {"function foo(bar, arguments) {", "}"},
    {"function foo(bar, yield) {", "}"},
    {"function foo(bar, interface) {", "}"},
    {"function foo(bar, bar) {", "}"},
    { NULL, NULL }
  };

  const char* strict_statement_data[] = {
    "\"use strict\";",
    NULL
  };

  const char* non_strict_statement_data[] = {
    ";",
    NULL
  };

  RunParserSyncTest(context_data, strict_statement_data, kError);
  RunParserSyncTest(context_data, non_strict_statement_data, kSuccess);
}


TEST(ErrorsTryWithoutCatchOrFinally) {
  const char* context_data[][2] = {
    {"", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "try { }",
    "try { } foo();",
    "try { } catch (e) foo();",
    "try { } catch { }",
    "try { } finally foo();",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(NoErrorsTryCatchFinally) {
  const char* context_data[][2] = {
    {"", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "try { } catch (e) { }",
    "try { } catch (e) { } finally { }",
    "try { } finally { }",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
}


TEST(ErrorsRegexpLiteral) {
  const char* context_data[][2] = {
    {"var r = ", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "/unterminated",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(NoErrorsRegexpLiteral) {
  const char* context_data[][2] = {
    {"var r = ", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "/foo/",
    "/foo/g",
    "/foo/whatever",  // This is an error but not detected by the parser.
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
}


TEST(Intrinsics) {
  const char* context_data[][2] = {
    {"", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "%someintrinsic(arg)",
    NULL
  };

  // This test requires kAllowNativesSyntax to succeed.
  static const ParserFlag always_true_flags[] = {
    kAllowNativesSyntax
  };

  RunParserSyncTest(context_data, statement_data, kSuccess, NULL, 0,
                    always_true_flags, 1);
}


TEST(NoErrorsNewExpression) {
  const char* context_data[][2] = {
    {"", ""},
    {"var f =", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "new foo",
    "new foo();",
    "new foo(1);",
    "new foo(1, 2);",
    // The first () will be processed as a part of the NewExpression and the
    // second () will be processed as part of LeftHandSideExpression.
    "new foo()();",
    // The first () will be processed as a part of the inner NewExpression and
    // the second () will be processed as a part of the outer NewExpression.
    "new new foo()();",
    "new foo.bar;",
    "new foo.bar();",
    "new foo.bar.baz;",
    "new foo.bar().baz;",
    "new foo[bar];",
    "new foo[bar]();",
    "new foo[bar][baz];",
    "new foo[bar]()[baz];",
    "new foo[bar].baz(baz)()[bar].baz;",
    "new \"foo\"",  // Runtime error
    "new 1",  // Runtime error
    // This even runs:
    "(new new Function(\"this.x = 1\")).x;",
    "new new Test_Two(String, 2).v(0123).length;",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
}


TEST(ErrorsNewExpression) {
  const char* context_data[][2] = {
    {"", ""},
    {"var f =", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "new foo bar",
    "new ) foo",
    "new ++foo",
    "new foo ++",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(StrictObjectLiteralChecking) {
  const char* strict_context_data[][2] = {
    {"\"use strict\"; var myobject = {", "};"},
    {"\"use strict\"; var myobject = {", ",};"},
    { NULL, NULL }
  };
  const char* non_strict_context_data[][2] = {
    {"var myobject = {", "};"},
    {"var myobject = {", ",};"},
    { NULL, NULL }
  };

  // These are only errors in strict mode.
  const char* statement_data[] = {
    "foo: 1, foo: 2",
    "\"foo\": 1, \"foo\": 2",
    "foo: 1, \"foo\": 2",
    "1: 1, 1: 2",
    "1: 1, \"1\": 2",
    "get: 1, get: 2",  // Not a getter for real, just a property called get.
    "set: 1, set: 2",  // Not a setter for real, just a property called set.
    NULL
  };

  RunParserSyncTest(non_strict_context_data, statement_data, kSuccess);
  RunParserSyncTest(strict_context_data, statement_data, kError);
}


TEST(ErrorsObjectLiteralChecking) {
  const char* context_data[][2] = {
    {"\"use strict\"; var myobject = {", "};"},
    {"var myobject = {", "};"},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    ",",
    "foo: 1, get foo() {}",
    "foo: 1, set foo(v) {}",
    "\"foo\": 1, get \"foo\"() {}",
    "\"foo\": 1, set \"foo\"(v) {}",
    "1: 1, get 1() {}",
    "1: 1, set 1() {}",
    // It's counter-intuitive, but these collide too (even in classic
    // mode). Note that we can have "foo" and foo as properties in classic mode,
    // but we cannot have "foo" and get foo, or foo and get "foo".
    "foo: 1, get \"foo\"() {}",
    "foo: 1, set \"foo\"(v) {}",
    "\"foo\": 1, get foo() {}",
    "\"foo\": 1, set foo(v) {}",
    "1: 1, get \"1\"() {}",
    "1: 1, set \"1\"() {}",
    "\"1\": 1, get 1() {}"
    "\"1\": 1, set 1(v) {}"
    // Wrong number of parameters
    "get bar(x) {}",
    "get bar(x, y) {}",
    "set bar() {}",
    "set bar(x, y) {}",
    // Parsing FunctionLiteral for getter or setter fails
    "get foo( +",
    "get foo() \"error\"",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(NoErrorsObjectLiteralChecking) {
  const char* context_data[][2] = {
    {"var myobject = {", "};"},
    {"var myobject = {", ",};"},
    {"\"use strict\"; var myobject = {", "};"},
    {"\"use strict\"; var myobject = {", ",};"},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "foo: 1, bar: 2",
    "\"foo\": 1, \"bar\": 2",
    "1: 1, 2: 2",
    // Syntax: IdentifierName ':' AssignmentExpression
    "foo: bar = 5 + baz",
    // Syntax: 'get' PropertyName '(' ')' '{' FunctionBody '}'
    "get foo() {}",
    "get \"foo\"() {}",
    "get 1() {}",
    // Syntax: 'set' PropertyName '(' PropertySetParameterList ')'
    //     '{' FunctionBody '}'
    "set foo(v) {}",
    "set \"foo\"(v) {}",
    "set 1(v) {}",
    // Non-colliding getters and setters -> no errors
    "foo: 1, get bar() {}",
    "foo: 1, set bar(v) {}",
    "\"foo\": 1, get \"bar\"() {}",
    "\"foo\": 1, set \"bar\"(v) {}",
    "1: 1, get 2() {}",
    "1: 1, set 2(v) {}",
    // Keywords, future reserved and strict future reserved are also allowed as
    // property names.
    "if: 4",
    "interface: 5",
    "super: 6",
    "eval: 7",
    "arguments: 8",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
}


TEST(TooManyArguments) {
  const char* context_data[][2] = {
    {"foo(", "0)"},
    { NULL, NULL }
  };

  using v8::internal::Code;
  char statement[Code::kMaxArguments * 2 + 1];
  for (int i = 0; i < Code::kMaxArguments; ++i) {
    statement[2 * i] = '0';
    statement[2 * i + 1] = ',';
  }
  statement[Code::kMaxArguments * 2] = 0;

  const char* statement_data[] = {
    statement,
    NULL
  };

  // The test is quite slow, so run it with a reduced set of flags.
  static const ParserFlag empty_flags[] = {kAllowLazy};
  RunParserSyncTest(context_data, statement_data, kError, empty_flags, 1);
}


TEST(StrictDelete) {
  // "delete <Identifier>" is not allowed in strict mode.
  const char* strict_context_data[][2] = {
    {"\"use strict\"; ", ""},
    { NULL, NULL }
  };

  const char* sloppy_context_data[][2] = {
    {"", ""},
    { NULL, NULL }
  };

  // These are errors in the strict mode.
  const char* sloppy_statement_data[] = {
    "delete foo;",
    "delete foo + 1;",
    "delete (foo);",
    "delete eval;",
    "delete interface;",
    NULL
  };

  // These are always OK
  const char* good_statement_data[] = {
    "delete this;",
    "delete 1;",
    "delete 1 + 2;",
    "delete foo();",
    "delete foo.bar;",
    "delete foo[bar];",
    "delete foo--;",
    "delete --foo;",
    "delete new foo();",
    "delete new foo(bar);",
    NULL
  };

  // These are always errors
  const char* bad_statement_data[] = {
    "delete if;",
    NULL
  };

  RunParserSyncTest(strict_context_data, sloppy_statement_data, kError);
  RunParserSyncTest(sloppy_context_data, sloppy_statement_data, kSuccess);

  RunParserSyncTest(strict_context_data, good_statement_data, kSuccess);
  RunParserSyncTest(sloppy_context_data, good_statement_data, kSuccess);

  RunParserSyncTest(strict_context_data, bad_statement_data, kError);
  RunParserSyncTest(sloppy_context_data, bad_statement_data, kError);
}


TEST(InvalidLeftHandSide) {
  const char* assignment_context_data[][2] = {
    {"", " = 1;"},
    {"\"use strict\"; ", " = 1;"},
    { NULL, NULL }
  };

  const char* prefix_context_data[][2] = {
    {"++", ";"},
    {"\"use strict\"; ++", ";"},
    {NULL, NULL},
  };

  const char* postfix_context_data[][2] = {
    {"", "++;"},
    {"\"use strict\"; ", "++;"},
    { NULL, NULL }
  };

  // Good left hand sides for assigment or prefix / postfix operations.
  const char* good_statement_data[] = {
    "foo",
    "foo.bar",
    "foo[bar]",
    "foo()[bar]",
    "foo().bar",
    "this.foo",
    "this[foo]",
    "new foo()[bar]",
    "new foo().bar",
    "foo()",
    "foo(bar)",
    "foo[bar]()",
    "foo.bar()",
    "this()",
    "this.foo()",
    "this[foo].bar()",
    "this.foo[foo].bar(this)(bar)[foo]()",
    NULL
  };

  // Bad left hand sides for assigment or prefix / postfix operations.
  const char* bad_statement_data_common[] = {
    "2",
    "new foo",
    "new foo()",
    "null",
    "if",  // Unexpected token
    "{x: 1}",  // Unexpected token
    "this",
    "\"bar\"",
    "(foo + bar)",
    "new new foo()[bar]",  // means: new (new foo()[bar])
    "new new foo().bar",  // means: new (new foo()[bar])
    NULL
  };

  // These are not okay for assignment, but okay for prefix / postix.
  const char* bad_statement_data_for_assignment[] = {
    "++foo",
    "foo++",
    "foo + bar",
    NULL
  };

  RunParserSyncTest(assignment_context_data, good_statement_data, kSuccess);
  RunParserSyncTest(assignment_context_data, bad_statement_data_common, kError);
  RunParserSyncTest(assignment_context_data, bad_statement_data_for_assignment,
                    kError);

  RunParserSyncTest(prefix_context_data, good_statement_data, kSuccess);
  RunParserSyncTest(prefix_context_data, bad_statement_data_common, kError);

  RunParserSyncTest(postfix_context_data, good_statement_data, kSuccess);
  RunParserSyncTest(postfix_context_data, bad_statement_data_common, kError);
}


TEST(FuncNameInferrerBasic) {
  // Tests that function names are inferred properly.
  i::FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  CompileRun("var foo1 = function() {}; "
             "var foo2 = function foo3() {}; "
             "function not_ctor() { "
             "  var foo4 = function() {}; "
             "  return %FunctionGetInferredName(foo4); "
             "} "
             "function Ctor() { "
             "  var foo5 = function() {}; "
             "  return %FunctionGetInferredName(foo5); "
             "} "
             "var obj1 = { foo6: function() {} }; "
             "var obj2 = { 'foo7': function() {} }; "
             "var obj3 = {}; "
             "obj3[1] = function() {}; "
             "var obj4 = {}; "
             "obj4[1] = function foo8() {}; "
             "var obj5 = {}; "
             "obj5['foo9'] = function() {}; "
             "var obj6 = { obj7 : { foo10: function() {} } };");
  ExpectString("%FunctionGetInferredName(foo1)", "foo1");
  // foo2 is not unnamed -> its name is not inferred.
  ExpectString("%FunctionGetInferredName(foo2)", "");
  ExpectString("not_ctor()", "foo4");
  ExpectString("Ctor()", "Ctor.foo5");
  ExpectString("%FunctionGetInferredName(obj1.foo6)", "obj1.foo6");
  ExpectString("%FunctionGetInferredName(obj2.foo7)", "obj2.foo7");
  ExpectString("%FunctionGetInferredName(obj3[1])",
               "obj3.(anonymous function)");
  ExpectString("%FunctionGetInferredName(obj4[1])", "");
  ExpectString("%FunctionGetInferredName(obj5['foo9'])", "obj5.foo9");
  ExpectString("%FunctionGetInferredName(obj6.obj7.foo10)", "obj6.obj7.foo10");
}


TEST(FuncNameInferrerTwoByte) {
  // Tests function name inferring in cases where some parts of the inferred
  // function name are two-byte strings.
  i::FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  uint16_t* two_byte_source = AsciiToTwoByteString(
      "var obj1 = { oXj2 : { foo1: function() {} } }; "
      "%FunctionGetInferredName(obj1.oXj2.foo1)");
  uint16_t* two_byte_name = AsciiToTwoByteString("obj1.oXj2.foo1");
  // Make it really non-ASCII (replace the Xs with a non-ASCII character).
  two_byte_source[14] = two_byte_source[78] = two_byte_name[6] = 0x010d;
  v8::Local<v8::String> source =
      v8::String::NewFromTwoByte(isolate, two_byte_source);
  v8::Local<v8::Value> result = CompileRun(source);
  CHECK(result->IsString());
  v8::Local<v8::String> expected_name =
      v8::String::NewFromTwoByte(isolate, two_byte_name);
  CHECK(result->Equals(expected_name));
  i::DeleteArray(two_byte_source);
  i::DeleteArray(two_byte_name);
}


TEST(FuncNameInferrerEscaped) {
  // The same as FuncNameInferrerTwoByte, except that we express the two-byte
  // character as a unicode escape.
  i::FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  uint16_t* two_byte_source = AsciiToTwoByteString(
      "var obj1 = { o\\u010dj2 : { foo1: function() {} } }; "
      "%FunctionGetInferredName(obj1.o\\u010dj2.foo1)");
  uint16_t* two_byte_name = AsciiToTwoByteString("obj1.oXj2.foo1");
  // Fix to correspond to the non-ASCII name in two_byte_source.
  two_byte_name[6] = 0x010d;
  v8::Local<v8::String> source =
      v8::String::NewFromTwoByte(isolate, two_byte_source);
  v8::Local<v8::Value> result = CompileRun(source);
  CHECK(result->IsString());
  v8::Local<v8::String> expected_name =
      v8::String::NewFromTwoByte(isolate, two_byte_name);
  CHECK(result->Equals(expected_name));
  i::DeleteArray(two_byte_source);
  i::DeleteArray(two_byte_name);
}


TEST(RegressionLazyFunctionWithErrorWithArg) {
  // The bug occurred when a lazy function had an error which requires a
  // parameter (such as "unknown label" here). The error message was processed
  // before the AstValueFactory containing the error message string was
  // internalized.
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  i::FLAG_lazy = true;
  i::FLAG_min_preparse_length = 0;
  CompileRun("function this_is_lazy() {\n"
             "  break p;\n"
             "}\n"
             "this_is_lazy();\n");
}


TEST(SerializationOfMaybeAssignmentFlag) {
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  i::HandleScope scope(isolate);
  LocalContext env;

  const char* src =
      "function h() {"
      "  var result = [];"
      "  function f() {"
      "    result.push(2);"
      "  }"
      "  function assertResult(r) {"
      "    f();"
      "    result = [];"
      "  }"
      "  assertResult([2]);"
      "  assertResult([2]);"
      "  return f;"
      "};"
      "h();";

  i::ScopedVector<char> program(Utf8LengthHelper(src) + 1);
  i::SNPrintF(program, "%s", src);
  i::Handle<i::String> source = factory->InternalizeUtf8String(program.start());
  source->PrintOn(stdout);
  printf("\n");
  i::Zone zone(isolate);
  v8::Local<v8::Value> v = CompileRun(src);
  i::Handle<i::Object> o = v8::Utils::OpenHandle(*v);
  i::Handle<i::JSFunction> f = i::Handle<i::JSFunction>::cast(o);
  i::Context* context = f->context();
  i::AstValueFactory avf(&zone, isolate->heap()->HashSeed());
  avf.Internalize(isolate);
  const i::AstRawString* name = avf.GetOneByteString("result");
  i::Handle<i::String> str = name->string();
  CHECK(str->IsInternalizedString());
  i::Scope* global_scope =
      new (&zone) i::Scope(NULL, i::GLOBAL_SCOPE, &avf, &zone);
  global_scope->Initialize();
  i::Scope* s = i::Scope::DeserializeScopeChain(context, global_scope, &zone);
  DCHECK(s != global_scope);
  DCHECK(name != NULL);

  // Get result from h's function context (that is f's context)
  i::Variable* var = s->Lookup(name);

  CHECK(var != NULL);
  // Maybe assigned should survive deserialization
  CHECK(var->maybe_assigned() == i::kMaybeAssigned);
  // TODO(sigurds) Figure out if is_used should survive context serialization.
}


TEST(IfArgumentsArrayAccessedThenParametersMaybeAssigned) {
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  i::HandleScope scope(isolate);
  LocalContext env;


  const char* src =
      "function f(x) {"
      "    var a = arguments;"
      "    function g(i) {"
      "      ++a[0];"
      "    };"
      "    return g;"
      "  }"
      "f(0);";

  i::ScopedVector<char> program(Utf8LengthHelper(src) + 1);
  i::SNPrintF(program, "%s", src);
  i::Handle<i::String> source = factory->InternalizeUtf8String(program.start());
  source->PrintOn(stdout);
  printf("\n");
  i::Zone zone(isolate);
  v8::Local<v8::Value> v = CompileRun(src);
  i::Handle<i::Object> o = v8::Utils::OpenHandle(*v);
  i::Handle<i::JSFunction> f = i::Handle<i::JSFunction>::cast(o);
  i::Context* context = f->context();
  i::AstValueFactory avf(&zone, isolate->heap()->HashSeed());
  avf.Internalize(isolate);

  i::Scope* global_scope =
      new (&zone) i::Scope(NULL, i::GLOBAL_SCOPE, &avf, &zone);
  global_scope->Initialize();
  i::Scope* s = i::Scope::DeserializeScopeChain(context, global_scope, &zone);
  DCHECK(s != global_scope);
  const i::AstRawString* name_x = avf.GetOneByteString("x");

  // Get result from f's function context (that is g's outer context)
  i::Variable* var_x = s->Lookup(name_x);
  CHECK(var_x != NULL);
  CHECK(var_x->maybe_assigned() == i::kMaybeAssigned);
}


TEST(ExportsMaybeAssigned) {
  i::FLAG_use_strict = true;
  i::FLAG_harmony_scoping = true;
  i::FLAG_harmony_modules = true;

  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  i::HandleScope scope(isolate);
  LocalContext env;

  const char* src =
      "module A {"
      "  export var x = 1;"
      "  export function f() { return x };"
      "  export const y = 2;"
      "  module B {}"
      "  export module C {}"
      "};"
      "A.f";

  i::ScopedVector<char> program(Utf8LengthHelper(src) + 1);
  i::SNPrintF(program, "%s", src);
  i::Handle<i::String> source = factory->InternalizeUtf8String(program.start());
  source->PrintOn(stdout);
  printf("\n");
  i::Zone zone(isolate);
  v8::Local<v8::Value> v = CompileRun(src);
  i::Handle<i::Object> o = v8::Utils::OpenHandle(*v);
  i::Handle<i::JSFunction> f = i::Handle<i::JSFunction>::cast(o);
  i::Context* context = f->context();
  i::AstValueFactory avf(&zone, isolate->heap()->HashSeed());
  avf.Internalize(isolate);

  i::Scope* global_scope =
      new (&zone) i::Scope(NULL, i::GLOBAL_SCOPE, &avf, &zone);
  global_scope->Initialize();
  i::Scope* s = i::Scope::DeserializeScopeChain(context, global_scope, &zone);
  DCHECK(s != global_scope);
  const i::AstRawString* name_x = avf.GetOneByteString("x");
  const i::AstRawString* name_f = avf.GetOneByteString("f");
  const i::AstRawString* name_y = avf.GetOneByteString("y");
  const i::AstRawString* name_B = avf.GetOneByteString("B");
  const i::AstRawString* name_C = avf.GetOneByteString("C");

  // Get result from h's function context (that is f's context)
  i::Variable* var_x = s->Lookup(name_x);
  CHECK(var_x != NULL);
  CHECK(var_x->maybe_assigned() == i::kMaybeAssigned);
  i::Variable* var_f = s->Lookup(name_f);
  CHECK(var_f != NULL);
  CHECK(var_f->maybe_assigned() == i::kMaybeAssigned);
  i::Variable* var_y = s->Lookup(name_y);
  CHECK(var_y != NULL);
  CHECK(var_y->maybe_assigned() == i::kNotAssigned);
  i::Variable* var_B = s->Lookup(name_B);
  CHECK(var_B != NULL);
  CHECK(var_B->maybe_assigned() == i::kNotAssigned);
  i::Variable* var_C = s->Lookup(name_C);
  CHECK(var_C != NULL);
  CHECK(var_C->maybe_assigned() == i::kNotAssigned);
}


TEST(InnerAssignment) {
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  i::HandleScope scope(isolate);
  LocalContext env;

  const char* prefix = "function f() {";
  const char* midfix = " function g() {";
  const char* suffix = "}}";
  struct { const char* source; bool assigned; bool strict; } outers[] = {
    // Actual assignments.
    { "var x; var x = 5;", true, false },
    { "var x; { var x = 5; }", true, false },
    { "'use strict'; let x; x = 6;", true, true },
    { "var x = 5; function x() {}", true, false },
    // Actual non-assignments.
    { "var x;", false, false },
    { "var x = 5;", false, false },
    { "'use strict'; let x;", false, true },
    { "'use strict'; let x = 6;", false, true },
    { "'use strict'; var x = 0; { let x = 6; }", false, true },
    { "'use strict'; var x = 0; { let x; x = 6; }", false, true },
    { "'use strict'; let x = 0; { let x = 6; }", false, true },
    { "'use strict'; let x = 0; { let x; x = 6; }", false, true },
    { "var x; try {} catch (x) { x = 5; }", false, false },
    { "function x() {}", false, false },
    // Eval approximation.
    { "var x; eval('');", true, false },
    { "eval(''); var x;", true, false },
    { "'use strict'; let x; eval('');", true, true },
    { "'use strict'; eval(''); let x;", true, true },
    // Non-assignments not recognized, because the analysis is approximative.
    { "var x; var x;", true, false },
    { "var x = 5; var x;", true, false },
    { "var x; { var x; }", true, false },
    { "var x; function x() {}", true, false },
    { "function x() {}; var x;", true, false },
    { "var x; try {} catch (x) { var x = 5; }", true, false },
  };
  struct { const char* source; bool assigned; bool with; } inners[] = {
    // Actual assignments.
    { "x = 1;", true, false },
    { "x++;", true, false },
    { "++x;", true, false },
    { "x--;", true, false },
    { "--x;", true, false },
    { "{ x = 1; }", true, false },
    { "'use strict'; { let x; }; x = 0;", true, false },
    { "'use strict'; { const x = 1; }; x = 0;", true, false },
    { "'use strict'; { function x() {} }; x = 0;", true, false },
    { "with ({}) { x = 1; }", true, true },
    { "eval('');", true, false },
    { "'use strict'; { let y; eval('') }", true, false },
    { "function h() { x = 0; }", true, false },
    { "(function() { x = 0; })", true, false },
    { "(function() { x = 0; })", true, false },
    { "with ({}) (function() { x = 0; })", true, true },
    // Actual non-assignments.
    { "", false, false },
    { "x;", false, false },
    { "var x;", false, false },
    { "var x = 8;", false, false },
    { "var x; x = 8;", false, false },
    { "'use strict'; let x;", false, false },
    { "'use strict'; let x = 8;", false, false },
    { "'use strict'; let x; x = 8;", false, false },
    { "'use strict'; const x = 8;", false, false },
    { "function x() {}", false, false },
    { "function x() { x = 0; }", false, false },
    { "function h(x) { x = 0; }", false, false },
    { "'use strict'; { let x; x = 0; }", false, false },
    { "{ var x; }; x = 0;", false, false },
    { "with ({}) {}", false, true },
    { "var x; { with ({}) { x = 1; } }", false, true },
    { "try {} catch(x) { x = 0; }", false, false },
    { "try {} catch(x) { with ({}) { x = 1; } }", false, true },
    // Eval approximation.
    { "eval('');", true, false },
    { "function h() { eval(''); }", true, false },
    { "(function() { eval(''); })", true, false },
    // Shadowing not recognized because of eval approximation.
    { "var x; eval('');", true, false },
    { "'use strict'; let x; eval('');", true, false },
    { "try {} catch(x) { eval(''); }", true, false },
    { "function x() { eval(''); }", true, false },
    { "(function(x) { eval(''); })", true, false },
  };

  // Used to trigger lazy compilation of function
  int comment_len = 2048;
  i::ScopedVector<char> comment(comment_len + 1);
  i::SNPrintF(comment, "/*%0*d*/", comment_len - 4, 0);
  int prefix_len = Utf8LengthHelper(prefix);
  int midfix_len = Utf8LengthHelper(midfix);
  int suffix_len = Utf8LengthHelper(suffix);
  for (unsigned i = 0; i < ARRAY_SIZE(outers); ++i) {
    const char* outer = outers[i].source;
    int outer_len = Utf8LengthHelper(outer);
    for (unsigned j = 0; j < ARRAY_SIZE(inners); ++j) {
      for (unsigned outer_lazy = 0; outer_lazy < 2; ++outer_lazy) {
        for (unsigned inner_lazy = 0; inner_lazy < 2; ++inner_lazy) {
          if (outers[i].strict && inners[j].with) continue;
          const char* inner = inners[j].source;
          int inner_len = Utf8LengthHelper(inner);

          int outer_comment_len = outer_lazy ? comment_len : 0;
          int inner_comment_len = inner_lazy ? comment_len : 0;
          const char* outer_comment = outer_lazy ? comment.start() : "";
          const char* inner_comment = inner_lazy ? comment.start() : "";
          int len = prefix_len + outer_comment_len + outer_len + midfix_len +
                    inner_comment_len + inner_len + suffix_len;
          i::ScopedVector<char> program(len + 1);

          i::SNPrintF(program, "%s%s%s%s%s%s%s", prefix, outer_comment, outer,
                      midfix, inner_comment, inner, suffix);
          i::Handle<i::String> source =
              factory->InternalizeUtf8String(program.start());
          source->PrintOn(stdout);
          printf("\n");

          i::Handle<i::Script> script = factory->NewScript(source);
          i::CompilationInfoWithZone info(script);
          i::Parser parser(&info);
          parser.set_allow_harmony_scoping(true);
          CHECK(parser.Parse());
          CHECK(i::Rewriter::Rewrite(&info));
          CHECK(i::Scope::Analyze(&info));
          CHECK(info.function() != NULL);

          i::Scope* scope = info.function()->scope();
          CHECK_EQ(scope->inner_scopes()->length(), 1);
          i::Scope* inner_scope = scope->inner_scopes()->at(0);
          const i::AstRawString* var_name =
              info.ast_value_factory()->GetOneByteString("x");
          i::Variable* var = inner_scope->Lookup(var_name);
          bool expected = outers[i].assigned || inners[j].assigned;
          CHECK(var != NULL);
          CHECK(var->is_used() || !expected);
          CHECK((var->maybe_assigned() == i::kMaybeAssigned) == expected);
        }
      }
    }
  }
}

namespace {

int* global_use_counts = NULL;

void MockUseCounterCallback(v8::Isolate* isolate,
                            v8::Isolate::UseCounterFeature feature) {
  ++global_use_counts[feature];
}

}


TEST(UseAsmUseCount) {
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);
  LocalContext env;
  int use_counts[v8::Isolate::kUseCounterFeatureCount] = {};
  global_use_counts = use_counts;
  CcTest::isolate()->SetUseCounterCallback(MockUseCounterCallback);
  CompileRun("\"use asm\";\n"
             "var foo = 1;\n"
             "\"use asm\";\n"  // Only the first one counts.
             "function bar() { \"use asm\"; var baz = 1; }");
  CHECK_EQ(2, use_counts[v8::Isolate::kUseAsm]);
}


TEST(ErrorsArrowFunctions) {
  // Tests that parser and preparser generate the same kind of errors
  // on invalid arrow function syntax.
  const char* context_data[][2] = {
    {"", ";"},
    {"v = ", ";"},
    {"bar ? (", ") : baz;"},
    {"bar ? baz : (", ");"},
    {"bar[", "];"},
    {"bar, ", ";"},
    {"", ", bar;"},
    {NULL, NULL}
  };

  const char* statement_data[] = {
    "=> 0",
    "=>",
    "() =>",
    "=> {}",
    ") => {}",
    ", => {}",
    "(,) => {}",
    "return => {}",
    "() => {'value': 42}",

    // Check that the early return introduced in ParsePrimaryExpression
    // does not accept stray closing parentheses.
    ")",
    ") => 0",
    "foo[()]",
    "()",

    // Parameter lists with extra parens should be recognized as errors.
    "(()) => 0",
    "((x)) => 0",
    "((x, y)) => 0",
    "(x, (y)) => 0",
    "((x, y, z)) => 0",
    "(x, (y, z)) => 0",
    "((x, y), z) => 0",

    // Parameter lists are always validated as strict, so those are errors.
    "eval => {}",
    "arguments => {}",
    "yield => {}",
    "interface => {}",
    "(eval) => {}",
    "(arguments) => {}",
    "(yield) => {}",
    "(interface) => {}",
    "(eval, bar) => {}",
    "(bar, eval) => {}",
    "(bar, arguments) => {}",
    "(bar, yield) => {}",
    "(bar, interface) => {}",
    // TODO(aperez): Detecting duplicates does not work in PreParser.
    // "(bar, bar) => {}",

    // The parameter list is parsed as an expression, but only
    // a comma-separated list of identifier is valid.
    "32 => {}",
    "(32) => {}",
    "(a, 32) => {}",
    "if => {}",
    "(if) => {}",
    "(a, if) => {}",
    "a + b => {}",
    "(a + b) => {}",
    "(a + b, c) => {}",
    "(a, b - c) => {}",
    "\"a\" => {}",
    "(\"a\") => {}",
    "(\"a\", b) => {}",
    "(a, \"b\") => {}",
    "-a => {}",
    "(-a) => {}",
    "(-a, b) => {}",
    "(a, -b) => {}",
    "{} => {}",
    "({}) => {}",
    "(a, {}) => {}",
    "({}, a) => {}",
    "a++ => {}",
    "(a++) => {}",
    "(a++, b) => {}",
    "(a, b++) => {}",
    "[] => {}",
    "([]) => {}",
    "(a, []) => {}",
    "([], a) => {}",
    "(a = b) => {}",
    "(a = b, c) => {}",
    "(a, b = c) => {}",
    "(foo ? bar : baz) => {}",
    "(a, foo ? bar : baz) => {}",
    "(foo ? bar : baz, a) => {}",
    NULL
  };

  // The test is quite slow, so run it with a reduced set of flags.
  static const ParserFlag flags[] = {
    kAllowLazy, kAllowHarmonyScoping, kAllowGenerators
  };
  static const ParserFlag always_flags[] = { kAllowArrowFunctions };
  RunParserSyncTest(context_data, statement_data, kError, flags,
                    ARRAY_SIZE(flags), always_flags, ARRAY_SIZE(always_flags));
}


TEST(NoErrorsArrowFunctions) {
  // Tests that parser and preparser accept valid arrow functions syntax.
  const char* context_data[][2] = {
    {"", ";"},
    {"bar ? (", ") : baz;"},
    {"bar ? baz : (", ");"},
    {"bar, ", ";"},
    {"", ", bar;"},
    {NULL, NULL}
  };

  const char* statement_data[] = {
    "() => {}",
    "() => { return 42 }",
    "x => { return x; }",
    "(x) => { return x; }",
    "(x, y) => { return x + y; }",
    "(x, y, z) => { return x + y + z; }",
    "(x, y) => { x.a = y; }",
    "() => 42",
    "x => x",
    "x => x * x",
    "(x) => x",
    "(x) => x * x",
    "(x, y) => x + y",
    "(x, y, z) => x, y, z",
    "(x, y) => x.a = y",
    "() => ({'value': 42})",
    "x => y => x + y",
    "(x, y) => (u, v) => x*u + y*v",
    "(x, y) => z => z * (x + y)",
    "x => (y, z) => z * (x + y)",

    // Those are comma-separated expressions, with arrow functions as items.
    // They stress the code for validating arrow function parameter lists.
    "a, b => 0",
    "a, b, (c, d) => 0",
    "(a, b, (c, d) => 0)",
    "(a, b) => 0, (c, d) => 1",
    "(a, b => {}, a => a + 1)",
    "((a, b) => {}, (a => a + 1))",
    "(a, (a, (b, c) => 0))",

    // Arrow has more precedence, this is the same as: foo ? bar : (baz = {})
    "foo ? bar : baz => {}",
    NULL
  };

  static const ParserFlag always_flags[] = {kAllowArrowFunctions};
  RunParserSyncTest(context_data, statement_data, kSuccess, NULL, 0,
                    always_flags, ARRAY_SIZE(always_flags));
}
