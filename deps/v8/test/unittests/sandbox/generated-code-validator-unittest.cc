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
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

  __ Mov(kRootRegister, 0);
  __ ret();

  CheckValidationFails(i_isolate, masm,
                       "Root register initialization interrupted");
}

TEST_F(GeneratedCodeValidatorTest, ValidateSystemRegisterWrites) {
  Isolate* i_isolate = this->i_isolate();

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(i_isolate, CodeObjectRequired{false},
                      buffer->CreateView());

  // Writing to system register should fail validation.
  __ Msr(NZCV, x0);
  __ ret();

  CheckValidationFails(i_isolate, masm,
                       "Instruction writes to prohibited system registers");
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
