// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_RISCV_INSTRUCTION_SELECTOR_RISCV_H_
#define V8_COMPILER_BACKEND_RISCV_INSTRUCTION_SELECTOR_RISCV_H_

#include <optional>

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/codegen/machine-type.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction-codes.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/backend/riscv/register-constraints-riscv.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/turboshaft/operation-matcher.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...) PrintF(__VA_ARGS__)

using namespace turboshaft;  // NOLINT(build/namespaces)

static int EncodeElementWidth(VSew sew) {
  // We currently encode the element size in 2 bits.
  // The lane size field has 8 free bits, so there is plenty of room.
  static_assert((0 <= static_cast<int>(VSew::E8)) &&
                (static_cast<int>(VSew::E64) <= 3));
#ifdef DEBUG
  // In debug mode, we mark one bit to indicate that the lane size is
  // populated.
  return LaneSizeField::encode(0x4 | sew);
#else
  return LaneSizeField::encode(sew);
#endif
}

static int EncodeRegisterConstraint(RiscvRegisterConstraint constraint) {
  // The element width is encoded in 3 bits, which leaves us some bits
  // for asserting that the register constraints are correct.
#ifdef DEBUG
  static_assert(static_cast<int>(VSew::E64) <= 3);
  DCHECK(static_cast<int>(constraint) <= 0xF);
  return LaneSizeField::encode(static_cast<int>(constraint) << 3);
#else
  return 0;
#endif
}

static VSew ByteSizeToSew(int byte_size) {
  switch (byte_size) {
    case 1:
      return VSew::E8;
    case 2:
      return VSew::E16;
    case 4:
      return VSew::E32;
    case 8:
      return VSew::E64;
    default:
      UNREACHABLE();
  }
}

// Adds RISC-V-specific methods for generating InstructionOperands.

class RiscvOperandGenerator final : public OperandGenerator {
 public:
  explicit RiscvOperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand UseOperand(OpIndex node, InstructionCode opcode) {
    if (CanBeImmediate(node, opcode)) {
      return UseImmediate(node);
    }
    return UseRegister(node);
  }

  // Use the zero register if the node has the immediate value zero, otherwise
  // assign a register.
  InstructionOperand UseRegisterOrImmediateZero(OpIndex node) {
    if (const ConstantOp* constant =
            selector()->Get(node).TryCast<ConstantOp>()) {
      if ((constant->IsIntegral() && constant->integral() == 0) ||
          (constant->kind == ConstantOp::Kind::kFloat32 &&
           constant->float32().get_bits() == 0) ||
          (constant->kind == ConstantOp::Kind::kFloat64 &&
           constant->float64().get_bits() == 0))
        return UseImmediate(node);
    }
    return UseRegister(node);
  }

  bool IsIntegerConstant(OpIndex node) {
    int64_t unused;
    return selector()->MatchSignedIntegralConstant(node, &unused);
  }

  bool IsIntegerConstant(OptionalOpIndex node) {
    return node.has_value() && IsIntegerConstant(node.value());
  }

  std::optional<int64_t> GetOptionalIntegerConstant(OpIndex operation) {
    if (int64_t constant; MatchSignedIntegralConstant(operation, &constant)) {
      return constant;
    }
    return std::nullopt;
  }

  bool CanBeZero(OpIndex node) { return MatchZero(node); }

  bool CanBeImmediate(OpIndex node, InstructionCode mode) {
    const ConstantOp* constant = selector()->Get(node).TryCast<ConstantOp>();
    if (!constant) return false;
    if (constant->kind == ConstantOp::Kind::kCompressedHeapObject) {
      if (!COMPRESS_POINTERS_BOOL) return false;
      // For builtin code we need static roots
      if (selector()->isolate()->bootstrapper() && !V8_STATIC_ROOTS_BOOL) {
        return false;
      }
      const RootsTable& roots_table = selector()->isolate()->roots_table();
      RootIndex root_index;
      Handle<HeapObject> value = constant->handle();
      if (roots_table.IsRootHandle(value, &root_index)) {
        if (!RootsTable::IsReadOnly(root_index)) return false;
        return CanBeImmediate(MacroAssemblerBase::ReadOnlyRootPtr(
                                  root_index, selector()->isolate()),
                              mode);
      }
      return false;
    }

    int64_t value;
    return selector()->MatchSignedIntegralConstant(node, &value) &&
           CanBeImmediate(value, mode);
  }

  bool CanBeImmediate(int64_t value, InstructionCode opcode);

 private:
  bool ImmediateFitsAddrMode1Instruction(int32_t imm) const {
    TRACE("UNIMPLEMENTED instr_sel: %s at line %d\n", __FUNCTION__, __LINE__);
    return false;
  }
};

void VisitRR(InstructionSelector* selector, ArchOpcode opcode, OpIndex node) {
  RiscvOperandGenerator g(selector);
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 1);
  selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

void VisitRR(InstructionSelector* selector, InstructionCode opcode,
             OpIndex node) {
  RiscvOperandGenerator g(selector);
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 1);
  selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

static void VisitRRI(InstructionSelector* selector, ArchOpcode opcode,
                     OpIndex node) {
  RiscvOperandGenerator g(selector);
  const Operation& op = selector->Get(node);
  int imm = op.template Cast<Simd128ExtractLaneOp>().lane;
  selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
                 g.UseImmediate(imm));
}

static void VisitSimdShift(InstructionSelector* selector, ArchOpcode opcode,
                           OpIndex node) {
  RiscvOperandGenerator g(selector);
  const Operation& op = selector->Get(node);
  OpIndex rhs = op.input(0);
  if (selector->Get(rhs).TryCast<ConstantOp>()) {
    selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
                   g.UseImmediate(op.input(1)));
  } else {
    selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
                   g.UseRegister(op.input(1)));
  }
}

static void VisitRRIR(InstructionSelector* selector, ArchOpcode opcode,
                      OpIndex node) {
  RiscvOperandGenerator g(selector);
  const turboshaft::Simd128ReplaceLaneOp& op =
      selector->Get(node).template Cast<turboshaft::Simd128ReplaceLaneOp>();
  selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
                 g.UseImmediate(op.lane), g.UseRegister(op.input(1)));
}

void VisitRRR(InstructionSelector* selector, InstructionCode opcode,
              OpIndex node,
              typename OperandGenerator::RegisterUseKind kind =
                  OperandGenerator::RegisterUseKind::kUseRegister) {
  RiscvOperandGenerator g(selector);
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 2);
  selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
                 g.UseRegister(op.input(1), kind));
}

void VisitRRRR(InstructionSelector* selector, ArchOpcode opcode, OpIndex node) {
  RiscvOperandGenerator g(selector);
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 3);
  selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(op.input(0)),
                 g.UseRegister(op.input(1)), g.UseRegister(op.input(2)));
}

static void VisitRRO(InstructionSelector* selector, ArchOpcode opcode,
                     OpIndex node) {
  RiscvOperandGenerator g(selector);
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 2);
  selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
                 g.UseOperand(op.input(1), opcode));
}

bool TryMatchImmediate(InstructionSelector* selector,
                       InstructionCode* opcode_return, OpIndex node,
                       size_t* input_count_return, InstructionOperand* inputs) {
  RiscvOperandGenerator g(selector);
  if (g.CanBeImmediate(node, *opcode_return)) {
    *opcode_return |= AddressingModeField::encode(kMode_MRI);
    inputs[0] = g.UseImmediate(node);
    *input_count_return = 1;
    return true;
  }
  return false;
}

