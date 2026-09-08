// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is goValidatened by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/generated-code-validator.h"

#ifdef V8_ENABLE_GENERATED_CODE_VALIDATOR

#include "src/base/logging.h"
#include "src/builtins/builtins.h"
#include "src/codegen/arm64/instructions-arm64.h"
#include "src/codegen/arm64/register-arm64.h"
#include "src/objects/code-inl.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif
#include "third_party/disarm/src/disarm64.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace v8::internal {

namespace {

struct Da64InstFormatter {
  static std::string Format(const Da64Inst& instr) {
    char formatted_instruction[128] = {'\0'};
    da64_format(&instr, formatted_instruction);
    return formatted_instruction;
  }
};

static bool Da64OpEquals(const Da64Op& op1, const Da64Op& op2) {
  if (op1.type != op2.type) {
    return false;
  }

  switch (op1.type) {
    case DA_OP_NONE:
      return true;
    case DA_OP_IMMLARGE:
    case DA_OP_RELADDR:
    case DA_OP_IMMFLOAT:
      // These operand types keep their values in the Da64Inst struct. Comparing
      // such operand should not use this method.
      UNREACHABLE();
    case DA_OP_REGGP:
    case DA_OP_REGGPINC:
    case DA_OP_REGSP:
      return op1.reg == op2.reg && op1.reggp.sf == op2.reggp.sf;
    case DA_OP_REGGPEXT:
      return op1.reg == op2.reg && op1.reggpext.sf == op2.reggpext.sf &&
             op1.reggpext.ext == op2.reggpext.ext &&
             op1.reggpext.shift == op2.reggpext.shift;
    case DA_OP_REGFP:
      return op1.reg == op2.reg && op1.regfp.size == op2.regfp.size;
    case DA_OP_REGVEC:
      return op1.reg == op2.reg && op1.regvec.va == op2.regvec.va;
    case DA_OP_REGVTBL:
      return op1.reg == op2.reg && op1.regvtbl.va == op2.regvtbl.va &&
             op1.regvtbl.cnt == op2.regvtbl.cnt;
    case DA_OP_REGVIDX:
      return op1.reg == op2.reg && op1.regvidx.esize == op2.regvidx.esize &&
             op1.regvidx.elem == op2.regvidx.elem;
    case DA_OP_REGVTBLIDX:
      return op1.reg == op2.reg &&
             op1.regvtblidx.esize == op2.regvtblidx.esize &&
             op1.regvtblidx.elem == op2.regvtblidx.elem &&
             op1.regvtblidx.cnt == op2.regvtblidx.cnt;
    case DA_OP_MEMUOFF:
      return op1.reg == op2.reg && op1.uimm16 == op2.uimm16;
    case DA_OP_MEMSOFF:
    case DA_OP_MEMSOFFPRE:
    case DA_OP_MEMSOFFPOST:
      return op1.reg == op2.reg && op1.simm16 == op2.simm16;
    case DA_OP_MEMREG:
    case DA_OP_MEMREGPOST:
      return op1.reg == op2.reg && op1.memreg.sc == op2.memreg.sc &&
             op1.memreg.ext == op2.memreg.ext &&
             op1.memreg.shift == op2.memreg.shift &&
             op1.memreg.offreg == op2.memreg.offreg;
    case DA_OP_MEMINC:
      return op1.reg == op2.reg;
    case DA_OP_COND:
      return op1.cond == op2.cond;
    case DA_OP_PRFOP:
      return op1.prfop == op2.prfop;
    case DA_OP_SYSREG:
      return op1.sysreg == op2.sysreg;
    case DA_OP_IMMSMALL:
    case DA_OP_UIMM:
      return op1.uimm16 == op2.uimm16;
    case DA_OP_SIMM:
      return op1.simm16 == op2.simm16;
    case DA_OP_UIMMSHIFT:
      return op1.uimm16 == op2.uimm16 &&
             op1.immshift.mask == op2.immshift.mask &&
             op1.immshift.shift == op2.immshift.shift;
  }

  UNREACHABLE();
}

}  // namespace

#define VALIDATOR_CHECK(condition, message)                 \
  do {                                                      \
    if (V8_UNLIKELY(!(condition))) {                        \
      violations_reporter_.ReportViolationWithInstruction(  \
          pc, Da64InstFormatter::Format(instr), (message)); \
    }                                                       \
  } while (false)

class InstructionChecker {
  static_assert(kPtrComprCageBaseRegister != no_reg);
  static constexpr int cage_base_register = kPtrComprCageBaseRegister.code();
  static_assert(kRootRegister != no_reg);
  static constexpr int root_register = kRootRegister.code();
  static constexpr int kMaxOperands = 5;

