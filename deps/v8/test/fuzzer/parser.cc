// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <cctype>
#include <list>

#include "include/v8.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "src/parsing/preparser.h"
#include "test/fuzzer/fuzzer-support.h"

bool IsValidInput(const uint8_t* data, size_t size) {
  // Ignore too long inputs as they tend to find OOM or timeouts, not real bugs.
  if (size > 2048) {
    return false;
  }

  std::list<char> parentheses;
  const char* ptr = reinterpret_cast<const char*>(data);

  for (size_t i = 0; i != size; ++i) {
    // Check that all characters in the data are valid.
    if (!(std::isspace(ptr[i]) || std::isprint(ptr[i]))) {
      return false;
    }

    // Check balance of parentheses in the data.
    switch (ptr[i]) {
      case '(':
      case '[':
      case '{':
        parentheses.push_back(ptr[i]);
        break;
      case ')':
        if (parentheses.back() != '(') return false;
        parentheses.pop_back();
        break;
      case ']':
        if (parentheses.back() != '[') return false;
        parentheses.pop_back();
        break;
      case '}':
        if (parentheses.back() != '{') return false;
        parentheses.pop_back();
        break;
      default:
        break;
    }
  }

  return parentheses.empty();
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!IsValidInput(data, size)) {
    return 0;
  }

  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());
  v8::TryCatch try_catch(isolate);

  v8::internal::Isolate* i_isolate =
      reinterpret_cast<v8::internal::Isolate*>(isolate);
  v8::internal::Factory* factory = i_isolate->factory();

  if (size > INT_MAX) return 0;
  v8::internal::MaybeHandle<v8::internal::String> source =
      factory->NewStringFromOneByte(
          v8::internal::Vector<const uint8_t>(data, static_cast<int>(size)));
  if (source.is_null()) return 0;

  v8::internal::Handle<v8::internal::Script> script =
      factory->NewScript(source.ToHandleChecked());
  v8::internal::ParseInfo info(i_isolate, script);
  if (!v8::internal::parsing::ParseProgram(&info, i_isolate)) {
    i_isolate->OptionalRescheduleException(true);
  }
  isolate->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
  return 0;
}
