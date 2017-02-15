// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/fuzzer/fuzzer-support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/libplatform/libplatform.h"

#include "src/flags.h"

namespace v8_fuzzer {

namespace {

FuzzerSupport* g_fuzzer_support = nullptr;

void DeleteFuzzerSupport() {
  if (g_fuzzer_support) {
    delete g_fuzzer_support;
    g_fuzzer_support = nullptr;
  }
}

}  // namespace

FuzzerSupport::FuzzerSupport(int* argc, char*** argv) {
  v8::internal::FLAG_expose_gc = true;
  v8::V8::SetFlagsFromCommandLine(argc, *argv, true);
  v8::V8::InitializeICUDefaultLocation((*argv)[0]);
  v8::V8::InitializeExternalStartupData((*argv)[0]);
  platform_ = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platform_);
  v8::V8::Initialize();

  allocator_ = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = allocator_;
  isolate_ = v8::Isolate::New(create_params);

  {
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    context_.Reset(isolate_, v8::Context::New(isolate_));
  }
}

FuzzerSupport::~FuzzerSupport() {
  {
    v8::Isolate::Scope isolate_scope(isolate_);
    while (v8::platform::PumpMessageLoop(platform_, isolate_)) /* empty */
      ;

    v8::HandleScope handle_scope(isolate_);
    context_.Reset();
  }

  isolate_->LowMemoryNotification();
  isolate_->Dispose();
  isolate_ = nullptr;

  delete allocator_;
  allocator_ = nullptr;

  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();

  delete platform_;
  platform_ = nullptr;
}

// static
FuzzerSupport* FuzzerSupport::Get() { return g_fuzzer_support; }

v8::Isolate* FuzzerSupport::GetIsolate() { return isolate_; }

v8::Local<v8::Context> FuzzerSupport::GetContext() {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::EscapableHandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  return handle_scope.Escape(context);
}

}  // namespace v8_fuzzer

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  v8_fuzzer::g_fuzzer_support = new v8_fuzzer::FuzzerSupport(argc, argv);
  atexit(&v8_fuzzer::DeleteFuzzerSupport);
  return 0;
}
