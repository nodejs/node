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

// Adds RISC-V-specific methods for generating InstructionOperands.

class RiscvOperandGeneratorT final : public OperandGeneratorT {
 public:
  explicit RiscvOperandGeneratorT(InstructionSelectorT* selector)
      : OperandGeneratorT(selector) {}

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

void VisitRR(InstructionSelectorT* selector, ArchOpcode opcode, OpIndex node) {
  RiscvOperandGeneratorT g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(selector->input_at(node, 0)));
}

void VisitRR(InstructionSelectorT* selector, InstructionCode opcode,
             OpIndex node) {
  RiscvOperandGeneratorT g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(selector->input_at(node, 0)));
}

static void VisitRRI(InstructionSelectorT* selector, ArchOpcode opcode,
                     OpIndex node) {
  RiscvOperandGeneratorT g(selector);
  const Operation& op = selector->Get(node);
  int imm = op.template Cast<Simd128ExtractLaneOp>().lane;
  selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
                 g.UseImmediate(imm));
}

static void VisitSimdShift(InstructionSelectorT* selector, ArchOpcode opcode,
                           OpIndex node) {
  RiscvOperandGeneratorT g(selector);
  OpIndex rhs = selector->input_at(node, 1);
  if (selector->Get(rhs).TryCast<ConstantOp>()) {
    selector->Emit(opcode, g.DefineAsRegister(node),
                   g.UseRegister(selector->input_at(node, 0)),
                   g.UseImmediate(selector->input_at(node, 1)));
  } else {
    selector->Emit(opcode, g.DefineAsRegister(node),
                   g.UseRegister(selector->input_at(node, 0)),
                   g.UseRegister(selector->input_at(node, 1)));
  }
}

static void VisitRRIR(InstructionSelectorT* selector, ArchOpcode opcode,
                      OpIndex node) {
  RiscvOperandGeneratorT g(selector);
  const turboshaft::Simd128ReplaceLaneOp& op =
      selector->Get(node).template Cast<turboshaft::Simd128ReplaceLaneOp>();
  selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
                 g.UseImmediate(op.lane), g.UseUniqueRegister(op.input(1)));
}

void VisitRRR(InstructionSelectorT* selector, InstructionCode opcode,
              OpIndex node,
              typename OperandGeneratorT::RegisterUseKind kind =
                  OperandGeneratorT::RegisterUseKind::kUseRegister) {
  RiscvOperandGeneratorT g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(selector->input_at(node, 0)),
                 g.UseRegister(selector->input_at(node, 1), kind));
}

static void VisitUniqueRRR(InstructionSelectorT* selector, ArchOpcode opcode,
                           OpIndex node) {
  RiscvOperandGeneratorT g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseUniqueRegister(selector->input_at(node, 0)),
                 g.UseUniqueRegister(selector->input_at(node, 1)));
}

void VisitRRRR(InstructionSelectorT* selector, ArchOpcode opcode,
               OpIndex node) {
  RiscvOperandGeneratorT g(selector);
  selector->Emit(opcode, g.DefineSameAsFirst(node),
                 g.UseRegister(selector->input_at(node, 0)),
                 g.UseRegister(selector->input_at(node, 1)),
                 g.UseRegister(selector->input_at(node, 2)));
}

static void VisitRRO(InstructionSelectorT* selector, ArchOpcode opcode,
                     OpIndex node) {
  RiscvOperandGeneratorT g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(selector->input_at(node, 0)),
                 g.UseOperand(selector->input_at(node, 1), opcode));
}

bool TryMatchImmediate(InstructionSelectorT* selector,
                       InstructionCode* opcode_return, OpIndex node,
                       size_t* input_count_return, InstructionOperand* inputs) {
  RiscvOperandGeneratorT g(selector);
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
static void VisitBinop(InstructionSelectorT* selector, OpIndex node,
                       InstructionCode opcode, bool has_reverse_opcode,
                       InstructionCode reverse_opcode,
                       FlagsContinuationT* cont) {
  RiscvOperandGeneratorT g(selector);
  InstructionOperand inputs[2];
  size_t input_count = 0;
  InstructionOperand outputs[1];
  size_t output_count = 0;

  const Operation& binop = selector->Get(node);
  OpIndex left_node = binop.input(0);
  OpIndex right_node = binop.input(1);

  if (TryMatchImmediate(selector, &opcode, right_node, &input_count,
                        &inputs[1])) {
    inputs[0] = g.UseRegisterOrImmediateZero(left_node);
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

  if (cont->IsDeoptimize()) {
    // If we can deoptimize as a result of the binop, we need to make sure that
    // the deopt inputs are not overwritten by the binop result. One way
    // to achieve that is to declare the output register as same-as-first.
    outputs[output_count++] = g.DefineSameAsFirst(node);
  } else {
    outputs[output_count++] = g.DefineAsRegister(node);
  }

  DCHECK_NE(0u, input_count);
  DCHECK_EQ(1u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}

template <typename Matcher>
static void VisitBinop(InstructionSelectorT* selector, OpIndex node,
                       InstructionCode opcode, bool has_reverse_opcode,
                       InstructionCode reverse_opcode) {
  FlagsContinuationT cont;
  VisitBinop<Matcher>(selector, node, opcode, has_reverse_opcode,
                      reverse_opcode, &cont);
}

template <typename Matcher>
static void VisitBinop(InstructionSelectorT* selector, OpIndex node,
                       InstructionCode opcode, FlagsContinuationT* cont) {
  VisitBinop<Matcher>(selector, node, opcode, false, kArchNop, cont);
}

template <typename Matcher>
static void VisitBinop(InstructionSelectorT* selector, OpIndex node,
                       InstructionCode opcode) {
  VisitBinop<Matcher>(selector, node, opcode, false, kArchNop);
}

void InstructionSelectorT::VisitStackSlot(OpIndex node) {
  const StackSlotOp& stack_slot = Cast<StackSlotOp>(node);
  int slot = frame_->AllocateSpillSlot(stack_slot.size, stack_slot.alignment,
                                       stack_slot.is_tagged);
  OperandGenerator g(this);

  Emit(kArchStackSlot, g.DefineAsRegister(node),
       sequence()->AddImmediate(Constant(slot)), 0, nullptr);
}

void InstructionSelectorT::VisitAbortCSADcheck(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  Emit(kArchAbortCSADcheck, g.NoOutput(),
       g.UseFixed(this->input_at(node, 0), a0));
}

void EmitS128Load(InstructionSelectorT* selector, OpIndex node,
                  InstructionCode opcode, VSew sew, Vlmul lmul);

void InstructionSelectorT::VisitLoadTransform(OpIndex node) {
  const Simd128LoadTransformOp& op =
      this->Get(node).Cast<Simd128LoadTransformOp>();
  bool is_protected = (op.load_kind.with_trap_handler);
  InstructionCode opcode = kArchNop;
  switch (op.transform_kind) {
    case Simd128LoadTransformOp::TransformKind::k8Splat:
      opcode = kRiscvS128LoadSplat;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode, E8, m1);
      break;
    case Simd128LoadTransformOp::TransformKind::k16Splat:
      opcode = kRiscvS128LoadSplat;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode, E16, m1);
      break;
    case Simd128LoadTransformOp::TransformKind::k32Splat:
      opcode = kRiscvS128LoadSplat;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode, E32, m1);
      break;
    case Simd128LoadTransformOp::TransformKind::k64Splat:
      opcode = kRiscvS128LoadSplat;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode, E64, m1);
      break;
    case Simd128LoadTransformOp::TransformKind::k8x8S:
      opcode = kRiscvS128Load64ExtendS;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode, E16, m1);
      break;
    case Simd128LoadTransformOp::TransformKind::k8x8U:
      opcode = kRiscvS128Load64ExtendU;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode, E16, m1);
      break;
    case Simd128LoadTransformOp::TransformKind::k16x4S:
      opcode = kRiscvS128Load64ExtendS;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode, E32, m1);
      break;
    case Simd128LoadTransformOp::TransformKind::k16x4U:
      opcode = kRiscvS128Load64ExtendU;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode, E32, m1);
      break;
    case Simd128LoadTransformOp::TransformKind::k32x2S:
      opcode = kRiscvS128Load64ExtendS;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode, E64, m1);
      break;
    case Simd128LoadTransformOp::TransformKind::k32x2U:
      opcode = kRiscvS128Load64ExtendU;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode, E64, m1);
      break;
    case Simd128LoadTransformOp::TransformKind::k32Zero:
      opcode = kRiscvS128Load32Zero;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode, E32, m1);
      break;
    case Simd128LoadTransformOp::TransformKind::k64Zero:
      opcode = kRiscvS128Load64Zero;
      if (is_protected) {
        opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
      }
      EmitS128Load(this, node, opcode, E64, m1);
      break;
    default:
      UNIMPLEMENTED();
  }
}

