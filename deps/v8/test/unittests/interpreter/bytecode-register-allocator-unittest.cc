// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/bytecode-register-allocator.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class TemporaryRegisterAllocatorTest : public TestWithIsolateAndZone {
 public:
  TemporaryRegisterAllocatorTest() : allocator_(zone(), 0) {}
  ~TemporaryRegisterAllocatorTest() override {}
  TemporaryRegisterAllocator* allocator() { return &allocator_; }

 private:
  TemporaryRegisterAllocator allocator_;
};

TEST_F(TemporaryRegisterAllocatorTest, FirstAllocation) {
  CHECK_EQ(allocator()->allocation_count(), 0);
  int reg0_index = allocator()->BorrowTemporaryRegister();
  CHECK_EQ(reg0_index, 0);
  CHECK_EQ(allocator()->allocation_count(), 1);
  CHECK(allocator()->RegisterIsLive(Register(reg0_index)));
  allocator()->ReturnTemporaryRegister(reg0_index);
  CHECK(!allocator()->RegisterIsLive(Register(reg0_index)));
  CHECK_EQ(allocator()->allocation_count(), 1);
  CHECK(allocator()->first_temporary_register() == Register(0));
  CHECK(allocator()->last_temporary_register() == Register(0));
}

TEST_F(TemporaryRegisterAllocatorTest, SimpleAllocations) {
  for (int i = 0; i < 13; i++) {
    int reg_index = allocator()->BorrowTemporaryRegister();
    CHECK_EQ(reg_index, i);
    CHECK_EQ(allocator()->allocation_count(), i + 1);
  }
  for (int i = 0; i < 13; i++) {
    CHECK(allocator()->RegisterIsLive(Register(i)));
    allocator()->ReturnTemporaryRegister(i);
    CHECK(!allocator()->RegisterIsLive(Register(i)));
    int reg_index = allocator()->BorrowTemporaryRegister();
    CHECK_EQ(reg_index, i);
    CHECK_EQ(allocator()->allocation_count(), 13);
  }
  for (int i = 0; i < 13; i++) {
    CHECK(allocator()->RegisterIsLive(Register(i)));
    allocator()->ReturnTemporaryRegister(i);
    CHECK(!allocator()->RegisterIsLive(Register(i)));
  }
}

TEST_F(TemporaryRegisterAllocatorTest, SimpleRangeAllocation) {
  static const int kRunLength = 7;
  int start = allocator()->PrepareForConsecutiveTemporaryRegisters(kRunLength);
  CHECK(!allocator()->RegisterIsLive(Register(start)));
  for (int i = 0; i < kRunLength; i++) {
    CHECK(!allocator()->RegisterIsLive(Register(start + i)));
    allocator()->BorrowConsecutiveTemporaryRegister(start + i);
    CHECK(allocator()->RegisterIsLive(Register(start + i)));
  }
}

TEST_F(TemporaryRegisterAllocatorTest, RangeAllocationAbuttingFree) {
  static const int kFreeCount = 3;
  static const int kRunLength = 6;

  for (int i = 0; i < kFreeCount; i++) {
    int to_free = allocator()->BorrowTemporaryRegister();
    CHECK_EQ(to_free, i);
  }
  for (int i = 0; i < kFreeCount; i++) {
    allocator()->ReturnTemporaryRegister(i);
  }

  int start = allocator()->PrepareForConsecutiveTemporaryRegisters(kRunLength);
  CHECK(!allocator()->RegisterIsLive(Register(start)));
  for (int i = 0; i < kRunLength; i++) {
    CHECK(!allocator()->RegisterIsLive(Register(start + i)));
    allocator()->BorrowConsecutiveTemporaryRegister(start + i);
    CHECK(allocator()->RegisterIsLive(Register(start + i)));
  }
}

TEST_F(TemporaryRegisterAllocatorTest, RangeAllocationAbuttingHole) {
  static const int kPreAllocatedCount = 7;
  static const int kPreAllocatedFreeCount = 6;
  static const int kRunLength = 8;

  for (int i = 0; i < kPreAllocatedCount; i++) {
    int to_free = allocator()->BorrowTemporaryRegister();
    CHECK_EQ(to_free, i);
  }
  for (int i = 0; i < kPreAllocatedFreeCount; i++) {
    allocator()->ReturnTemporaryRegister(i);
  }
  int start = allocator()->PrepareForConsecutiveTemporaryRegisters(kRunLength);
  CHECK(!allocator()->RegisterIsLive(Register(start)));
  CHECK_EQ(start, kPreAllocatedCount);
  for (int i = 0; i < kRunLength; i++) {
    CHECK(!allocator()->RegisterIsLive(Register(start + i)));
    allocator()->BorrowConsecutiveTemporaryRegister(start + i);
    CHECK(allocator()->RegisterIsLive(Register(start + i)));
  }
}

