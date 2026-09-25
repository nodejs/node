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
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/init/v8.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/heap/heap-utils.h"

namespace v8 {

namespace {
// counter_lookup_callback doesn't pass through any state information about
// the current Isolate, so we have to store the current counter map somewhere.
// Fortunately tests run serially, so we can just store it in a static global.
CounterMap* kCurrentCounterMap = nullptr;
}  // namespace

std::unique_ptr<CppHeap> IsolateWrapper::cpp_heap_;

IsolateWrapper::IsolateWrapper(CountersMode counters_mode,
                               bool use_statically_set_cpp_heap)
    : array_buffer_allocator_(
          v8::ArrayBuffer::Allocator::NewDefaultAllocator()) {
  CHECK_NULL(kCurrentCounterMap);

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = array_buffer_allocator_.get();
  if (use_statically_set_cpp_heap) {
    create_params.cpp_heap = cpp_heap_.release();
  }

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
  isolate_->Enter();
  while (platform::PumpMessageLoop(platform, isolate())) continue;
  isolate_->Exit();
  isolate_->Dispose();
  if (counter_map_) {
    CHECK_EQ(kCurrentCounterMap, counter_map_.get());
    kCurrentCounterMap = nullptr;
  } else {
    CHECK_NULL(kCurrentCounterMap);
  }
}

namespace internal {

std::unordered_set<std::string> CrashKeyStore::KeyNames() const {
  std::unordered_set<std::string> names;
  names.reserve(entries_.size());
  for (const auto& [name, unused] : entries_) {
    static_cast<void>(unused);
    names.insert(name);
  }
  return names;
}

void CrashKeyStore::InstallCallbacks() {
  isolate_->SetCrashKeyStringCallbacks(
      [this](const char key[], CrashKeySize size) {
        return AllocateKey(key, size);
      },
      [this](CrashKey crash_key, const std::string_view value) {
        SetValue(crash_key, value);
      });
}

CrashKey CrashKeyStore::AllocateKey(const char key[], CrashKeySize size) {
  auto [it, inserted] = entries_.emplace(key, nullptr);
  if (inserted || it->second == nullptr) {
    it->second = std::make_unique<Entry>();
  }
  Entry* entry_ptr = it->second.get();
  entry_ptr->size = size;
  entry_ptr->value.clear();
  return static_cast<CrashKey>(entry_ptr);
}

void CrashKeyStore::SetValue(CrashKey crash_key, const std::string_view value) {
  auto* entry = static_cast<Entry*>(crash_key);
  bool found = false;
  for (const auto& [name, entry_holder] : entries_) {
    if (entry_holder.get() == entry) {
      found = true;
      break;
    }
  }
  CHECK(found);
  entry->value.assign(value.begin(), value.end());
}

}  // namespace internal
}  // namespace v8