  using ViolationsReporter = GeneratedCodeValidator::ViolationsReporter;
  using Utils = GeneratedCodeValidator::Utils;
  using State = GeneratedCodeValidator::State;

 public:
  InstructionChecker(Isolate* isolate, Tagged<Code> code,
                     ViolationsReporter& violations_reporter)
      : isolate_(isolate),
        code_(code),
        violations_reporter_(violations_reporter),
        state_(code_),
        root_reg_init_state_(isolate_) {}

  void Check(const uint8_t* pc, const Da64Inst& instr) {
    static_assert(kMaxOperands == (sizeof(instr.ops) / sizeof(Da64Op)));
    // REGEXP code doesn't follow the V8 ABI and instead uses standard C ABI.
    if (code_->kind() != CodeKind::REGEXP) {
      CheckNoWritesToCageBaseRegister(pc, instr);
      CheckNoWritesToRootRegister(pc, instr);
    }
    CheckNoSystemRegisterWrites(pc, instr);
  }

 private:
  // Verifies whether the instruction accesses the pointer compression cage base
  // register (x28) directly or modifies it via writeback. The cage base
  // register must remain read-only across all generated code to preserve
  // sandbox integrity and prevent pointer corruption. This will not block using
  // the cage base register as a base address for a memory operand without
  // writeback.
  void CheckNoWritesToCageBaseRegister(const uint8_t* pc,
                                       const Da64Inst& instr) {
    switch (instr.mnem) {
      // Subs is used as a comparison to assert that a register holds a heap
      // object in the main cage.
      case DA64I_SUBS_SHIFT:
      // Add and sub instructions are used for decompressing and compressing
      // sandboxed pointers.
      case DA64I_ADD_SHIFT:
      case DA64I_SUB_SHIFT:
      // Or and Add instructions are used for decompressing tagged pointers.
      case DA64I_ADD_IMM:
      case DA64I_ORR_IMM:
      case DA64I_ORR_SHIFT:
      // Add extended register is used for decompressing tagged pointers with
      // zero-extension: add xd, x28, ws, uxtw #0.
      case DA64I_ADD_EXT:
        // Check that the target is not the cage base register.
        if (!IsCageBaseReg(instr.ops[0])) {
          // Arithmetic and logical instructions do not support writeback
          // operands.
          return;
        }
        break;
      // Stp is used by entry and deopt builtins to save the cage base
      // register's value in C++ code.
      case DA64I_STPX:
      case DA64I_STPX_PRE:
      case DA64I_STPX_POST:
        // Stp isn't updating any registers.
        DCHECK_IMPLIES(
            IsCageBaseReg(instr.ops[0]) || IsCageBaseReg(instr.ops[1]),
            Utils::IsEntryCode(code_) || Utils::IsDeoptCode(code_));
        if (!IsCageBaseWritebackReg(instr.ops[2])) {
          return;
        }
        break;
      // Ldp is used by entry and deopt builtins to restore the cage base
      // register's value in C++ code.
      case DA64I_LDPX:
      case DA64I_LDPX_PRE:
      case DA64I_LDPX_POST:
        // TODO(523128533): verify the popped value is the same as the
        // previously pushed value. E.g. track changes to stack register and
        // check that push and pop operate on the same offset (assuming calls
        // don't violate it).
        // TODO(523128533): Deopt builtins save and restore the cage base
        // register so that the deoptimizer can update register values as
        // needed. Since the cage base register is callee saved and should not
        // change, can we avoid saving and restoring it?
        if (Utils::IsEntryCode(code_) || Utils::IsDeoptCode(code_)) {
          if ((IsCageBaseReg(instr.ops[0]) || IsCageBaseReg(instr.ops[1]))) {
            state_.is_cage_base_reg_valid_ = false;
          }
          if (!IsCageBaseWritebackReg(instr.ops[2])) {
            return;
          }
        }
        break;
      // Ldur is used by entry builtins to initialize the cage base register.
      case DA64I_LDURX:
        if (!IsCageBaseReg(instr.ops[0])) {
          return;
        }
        VALIDATOR_CHECK(!state_.is_cage_base_reg_valid_,
                        "Instruction overwrites a valid cage base register");
        if (Utils::IsEntryCode(code_) &&
            IsExpectedOperand(instr.ops[1],
                              {.type = DA_OP_MEMSOFF,
                               .reg = root_register,
                               .simm16 = IsolateData::cage_base_offset()})) {
          VALIDATOR_CHECK(
              state_.is_root_reg_valid_,
              "Cage base register initialization uses invalid root register");
          state_.is_cage_base_reg_valid_ = true;
          return;
        }
        break;
      default:
        // All other cases are not expected to accesses the cage base register
        // directly and fall through to the generic handling below.
        break;
    }

    // Check that no operand is the cage base register.
    for (int i = 0; i < kMaxOperands; i++) {
      VALIDATOR_CHECK(
          !IsCageBaseReg(instr.ops[i]) && !IsCageBaseWritebackReg(instr.ops[i]),
          std::format("Instruction accesses cage bage register at operand {0}",
                      i));
    }
  }

