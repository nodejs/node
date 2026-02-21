// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/allocation-observer.h"

#include "src/base/logging.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {
class UnusedObserver : public AllocationObserver {
 public:
  explicit UnusedObserver(size_t step_size) : AllocationObserver(step_size) {}
  void Step(int bytes_allocated, Address soon_object, size_t size) override {
    CHECK(false);
  }
};
}  // namespace

TEST(AllocationObserverTest, AddAndRemoveUnusedObservers) {
  AllocationCounter counter;
  CHECK_EQ(SIZE_MAX, counter.NextBytes());

  UnusedObserver observer100(100);
  UnusedObserver observer200(200);

  counter.AddAllocationObserver(&observer200);
  CHECK_EQ(counter.NextBytes(), 200);

  counter.AddAllocationObserver(&observer100);
  CHECK_EQ(counter.NextBytes(), 100);

  counter.AdvanceAllocationObservers(90);
  CHECK_EQ(counter.NextBytes(), 10);

  counter.RemoveAllocationObserver(&observer100);
  CHECK_EQ(counter.NextBytes(), 110);

  counter.RemoveAllocationObserver(&observer200);
  CHECK_EQ(SIZE_MAX, counter.NextBytes());
}

namespace {
class VerifyStepObserver : public AllocationObserver {
 public:
  explicit VerifyStepObserver(size_t step_size)
      : AllocationObserver(step_size) {}

  void Step(int bytes_allocated, Address soon_object, size_t size) override {
    CHECK(!do_not_invoke_);

    invocations_++;
    CHECK_EQ(expected_bytes_allocated_, bytes_allocated);
    CHECK_EQ(expected_size_, size);
  }

  void ExpectNoInvocation() { do_not_invoke_ = true; }
  void Expect(int expected_bytes_allocated, size_t expected_size) {
    do_not_invoke_ = false;
    expected_bytes_allocated_ = expected_bytes_allocated;
    expected_size_ = expected_size;
  }

  int Invocations() { return invocations_; }

 private:
  bool do_not_invoke_ = false;
  int invocations_ = 0;
  int expected_bytes_allocated_ = 0;
  size_t expected_size_ = 0;
};
}  // namespace

TEST(AllocationObserverTest, Step) {
  AllocationCounter counter;
  CHECK_EQ(SIZE_MAX, counter.NextBytes());
  const Address kSomeObjectAddress = 8;

  VerifyStepObserver observer100(100);
  VerifyStepObserver observer200(200);

  counter.AddAllocationObserver(&observer100);
  counter.AddAllocationObserver(&observer200);

  observer100.Expect(90, 8);
  observer200.ExpectNoInvocation();

  counter.AdvanceAllocationObservers(90);
  counter.InvokeAllocationObservers(kSomeObjectAddress, 8, 10);
  CHECK_EQ(observer100.Invocations(), 1);
  CHECK_EQ(observer200.Invocations(), 0);
  CHECK_EQ(counter.NextBytes(),
           10 /* aligned_object_size */ + 100 /* smallest step size*/);

  observer100.Expect(90, 16);
  observer200.Expect(180, 16);

  counter.AdvanceAllocationObservers(90);
  counter.InvokeAllocationObservers(kSomeObjectAddress, 16, 20);
  CHECK_EQ(observer100.Invocations(), 2);
  CHECK_EQ(observer200.Invocations(), 1);
  CHECK_EQ(counter.NextBytes(),
           20 /* aligned_object_size */ + 100 /* smallest step size*/);
}

namespace {
class RecursiveAddObserver : public AllocationObserver {
 public:
  explicit RecursiveAddObserver(size_t step_size, AllocationCounter* counter,
                                AllocationObserver* observer)
      : AllocationObserver(step_size), counter_(counter), observer_(observer) {}

  void Step(int bytes_allocated, Address soon_object, size_t size) override {
    counter_->AddAllocationObserver(observer_);
  }

 private:
  AllocationCounter* counter_;
  AllocationObserver* observer_;
};
}  // namespace

TEST(AllocationObserverTest, RecursiveAdd) {
  AllocationCounter counter;
  const Address kSomeObjectAddress = 8;

  UnusedObserver observer50(50);
  RecursiveAddObserver observer100(100, &counter, &observer50);

  counter.AddAllocationObserver(&observer100);

  CHECK_EQ(counter.NextBytes(), 100);
  counter.AdvanceAllocationObservers(90);
  counter.InvokeAllocationObservers(kSomeObjectAddress, 10, 10);

  CHECK_EQ(counter.NextBytes(),
           10 /* aligned_object_size */ + 50 /* smallest step size */);
}

namespace {
class RecursiveRemoveObserver : public AllocationObserver {
 public:
  explicit RecursiveRemoveObserver(size_t step_size, AllocationCounter* counter,
                                   AllocationObserver* observer)
      : AllocationObserver(step_size), counter_(counter), observer_(observer) {}

  void Step(int bytes_allocated, Address soon_object, size_t size) override {
    counter_->RemoveAllocationObserver(observer_);
  }

 private:
  AllocationCounter* counter_;
  AllocationObserver* observer_;
};
}  // namespace

TEST(AllocationObserverTest, RecursiveRemove) {
  AllocationCounter counter;
  const Address kSomeObjectAddress = 8;

  UnusedObserver observer75(75);
  RecursiveRemoveObserver observer50(50, &counter, &observer75);

  counter.AddAllocationObserver(&observer50);
  counter.AddAllocationObserver(&observer75);

  CHECK_EQ(counter.NextBytes(), 50);
  counter.AdvanceAllocationObservers(40);
  counter.InvokeAllocationObservers(kSomeObjectAddress, 10, 10);

  CHECK_EQ(counter.NextBytes(),
           10 /* aligned_object_size */ + 50 /* smallest step size */);
}

}  // namespace internal
}  // namespace v8
