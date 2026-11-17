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

class InstructionHandler {
  static_assert(kPtrComprCageBaseRegister != no_reg);
  static constexpr int cage_base_register = kPtrComprCageBaseRegister.code();
  static_assert(kRootRegister != no_reg);
  static constexpr int root_register = kRootRegister.code();
  static constexpr int kMaxOperands = 5;

  using ViolationsReporter = GeneratedCodeValidator::ViolationsReporter;
  using Utils = GeneratedCodeValidator::Utils;
  using State = GeneratedCodeValidator::State;
  using BoolValue = State::BoolValue;
  using Node = GeneratedCodeValidator::ControlFlowGraph<Da64Inst>::Node;

 public:
  InstructionHandler(Isolate* isolate, Tagged<Code> code,
                     ViolationsReporter& violations_reporter)
      : code_(code),
        violations_reporter_(violations_reporter),
        expected_root_reg_value_(
            ExternalReference::isolate_root(isolate).raw()) {
    CHECK_NE(expected_root_reg_value_, kNullAddress);
  }

  void CheckStateDataFlowIndependent(const uint8_t* pc, const Da64Inst& instr) {
    static_assert(kMaxOperands == (sizeof(instr.ops) / sizeof(Da64Op)));
    CheckNoSystemRegisterWrites(pc, instr);
  }

  State UpdateState(const Node& node) {
    State state = node.input_state();
    // REGEXP code doesn't follow the V8 ABI and instead uses standard C ABI.
    if (code_->kind() != CodeKind::REGEXP) {
      const Da64Inst& instr = node.instr();
      UpdateCageBaseRegisterState(instr, state);
      UpdateRootRegisterState(instr, state);
    }
    return state;
  }

  void CheckState(const Node& node) {
    const uint8_t* pc = node.pc();
    const Da64Inst& instr = node.instr();
    const State& state = node.input_state();
    // REGEXP code doesn't follow the V8 ABI and instead uses standard C ABI.
    if (code_->kind() != CodeKind::REGEXP) {
      CheckNoWritesToCageBaseRegister(pc, instr, state);
      CheckNoWritesToRootRegister(pc, instr, state);
    }
  }

 private:
  void UpdateCageBaseRegisterState(const Da64Inst& instr, State& state) {
    switch (instr.mnem) {
      case DA64I_LDPX:
      case DA64I_LDPX_PRE:
      case DA64I_LDPX_POST:
        if ((Utils::IsEntryCode(code_) || Utils::IsDeoptCode(code_)) &&
            (IsCageBaseReg(instr.ops[0]) || IsCageBaseReg(instr.ops[1]))) {
          state.is_cage_base_reg_valid_ = BoolValue::kFalse;
        }
        break;
      case DA64I_LDURX:
        if (IsCageBaseReg(instr.ops[0]) &&
            IsValidCageBaseRegInitialization(instr)) {
          state.is_cage_base_reg_valid_ = BoolValue::kTrue;
        }
        break;
      default:
        break;
    }
  }

  void UpdateRootRegisterState(const Da64Inst& instr, State& state) {
    switch (instr.mnem) {
      case DA64I_LDPX:
      case DA64I_LDPX_PRE:
      case DA64I_LDPX_POST:
        if ((Utils::IsEntryCode(code_) || Utils::IsDeoptCode(code_)) &&
            (IsRootReg(instr.ops[0]) || IsRootReg(instr.ops[1]))) {
          state.is_root_reg_valid_ = BoolValue::kFalse;
          state.root_reg_init_state_ = {};
        }
        break;
      case DA64I_ORR_SHIFT:
      case DA64I_MOVZ:
      case DA64I_MOVK:
        if (IsRootReg(instr.ops[0])) {
          UpdateRootRegInitialization(instr, state);
        }
        break;
      default:
        break;
    }
  }