// Shared routine for multiple binary operations.
template <typename Matcher>
static void VisitBinop(InstructionSelector* selector, OpIndex node,
                       InstructionCode opcode, bool has_reverse_opcode,
                       InstructionCode reverse_opcode,
                       FlagsContinuation* cont) {
  RiscvOperandGenerator g(selector);
  InstructionOperand inputs[2];
  size_t input_count = 0;
  InstructionOperand outputs[1];
  size_t output_count = 0;

  const Operation& binop = selector->Get(node);
  OpIndex left_node = binop.input(0);
  OpIndex right_node = binop.input(1);

  if (TryMatchImmediate(selector, &opcode, right_node, &input_count,
                        &inputs[1])) {
    // If right node is an immediate, we will use left to check overflow
    // so we hope left isn't overlapped by output.
    if (!cont->IsNone() &&
        (cont->condition() == FlagsCondition::kOverflow ||
         cont->condition() == FlagsCondition::kNotOverflow)) {
      inputs[0] = g.UseUniqueRegister(left_node);
    } else {
      inputs[0] = g.UseRegisterOrImmediateZero(left_node);
    }
    input_count++;
  } else if (has_reverse_opcode &&
             TryMatchImmediate(selector, &reverse_opcode, left_node,
                               &input_count, &inputs[1])) {
    inputs[0] = g.UseRegisterOrImmediateZero(right_node);
    opcode = reverse_opcode;
    input_count++;
  } else {
    inputs[input_count++] = g.UseRegister(left_node);
    inputs[input_count++] = g.UseOperand(right_node, opcode);
  }

  outputs[output_count++] = g.DefineAsRegister(node);

  DCHECK_NE(0u, input_count);
  DCHECK_EQ(1u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}

template <typename Matcher>
static void VisitBinop(InstructionSelector* selector, OpIndex node,
                       InstructionCode opcode, bool has_reverse_opcode,
                       InstructionCode reverse_opcode) {
  FlagsContinuation cont;
  VisitBinop<Matcher>(selector, node, opcode, has_reverse_opcode,
                      reverse_opcode, &cont);
}

template <typename Matcher>
static void VisitBinop(InstructionSelector* selector, OpIndex node,
                       InstructionCode opcode, FlagsContinuation* cont) {
  VisitBinop<Matcher>(selector, node, opcode, false, kArchNop, cont);
}

template <typename Matcher>
static void VisitBinop(InstructionSelector* selector, OpIndex node,
                       InstructionCode opcode) {
  VisitBinop<Matcher>(selector, node, opcode, false, kArchNop);
}

void InstructionSelector::VisitStackSlot(OpIndex node) {
  const StackSlotOp& stack_slot = Cast<StackSlotOp>(node);
  int slot = frame_->AllocateSpillSlot(stack_slot.size, stack_slot.alignment,
                                       stack_slot.is_tagged);
  OperandGenerator g(this);

  Emit(kArchStackSlot, g.DefineAsRegister(node),
       sequence()->AddImmediate(Constant(slot)), 0, nullptr);
}

void InstructionSelector::VisitAbortCSADcheck(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kArchAbortCSADcheck, g.NoOutput(), g.UseFixed(op.input(0), a0));
}

void EmitS128Load(InstructionSelector* selector, OpIndex node,
                  InstructionCode opcode);

void InstructionSelector::VisitLoadTransform(OpIndex node) {
  const Simd128LoadTransformOp& op =
      this->Get(node).Cast<Simd128LoadTransformOp>();
  bool is_protected = (op.load_kind.with_trap_handler);
  InstructionCode opcode = kArchNop;
  switch (op.transform_kind) {
    case Simd128LoadTransformOp::TransformKind::k8Splat:
      opcode = kRiscvS128LoadSplat | EncodeElementWidth(E8);
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode);
      break;
    case Simd128LoadTransformOp::TransformKind::k16Splat:
      opcode = kRiscvS128LoadSplat | EncodeElementWidth(E16);
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode);
      break;
    case Simd128LoadTransformOp::TransformKind::k32Splat:
      opcode = kRiscvS128LoadSplat | EncodeElementWidth(E32);
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode);
      break;
    case Simd128LoadTransformOp::TransformKind::k64Splat:
      opcode = kRiscvS128LoadSplat | EncodeElementWidth(E64);
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode);
      break;
    case Simd128LoadTransformOp::TransformKind::k8x8S:
      opcode = kRiscvS128Load64ExtendS | EncodeElementWidth(E16);
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode);
      break;
    case Simd128LoadTransformOp::TransformKind::k8x8U:
      opcode = kRiscvS128Load64ExtendU | EncodeElementWidth(E16);
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode);
      break;
    case Simd128LoadTransformOp::TransformKind::k16x4S:
      opcode = kRiscvS128Load64ExtendS | EncodeElementWidth(E32);
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode);
      break;
    case Simd128LoadTransformOp::TransformKind::k16x4U:
      opcode = kRiscvS128Load64ExtendU | EncodeElementWidth(E32);
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode);
      break;
    case Simd128LoadTransformOp::TransformKind::k32x2S:
      opcode = kRiscvS128Load64ExtendS | EncodeElementWidth(E64);
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode);
      break;
    case Simd128LoadTransformOp::TransformKind::k32x2U:
      opcode = kRiscvS128Load64ExtendU | EncodeElementWidth(E64);
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode);
      break;
    case Simd128LoadTransformOp::TransformKind::k32Zero:
      opcode = kRiscvS128Load32Zero;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode);
      break;
    case Simd128LoadTransformOp::TransformKind::k64Zero:
      opcode = kRiscvS128Load64Zero;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode);
      break;
    default:
      UNIMPLEMENTED();
  }
}

// Shared routine for multiple compare operations.

static Instruction* VisitCompare(InstructionSelector* selector,
                                 InstructionCode opcode,
                                 InstructionOperand left,
                                 InstructionOperand right,
                                 FlagsContinuation* cont) {
#ifdef V8_COMPRESS_POINTERS
  if (opcode == kRiscvCmp32) {
    RiscvOperandGenerator g(selector);
    InstructionOperand inputs[] = {left, right};
    if (right.IsImmediate()) {
      InstructionOperand temps[1] = {g.TempRegister()};
      return selector->EmitWithContinuation(opcode, 0, nullptr,
                                            arraysize(inputs), inputs,
                                            arraysize(temps), temps, cont);
    } else {
      InstructionOperand temps[2] = {g.TempRegister(), g.TempRegister()};
      return selector->EmitWithContinuation(opcode, 0, nullptr,
                                            arraysize(inputs), inputs,
                                            arraysize(temps), temps, cont);
    }
  }
#endif
  return selector->EmitWithContinuation(opcode, left, right, cont);
}

// Shared routine for multiple compare operations.

static Instruction* VisitWordCompareZero(InstructionSelector* selector,
                                         InstructionOperand value,
                                         FlagsContinuation* cont) {
  return selector->EmitWithContinuation(kRiscvCmpZero, value, cont);
}

// Shared routine for multiple float32 compare operations.

void VisitFloat32Compare(InstructionSelector* selector, OpIndex node,
                         FlagsContinuation* cont) {
  RiscvOperandGenerator g(selector);
  const ComparisonOp& op = selector->Get(node).template Cast<ComparisonOp>();
  OpIndex left = op.left();
  OpIndex right = op.right();
  if (selector->MatchZero(right)) {
    VisitCompare(selector, kRiscvCmpS, g.UseRegister(left),
                 g.UseImmediate(right), cont);
  } else if (selector->MatchZero(left)) {
    cont->Commute();
    VisitCompare(selector, kRiscvCmpS, g.UseRegister(right),
                 g.UseImmediate(left), cont);
  } else {
    VisitCompare(selector, kRiscvCmpS, g.UseRegister(left),
                 g.UseRegister(right), cont);
  }
}

// Shared routine for multiple float64 compare operations.

