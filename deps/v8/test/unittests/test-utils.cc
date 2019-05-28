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

IsolateWrapper::IsolateWrapper(CounterLookupCallback counter_lookup_callback,
                               bool enforce_pointer_compression)
    : array_buffer_allocator_(
          v8::ArrayBuffer::Allocator::NewDefaultAllocator()) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = array_buffer_allocator_;
  create_params.counter_lookup_callback = counter_lookup_callback;
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

// static
int* SharedIsolateAndCountersHolder::LookupCounter(const char* name) {
  DCHECK_NOT_NULL(counter_map_);
  auto map_entry = counter_map_->find(name);
  if (map_entry == counter_map_->end()) {
    counter_map_->emplace(name, 0);
  }
  return &counter_map_->at(name);
}

// static
v8::IsolateWrapper* SharedIsolateAndCountersHolder::isolate_wrapper_ = nullptr;

// static
CounterMap* SharedIsolateAndCountersHolder::counter_map_ = nullptr;

namespace internal {

SaveFlags::SaveFlags() {
  // For each flag, save the current flag value.
#define FLAG_MODE_APPLY(ftype, ctype, nam, def, cmt) SAVED_##nam = FLAG_##nam;
#include "src/flag-definitions.h"  // NOLINT
#undef FLAG_MODE_APPLY
}

SaveFlags::~SaveFlags() {
  // For each flag, set back the old flag value if it changed (don't write the
  // flag if it didn't change, to keep TSAN happy).
#define FLAG_MODE_APPLY(ftype, ctype, nam, def, cmt) \
  if (SAVED_##nam != FLAG_##nam) {                   \
    FLAG_##nam = SAVED_##nam;                        \
  }
#include "src/flag-definitions.h"  // NOLINT
#undef FLAG_MODE_APPLY
}

}  // namespace internal
}  // namespace v8