TEST_F(TemporaryRegisterAllocatorTest, RangeAllocationAvailableInTemporaries) {
  static const int kNotRunLength = 13;
  static const int kRunLength = 8;

  // Allocate big batch
  for (int i = 0; i < kNotRunLength * 2 + kRunLength; i++) {
    int allocated = allocator()->BorrowTemporaryRegister();
    CHECK_EQ(allocated, i);
  }
  // Free every other register either side of target.
  for (int i = 0; i < kNotRunLength; i++) {
    if ((i & 2) == 1) {
      allocator()->ReturnTemporaryRegister(i);
      allocator()->ReturnTemporaryRegister(kNotRunLength + kRunLength + i);
    }
  }
  // Free all registers for target.
  for (int i = kNotRunLength; i < kNotRunLength + kRunLength; i++) {
    allocator()->ReturnTemporaryRegister(i);
  }

  int start = allocator()->PrepareForConsecutiveTemporaryRegisters(kRunLength);
  CHECK_EQ(start, kNotRunLength);
  for (int i = 0; i < kRunLength; i++) {
    CHECK(!allocator()->RegisterIsLive(Register(start + i)));
    allocator()->BorrowConsecutiveTemporaryRegister(start + i);
    CHECK(allocator()->RegisterIsLive(Register(start + i)));
  }
}

TEST_F(TemporaryRegisterAllocatorTest, RangeAvoidsTranslationBoundary) {
  int boundary = RegisterTranslator::DistanceToTranslationWindow(Register(0));
  int limit = boundary + 64;

  for (int run_length = 2; run_length < 32; run_length += 7) {
    ZoneVector<int> run_starts(zone());
    for (int start = 0; start < limit; start += run_length) {
      int run_start =
          allocator()->PrepareForConsecutiveTemporaryRegisters(run_length);
      run_starts.push_back(run_start);
      for (int i = 0; i < run_length; i++) {
        allocator()->BorrowConsecutiveTemporaryRegister(run_start + i);
      }
      CHECK(run_start >= boundary || run_start + run_length <= boundary);
    }
    for (size_t batch = 0; batch < run_starts.size(); batch++) {
      for (int i = run_starts[batch]; i < run_starts[batch] + run_length; i++) {
        allocator()->ReturnTemporaryRegister(i);
      }
    }
  }
}

TEST_F(TemporaryRegisterAllocatorTest, NotInRange) {
  for (int i = 0; i < 10; i++) {
    int reg = allocator()->BorrowTemporaryRegisterNotInRange(2, 5);
    CHECK(reg == i || (reg > 2 && reg == i + 4));
  }
  for (int i = 0; i < 10; i++) {
    if (i < 2) {
      allocator()->ReturnTemporaryRegister(i);
    } else {
      allocator()->ReturnTemporaryRegister(i + 4);
    }
  }
  int reg0 = allocator()->BorrowTemporaryRegisterNotInRange(0, 3);
  CHECK_EQ(reg0, 4);
  int reg1 = allocator()->BorrowTemporaryRegisterNotInRange(3, 10);
  CHECK_EQ(reg1, 2);
  int reg2 = allocator()->BorrowTemporaryRegisterNotInRange(2, 6);
  CHECK_EQ(reg2, 1);
  allocator()->ReturnTemporaryRegister(reg0);
  allocator()->ReturnTemporaryRegister(reg1);
  allocator()->ReturnTemporaryRegister(reg2);
}

class BytecodeRegisterAllocatorTest : public TestWithIsolateAndZone {
 public:
  BytecodeRegisterAllocatorTest() {}
  ~BytecodeRegisterAllocatorTest() override {}
};

TEST_F(BytecodeRegisterAllocatorTest, TemporariesRecycled) {
  BytecodeArrayBuilder builder(isolate(), zone(), 0, 0, 0);

  int first;
  {
    BytecodeRegisterAllocator allocator(zone(),
                                        builder.temporary_register_allocator());
    first = allocator.NewRegister().index();
    allocator.NewRegister();
    allocator.NewRegister();
    allocator.NewRegister();
  }

  int second;
  {
    BytecodeRegisterAllocator allocator(zone(),
                                        builder.temporary_register_allocator());
    second = allocator.NewRegister().index();
  }

  CHECK_EQ(first, second);
}

TEST_F(BytecodeRegisterAllocatorTest, ConsecutiveRegisters) {
  BytecodeArrayBuilder builder(isolate(), zone(), 0, 0, 0);
  BytecodeRegisterAllocator allocator(zone(),
                                      builder.temporary_register_allocator());
  allocator.PrepareForConsecutiveAllocations(4);
  Register reg0 = allocator.NextConsecutiveRegister();
  Register other = allocator.NewRegister();
  Register reg1 = allocator.NextConsecutiveRegister();
  Register reg2 = allocator.NextConsecutiveRegister();
  Register reg3 = allocator.NextConsecutiveRegister();
  USE(other);

  CHECK(Register::AreContiguous(reg0, reg1, reg2, reg3));
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
