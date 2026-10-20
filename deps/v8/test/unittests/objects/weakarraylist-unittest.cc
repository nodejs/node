// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/factory.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

using WeakArrayListTest = TestWithIsolate;

TEST_F(WeakArrayListTest, Compact) {
  DirectHandle<WeakArrayList> list = isolate()->factory()->NewWeakArrayList(10);
  EXPECT_EQ(list->length().value(), 0u);
  EXPECT_EQ(list->capacity().value(), 10u);

  Tagged<FixedArray> some_object = *isolate()->factory()->empty_fixed_array();
  Tagged<MaybeObject> weak_ref = MakeWeak(some_object);
  Tagged<MaybeObject> smi = Smi::FromInt(0);
  Tagged<MaybeObject> cleared_ref = kClearedWeakValue;
  list->Set(0, weak_ref);
  list->Set(1, smi);
  list->Set(2, cleared_ref);
  list->Set(3, cleared_ref);
  list->set_length(5);

  list->Compact(isolate());
  EXPECT_EQ(list->length().value(), 3u);
  EXPECT_EQ(list->capacity().value(), 10u);
}

TEST_F(WeakArrayListTest, OutOfPlaceCompact) {
  DirectHandle<WeakArrayList> list = isolate()->factory()->NewWeakArrayList(20);
  EXPECT_EQ(list->length().value(), 0u);
  EXPECT_EQ(list->capacity().value(), 20u);

  Tagged<FixedArray> some_object = *isolate()->factory()->empty_fixed_array();
  Tagged<MaybeObject> weak_ref = MakeWeak(some_object);
  Tagged<MaybeObject> smi = Smi::FromInt(0);
  Tagged<MaybeObject> cleared_ref = kClearedWeakValue;
  list->Set(0, weak_ref);
  list->Set(1, smi);
  list->Set(2, cleared_ref);
  list->Set(3, smi);
  list->Set(4, cleared_ref);
  list->set_length(6);

  DirectHandle<WeakArrayList> compacted =
      isolate()->factory()->CompactWeakArrayList(list, 4);
  EXPECT_EQ(list->length().value(), 6u);
  EXPECT_EQ(compacted->length().value(), 4u);
  EXPECT_EQ(compacted->capacity().value(), 4u);
}

}  // namespace internal
}  // namespace v8