void VisitFloat64Compare(InstructionSelector* selector, OpIndex node,
                         FlagsContinuation* cont) {
  RiscvOperandGenerator g(selector);
  const Operation& compare = selector->Get(node);
  DCHECK(compare.Is<ComparisonOp>());
  OpIndex lhs = compare.input(0);
  OpIndex rhs = compare.input(1);
  if (selector->MatchZero(rhs)) {
    VisitCompare(selector, kRiscvCmpD, g.UseRegister(lhs), g.UseImmediate(rhs),
                 cont);
  } else if (selector->MatchZero(lhs)) {
    VisitCompare(selector, kRiscvCmpD, g.UseImmediate(lhs), g.UseRegister(rhs),
                 cont);
  } else {
    VisitCompare(selector, kRiscvCmpD, g.UseRegister(lhs), g.UseRegister(rhs),
                 cont);
  }
}

// Shared routine for multiple word compare operations.

Instruction* VisitWordCompare(InstructionSelector* selector, OpIndex node,
                              InstructionCode opcode, FlagsContinuation* cont,
                              bool commutative) {
  RiscvOperandGenerator g(selector);
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 2);
  auto left = op.input(0);
  auto right = op.input(1);
  // If one of the two inputs is an immediate, make sure it's on the right.
  if (!g.CanBeImmediate(right, opcode) && g.CanBeImmediate(left, opcode)) {
    cont->Commute();
    std::swap(left, right);
  }
  // Match immediates on right side of comparison.
  if (g.CanBeImmediate(right, opcode)) {
#if V8_TARGET_ARCH_RISCV64
    if (opcode == kRiscvTst64 || opcode == kRiscvTst32) {
#elif V8_TARGET_ARCH_RISCV32
    if (opcode == kRiscvTst32) {
#endif
      return VisitCompare(selector, opcode, g.UseRegister(left),
                          g.UseImmediate(right), cont);
    } else {
      switch (cont->condition()) {
        case kEqual:
        case kNotEqual:
          if (cont->IsSet()) {
            return VisitCompare(selector, opcode, g.UseRegister(left),
                                g.UseImmediate(right), cont);
          } else {
            if (g.CanBeZero(right)) {
              return VisitWordCompareZero(
                  selector, g.UseRegisterOrImmediateZero(left), cont);
            } else {
              return VisitCompare(selector, opcode, g.UseRegister(left),
                                  g.UseRegister(right), cont);
            }
          }
          break;
        case kSignedLessThan:
        case kSignedGreaterThanOrEqual:
        case kUnsignedLessThan:
        case kUnsignedGreaterThanOrEqual: {
          if (g.CanBeZero(right)) {
            return VisitWordCompareZero(
                selector, g.UseRegisterOrImmediateZero(left), cont);
          } else {
            return VisitCompare(selector, opcode, g.UseRegister(left),
                                g.UseImmediate(right), cont);
          }
        } break;
        default:
          if (g.CanBeZero(right)) {
            return VisitWordCompareZero(
                selector, g.UseRegisterOrImmediateZero(left), cont);
          } else {
            return VisitCompare(selector, opcode, g.UseRegister(left),
                                g.UseRegister(right), cont);
          }
      }
    }
  } else {
    return VisitCompare(selector, opcode, g.UseRegister(left),
                        g.UseRegister(right), cont);
  }
}

void InstructionSelector::VisitSwitch(OpIndex node, const SwitchInfo& sw) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  InstructionOperand value_operand = g.UseRegister(op.input(0));

  // Emit either ArchTableSwitch or ArchBinarySearchSwitch.
  if (enable_switch_jump_table_) {
    static const size_t kMaxTableSwitchValueRange = 2 << 16;
    size_t table_space_cost = 10 + 2 * sw.value_range();
    size_t table_time_cost = 3;
    size_t lookup_space_cost = 2 + 2 * sw.case_count();
    size_t lookup_time_cost = sw.case_count();
    if (sw.case_count() > 0 &&
        table_space_cost + 3 * table_time_cost <=
            lookup_space_cost + 3 * lookup_time_cost &&
        sw.min_value() > std::numeric_limits<int32_t>::min() &&
        sw.value_range() <= kMaxTableSwitchValueRange) {
      InstructionOperand index_operand = value_operand;
      if (sw.min_value()) {
        index_operand = g.TempRegister();
        Emit(kRiscvSub32, index_operand, value_operand,
             g.TempImmediate(sw.min_value()));
      }
      // Generate a table lookup.
      return EmitTableSwitch(sw, index_operand);
    }
  }

  // Generate a tree of conditional jumps.
  return EmitBinarySearchSwitch(sw, value_operand);
}

void EmitWordCompareZero(InstructionSelector* selector, OpIndex value,
                         FlagsContinuation* cont) {
  RiscvOperandGenerator g(selector);
  selector->EmitWithContinuation(kRiscvCmpZero,
                                 g.UseRegisterOrImmediateZero(value), cont);
}

#ifdef V8_TARGET_ARCH_RISCV64

void EmitWord32CompareZero(InstructionSelector* selector, OpIndex value,
                           FlagsContinuation* cont) {
  RiscvOperandGenerator g(selector);
  InstructionOperand inputs[] = {g.UseRegisterOrImmediateZero(value)};
  InstructionOperand temps[] = {g.TempRegister()};
  selector->EmitWithContinuation(kRiscvCmpZero32, 0, nullptr, arraysize(inputs),
                                 inputs, arraysize(temps), temps, cont);
}
#endif

