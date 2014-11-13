// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/base/platform/time.h"
#include "src/flags.h"
#include "src/isolate-inl.h"

namespace v8 {

std::ostream& operator<<(std::ostream& os, ExternalArrayType type) {
  switch (type) {
    case kExternalInt8Array:
      return os << "ExternalInt8Array";
    case kExternalUint8Array:
      return os << "ExternalUint8Array";
    case kExternalInt16Array:
      return os << "ExternalInt16Array";
    case kExternalUint16Array:
      return os << "ExternalUint16Array";
    case kExternalInt32Array:
      return os << "ExternalInt32Array";
    case kExternalUint32Array:
      return os << "ExternalUint32Array";
    case kExternalFloat32Array:
      return os << "ExternalFloat32Array";
    case kExternalFloat64Array:
      return os << "ExternalFloat64Array";
    case kExternalUint8ClampedArray:
      return os << "ExternalUint8ClampedArray";
  }
  UNREACHABLE();
  return os;
}


// static
Isolate* TestWithIsolate::isolate_ = NULL;


TestWithIsolate::TestWithIsolate()
    : isolate_scope_(isolate()), handle_scope_(isolate()) {}


TestWithIsolate::~TestWithIsolate() {}


// static
void TestWithIsolate::SetUpTestCase() {
  Test::SetUpTestCase();
  EXPECT_EQ(NULL, isolate_);
  isolate_ = v8::Isolate::New();
  EXPECT_TRUE(isolate_ != NULL);
}


// static
void TestWithIsolate::TearDownTestCase() {
  ASSERT_TRUE(isolate_ != NULL);
  isolate_->Dispose();
  isolate_ = NULL;
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
    : rng_(GetRandomSeedFromFlag(internal::FLAG_random_seed)) {}


TestWithRandomNumberGenerator::~TestWithRandomNumberGenerator() {}

}  // namespace base


namespace internal {

TestWithIsolate::~TestWithIsolate() {}


Factory* TestWithIsolate::factory() const { return isolate()->factory(); }


TestWithZone::~TestWithZone() {}

}  // namespace internal
}  // namespace v8
