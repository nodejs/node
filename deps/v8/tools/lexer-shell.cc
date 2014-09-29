// Copyright 2013 the V8 project authors. All rights reserved.
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
#include "src/v8.h"

#include "include/libplatform/libplatform.h"
#include "src/api.h"
#include "src/base/platform/platform.h"
#include "src/messages.h"
#include "src/runtime.h"
#include "src/scanner-character-streams.h"
#include "src/scopeinfo.h"
#include "tools/shell-utils.h"
#include "src/string-stream.h"
#include "src/scanner.h"


using namespace v8::internal;


class BaselineScanner {
 public:
  BaselineScanner(const char* fname,
                  Isolate* isolate,
                  Encoding encoding,
                  v8::base::ElapsedTimer* timer,
                  int repeat)
      : stream_(NULL) {
    int length = 0;
    source_ = ReadFileAndRepeat(fname, &length, repeat);
    unicode_cache_ = new UnicodeCache();
    scanner_ = new Scanner(unicode_cache_);
    switch (encoding) {
      case UTF8:
        stream_ = new Utf8ToUtf16CharacterStream(source_, length);
        break;
      case UTF16: {
        Handle<String> result = isolate->factory()->NewStringFromTwoByte(
            Vector<const uint16_t>(
                reinterpret_cast<const uint16_t*>(source_),
                length / 2)).ToHandleChecked();
        stream_ =
            new GenericStringUtf16CharacterStream(result, 0, result->length());
        break;
      }
      case LATIN1: {
        Handle<String> result = isolate->factory()->NewStringFromOneByte(
            Vector<const uint8_t>(source_, length)).ToHandleChecked();
        stream_ =
            new GenericStringUtf16CharacterStream(result, 0, result->length());
        break;
      }
    }
    timer->Start();
    scanner_->Initialize(stream_);
  }

  ~BaselineScanner() {
    delete scanner_;
    delete stream_;
    delete unicode_cache_;
    delete[] source_;
  }

  Token::Value Next(int* beg_pos, int* end_pos) {
    Token::Value res = scanner_->Next();
    *beg_pos = scanner_->location().beg_pos;
    *end_pos = scanner_->location().end_pos;
    return res;
  }

 private:
  UnicodeCache* unicode_cache_;
  Scanner* scanner_;
  const byte* source_;
  BufferedUtf16CharacterStream* stream_;
};


struct TokenWithLocation {
  Token::Value value;
  size_t beg;
  size_t end;
  TokenWithLocation() : value(Token::ILLEGAL), beg(0), end(0) { }
  TokenWithLocation(Token::Value value, size_t beg, size_t end) :
      value(value), beg(beg), end(end) { }
  bool operator==(const TokenWithLocation& other) {
    return value == other.value && beg == other.beg && end == other.end;
  }
  bool operator!=(const TokenWithLocation& other) {
    return !(*this == other);
  }
  void Print(const char* prefix) const {
    printf("%s %11s at (%d, %d)\n",
           prefix, Token::Name(value),
           static_cast<int>(beg), static_cast<int>(end));
  }
};


v8::base::TimeDelta RunBaselineScanner(const char* fname, Isolate* isolate,
                                       Encoding encoding, bool dump_tokens,
                                       std::vector<TokenWithLocation>* tokens,
                                       int repeat) {
  v8::base::ElapsedTimer timer;
  BaselineScanner scanner(fname, isolate, encoding, &timer, repeat);
  Token::Value token;
  int beg, end;
  do {
    token = scanner.Next(&beg, &end);
    if (dump_tokens) {
      tokens->push_back(TokenWithLocation(token, beg, end));
    }
  } while (token != Token::EOS);
  return timer.Elapsed();
}


void PrintTokens(const char* name,
                 const std::vector<TokenWithLocation>& tokens) {
  printf("No of tokens: %d\n",
         static_cast<int>(tokens.size()));
  printf("%s:\n", name);
  for (size_t i = 0; i < tokens.size(); ++i) {
    tokens[i].Print("=>");
  }
}


v8::base::TimeDelta ProcessFile(
    const char* fname,
    Encoding encoding,
    Isolate* isolate,
    bool print_tokens,
    int repeat) {
  if (print_tokens) {
    printf("Processing file %s\n", fname);
  }
  HandleScope handle_scope(isolate);
  std::vector<TokenWithLocation> baseline_tokens;
  v8::base::TimeDelta baseline_time;
  baseline_time = RunBaselineScanner(
      fname, isolate, encoding, print_tokens,
      &baseline_tokens, repeat);
  if (print_tokens) {
    PrintTokens("Baseline", baseline_tokens);
  }
  return baseline_time;
}


int main(int argc, char* argv[]) {
  v8::V8::InitializeICU();
  v8::Platform* platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platform);
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  Encoding encoding = LATIN1;
  bool print_tokens = false;
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
    } else if (strcmp(argv[i], "--print-tokens") == 0) {
      print_tokens = true;
    } else if (strncmp(argv[i], "--benchmark=", 12) == 0) {
      benchmark = std::string(argv[i]).substr(12);
    } else if (strncmp(argv[i], "--repeat=", 9) == 0) {
      std::string repeat_str = std::string(argv[i]).substr(9);
      repeat = atoi(repeat_str.c_str());
    } else if (i > 0 && argv[i][0] != '-') {
      fnames.push_back(std::string(argv[i]));
    }
  }
  v8::Isolate* isolate = v8::Isolate::New();
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate, NULL, global);
    DCHECK(!context.IsEmpty());
    {
      v8::Context::Scope scope(context);
      double baseline_total = 0;
      for (size_t i = 0; i < fnames.size(); i++) {
        v8::base::TimeDelta time;
        time = ProcessFile(fnames[i].c_str(), encoding,
                           reinterpret_cast<Isolate*>(isolate), print_tokens,
                           repeat);
        baseline_total += time.InMillisecondsF();
      }
      if (benchmark.empty()) benchmark = "Baseline";
      printf("%s(RunTime): %.f ms\n", benchmark.c_str(), baseline_total);
    }
  }
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  delete platform;
  return 0;
}
