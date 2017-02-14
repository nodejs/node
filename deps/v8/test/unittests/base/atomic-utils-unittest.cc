// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>

#include "src/base/atomic-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

TEST(AtomicNumber, Constructor) {
  // Test some common types.
  AtomicNumber<int> zero_int;
  AtomicNumber<size_t> zero_size_t;
  AtomicNumber<intptr_t> zero_intptr_t;
  EXPECT_EQ(0, zero_int.Value());
  EXPECT_EQ(0u, zero_size_t.Value());
  EXPECT_EQ(0, zero_intptr_t.Value());
}


TEST(AtomicNumber, Value) {
  AtomicNumber<int> a(1);
  EXPECT_EQ(1, a.Value());
  AtomicNumber<int> b(-1);
  EXPECT_EQ(-1, b.Value());
  AtomicNumber<size_t> c(1);
  EXPECT_EQ(1u, c.Value());
  AtomicNumber<size_t> d(static_cast<size_t>(-1));
  EXPECT_EQ(std::numeric_limits<size_t>::max(), d.Value());
}


TEST(AtomicNumber, SetValue) {
  AtomicNumber<int> a(1);
  a.SetValue(-1);
  EXPECT_EQ(-1, a.Value());
}


TEST(AtomicNumber, Increment) {
  AtomicNumber<int> a(std::numeric_limits<int>::max());
  a.Increment(1);
  EXPECT_EQ(std::numeric_limits<int>::min(), a.Value());
  // Check that potential signed-ness of the underlying storage has no impact
  // on unsigned types.
  AtomicNumber<size_t> b(std::numeric_limits<intptr_t>::max());
  b.Increment(1);
  EXPECT_EQ(static_cast<size_t>(std::numeric_limits<intptr_t>::max()) + 1,
            b.Value());
  // Should work as decrement as well.
  AtomicNumber<size_t> c(1);
  c.Increment(-1);
  EXPECT_EQ(0u, c.Value());
  c.Increment(-1);
  EXPECT_EQ(std::numeric_limits<size_t>::max(), c.Value());
}

TEST(AtomicNumber, Decrement) {
  AtomicNumber<size_t> a(std::numeric_limits<size_t>::max());
  a.Increment(1);
  EXPECT_EQ(0u, a.Value());
  a.Decrement(1);
  EXPECT_EQ(std::numeric_limits<size_t>::max(), a.Value());
}

TEST(AtomicNumber, OperatorAdditionAssignment) {
  AtomicNumber<size_t> a(0u);
  AtomicNumber<size_t> b(std::numeric_limits<size_t>::max());
  a += b.Value();
  EXPECT_EQ(a.Value(), b.Value());
  EXPECT_EQ(b.Value(), std::numeric_limits<size_t>::max());
}

TEST(AtomicNumber, OperatorSubtractionAssignment) {
  AtomicNumber<size_t> a(std::numeric_limits<size_t>::max());
  AtomicNumber<size_t> b(std::numeric_limits<size_t>::max());
  a -= b.Value();
  EXPECT_EQ(a.Value(), 0u);
  EXPECT_EQ(b.Value(), std::numeric_limits<size_t>::max());
}

namespace {

enum TestFlag : base::AtomicWord {
  kA,
  kB,
  kC,
};

}  // namespace


TEST(AtomicValue, Initial) {
  AtomicValue<TestFlag> a(kA);
  EXPECT_EQ(TestFlag::kA, a.Value());
}


TEST(AtomicValue, TrySetValue) {
  AtomicValue<TestFlag> a(kA);
  EXPECT_FALSE(a.TrySetValue(kB, kC));
  EXPECT_TRUE(a.TrySetValue(kA, kC));
  EXPECT_EQ(TestFlag::kC, a.Value());
}


TEST(AtomicValue, SetValue) {
  AtomicValue<TestFlag> a(kB);
  a.SetValue(kC);
  EXPECT_EQ(TestFlag::kC, a.Value());
}


TEST(AtomicValue, WithVoidStar) {
  AtomicValue<void*> a(nullptr);
  AtomicValue<void*> dummy(nullptr);
  EXPECT_EQ(nullptr, a.Value());
  a.SetValue(&a);
  EXPECT_EQ(&a, a.Value());
  EXPECT_FALSE(a.TrySetValue(nullptr, &dummy));
  EXPECT_TRUE(a.TrySetValue(&a, &dummy));
  EXPECT_EQ(&dummy, a.Value());
}

TEST(NoBarrierAtomicValue, Initial) {
  NoBarrierAtomicValue<TestFlag> a(kA);
  EXPECT_EQ(TestFlag::kA, a.Value());
}

TEST(NoBarrierAtomicValue, SetValue) {
  NoBarrierAtomicValue<TestFlag> a(kB);
  a.SetValue(kC);
  EXPECT_EQ(TestFlag::kC, a.Value());
}