// Shared routine for multiple compare operations.

static Instruction* VisitCompare(InstructionSelectorT* selector,
                                 InstructionCode opcode,
                                 InstructionOperand left,
                                 InstructionOperand right,
                                 FlagsContinuationT* cont) {
#ifdef V8_COMPRESS_POINTERS
  if (opcode == kRiscvCmp32) {
    RiscvOperandGeneratorT g(selector);
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

static Instruction* VisitWordCompareZero(InstructionSelectorT* selector,
                                         InstructionOperand value,
                                         FlagsContinuationT* cont) {
  return selector->EmitWithContinuation(kRiscvCmpZero, value, cont);
}

// Shared routine for multiple float32 compare operations.

void VisitFloat32Compare(InstructionSelectorT* selector, OpIndex node,
                         FlagsContinuationT* cont) {
  RiscvOperandGeneratorT g(selector);
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

void VisitFloat64Compare(InstructionSelectorT* selector, OpIndex node,
                         FlagsContinuationT* cont) {
  RiscvOperandGeneratorT g(selector);
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

Instruction* VisitWordCompare(InstructionSelectorT* selector, OpIndex node,
                              InstructionCode opcode, FlagsContinuationT* cont,
                              bool commutative) {
  RiscvOperandGeneratorT g(selector);
  DCHECK_EQ(selector->value_input_count(node), 2);
  auto left = selector->input_at(node, 0);
  auto right = selector->input_at(node, 1);
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

void InstructionSelectorT::VisitSwitch(OpIndex node, const SwitchInfo& sw) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand value_operand = g.UseRegister(this->input_at(node, 0));

  // Emit either ArchTableSwitch or ArchBinarySearchSwitch.
  if (enable_switch_jump_table_ ==
      InstructionSelector::kEnableSwitchJumpTable) {
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

void EmitWordCompareZero(InstructionSelectorT* selector, OpIndex value,
                         FlagsContinuationT* cont) {
  RiscvOperandGeneratorT g(selector);
  selector->EmitWithContinuation(kRiscvCmpZero,
                                 g.UseRegisterOrImmediateZero(value), cont);
}

#ifdef V8_TARGET_ARCH_RISCV64

void EmitWord32CompareZero(InstructionSelectorT* selector, OpIndex value,
                           FlagsContinuationT* cont) {
  RiscvOperandGeneratorT g(selector);
  InstructionOperand inputs[] = {g.UseRegisterOrImmediateZero(value)};
  InstructionOperand temps[] = {g.TempRegister()};
  selector->EmitWithContinuation(kRiscvCmpZero32, 0, nullptr, arraysize(inputs),
                                 inputs, arraysize(temps), temps, cont);
}
#endif

void InstructionSelectorT::VisitFloat32Equal(OpIndex node) {
  FlagsContinuationT cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelectorT::VisitFloat32LessThan(OpIndex node) {
  FlagsContinuationT cont = FlagsContinuation::ForSet(kFloatLessThan, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelectorT::VisitFloat32LessThanOrEqual(OpIndex node) {
  FlagsContinuationT cont =
      FlagsContinuation::ForSet(kFloatLessThanOrEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelectorT::VisitFloat64Equal(OpIndex node) {
  FlagsContinuationT cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelectorT::VisitFloat64LessThan(OpIndex node) {
  FlagsContinuationT cont = FlagsContinuation::ForSet(kFloatLessThan, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelectorT::VisitFloat64LessThanOrEqual(OpIndex node) {
  FlagsContinuationT cont =
      FlagsContinuation::ForSet(kFloatLessThanOrEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelectorT::VisitFloat64ExtractLowWord32(OpIndex node) {
  VisitRR(this, kRiscvFloat64ExtractLowWord32, node);
}

void InstructionSelectorT::VisitFloat64ExtractHighWord32(OpIndex node) {
  VisitRR(this, kRiscvFloat64ExtractHighWord32, node);
}

void InstructionSelectorT::VisitFloat64SilenceNaN(OpIndex node) {
  VisitRR(this, kRiscvFloat64SilenceNaN, node);
}

void InstructionSelectorT::VisitBitcastWord32PairToFloat64(OpIndex node) {
  RiscvOperandGeneratorT g(this);
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

void InstructionSelectorT::VisitFloat64InsertLowWord32(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  OpIndex left = this->input_at(node, 0);
  OpIndex right = this->input_at(node, 1);
  Emit(kRiscvFloat64InsertLowWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.UseRegister(right));
}

void InstructionSelectorT::VisitFloat64InsertHighWord32(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  OpIndex left = this->input_at(node, 0);
  OpIndex right = this->input_at(node, 1);
  Emit(kRiscvFloat64InsertHighWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.UseRegister(right));
}

void InstructionSelectorT::VisitMemoryBarrier(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  Emit(kRiscvSync, g.NoOutput());
}

bool InstructionSelectorT::IsTailCallAddressImmediate() { return false; }

void InstructionSelectorT::EmitPrepareResults(
    ZoneVector<PushParameter>* results, const CallDescriptor* call_descriptor,
    OpIndex node) {
  RiscvOperandGeneratorT g(this);

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

void InstructionSelectorT::EmitMoveParamToFPR(OpIndex node, int index) {}

void InstructionSelectorT::EmitMoveFPRToParam(InstructionOperand* op,
                                              LinkageLocation location) {}

void InstructionSelectorT::VisitFloat32Abs(OpIndex node) {
  VisitRR(this, kRiscvAbsS, node);
}

void InstructionSelectorT::VisitFloat64Abs(OpIndex node) {
  VisitRR(this, kRiscvAbsD, node);
}

void InstructionSelectorT::VisitFloat32Sqrt(OpIndex node) {
  VisitRR(this, kRiscvSqrtS, node);
}

void InstructionSelectorT::VisitFloat64Sqrt(OpIndex node) {
  VisitRR(this, kRiscvSqrtD, node);
}

void InstructionSelectorT::VisitFloat32RoundDown(OpIndex node) {
  VisitRR(this, kRiscvFloat32RoundDown, node);
}

void InstructionSelectorT::VisitFloat32Add(OpIndex node) {
  VisitRRR(this, kRiscvAddS, node);
}

void InstructionSelectorT::VisitFloat64Add(OpIndex node) {
  VisitRRR(this, kRiscvAddD, node);
}

void InstructionSelectorT::VisitFloat32Sub(OpIndex node) {
  VisitRRR(this, kRiscvSubS, node);
}

void InstructionSelectorT::VisitFloat64Sub(OpIndex node) {
  VisitRRR(this, kRiscvSubD, node);
}

void InstructionSelectorT::VisitFloat32Mul(OpIndex node) {
  VisitRRR(this, kRiscvMulS, node);
}

void InstructionSelectorT::VisitFloat64Mul(OpIndex node) {
  VisitRRR(this, kRiscvMulD, node);
}

void InstructionSelectorT::VisitFloat32Div(OpIndex node) {
  VisitRRR(this, kRiscvDivS, node);
}

void InstructionSelectorT::VisitFloat64Div(OpIndex node) {
  VisitRRR(this, kRiscvDivD, node);
}

void InstructionSelectorT::VisitFloat64Mod(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  Emit(kRiscvModD, g.DefineAsFixed(node, fa0),
       g.UseFixed(this->input_at(node, 0), fa0),
       g.UseFixed(this->input_at(node, 1), fa1))
      ->MarkAsCall();
}

void InstructionSelectorT::VisitFloat32Max(OpIndex node) {
  VisitRRR(this, kRiscvFloat32Max, node);
}

void InstructionSelectorT::VisitFloat64Max(OpIndex node) {
  VisitRRR(this, kRiscvFloat64Max, node);
}

void InstructionSelectorT::VisitFloat32Min(OpIndex node) {
  VisitRRR(this, kRiscvFloat32Min, node);
}

void InstructionSelectorT::VisitFloat64Min(OpIndex node) {
  VisitRRR(this, kRiscvFloat64Min, node);
}

void InstructionSelectorT::VisitTruncateFloat64ToWord32(OpIndex node) {
  VisitRR(this, kArchTruncateDoubleToI, node);
}

void InstructionSelectorT::VisitRoundFloat64ToInt32(OpIndex node) {
  VisitRR(this, kRiscvTruncWD, node);
}

void InstructionSelectorT::VisitTruncateFloat64ToFloat32(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  OpIndex value = this->input_at(node, 0);
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
               g.UseRegister(this->input_at(value, 0)));
          return;
        }
      }
    }
  }
  VisitRR(this, kRiscvCvtSD, node);
}

void InstructionSelectorT::VisitWord32Shl(OpIndex node) {
  // todo(RISCV): Optimize it
  VisitRRO(this, kRiscvShl32, node);
}

void InstructionSelectorT::VisitWord32Shr(OpIndex node) {
  VisitRRO(this, kRiscvShr32, node);
}

void InstructionSelectorT::VisitWord32Sar(OpIndex node) {
  // todo(RISCV): Optimize it
  VisitRRO(this, kRiscvSar32, node);
}

void InstructionSelectorT::VisitI32x4ExtAddPairwiseI16x8S(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand src1 = g.TempSimd128Register();
  InstructionOperand src2 = g.TempSimd128Register();
  InstructionOperand src = g.UseUniqueRegister(this->input_at(node, 0));
  Emit(kRiscvVrgather, src1, src, g.UseImmediate64(0x0006000400020000),
       g.UseImmediate(int8_t(E16)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVrgather, src2, src, g.UseImmediate64(0x0007000500030001),
       g.UseImmediate(int8_t(E16)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVwaddVv, g.DefineAsRegister(node), src1, src2,
       g.UseImmediate(int8_t(E16)), g.UseImmediate(int8_t(mf2)));
}

void InstructionSelectorT::VisitI32x4ExtAddPairwiseI16x8U(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand src1 = g.TempSimd128Register();
  InstructionOperand src2 = g.TempSimd128Register();
  InstructionOperand src = g.UseUniqueRegister(this->input_at(node, 0));
  Emit(kRiscvVrgather, src1, src, g.UseImmediate64(0x0006000400020000),
       g.UseImmediate(int8_t(E16)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVrgather, src2, src, g.UseImmediate64(0x0007000500030001),
       g.UseImmediate(int8_t(E16)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVwadduVv, g.DefineAsRegister(node), src1, src2,
       g.UseImmediate(int8_t(E16)), g.UseImmediate(int8_t(mf2)));
}

void InstructionSelectorT::VisitI16x8ExtAddPairwiseI8x16S(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand src1 = g.TempSimd128Register();
  InstructionOperand src2 = g.TempSimd128Register();
  InstructionOperand src = g.UseUniqueRegister(this->input_at(node, 0));
  Emit(kRiscvVrgather, src1, src, g.UseImmediate64(0x0E0C0A0806040200),
       g.UseImmediate(int8_t(E8)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVrgather, src2, src, g.UseImmediate64(0x0F0D0B0907050301),
       g.UseImmediate(int8_t(E8)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVwaddVv, g.DefineAsRegister(node), src1, src2,
       g.UseImmediate(int8_t(E8)), g.UseImmediate(int8_t(mf2)));
}

void InstructionSelectorT::VisitI16x8ExtAddPairwiseI8x16U(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand src1 = g.TempSimd128Register();
  InstructionOperand src2 = g.TempSimd128Register();
  InstructionOperand src = g.UseUniqueRegister(this->input_at(node, 0));
  Emit(kRiscvVrgather, src1, src, g.UseImmediate64(0x0E0C0A0806040200),
       g.UseImmediate(int8_t(E8)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVrgather, src2, src, g.UseImmediate64(0x0F0D0B0907050301),
       g.UseImmediate(int8_t(E8)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVwadduVv, g.DefineAsRegister(node), src1, src2,
       g.UseImmediate(int8_t(E8)), g.UseImmediate(int8_t(mf2)));
}

#define SIMD_INT_TYPE_LIST(V) \
  V(I64x2, E64, m1)           \
  V(I32x4, E32, m1)           \
  V(I16x8, E16, m1)           \
  V(I8x16, E8, m1)

#define SIMD_TYPE_LIST(V) \
  V(F32x4)                \
  V(I64x2)                \
  V(I32x4)                \
  V(I16x8)                \
  V(I8x16)

#define SIMD_UNOP_LIST2(V)                 \
  V(F32x4Splat, kRiscvVfmvVf, E32, m1)     \
  V(I8x16Neg, kRiscvVnegVv, E8, m1)        \
  V(I16x8Neg, kRiscvVnegVv, E16, m1)       \
  V(I32x4Neg, kRiscvVnegVv, E32, m1)       \
  V(I64x2Neg, kRiscvVnegVv, E64, m1)       \
  V(I8x16Splat, kRiscvVmv, E8, m1)         \
  V(I16x8Splat, kRiscvVmv, E16, m1)        \
  V(I32x4Splat, kRiscvVmv, E32, m1)        \
  V(I64x2Splat, kRiscvVmv, E64, m1)        \
  V(F32x4Neg, kRiscvVfnegVv, E32, m1)      \
  V(F64x2Neg, kRiscvVfnegVv, E64, m1)      \
  V(F64x2Splat, kRiscvVfmvVf, E64, m1)     \
  V(I32x4AllTrue, kRiscvVAllTrue, E32, m1) \
  V(I16x8AllTrue, kRiscvVAllTrue, E16, m1) \
  V(I8x16AllTrue, kRiscvVAllTrue, E8, m1)  \
  V(I64x2AllTrue, kRiscvVAllTrue, E64, m1) \
  V(I64x2Abs, kRiscvVAbs, E64, m1)         \
  V(I32x4Abs, kRiscvVAbs, E32, m1)         \
  V(I16x8Abs, kRiscvVAbs, E16, m1)         \
  V(I8x16Abs, kRiscvVAbs, E8, m1)

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

#define SIMD_BINOP_LIST(V)                    \
  V(I64x2Add, kRiscvVaddVv, E64, m1)          \
  V(I32x4Add, kRiscvVaddVv, E32, m1)          \
  V(I16x8Add, kRiscvVaddVv, E16, m1)          \
  V(I8x16Add, kRiscvVaddVv, E8, m1)           \
  V(I64x2Sub, kRiscvVsubVv, E64, m1)          \
  V(I32x4Sub, kRiscvVsubVv, E32, m1)          \
  V(I16x8Sub, kRiscvVsubVv, E16, m1)          \
  V(I8x16Sub, kRiscvVsubVv, E8, m1)           \
  V(I32x4MaxU, kRiscvVmaxuVv, E32, m1)        \
  V(I16x8MaxU, kRiscvVmaxuVv, E16, m1)        \
  V(I8x16MaxU, kRiscvVmaxuVv, E8, m1)         \
  V(I32x4MaxS, kRiscvVmax, E32, m1)           \
  V(I16x8MaxS, kRiscvVmax, E16, m1)           \
  V(I8x16MaxS, kRiscvVmax, E8, m1)            \
  V(I32x4MinS, kRiscvVminsVv, E32, m1)        \
  V(I16x8MinS, kRiscvVminsVv, E16, m1)        \
  V(I8x16MinS, kRiscvVminsVv, E8, m1)         \
  V(I32x4MinU, kRiscvVminuVv, E32, m1)        \
  V(I16x8MinU, kRiscvVminuVv, E16, m1)        \
  V(I8x16MinU, kRiscvVminuVv, E8, m1)         \
  V(I64x2Mul, kRiscvVmulVv, E64, m1)          \
  V(I32x4Mul, kRiscvVmulVv, E32, m1)          \
  V(I16x8Mul, kRiscvVmulVv, E16, m1)          \
  V(I64x2GtS, kRiscvVgtsVv, E64, m1)          \
  V(I32x4GtS, kRiscvVgtsVv, E32, m1)          \
  V(I16x8GtS, kRiscvVgtsVv, E16, m1)          \
  V(I8x16GtS, kRiscvVgtsVv, E8, m1)           \
  V(I64x2GeS, kRiscvVgesVv, E64, m1)          \
  V(I32x4GeS, kRiscvVgesVv, E32, m1)          \
  V(I16x8GeS, kRiscvVgesVv, E16, m1)          \
  V(I8x16GeS, kRiscvVgesVv, E8, m1)           \
  V(I32x4GeU, kRiscvVgeuVv, E32, m1)          \
  V(I16x8GeU, kRiscvVgeuVv, E16, m1)          \
  V(I8x16GeU, kRiscvVgeuVv, E8, m1)           \
  V(I32x4GtU, kRiscvVgtuVv, E32, m1)          \
  V(I16x8GtU, kRiscvVgtuVv, E16, m1)          \
  V(I8x16GtU, kRiscvVgtuVv, E8, m1)           \
  V(I64x2Eq, kRiscvVeqVv, E64, m1)            \
  V(I32x4Eq, kRiscvVeqVv, E32, m1)            \
  V(I16x8Eq, kRiscvVeqVv, E16, m1)            \
  V(I8x16Eq, kRiscvVeqVv, E8, m1)             \
  V(I64x2Ne, kRiscvVneVv, E64, m1)            \
  V(I32x4Ne, kRiscvVneVv, E32, m1)            \
  V(I16x8Ne, kRiscvVneVv, E16, m1)            \
  V(I8x16Ne, kRiscvVneVv, E8, m1)             \
  V(I16x8AddSatS, kRiscvVaddSatSVv, E16, m1)  \
  V(I8x16AddSatS, kRiscvVaddSatSVv, E8, m1)   \
  V(I16x8AddSatU, kRiscvVaddSatUVv, E16, m1)  \
  V(I8x16AddSatU, kRiscvVaddSatUVv, E8, m1)   \
  V(I16x8SubSatS, kRiscvVsubSatSVv, E16, m1)  \
  V(I8x16SubSatS, kRiscvVsubSatSVv, E8, m1)   \
  V(I16x8SubSatU, kRiscvVsubSatUVv, E16, m1)  \
  V(I8x16SubSatU, kRiscvVsubSatUVv, E8, m1)   \
  V(F64x2Add, kRiscvVfaddVv, E64, m1)         \
  V(F32x4Add, kRiscvVfaddVv, E32, m1)         \
  V(F64x2Sub, kRiscvVfsubVv, E64, m1)         \
  V(F32x4Sub, kRiscvVfsubVv, E32, m1)         \
  V(F64x2Mul, kRiscvVfmulVv, E64, m1)         \
  V(F32x4Mul, kRiscvVfmulVv, E32, m1)         \
  V(F64x2Div, kRiscvVfdivVv, E64, m1)         \
  V(F32x4Div, kRiscvVfdivVv, E32, m1)         \
  V(S128And, kRiscvVandVv, E8, m1)            \
  V(S128Or, kRiscvVorVv, E8, m1)              \
  V(S128Xor, kRiscvVxorVv, E8, m1)            \
  V(I16x8Q15MulRSatS, kRiscvVsmulVv, E16, m1) \
  V(I16x8RelaxedQ15MulRS, kRiscvVsmulVv, E16, m1)

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
  void InstructionSelectorT::Visit##Name(OpIndex node) { UNIMPLEMENTED(); }
UNIMPLEMENTED_SIMD_FP16_OP_LIST(SIMD_VISIT_UNIMPL_FP16_OP)
#undef SIMD_VISIT_UNIMPL_FP16_OP
#undef UNIMPLEMENTED_SIMD_FP16_OP_LIST

void InstructionSelectorT::VisitS128AndNot(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  this->Emit(kRiscvVnotVv, temp1, g.UseRegister(this->input_at(node, 1)),
             g.UseImmediate(E8), g.UseImmediate(m1));
  this->Emit(kRiscvVandVv, g.DefineAsRegister(node),
             g.UseRegister(this->input_at(node, 0)), temp1, g.UseImmediate(E8),
             g.UseImmediate(m1));
}

void InstructionSelectorT::VisitS128Const(OpIndex node) {
  RiscvOperandGeneratorT g(this);
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

void InstructionSelectorT::VisitS128Zero(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  Emit(kRiscvS128Zero, g.DefineAsRegister(node));
}

#define SIMD_VISIT_EXTRACT_LANE(Type, Sign)                                 \
                                                                            \
  void InstructionSelectorT::Visit##Type##ExtractLane##Sign(OpIndex node) { \
    VisitRRI(this, kRiscv##Type##ExtractLane##Sign, node);                  \
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

#define SIMD_VISIT_REPLACE_LANE(Type)                                 \
                                                                      \
  void InstructionSelectorT::Visit##Type##ReplaceLane(OpIndex node) { \
    VisitRRIR(this, kRiscv##Type##ReplaceLane, node);                 \
  }
SIMD_TYPE_LIST(SIMD_VISIT_REPLACE_LANE)
SIMD_VISIT_REPLACE_LANE(F64x2)
#undef SIMD_VISIT_REPLACE_LANE

#define SIMD_VISIT_UNOP(Name, instruction)               \
                                                         \
  void InstructionSelectorT::Visit##Name(OpIndex node) { \
    VisitRR(this, instruction, node);                    \
  }
SIMD_UNOP_LIST(SIMD_VISIT_UNOP)
#undef SIMD_VISIT_UNOP

#define SIMD_VISIT_SHIFT_OP(Name)                        \
                                                         \
  void InstructionSelectorT::Visit##Name(OpIndex node) { \
    VisitSimdShift(this, kRiscv##Name, node);            \
  }
SIMD_SHIFT_OP_LIST(SIMD_VISIT_SHIFT_OP)
#undef SIMD_VISIT_SHIFT_OP

#define SIMD_VISIT_BINOP_RVV(Name, instruction, VSEW, LMUL)                  \
                                                                             \
  void InstructionSelectorT::Visit##Name(OpIndex node) {                     \
    RiscvOperandGeneratorT g(this);                                          \
    this->Emit(instruction, g.DefineAsRegister(node),                        \
               g.UseRegister(this->input_at(node, 0)),                       \
               g.UseRegister(this->input_at(node, 1)), g.UseImmediate(VSEW), \
               g.UseImmediate(LMUL));                                        \
  }
SIMD_BINOP_LIST(SIMD_VISIT_BINOP_RVV)
#undef SIMD_VISIT_BINOP_RVV

#define SIMD_VISIT_UNOP2(Name, instruction, VSEW, LMUL)                      \
                                                                             \
  void InstructionSelectorT::Visit##Name(OpIndex node) {                     \
    RiscvOperandGeneratorT g(this);                                          \
    this->Emit(instruction, g.DefineAsRegister(node),                        \
               g.UseRegister(this->input_at(node, 0)), g.UseImmediate(VSEW), \
               g.UseImmediate(LMUL));                                        \
  }
SIMD_UNOP_LIST2(SIMD_VISIT_UNOP2)
#undef SIMD_VISIT_UNOP2

void InstructionSelectorT::VisitS128Select(OpIndex node) {
  VisitRRRR(this, kRiscvS128Select, node);
}

#define SIMD_VISIT_SELECT_LANE(Name)                     \
                                                         \
  void InstructionSelectorT::Visit##Name(OpIndex node) { \
    VisitRRRR(this, kRiscvS128Select, node);             \
  }
SIMD_VISIT_SELECT_LANE(I8x16RelaxedLaneSelect)
SIMD_VISIT_SELECT_LANE(I16x8RelaxedLaneSelect)
SIMD_VISIT_SELECT_LANE(I32x4RelaxedLaneSelect)
SIMD_VISIT_SELECT_LANE(I64x2RelaxedLaneSelect)
#undef SIMD_VISIT_SELECT_LANE

#define VISIT_SIMD_QFMOP(Name, instruction)              \
                                                         \
  void InstructionSelectorT::Visit##Name(OpIndex node) { \
    VisitRRRR(this, instruction, node);                  \
  }
VISIT_SIMD_QFMOP(F64x2Qfma, kRiscvF64x2Qfma)
VISIT_SIMD_QFMOP(F64x2Qfms, kRiscvF64x2Qfms)
VISIT_SIMD_QFMOP(F32x4Qfma, kRiscvF32x4Qfma)
VISIT_SIMD_QFMOP(F32x4Qfms, kRiscvF32x4Qfms)
#undef VISIT_SIMD_QFMOP

void InstructionSelectorT::VisitF32x4Min(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  InstructionOperand mask_reg = g.TempFpRegister(v0);
  InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);

  this->Emit(kRiscvVmfeqVv, temp1, g.UseRegister(this->input_at(node, 0)),
             g.UseRegister(this->input_at(node, 0)), g.UseImmediate(E32),
             g.UseImmediate(m1));
  this->Emit(kRiscvVmfeqVv, temp2, g.UseRegister(this->input_at(node, 1)),
             g.UseRegister(this->input_at(node, 1)), g.UseImmediate(E32),
             g.UseImmediate(m1));
  this->Emit(kRiscvVandVv, mask_reg, temp2, temp1, g.UseImmediate(E32),
             g.UseImmediate(m1));

  InstructionOperand NaN = g.TempFpRegister(kSimd128ScratchReg);
  InstructionOperand result = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, NaN, g.UseImmediate(0x7FC00000), g.UseImmediate(E32),
             g.UseImmediate(m1));
  this->Emit(kRiscvVfminVv, result, g.UseRegister(this->input_at(node, 1)),
             g.UseRegister(this->input_at(node, 0)), g.UseImmediate(E32),
             g.UseImmediate(m1), g.UseImmediate(MaskType::Mask));
  this->Emit(kRiscvVmv, g.DefineAsRegister(node), result, g.UseImmediate(E32),
             g.UseImmediate(m1));
}

void InstructionSelectorT::VisitF32x4Max(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  InstructionOperand mask_reg = g.TempFpRegister(v0);
  InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);

  this->Emit(kRiscvVmfeqVv, temp1, g.UseRegister(this->input_at(node, 0)),
             g.UseRegister(this->input_at(node, 0)), g.UseImmediate(E32),
             g.UseImmediate(m1));
  this->Emit(kRiscvVmfeqVv, temp2, g.UseRegister(this->input_at(node, 1)),
             g.UseRegister(this->input_at(node, 1)), g.UseImmediate(E32),
             g.UseImmediate(m1));
  this->Emit(kRiscvVandVv, mask_reg, temp2, temp1, g.UseImmediate(E32),
             g.UseImmediate(m1));

  InstructionOperand NaN = g.TempFpRegister(kSimd128ScratchReg);
  InstructionOperand result = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, NaN, g.UseImmediate(0x7FC00000), g.UseImmediate(E32),
             g.UseImmediate(m1));
  this->Emit(kRiscvVfmaxVv, result, g.UseRegister(this->input_at(node, 1)),
             g.UseRegister(this->input_at(node, 0)), g.UseImmediate(E32),
             g.UseImmediate(m1), g.UseImmediate(MaskType::Mask));
  this->Emit(kRiscvVmv, g.DefineAsRegister(node), result, g.UseImmediate(E32),
             g.UseImmediate(m1));
}

void InstructionSelectorT::VisitF32x4RelaxedMin(OpIndex node) {
  VisitF32x4Min(node);
}

void InstructionSelectorT::VisitF64x2RelaxedMin(OpIndex node) {
  VisitF64x2Min(node);
}

void InstructionSelectorT::VisitF64x2RelaxedMax(OpIndex node) {
  VisitF64x2Max(node);
}

void InstructionSelectorT::VisitF32x4RelaxedMax(OpIndex node) {
  VisitF32x4Max(node);
}

void InstructionSelectorT::VisitF64x2Eq(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  this->Emit(kRiscvVmfeqVv, temp1, g.UseRegister(this->input_at(node, 1)),
             g.UseRegister(this->input_at(node, 0)), g.UseImmediate(E64),
             g.UseImmediate(m1));
  InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, temp2, g.UseImmediate(0), g.UseImmediate(E64),
             g.UseImmediate(m1));
  this->Emit(kRiscvVmergeVx, g.DefineAsRegister(node), g.UseImmediate(-1),
             temp2, g.UseImmediate(E64), g.UseImmediate(m1));
}

void InstructionSelectorT::VisitF64x2Ne(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  this->Emit(kRiscvVmfneVv, temp1, g.UseRegister(this->input_at(node, 1)),
             g.UseRegister(this->input_at(node, 0)), g.UseImmediate(E64),
             g.UseImmediate(m1));
  InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, temp2, g.UseImmediate(0), g.UseImmediate(E64),
             g.UseImmediate(m1));
  this->Emit(kRiscvVmergeVx, g.DefineAsRegister(node), g.UseImmediate(-1),
             temp2, g.UseImmediate(E64), g.UseImmediate(m1));
}

void InstructionSelectorT::VisitF64x2Lt(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  this->Emit(kRiscvVmfltVv, temp1, g.UseRegister(this->input_at(node, 0)),
             g.UseRegister(this->input_at(node, 1)), g.UseImmediate(E64),
             g.UseImmediate(m1));
  InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, temp2, g.UseImmediate(0), g.UseImmediate(E64),
             g.UseImmediate(m1));
  this->Emit(kRiscvVmergeVx, g.DefineAsRegister(node), g.UseImmediate(-1),
             temp2, g.UseImmediate(E64), g.UseImmediate(m1));
}

void InstructionSelectorT::VisitF64x2Le(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  this->Emit(kRiscvVmfleVv, temp1, g.UseRegister(this->input_at(node, 0)),
             g.UseRegister(this->input_at(node, 1)), g.UseImmediate(E64),
             g.UseImmediate(m1));
  InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, temp2, g.UseImmediate(0), g.UseImmediate(E64),
             g.UseImmediate(m1));
  this->Emit(kRiscvVmergeVx, g.DefineAsRegister(node), g.UseImmediate(-1),
             temp2, g.UseImmediate(E64), g.UseImmediate(m1));
}

void InstructionSelectorT::VisitF32x4Eq(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  this->Emit(kRiscvVmfeqVv, temp1, g.UseRegister(this->input_at(node, 1)),
             g.UseRegister(this->input_at(node, 0)), g.UseImmediate(E32),
             g.UseImmediate(m1));
  InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, temp2, g.UseImmediate(0), g.UseImmediate(E32),
             g.UseImmediate(m1));
  this->Emit(kRiscvVmergeVx, g.DefineAsRegister(node), g.UseImmediate(-1),
             temp2, g.UseImmediate(E32), g.UseImmediate(m1));
}

void InstructionSelectorT::VisitF32x4Ne(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  this->Emit(kRiscvVmfneVv, temp1, g.UseRegister(this->input_at(node, 1)),
             g.UseRegister(this->input_at(node, 0)), g.UseImmediate(E32),
             g.UseImmediate(m1));
  InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, temp2, g.UseImmediate(0), g.UseImmediate(E32),
             g.UseImmediate(m1));
  this->Emit(kRiscvVmergeVx, g.DefineAsRegister(node), g.UseImmediate(-1),
             temp2, g.UseImmediate(E32), g.UseImmediate(m1));
}

void InstructionSelectorT::VisitF32x4Lt(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  this->Emit(kRiscvVmfltVv, temp1, g.UseRegister(this->input_at(node, 0)),
             g.UseRegister(this->input_at(node, 1)), g.UseImmediate(E32),
             g.UseImmediate(m1));
  InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, temp2, g.UseImmediate(0), g.UseImmediate(E32),
             g.UseImmediate(m1));
  this->Emit(kRiscvVmergeVx, g.DefineAsRegister(node), g.UseImmediate(-1),
             temp2, g.UseImmediate(E32), g.UseImmediate(m1));
}

void InstructionSelectorT::VisitF32x4Le(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  this->Emit(kRiscvVmfleVv, temp1, g.UseRegister(this->input_at(node, 0)),
             g.UseRegister(this->input_at(node, 1)), g.UseImmediate(E32),
             g.UseImmediate(m1));
  InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, temp2, g.UseImmediate(0), g.UseImmediate(E32),
             g.UseImmediate(m1));
  this->Emit(kRiscvVmergeVx, g.DefineAsRegister(node), g.UseImmediate(-1),
             temp2, g.UseImmediate(E32), g.UseImmediate(m1));
}

void InstructionSelectorT::VisitI32x4SConvertI16x8Low(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, temp, g.UseRegister(this->input_at(node, 0)),
             g.UseImmediate(E32), g.UseImmediate(m1));
  this->Emit(kRiscvVsextVf2, g.DefineAsRegister(node), temp,
             g.UseImmediate(E32), g.UseImmediate(m1));
}

void InstructionSelectorT::VisitI32x4UConvertI16x8Low(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, temp, g.UseRegister(this->input_at(node, 0)),
             g.UseImmediate(E32), g.UseImmediate(m1));
  this->Emit(kRiscvVzextVf2, g.DefineAsRegister(node), temp,
             g.UseImmediate(E32), g.UseImmediate(m1));
}

void InstructionSelectorT::VisitI16x8SConvertI8x16High(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  Emit(kRiscvVslidedown, temp1, g.UseRegister(this->input_at(node, 0)),
       g.UseImmediate(8), g.UseImmediate(E8), g.UseImmediate(m1));
  Emit(kRiscvVsextVf2, g.DefineAsRegister(node), temp1, g.UseImmediate(E16),
       g.UseImmediate(m1));
}

void InstructionSelectorT::VisitI16x8SConvertI32x4(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(v26);
  InstructionOperand temp2 = g.TempFpRegister(v27);
  this->Emit(kRiscvVmv, temp, g.UseRegister(this->input_at(node, 0)),
             g.UseImmediate(E32), g.UseImmediate(m1));
  this->Emit(kRiscvVmv, temp2, g.UseRegister(this->input_at(node, 1)),
             g.UseImmediate(E32), g.UseImmediate(m1));
  this->Emit(kRiscvVnclip, g.DefineAsRegister(node), temp, g.UseImmediate(0),
             g.UseImmediate(E16), g.UseImmediate(m1),
             g.UseImmediate(FPURoundingMode::RNE));
}

void InstructionSelectorT::VisitI16x8UConvertI32x4(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(v26);
  InstructionOperand temp2 = g.TempFpRegister(v27);
  InstructionOperand temp3 = g.TempFpRegister(v26);
  this->Emit(kRiscvVmv, temp, g.UseRegister(this->input_at(node, 0)),
             g.UseImmediate(E32), g.UseImmediate(m1));
  this->Emit(kRiscvVmv, temp2, g.UseRegister(this->input_at(node, 1)),
             g.UseImmediate(E32), g.UseImmediate(m1));
  this->Emit(kRiscvVmax, temp3, temp, g.UseImmediate(0), g.UseImmediate(E32),
             g.UseImmediate(m2));
  this->Emit(kRiscvVnclipu, g.DefineAsRegister(node), temp3, g.UseImmediate(0),
             g.UseImmediate(E16), g.UseImmediate(m1),
             g.UseImmediate(FPURoundingMode::RNE));
}

void InstructionSelectorT::VisitI8x16RoundingAverageU(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVwadduVv, temp, g.UseRegister(this->input_at(node, 0)),
             g.UseRegister(this->input_at(node, 1)), g.UseImmediate(E8),
             g.UseImmediate(m1));
  InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg3);
  this->Emit(kRiscvVwadduWx, temp2, temp, g.UseImmediate(1), g.UseImmediate(E8),
             g.UseImmediate(m1));
  InstructionOperand temp3 = g.TempFpRegister(kSimd128ScratchReg3);
  this->Emit(kRiscvVdivu, temp3, temp2, g.UseImmediate(2), g.UseImmediate(E16),
             g.UseImmediate(m2));
  this->Emit(kRiscvVnclipu, g.DefineAsRegister(node), temp3, g.UseImmediate(0),
             g.UseImmediate(E8), g.UseImmediate(m1),
             g.UseImmediate(FPURoundingMode::RNE));
}

void InstructionSelectorT::VisitI8x16SConvertI16x8(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(v26);
  InstructionOperand temp2 = g.TempFpRegister(v27);
  this->Emit(kRiscvVmv, temp, g.UseRegister(this->input_at(node, 0)),
             g.UseImmediate(E16), g.UseImmediate(m1));
  this->Emit(kRiscvVmv, temp2, g.UseRegister(this->input_at(node, 1)),
             g.UseImmediate(E16), g.UseImmediate(m1));
  this->Emit(kRiscvVnclip, g.DefineAsRegister(node), temp, g.UseImmediate(0),
             g.UseImmediate(E8), g.UseImmediate(m1),
             g.UseImmediate(FPURoundingMode::RNE));
}

void InstructionSelectorT::VisitI8x16UConvertI16x8(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(v26);
  InstructionOperand temp2 = g.TempFpRegister(v27);
  InstructionOperand temp3 = g.TempFpRegister(v26);
  this->Emit(kRiscvVmv, temp, g.UseRegister(this->input_at(node, 0)),
             g.UseImmediate(E16), g.UseImmediate(m1));
  this->Emit(kRiscvVmv, temp2, g.UseRegister(this->input_at(node, 1)),
             g.UseImmediate(E16), g.UseImmediate(m1));
  this->Emit(kRiscvVmax, temp3, temp, g.UseImmediate(0), g.UseImmediate(E16),
             g.UseImmediate(m2));
  this->Emit(kRiscvVnclipu, g.DefineAsRegister(node), temp3, g.UseImmediate(0),
             g.UseImmediate(E8), g.UseImmediate(m1),
             g.UseImmediate(FPURoundingMode::RNE));
}

void InstructionSelectorT::VisitI16x8RoundingAverageU(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(v16);
  InstructionOperand temp2 = g.TempFpRegister(v16);
  InstructionOperand temp3 = g.TempFpRegister(v16);
  this->Emit(kRiscvVwadduVv, temp, g.UseRegister(this->input_at(node, 0)),
             g.UseRegister(this->input_at(node, 1)), g.UseImmediate(E16),
             g.UseImmediate(m1));
  this->Emit(kRiscvVwadduWx, temp2, temp, g.UseImmediate(1),
             g.UseImmediate(E16), g.UseImmediate(m1));
  this->Emit(kRiscvVdivu, temp3, temp2, g.UseImmediate(2), g.UseImmediate(E32),
             g.UseImmediate(m2));
  this->Emit(kRiscvVnclipu, g.DefineAsRegister(node), temp3, g.UseImmediate(0),
             g.UseImmediate(E16), g.UseImmediate(m1),
             g.UseImmediate(FPURoundingMode::RNE));
}

void InstructionSelectorT::VisitI32x4DotI16x8S(OpIndex node) {
  constexpr int32_t FIRST_INDEX = 0b01010101;
  constexpr int32_t SECOND_INDEX = 0b10101010;
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(v16);
  InstructionOperand temp1 = g.TempFpRegister(v14);
  InstructionOperand temp2 = g.TempFpRegister(v30);
  InstructionOperand dst = g.DefineAsRegister(node);
  this->Emit(kRiscvVwmul, temp, g.UseRegister(this->input_at(node, 0)),
             g.UseRegister(this->input_at(node, 1)), g.UseImmediate(E16),
             g.UseImmediate(m1));
  this->Emit(kRiscvVcompress, temp2, temp, g.UseImmediate(FIRST_INDEX),
             g.UseImmediate(E32), g.UseImmediate(m2));
  this->Emit(kRiscvVcompress, temp1, temp, g.UseImmediate(SECOND_INDEX),
             g.UseImmediate(E32), g.UseImmediate(m2));
  this->Emit(kRiscvVaddVv, dst, temp1, temp2, g.UseImmediate(E32),
             g.UseImmediate(m1));
}

void InstructionSelectorT::VisitI16x8DotI8x16I7x16S(OpIndex node) {
  constexpr int32_t FIRST_INDEX = 0b0101010101010101;
  constexpr int32_t SECOND_INDEX = 0b1010101010101010;
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(v16);
  InstructionOperand temp1 = g.TempFpRegister(v14);
  InstructionOperand temp2 = g.TempFpRegister(v30);
  InstructionOperand dst = g.DefineAsRegister(node);
  this->Emit(kRiscvVwmul, temp, g.UseRegister(this->input_at(node, 0)),
             g.UseRegister(this->input_at(node, 1)), g.UseImmediate(E8),
             g.UseImmediate(m1));
  this->Emit(kRiscvVcompress, temp2, temp, g.UseImmediate(FIRST_INDEX),
             g.UseImmediate(E16), g.UseImmediate(m2));
  this->Emit(kRiscvVcompress, temp1, temp, g.UseImmediate(SECOND_INDEX),
             g.UseImmediate(E16), g.UseImmediate(m2));
  this->Emit(kRiscvVaddVv, dst, temp1, temp2, g.UseImmediate(E16),
             g.UseImmediate(m1));
}

void InstructionSelectorT::VisitI32x4DotI8x16I7x16AddS(OpIndex node) {
  constexpr int32_t FIRST_INDEX = 0b0001000100010001;
  constexpr int32_t SECOND_INDEX = 0b0010001000100010;
  constexpr int32_t THIRD_INDEX = 0b0100010001000100;
  constexpr int32_t FOURTH_INDEX = 0b1000100010001000;
  RiscvOperandGeneratorT g(this);
  InstructionOperand intermediate = g.TempFpRegister(v12);
  this->Emit(kRiscvVwmul, intermediate, g.UseRegister(this->input_at(node, 0)),
             g.UseRegister(this->input_at(node, 1)), g.UseImmediate(E8),
             g.UseImmediate(m1));

  InstructionOperand compressedPart1 = g.TempFpRegister(v14);
  InstructionOperand compressedPart2 = g.TempFpRegister(v16);
  this->Emit(kRiscvVcompress, compressedPart2, intermediate,
             g.UseImmediate(FIRST_INDEX), g.UseImmediate(E16),
             g.UseImmediate(m2));
  this->Emit(kRiscvVcompress, compressedPart1, intermediate,
             g.UseImmediate(SECOND_INDEX), g.UseImmediate(E16),
             g.UseImmediate(m2));

  InstructionOperand compressedPart3 = g.TempFpRegister(v20);
  InstructionOperand compressedPart4 = g.TempFpRegister(v26);
  this->Emit(kRiscvVcompress, compressedPart3, intermediate,
             g.UseImmediate(THIRD_INDEX), g.UseImmediate(E16),
             g.UseImmediate(m2));
  this->Emit(kRiscvVcompress, compressedPart4, intermediate,
             g.UseImmediate(FOURTH_INDEX), g.UseImmediate(E16),
             g.UseImmediate(m2));

  InstructionOperand temp2 = g.TempFpRegister(v18);
  InstructionOperand temp = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVwaddVv, temp2, compressedPart1, compressedPart2,
             g.UseImmediate(E16), g.UseImmediate(m1));
  this->Emit(kRiscvVwaddVv, temp, compressedPart3, compressedPart4,
             g.UseImmediate(E16), g.UseImmediate(m1));

  InstructionOperand mul_result = g.TempFpRegister(v16);
  InstructionOperand dst = g.DefineAsRegister(node);
  this->Emit(kRiscvVaddVv, mul_result, temp2, temp, g.UseImmediate(E32),
             g.UseImmediate(m1));
  this->Emit(kRiscvVaddVv, dst, mul_result,
             g.UseRegister(this->input_at(node, 2)), g.UseImmediate(E32),
             g.UseImmediate(m1));
}

void InstructionSelectorT::VisitI8x16Shuffle(OpIndex node) {
  uint8_t shuffle[kSimd128Size];
  bool is_swizzle;
  // TODO(riscv): Properly use view here once Turboshaft support is
  // implemented.
  auto view = this->simd_shuffle_view(node);
  CanonicalizeShuffle(view, shuffle, &is_swizzle);
  OpIndex input0 = view.input(0);
  OpIndex input1 = view.input(1);
  RiscvOperandGeneratorT g(this);
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
  Emit(kRiscvI8x16Shuffle, g.DefineAsRegister(node), g.UseRegister(input0),
       g.UseRegister(input1),
       g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle)),
       g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle + 4)),
       g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle + 8)),
       g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle + 12)));
}

