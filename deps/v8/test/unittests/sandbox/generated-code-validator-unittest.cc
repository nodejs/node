// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/generated-code-validator.h"

#ifdef V8_ENABLE_GENERATED_CODE_VALIDATOR

#include "src/codegen/macro-assembler-inl.h"
#include "src/heap/factory-inl.h"
#include "test/common/assembler-tester.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

using GeneratedCodeValidatorTest = TestWithContext;

namespace {

void CheckValidationSucceeds(Isolate* i_isolate, MacroAssembler& masm) {
  // Delay validation until we're ready to call it explicitly.
  v8_flags.validate_generated_code = false;
  CodeDesc desc;
  masm.GetCode(i_isolate, &desc);
  DirectHandle<Code> code =
      Factory::CodeBuilder(i_isolate, desc, CodeKind::FOR_TESTING).Build();

  // This should not crash.
  v8_flags.validate_generated_code = true;
  v8_flags.validate_generated_code_non_fatal = false;
  GeneratedCodeValidator::Validate(i_isolate, *code);
}

void CheckValidationFails(Isolate* i_isolate, MacroAssembler& masm,
                          std::string expected_error) {
  // Delay validation until we're ready to call it explicitly.
  v8_flags.validate_generated_code = false;
  CodeDesc desc;
  masm.GetCode(i_isolate, &desc);
  DirectHandle<Code> code =
      Factory::CodeBuilder(i_isolate, desc, CodeKind::FOR_TESTING).Build();

  // This should not crash.
  v8_flags.validate_generated_code = true;
  v8_flags.validate_generated_code_non_fatal = false;
  ASSERT_DEATH_IF_SUPPORTED(GeneratedCodeValidator::Validate(i_isolate, *code),
                            expected_error);
}

}  // namespace

#define __ masm.

TEST_F(GeneratedCodeValidatorTest, DisassembleValidCode) {
  // This validation until we're ready to call it explicitly.
  v8_flags.validate_generated_code = false;
  Isolate* i_isolate = this->i_isolate();
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

#if V8_TARGET_ARCH_X64
  __ nop();
  __ ret(0);
#elif V8_TARGET_ARCH_ARM64
  __ nop();
  __ ret();
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif

  CheckValidationSucceeds(i_isolate, masm);
}

TEST_F(GeneratedCodeValidatorTest, DisassembleInvalidCode) {
  Isolate* i_isolate = this->i_isolate();
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

#if V8_TARGET_ARCH_X64
  // Write a partial instruction (LOCK prefix only) to trigger decode error.
  __ db(0xF0);
#elif V8_TARGET_ARCH_ARM64
  // Write an instruction that is DA64I_UNKNOWN.
  // 0x0b205400 is known to be decoded as DA64I_UNKNOWN.
  __ db(0x00);
  __ db(0x54);
  __ db(0x20);
  __ db(0x0b);
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif

  CheckValidationFails(i_isolate, masm,
                       "Failed to disassemble invalid instruction");
}

TEST_F(GeneratedCodeValidatorTest, ValidateCageBaseModificationFails) {
  static_assert(kPtrComprCageBaseRegister != no_reg);
  Isolate* i_isolate = this->i_isolate();
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

#if V8_TARGET_ARCH_X64
  __ movq(kPtrComprCageBaseRegister, Immediate(0));
  __ ret(0);
#elif V8_TARGET_ARCH_ARM64
  __ Mov(kPtrComprCageBaseRegister, 0);
  __ ret();
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif

  CheckValidationFails(i_isolate, masm,
                       "Instruction accesses cage bage register at operand 0");
}

TEST_F(GeneratedCodeValidatorTest, ValidateRootRegisterModificationFails) {
  static_assert(kRootRegister != no_reg);
  Isolate* i_isolate = this->i_isolate();
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

#if V8_TARGET_ARCH_X64
  __ movq(kRootRegister, Immediate(0));
  __ ret(0);
#elif V8_TARGET_ARCH_ARM64
  __ Add(kRootRegister, x13, 0);
  __ ret();
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif

  CheckValidationFails(i_isolate, masm,
                       "Instruction accesses root register at operand 0");
}

