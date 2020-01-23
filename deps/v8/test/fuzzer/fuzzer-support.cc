// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/fuzzer/fuzzer-support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/libplatform/libplatform.h"

#include "src/flags/flags.h"

namespace v8_fuzzer {

FuzzerSupport::FuzzerSupport(int* argc, char*** argv) {
  v8::internal::FLAG_expose_gc = true;
  v8::V8::SetFlagsFromCommandLine(argc, *argv, true);
  v8::V8::InitializeICUDefaultLocation((*argv)[0]);
  v8::V8::InitializeExternalStartupData((*argv)[0]);
  platform_ = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform_.get());
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
    while (PumpMessageLoop()) {
      // empty
    }

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
}

std::unique_ptr<FuzzerSupport> FuzzerSupport::fuzzer_support_;

// static
void FuzzerSupport::InitializeFuzzerSupport(int* argc, char*** argv) {
  DCHECK_NULL(FuzzerSupport::fuzzer_support_);
  FuzzerSupport::fuzzer_support_ =
      std::make_unique<v8_fuzzer::FuzzerSupport>(argc, argv);
}

// static
FuzzerSupport* FuzzerSupport::Get() {
  DCHECK_NOT_NULL(FuzzerSupport::fuzzer_support_);
  return FuzzerSupport::fuzzer_support_.get();
}

v8::Local<v8::Context> FuzzerSupport::GetContext() {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::EscapableHandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  return handle_scope.Escape(context);
}

bool FuzzerSupport::PumpMessageLoop(
    v8::platform::MessageLoopBehavior behavior) {
  return v8::platform::PumpMessageLoop(platform_.get(), isolate_, behavior);
}

}  // namespace v8_fuzzer

// Explicitly specify some attributes to avoid issues with the linker dead-
// stripping the following function on macOS, as it is not called directly
// by fuzz target. LibFuzzer runtime uses dlsym() to resolve that function.
#if V8_OS_MACOSX
__attribute__((used)) __attribute__((visibility("default")))
#endif  // V8_OS_MACOSX
extern "C" int
LLVMFuzzerInitialize(int* argc, char*** argv) {
  v8_fuzzer::FuzzerSupport::InitializeFuzzerSupport(argc, argv);
  return 0;
}
