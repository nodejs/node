// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "include/v8.h"
#include "test/fuzzer/fuzzer-support.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());
  v8::TryCatch try_catch(isolate);

  if (size > INT_MAX) return 0;
  v8::Local<v8::String> source;
  if (!v8::String::NewFromOneByte(isolate, data, v8::NewStringType::kNormal,
                                  static_cast<int>(size))
           .ToLocal(&source)) {
    return 0;
  }

  v8::JSON::Parse(support->GetContext(), source).IsEmpty();
  isolate->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
  return 0;
}
