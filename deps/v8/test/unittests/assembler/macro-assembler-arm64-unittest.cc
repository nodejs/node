// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/arm64/macro-assembler-arm64-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/execution/simulator.h"
#include "src/heap/factory.h"
#include "src/heap/heap-allocator.h"
#include "src/objects/smi.h"
#include "src/roots/roots-inl.h"
#include "src/utils/ostreams.h"
#include "test/common/assembler-tester.h"
#include "test/common/flag-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {

#define __ masm.

const char* ErrorMessage(const char* msg) {
// If we are running on android and the output is not redirected (i.e. ends up
// in the android log) then we cannot find the error message in the output. This
// macro just returns the empty string in that case.
#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
  return "";
#else
  // We only print the abort reason if debug code is enabled. Otherwise we just
  // stop execution. Return an empty string in that case.
  return v8_flags.debug_code ? msg : "";
#endif
}

// Test the x64 assembler by compiling some simple functions into
// a buffer and executing them.  These tests do not initialize the
// V8 library, create a context, or use any V8 objects.

class MacroAssemblerTest : public TestWithIsolate {};

TEST_F(MacroAssemblerTest, TestHardAbort) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);

  {
    AssemblerBufferWriteScope rw_scope(*buffer);

    __ CodeEntry();

    __ Abort(AbortReason::kNoReason);

    CodeDesc desc;
    masm.GetCode(isolate(), &desc);
  }
  // We need an isolate here to execute in the simulator.
  auto f = GeneratedCode<void>::FromBuffer(isolate(), buffer->start());

  ASSERT_DEATH_IF_SUPPORTED({ f.Call(); }, ErrorMessage("abort: no reason"));
}

TEST_F(MacroAssemblerTest, TestCheck) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);

  {
    AssemblerBufferWriteScope rw_scope(*buffer);

    __ CodeEntry();

    // Fail if the first parameter is 17.
    __ Mov(w1, Immediate(17));
    __ Cmp(w0, w1);  // 1st parameter is in {w0}.
    __ Check(Condition::ne, AbortReason::kNoReason);
    __ Ret();

    CodeDesc desc;
    masm.GetCode(isolate(), &desc);
  }
  // We need an isolate here to execute in the simulator.
  auto f = GeneratedCode<void, int>::FromBuffer(isolate(), buffer->start());

  f.Call(0);
  f.Call(18);
  ASSERT_DEATH_IF_SUPPORTED({ f.Call(17); }, ErrorMessage("abort: no reason"));
}

#ifdef V8_ENABLE_DEBUG_CODE
TEST_F(MacroAssemblerTest, TestAssertZeroExtended) {
  FLAG_SCOPE(debug_code);
  FLAG_SCOPE(slow_debug_code);

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);

  {
    AssemblerBufferWriteScope rw_scope(*buffer);

    __ CodeEntry();

    __ AssertZeroExtended(x0);
    __ Ret();

    CodeDesc desc;
    masm.GetCode(isolate(), &desc);
  }
  // We need an isolate here to execute in the simulator.
  auto f =
      GeneratedCode<void, uint64_t>::FromBuffer(isolate(), buffer->start());

  f.Call(0);
  f.Call(42);
  f.Call(0xFFFFFFFFULL);
  ASSERT_DEATH_IF_SUPPORTED(
      { f.Call(0x100000000ULL); },
      ErrorMessage("abort: 32 bit value in register is not zero-extended"));
  ASSERT_DEATH_IF_SUPPORTED(
      { f.Call(0xFFFFFFFFFFFFFFFFULL); },
      ErrorMessage("abort: 32 bit value in register is not zero-extended"));
  ASSERT_DEATH_IF_SUPPORTED(
      { f.Call(0x8000000000000000ULL); },
      ErrorMessage("abort: 32 bit value in register is not zero-extended"));

  {
    auto buffer_w = AllocateAssemblerBuffer();
    MacroAssembler masm_w(isolate(), AssemblerOptions{},
                          CodeObjectRequired{false}, buffer_w->CreateView());
    masm_w.set_root_array_available(false);
    masm_w.set_abort_hard(true);

    {
      AssemblerBufferWriteScope rw_scope(*buffer_w);

      masm_w.CodeEntry();
      masm_w.AssertZeroExtended(w0);
      masm_w.Ret();

      CodeDesc desc;
      masm_w.GetCode(isolate(), &desc);
    }
    auto f_w =
        GeneratedCode<void, uint64_t>::FromBuffer(isolate(), buffer_w->start());

    f_w.Call(0);
    f_w.Call(42);
    f_w.Call(0xFFFFFFFFULL);
    ASSERT_DEATH_IF_SUPPORTED(
        { f_w.Call(0x100000000ULL); },
        ErrorMessage("abort: 32 bit value in register is not zero-extended"));
  }
}
#endif  // V8_ENABLE_DEBUG_CODE

