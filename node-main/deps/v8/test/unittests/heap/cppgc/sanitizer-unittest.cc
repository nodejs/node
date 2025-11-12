// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/allocation.h"
#include "src/base/macros.h"
#include "src/base/sanitizer/asan.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif  // LEAK_SANITIZER

namespace cppgc {
namespace internal {

#if defined(LEAK_SANITIZER)

using LsanTest = testing::TestWithHeap;

class GCed final : public GarbageCollected<GCed> {
 public:
  void Trace(cppgc::Visitor*) const {}
  std::unique_ptr<int> dummy{std::make_unique<int>(17)};
};

TEST_F(LsanTest, LeakDetectionDoesNotFindMemoryRetainedFromManaged) {
  auto* o = MakeGarbageCollected<GCed>(GetAllocationHandle());
  __lsan_do_leak_check();
  USE(o);
}

#endif  // LEAK_SANITIZER

#ifdef V8_USE_ADDRESS_SANITIZER

using AsanTest = testing::TestWithHeap;

class ObjectPoisoningInDestructor final
    : public GarbageCollected<ObjectPoisoningInDestructor> {
 public:
  ~ObjectPoisoningInDestructor() {
    ASAN_POISON_MEMORY_REGION(this, sizeof(ObjectPoisoningInDestructor));
  }
  void Trace(cppgc::Visitor*) const {}

  void* dummy{0};
};

TEST_F(AsanTest, ObjectPoisoningInDestructor) {
  MakeGarbageCollected<ObjectPoisoningInDestructor>(GetAllocationHandle());
  PreciseGC();
}

#endif  // V8_USE_ADDRESS_SANITIZER

}  // namespace internal
}  // namespace cppgc
