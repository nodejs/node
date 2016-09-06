// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "include/v8.h"
#include "src/objects.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/preparser.h"
#include "test/fuzzer/fuzzer-support.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
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
  v8::internal::Zone zone(i_isolate->allocator());
  v8::internal::ParseInfo info(&zone, script);
  info.set_global();
  v8::internal::Parser parser(&info);
  parser.Parse(&info);
  isolate->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
  return 0;
}
