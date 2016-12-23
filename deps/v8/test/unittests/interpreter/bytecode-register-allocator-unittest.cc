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

class BytecodeRegisterAllocatorTest : public TestWithIsolateAndZone {
 public:
  BytecodeRegisterAllocatorTest() : allocator_(0) {}
  ~BytecodeRegisterAllocatorTest() override {}

  BytecodeRegisterAllocator* allocator() { return &allocator_; }

 private:
  BytecodeRegisterAllocator allocator_;
};

TEST_F(BytecodeRegisterAllocatorTest, SimpleAllocations) {
  CHECK_EQ(allocator()->maximum_register_count(), 0);
  Register reg0 = allocator()->NewRegister();
  CHECK_EQ(reg0.index(), 0);
  CHECK_EQ(allocator()->maximum_register_count(), 1);
  CHECK_EQ(allocator()->next_register_index(), 1);
  CHECK(allocator()->RegisterIsLive(reg0));

  allocator()->ReleaseRegisters(0);
  CHECK(!allocator()->RegisterIsLive(reg0));
  CHECK_EQ(allocator()->maximum_register_count(), 1);
  CHECK_EQ(allocator()->next_register_index(), 0);

  reg0 = allocator()->NewRegister();
  Register reg1 = allocator()->NewRegister();
  CHECK_EQ(reg0.index(), 0);
  CHECK_EQ(reg1.index(), 1);
  CHECK(allocator()->RegisterIsLive(reg0));
  CHECK(allocator()->RegisterIsLive(reg1));
  CHECK_EQ(allocator()->maximum_register_count(), 2);
  CHECK_EQ(allocator()->next_register_index(), 2);

  allocator()->ReleaseRegisters(1);
  CHECK(allocator()->RegisterIsLive(reg0));
  CHECK(!allocator()->RegisterIsLive(reg1));
  CHECK_EQ(allocator()->maximum_register_count(), 2);
  CHECK_EQ(allocator()->next_register_index(), 1);
}

TEST_F(BytecodeRegisterAllocatorTest, RegisterListAllocations) {
  CHECK_EQ(allocator()->maximum_register_count(), 0);
  RegisterList reg_list = allocator()->NewRegisterList(3);
  CHECK_EQ(reg_list.first_register().index(), 0);
  CHECK_EQ(reg_list.register_count(), 3);
  CHECK_EQ(reg_list[0].index(), 0);
  CHECK_EQ(reg_list[1].index(), 1);
  CHECK_EQ(reg_list[2].index(), 2);
  CHECK_EQ(allocator()->maximum_register_count(), 3);
  CHECK_EQ(allocator()->next_register_index(), 3);
  CHECK(allocator()->RegisterIsLive(reg_list[2]));

  Register reg = allocator()->NewRegister();
  RegisterList reg_list_2 = allocator()->NewRegisterList(2);
  CHECK_EQ(reg.index(), 3);
  CHECK_EQ(reg_list_2.first_register().index(), 4);
  CHECK_EQ(reg_list_2.register_count(), 2);
  CHECK_EQ(reg_list_2[0].index(), 4);
  CHECK_EQ(reg_list_2[1].index(), 5);
  CHECK_EQ(allocator()->maximum_register_count(), 6);
  CHECK_EQ(allocator()->next_register_index(), 6);
  CHECK(allocator()->RegisterIsLive(reg));
  CHECK(allocator()->RegisterIsLive(reg_list_2[1]));

  allocator()->ReleaseRegisters(reg.index());
  CHECK(!allocator()->RegisterIsLive(reg));
  CHECK(!allocator()->RegisterIsLive(reg_list_2[0]));
  CHECK(!allocator()->RegisterIsLive(reg_list_2[1]));
  CHECK(allocator()->RegisterIsLive(reg_list[2]));
  CHECK_EQ(allocator()->maximum_register_count(), 6);
  CHECK_EQ(allocator()->next_register_index(), 3);

  RegisterList empty_reg_list = allocator()->NewRegisterList(0);
  CHECK_EQ(empty_reg_list.first_register().index(), 0);
  CHECK_EQ(empty_reg_list.register_count(), 0);
  CHECK_EQ(allocator()->maximum_register_count(), 6);
  CHECK_EQ(allocator()->next_register_index(), 3);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