  // Verifies whether the instruction accesses the pointer compression cage base
  // register (x28) directly or modifies it via writeback. The cage base
  // register must remain read-only across all generated code to preserve
  // sandbox integrity and prevent pointer corruption. This will not block using
  // the cage base register as a base address for a memory operand without
  // writeback.
  void CheckNoWritesToCageBaseRegister(const uint8_t* pc, const Da64Inst& instr,
                                       const State& state) {
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
        if ((Utils::IsEntryCode(code_) || Utils::IsDeoptCode(code_)) &&
            !IsCageBaseWritebackReg(instr.ops[2])) {
          return;
        }
        break;
      // Ldur is used by entry builtins to initialize the cage base register.
      case DA64I_LDURX:
        if (!IsCageBaseReg(instr.ops[0])) {
          return;
        }
        VALIDATOR_CHECK(state.is_cage_base_reg_valid_ == BoolValue::kFalse,
                        "Instruction overwrites a valid cage base register");
        if (IsValidCageBaseRegInitialization(instr)) {
          VALIDATOR_CHECK(
              state.is_root_reg_valid_ == BoolValue::kTrue,
              "Cage base register initialization uses invalid root register");
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
  void CheckNoWritesToRootRegister(const uint8_t* pc, const Da64Inst& instr,
                                   const State& state) {
    const bool expecting_init =
        state.root_reg_init_state_.in_progress != BoolValue::kFalse;
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
        // Check that the target is not the root register.
        if (!IsRootReg(instr.ops[0])) {
          // Arithmetic instructions do not support writeback operands.
          is_validated = true;
        }
        break;
      // Orr is used for copying the root register value and by entry builtins
      // to initialize the root register.
      case DA64I_ORR_SHIFT:
        // Check that the target is not the root register.
        if (!IsRootReg(instr.ops[0])) {
          // Logical instructions do not support writeback operands.
          is_validated = true;
          break;
        }
        VALIDATOR_CHECK(state.is_root_reg_valid_ == BoolValue::kFalse,
                        "Instruction overwrites a valid root register");
        if (IsValidRootRegInitialization(instr, state)) {
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
        if ((Utils::IsEntryCode(code_) || Utils::IsDeoptCode(code_)) &&
            !IsRootWritebackReg(instr.ops[2])) {
          is_validated = true;
        }
        break;
      // Mov is used by entry builtins to initialize the root register.
      case DA64I_MOVZ:
      case DA64I_MOVK:
        if (!IsRootReg(instr.ops[0])) {
          is_validated = true;
          break;
        }
        VALIDATOR_CHECK(state.is_root_reg_valid_ == BoolValue::kFalse,
                        "Instruction overwrites a valid root register");
        was_init = true;
        if (IsValidRootRegInitialization(instr, state)) {
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

  // Verifies that system registers are never updated. This prevents
  // unintended modification of CPU control flags or floating-point execution
  // state.
  void CheckNoSystemRegisterWrites(const uint8_t* pc, const Da64Inst& instr) {
    static constexpr Da64InstKind prohibited_instructions[] = {
        DA64I_SYS,         DA64I_SYSL,       DA64I_MSR,
        DA64I_MSR_UAO,     DA64I_MSR_PAN,    DA64I_MSR_SPSel,
        DA64I_MSR_ALLINT,  DA64I_MSR_PM,     DA64I_MSR_SSBS,
        DA64I_MSR_DIT,     DA64I_MSR_TCO,    DA64I_MSR_DAIFSet,
        DA64I_MSR_DAIFClr, DA64I_MSR_SVCRSM, DA64I_MSR_SVCRZA,
        DA64I_MSR_SVCRSMZA};
    VALIDATOR_CHECK(std::find(std::begin(prohibited_instructions),
                              std::end(prohibited_instructions),
                              instr.mnem) == std::end(prohibited_instructions),
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

  bool IsValidCageBaseRegInitialization(const Da64Inst& instr) const {
    DCHECK(IsCageBaseReg(instr.ops[0]));
    return Utils::IsEntryCode(code_) && (instr.mnem == DA64I_LDURX) &&
           IsExpectedOperand(instr.ops[1],
                             {.type = DA_OP_MEMSOFF,
                              .reg = root_register,
                              .simm16 = IsolateData::cage_base_offset()});
  }

  void UpdateRootRegInitialization(const Da64Inst& instr, State& state) const {
    if (!IsValidRootRegInitialization(instr, state)) {
      state.is_root_reg_valid_ = BoolValue::kFalse;
      state.root_reg_init_state_ = {};
      return;
    }
    auto& root_reg_init_state = state.root_reg_init_state_;
    switch (instr.mnem) {
      case DA64I_MOVZ:
        DCHECK_EQ(root_reg_init_state.current_value, kNullAddress);
        [[fallthrough]];
      case DA64I_MOVK: {
        const Da64Op& op = instr.ops[1];
        DCHECK_EQ(DA_OP_UIMMSHIFT, op.type);
        DCHECK_EQ(0, op.immshift.mask);
        const uint64_t mask = 0xFFFFULL << op.immshift.shift;
        root_reg_init_state.current_value =
            (root_reg_init_state.current_value & ~mask) |
            (static_cast<uint64_t>(op.uimm16) << op.immshift.shift);
        const bool is_complete =
            root_reg_init_state.current_value == expected_root_reg_value_;
        state.is_root_reg_valid_ =
            is_complete ? BoolValue::kTrue : BoolValue::kFalse;
        if (is_complete) {
          state.root_reg_init_state_ = {};
        } else {
          root_reg_init_state.in_progress = BoolValue::kTrue;
        }
        break;
      }
      case DA64I_ORR_SHIFT:
        state.is_root_reg_valid_ = BoolValue::kTrue;
        break;
      default:
        UNREACHABLE();
    }
  }

  // `state` is the node's input state before executing `instr`.
  bool IsValidRootRegInitialization(const Da64Inst& instr,
                                    const State& state) const {
    DCHECK(IsRootReg(instr.ops[0]));
    if (!Utils::IsEntryCode(code_)) {
      return false;
    }
    switch (instr.mnem) {
      case DA64I_MOVZ:
      case DA64I_MOVK: {
        // Movz is the first in a sequence of movs to initialize the root
        // register, followed by Movk instructions to construct the expected
        // root register value.
        const BoolValue expected_in_progress =
            instr.mnem == DA64I_MOVZ ? BoolValue::kFalse : BoolValue::kTrue;
        if (state.root_reg_init_state_.in_progress != expected_in_progress) {
          return false;
        }
        const Da64Op& op = instr.ops[1];
        DCHECK_EQ(DA_OP_UIMMSHIFT, op.type);
        DCHECK_EQ(0, op.immshift.mask);
        const uint64_t mask = 0xFFFFULL << op.immshift.shift;
        const uint64_t value = static_cast<uint64_t>(op.uimm16)
                               << op.immshift.shift;
        return ((state.root_reg_init_state_.current_value & mask) == 0) &&
               ((expected_root_reg_value_ & mask) == value);
      }
      case DA64I_ORR_SHIFT:
        // Assumes builtins receive the correct value as their first
        // argument.
        return (code_->kind() == CodeKind::BUILTIN) &&
               IsExpectedReg(instr.ops[2], x0.code()) &&
               IsExpectedReg(instr.ops[1], xzr.code());
      default:
        UNREACHABLE();
    }
  }

  const Tagged<Code> code_;
  ViolationsReporter& violations_reporter_;
  const Address expected_root_reg_value_;
};

namespace {

// TODO(523128533): Handle jump tables, indirect calls, and tail calls.
base::SmallVector<const uint8_t*, 2> GetSuccessors(const uint8_t* pc,
                                                   const Da64Inst& instr,
                                                   const uint8_t* next_pc) {
  bool falls_through = true;
  const uint8_t* jump_target = nullptr;
  switch (instr.mnem) {
    // Unconditional direct branch.
    case DA64I_B:
      falls_through = false;
      jump_target = pc + static_cast<int64_t>(instr.imm64);
      break;
    // Conditional direct branches.
    case DA64I_BCOND:
    case DA64I_BCCOND:
      DCHECK_EQ(instr.ops[0].type, DA_OP_COND);
      if (instr.ops[0].cond == DA_AL || instr.ops[0].cond == DA_NV) {
        falls_through = false;
      }
      jump_target = pc + static_cast<int64_t>(instr.imm64);
      break;
    case DA64I_CBZ:
    case DA64I_CBNZ:
    case DA64I_TBZ:
    case DA64I_TBNZ:
      jump_target = pc + static_cast<int64_t>(instr.imm64);
      break;
    // Indirect branches.
    case DA64I_BR:
    case DA64I_BRAA:
    case DA64I_BRAB:
    case DA64I_BRAAZ:
    case DA64I_BRABZ:
      // TODO(523128533): Handle jump tables and indirect jumps.
      falls_through = false;
      break;
    // Return instructions.
    case DA64I_RET:
    case DA64I_RETAA:
    case DA64I_RETAB:
    case DA64I_ERET:
    case DA64I_ERETAA:
    case DA64I_ERETAB:
    case DA64I_DRPS:
      falls_through = false;
      break;
    // Call instructions are treated as non-control-flow to keep the graph
    // intra-procedural.
    // TODO(523128533): Properly handle calls, including handling of register
    // values across calls.
    case DA64I_BL:
    case DA64I_BLR:
    case DA64I_BLRAA:
    case DA64I_BLRAB:
    case DA64I_BLRAAZ:
    case DA64I_BLRABZ:
      DCHECK(falls_through);
      break;
    default:
      break;
  }
  base::SmallVector<const uint8_t*, 2> successors;
  if (falls_through && next_pc != nullptr) {
    successors.push_back(next_pc);
  }
  if (jump_target != nullptr && (!falls_through || jump_target != next_pc)) {
    successors.push_back(jump_target);
  }
  return successors;
}

}  // namespace

// static
size_t GeneratedCodeValidator::EstimateInstructionCount(Tagged<Code> code) {
  DCHECK_EQ(code->instruction_size() % kInstrSize, 0);
  return code->instruction_size() / kInstrSize;
}

void GeneratedCodeValidator::ValidateImpl(Isolate* isolate, Tagged<Code> code) {
  DCHECK_EQ(code->instruction_size() % kInstrSize, 0);

  ViolationsReporter reporter(code);
  InstructionHandler instruction_handler(isolate, code, reporter);
  ControlFlowGraph<Da64Inst> graph(code);

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
    instruction_handler.CheckStateDataFlowIndependent(pc, instr);
    it.Advance(kInstrSize);
    const uint8_t* next_pc = it.IsDone() ? nullptr : it.GetCurrent();
    graph.AddNode(pc, instr, GetSuccessors(pc, instr, next_pc));
  }

  graph.Validate(instruction_handler);
}

}  // namespace v8::internal

#undef VALIDATOR_CHECK

#endif  // V8_ENABLE_GENERATED_CODE_VALIDATOR