TEST_F(GeneratedCodeValidatorTest, ValidateCageBaseRegInitWithoutRootRegFails) {
  static_assert(kRootRegister != no_reg);
  Isolate* i_isolate = this->i_isolate();
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());

  CheckValidationFails(
      i_isolate, masm,
      "Cage base register initialization uses invalid root register");
}

TEST_F(GeneratedCodeValidatorTest, ValidateOverwritingCageBaseRegFails) {
  static_assert(kRootRegister != no_reg);
  Isolate* i_isolate = this->i_isolate();
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

#if V8_TARGET_ARCH_X64
  __ movq(kRootRegister, kCArgRegs[0]);
#elif V8_TARGET_ARCH_ARM64
  __ Mov(kRootRegister, x0);
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif
  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());
  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());

  CheckValidationFails(i_isolate, masm,
                       "Instruction overwrites a valid cage base register");
}

TEST_F(GeneratedCodeValidatorTest, ValidateDecompressionPasses) {
  static_assert(kPtrComprCageBaseRegister != no_reg);
  Isolate* i_isolate = this->i_isolate();
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

#if V8_TARGET_ARCH_X64
  __ orq(rcx, kPtrComprCageBaseRegister);
  __ ret(0);
#elif V8_TARGET_ARCH_ARM64
  __ Orr(x13, kPtrComprCageBaseRegister, x13);
  __ DecompressTagged(x13, x13);
  __ Add(x13, kPtrComprCageBaseRegister, Operand(w13, UXTW, 0));
  __ ret();
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif

  CheckValidationSucceeds(i_isolate, masm);
}

TEST_F(GeneratedCodeValidatorTest, ValidateControlFlowJumpToInitSucceeds) {
  Isolate* i_isolate = this->i_isolate();
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

  Label init, use_root_reg;
#if V8_TARGET_ARCH_X64
  __ jmp(&init);

  __ bind(&use_root_reg);
  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());
  __ ret(0);

  __ bind(&init);
  __ Move(kRootRegister, ExternalReference::isolate_root(i_isolate));
  __ jmp(&use_root_reg);
#elif V8_TARGET_ARCH_ARM64
  __ B(&init);

  __ Bind(&use_root_reg);
  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());
  __ ret();

  __ Bind(&init);
  __ Mov(kRootRegister, Operand(ExternalReference::isolate_root(i_isolate)));
  __ B(&use_root_reg);
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif

  CheckValidationSucceeds(i_isolate, masm);
}

TEST_F(GeneratedCodeValidatorTest, ValidateControlFlowJumpOverInitFails) {
  Isolate* i_isolate = this->i_isolate();
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

  Label skip_init;
#if V8_TARGET_ARCH_X64
  __ jmp(&skip_init);
  __ Move(kRootRegister, ExternalReference::isolate_root(i_isolate));
  __ ret(0);

  __ bind(&skip_init);
  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());
  __ ret(0);
#elif V8_TARGET_ARCH_ARM64
  __ B(&skip_init);
  __ Mov(kRootRegister, Operand(ExternalReference::isolate_root(i_isolate)));
  __ ret();

  __ Bind(&skip_init);
  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());
  __ ret();
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif

  CheckValidationFails(
      i_isolate, masm,
      "Cage base register initialization uses invalid root register");
}

TEST_F(GeneratedCodeValidatorTest,
       ValidateControlFlowConditionalBranchSeparatePathsSucceeds) {
  Isolate* i_isolate = this->i_isolate();
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

  Label second_path;
#if V8_TARGET_ARCH_X64
  __ Move(kRootRegister, ExternalReference::isolate_root(i_isolate));
  __ j(equal, &second_path);
  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());
  __ ret(0);

  __ bind(&second_path);
  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());
  __ ret(0);
#elif V8_TARGET_ARCH_ARM64
  __ Mov(kRootRegister, Operand(ExternalReference::isolate_root(i_isolate)));
  __ Cbz(x0, &second_path);
  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());
  __ ret();

  __ Bind(&second_path);
  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());
  __ ret();
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif

  CheckValidationSucceeds(i_isolate, masm);
}