void InstructionSelector::VisitFloat32Equal(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat32LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kFloatLessThan, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat32LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kFloatLessThanOrEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64Equal(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kFloatLessThan, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kFloatLessThanOrEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64ExtractLowWord32(OpIndex node) {
  VisitRR(this, kRiscvFloat64ExtractLowWord32, node);
}

void InstructionSelector::VisitFloat64ExtractHighWord32(OpIndex node) {
  VisitRR(this, kRiscvFloat64ExtractHighWord32, node);
}

void InstructionSelector::VisitFloat64SilenceNaN(OpIndex node) {
  VisitRR(this, kRiscvFloat64SilenceNaN, node);
}

void InstructionSelector::VisitBitcastWord32PairToFloat64(OpIndex node) {
  RiscvOperandGenerator g(this);
  const auto& bitcast =
      this->Cast<turboshaft::BitcastWord32PairToFloat64Op>(node);
  OpIndex hi = bitcast.high_word32();
  OpIndex lo = bitcast.low_word32();
  // TODO(nicohartmann@): We could try to emit a better sequence here.
  InstructionOperand zero = sequence()->AddImmediate(Constant(0.0));
  InstructionOperand temp = g.TempDoubleRegister();
  Emit(kRiscvFloat64InsertHighWord32, temp, zero, g.Use(hi));
  Emit(kRiscvFloat64InsertLowWord32, g.DefineSameAsFirst(node), temp,
       g.Use(lo));
}

void InstructionSelector::VisitFloat64InsertLowWord32(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  OpIndex left = op.input(0);
  OpIndex right = op.input(1);
  Emit(kRiscvFloat64InsertLowWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.UseRegister(right));
}

void InstructionSelector::VisitFloat64InsertHighWord32(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  OpIndex left = op.input(0);
  OpIndex right = op.input(1);
  Emit(kRiscvFloat64InsertHighWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.UseRegister(right));
}

void InstructionSelector::VisitMemoryBarrier(OpIndex node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvSync, g.NoOutput());
}

bool InstructionSelector::IsTailCallAddressImmediate() { return false; }

void InstructionSelector::EmitPrepareResults(
    ZoneVector<PushParameter>* results, const CallDescriptor* call_descriptor,
    OpIndex node) {
  RiscvOperandGenerator g(this);

  for (PushParameter output : *results) {
    if (!output.location.IsCallerFrameSlot()) continue;
    // Skip any alignment holes in nodes.
    if (output.node.valid()) {
      DCHECK(!call_descriptor->IsCFunctionCall());
      if (output.location.GetType() == MachineType::Float32()) {
        MarkAsFloat32(output.node);
      } else if (output.location.GetType() == MachineType::Float64()) {
        MarkAsFloat64(output.node);
      } else if (output.location.GetType() == MachineType::Simd128()) {
        MarkAsSimd128(output.node);
      }
      int offset = call_descriptor->GetOffsetToReturns();
      int reverse_slot = -output.location.GetLocation() - offset;
      Emit(kRiscvPeek, g.DefineAsRegister(output.node),
           g.UseImmediate(reverse_slot));
    }
  }
}

void InstructionSelector::EmitMoveParamToFPR(OpIndex node, int index) {}

void InstructionSelector::EmitMoveFPRToParam(InstructionOperand* op,
                                             LinkageLocation location) {}

void InstructionSelector::VisitFloat32Abs(OpIndex node) {
  VisitRR(this, kRiscvAbsS, node);
}

void InstructionSelector::VisitFloat64Abs(OpIndex node) {
  VisitRR(this, kRiscvAbsD, node);
}

void InstructionSelector::VisitFloat32Sqrt(OpIndex node) {
  VisitRR(this, kRiscvSqrtS, node);
}

void InstructionSelector::VisitFloat64Sqrt(OpIndex node) {
  VisitRR(this, kRiscvSqrtD, node);
}

void InstructionSelector::VisitFloat32RoundDown(OpIndex node) {
  VisitRR(this, kRiscvFloat32RoundDown, node);
}

void InstructionSelector::VisitFloat32Add(OpIndex node) {
  VisitRRR(this, kRiscvAddS, node);
}

void InstructionSelector::VisitFloat64Add(OpIndex node) {
  VisitRRR(this, kRiscvAddD, node);
}

void InstructionSelector::VisitFloat32Sub(OpIndex node) {
  VisitRRR(this, kRiscvSubS, node);
}

void InstructionSelector::VisitFloat64Sub(OpIndex node) {
  VisitRRR(this, kRiscvSubD, node);
}

void InstructionSelector::VisitFloat32Mul(OpIndex node) {
  VisitRRR(this, kRiscvMulS, node);
}

void InstructionSelector::VisitFloat64Mul(OpIndex node) {
  VisitRRR(this, kRiscvMulD, node);
}

void InstructionSelector::VisitFloat32Div(OpIndex node) {
  VisitRRR(this, kRiscvDivS, node);
}

void InstructionSelector::VisitFloat64Div(OpIndex node) {
  VisitRRR(this, kRiscvDivD, node);
}

void InstructionSelector::VisitFloat64Mod(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  Emit(kRiscvModD, g.DefineAsFixed(node, fa0), g.UseFixed(op.input(0), fa0),
       g.UseFixed(op.input(1), fa1))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat32Max(OpIndex node) {
  VisitRRR(this, kRiscvFloat32Max, node);
}

void InstructionSelector::VisitFloat64Max(OpIndex node) {
  VisitRRR(this, kRiscvFloat64Max, node);
}

void InstructionSelector::VisitFloat32Min(OpIndex node) {
  VisitRRR(this, kRiscvFloat32Min, node);
}

void InstructionSelector::VisitFloat64Min(OpIndex node) {
  VisitRRR(this, kRiscvFloat64Min, node);
}

void InstructionSelector::VisitTruncateFloat64ToWord32(OpIndex node) {
  VisitRR(this, kArchTruncateDoubleToI, node);
}

void InstructionSelector::VisitRoundFloat64ToInt32(OpIndex node) {
  VisitRR(this, kRiscvTruncWD, node);
}

void InstructionSelector::VisitTruncateFloat64ToFloat32(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  OpIndex value = op.input(0);
  // Match TruncateFloat64ToFloat32(ChangeInt32ToFloat64) to corresponding
  // instruction.
  using Rep = turboshaft::RegisterRepresentation;
  if (CanCover(node, value)) {
    const turboshaft::Operation& op = this->Get(value);
    if (op.Is<turboshaft::ChangeOp>()) {
      const turboshaft::ChangeOp& change = op.Cast<turboshaft::ChangeOp>();
      if (change.kind == turboshaft::ChangeOp::Kind::kSignedToFloat) {
        if (change.from == Rep::Word32() && change.to == Rep::Float64()) {
          Emit(kRiscvCvtSW, g.DefineAsRegister(node),
               g.UseRegister(op.input(0)));
          return;
        }
      }
    }
  }
  VisitRR(this, kRiscvCvtSD, node);
}

void InstructionSelector::VisitWord32Shl(OpIndex node) {
  // todo(RISCV): Optimize it
  VisitRRO(this, kRiscvShl32, node);
}

void InstructionSelector::VisitWord32Shr(OpIndex node) {
  VisitRRO(this, kRiscvShr32, node);
}

void InstructionSelector::VisitWord32Sar(OpIndex node) {
  const ShiftOp& shift = Get(node).Cast<ShiftOp>();
  const Operation& lhs = Get(shift.left());
  if (lhs.Is<Opmask::kTruncateWord64ToWord32>() &&
      CanCover(node, lhs.input(0))) {
    RiscvOperandGenerator g(this);
    Emit(kRiscvSar32, g.DefineAsRegister(node), g.UseRegister(lhs.input(0)),
         g.UseOperand(shift.right(), kRiscvSar32));
    return;
  }
  VisitRRO(this, kRiscvSar32, node);
}

void InstructionSelector::VisitI32x4ExtAddPairwiseI16x8S(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  InstructionCode opcode = kRiscvExtAddPairwiseS | EncodeElementWidth(E16);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitI32x4ExtAddPairwiseI16x8U(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  InstructionCode opcode = kRiscvExtAddPairwiseU | EncodeElementWidth(E16);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitI16x8ExtAddPairwiseI8x16S(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  InstructionCode opcode = kRiscvExtAddPairwiseS | EncodeElementWidth(E8);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitI16x8ExtAddPairwiseI8x16U(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  InstructionCode opcode = kRiscvExtAddPairwiseU | EncodeElementWidth(E8);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

#define SIMD_INT_TYPE_LIST(V) \
  V(I64x2, E64)               \
  V(I32x4, E32)               \
  V(I16x8, E16)               \
  V(I8x16, E8)

#define SIMD_TYPE_LIST(V) \
  V(F32x4)                \
  V(I64x2)                \
  V(I32x4)                \
  V(I16x8)                \
  V(I8x16)

#define SIMD_UNOP_LIST2(V)             \
  V(F32x4Splat, kRiscvVfmvVf, E32)     \
  V(I8x16Neg, kRiscvVnegVv, E8)        \
  V(I16x8Neg, kRiscvVnegVv, E16)       \
  V(I32x4Neg, kRiscvVnegVv, E32)       \
  V(I64x2Neg, kRiscvVnegVv, E64)       \
  V(I8x16Splat, kRiscvVmv, E8)         \
  V(I16x8Splat, kRiscvVmv, E16)        \
  V(I32x4Splat, kRiscvVmv, E32)        \
  V(I64x2Splat, kRiscvVmv, E64)        \
  V(F32x4Neg, kRiscvVfnegVv, E32)      \
  V(F64x2Neg, kRiscvVfnegVv, E64)      \
  V(F64x2Splat, kRiscvVfmvVf, E64)     \
  V(I32x4AllTrue, kRiscvVAllTrue, E32) \
  V(I16x8AllTrue, kRiscvVAllTrue, E16) \
  V(I8x16AllTrue, kRiscvVAllTrue, E8)  \
  V(I64x2AllTrue, kRiscvVAllTrue, E64) \
  V(I64x2Abs, kRiscvVAbs, E64)         \
  V(I32x4Abs, kRiscvVAbs, E32)         \
  V(I16x8Abs, kRiscvVAbs, E16)         \
  V(I8x16Abs, kRiscvVAbs, E8)

#define SIMD_UNOP_LIST(V)                                       \
  V(F64x2Abs, kRiscvF64x2Abs)                                   \
  V(F64x2Sqrt, kRiscvF64x2Sqrt)                                 \
  V(F64x2ConvertLowI32x4S, kRiscvF64x2ConvertLowI32x4S)         \
  V(F64x2ConvertLowI32x4U, kRiscvF64x2ConvertLowI32x4U)         \
  V(F64x2PromoteLowF32x4, kRiscvF64x2PromoteLowF32x4)           \
  V(F64x2Ceil, kRiscvF64x2Ceil)                                 \
  V(F64x2Floor, kRiscvF64x2Floor)                               \
  V(F64x2Trunc, kRiscvF64x2Trunc)                               \
  V(F64x2NearestInt, kRiscvF64x2NearestInt)                     \
  V(F32x4SConvertI32x4, kRiscvF32x4SConvertI32x4)               \
  V(F32x4UConvertI32x4, kRiscvF32x4UConvertI32x4)               \
  V(F32x4Abs, kRiscvF32x4Abs)                                   \
  V(F32x4Sqrt, kRiscvF32x4Sqrt)                                 \
  V(F32x4DemoteF64x2Zero, kRiscvF32x4DemoteF64x2Zero)           \
  V(F32x4Ceil, kRiscvF32x4Ceil)                                 \
  V(F32x4Floor, kRiscvF32x4Floor)                               \
  V(F32x4Trunc, kRiscvF32x4Trunc)                               \
  V(F32x4NearestInt, kRiscvF32x4NearestInt)                     \
  V(I32x4RelaxedTruncF32x4S, kRiscvI32x4SConvertF32x4)          \
  V(I32x4RelaxedTruncF32x4U, kRiscvI32x4UConvertF32x4)          \
  V(I32x4RelaxedTruncF64x2SZero, kRiscvI32x4TruncSatF64x2SZero) \
  V(I32x4RelaxedTruncF64x2UZero, kRiscvI32x4TruncSatF64x2UZero) \
  V(I64x2SConvertI32x4Low, kRiscvI64x2SConvertI32x4Low)         \
  V(I64x2SConvertI32x4High, kRiscvI64x2SConvertI32x4High)       \
  V(I64x2UConvertI32x4Low, kRiscvI64x2UConvertI32x4Low)         \
  V(I64x2UConvertI32x4High, kRiscvI64x2UConvertI32x4High)       \
  V(I32x4SConvertF32x4, kRiscvI32x4SConvertF32x4)               \
  V(I32x4UConvertF32x4, kRiscvI32x4UConvertF32x4)               \
  V(I32x4TruncSatF64x2SZero, kRiscvI32x4TruncSatF64x2SZero)     \
  V(I32x4TruncSatF64x2UZero, kRiscvI32x4TruncSatF64x2UZero)     \
  V(I8x16Popcnt, kRiscvI8x16Popcnt)                             \
  V(S128Not, kRiscvVnot)                                        \
  V(V128AnyTrue, kRiscvV128AnyTrue)

#define SIMD_SHIFT_OP_LIST(V) \
  V(I64x2Shl)                 \
  V(I64x2ShrS)                \
  V(I64x2ShrU)                \
  V(I32x4Shl)                 \
  V(I32x4ShrS)                \
  V(I32x4ShrU)                \
  V(I16x8Shl)                 \
  V(I16x8ShrS)                \
  V(I16x8ShrU)                \
  V(I8x16Shl)                 \
  V(I8x16ShrS)                \
  V(I8x16ShrU)

#define SIMD_BINOP_LIST(V)                \
  V(I64x2Add, kRiscvVaddVv, E64)          \
  V(I32x4Add, kRiscvVaddVv, E32)          \
  V(I16x8Add, kRiscvVaddVv, E16)          \
  V(I8x16Add, kRiscvVaddVv, E8)           \
  V(I64x2Sub, kRiscvVsubVv, E64)          \
  V(I32x4Sub, kRiscvVsubVv, E32)          \
  V(I16x8Sub, kRiscvVsubVv, E16)          \
  V(I8x16Sub, kRiscvVsubVv, E8)           \
  V(I32x4MaxU, kRiscvVmaxuVv, E32)        \
  V(I16x8MaxU, kRiscvVmaxuVv, E16)        \
  V(I8x16MaxU, kRiscvVmaxuVv, E8)         \
  V(I32x4MaxS, kRiscvVmax, E32)           \
  V(I16x8MaxS, kRiscvVmax, E16)           \
  V(I8x16MaxS, kRiscvVmax, E8)            \
  V(I32x4MinS, kRiscvVminsVv, E32)        \
  V(I16x8MinS, kRiscvVminsVv, E16)        \
  V(I8x16MinS, kRiscvVminsVv, E8)         \
  V(I32x4MinU, kRiscvVminuVv, E32)        \
  V(I16x8MinU, kRiscvVminuVv, E16)        \
  V(I8x16MinU, kRiscvVminuVv, E8)         \
  V(I64x2Mul, kRiscvVmulVv, E64)          \
  V(I32x4Mul, kRiscvVmulVv, E32)          \
  V(I16x8Mul, kRiscvVmulVv, E16)          \
  V(I64x2GtS, kRiscvVgtsVv, E64)          \
  V(I32x4GtS, kRiscvVgtsVv, E32)          \
  V(I16x8GtS, kRiscvVgtsVv, E16)          \
  V(I8x16GtS, kRiscvVgtsVv, E8)           \
  V(I64x2GeS, kRiscvVgesVv, E64)          \
  V(I32x4GeS, kRiscvVgesVv, E32)          \
  V(I16x8GeS, kRiscvVgesVv, E16)          \
  V(I8x16GeS, kRiscvVgesVv, E8)           \
  V(I32x4GeU, kRiscvVgeuVv, E32)          \
  V(I16x8GeU, kRiscvVgeuVv, E16)          \
  V(I8x16GeU, kRiscvVgeuVv, E8)           \
  V(I32x4GtU, kRiscvVgtuVv, E32)          \
  V(I16x8GtU, kRiscvVgtuVv, E16)          \
  V(I8x16GtU, kRiscvVgtuVv, E8)           \
  V(I64x2Eq, kRiscvVeqVv, E64)            \
  V(I32x4Eq, kRiscvVeqVv, E32)            \
  V(I16x8Eq, kRiscvVeqVv, E16)            \
  V(I8x16Eq, kRiscvVeqVv, E8)             \
  V(I64x2Ne, kRiscvVneVv, E64)            \
  V(I32x4Ne, kRiscvVneVv, E32)            \
  V(I16x8Ne, kRiscvVneVv, E16)            \
  V(I8x16Ne, kRiscvVneVv, E8)             \
  V(I16x8AddSatS, kRiscvVaddSatSVv, E16)  \
  V(I8x16AddSatS, kRiscvVaddSatSVv, E8)   \
  V(I16x8AddSatU, kRiscvVaddSatUVv, E16)  \
  V(I8x16AddSatU, kRiscvVaddSatUVv, E8)   \
  V(I16x8SubSatS, kRiscvVsubSatSVv, E16)  \
  V(I8x16SubSatS, kRiscvVsubSatSVv, E8)   \
  V(I16x8SubSatU, kRiscvVsubSatUVv, E16)  \
  V(I8x16SubSatU, kRiscvVsubSatUVv, E8)   \
  V(F64x2Add, kRiscvVfaddVv, E64)         \
  V(F32x4Add, kRiscvVfaddVv, E32)         \
  V(F64x2Sub, kRiscvVfsubVv, E64)         \
  V(F32x4Sub, kRiscvVfsubVv, E32)         \
  V(F64x2Mul, kRiscvVfmulVv, E64)         \
  V(F32x4Mul, kRiscvVfmulVv, E32)         \
  V(F64x2Div, kRiscvVfdivVv, E64)         \
  V(F32x4Div, kRiscvVfdivVv, E32)         \
  V(S128And, kRiscvVandVv, E8)            \
  V(S128Or, kRiscvVorVv, E8)              \
  V(S128Xor, kRiscvVxorVv, E8)            \
  V(I16x8Q15MulRSatS, kRiscvVsmulVv, E16) \
  V(I16x8RelaxedQ15MulRS, kRiscvVsmulVv, E16)

#define UNIMPLEMENTED_SIMD_FP16_OP_LIST(V) \
  V(F16x8Splat)                            \
  V(F16x8ExtractLane)                      \
  V(F16x8ReplaceLane)                      \
  V(F16x8Abs)                              \
  V(F16x8Neg)                              \
  V(F16x8Sqrt)                             \
  V(F16x8Floor)                            \
  V(F16x8Ceil)                             \
  V(F16x8Trunc)                            \
  V(F16x8NearestInt)                       \
  V(F16x8Add)                              \
  V(F16x8Sub)                              \
  V(F16x8Mul)                              \
  V(F16x8Div)                              \
  V(F16x8Min)                              \
  V(F16x8Max)                              \
  V(F16x8Pmin)                             \
  V(F16x8Pmax)                             \
  V(F16x8Eq)                               \
  V(F16x8Ne)                               \
  V(F16x8Lt)                               \
  V(F16x8Le)                               \
  V(F16x8SConvertI16x8)                    \
  V(F16x8UConvertI16x8)                    \
  V(I16x8SConvertF16x8)                    \
  V(I16x8UConvertF16x8)                    \
  V(F16x8DemoteF32x4Zero)                  \
  V(F16x8DemoteF64x2Zero)                  \
  V(F32x4PromoteLowF16x8)                  \
  V(F16x8Qfma)                             \
  V(F16x8Qfms)

#define SIMD_VISIT_UNIMPL_FP16_OP(Name) \
                                        \
  void InstructionSelector::Visit##Name(OpIndex node) { UNIMPLEMENTED(); }
UNIMPLEMENTED_SIMD_FP16_OP_LIST(SIMD_VISIT_UNIMPL_FP16_OP)
#undef SIMD_VISIT_UNIMPL_FP16_OP
#undef UNIMPLEMENTED_SIMD_FP16_OP_LIST

void InstructionSelector::VisitS128AndNot(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  Emit(kRiscvS128AndNot, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
       g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitS128Const(OpIndex node) {
  RiscvOperandGenerator g(this);
  static const int kUint32Immediates = kSimd128Size / sizeof(uint32_t);
  uint32_t val[kUint32Immediates];
  const turboshaft::Simd128ConstantOp& constant =
      this->Get(node).template Cast<turboshaft::Simd128ConstantOp>();
  memcpy(val, constant.value, kSimd128Size);
  // If all bytes are zeros or ones, avoid emitting code for generic constants
  bool all_zeros = !(val[0] || val[1] || val[2] || val[3]);
  bool all_ones = val[0] == UINT32_MAX && val[1] == UINT32_MAX &&
                  val[2] == UINT32_MAX && val[3] == UINT32_MAX;
  InstructionOperand dst = g.DefineAsRegister(node);
  if (all_zeros) {
    Emit(kRiscvS128Zero, dst);
  } else if (all_ones) {
    Emit(kRiscvS128AllOnes, dst);
  } else {
    Emit(kRiscvS128Const, dst, g.UseImmediate(val[0]), g.UseImmediate(val[1]),
         g.UseImmediate(val[2]), g.UseImmediate(val[3]));
  }
}

void InstructionSelector::VisitS128Zero(OpIndex node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvS128Zero, g.DefineAsRegister(node));
}

#define SIMD_VISIT_EXTRACT_LANE(Type, Sign)                                \
                                                                           \
  void InstructionSelector::Visit##Type##ExtractLane##Sign(OpIndex node) { \
    VisitRRI(this, kRiscv##Type##ExtractLane##Sign, node);                 \
  }
SIMD_VISIT_EXTRACT_LANE(F64x2, )
SIMD_VISIT_EXTRACT_LANE(F32x4, )
SIMD_VISIT_EXTRACT_LANE(I32x4, )
SIMD_VISIT_EXTRACT_LANE(I64x2, )
SIMD_VISIT_EXTRACT_LANE(I16x8, U)
SIMD_VISIT_EXTRACT_LANE(I16x8, S)
SIMD_VISIT_EXTRACT_LANE(I8x16, U)
SIMD_VISIT_EXTRACT_LANE(I8x16, S)
#undef SIMD_VISIT_EXTRACT_LANE

#define SIMD_VISIT_REPLACE_LANE(Type)                                \
                                                                     \
  void InstructionSelector::Visit##Type##ReplaceLane(OpIndex node) { \
    VisitRRIR(this, kRiscv##Type##ReplaceLane, node);                \
  }
SIMD_TYPE_LIST(SIMD_VISIT_REPLACE_LANE)
SIMD_VISIT_REPLACE_LANE(F64x2)
#undef SIMD_VISIT_REPLACE_LANE

#define SIMD_VISIT_UNOP(Name, instruction)              \
                                                        \
  void InstructionSelector::Visit##Name(OpIndex node) { \
    VisitRR(this, instruction, node);                   \
  }
SIMD_UNOP_LIST(SIMD_VISIT_UNOP)
#undef SIMD_VISIT_UNOP

#define SIMD_VISIT_SHIFT_OP(Name)                       \
                                                        \
  void InstructionSelector::Visit##Name(OpIndex node) { \
    VisitSimdShift(this, kRiscv##Name, node);           \
  }
SIMD_SHIFT_OP_LIST(SIMD_VISIT_SHIFT_OP)
#undef SIMD_VISIT_SHIFT_OP

#define SIMD_VISIT_BINOP_RVV(Name, instruction, VSEW)                  \
                                                                       \
  void InstructionSelector::Visit##Name(OpIndex node) {                \
    RiscvOperandGenerator g(this);                                     \
    const Operation& op = this->Get(node);                             \
    DCHECK_EQ(op.input_count, 2);                                      \
    InstructionCode opcode = instruction | EncodeElementWidth(VSEW);   \
    Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)), \
         g.UseRegister(op.input(1)));                                  \
  }
SIMD_BINOP_LIST(SIMD_VISIT_BINOP_RVV)
#undef SIMD_VISIT_BINOP_RVV

#define SIMD_VISIT_UNOP2(Name, instruction, VSEW)                       \
                                                                        \
  void InstructionSelector::Visit##Name(OpIndex node) {                 \
    RiscvOperandGenerator g(this);                                      \
    const Operation& op = this->Get(node);                              \
    DCHECK_EQ(op.input_count, 1);                                       \
    InstructionCode opcode = instruction | EncodeElementWidth(VSEW);    \
    Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0))); \
  }