TEST_F(MacroAssemblerTest, CompareAndBranch) {
  const int kTestCases[] = {-42, 0, 42};
  static_assert(Condition::eq == 0);
  static_assert(Condition::le == 13);
  TRACED_FORRANGE(int, cc, 0, 13) {  // All conds except al and nv
    Condition cond = static_cast<Condition>(cc);
    TRACED_FOREACH(int, imm, kTestCases) {
      auto buffer = AllocateAssemblerBuffer();
      MacroAssembler masm(isolate(), AssemblerOptions{},
                          CodeObjectRequired{false}, buffer->CreateView());
      __ set_root_array_available(false);
      __ set_abort_hard(true);

      {
        AssemblerBufferWriteScope rw_scope(*buffer);

        __ CodeEntry();

        Label start, lab;
        __ Bind(&start);
        __ CompareAndBranch(x0, Immediate(imm), cond, &lab);
        if (imm == 0 &&
            ((cond == eq) || (cond == ne) || (cond == hi) || (cond == ls) ||
             (cond == lt) || (cond == ge))) {  // One instruction generated
          ASSERT_EQ(kInstrSize, __ SizeOfCodeGeneratedSince(&start));
        } else {  // Two instructions generated
          ASSERT_EQ(static_cast<uint8_t>(2 * kInstrSize),
                    __ SizeOfCodeGeneratedSince(&start));
        }
        __ Cmp(x0, Immediate(imm));
        __ Check(NegateCondition(cond),
                 AbortReason::kNoReason);  // cond must not hold
        __ Ret();
        __ Bind(&lab);  // Branch leads here
        __ Cmp(x0, Immediate(imm));
        __ Check(cond, AbortReason::kNoReason);  // cond must hold
        __ Ret();

        CodeDesc desc;
        masm.GetCode(isolate(), &desc);
      }
      // We need an isolate here to execute in the simulator.
      auto f = GeneratedCode<void, int>::FromBuffer(isolate(), buffer->start());

      TRACED_FOREACH(int, n, kTestCases) { f.Call(n); }
    }
  }
}

TEST_F(MacroAssemblerTest, DispatchHandlePoolEntries) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  const uint32_t kHandleA = 0x1200;
  const uint32_t kHandleB = 0x3400;

  {
    AssemblerBufferWriteScope rw_scope(*buffer);

    __ Mov(w0, Immediate(kHandleA, RelocInfo::JS_DISPATCH_HANDLE));
    __ Mov(w1, Immediate(kHandleA, RelocInfo::JS_DISPATCH_HANDLE));
    __ Mov(w2, Immediate(kHandleB, RelocInfo::JS_DISPATCH_HANDLE));

    CodeDesc desc;
    masm.GetCode(isolate(), &desc);
  }

  Instruction* load_a = reinterpret_cast<Instruction*>(buffer->start());
  Instruction* load_a_again = load_a->following();
  Instruction* load_b = load_a_again->following();
  ASSERT_TRUE(load_a->IsLdrLiteralW());
  ASSERT_TRUE(load_a_again->IsLdrLiteralW());
  ASSERT_TRUE(load_b->IsLdrLiteralW());
  EXPECT_EQ(load_a->ImmPCOffsetTarget(), load_a_again->ImmPCOffsetTarget());
  EXPECT_EQ(kHandleA,
            *reinterpret_cast<uint32_t*>(load_a->ImmPCOffsetTarget()));
  EXPECT_EQ(kHandleB,
            *reinterpret_cast<uint32_t*>(load_b->ImmPCOffsetTarget()));

  // The pool holds one 4-byte entry per handle.
  Instruction* marker = load_b->following();
  ASSERT_TRUE(marker->IsUdf());
  EXPECT_EQ(2, marker->ImmUdf());
}

struct MoveObjectAndSlotTestCase {
  const char* comment;
  Register dst_object;
  Register dst_slot;
  Register object;
  Register offset_register = no_reg;
  Extend offset_extend = NO_EXTEND;
};