TEST_F(GeneratedCodeValidatorTest, ValidateControlFlowMergeStatesSucceeds) {
  Isolate* i_isolate = this->i_isolate();
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

  Label second_path, merge_block;
#if V8_TARGET_ARCH_X64
  __ j(equal, &second_path);
  __ Move(kRootRegister, ExternalReference::isolate_root(i_isolate));
  __ jmp(&merge_block);

  __ bind(&second_path);
  __ movq_imm64(kRootRegister,
                ExternalReference::isolate_root(i_isolate).raw());

  __ bind(&merge_block);
  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());
  __ ret(0);
#elif V8_TARGET_ARCH_ARM64
  __ B(eq, &second_path);
  __ Mov(kRootRegister, Operand(ExternalReference::isolate_root(i_isolate)));
  __ B(&merge_block);

  __ Bind(&second_path);
  __ Mov(kRootRegister, Operand(ExternalReference::isolate_root(i_isolate)));

  __ Bind(&merge_block);
  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());
  __ ret();
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif

  CheckValidationSucceeds(i_isolate, masm);
}

TEST_F(GeneratedCodeValidatorTest, ValidateControlFlowMergeStatesFails) {
  Isolate* i_isolate = this->i_isolate();

  // Case 1: One path skips root register initialization before converging at
  // `merge_block`.
  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());

    Label merge_block;
#if V8_TARGET_ARCH_X64
    __ j(equal, &merge_block);
    __ Move(kRootRegister, ExternalReference::isolate_root(i_isolate));

    __ bind(&merge_block);
    __ LoadRootRelative(kPtrComprCageBaseRegister,
                        IsolateData::cage_base_offset());
    __ ret(0);
#elif V8_TARGET_ARCH_ARM64
    __ B(eq, &merge_block);
    __ Mov(kRootRegister, Operand(ExternalReference::isolate_root(i_isolate)));

    __ Bind(&merge_block);
    __ LoadRootRelative(kPtrComprCageBaseRegister,
                        IsolateData::cage_base_offset());
    __ ret();
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif

    CheckValidationFails(
        i_isolate, masm,
        "Cage base register initialization uses invalid root register");
  }

  // Case 2: Root register is valid on the first incoming edge to `merge_block`,
  // but invalidated on the second incoming edge.
  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());

    Label merge_block;
#if V8_TARGET_ARCH_X64
    __ Move(kRootRegister, ExternalReference::isolate_root(i_isolate));
    __ j(equal, &merge_block);
    __ popq(kRootRegister);

    __ bind(&merge_block);
    __ LoadRootRelative(kPtrComprCageBaseRegister,
                        IsolateData::cage_base_offset());
    __ ret(0);
#elif V8_TARGET_ARCH_ARM64
    __ Mov(kRootRegister, Operand(ExternalReference::isolate_root(i_isolate)));
    __ B(eq, &merge_block);
    __ Ldp(kRootRegister, xzr, MemOperand(sp, 16, PostIndex));

    __ Bind(&merge_block);
    __ LoadRootRelative(kPtrComprCageBaseRegister,
                        IsolateData::cage_base_offset());
    __ ret();
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif

    CheckValidationFails(
        i_isolate, masm,
        "Cage base register initialization uses invalid root register");
  }

  // Case 3: One path initializes the root register and the other skips it
  // before converging at `merge_block`, where re-initializing the root
  // register overwrites the valid path.
  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());

    Label merge_block;
#if V8_TARGET_ARCH_X64
    __ j(equal, &merge_block);
    __ Move(kRootRegister, ExternalReference::isolate_root(i_isolate));

    __ bind(&merge_block);
    __ Move(kRootRegister, ExternalReference::isolate_root(i_isolate));
    __ ret(0);
