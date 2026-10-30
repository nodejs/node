// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#ifdef V8_ENABLE_MAGLEV

#include "src/api/api-inl.h"
#include "src/execution/simulator.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-assembler.h"
#include "src/maglev/maglev-code-generator.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compiler.h"
#include "src/maglev/maglev-graph.h"
#include "test/unittests/maglev/maglev-test.h"

namespace v8 {
namespace internal {
namespace maglev {

class MaglevAssemblerTest : public MaglevTest {
 public:
  MaglevAssemblerTest()
      : MaglevTest(),
        codegen_state(nullptr, nullptr, nullptr, 0),
        as(isolate(), zone(), &codegen_state) {
#if V8_TARGET_ARCH_PPC64
    // Default scratch list {r26, ip} includes callee-saved r26. Since
    // test-generated code has no prologue, restrict to volatile ip only.
    *as.GetScratchRegisterList() = RegList{ip};
#endif
  }

  void FinalizeAndRun(Label* pass, Label* fail) {
    as.bind(pass);
    as.Ret();
    as.bind(fail);
    as.AssertUnreachable(AbortReason::kNoReason);
    CodeDesc desc;
    as.GetCode(isolate(), &desc);
    Factory::CodeBuilder build(isolate(), desc, CodeKind::FOR_TESTING);
    auto res = build.TryBuild().ToHandleChecked();
    using Function = GeneratedCode<Address()>;
    auto fun = Function::FromAddress(isolate(), res->instruction_start());
    fun.Call();
  }

  MaglevCodeGenState codegen_state;
  MaglevAssembler as;
};

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToUint32One) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, 1.0);
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToUint32(kReturnRegister0, kFPReturnRegister0,
                               &cannot_convert);
  as.Cmp(kReturnRegister0, 1);
  as.Assert(Condition::kEqual, AbortReason::kNoReason);
  as.jmp(&can_convert);
  FinalizeAndRun(&can_convert, &cannot_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToUint32Zero) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, 0.0);
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToUint32(kReturnRegister0, kFPReturnRegister0,
                               &cannot_convert);
  as.Cmp(kReturnRegister0, 0);
  as.Assert(Condition::kEqual, AbortReason::kNoReason);
  as.jmp(&can_convert);
  FinalizeAndRun(&can_convert, &cannot_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToUint32Large) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, std::numeric_limits<uint32_t>::max());
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToUint32(kReturnRegister0, kFPReturnRegister0,
                               &cannot_convert);
  as.Cmp(kReturnRegister0, std::numeric_limits<uint32_t>::max());
  as.Assert(Condition::kEqual, AbortReason::kNoReason);
  as.jmp(&can_convert);
  FinalizeAndRun(&can_convert, &cannot_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToUint32TooLarge) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0,
          static_cast<double>(std::numeric_limits<uint32_t>::max()) + 1.0);
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToUint32(kReturnRegister0, kFPReturnRegister0,
                               &cannot_convert);
  as.jmp(&can_convert);
  FinalizeAndRun(&cannot_convert, &can_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToUint32Negative) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, -1.0);
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToUint32(kReturnRegister0, kFPReturnRegister0,
                               &cannot_convert);
  as.jmp(&can_convert);
  FinalizeAndRun(&cannot_convert, &can_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToUint32NegativeZero) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, -0.0);
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToUint32(kReturnRegister0, kFPReturnRegister0,
                               &cannot_convert);
  as.jmp(&can_convert);
  FinalizeAndRun(&cannot_convert, &can_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToUint32NotItegral) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, 1.1);
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToUint32(kReturnRegister0, kFPReturnRegister0,
                               &cannot_convert);
  as.jmp(&can_convert);
  FinalizeAndRun(&cannot_convert, &can_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32One) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, 1.0);
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.Cmp(kReturnRegister0, 1);
  as.Assert(Condition::kEqual, AbortReason::kNoReason);
  as.jmp(&can_convert);
  FinalizeAndRun(&can_convert, &cannot_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32MinusOne) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, -1.0);
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.Cmp(kReturnRegister0, static_cast<uint32_t>(-1));
  as.Assert(Condition::kEqual, AbortReason::kNoReason);
  as.jmp(&can_convert);
  FinalizeAndRun(&can_convert, &cannot_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32Zero) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, 0.0);
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.Cmp(kReturnRegister0, 0);
  as.Assert(Condition::kEqual, AbortReason::kNoReason);
  as.jmp(&can_convert);
  FinalizeAndRun(&can_convert, &cannot_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32Large) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, std::numeric_limits<int32_t>::max());
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.Cmp(kReturnRegister0, std::numeric_limits<int32_t>::max());
  as.Assert(Condition::kEqual, AbortReason::kNoReason);
  as.jmp(&can_convert);
  FinalizeAndRun(&can_convert, &cannot_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32Small) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, std::numeric_limits<int32_t>::min());
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.Cmp(kReturnRegister0,
         static_cast<uint32_t>(std::numeric_limits<int32_t>::min()));
  as.Assert(Condition::kEqual, AbortReason::kNoReason);
  as.jmp(&can_convert);
  FinalizeAndRun(&can_convert, &cannot_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32NegativeZero) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, -0.0);
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.jmp(&can_convert);
  FinalizeAndRun(&cannot_convert, &can_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32NotItegral) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, 1.1);
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.jmp(&can_convert);
  FinalizeAndRun(&cannot_convert, &can_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32TooLarge) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0,
          static_cast<double>(std::numeric_limits<int32_t>::max()) + 1);
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.jmp(&can_convert);
  FinalizeAndRun(&cannot_convert, &can_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32TooSmall) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0,
          static_cast<double>(std::numeric_limits<int32_t>::min()) - 1);
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.jmp(&can_convert);
  FinalizeAndRun(&cannot_convert, &can_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32Denormal) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, std::numeric_limits<double>::denorm_min());
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.jmp(&can_convert);
  FinalizeAndRun(&cannot_convert, &can_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32DenormalNegative) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, -std::numeric_limits<double>::denorm_min());
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.jmp(&can_convert);
  FinalizeAndRun(&cannot_convert, &can_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32NaN) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, std::numeric_limits<double>::quiet_NaN());
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.jmp(&can_convert);
  FinalizeAndRun(&cannot_convert, &can_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32Inf) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, std::numeric_limits<double>::infinity());
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.jmp(&can_convert);
  FinalizeAndRun(&cannot_convert, &can_convert);
}