  // Verifies whether the instruction accesses the root register (x26) directly
  // or modifies it via writeback. The root register must remain read-only
  // across all generated code to preserve sandbox integrity and prevent pointer
  // corruption. This will not block using the root register as a base address
  // for a memory operand without writeback.
  void CheckNoWritesToRootRegister(const uint8_t* pc, const Da64Inst& instr) {
    const bool expecting_init = root_reg_init_state_.in_progress;
    bool was_init = false;
    // Flag to skip the generic validation at the end of this method. Used to
    // prevent accidentally skipping the `expecting_init && !was_init` check
    // below.
    bool is_validated = false;
    switch (instr.mnem) {
      // Add and sub are also used for root relative indexing and accessing
      // isolate fields (or the isolate itself) via the root register.
      case DA64I_ADD_SHIFT:
      case DA64I_ADD_IMM:
      case DA64I_ADD_EXT:
      case DA64I_SUB_IMM:
      // Orr is used for copying the root register value and by entry builtins
      // to initialize the root register.
      case DA64I_ORR_SHIFT:
        // Check that the target is not the cage base register or root register.
        if (!IsRootReg(instr.ops[0])) {
          // Arithmetic and logical instructions do not support writeback
          // operands.
          is_validated = true;
          break;
        }
        VALIDATOR_CHECK(!state_.is_root_reg_valid_,
                        "Instruction overwrites a valid root register");
        if (Utils::IsEntryCode(code_) && IsValidRootRegInitialization(instr)) {
          is_validated = true;
        }
        break;
      // Stp is used by entry and deopt builtins to save the root register's
      // value in C++ code.
      case DA64I_STPX:
      case DA64I_STPX_PRE:
      case DA64I_STPX_POST:
        // Stp isn't updating any registers.
        DCHECK_IMPLIES(IsRootReg(instr.ops[0]) || IsRootReg(instr.ops[1]),
                       Utils::IsEntryCode(code_) || Utils::IsDeoptCode(code_));
        if (!IsRootWritebackReg(instr.ops[2])) {
          is_validated = true;
        }
        break;
      // Ldp is used by entry and deopt builtins to restore the root registers'
      // value in C++ code.
      case DA64I_LDPX:
      case DA64I_LDPX_PRE:
      case DA64I_LDPX_POST:
        // TODO(523128533): verify the popped value is the same as the
        // previously pushed value. E.g. track changes to stack register and
        // check that push and pop operate on the same offset (assuming calls
        // don't violate it).
        // TODO(523128533): Deopt builtins save and restore the root
        // register so that the deoptimizer can update register values as
        // needed. Since the root register is callee saved and should not
        // change, can we avoid saving and restoring it?
        if (Utils::IsEntryCode(code_) || Utils::IsDeoptCode(code_)) {
          if ((IsRootReg(instr.ops[0]) || IsRootReg(instr.ops[1]))) {
            state_.is_root_reg_valid_ = false;
          }
          if (!IsRootWritebackReg(instr.ops[2])) {
            is_validated = true;
          }
        }
        break;
      // Mov is used by entry builtins to initialize the root register.
      case DA64I_MOVZ:
      case DA64I_MOVK:
        if (!IsRootReg(instr.ops[0])) {
          is_validated = true;
          break;
        }
        VALIDATOR_CHECK(!state_.is_root_reg_valid_,
                        "Instruction overwrites a valid root register");
        was_init = true;
        if (Utils::IsEntryCode(code_) && IsValidRootRegInitialization(instr)) {
          is_validated = true;
        }
        break;
      default:
        // All other cases are not expected to accesses the cage base register
        // or root register directly and fall through to the generic handling
        // below.
        break;
    }

    VALIDATOR_CHECK(!(expecting_init && !was_init),
                    "Root register initialization interrupted");

    if (is_validated) {
      return;
    }

    // Check that no operand is the cage base register or the root register.
    for (int i = 0; i < kMaxOperands; i++) {
      VALIDATOR_CHECK(
          !IsRootReg(instr.ops[i]) && !IsRootWritebackReg(instr.ops[i]),
          std::format("Instruction accesses root register at operand {0}", i));
    }
  }

  // Verifies that system register instruction MSR is never used. This prevents
  // unintended modification of CPU control flags or floating-point execution
  // state.
  void CheckNoSystemRegisterWrites(const uint8_t* pc, const Da64Inst& instr) {
    VALIDATOR_CHECK(instr.mnem != DA64I_MSR,
                    "Instruction writes to prohibited system registers");
  }