const MoveObjectAndSlotTestCase kMoveObjectAndSlotTestCases[] = {
    {"no overlap", x0, x1, x2},
    {"no overlap", x0, x1, x2, x3},
    {"no overlap", x0, x1, x2, w3, UXTW},

    {"object == dst_object", x2, x1, x2},
    {"object == dst_object", x2, x1, x2, x3},
    {"object == dst_object", x2, x1, x2, w3, UXTW},

    {"object == dst_slot", x1, x2, x2},
    {"object == dst_slot", x1, x2, x2, x3},
    {"object == dst_slot", x1, x2, x2, w3, UXTW},

    {"offset == dst_object", x0, x1, x2, x0},
    {"offset == dst_object", x0, x1, x2, w0, UXTW},

    {"offset == dst_object && object == dst_slot", x0, x1, x1, x0},
    {"offset == dst_object && object == dst_slot", x0, x1, x1, w0, UXTW},

    {"offset == dst_slot", x0, x1, x2, x1},
    {"offset == dst_slot", x0, x1, x2, w1, UXTW},

    {"offset == dst_slot && object == dst_object", x0, x1, x0, x1},
    {"offset == dst_slot && object == dst_object", x0, x1, x0, w1, UXTW}};

// Make sure we include offsets that cannot be encoded in an add instruction.
const int kOffsets[] = {0, 42, kMaxRegularHeapObjectSize, 0x101001};

TEST_F(MacroAssemblerTest, MaybeJumpIfReadOnlyOrSmallSmi) {
  // The read-only shortcut of the write barrier must take the jump for
  // read-only objects and small Smis, and must not for other heap objects. It
  // tests the compressed half of the tagged value, so a decompressed pointer
  // (cage base plus offset) has to match as well.
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  {
    AssemblerBufferWriteScope rw_scope(*buffer);

    __ CodeEntry();

    Label skip;
    __ MaybeJumpIfReadOnlyOrSmallSmi(x0, &skip);
    __ Mov(x0, 0);
    __ Ret();
    __ Bind(&skip);
    __ Mov(x0, 1);
    __ Ret();

    CodeDesc desc;
    masm.GetCode(isolate(), &desc);
  }
  auto f = GeneratedCode<int, Address>::FromBuffer(isolate(), buffer->start());

  HandleScope scope(isolate());
  auto array = isolate()->factory()->NewFixedArray(1);
#if V8_STATIC_ROOTS_BOOL && CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
  const int kReadOnlyOrSmallSmi = 1;
#else
  const int kReadOnlyOrSmallSmi = 0;
#endif
  EXPECT_EQ(kReadOnlyOrSmallSmi,
            f.Call(ReadOnlyRoots(isolate()).undefined_value().ptr()));
  EXPECT_EQ(kReadOnlyOrSmallSmi, f.Call(Smi::FromInt(42).ptr()));
  EXPECT_EQ(0, f.Call((*array).ptr()));
  EXPECT_EQ(0, f.Call(Smi::FromInt(-1).ptr()));
#if V8_STATIC_ROOTS_BOOL && CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
  // The shortcut covers the whole reservation: the last tagged address in it
  // takes the jump and the first one past it does not.
  const Address reservation_end =
      isolate()->cage_base() + kContiguousReadOnlyReservationSize;
  EXPECT_EQ(1, f.Call(reservation_end - kTaggedSize + kHeapObjectTag));
  EXPECT_EQ(0, f.Call(reservation_end + kHeapObjectTag));
#endif
}

TEST_F(MacroAssemblerTest, PreCheckSkippedWriteBarrier) {
  // The verifier's fast path must accept a store into the most recent young
  // allocation and a store of a read-only value, and must send everything
  // else to the C++ verifier.
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  {
    AssemblerBufferWriteScope rw_scope(*buffer);

    __ CodeEntry();
    // The pre-check reads the allocation state through the root register,
    // which is callee-saved.
    __ Push(kRootRegister, padreg);
    __ Mov(kRootRegister, x2);

    Label ok, done;
    __ PreCheckSkippedWriteBarrier(x0, x1, x3, &ok);
    __ Mov(x0, 0);
    __ B(&done);
    __ Bind(&ok);
    __ Mov(x0, 1);
    __ Bind(&done);
    __ Pop(padreg, kRootRegister);
    __ Ret();

    CodeDesc desc;
    masm.GetCode(isolate(), &desc);
  }
  auto f = GeneratedCode<int, Address, Address, Address>::FromBuffer(
      isolate(), buffer->start());
  const Address root = isolate()->isolate_root();

  HandleScope scope(isolate());
  auto object = isolate()->factory()->NewFixedArray(1);
  auto value = isolate()->factory()->NewFixedArray(1);
  HeapAllocator* allocator = isolate()->heap()->allocator();
  // A store into the most recent young allocation may skip its barrier.
  allocator->set_last_young_allocation((*object).address());
  EXPECT_EQ(1, f.Call((*object).ptr(), (*value).ptr(), root));
  // Otherwise only a read-only value may.
  allocator->set_last_young_allocation(kNullAddress);
  EXPECT_EQ(0, f.Call((*object).ptr(), (*value).ptr(), root));
  EXPECT_EQ(1, f.Call((*object).ptr(),
                      ReadOnlyRoots(isolate()).undefined_value().ptr(), root));
#if CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
  // The check covers the whole reservation: the last tagged address in it
  // passes and the first one past it does not.
  const Address reservation_end =
      isolate()->cage_base() + kContiguousReadOnlyReservationSize;
  EXPECT_EQ(1, f.Call((*object).ptr(),
                      reservation_end - kTaggedSize + kHeapObjectTag, root));
  EXPECT_EQ(0, f.Call((*object).ptr(), reservation_end + kHeapObjectTag, root));
#endif
}