TEST_F(MaglevAssemblerTest, TryTruncateDoubleToInt32InfNegative) {
  as.CodeEntry();
  as.Move(kFPReturnRegister0, -std::numeric_limits<double>::infinity());
  Label can_convert, cannot_convert;
  as.TryTruncateDoubleToInt32(kReturnRegister0, kFPReturnRegister0,
                              &cannot_convert);
  as.jmp(&can_convert);
  FinalizeAndRun(&cannot_convert, &can_convert);
}

#if V8_TARGET_ARCH_LOONG64 || V8_TARGET_ARCH_RISCV64
TEST_F(MaglevAssemblerTest, SetSlotAddressForFixedArrayElementLargeIndex) {
  as.CodeEntry();
  intptr_t base = intptr_t{0x300000000};
  uint32_t index = static_cast<uint32_t>(std::numeric_limits<int32_t>::min());
  as.Move(kReturnRegister1, base);
  as.Move(kReturnRegister2, static_cast<int32_t>(index));
  as.SetSlotAddressForFixedArrayElement(kReturnRegister0, kReturnRegister1,
                                        kReturnRegister2);
  intptr_t expected = base + OFFSET_OF_DATA_START(FixedArray) - kHeapObjectTag +
                      (intptr_t{index} << kTaggedSizeLog2);
  as.Move(kReturnRegister1, expected);
  Label pass, fail;
  as.CompareIntPtrAndJumpIf(kReturnRegister0, kReturnRegister1, kEqual, &pass);
  as.jmp(&fail);
  FinalizeAndRun(&pass, &fail);
}
#endif  // V8_TARGET_ARCH_LOONG64 || V8_TARGET_ARCH_RISCV64