#elif V8_TARGET_ARCH_ARM64
    __ B(eq, &merge_block);
    __ Mov(kRootRegister, Operand(ExternalReference::isolate_root(i_isolate)));

    __ Bind(&merge_block);
    __ Mov(kRootRegister, Operand(ExternalReference::isolate_root(i_isolate)));
    __ ret();
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif

    CheckValidationFails(i_isolate, masm,
                         "Instruction overwrites a valid root register");
  }
}

TEST_F(GeneratedCodeValidatorTest, ValidateControlFlowLoop) {
  Isolate* i_isolate = this->i_isolate();

  // Valid loop: state converges at `loop` header and fixed-point terminates.
  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());
    Label loop;
#if V8_TARGET_ARCH_X64
    __ Move(kRootRegister, ExternalReference::isolate_root(i_isolate));
    __ bind(&loop);
    __ nop();
    __ j(equal, &loop);
    __ LoadRootRelative(kPtrComprCageBaseRegister,
                        IsolateData::cage_base_offset());
    __ ret(0);
#elif V8_TARGET_ARCH_ARM64
    __ Mov(kRootRegister, Operand(ExternalReference::isolate_root(i_isolate)));
    __ Bind(&loop);
    __ nop();
    __ B(eq, &loop);
    __ LoadRootRelative(kPtrComprCageBaseRegister,
                        IsolateData::cage_base_offset());
    __ ret();
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif
    CheckValidationSucceeds(i_isolate, masm);
  }

  // Invalid loop: back-edge invalidates the root register, which propagates to
  // the loop header and then to the loop exit.
  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());
    Label loop, loop_exit;
#if V8_TARGET_ARCH_X64
    __ Move(kRootRegister, ExternalReference::isolate_root(i_isolate));
    __ bind(&loop);
    __ j(equal, &loop_exit);
    __ popq(kRootRegister);
    __ jmp(&loop);

    __ bind(&loop_exit);
    __ LoadRootRelative(kPtrComprCageBaseRegister,
                        IsolateData::cage_base_offset());
    __ ret(0);
#elif V8_TARGET_ARCH_ARM64
    __ Mov(kRootRegister, Operand(ExternalReference::isolate_root(i_isolate)));
    __ Bind(&loop);
    __ B(eq, &loop_exit);
    __ Ldp(kRootRegister, xzr, MemOperand(sp, 16, PostIndex));
    __ B(&loop);

    __ Bind(&loop_exit);
    __ LoadRootRelative(kPtrComprCageBaseRegister,
                        IsolateData::cage_base_offset());
    __ ret();
#else
#error "Unsupported architecture for GeneratedCodeValidatorTest"
#endif
    CheckValidationFails(
        i_isolate, masm,
        "Cage base register initialization uses invalid root register");
  }
}

#if V8_TARGET_ARCH_ARM64

TEST_F(GeneratedCodeValidatorTest, ValidateCageBaseWritebackFails) {
  static_assert(kPtrComprCageBaseRegister != no_reg);
  Isolate* i_isolate = this->i_isolate();
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

  // Pre-indexed load with writeback updating the cage base register.
  __ Ldr(x0, MemOperand(kPtrComprCageBaseRegister, 16, PreIndex));
  __ ret();

  CheckValidationFails(i_isolate, masm,
                       "Instruction accesses cage bage register at operand 1");
}

TEST_F(GeneratedCodeValidatorTest, ValidateRootRegisterPartialInitFails) {
  static_assert(kRootRegister != no_reg);
  Isolate* i_isolate = this->i_isolate();
  const Address isolate_root = ExternalReference::isolate_root(i_isolate).raw();

  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());

    __ movz(kRootRegister, isolate_root & 0xFFFF, 0);
    __ ret();

    CheckValidationFails(i_isolate, masm,
                         "Root register initialization interrupted");
  }

  // `movk` without a preceding `movz`.
  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());

    __ movk(kRootRegister, isolate_root & 0xFFFF, 0);
    __ ret();

    CheckValidationFails(i_isolate, masm,
                         "Instruction accesses root register at operand 0");
  }

  // `movz` while root register initialization is already in progress.
  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());

    __ movz(kRootRegister, isolate_root & 0xFFFF, 0);
    __ movz(kRootRegister, (isolate_root >> 16) & 0xFFFF, 16);
    __ ret();

    CheckValidationFails(i_isolate, masm,
                         "Instruction accesses root register at operand 0");
  }
}