TEST(NoBarrierAtomicValue, WithVoidStar) {
  NoBarrierAtomicValue<void*> a(nullptr);
  NoBarrierAtomicValue<void*> dummy(nullptr);
  EXPECT_EQ(nullptr, a.Value());
  a.SetValue(&a);
  EXPECT_EQ(&a, a.Value());
}

TEST(NoBarrierAtomicValue, Construction) {
  NoBarrierAtomicValue<TestFlag> a(kA);
  TestFlag b = kA;
  NoBarrierAtomicValue<TestFlag>* ptr =
      NoBarrierAtomicValue<TestFlag>::FromAddress(&b);
  EXPECT_EQ(ptr->Value(), a.Value());
}

TEST(NoBarrierAtomicValue, ConstructionVoidStar) {
  NoBarrierAtomicValue<void*> a(nullptr);
  void* b = nullptr;
  NoBarrierAtomicValue<void*>* ptr =
      NoBarrierAtomicValue<void*>::FromAddress(&b);
  EXPECT_EQ(ptr->Value(), a.Value());
}

namespace {

enum TestSetValue { kAA, kBB, kCC, kLastValue = kCC };

}  // namespace


TEST(AtomicEnumSet, Constructor) {
  AtomicEnumSet<TestSetValue> a;
  EXPECT_TRUE(a.IsEmpty());
  EXPECT_FALSE(a.Contains(kAA));
}


TEST(AtomicEnumSet, AddSingle) {
  AtomicEnumSet<TestSetValue> a;
  a.Add(kAA);
  EXPECT_FALSE(a.IsEmpty());
  EXPECT_TRUE(a.Contains(kAA));
  EXPECT_FALSE(a.Contains(kBB));
  EXPECT_FALSE(a.Contains(kCC));
}


TEST(AtomicEnumSet, AddOtherSet) {
  AtomicEnumSet<TestSetValue> a;
  AtomicEnumSet<TestSetValue> b;
  a.Add(kAA);
  EXPECT_FALSE(a.IsEmpty());
  EXPECT_TRUE(b.IsEmpty());
  b.Add(a);
  EXPECT_FALSE(b.IsEmpty());
  EXPECT_TRUE(a.Contains(kAA));
  EXPECT_TRUE(b.Contains(kAA));
}


TEST(AtomicEnumSet, RemoveSingle) {
  AtomicEnumSet<TestSetValue> a;
  a.Add(kAA);
  a.Add(kBB);
  EXPECT_TRUE(a.Contains(kAA));
  EXPECT_TRUE(a.Contains(kBB));
  a.Remove(kAA);
  EXPECT_FALSE(a.Contains(kAA));
  EXPECT_TRUE(a.Contains(kBB));
}


TEST(AtomicEnumSet, RemoveOtherSet) {
  AtomicEnumSet<TestSetValue> a;
  AtomicEnumSet<TestSetValue> b;
  a.Add(kAA);
  a.Add(kBB);
  b.Add(kBB);
  a.Remove(b);
  EXPECT_TRUE(a.Contains(kAA));
  EXPECT_FALSE(a.Contains(kBB));
  EXPECT_FALSE(a.Contains(kCC));
}


TEST(AtomicEnumSet, RemoveEmptySet) {
  AtomicEnumSet<TestSetValue> a;
  AtomicEnumSet<TestSetValue> b;
  a.Add(kAA);
  a.Add(kBB);
  EXPECT_TRUE(a.Contains(kAA));
  EXPECT_TRUE(a.Contains(kBB));
  EXPECT_FALSE(a.Contains(kCC));
  EXPECT_TRUE(b.IsEmpty());
  a.Remove(b);
  EXPECT_TRUE(a.Contains(kAA));
  EXPECT_TRUE(a.Contains(kBB));
  EXPECT_FALSE(a.Contains(kCC));
}


TEST(AtomicEnumSet, Intersect) {
  AtomicEnumSet<TestSetValue> a;
  AtomicEnumSet<TestSetValue> b;
  a.Add(kAA);
  b.Add(kCC);
  a.Intersect(b);
  EXPECT_TRUE(a.IsEmpty());
}


TEST(AtomicEnumSet, ContainsAnyOf) {
  AtomicEnumSet<TestSetValue> a;
  AtomicEnumSet<TestSetValue> b;
  a.Add(kAA);
  b.Add(kCC);
  EXPECT_FALSE(a.ContainsAnyOf(b));
  b.Add(kAA);
  EXPECT_TRUE(a.ContainsAnyOf(b));
}


TEST(AtomicEnumSet, Equality) {
  AtomicEnumSet<TestSetValue> a;
  AtomicEnumSet<TestSetValue> b;
  a.Add(kAA);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
  b.Add(kAA);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

}  // namespace base
}  // namespace v8