#if V8_TARGET_ARCH_ARM64
// Compares against read-only roots, which arm64 reads as immediates under
// static roots instead of loading them from the constant pool.
class MaglevAssemblerReadOnlyCompareTest : public MaglevAssemblerTest {
 public:
  // Test code has no prologue. A compare against a root reads the root
  // register, and under --debug-code checks the cage base register, so set
  // both up as JSEntry does, saving the callee-saved registers, and restore
  // them before returning.
  void SetUpRootRegisters() {
#ifdef V8_COMPRESS_POINTERS
    as.MacroAssembler::Push(kRootRegister, kPtrComprCageBaseRegister);
    as.Move(kRootRegister, ExternalReference::isolate_root(isolate()));
    as.LoadRootRelative(kPtrComprCageBaseRegister,
                        IsolateData::cage_base_offset());
#else
    as.MacroAssembler::Push(kRootRegister, padreg);
    as.Move(kRootRegister, ExternalReference::isolate_root(isolate()));
#endif
  }
  void RestoreRootRegisters() {
#ifdef V8_COMPRESS_POINTERS
    as.MacroAssembler::Pop(kPtrComprCageBaseRegister, kRootRegister);
#else
    as.MacroAssembler::Pop(padreg, kRootRegister);
#endif
  }

  // The code bytes that {emit} adds, to check the shape of a compare and its
  // branch.
  template <typename Emit>
  int EmittedBytes(Emit emit) {
    int start = as.pc_offset();
    emit();
    return as.pc_offset() - start;
  }

  // Whether a read-only root compares as `cmp w, #imm` and branches, without
  // a pool load: under static roots, unless --debug-code adds its checks.
  bool ExpectImmediateCompare() {
    return V8_STATIC_ROOTS_BOOL && !v8_flags.debug_code;
  }
};

TEST_F(MaglevAssemblerReadOnlyCompareTest, MapCompareReadOnlyMap) {
  // A FixedArray's map and the COW map are read-only; a JSObject's map is not.
  Handle<FixedArray> array = isolate()->factory()->NewFixedArray(2);
  Handle<Map> map = isolate()->factory()->fixed_array_map();
  Handle<Map> cow_map = isolate()->factory()->fixed_cow_array_map();
  Handle<JSObject> object =
      isolate()->factory()->NewJSObject(isolate()->object_function());
  Handle<Map> object_map(object->map(), isolate());
  Label pass, fail;
  as.CodeEntry();
  SetUpRootRegisters();
  MaglevAssembler::TemporaryRegisterScope temps(&as);
  as.Move(kReturnRegister0, array);
  MapCompare map_compare(&as, kReturnRegister0, 3);
  int bytes =
      EmittedBytes([&] { map_compare.Generate(cow_map, kEqual, &fail); });
  map_compare.Generate(object_map, kEqual, &fail);
  map_compare.Generate(map, kNotEqual, &fail);
  RestoreRootRegisters();
  as.Jump(&pass);
  if (ExpectImmediateCompare()) EXPECT_EQ(2 * kInstrSize, bytes);
  FinalizeAndRun(&pass, &fail);
}

TEST_F(MaglevAssemblerReadOnlyCompareTest, CompareTaggedAndJumpIfReadOnly) {
  // true and null sit low in the read-only heap, the "object" string higher,
  // and a fresh JSObject outside it.
  Handle<HeapObject> value = isolate()->factory()->true_value();
  Handle<HeapObject> other = isolate()->factory()->null_value();
  Handle<HeapObject> string = isolate()->factory()->object_string();
  Handle<HeapObject> object =
      isolate()->factory()->NewJSObject(isolate()->object_function());
  Label pass, fail;
  as.CodeEntry();
  SetUpRootRegisters();
  as.Move(kReturnRegister0, value);
  int bytes = EmittedBytes([&] {
    as.CompareTaggedAndJumpIf(kReturnRegister0, other, kEqual, &fail);
  });
  as.CompareTaggedAndJumpIf(kReturnRegister0, string, kEqual, &fail);
  as.CompareTaggedAndJumpIf(kReturnRegister0, object, kEqual, &fail);
  as.CompareTaggedAndJumpIf(kReturnRegister0, value, kNotEqual, &fail);
  as.Move(kReturnRegister0, string);
  as.CompareTaggedAndJumpIf(kReturnRegister0, value, kEqual, &fail);
  as.CompareTaggedAndJumpIf(kReturnRegister0, string, kNotEqual, &fail);
  as.Move(kReturnRegister0, object);
  as.CompareTaggedAndJumpIf(kReturnRegister0, string, kEqual, &fail);
  as.CompareTaggedAndJumpIf(kReturnRegister0, object, kNotEqual, &fail);
  RestoreRootRegisters();
  as.Jump(&pass);
  if (ExpectImmediateCompare()) EXPECT_EQ(2 * kInstrSize, bytes);
  FinalizeAndRun(&pass, &fail);
}
#endif  // V8_TARGET_ARCH_ARM64