SIMD_UNOP_LIST2(SIMD_VISIT_UNOP2)
#undef SIMD_VISIT_UNOP2

void InstructionSelector::VisitS128Select(OpIndex node) {
  VisitRRRR(this, kRiscvS128Select, node);
}

#define SIMD_VISIT_SELECT_LANE(Name)                    \
                                                        \
  void InstructionSelector::Visit##Name(OpIndex node) { \
    VisitRRRR(this, kRiscvS128Select, node);            \
  }
SIMD_VISIT_SELECT_LANE(I8x16RelaxedLaneSelect)
SIMD_VISIT_SELECT_LANE(I16x8RelaxedLaneSelect)
SIMD_VISIT_SELECT_LANE(I32x4RelaxedLaneSelect)
SIMD_VISIT_SELECT_LANE(I64x2RelaxedLaneSelect)
#undef SIMD_VISIT_SELECT_LANE

#define VISIT_SIMD_QFMOP(Name, instruction)             \
                                                        \
  void InstructionSelector::Visit##Name(OpIndex node) { \
    VisitRRRR(this, instruction, node);                 \
  }
VISIT_SIMD_QFMOP(F64x2Qfma, kRiscvF64x2Qfma)
VISIT_SIMD_QFMOP(F64x2Qfms, kRiscvF64x2Qfms)
VISIT_SIMD_QFMOP(F32x4Qfma, kRiscvF32x4Qfma)
VISIT_SIMD_QFMOP(F32x4Qfms, kRiscvF32x4Qfms)
#undef VISIT_SIMD_QFMOP

