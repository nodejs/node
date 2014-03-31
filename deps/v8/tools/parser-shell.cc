// Copyright 2014 the V8 project authors. All rights reserved.
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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include "v8.h"

#include "api.h"
#include "compiler.h"
#include "scanner-character-streams.h"
#include "shell-utils.h"
#include "parser.h"
#include "preparse-data-format.h"
#include "preparse-data.h"
#include "preparser.h"

using namespace v8::internal;

enum TestMode {
  PreParseAndParse,
  PreParse,
  Parse
};

std::pair<TimeDelta, TimeDelta> RunBaselineParser(
    const char* fname, Encoding encoding, int repeat, v8::Isolate* isolate,
    v8::Handle<v8::Context> context, TestMode test_mode) {
  int length = 0;
  const byte* source = ReadFileAndRepeat(fname, &length, repeat);
  v8::Handle<v8::String> source_handle;
  switch (encoding) {
    case UTF8: {
      source_handle = v8::String::NewFromUtf8(
          isolate, reinterpret_cast<const char*>(source));
      break;
    }
    case UTF16: {
      source_handle = v8::String::NewFromTwoByte(
          isolate, reinterpret_cast<const uint16_t*>(source),
          v8::String::kNormalString, length / 2);
      break;
    }
    case LATIN1: {
      source_handle = v8::String::NewFromOneByte(isolate, source);
      break;
    }
  }
  v8::ScriptData* cached_data = NULL;
  TimeDelta preparse_time, parse_time;
  if (test_mode == PreParseAndParse || test_mode == PreParse) {
    ElapsedTimer timer;
    timer.Start();
    cached_data = v8::ScriptData::PreCompile(source_handle);
    preparse_time = timer.Elapsed();
    if (cached_data == NULL || cached_data->HasError()) {
      fprintf(stderr, "Preparsing failed\n");
      return std::make_pair(TimeDelta(), TimeDelta());
    }
  }
  if (test_mode == PreParseAndParse || test_mode == Parse) {
    Handle<String> str = v8::Utils::OpenHandle(*source_handle);
    i::Isolate* internal_isolate = str->GetIsolate();
    Handle<Script> script = internal_isolate->factory()->NewScript(str);
    CompilationInfoWithZone info(script);
    info.MarkAsGlobal();
    i::ScriptDataImpl* cached_data_impl =
        static_cast<i::ScriptDataImpl*>(cached_data);
    if (test_mode == PreParseAndParse) {
      info.SetCachedData(&cached_data_impl,
                         i::CONSUME_CACHED_DATA);
    }
    info.SetContext(v8::Utils::OpenHandle(*context));
    ElapsedTimer timer;
    timer.Start();
    // Allow lazy parsing; otherwise the preparse data won't help.
    bool success = Parser::Parse(&info, true);
    parse_time = timer.Elapsed();
    if (!success) {
      fprintf(stderr, "Parsing failed\n");
      return std::make_pair(TimeDelta(), TimeDelta());
    }
  }
  return std::make_pair(preparse_time, parse_time);
}


int main(int argc, char* argv[]) {
  v8::V8::InitializeICU();
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  Encoding encoding = LATIN1;
  TestMode test_mode = PreParseAndParse;
  std::vector<std::string> fnames;
  std::string benchmark;
  int repeat = 1;
  for (int i = 0; i < argc; ++i) {
    if (strcmp(argv[i], "--latin1") == 0) {
      encoding = LATIN1;
    } else if (strcmp(argv[i], "--utf8") == 0) {
      encoding = UTF8;
    } else if (strcmp(argv[i], "--utf16") == 0) {
      encoding = UTF16;
    } else if (strcmp(argv[i], "--preparse-and-parse") == 0) {
      test_mode = PreParseAndParse;
    } else if (strcmp(argv[i], "--preparse") == 0) {
      test_mode = PreParse;
    } else if (strcmp(argv[i], "--parse") == 0) {
      test_mode = Parse;
    } else if (strncmp(argv[i], "--benchmark=", 12) == 0) {
      benchmark = std::string(argv[i]).substr(12);
    } else if (strncmp(argv[i], "--repeat=", 9) == 0) {
      std::string repeat_str = std::string(argv[i]).substr(9);
      repeat = atoi(repeat_str.c_str());
    } else if (i > 0 && argv[i][0] != '-') {
      fnames.push_back(std::string(argv[i]));
    }
  }
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  {
    v8::HandleScope handle_scope(isolate);
    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate, NULL, global);
    ASSERT(!context.IsEmpty());
    {
      v8::Context::Scope scope(context);
      double preparse_total = 0;
      double parse_total = 0;
      for (size_t i = 0; i < fnames.size(); i++) {
        std::pair<TimeDelta, TimeDelta> time = RunBaselineParser(
            fnames[i].c_str(), encoding, repeat, isolate, context, test_mode);
        preparse_total += time.first.InMillisecondsF();
        parse_total += time.second.InMillisecondsF();
      }
      if (benchmark.empty()) benchmark = "Baseline";
      printf("%s(PreParseRunTime): %.f ms\n", benchmark.c_str(),
             preparse_total);
      printf("%s(ParseRunTime): %.f ms\n", benchmark.c_str(), parse_total);
      printf("%s(RunTime): %.f ms\n", benchmark.c_str(),
             preparse_total + parse_total);
    }
  }
  v8::V8::Dispose();
  return 0;
}