void InstructionSelectorT::VisitI8x16Swizzle(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temps[] = {g.TempSimd128Register()};
  // We don't want input 0 or input 1 to be the same as output, since we will
  // modify output before do the calculation.
  Emit(kRiscvVrgather, g.DefineAsRegister(node),
       g.UseUniqueRegister(this->input_at(node, 0)),
       g.UseUniqueRegister(this->input_at(node, 1)), g.UseImmediate(E8),
       g.UseImmediate(m1), arraysize(temps), temps);
}

#define VISIT_BIMASK(TYPE, VSEW, LMUL)                                      \
                                                                            \
  void InstructionSelectorT::Visit##TYPE##BitMask(OpIndex node) {           \
    RiscvOperandGeneratorT g(this);                                         \
    InstructionOperand temp = g.TempFpRegister(v16);                        \
    this->Emit(kRiscvVmslt, temp, g.UseRegister(this->input_at(node, 0)),   \
               g.UseImmediate(0), g.UseImmediate(VSEW), g.UseImmediate(m1), \
               g.UseImmediate(true));                                       \
    this->Emit(kRiscvVmvXs, g.DefineAsRegister(node), temp,                 \
               g.UseImmediate(E32), g.UseImmediate(m1));                    \
  }

SIMD_INT_TYPE_LIST(VISIT_BIMASK)
#undef VISIT_BIMASK