#if V8_TARGET_ARCH_X64
TEST_F(MaglevAssemblerTest, StackAlignmentSavedRegisterAndFrameSizes) {
  constexpr int kAlign = V8_X64_16BYTE_STACK_ALIGNMENT_BOOL
                             ? 2 * kSystemPointerSize
                             : kSystemPointerSize;

  // GP PushAll / PopAll for 1..4 registers.
  for (RegList regs : {RegList{rax}, RegList{rax, rcx}, RegList{rax, rcx, rdx},
                       RegList{rax, rcx, rdx, rbx}}) {
    EXPECT_EQ(0, as.PushAll(regs) % kAlign);
    EXPECT_EQ(0, as.PopAll(regs) % kAlign);
  }

  // FP PushAll / PopAll for 1..3 double registers.
  for (DoubleRegList regs : {DoubleRegList{xmm0}, DoubleRegList{xmm0, xmm1},
                             DoubleRegList{xmm0, xmm1, xmm2}}) {
    EXPECT_EQ(0, as.PushAll(regs) % kAlign);
    EXPECT_EQ(0, as.PopAll(regs) % kAlign);
  }

  // Caller-saved stack size with and without an excluded GP register.
  for (SaveFPRegsMode fp_mode :
       {SaveFPRegsMode::kIgnore, SaveFPRegsMode::kSave}) {
    EXPECT_EQ(0, as.RequiredStackSizeForCallerSaved(fp_mode) % kAlign);
    EXPECT_EQ(0, as.RequiredStackSizeForCallerSaved(fp_mode, rax) % kAlign);
  }
}

using MaglevSpillSlotAlignmentTest = TestWithNativeContextAndZone;

TEST_F(MaglevSpillSlotAlignmentTest, StackAlignmentSpillSlots) {
  // Zero-argument functions with varying numbers of locals and temporaries.
  const char* kSources[] = {
      "(() => {})()",
      "(() => { let a = 1; return a; })()",
      "(() => { let a = 1, b = 2; return a + b; })()",
      "(() => { let a = 1, b = 2, c = 3; return (a + b) * (b + c); })()",
      "(() => { let a = 1, b = 2, c = 3, d = 4; return (a + b) * (c + d); })()",
  };

  v8::Local<v8::Context> context = v8_isolate()->GetCurrentContext();
  for (const char* src : kSources) {
    v8::Local<v8::Script> script =
        v8::Script::Compile(
            context,
            v8::String::NewFromUtf8(v8_isolate(), src).ToLocalChecked())
            .ToLocalChecked();
    Handle<JSFunction> top_level = v8::Utils::OpenHandle(*script);
    IsCompiledScope is_compiled_scope(
        top_level->shared()->is_compiled_scope(isolate()));
    JSFunction::EnsureFeedbackVector(isolate(), top_level, &is_compiled_scope);

    auto info = MaglevCompilationInfo::New(isolate(), top_level,
                                           BytecodeOffset::None());
    {
      PersistentHandlesScope persistent_scope(isolate());
      CHECK(MaglevCompiler::Compile(isolate()->AsLocalIsolate(), info.get()));
      persistent_scope.Detach();
    }

    Graph* graph = info->code_generator()->graph();
    const uint32_t frame_slots_below_fp =
        StandardFrameConstants::kFixedSlotCountFromFp +
        graph->tagged_stack_slots() + graph->untagged_stack_slots();
    if constexpr (V8_X64_16BYTE_STACK_ALIGNMENT_BOOL) {
      EXPECT_EQ(0u, frame_slots_below_fp % 2) << src;
    }
  }
}
#endif  // V8_TARGET_ARCH_X64

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_MAGLEV
