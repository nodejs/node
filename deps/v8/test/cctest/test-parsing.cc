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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "v8.h"

#include "cctest.h"
#include "compiler.h"
#include "execution.h"
#include "isolate.h"
#include "objects.h"
#include "parser.h"
#include "preparser.h"
#include "scanner-character-streams.h"
#include "token.h"
#include "utils.h"

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
      i::OS::MemMove(buffer, keyword, length);
      buffer[length] = chars_to_append[j];
      i::Utf8ToUtf16CharacterStream stream(buffer, length + 1);
      i::Scanner scanner(&unicode_cache);
      scanner.Initialize(&stream);
      CHECK_EQ(i::Token::IDENTIFIER, scanner.Next());
      CHECK_EQ(i::Token::EOS, scanner.Next());
    }
    // Replacing characters will make keyword matching fail.
    {
      i::OS::MemMove(buffer, keyword, length);
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
  int marker;
  CcTest::i_isolate()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  for (int i = 0; tests[i]; i++) {
    v8::Handle<v8::String> source = v8::String::NewFromUtf8(
        isolate, tests[i], v8::String::kNormalString, i::StrLength(tests[i]));
    v8::ScriptData* data = v8::ScriptData::PreCompile(source);
    CHECK(data != NULL && !data->HasError());
    delete data;
  }

  for (int i = 0; fail_tests[i]; i++) {
    v8::Handle<v8::String> source =
        v8::String::NewFromUtf8(isolate,
                                fail_tests[i],
                                v8::String::kNormalString,
                                i::StrLength(fail_tests[i]));
    v8::ScriptData* data = v8::ScriptData::PreCompile(source);
    CHECK(data == NULL || data->HasError());
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
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handles(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  int marker;
  CcTest::i_isolate()->stack_guard()->SetStackLimit(
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

  v8::ScriptData* preparse = v8::ScriptData::PreCompile(v8::String::NewFromUtf8(
      isolate, source, v8::String::kNormalString, source_length));
  CHECK(!preparse->HasError());
  bool lazy_flag = i::FLAG_lazy;
  {
    i::FLAG_lazy = true;
    ScriptResource* resource = new ScriptResource(source, source_length);
    v8::ScriptCompiler::Source script_source(
        v8::String::NewExternal(isolate, resource),
        new v8::ScriptCompiler::CachedData(
            reinterpret_cast<const uint8_t*>(preparse->Data()),
            preparse->Length()));
    v8::ScriptCompiler::Compile(isolate,
                                &script_source);
  }

  {
    i::FLAG_lazy = false;

    ScriptResource* resource = new ScriptResource(source, source_length);
    v8::ScriptCompiler::Source script_source(
        v8::String::NewExternal(isolate, resource),
        new v8::ScriptCompiler::CachedData(
            reinterpret_cast<const uint8_t*>(preparse->Data()),
            preparse->Length()));
    v8::ScriptCompiler::CompileUnbound(isolate, &script_source);
  }
  delete preparse;
  i::FLAG_lazy = lazy_flag;

  // Syntax error.
  v8::ScriptData* error_preparse = v8::ScriptData::PreCompile(
      v8::String::NewFromUtf8(isolate,
                              error_source,
                              v8::String::kNormalString,
                              error_source_length));
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
  args.Dispose();
  i::DeleteArray(message);
  delete error_preparse;
}


TEST(PreparseFunctionDataIsUsed) {
  // This tests that we actually do use the function data generated by the
  // preparser.

  // Make preparsing work for short scripts.
  i::FLAG_min_preparse_length = 0;

  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handles(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  int marker;
  CcTest::i_isolate()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  const char* good_code =
      "function this_is_lazy() { var a; } function foo() { return 25; } foo();";

  // Insert a syntax error inside the lazy function.
  const char* bad_code =
      "function this_is_lazy() { if (   } function foo() { return 25; } foo();";

  v8::ScriptCompiler::Source good_source(v8_str(good_code));
  v8::ScriptCompiler::Compile(isolate, &good_source,
                              v8::ScriptCompiler::kProduceDataToCache);

  const v8::ScriptCompiler::CachedData* cached_data =
      good_source.GetCachedData();
  CHECK(cached_data->data != NULL);
  CHECK_GT(cached_data->length, 0);

  // Now compile the erroneous code with the good preparse data. If the preparse
  // data is used, the lazy function is skipped and it should compile fine.
  v8::ScriptCompiler::Source bad_source(
      v8_str(bad_code), new v8::ScriptCompiler::CachedData(
                            cached_data->data, cached_data->length));
  v8::Local<v8::Value> result =
      v8::ScriptCompiler::Compile(isolate, &bad_source)->Run();
  CHECK(result->IsInt32());
  CHECK_EQ(25, result->Int32Value());
}


TEST(PreparseSymbolDataIsUsed) {
  // This tests that we actually do use the symbol data generated by the
  // preparser.

  // Only do one compilation pass in this test (otherwise we will parse the
  // source code again without preparse data and it will fail).
  i::FLAG_crankshaft = false;

  // Make preparsing work for short scripts.
  i::FLAG_min_preparse_length = 0;

  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handles(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  int marker;
  CcTest::i_isolate()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  // Note that the ( before function makes the function not lazily compiled.
  const char* good_code =
      "(function weird() { var foo = 26; return foo; })()";

  // Insert an undefined identifier. If the preparser data is used, the symbol
  // stream is used instead, and this identifier resolves to "foo".
  const char* bad_code =
      "(function weird() { var foo = 26; return wut; })()";

  v8::ScriptCompiler::Source good_source(v8_str(good_code));
  v8::ScriptCompiler::Compile(isolate, &good_source,
                              v8::ScriptCompiler::kProduceDataToCache);

  const v8::ScriptCompiler::CachedData* cached_data =
      good_source.GetCachedData();
  CHECK(cached_data->data != NULL);
  CHECK_GT(cached_data->length, 0);

  // Now compile the erroneous code with the good preparse data. If the preparse
  // data is used, we will see a second occurrence of "foo" instead of the
  // unknown "wut".
  v8::ScriptCompiler::Source bad_source(
      v8_str(bad_code), new v8::ScriptCompiler::CachedData(
                            cached_data->data, cached_data->length));
  v8::Local<v8::Value> result =
      v8::ScriptCompiler::Compile(isolate, &bad_source)->Run();
  CHECK(result->IsInt32());
  CHECK_EQ(26, result->Int32Value());
}


TEST(StandAlonePreParser) {
  v8::V8::Initialize();

  int marker;
  CcTest::i_isolate()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  const char* programs[] = {
      "{label: 42}",
      "var x = 42;",
      "function foo(x, y) { return x + y; }",
      "%ArgleBargle(glop);",
      "var x = new new Function('this.x = 42');",
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
    i::PreParser::PreParseResult result = preparser.PreParseProgram();
    CHECK_EQ(i::PreParser::kPreParseSuccess, result);
    i::ScriptDataImpl data(log.ExtractData());
    CHECK(!data.has_error());
  }
}


TEST(StandAlonePreParserNoNatives) {
  v8::V8::Initialize();

  int marker;
  CcTest::i_isolate()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

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
    i::ScriptDataImpl data(log.ExtractData());
    // Data contains syntax error.
    CHECK(data.has_error());
  }
}


TEST(PreparsingObjectLiterals) {
  // Regression test for a bug where the symbol stream produced by PreParser
  // didn't match what Parser wanted to consume.
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handles(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  int marker;
  CcTest::i_isolate()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  {
    const char* source = "var myo = {if: \"foo\"}; myo.if;";
    v8::Local<v8::Value> result = PreCompileCompileRun(source);
    CHECK(result->IsString());
    v8::String::Utf8Value utf8(result);
    CHECK_EQ("foo", *utf8);
  }

  {
    const char* source = "var myo = {\"bar\": \"foo\"}; myo[\"bar\"];";
    v8::Local<v8::Value> result = PreCompileCompileRun(source);
    CHECK(result->IsString());
    v8::String::Utf8Value utf8(result);
    CHECK_EQ("foo", *utf8);
  }

  {
    const char* source = "var myo = {1: \"foo\"}; myo[1];";
    v8::Local<v8::Value> result = PreCompileCompileRun(source);
    CHECK(result->IsString());
    v8::String::Utf8Value utf8(result);
    CHECK_EQ("foo", *utf8);
  }
}

namespace v8 {
namespace internal {

struct CompleteParserRecorderFriend {
  static void FakeWritingSymbolIdInPreParseData(CompleteParserRecorder* log,
                                                int number) {
    log->WriteNumber(number);
    if (log->symbol_id_ < number + 1) {
      log->symbol_id_ = number + 1;
    }
  }
  static int symbol_position(CompleteParserRecorder* log) {
    return log->symbol_store_.size();
  }
  static int symbol_ids(CompleteParserRecorder* log) {
    return log->symbol_id_;
  }
  static int function_position(CompleteParserRecorder* log) {
    return log->function_store_.size();
  }
};

}
}


TEST(StoringNumbersInPreParseData) {
  // Symbol IDs are split into chunks of 7 bits for storing. This is a
  // regression test for a bug where a symbol id was incorrectly stored if some
  // of the chunks in the middle were all zeros.
  typedef i::CompleteParserRecorderFriend F;
  i::CompleteParserRecorder log;
  for (int i = 0; i < 18; ++i) {
    F::FakeWritingSymbolIdInPreParseData(&log, 1 << i);
  }
  for (int i = 1; i < 18; ++i) {
    F::FakeWritingSymbolIdInPreParseData(&log, (1 << i) + 1);
  }
  for (int i = 6; i < 18; ++i) {
    F::FakeWritingSymbolIdInPreParseData(&log, (3 << i) + (5 << (i - 6)));
  }
  i::Vector<unsigned> store = log.ExtractData();
  i::ScriptDataImpl script_data(store);
  script_data.Initialize();
  // Check that we get the same symbols back.
  for (int i = 0; i < 18; ++i) {
    CHECK_EQ(1 << i, script_data.GetSymbolIdentifier());
  }
  for (int i = 1; i < 18; ++i) {
    CHECK_EQ((1 << i) + 1, script_data.GetSymbolIdentifier());
  }
  for (int i = 6; i < 18; ++i) {
    CHECK_EQ((3 << i) + (5 << (i - 6)), script_data.GetSymbolIdentifier());
  }
}


TEST(RegressChromium62639) {
  v8::V8::Initialize();
  i::Isolate* isolate = CcTest::i_isolate();

  int marker;
  isolate->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  const char* program = "var x = 'something';\n"
                        "escape: function() {}";
  // Fails parsing expecting an identifier after "function".
  // Before fix, didn't check *ok after Expect(Token::Identifier, ok),
  // and then used the invalid currently scanned literal. This always
  // failed in debug mode, and sometimes crashed in release mode.

  i::Utf8ToUtf16CharacterStream stream(
      reinterpret_cast<const i::byte*>(program),
      static_cast<unsigned>(strlen(program)));
  i::ScriptDataImpl* data = i::PreParserApi::PreParse(isolate, &stream);
  CHECK(data->HasError());
  delete data;
}


TEST(Regress928) {
  v8::V8::Initialize();
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();

  // Preparsing didn't consider the catch clause of a try statement
  // as with-content, which made it assume that a function inside
  // the block could be lazily compiled, and an extra, unexpected,
  // entry was added to the data.
  int marker;
  isolate->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  const char* program =
      "try { } catch (e) { var foo = function () { /* first */ } }"
      "var bar = function () { /* second */ }";

  v8::HandleScope handles(CcTest::isolate());
  i::Handle<i::String> source(
      factory->NewStringFromAscii(i::CStrVector(program)));
  i::GenericStringUtf16CharacterStream stream(source, 0, source->length());
  i::ScriptDataImpl* data = i::PreParserApi::PreParse(isolate, &stream);
  CHECK(!data->HasError());

  data->Initialize();

  int first_function =
      static_cast<int>(strstr(program, "function") - program);
  int first_lbrace = first_function + i::StrLength("function () ");
  CHECK_EQ('{', program[first_lbrace]);
  i::FunctionEntry entry1 = data->GetFunctionEntry(first_lbrace);
  CHECK(!entry1.is_valid());

  int second_function =
      static_cast<int>(strstr(program + first_lbrace, "function") - program);
  int second_lbrace =
      second_function + i::StrLength("function () ");
  CHECK_EQ('{', program[second_lbrace]);
  i::FunctionEntry entry2 = data->GetFunctionEntry(second_lbrace);
  CHECK(entry2.is_valid());
  CHECK_EQ('}', program[entry2.end_pos() - 1]);
  delete data;
}


TEST(PreParseOverflow) {
  v8::V8::Initialize();

  int marker;
  CcTest::i_isolate()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

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
  i::Handle<i::String> ascii_string(
      factory->NewStringFromAscii(ascii_vector));
  TestExternalResource resource(uc16_buffer.get(), length);
  i::Handle<i::String> uc16_string(
      factory->NewExternalStringFromTwoByte(&resource));

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
  ASSERT(cursor == kAllUtf8CharsSizeU);

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
  i::Handle<i::String> val =
      scanner.AllocateInternalizedString(CcTest::i_isolate());
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
    { "  (function fun", "(a,b) { infunction; }", ")();",
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

  int marker;
  isolate->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

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
    i::OS::SNPrintF(program, "%s%s%s",
                             source_data[i].outer_prefix,
                             source_data[i].inner_source,
                             source_data[i].outer_suffix);

    // Parse program source.
    i::Handle<i::String> source(
        factory->NewStringFromUtf8(i::CStrVector(program.start())));
    CHECK_EQ(source->length(), kProgramSize);
    i::Handle<i::Script> script = factory->NewScript(source);
    i::CompilationInfoWithZone info(script);
    i::Parser parser(&info);
    parser.set_allow_lazy(true);
    parser.set_allow_harmony_scoping(true);
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


i::Handle<i::String> FormatMessage(i::ScriptDataImpl* data) {
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  const char* message = data->BuildMessage();
  i::Handle<i::String> format = v8::Utils::OpenHandle(
      *v8::String::NewFromUtf8(CcTest::isolate(), message));
  i::Vector<const char*> args = data->BuildArgs();
  i::Handle<i::JSArray> args_array = factory->NewJSArray(args.length());
  for (int i = 0; i < args.length(); i++) {
    i::JSArray::SetElement(
        args_array, i, v8::Utils::OpenHandle(*v8::String::NewFromUtf8(
                                                  CcTest::isolate(), args[i])),
        NONE, i::SLOPPY);
  }
  i::Handle<i::JSObject> builtins(isolate->js_builtins_object());
  i::Handle<i::Object> format_fun =
      i::GetProperty(builtins, "FormatMessage");
  i::Handle<i::Object> arg_handles[] = { format, args_array };
  bool has_exception = false;
  i::Handle<i::Object> result = i::Execution::Call(
      isolate, format_fun, builtins, 2, arg_handles, &has_exception);
  CHECK(!has_exception);
  CHECK(result->IsString());
  for (int i = 0; i < args.length(); i++) {
    i::DeleteArray(args[i]);
  }
  i::DeleteArray(args.start());
  i::DeleteArray(message);
  return i::Handle<i::String>::cast(result);
}


enum ParserFlag {
  kAllowLazy,
  kAllowNativesSyntax,
  kAllowHarmonyScoping,
  kAllowModules,
  kAllowGenerators,
  kAllowForOf,
  kAllowHarmonyNumericLiterals
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
  parser->set_allow_for_of(flags.Contains(kAllowForOf));
  parser->set_allow_harmony_numeric_literals(
      flags.Contains(kAllowHarmonyNumericLiterals));
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
  i::ScriptDataImpl data(log.ExtractData());

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
    i::MaybeObject* maybe_object = isolate->pending_exception();
    i::JSObject* exception = NULL;
    CHECK(maybe_object->To(&exception));
    i::Handle<i::JSObject> exception_handle(exception);
    i::Handle<i::String> message_string =
        i::Handle<i::String>::cast(i::GetProperty(exception_handle, "message"));

    if (result == kSuccess) {
      i::OS::Print(
          "Parser failed on:\n"
          "\t%s\n"
          "with error:\n"
          "\t%s\n"
          "However, we expected no error.",
          source->ToCString().get(), message_string->ToCString().get());
      CHECK(false);
    }

    if (!data.has_error()) {
      i::OS::Print(
          "Parser failed on:\n"
          "\t%s\n"
          "with error:\n"
          "\t%s\n"
          "However, the preparser succeeded",
          source->ToCString().get(), message_string->ToCString().get());
      CHECK(false);
    }
    // Check that preparser and parser produce the same error.
    i::Handle<i::String> preparser_message = FormatMessage(&data);
    if (!message_string->Equals(*preparser_message)) {
      i::OS::Print(
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
  } else if (data.has_error()) {
    i::OS::Print(
        "Preparser failed on:\n"
        "\t%s\n"
        "with error:\n"
        "\t%s\n"
        "However, the parser succeeded",
        source->ToCString().get(), FormatMessage(&data)->ToCString().get());
    CHECK(false);
  } else if (result == kError) {
    i::OS::Print(
        "Expected error on:\n"
        "\t%s\n"
        "However, parser and preparser succeeded",
        source->ToCString().get());
    CHECK(false);
  }
}


void TestParserSync(const char* source,
                    const ParserFlag* flag_list,
                    size_t flag_list_length,
                    ParserSyncTestResult result = kSuccessOrError) {
  i::Handle<i::String> str =
      CcTest::i_isolate()->factory()->NewStringFromAscii(i::CStrVector(source));
  for (int bits = 0; bits < (1 << flag_list_length); bits++) {
    i::EnumSet<ParserFlag> flags;
    for (size_t flag_index = 0; flag_index < flag_list_length; flag_index++) {
      if ((bits & (1 << flag_index)) != 0) flags.Add(flag_list[flag_index]);
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
    "return",
    "return  12",
    "return\n12",
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

  int marker;
  CcTest::i_isolate()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  static const ParserFlag flags1[] = {
    kAllowLazy, kAllowHarmonyScoping, kAllowModules, kAllowGenerators,
    kAllowForOf
  };
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
        int length = i::OS::SNPrintF(program,
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


TEST(PreparserStrictOctal) {
  // Test that syntax error caused by octal literal is reported correctly as
  // such (issue 2220).
  v8::internal::FLAG_min_preparse_length = 1;  // Force preparsing.
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
                       int flags_len = 0) {
  v8::HandleScope handles(CcTest::isolate());
  v8::Handle<v8::Context> context = v8::Context::New(CcTest::isolate());
  v8::Context::Scope context_scope(context);

  int marker;
  CcTest::i_isolate()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  static const ParserFlag default_flags[] = {
    kAllowLazy, kAllowHarmonyScoping, kAllowModules, kAllowGenerators,
    kAllowForOf, kAllowNativesSyntax
  };
  if (!flags) {
    flags = default_flags;
    flags_len = ARRAY_SIZE(default_flags);
  }
  for (int i = 0; context_data[i][0] != NULL; ++i) {
    for (int j = 0; statement_data[j] != NULL; ++j) {
      int kPrefixLen = i::StrLength(context_data[i][0]);
      int kStatementLen = i::StrLength(statement_data[j]);
      int kSuffixLen = i::StrLength(context_data[i][1]);
      int kProgramSize = kPrefixLen + kStatementLen + kSuffixLen;

      // Plug the source code pieces together.
      i::ScopedVector<char> program(kProgramSize + 1);
      int length = i::OS::SNPrintF(program,
                                   "%s%s%s",
                                   context_data[i][0],
                                   statement_data[j],
                                   context_data[i][1]);
      CHECK(length == kProgramSize);
      TestParserSync(program.start(),
                     flags,
                     flags_len,
                     result);
    }
  }
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

  RunParserSyncTest(context_data, statement_data, kSuccess);
}


TEST(ErrorsFutureStrictReservedWords) {
  // Tests that both preparsing and parsing produce the right kind of errors for
  // using future strict reserved words as identifiers. Without the strict mode,
  // it's ok to use future strict reserved words as identifiers. With the strict
  // mode, it isn't.
  const char* context_data[][2] = {
    { "\"use strict\";", "" },
    { "function test_func() {\"use strict\"; ", "}"},
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
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(NoErrorsFutureStrictReservedWords) {
  const char* context_data[][2] = {
    { "", "" },
    { "function test_func() {", "}"},
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
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
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
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var super;",
    "var foo, super;",
    "try { } catch (super) { }",
    "function super() { }",
    "function foo(super) { }",
    "function foo(bar, super) { }",
    "super = 1;",
    "var foo = super = 1;",
    "++super;",
    "super++;",
    "function foo super",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(NoErrorsYieldSloppy) {
  // In sloppy mode, it's okay to use "yield" as identifier, *except* inside a
  // generator (see next test).
  const char* context_data[][2] = {
    { "", "" },
    { "function is_not_gen() {", "}" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var yield;",
    "var foo, yield;",
    "try { } catch (yield) { }",
    "function yield() { }",
    "function foo(yield) { }",
    "function foo(bar, yield) { }",
    "yield = 1;",
    "var foo = yield = 1;",
    "++yield;",
    "yield++;",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
}


TEST(ErrorsYieldSloppyGenerator) {
  const char* context_data[][2] = {
    { "function * is_gen() {", "}" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var yield;",
    "var foo, yield;",
    "try { } catch (yield) { }",
    "function yield() { }",
    // BUG: These should not be allowed, but they are (if kAllowGenerators is
    // set)
    // "function foo(yield) { }",
    // "function foo(bar, yield) { }",
    "yield = 1;",
    "var foo = yield = 1;",
    "++yield;",
    "yield++;",
    NULL
  };

  // If generators are not allowed, the error will be produced at the '*' token,
  // so this test works both with and without the kAllowGenerators flag.
  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(ErrorsYieldStrict) {
  const char* context_data[][2] = {
    { "\"use strict\";", "" },
    { "\"use strict\"; function is_not_gen() {", "}" },
    { "function test_func() {\"use strict\"; ", "}"},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "var yield;",
    "var foo, yield;",
    "try { } catch (yield) { }",
    "function yield() { }",
    "function foo(yield) { }",
    "function foo(bar, yield) { }",
    "yield = 1;",
    "var foo = yield = 1;",
    "++yield;",
    "yield++;",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(ErrorsYield) {
  const char* context_data[][2] = {
    { "function * is_gen() {", "}" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "yield 2;",  // this is legal inside generator
    "yield * 2;",  // this is legal inside generator
    NULL
  };

  // Here we cannot assert that there is no error, since there will be without
  // the kAllowGenerators flag. However, we test that Parser and PreParser
  // produce the same errors.
  RunParserSyncTest(context_data, statement_data, kSuccessOrError);
}


TEST(ErrorsNameOfStrictFunction) {
  // Tests that illegal tokens as names of a strict function produce the correct
  // errors.
  const char* context_data[][2] = {
    { "", ""},
    { "\"use strict\";", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "function eval() {\"use strict\";}",
    "function arguments() {\"use strict\";}",
    "function interface() {\"use strict\";}",
    "function yield() {\"use strict\";}",
    // Future reserved words are always illegal
    "function super() { }",
    "function super() {\"use strict\";}",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kError);
}


TEST(NoErrorsNameOfStrictFunction) {
  const char* context_data[][2] = {
    { "", ""},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "function eval() { }",
    "function arguments() { }",
    "function interface() { }",
    "function yield() { }",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
}



TEST(ErrorsIllegalWordsAsLabelsSloppy) {
  // Using future reserved words as labels is always an error.
  const char* context_data[][2] = {
    { "", ""},
    { "function test_func() {", "}" },
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
    { "\"use strict\";", "" },
    { "\"use strict\"; function test_func() {", "}" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "mylabel: while(true) { break mylabel; }",
    "eval: while(true) { break eval; }",
    "arguments: while(true) { break arguments; }",
    NULL
  };

  RunParserSyncTest(context_data, statement_data, kSuccess);
}


TEST(ErrorsParenthesizedLabels) {
  // Parenthesized identifiers shouldn't be recognized as labels.
  const char* context_data[][2] = {
    { "", ""},
    { "function test_func() {", "}" },
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
  // These tests make sure that PreParser doesn't start producing less data.

  v8::V8::Initialize();
  int marker;
  CcTest::i_isolate()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  struct TestCase {
    const char* program;
    int symbols;
    int functions;
  } test_cases[] = {
    // Labels and variables are recorded as symbols.
    {"{label: 42}", 1, 0}, {"{label: 42; label2: 43}", 2, 0},
    {"var x = 42;", 1, 0}, {"var x = 42, y = 43;", 2, 0},
    {"var x = {y: 1};", 2, 0},
    {"var x = {}; x.y = 1", 2, 0},
    // "get" is recorded as a symbol too.
    {"var x = {get foo(){} };", 3, 1},
    // When keywords are used as identifiers, they're logged as symbols, too:
    {"var x = {if: 1};", 2, 0},
    {"var x = {}; x.if = 1", 2, 0},
    {"var x = {get if(){} };", 3, 1},
    // Functions
    {"function foo() {}", 1, 1}, {"function foo() {} function bar() {}", 2, 2},
    // Labels, variables and functions insize lazy functions are not recorded.
    {"function lazy() { var a, b, c; }", 1, 1},
    {"function lazy() { a: 1; b: 2; c: 3; }", 1, 1},
    {"function lazy() { function a() {} function b() {} function c() {} }", 1,
     1},
    {NULL, 0, 0}
  };
  // Each function adds 5 elements to the preparse function data.
  const int kDataPerFunction = 5;

  typedef i::CompleteParserRecorderFriend F;
  uintptr_t stack_limit = CcTest::i_isolate()->stack_guard()->real_climit();
  for (int i = 0; test_cases[i].program; i++) {
    const char* program = test_cases[i].program;
    i::Utf8ToUtf16CharacterStream stream(
        reinterpret_cast<const i::byte*>(program),
        static_cast<unsigned>(strlen(program)));
    i::CompleteParserRecorder log;
    i::Scanner scanner(CcTest::i_isolate()->unicode_cache());
    scanner.Initialize(&stream);

    i::PreParser preparser(&scanner, &log, stack_limit);
    preparser.set_allow_lazy(true);
    preparser.set_allow_natives_syntax(true);
    i::PreParser::PreParseResult result = preparser.PreParseProgram();
    CHECK_EQ(i::PreParser::kPreParseSuccess, result);
    if (F::symbol_ids(&log) != test_cases[i].symbols) {
      i::OS::Print(
          "Expected preparse data for program:\n"
          "\t%s\n"
          "to contain %d symbols, however, received %d symbols.\n",
          program, test_cases[i].symbols, F::symbol_ids(&log));
      CHECK(false);
    }
    if (F::function_position(&log) !=
          test_cases[i].functions * kDataPerFunction) {
      i::OS::Print(
          "Expected preparse data for program:\n"
          "\t%s\n"
          "to contain %d functions, however, received %d functions.\n",
          program, test_cases[i].functions,
          F::function_position(&log) / kDataPerFunction);
      CHECK(false);
    }
    i::ScriptDataImpl data(log.ExtractData());
    CHECK(!data.has_error());
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

  // Parsing will fail or succeed depending on whether we allow natives syntax
  // or not.
  RunParserSyncTest(context_data, statement_data, kSuccessOrError);
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
    { NULL, NULL }
  };
  const char* non_strict_context_data[][2] = {
    {"var myobject = {", "};"},
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
    "foo: 1, get foo() {}",
    "foo: 1, set foo() {}",
    "\"foo\": 1, get \"foo\"() {}",
    "\"foo\": 1, set \"foo\"() {}",
    "1: 1, get 1() {}",
    "1: 1, set 1() {}",
    // It's counter-intuitive, but these collide too (even in classic
    // mode). Note that we can have "foo" and foo as properties in classic mode,
    // but we cannot have "foo" and get foo, or foo and get "foo".
    "foo: 1, get \"foo\"() {}",
    "foo: 1, set \"foo\"() {}",
    "\"foo\": 1, get foo() {}",
    "\"foo\": 1, set foo() {}",
    "1: 1, get \"1\"() {}",
    "1: 1, set \"1\"() {}",
    "\"1\": 1, get 1() {}"
    "\"1\": 1, set 1() {}"
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
    {"\"use strict\"; var myobject = {", "};"},
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "foo: 1, bar: 2",
    "\"foo\": 1, \"bar\": 2",
    "1: 1, 2: 2",
    // Syntax: IdentifierName ':' AssignmentExpression
    "foo: bar = 5 + baz",
    // Syntax: 'get' (IdentifierName | String | Number) FunctionLiteral
    "get foo() {}",
    "get \"foo\"() {}",
    "get 1() {}",
    // Syntax: 'set' (IdentifierName | String | Number) FunctionLiteral
    "set foo() {}",
    "set \"foo\"() {}",
    "set 1() {}",
    // Non-colliding getters and setters -> no errors
    "foo: 1, get bar() {}",
    "foo: 1, set bar(b) {}",
    "\"foo\": 1, get \"bar\"() {}",
    "\"foo\": 1, set \"bar\"() {}",
    "1: 1, get 2() {}",
    "1: 1, set 2() {}",
    // Weird number of parameters -> no errors
    "get bar() {}, set bar() {}",
    "get bar(x) {}, set bar(x) {}",
    "get bar(x, y) {}, set bar(x, y) {}",
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
    NULL
  };

  // Bad left hand sides for assigment or prefix / postfix operations.
  const char* bad_statement_data_common[] = {
    "2",
    "foo()",
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