TEST_F(GeneratedCodeValidatorTest, ValidateRootRegisterPartialInitMergeFails) {
  static_assert(kRootRegister != no_reg);
  Isolate* i_isolate = this->i_isolate();
  const Address isolate_root = ExternalReference::isolate_root(i_isolate).raw();

  // Case 1: One incoming path to `merge_block` has partially initialized the
  // root register while the other has not started initialization.
  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());

    Label merge_block;
    __ B(eq, &merge_block);
    __ movz(kRootRegister, isolate_root & 0xFFFF, 0);

    __ Bind(&merge_block);
    __ ret();

    CheckValidationFails(i_isolate, masm,
                         "Root register initialization interrupted");
  }

  // Case 2: Two incoming paths to `merge_block` both partially initialize the
  // root register, but set different 16-bit chunks via `movz`, so the root
  // register initialization remains incomplete after the remaining `movk`
  // instructions.
  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());

    int first_shift = -1;
    int second_shift = -1;
    for (int shift = 0; shift < 64; shift += 16) {
      uint64_t chunk = (isolate_root >> shift) & 0xFFFF;
      if (chunk == 0) continue;
      if (first_shift == -1) {
        first_shift = shift;
      } else if (second_shift == -1) {
        second_shift = shift;
        break;
      }
    }
    CHECK_NE(first_shift, -1);
    CHECK_NE(second_shift, -1);

    Label else_block, merge_block;
    __ B(eq, &else_block);
    __ movz(kRootRegister, (isolate_root >> first_shift) & 0xFFFF, first_shift);
    __ B(&merge_block);

    __ Bind(&else_block);
    __ movz(kRootRegister, (isolate_root >> second_shift) & 0xFFFF,
            second_shift);

    __ Bind(&merge_block);
    for (int shift = 0; shift < 64; shift += 16) {
      uint64_t chunk = (isolate_root >> shift) & 0xFFFF;
      if (chunk == 0 || shift == first_shift) continue;
      __ Movk(kRootRegister, chunk, shift);
    }
    __ ret();

    CheckValidationFails(i_isolate, masm,
                         "Root register initialization interrupted");
  }
}

TEST_F(GeneratedCodeValidatorTest, ValidateSystemRegisterWrites) {
  Isolate* i_isolate = this->i_isolate();

  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());

    // Writing to system register should fail validation.
    __ Msr(NZCV, x0);
    __ ret();

    CheckValidationFails(i_isolate, masm,
                         "Instruction writes to prohibited system registers");
  }

  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());

    // Raw ARM64 encoding for `msr daifset, #2` (opcode 0xd50342df)
    __ db(0xdf);
    __ db(0x42);
    __ db(0x03);
    __ db(0xd5);
    __ ret();

    CheckValidationFails(i_isolate, masm,
                         "Instruction writes to prohibited system registers");
  }
}

#endif  // V8_TARGET_ARCH_ARM64

#if V8_TARGET_ARCH_X64

TEST_F(GeneratedCodeValidatorTest, ValidateSegmentRegisterFails) {
  Isolate* i_isolate = this->i_isolate();

  // Instruction with segment override prefix.
  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());
    // mov rax, [fs:rax]
    __ db(0x64);
    __ db(0x48);
    __ db(0x8B);
    __ db(0x00);
    __ ret(0);

    CheckValidationFails(i_isolate, masm,
                         "Instruction uses a segment register");
  }
  // Instruction with segment register operand.
  {
    auto buffer = AllocateAssemblerBuffer();
    MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                        buffer->CreateView());
    // mov fs, ax
    __ db(0x8E);
    __ db(0xE0);
    __ ret(0);

    CheckValidationFails(i_isolate, masm,
                         "Instruction uses a segment register at operand 0");
  }
}

#endif  // V8_TARGET_ARCH_X64

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_GENERATED_CODE_VALIDATOR