void InstructionSelectorT::VisitI32x4SConvertI16x8High(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVslidedown, temp, g.UseRegister(this->input_at(node, 0)),
             g.UseImmediate(4), g.UseImmediate(E16), g.UseImmediate(m1));
  this->Emit(kRiscvVsextVf2, g.DefineAsRegister(node), temp,
             g.UseImmediate(E32), g.UseImmediate(m1));
}

void InstructionSelectorT::VisitI32x4UConvertI16x8High(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVslidedown, temp, g.UseRegister(this->input_at(node, 0)),
             g.UseImmediate(4), g.UseImmediate(E16), g.UseImmediate(m1));
  this->Emit(kRiscvVzextVf2, g.DefineAsRegister(node), temp,
             g.UseImmediate(E32), g.UseImmediate(m1));
}

void InstructionSelectorT::VisitI16x8SConvertI8x16Low(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, temp, g.UseRegister(this->input_at(node, 0)),
             g.UseImmediate(E16), g.UseImmediate(m1));
  this->Emit(kRiscvVsextVf2, g.DefineAsRegister(node), temp,
             g.UseImmediate(E16), g.UseImmediate(m1));
}

void InstructionSelectorT::VisitI16x8UConvertI8x16High(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(kSimd128ScratchReg);
  Emit(kRiscvVslidedown, temp, g.UseRegister(this->input_at(node, 0)),
       g.UseImmediate(8), g.UseImmediate(E8), g.UseImmediate(m1));
  Emit(kRiscvVzextVf2, g.DefineAsRegister(node), temp, g.UseImmediate(E16),
       g.UseImmediate(m1));
}