void InstructionSelector::VisitF32x4Min(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvFMin | EncodeElementWidth(E32);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
       g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitF32x4Max(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvFMax | EncodeElementWidth(E32);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
       g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitF64x2Min(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvFMin | EncodeElementWidth(E64);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
       g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitF64x2Max(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvFMax | EncodeElementWidth(E64);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
       g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitF32x4RelaxedMin(OpIndex node) {
  VisitF32x4Min(node);
}

void InstructionSelector::VisitF64x2RelaxedMin(OpIndex node) {
  VisitF64x2Min(node);
}

void InstructionSelector::VisitF64x2RelaxedMax(OpIndex node) {
  VisitF64x2Max(node);
}

void InstructionSelector::VisitF32x4RelaxedMax(OpIndex node) {
  VisitF32x4Max(node);
}

void InstructionSelector::VisitF64x2Eq(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvFEq | EncodeElementWidth(E64);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
       g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitF64x2Ne(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvFNe | EncodeElementWidth(E64);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
       g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitF64x2Lt(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvFLt | EncodeElementWidth(E64);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
       g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitF64x2Le(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvFLe | EncodeElementWidth(E64);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
       g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitF32x4Eq(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvFEq | EncodeElementWidth(E32);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
       g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitF32x4Ne(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvFNe | EncodeElementWidth(E32);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
       g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitF32x4Lt(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvFLt | EncodeElementWidth(E32);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
       g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitF32x4Le(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvFLe | EncodeElementWidth(E32);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
       g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitI32x4SConvertI16x8Low(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvI32x4SConvertI16x8Low, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitI32x4UConvertI16x8Low(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvI32x4UConvertI16x8Low, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitI16x8SConvertI8x16High(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvI16x8SConvertI8x16High, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitI16x8SConvertI32x4(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvI16x8SConvertI32x4;
  InstructionOperand input0;
  InstructionOperand input1;
  if (CpuFeatures::vlen() > 128) {
    // No need for register groups, as one register is big enough to hold
    // 256 bits.
    input0 = g.UseRegister(op.input(0));
    input1 = g.UseRegister(op.input(1));
  } else {
    // Request a register group (two adjacent registers starting at an even
    // index). There is nothing special about the registers, as long as they
    // are adjacent and start at an even index.
    // Fixed registers also ensure that the inputs don't overlap with the
    // output.
    input0 = g.UseFixed(op.input(0), v28);
    input1 = g.UseFixed(op.input(1), v29);
    opcode |= EncodeRegisterConstraint(
        RiscvRegisterConstraint::kRegisterGroupNoOverlap);
  }
  Emit(opcode, g.DefineAsRegister(node), input0, input1);
}

void InstructionSelector::VisitI16x8UConvertI32x4(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvI16x8UConvertI32x4;
  InstructionOperand input0;
  InstructionOperand input1;
  if (CpuFeatures::vlen() > 128) {
    // No need for register groups, as one register is big enough to hold
    // 256 bits.
    input0 = g.UseRegister(op.input(0));
    input1 = g.UseRegister(op.input(1));
  } else {
    // Request a register group (two adjacent registers starting at an even
    // index). There is nothing special about the registers, as long as they
    // are adjacent and start at an even index.
    // Fixed registers also ensure that the inputs don't overlap with the
    // output.
    input0 = g.UseFixed(op.input(0), v28);
    input1 = g.UseFixed(op.input(1), v29);
    opcode |= EncodeRegisterConstraint(
        RiscvRegisterConstraint::kRegisterGroupNoOverlap);
  }
  Emit(opcode, g.DefineAsRegister(node), input0, input1);
}

void InstructionSelector::VisitI8x16RoundingAverageU(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  Emit(kRiscvI8x16RoundingAverageU, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)), g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitI8x16SConvertI16x8(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvI8x16SConvertI16x8;
  InstructionOperand input0;
  InstructionOperand input1;
  if (CpuFeatures::vlen() > 128) {
    // No need for register groups, as one register is big enough to hold
    // 256 bits.
    input0 = g.UseRegister(op.input(0));
    input1 = g.UseRegister(op.input(1));
  } else {
    // Request a register group (two adjacent registers starting at an even
    // index). There is nothing special about the registers, as long as they
    // are adjacent and start at an even index.
    // Fixed registers also ensure that the inputs don't overlap with the
    // output.
    input0 = g.UseFixed(op.input(0), v28);
    input1 = g.UseFixed(op.input(1), v29);
    opcode |= EncodeRegisterConstraint(
        RiscvRegisterConstraint::kRegisterGroupNoOverlap);
  }
  Emit(opcode, g.DefineAsRegister(node), input0, input1);
}

void InstructionSelector::VisitI8x16UConvertI16x8(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvI8x16UConvertI16x8;
  InstructionOperand input0;
  InstructionOperand input1;
  if (CpuFeatures::vlen() > 128) {
    // No need for register groups, as one register is big enough to hold
    // 256 bits.
    input0 = g.UseRegister(op.input(0));
    input1 = g.UseRegister(op.input(1));
  } else {
    // Request a register group (two adjacent registers starting at an even
    // index). There is nothing special about the registers, as long as they
    // are adjacent and start at an even index.
    // Fixed registers also ensure that the inputs don't overlap with the
    // output.
    input0 = g.UseFixed(op.input(0), v28);
    input1 = g.UseFixed(op.input(1), v29);
    opcode |= EncodeRegisterConstraint(
        RiscvRegisterConstraint::kRegisterGroupNoOverlap);
  }
  Emit(opcode, g.DefineAsRegister(node), input0, input1);
}

void InstructionSelector::VisitI16x8RoundingAverageU(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  Emit(kRiscvI16x8RoundingAverageU, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)), g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitI32x4DotI16x8S(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  Emit(kRiscvI32x4DotI16x8S, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)), g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitI16x8DotI8x16I7x16S(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  Emit(kRiscvI16x8DotI8x16I7x16S, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)), g.UseRegister(op.input(1)));
}

void InstructionSelector::VisitI32x4DotI8x16I7x16AddS(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 3);
  InstructionCode opcode = kRiscvI32x4DotI8x16I7x16AddS;

  // Any even allocatable register can be used as input.
  InstructionOperand temps[] = {g.TempSimd128Register()};
  auto input0 = g.UseRegister(op.input(0));
  auto input1 = g.UseRegister(op.input(1));
  auto input2 = g.UseUniqueRegister(op.input(2));
  opcode |= EncodeRegisterConstraint(RiscvRegisterConstraint::kNoInput2Overlap);
  Emit(opcode, g.DefineAsRegister(node), input0, input1, input2,
       arraysize(temps), temps);
}

void InstructionSelector::VisitI8x16Shuffle(OpIndex node) {
  uint8_t shuffle[kSimd128Size];
  bool is_swizzle;
  // TODO(riscv): Properly use view here once Turboshaft support is
  // implemented.
  auto view = this->simd_shuffle_view(node);
  CanonicalizeShuffle(view, shuffle, &is_swizzle);
  OpIndex input0 = view.input(0);
  OpIndex input1 = view.input(1);
  RiscvOperandGenerator g(this);
  // uint8_t shuffle32x4[4];
  // ArchOpcode opcode;
  // if (TryMatchArchShuffle(shuffle, arch_shuffles, arraysize(arch_shuffles),
  //                         is_swizzle, &opcode)) {
  //   VisitRRR(this, opcode, node);
  //   return;
  // }
  // uint8_t offset;
  // if (wasm::SimdShuffle::TryMatchConcat(shuffle, &offset)) {
  //   Emit(kRiscvS8x16Concat, g.DefineSameAsFirst(node),
  //   g.UseRegister(input1),
  //        g.UseRegister(input0), g.UseImmediate(offset));
  //   return;
  // }
  // if (wasm::SimdShuffle::TryMatch32x4Shuffle(shuffle, shuffle32x4)) {
  //   Emit(kRiscvS32x4Shuffle, g.DefineAsRegister(node),
  //   g.UseRegister(input0),
  //        g.UseRegister(input1),
  //        g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle32x4)));
  //   return;
  // }
  InstructionCode opcode = kRiscvI8x16Shuffle;
  auto input0_reg = g.UseUniqueRegister(input0);
  auto input1_reg = g.UseUniqueRegister(input1);
  opcode |= EncodeRegisterConstraint(
      RiscvRegisterConstraint::kNoDestinationSourceOverlap);
  Emit(opcode, g.DefineAsRegister(node), input0_reg, input1_reg,
       g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle)),
       g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle + 4)),
       g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle + 8)),
       g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle + 12)));
}

void InstructionSelector::VisitI8x16Swizzle(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionCode opcode = kRiscvVrgather | EncodeElementWidth(E8);
  auto input0 = g.UseUniqueRegister(op.input(0));
  auto input1 = g.UseUniqueRegister(op.input(1));
  opcode |= EncodeRegisterConstraint(
      RiscvRegisterConstraint::kNoDestinationSourceOverlap);
  Emit(opcode, g.DefineAsRegister(node), input0, input1);
}

#define VISIT_BITMASK(TYPE, VSEW)                                       \
                                                                        \
  void InstructionSelector::Visit##TYPE##BitMask(OpIndex node) {        \
    RiscvOperandGenerator g(this);                                      \
    const Operation& op = this->Get(node);                              \
    DCHECK_EQ(op.input_count, 1);                                       \
    InstructionCode opcode = kRiscvBitMask | EncodeElementWidth(VSEW);  \
    Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0))); \
  }

