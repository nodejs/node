// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "include/libplatform/libplatform.h"
#include "include/v8-isolate.h"
#include "src/api/api-inl.h"
#include "src/base/platform/time.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/init/v8.h"
#include "src/objects/objects-inl.h"

namespace v8 {

namespace {
// counter_lookup_callback doesn't pass through any state information about
// the current Isolate, so we have to store the current counter map somewhere.
// Fortunately tests run serially, so we can just store it in a static global.
CounterMap* kCurrentCounterMap = nullptr;
}  // namespace

IsolateWrapper::IsolateWrapper(CountersMode counters_mode)
    : array_buffer_allocator_(
          v8::ArrayBuffer::Allocator::NewDefaultAllocator()) {
  CHECK_NULL(kCurrentCounterMap);

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = array_buffer_allocator_.get();

  if (counters_mode == kEnableCounters) {
    counter_map_ = std::make_unique<CounterMap>();
    kCurrentCounterMap = counter_map_.get();

    create_params.counter_lookup_callback = [](const char* name) {
      CHECK_NOT_NULL(kCurrentCounterMap);
      // If the name doesn't exist in the counter map, operator[] will default
      // initialize it to zero.
      return &(*kCurrentCounterMap)[name];
    };
  } else {
    create_params.counter_lookup_callback = [](const char* name) -> int* {
      return nullptr;
    };
  }

  isolate_ = v8::Isolate::New(create_params);
  CHECK_NOT_NULL(isolate());
}

IsolateWrapper::~IsolateWrapper() {
  v8::Platform* platform = internal::V8::GetCurrentPlatform();
  CHECK_NOT_NULL(platform);
  while (platform::PumpMessageLoop(platform, isolate())) continue;
  isolate_->Dispose();
  if (counter_map_) {
    CHECK_EQ(kCurrentCounterMap, counter_map_.get());
    kCurrentCounterMap = nullptr;
  } else {
    CHECK_NULL(kCurrentCounterMap);
  }
}

namespace internal {

SaveFlags::SaveFlags() {
  // For each flag, save the current flag value.
#define FLAG_MODE_APPLY(ftype, ctype, nam, def, cmt) SAVED_##nam = FLAG_##nam;
#include "src/flags/flag-definitions.h"
#undef FLAG_MODE_APPLY
}

SaveFlags::~SaveFlags() {
  // For each flag, set back the old flag value if it changed (don't write the
  // flag if it didn't change, to keep TSAN happy).
#define FLAG_MODE_APPLY(ftype, ctype, nam, def, cmt) \
  if (SAVED_##nam != FLAG_##nam) {                   \
    FLAG_##nam = SAVED_##nam;                        \
  }
#include "src/flags/flag-definitions.h"  // NOLINT
#undef FLAG_MODE_APPLY
}

ManualGCScope::ManualGCScope(i::Isolate* isolate) {
  // Some tests run threaded (back-to-back) and thus the GC may already be
  // running by the time a ManualGCScope is created. Finalizing existing marking
  // prevents any undefined/unexpected behavior.
  if (isolate && isolate->heap()->incremental_marking()->IsMarking()) {
    isolate->heap()->CollectGarbage(i::OLD_SPACE,
                                    i::GarbageCollectionReason::kTesting);
  }

  i::FLAG_concurrent_marking = false;
  i::FLAG_concurrent_sweeping = false;
  i::FLAG_stress_incremental_marking = false;
  i::FLAG_stress_concurrent_allocation = false;
  // Parallel marking has a dependency on concurrent marking.
  i::FLAG_parallel_marking = false;
  i::FLAG_detect_ineffective_gcs_near_heap_limit = false;
}

}  // namespace internal
}  // namespace v8
