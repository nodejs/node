// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/arm/assembler-arm-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/execution/simulator.h"
#include "src/utils/ostreams.h"
#include "test/common/assembler-tester.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {

#define __ tasm.

// If we are running on android and the output is not redirected (i.e. ends up
// in the android log) then we cannot find the error message in the output. This
// macro just returns the empty string in that case.
#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
#define ERROR_MESSAGE(msg) ""
#else
#define ERROR_MESSAGE(msg) msg
#endif

// Test the x64 assembler by compiling some simple functions into
// a buffer and executing them.  These tests do not initialize the
// V8 library, create a context, or use any V8 objects.

class TurboAssemblerTest : public TestWithIsolate {};

TEST_F(TurboAssemblerTest, TestHardAbort) {
  auto buffer = AllocateAssemblerBuffer();
  TurboAssembler tasm(nullptr, AssemblerOptions{}, CodeObjectRequired::kNo,
                      buffer->CreateView());
  __ set_abort_hard(true);

  __ Abort(AbortReason::kNoReason);

  CodeDesc desc;
  tasm.GetCode(nullptr, &desc);
  buffer->MakeExecutable();
  // We need an isolate here to execute in the simulator.
  auto f = GeneratedCode<void>::FromBuffer(isolate(), buffer->start());

  ASSERT_DEATH_IF_SUPPORTED({ f.Call(); }, ERROR_MESSAGE("abort: no reason"));
}

TEST_F(TurboAssemblerTest, TestCheck) {
  auto buffer = AllocateAssemblerBuffer();
  TurboAssembler tasm(nullptr, AssemblerOptions{}, CodeObjectRequired::kNo,
                      buffer->CreateView());
  __ set_abort_hard(true);

  // Fail if the first parameter is 17.
  __ Move32BitImmediate(r1, Operand(17));
  __ cmp(r0, r1);  // 1st parameter is in {r0}.
  __ Check(Condition::ne, AbortReason::kNoReason);
  __ Ret();

  CodeDesc desc;
  tasm.GetCode(nullptr, &desc);
  buffer->MakeExecutable();
  // We need an isolate here to execute in the simulator.
  auto f = GeneratedCode<void, int>::FromBuffer(isolate(), buffer->start());

  f.Call(0);
  f.Call(18);
  ASSERT_DEATH_IF_SUPPORTED({ f.Call(17); }, ERROR_MESSAGE("abort: no reason"));
}

struct MoveObjectAndSlotTestCase {
  const char* comment;
  Register dst_object;
  Register dst_slot;
  Register object;
  Register offset_register = no_reg;
};

const MoveObjectAndSlotTestCase kMoveObjectAndSlotTestCases[] = {
    {"no overlap", r0, r1, r2},
    {"no overlap", r0, r1, r2, r3},

    {"object == dst_object", r2, r1, r2},
    {"object == dst_object", r2, r1, r2, r3},

    {"object == dst_slot", r1, r2, r2},
    {"object == dst_slot", r1, r2, r2, r3},

    {"offset == dst_object", r0, r1, r2, r0},

    {"offset == dst_object && object == dst_slot", r0, r1, r1, r0},

    {"offset == dst_slot", r0, r1, r2, r1},

    {"offset == dst_slot && object == dst_object", r0, r1, r0, r1}};

// Make sure we include offsets that cannot be encoded in an add instruction.
const int kOffsets[] = {0, 42, kMaxRegularHeapObjectSize, 0x101001};

template <typename T>
class TurboAssemblerTestWithParam : public TurboAssemblerTest,
                                    public ::testing::WithParamInterface<T> {};

using TurboAssemblerTestMoveObjectAndSlot =
    TurboAssemblerTestWithParam<MoveObjectAndSlotTestCase>;

TEST_P(TurboAssemblerTestMoveObjectAndSlot, MoveObjectAndSlot) {
  const MoveObjectAndSlotTestCase test_case = GetParam();
  TRACED_FOREACH(int32_t, offset, kOffsets) {
    auto buffer = AllocateAssemblerBuffer();
    TurboAssembler tasm(nullptr, AssemblerOptions{}, CodeObjectRequired::kNo,
                        buffer->CreateView());
    __ Push(r0);
    __ Move(test_case.object, r1);

    Register src_object = test_case.object;
    Register dst_object = test_case.dst_object;
    Register dst_slot = test_case.dst_slot;

    Operand offset_operand(0);
    if (test_case.offset_register == no_reg) {
      offset_operand = Operand(offset);
    } else {
      __ mov(test_case.offset_register, Operand(offset));
      offset_operand = Operand(test_case.offset_register);
    }

    std::stringstream comment;
    comment << "-- " << test_case.comment << ": MoveObjectAndSlot("
            << dst_object << ", " << dst_slot << ", " << src_object << ", ";
    if (test_case.offset_register == no_reg) {
      comment << "#" << offset;
    } else {
      comment << test_case.offset_register;
    }
    comment << ") --";
    __ RecordComment(comment.str().c_str());
    __ MoveObjectAndSlot(dst_object, dst_slot, src_object, offset_operand);
    __ RecordComment("--");

    // The `result` pointer was saved on the stack.
    UseScratchRegisterScope temps(&tasm);
    Register scratch = temps.Acquire();
    __ Pop(scratch);
    __ str(dst_object, MemOperand(scratch));
    __ str(dst_slot, MemOperand(scratch, kSystemPointerSize));

    __ Ret();

    CodeDesc desc;
    tasm.GetCode(nullptr, &desc);
    if (FLAG_print_code) {
      Handle<Code> code =
          Factory::CodeBuilder(isolate(), desc, Code::STUB).Build();
      StdoutStream os;
      code->Print(os);
    }

    buffer->MakeExecutable();
    // We need an isolate here to execute in the simulator.
    auto f = GeneratedCode<void, byte**, byte*>::FromBuffer(isolate(),
                                                            buffer->start());

    byte* object = new byte[offset];
    byte* result[] = {nullptr, nullptr};

    f.Call(result, object);

    // The first element must be the address of the object, and the second the
    // slot addressed by `offset`.
    EXPECT_EQ(result[0], &object[0]);
    EXPECT_EQ(result[1], &object[offset]);

    delete[] object;
  }
}

INSTANTIATE_TEST_SUITE_P(TurboAssemblerTest,
                         TurboAssemblerTestMoveObjectAndSlot,
                         ::testing::ValuesIn(kMoveObjectAndSlotTestCases));

#undef __
#undef ERROR_MESSAGE

}  // namespace internal
}  // namespace v8
