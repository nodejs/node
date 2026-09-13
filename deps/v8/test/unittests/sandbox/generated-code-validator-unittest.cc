// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/generated-code-validator.h"

#ifdef V8_ENABLE_GENERATED_CODE_VALIDATOR

#include "src/codegen/macro-assembler.h"
#include "src/heap/factory-inl.h"
#include "test/common/assembler-tester.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

using GeneratedCodeValidatorTest = TestWithContext;

#define __ masm.

TEST_F(GeneratedCodeValidatorTest, ValidateValidCode) {
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

  CodeDesc desc;
  masm.GetCode(i_isolate, &desc);
  DirectHandle<Code> code =
      Factory::CodeBuilder(i_isolate, desc, CodeKind::FOR_TESTING).Build();

  // This should not crash.
  v8_flags.validate_generated_code = true;
  GeneratedCodeValidator::Validate(i_isolate, *code);
}

TEST_F(GeneratedCodeValidatorTest, ValidateInvalidCode) {
  // This validation until we're ready to call it explicitly.
  v8_flags.validate_generated_code = false;
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

  CodeDesc desc;
  masm.GetCode(i_isolate, &desc);
  DirectHandle<Code> code =
      Factory::CodeBuilder(i_isolate, desc, CodeKind::FOR_TESTING).Build();

  // This should crash.
  v8_flags.validate_generated_code = true;
  ASSERT_DEATH_IF_SUPPORTED(GeneratedCodeValidator::Validate(i_isolate, *code),
                            "Generated code validator failed");
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_GENERATED_CODE_VALIDATOR