  static bool IsExpectedReg(const Da64Op& op, int expected_reg) {
    return ((op.type == DA_OP_REGGP) || (op.type == DA_OP_REGGPEXT) ||
            (op.type == DA_OP_REGGPINC)) &&
           (op.reg == expected_reg);
  }

  static bool IsCageBaseReg(const Da64Op& op) {
    return IsExpectedReg(op, cage_base_register);
  }

  static bool IsRootReg(const Da64Op& op) {
    return IsExpectedReg(op, root_register);
  }

  static bool IsExpectedWritebackReg(const Da64Op& op, int expected_reg) {
    return ((op.type == DA_OP_MEMSOFFPRE) || (op.type == DA_OP_MEMSOFFPOST) ||
            (op.type == DA_OP_MEMREGPOST) || (op.type == DA_OP_MEMINC)) &&
           (op.reg == expected_reg);
  }

  static bool IsCageBaseWritebackReg(const Da64Op& op) {
    return IsExpectedWritebackReg(op, cage_base_register);
  }

  static bool IsRootWritebackReg(const Da64Op& op) {
    return IsExpectedWritebackReg(op, root_register);
  }

  static bool IsExpectedOperand(const Da64Op& op, const Da64Op& expected) {
    return Da64OpEquals(op, expected);
  }

  bool IsValidRootRegInitialization(const Da64Inst& instr) {
    DCHECK(IsRootReg(instr.ops[0]));
    switch (instr.mnem) {
      case DA64I_MOVZ:
        // Movz is the first in a sequence of movs to initialize the root
        // register.
        DCHECK_EQ(root_reg_init_state_.current_value, kNullAddress);
        DCHECK(!root_reg_init_state_.in_progress);
        [[fallthrough]];
      case DA64I_MOVK: {
        // Movk and Movz are used to construct the expected root register value.
        const Da64Op& op = instr.ops[1];
        DCHECK_EQ(DA_OP_UIMMSHIFT, op.type);
        DCHECK_EQ(0, op.immshift.mask);
        const Address previous_value = root_reg_init_state_.current_value;
        root_reg_init_state_.current_value |=
            (static_cast<uint64_t>(op.uimm16) << op.immshift.shift);
        state_.is_root_reg_valid_ = root_reg_init_state_.current_value ==
                                    root_reg_init_state_.expected_value_;
        root_reg_init_state_.in_progress = !state_.is_root_reg_valid_;
        // This is a valid initialization as long as it's making progress
        // towards the expected value.
        return (previous_value != root_reg_init_state_.current_value) &&
               ((root_reg_init_state_.current_value &
                 root_reg_init_state_.expected_value_) ==
                root_reg_init_state_.current_value);
      }
      case DA64I_ORR_SHIFT:
        DCHECK(!root_reg_init_state_.in_progress);
        // Assumes builtins receive the correct value as their first
        // argument.
        state_.is_root_reg_valid_ = (code_->kind() == CodeKind::BUILTIN) &&
                                    IsExpectedReg(instr.ops[2], x0.code()) &&
                                    IsExpectedReg(instr.ops[1], xzr.code());
        break;
      default:
        break;
    }
    return state_.is_root_reg_valid_;
  }

  Isolate* const isolate_;
  const Tagged<Code> code_;
  ViolationsReporter& violations_reporter_;
  State state_;
  struct RootRegisterInitializationState {
    explicit RootRegisterInitializationState(Isolate* isolate)
        : expected_value_(ExternalReference::isolate_root(isolate).raw()) {
      CHECK_NE(expected_value_, kNullAddress);
    }

    const Address expected_value_;
    Address current_value = kNullAddress;
    bool in_progress = false;
  } root_reg_init_state_;
};

void GeneratedCodeValidator::ValidateImpl(Isolate* isolate, Tagged<Code> code) {
  DCHECK_EQ(code->instruction_size() % kInstrSize, 0);

  ViolationsReporter reporter(code);

  InstructionChecker instruction_checker(isolate, code, reporter);

  InstructionIteratorSkippingData it(code);

  while (!it.IsDone()) {
    const uint8_t* pc = it.GetCurrent();
    Da64Inst instr;
    da64_decode(*reinterpret_cast<const uint32_t*>(pc), &instr);

    if (instr.mnem == DA64I_UNKNOWN) {
      reporter.ReportDisassemblyFailed(pc, kInstrSize);
      return;
    }
    if (v8_flags.validate_generated_code_include_code) {
      reporter.RecordDisassembledInstruction(pc, kInstrSize,
                                             Da64InstFormatter::Format(instr));
    }
    instruction_checker.Check(pc, instr);
    it.Advance(kInstrSize);
  }
}

}  // namespace v8::internal

#undef VALIDATOR_CHECK

#endif  // V8_ENABLE_GENERATED_CODE_VALIDATOR
