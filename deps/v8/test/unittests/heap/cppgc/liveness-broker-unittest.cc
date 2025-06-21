// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/liveness-broker.h"

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/liveness-broker.h"
#include "test/unittests/heap/cppgc/tests.h"

namespace cppgc {
namespace internal {

namespace {

using LivenessBrokerTest = testing::TestSupportingAllocationOnly;

class GCed : public GarbageCollected<GCed> {
 public:
  void Trace(cppgc::Visitor*) const {}
};

}  // namespace

TEST_F(LivenessBrokerTest, IsHeapObjectAliveForConstPointer) {
  // Regression test: http://crbug.com/661363.
  GCed* object = MakeGarbageCollected<GCed>(GetAllocationHandle());
  HeapObjectHeader& header = HeapObjectHeader::FromObject(object);
  LivenessBroker broker = internal::LivenessBrokerFactory::Create();
  EXPECT_TRUE(header.TryMarkAtomic());
  EXPECT_TRUE(broker.IsHeapObjectAlive(object));
  const GCed* const_object = const_cast<const GCed*>(object);
  EXPECT_TRUE(broker.IsHeapObjectAlive(const_object));
}

TEST_F(LivenessBrokerTest, IsHeapObjectAliveNullptr) {
  GCed* object = nullptr;
  LivenessBroker broker = internal::LivenessBrokerFactory::Create();
  EXPECT_TRUE(broker.IsHeapObjectAlive(object));
}

}  // namespace internal
}  // namespace cppgc