SIMD_INT_TYPE_LIST(VISIT_BITMASK)
#undef VISIT_BIMASK

void InstructionSelector::VisitI32x4SConvertI16x8High(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvI32x4SConvertI16x8High, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitI32x4UConvertI16x8High(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvI32x4UConvertI16x8High, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitI16x8SConvertI8x16Low(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvI16x8SConvertI8x16Low, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitI16x8UConvertI8x16High(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvI16x8UConvertI8x16High, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitI16x8UConvertI8x16Low(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvI16x8UConvertI8x16Low, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitSignExtendWord8ToInt32(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvSignExtendByte, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitSignExtendWord16ToInt32(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvSignExtendShort, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitWord32Clz(OpIndex node) {
  VisitRR(this, kRiscvClz32, node);
}

#define VISIT_EXT_MUL(OPCODE1, OPCODE2, TYPE)                                 \
                                                                              \
  void InstructionSelector::Visit##OPCODE1##ExtMulLow##OPCODE2##S(            \
      OpIndex node) {                                                         \
    RiscvOperandGenerator g(this);                                            \
    const Operation& op = this->Get(node);                                    \
    DCHECK_EQ(op.input_count, 2);                                             \
    InstructionCode opcode = kRiscvExtMulLowS | EncodeElementWidth(E##TYPE);  \
    auto input0 = g.UseUniqueRegister(op.input(0));                           \
    auto input1 = g.UseUniqueRegister(op.input(1));                           \
    opcode |= EncodeRegisterConstraint(                                       \
        RiscvRegisterConstraint::kNoDestinationSourceOverlap);                \
    Emit(opcode, g.DefineAsRegister(node), input0, input1);                   \
  }                                                                           \
                                                                              \
  void InstructionSelector::Visit##OPCODE1##ExtMulHigh##OPCODE2##S(           \
      OpIndex node) {                                                         \
    RiscvOperandGenerator g(this);                                            \
    const Operation& op = this->Get(node);                                    \
    DCHECK_EQ(op.input_count, 2);                                             \
    InstructionCode opcode = kRiscvExtMulHighS | EncodeElementWidth(E##TYPE); \
    Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),        \
         g.UseRegister(op.input(1)));                                         \
  }                                                                           \
                                                                              \
  void InstructionSelector::Visit##OPCODE1##ExtMulLow##OPCODE2##U(            \
      OpIndex node) {                                                         \
    RiscvOperandGenerator g(this);                                            \
    const Operation& op = this->Get(node);                                    \
    DCHECK_EQ(op.input_count, 2);                                             \
    InstructionCode opcode = kRiscvExtMulLowU | EncodeElementWidth(E##TYPE);  \
    auto input0 = g.UseUniqueRegister(op.input(0));                           \
    auto input1 = g.UseUniqueRegister(op.input(1));                           \
    opcode |= EncodeRegisterConstraint(                                       \
        RiscvRegisterConstraint::kNoDestinationSourceOverlap);                \
    Emit(opcode, g.DefineAsRegister(node), input0, input1);                   \
  }                                                                           \
                                                                              \
  void InstructionSelector::Visit##OPCODE1##ExtMulHigh##OPCODE2##U(           \
      OpIndex node) {                                                         \
    RiscvOperandGenerator g(this);                                            \
    const Operation& op = this->Get(node);                                    \
    DCHECK_EQ(op.input_count, 2);                                             \
    InstructionCode opcode = kRiscvExtMulHighU | EncodeElementWidth(E##TYPE); \
    Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),        \
         g.UseRegister(op.input(1)));                                         \
  }

VISIT_EXT_MUL(I64x2, I32x4, 32)
VISIT_EXT_MUL(I32x4, I16x8, 16)
VISIT_EXT_MUL(I16x8, I8x16, 8)
#undef VISIT_EXT_MUL

void InstructionSelector::VisitF32x4Pmin(OpIndex node) {
  VisitRRR(this, kRiscvF32x4Pmin, node);
}

void InstructionSelector::VisitF32x4Pmax(OpIndex node) {
  VisitRRR(this, kRiscvF32x4Pmax, node);
}

void InstructionSelector::VisitF64x2Pmin(OpIndex node) {
  VisitRRR(this, kRiscvF64x2Pmin, node);
}

void InstructionSelector::VisitF64x2Pmax(OpIndex node) {
  VisitRRR(this, kRiscvF64x2Pmax, node);
}

void InstructionSelector::VisitTruncateFloat64ToFloat16RawBits(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitChangeFloat16RawBitsToFloat64(OpIndex node) {
  UNIMPLEMENTED();
}

// static
MachineOperatorBuilder::AlignmentRequirements
InstructionSelector::AlignmentRequirements() {
#ifdef RISCV_HAS_NO_UNALIGNED
  return MachineOperatorBuilder::AlignmentRequirements::
      NoUnalignedAccessSupport();
#else
  return MachineOperatorBuilder::AlignmentRequirements::
      FullUnalignedAccessSupport();
#endif
}

void InstructionSelector::AddOutputToSelectContinuation(OperandGenerator* g,
                                                        int first_input_index,
                                                        OpIndex node) {
  UNREACHABLE();
}

#if V8_ENABLE_WEBASSEMBLY

void InstructionSelector::VisitSetStackPointer(OpIndex node) {
  OperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  auto input = g.UseRegister(op.input(0));
  Emit(kArchSetStackPointer, 0, nullptr, 1, &input);
}
#endif

#undef SIMD_BINOP_LIST
#undef SIMD_SHIFT_OP_LIST
#undef SIMD_UNOP_LIST
#undef SIMD_UNOP_LIST2
#undef SIMD_TYPE_LIST
#undef SIMD_INT_TYPE_LIST
#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_RISCV_INSTRUCTION_SELECTOR_RISCV_H_
