// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "src/api-inl.h"
#include "src/base/platform/time.h"
#include "src/flags.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/v8.h"

namespace v8 {

IsolateWrapper::IsolateWrapper(bool enforce_pointer_compression)
    : array_buffer_allocator_(
          v8::ArrayBuffer::Allocator::NewDefaultAllocator()) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = array_buffer_allocator_;
  if (enforce_pointer_compression) {
    isolate_ = reinterpret_cast<v8::Isolate*>(
        i::Isolate::New(i::IsolateAllocationMode::kInV8Heap));
    v8::Isolate::Initialize(isolate_, create_params);
  } else {
    isolate_ = v8::Isolate::New(create_params);
  }
  CHECK_NOT_NULL(isolate_);
}

IsolateWrapper::~IsolateWrapper() {
  v8::Platform* platform = internal::V8::GetCurrentPlatform();
  CHECK_NOT_NULL(platform);
  while (platform::PumpMessageLoop(platform, isolate_)) continue;
  isolate_->Dispose();
  delete array_buffer_allocator_;
}

// static
v8::IsolateWrapper* SharedIsolateHolder::isolate_wrapper_ = nullptr;

namespace internal {

SaveFlags::SaveFlags() { non_default_flags_ = FlagList::argv(); }

SaveFlags::~SaveFlags() {
  FlagList::ResetAllFlags();
  int argc = static_cast<int>(non_default_flags_->size());
  FlagList::SetFlagsFromCommandLine(
      &argc, const_cast<char**>(non_default_flags_->data()),
      false /* remove_flags */);
  for (auto flag = non_default_flags_->begin();
       flag != non_default_flags_->end(); ++flag) {
    delete[] * flag;
  }
  delete non_default_flags_;
}

}  // namespace internal
}  // namespace v8
