// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "include/libplatform/libplatform.h"
#include "src/base/platform/time.h"
#include "src/flags.h"
#include "src/isolate.h"
#include "src/list-inl.h"
#include "src/objects-inl.h"
#include "src/v8.h"

namespace v8 {

// static
v8::ArrayBuffer::Allocator* TestWithIsolate::array_buffer_allocator_ = nullptr;

// static
Isolate* TestWithIsolate::isolate_ = nullptr;

TestWithIsolate::TestWithIsolate()
    : isolate_scope_(isolate()), handle_scope_(isolate()) {}


TestWithIsolate::~TestWithIsolate() {}


// static
void TestWithIsolate::SetUpTestCase() {
  Test::SetUpTestCase();
  EXPECT_EQ(NULL, isolate_);
  v8::Isolate::CreateParams create_params;
  array_buffer_allocator_ = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  create_params.array_buffer_allocator = array_buffer_allocator_;
  isolate_ = v8::Isolate::New(create_params);
  EXPECT_TRUE(isolate_ != NULL);
}


// static
void TestWithIsolate::TearDownTestCase() {
  ASSERT_TRUE(isolate_ != NULL);
  v8::Platform* platform = internal::V8::GetCurrentPlatform();
  ASSERT_TRUE(platform != NULL);
  while (platform::PumpMessageLoop(platform, isolate_)) continue;
  isolate_->Dispose();
  isolate_ = NULL;
  delete array_buffer_allocator_;
  Test::TearDownTestCase();
}


TestWithContext::TestWithContext()
    : context_(Context::New(isolate())), context_scope_(context_) {}


TestWithContext::~TestWithContext() {}


namespace base {
namespace {

inline int64_t GetRandomSeedFromFlag(int random_seed) {
  return random_seed ? random_seed : TimeTicks::Now().ToInternalValue();
}

}  // namespace

TestWithRandomNumberGenerator::TestWithRandomNumberGenerator()
    : rng_(GetRandomSeedFromFlag(::v8::internal::FLAG_random_seed)) {}


TestWithRandomNumberGenerator::~TestWithRandomNumberGenerator() {}

}  // namespace base


namespace internal {

TestWithIsolate::~TestWithIsolate() {}

TestWithIsolateAndZone::~TestWithIsolateAndZone() {}

Factory* TestWithIsolate::factory() const { return isolate()->factory(); }

base::RandomNumberGenerator* TestWithIsolate::random_number_generator() const {
  return isolate()->random_number_generator();
}

TestWithZone::~TestWithZone() {}

TestWithNativeContext::~TestWithNativeContext() {}

Handle<Context> TestWithNativeContext::native_context() const {
  return isolate()->native_context();
}

SaveFlags::SaveFlags() { non_default_flags_ = FlagList::argv(); }

SaveFlags::~SaveFlags() {
  FlagList::ResetAllFlags();
  int argc = non_default_flags_->length();
  FlagList::SetFlagsFromCommandLine(
      &argc, const_cast<char**>(non_default_flags_->begin()),
      false /* remove_flags */);
  for (auto flag = non_default_flags_->begin();
       flag != non_default_flags_->end(); ++flag) {
    delete[] * flag;
  }
  delete non_default_flags_;
}

}  // namespace internal
}  // namespace v8