template <typename T>
class MacroAssemblerTestWithParam : public MacroAssemblerTest,
                                    public ::testing::WithParamInterface<T> {};

using MacroAssemblerTestMoveObjectAndSlot =
    MacroAssemblerTestWithParam<MoveObjectAndSlotTestCase>;

TEST_P(MacroAssemblerTestMoveObjectAndSlot, MoveObjectAndSlot) {
  const MoveObjectAndSlotTestCase test_case = GetParam();
  TRACED_FOREACH(int32_t, offset, kOffsets) {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(isolate(), AssemblerOptions{},
                        CodeObjectRequired{false}, buffer->CreateView());

    {
      AssemblerBufferWriteScope rw_buffer_scope(*buffer);

      __ CodeEntry();
      __ Push(x0, padreg);
      __ Mov(test_case.object, x1);

      Register src_object = test_case.object;
      Register dst_object = test_case.dst_object;
      Register dst_slot = test_case.dst_slot;

      Operand offset_operand(0);
      if (test_case.offset_register == no_reg) {
        offset_operand = Operand(offset);
      } else {
        __ Mov(test_case.offset_register, Operand(offset));
        if (test_case.offset_extend == NO_EXTEND) {
          offset_operand = Operand(test_case.offset_register);
        } else {
          offset_operand =
              Operand(test_case.offset_register, test_case.offset_extend);
        }
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
      UseScratchRegisterScope temps(&masm);
      Register scratch = temps.AcquireX();
      __ Pop(padreg, scratch);
      __ Str(dst_object, MemOperand(scratch));
      __ Str(dst_slot, MemOperand(scratch, kSystemPointerSize));

      __ Ret();

      CodeDesc desc;
      masm.GetCode(static_cast<LocalIsolate*>(nullptr), &desc);
      if (v8_flags.print_code) {
        Handle<Code> code =
            Factory::CodeBuilder(isolate(), desc, CodeKind::FOR_TESTING)
                .Build();
        StdoutStream os;
        Print(*code, os);
      }
    }

    // We need an isolate here to execute in the simulator.
    auto f = GeneratedCode<void, uint8_t**, uint8_t*>::FromBuffer(
        isolate(), buffer->start());

    uint8_t* object = new uint8_t[offset];
    uint8_t* result[] = {nullptr, nullptr};

    f.Call(result, object);

    // The first element must be the address of the object, and the second the
    // slot addressed by `offset`.
    EXPECT_EQ(result[0], &object[0]);
    EXPECT_EQ(result[1], &object[offset]);

    delete[] object;
  }
}

INSTANTIATE_TEST_SUITE_P(MacroAssemblerTest,
                         MacroAssemblerTestMoveObjectAndSlot,
                         ::testing::ValuesIn(kMoveObjectAndSlotTestCases));

#ifdef V8_COMPRESS_POINTERS
TEST_F(MacroAssemblerTest, ConstantPoolAlignmentGap) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  Handle<HeapObject> object = isolate()->factory()->undefined_value();

  {
    AssemblerBufferWriteScope rw_scope(*buffer);

    __ Mov(x0, Operand(object));
    __ Mov(w1, Operand(object, RelocInfo::COMPRESSED_EMBEDDED_OBJECT));

    CodeDesc desc;
    masm.GetCode(isolate(), &desc);
  }

  Instruction* load64 = reinterpret_cast<Instruction*>(buffer->start());
  Instruction* load32 = load64->following();
  Instruction* marker = load32->following();
  ASSERT_TRUE(load64->IsLdrLiteralX());
  ASSERT_TRUE(load32->IsLdrLiteralW());
  ASSERT_TRUE(marker->IsUdf());

  // The 32-bit entry fills the alignment gap after the marker.
  Instruction* gap = marker->following();
  EXPECT_EQ(gap, load32->ImmPCOffsetTarget());
  EXPECT_EQ(gap->following(), load64->ImmPCOffsetTarget());
  EXPECT_EQ(3, marker->ImmUdf());
}
#endif  // V8_COMPRESS_POINTERS

#undef __

}  // namespace internal
}  // namespace v8