void InstructionSelectorT::VisitI16x8UConvertI8x16Low(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp = g.TempFpRegister(kSimd128ScratchReg);
  Emit(kRiscvVmv, temp, g.UseRegister(this->input_at(node, 0)),
       g.UseImmediate(E16), g.UseImmediate(m1));
  Emit(kRiscvVzextVf2, g.DefineAsRegister(node), temp, g.UseImmediate(E16),
       g.UseImmediate(m1));
}

void InstructionSelectorT::VisitSignExtendWord8ToInt32(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  Emit(kRiscvSignExtendByte, g.DefineAsRegister(node),
       g.UseRegister(this->input_at(node, 0)));
}

void InstructionSelectorT::VisitSignExtendWord16ToInt32(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  Emit(kRiscvSignExtendShort, g.DefineAsRegister(node),
       g.UseRegister(this->input_at(node, 0)));
}

void InstructionSelectorT::VisitWord32Clz(OpIndex node) {
  VisitRR(this, kRiscvClz32, node);
}

#define VISIT_EXT_MUL(OPCODE1, OPCODE2, TYPE)                                \
                                                                             \
  void InstructionSelectorT::Visit##OPCODE1##ExtMulLow##OPCODE2##S(          \
      OpIndex node) {                                                        \
    RiscvOperandGeneratorT g(this);                                          \
    Emit(kRiscvVwmul, g.DefineAsRegister(node),                              \
         g.UseUniqueRegister(this->input_at(node, 0)),                       \
         g.UseUniqueRegister(this->input_at(node, 1)),                       \
         g.UseImmediate(E##TYPE), g.UseImmediate(mf2));                      \
  }                                                                          \
                                                                             \
  void InstructionSelectorT::Visit##OPCODE1##ExtMulHigh##OPCODE2##S(         \
      OpIndex node) {                                                        \
    RiscvOperandGeneratorT g(this);                                          \
    InstructionOperand t1 = g.TempFpRegister(v16);                           \
    Emit(kRiscvVslidedown, t1, g.UseUniqueRegister(this->input_at(node, 0)), \
         g.UseImmediate(kRvvVLEN / TYPE / 2), g.UseImmediate(E##TYPE),       \
         g.UseImmediate(m1));                                                \
    InstructionOperand t2 = g.TempFpRegister(v17);                           \
    Emit(kRiscvVslidedown, t2, g.UseUniqueRegister(this->input_at(node, 1)), \
         g.UseImmediate(kRvvVLEN / TYPE / 2), g.UseImmediate(E##TYPE),       \
         g.UseImmediate(m1));                                                \
    Emit(kRiscvVwmul, g.DefineAsRegister(node), t1, t2,                      \
         g.UseImmediate(E##TYPE), g.UseImmediate(mf2));                      \
  }                                                                          \
                                                                             \
  void InstructionSelectorT::Visit##OPCODE1##ExtMulLow##OPCODE2##U(          \
      OpIndex node) {                                                        \
    RiscvOperandGeneratorT g(this);                                          \
    Emit(kRiscvVwmulu, g.DefineAsRegister(node),                             \
         g.UseUniqueRegister(this->input_at(node, 0)),                       \
         g.UseUniqueRegister(this->input_at(node, 1)),                       \
         g.UseImmediate(E##TYPE), g.UseImmediate(mf2));                      \
  }                                                                          \
                                                                             \
  void InstructionSelectorT::Visit##OPCODE1##ExtMulHigh##OPCODE2##U(         \
      OpIndex node) {                                                        \
    RiscvOperandGeneratorT g(this);                                          \
    InstructionOperand t1 = g.TempFpRegister(v16);                           \
    Emit(kRiscvVslidedown, t1, g.UseUniqueRegister(this->input_at(node, 0)), \
         g.UseImmediate(kRvvVLEN / TYPE / 2), g.UseImmediate(E##TYPE),       \
         g.UseImmediate(m1));                                                \
    InstructionOperand t2 = g.TempFpRegister(v17);                           \
    Emit(kRiscvVslidedown, t2, g.UseUniqueRegister(this->input_at(node, 1)), \
         g.UseImmediate(kRvvVLEN / TYPE / 2), g.UseImmediate(E##TYPE),       \
         g.UseImmediate(m1));                                                \
    Emit(kRiscvVwmulu, g.DefineAsRegister(node), t1, t2,                     \
         g.UseImmediate(E##TYPE), g.UseImmediate(mf2));                      \
  }

VISIT_EXT_MUL(I64x2, I32x4, 32)
VISIT_EXT_MUL(I32x4, I16x8, 16)
VISIT_EXT_MUL(I16x8, I8x16, 8)
#undef VISIT_EXT_MUL

void InstructionSelectorT::VisitF32x4Pmin(OpIndex node) {
  VisitUniqueRRR(this, kRiscvF32x4Pmin, node);
}

void InstructionSelectorT::VisitF32x4Pmax(OpIndex node) {
  VisitUniqueRRR(this, kRiscvF32x4Pmax, node);
}

void InstructionSelectorT::VisitF64x2Pmin(OpIndex node) {
  VisitUniqueRRR(this, kRiscvF64x2Pmin, node);
}

void InstructionSelectorT::VisitF64x2Pmax(OpIndex node) {
  VisitUniqueRRR(this, kRiscvF64x2Pmax, node);
}

void InstructionSelectorT::VisitTruncateFloat64ToFloat16RawBits(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitChangeFloat16RawBitsToFloat64(OpIndex node) {
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

void InstructionSelectorT::AddOutputToSelectContinuation(OperandGenerator* g,
                                                         int first_input_index,
                                                         OpIndex node) {
  UNREACHABLE();
}

#if V8_ENABLE_WEBASSEMBLY

void InstructionSelectorT::VisitSetStackPointer(OpIndex node) {
  OperandGenerator g(this);
  auto input = g.UseRegister(this->input_at(node, 0));
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
