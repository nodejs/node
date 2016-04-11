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
  BytecodeRegisterAllocatorTest() {}
  ~BytecodeRegisterAllocatorTest() override {}
};


TEST_F(BytecodeRegisterAllocatorTest, TemporariesRecycled) {
  BytecodeArrayBuilder builder(isolate(), zone());
  builder.set_parameter_count(0);
  builder.set_locals_count(0);
  builder.set_context_count(0);

  int first;
  {
    BytecodeRegisterAllocator temporaries(&builder);
    first = temporaries.NewRegister().index();
    temporaries.NewRegister();
    temporaries.NewRegister();
    temporaries.NewRegister();
  }

  int second;
  {
    BytecodeRegisterAllocator temporaries(&builder);
    second = temporaries.NewRegister().index();
  }

  CHECK_EQ(first, second);
}


TEST_F(BytecodeRegisterAllocatorTest, ConsecutiveRegisters) {
  BytecodeArrayBuilder builder(isolate(), zone());
  builder.set_parameter_count(0);
  builder.set_locals_count(0);
  builder.set_context_count(0);

  BytecodeRegisterAllocator temporaries(&builder);
  temporaries.PrepareForConsecutiveAllocations(4);
  Register reg0 = temporaries.NextConsecutiveRegister();
  Register other = temporaries.NewRegister();
  Register reg1 = temporaries.NextConsecutiveRegister();
  Register reg2 = temporaries.NextConsecutiveRegister();
  Register reg3 = temporaries.NextConsecutiveRegister();
  USE(other);

  CHECK(Register::AreContiguous(reg0, reg1, reg2, reg3));
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
