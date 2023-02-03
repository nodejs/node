// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_COMPILER_BACKEND_RISCV_INSTRUCTION_SELECTOR_RISCV_H_
#define V8_COMPILER_BACKEND_RISCV_INSTRUCTION_SELECTOR_RISCV_H_

#include "src/base/bits.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE_UNIMPL() \
  PrintF("UNIMPLEMENTED instr_sel: %s at line %d\n", __FUNCTION__, __LINE__)

#define TRACE() PrintF("instr_sel: %s at line %d\n", __FUNCTION__, __LINE__)

// Adds RISC-V-specific methods for generating InstructionOperands.
class RiscvOperandGenerator final : public OperandGenerator {
 public:
  explicit RiscvOperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand UseOperand(Node* node, InstructionCode opcode) {
    if (CanBeImmediate(node, opcode)) {
      return UseImmediate(node);
    }
    return UseRegister(node);
  }

  // Use the zero register if the node has the immediate value zero, otherwise
  // assign a register.
  InstructionOperand UseRegisterOrImmediateZero(Node* node) {
    if ((IsIntegerConstant(node) && (GetIntegerConstantValue(node) == 0)) ||
        (IsFloatConstant(node) &&
         (base::bit_cast<int64_t>(GetFloatConstantValue(node)) == 0))) {
      return UseImmediate(node);
    }
    return UseRegister(node);
  }

  bool IsIntegerConstant(Node* node);

  int64_t GetIntegerConstantValue(Node* node);

  bool IsFloatConstant(Node* node) {
    return (node->opcode() == IrOpcode::kFloat32Constant) ||
           (node->opcode() == IrOpcode::kFloat64Constant);
  }

  double GetFloatConstantValue(Node* node) {
    if (node->opcode() == IrOpcode::kFloat32Constant) {
      return OpParameter<float>(node->op());
    }
    DCHECK_EQ(IrOpcode::kFloat64Constant, node->opcode());
    return OpParameter<double>(node->op());
  }

  bool CanBeImmediate(Node* node, InstructionCode mode) {
    return IsIntegerConstant(node) &&
           CanBeImmediate(GetIntegerConstantValue(node), mode);
  }

  bool CanBeImmediate(int64_t value, InstructionCode opcode);

 private:
  bool ImmediateFitsAddrMode1Instruction(int32_t imm) const {
    TRACE_UNIMPL();
    return false;
  }
};

void InstructionSelector::VisitProtectedStore(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelector::VisitProtectedLoad(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

static void VisitRR(InstructionSelector* selector, ArchOpcode opcode,
                    Node* node) {
  RiscvOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}

static void VisitRRI(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node) {
  RiscvOperandGenerator g(selector);
  int32_t imm = OpParameter<int32_t>(node->op());
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)), g.UseImmediate(imm));
}

static void VisitSimdShift(InstructionSelector* selector, ArchOpcode opcode,
                           Node* node) {
  RiscvOperandGenerator g(selector);
  if (g.IsIntegerConstant(node->InputAt(1))) {
    selector->Emit(opcode, g.DefineAsRegister(node),
                   g.UseRegister(node->InputAt(0)),
                   g.UseImmediate(node->InputAt(1)));
  } else {
    selector->Emit(opcode, g.DefineAsRegister(node),
                   g.UseRegister(node->InputAt(0)),
                   g.UseRegister(node->InputAt(1)));
  }
}

static void VisitRRIR(InstructionSelector* selector, ArchOpcode opcode,
                      Node* node) {
  RiscvOperandGenerator g(selector);
  int32_t imm = OpParameter<int32_t>(node->op());
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)), g.UseImmediate(imm),
                 g.UseRegister(node->InputAt(1)));
}

static void VisitRRR(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node) {
  RiscvOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
}

static void VisitUniqueRRR(InstructionSelector* selector, ArchOpcode opcode,
                           Node* node) {
  RiscvOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseUniqueRegister(node->InputAt(0)),
                 g.UseUniqueRegister(node->InputAt(1)));
}

void VisitRRRR(InstructionSelector* selector, ArchOpcode opcode, Node* node) {
  RiscvOperandGenerator g(selector);
  selector->Emit(
      opcode, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
      g.UseRegister(node->InputAt(1)), g.UseRegister(node->InputAt(2)));
}

static void VisitRRO(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node) {
  RiscvOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseOperand(node->InputAt(1), opcode));
}

bool TryMatchImmediate(InstructionSelector* selector,
                       InstructionCode* opcode_return, Node* node,
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

static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, bool has_reverse_opcode,
                       InstructionCode reverse_opcode,
                       FlagsContinuation* cont) {
  RiscvOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand inputs[2];
  size_t input_count = 0;
  InstructionOperand outputs[1];
  size_t output_count = 0;

  if (TryMatchImmediate(selector, &opcode, m.right().node(), &input_count,
                        &inputs[1])) {
    inputs[0] = g.UseRegisterOrImmediateZero(m.left().node());
    input_count++;
  } else if (has_reverse_opcode &&
             TryMatchImmediate(selector, &reverse_opcode, m.left().node(),
                               &input_count, &inputs[1])) {
    inputs[0] = g.UseRegisterOrImmediateZero(m.right().node());
    opcode = reverse_opcode;
    input_count++;
  } else {
    inputs[input_count++] = g.UseRegister(m.left().node());
    inputs[input_count++] = g.UseOperand(m.right().node(), opcode);
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

static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, bool has_reverse_opcode,
                       InstructionCode reverse_opcode) {
  FlagsContinuation cont;
  VisitBinop(selector, node, opcode, has_reverse_opcode, reverse_opcode, &cont);
}

static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, FlagsContinuation* cont) {
  VisitBinop(selector, node, opcode, false, kArchNop, cont);
}

static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode) {
  VisitBinop(selector, node, opcode, false, kArchNop);
}

void InstructionSelector::VisitStackSlot(Node* node) {
  StackSlotRepresentation rep = StackSlotRepresentationOf(node->op());
  int alignment = rep.alignment();
  int slot = frame_->AllocateSpillSlot(rep.size(), alignment);
  OperandGenerator g(this);

  Emit(kArchStackSlot, g.DefineAsRegister(node),
       sequence()->AddImmediate(Constant(slot)),
       sequence()->AddImmediate(Constant(alignment)), 0, nullptr);
}

void InstructionSelector::VisitAbortCSADcheck(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kArchAbortCSADcheck, g.NoOutput(), g.UseFixed(node->InputAt(0), a0));
}

void EmitS128Load(InstructionSelector* selector, Node* node,
                  InstructionCode opcode, VSew sew, Vlmul lmul);

void InstructionSelector::VisitLoadTransform(Node* node) {
  LoadTransformParameters params = LoadTransformParametersOf(node->op());

  switch (params.transformation) {
    case LoadTransformation::kS128Load8Splat:
      EmitS128Load(this, node, kRiscvS128LoadSplat, E8, m1);
      break;
    case LoadTransformation::kS128Load16Splat:
      EmitS128Load(this, node, kRiscvS128LoadSplat, E16, m1);
      break;
    case LoadTransformation::kS128Load32Splat:
      EmitS128Load(this, node, kRiscvS128LoadSplat, E32, m1);
      break;
    case LoadTransformation::kS128Load64Splat:
      EmitS128Load(this, node, kRiscvS128LoadSplat, E64, m1);
      break;
    case LoadTransformation::kS128Load8x8S:
      EmitS128Load(this, node, kRiscvS128Load64ExtendS, E16, m1);
      break;
    case LoadTransformation::kS128Load8x8U:
      EmitS128Load(this, node, kRiscvS128Load64ExtendU, E16, m1);
      break;
    case LoadTransformation::kS128Load16x4S:
      EmitS128Load(this, node, kRiscvS128Load64ExtendS, E32, m1);
      break;
    case LoadTransformation::kS128Load16x4U:
      EmitS128Load(this, node, kRiscvS128Load64ExtendU, E32, m1);
      break;
    case LoadTransformation::kS128Load32x2S:
      EmitS128Load(this, node, kRiscvS128Load64ExtendS, E64, m1);
      break;
    case LoadTransformation::kS128Load32x2U:
      EmitS128Load(this, node, kRiscvS128Load64ExtendU, E64, m1);
      break;
    case LoadTransformation::kS128Load32Zero:
      EmitS128Load(this, node, kRiscvS128Load32Zero, E32, m1);
      break;
    case LoadTransformation::kS128Load64Zero:
      EmitS128Load(this, node, kRiscvS128Load64Zero, E64, m1);
      break;
    default:
      UNIMPLEMENTED();
  }
}

// Shared routine for multiple compare operations.
static void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                         InstructionOperand left, InstructionOperand right,
                         FlagsContinuation* cont) {
  selector->EmitWithContinuation(opcode, left, right, cont);
}

// Shared routine for multiple compare operations.
static void VisitWordCompareZero(InstructionSelector* selector,
                                 InstructionOperand value,
                                 FlagsContinuation* cont) {
  selector->EmitWithContinuation(kRiscvCmpZero, value, cont);
}

// Shared routine for multiple float32 compare operations.
void VisitFloat32Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  RiscvOperandGenerator g(selector);
  Float32BinopMatcher m(node);
  InstructionOperand lhs, rhs;

  lhs = m.left().IsZero() ? g.UseImmediate(m.left().node())
                          : g.UseRegister(m.left().node());
  rhs = m.right().IsZero() ? g.UseImmediate(m.right().node())
                           : g.UseRegister(m.right().node());
  VisitCompare(selector, kRiscvCmpS, lhs, rhs, cont);
}

// Shared routine for multiple float64 compare operations.
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  RiscvOperandGenerator g(selector);
  Float64BinopMatcher m(node);
  InstructionOperand lhs, rhs;

  lhs = m.left().IsZero() ? g.UseImmediate(m.left().node())
                          : g.UseRegister(m.left().node());
  rhs = m.right().IsZero() ? g.UseImmediate(m.right().node())
                           : g.UseRegister(m.right().node());
  VisitCompare(selector, kRiscvCmpD, lhs, rhs, cont);
}

// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont,
                      bool commutative) {
  RiscvOperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  // If one of the two inputs is an immediate, make sure it's on the right.
  if (!g.CanBeImmediate(right, opcode) && g.CanBeImmediate(left, opcode)) {
    cont->Commute();
    std::swap(left, right);
  }
  // Match immediates on right side of comparison.
  if (g.CanBeImmediate(right, opcode)) {
    if (opcode == kRiscvTst) {
      VisitCompare(selector, opcode, g.UseRegister(left), g.UseImmediate(right),
                   cont);
    } else {
      switch (cont->condition()) {
        case kEqual:
        case kNotEqual:
          if (cont->IsSet()) {
            VisitCompare(selector, opcode, g.UseRegister(left),
                         g.UseImmediate(right), cont);
          } else {
            Int32BinopMatcher m(node, true);
            NumberBinopMatcher n(node, true);
            if (m.right().Is(0) || n.right().IsZero()) {
              VisitWordCompareZero(selector, g.UseRegisterOrImmediateZero(left),
                                   cont);
            } else {
              VisitCompare(selector, opcode, g.UseRegister(left),
                           g.UseRegister(right), cont);
            }
          }
          break;
        case kSignedLessThan:
        case kSignedGreaterThanOrEqual:
        case kUnsignedLessThan:
        case kUnsignedGreaterThanOrEqual: {
          Int32BinopMatcher m(node, true);
          if (m.right().Is(0)) {
            VisitWordCompareZero(selector, g.UseRegisterOrImmediateZero(left),
                                 cont);
          } else {
            VisitCompare(selector, opcode, g.UseRegister(left),
                         g.UseImmediate(right), cont);
          }
        } break;
        default:
          Int32BinopMatcher m(node, true);
          if (m.right().Is(0)) {
            VisitWordCompareZero(selector, g.UseRegisterOrImmediateZero(left),
                                 cont);
          } else {
            VisitCompare(selector, opcode, g.UseRegister(left),
                         g.UseRegister(right), cont);
          }
      }
    }
  } else {
    VisitCompare(selector, opcode, g.UseRegister(left), g.UseRegister(right),
                 cont);
  }
}

void InstructionSelector::VisitSwitch(Node* node, const SwitchInfo& sw) {
  RiscvOperandGenerator g(this);
  InstructionOperand value_operand = g.UseRegister(node->InputAt(0));

  // Emit either ArchTableSwitch or ArchBinarySearchSwitch.
  if (enable_switch_jump_table_ == kEnableSwitchJumpTable) {
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

void EmitWordCompareZero(InstructionSelector* selector, Node* value,
                         FlagsContinuation* cont) {
  RiscvOperandGenerator g(selector);
  selector->EmitWithContinuation(kRiscvCmpZero,
                                 g.UseRegisterOrImmediateZero(value), cont);
}

void VisitAtomicExchange(InstructionSelector* selector, Node* node,
                         ArchOpcode opcode, AtomicWidth width) {
  RiscvOperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  AddressingMode addressing_mode = kMode_MRI;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);
  InstructionOperand outputs[1];
  outputs[0] = g.UseUniqueRegister(node);
  InstructionOperand temp[3];
  temp[0] = g.TempRegister();
  temp[1] = g.TempRegister();
  temp[2] = g.TempRegister();
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode) |
                         AtomicWidthField::encode(width);
  selector->Emit(code, 1, outputs, input_count, inputs, 3, temp);
}

void VisitAtomicCompareExchange(InstructionSelector* selector, Node* node,
                                ArchOpcode opcode, AtomicWidth width) {
  RiscvOperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* old_value = node->InputAt(2);
  Node* new_value = node->InputAt(3);

  AddressingMode addressing_mode = kMode_MRI;
  InstructionOperand inputs[4];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(old_value);
  inputs[input_count++] = g.UseUniqueRegister(new_value);
  InstructionOperand outputs[1];
  outputs[0] = g.UseUniqueRegister(node);
  InstructionOperand temp[3];
  temp[0] = g.TempRegister();
  temp[1] = g.TempRegister();
  temp[2] = g.TempRegister();
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode) |
                         AtomicWidthField::encode(width);
  selector->Emit(code, 1, outputs, input_count, inputs, 3, temp);
}

void InstructionSelector::VisitFloat32Equal(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64Equal(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64ExtractLowWord32(Node* node) {
  VisitRR(this, kRiscvFloat64ExtractLowWord32, node);
}

void InstructionSelector::VisitFloat64ExtractHighWord32(Node* node) {
  VisitRR(this, kRiscvFloat64ExtractHighWord32, node);
}

void InstructionSelector::VisitFloat64SilenceNaN(Node* node) {
  VisitRR(this, kRiscvFloat64SilenceNaN, node);
}

void InstructionSelector::VisitFloat64InsertLowWord32(Node* node) {
  RiscvOperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Emit(kRiscvFloat64InsertLowWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.UseRegister(right));
}

void InstructionSelector::VisitFloat64InsertHighWord32(Node* node) {
  RiscvOperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Emit(kRiscvFloat64InsertHighWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.UseRegister(right));
}

void InstructionSelector::VisitMemoryBarrier(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvSync, g.NoOutput());
}

bool InstructionSelector::IsTailCallAddressImmediate() { return false; }

void InstructionSelector::EmitPrepareResults(
    ZoneVector<PushParameter>* results, const CallDescriptor* call_descriptor,
    Node* node) {
  RiscvOperandGenerator g(this);

  int reverse_slot = 1;
  for (PushParameter output : *results) {
    if (!output.location.IsCallerFrameSlot()) continue;
    // Skip any alignment holes in nodes.
    if (output.node != nullptr) {
      DCHECK(!call_descriptor->IsCFunctionCall());
      if (output.location.GetType() == MachineType::Float32()) {
        MarkAsFloat32(output.node);
      } else if (output.location.GetType() == MachineType::Float64()) {
        MarkAsFloat64(output.node);
      }
      Emit(kRiscvPeek, g.DefineAsRegister(output.node),
           g.UseImmediate(reverse_slot));
    }
    reverse_slot += output.location.GetSizeInPointers();
  }
}

void InstructionSelector::EmitMoveParamToFPR(Node* node, int index) {}

void InstructionSelector::EmitMoveFPRToParam(InstructionOperand* op,
                                             LinkageLocation location) {}

void InstructionSelector::VisitFloat32Abs(Node* node) {
  VisitRR(this, kRiscvAbsS, node);
}

void InstructionSelector::VisitFloat64Abs(Node* node) {
  VisitRR(this, kRiscvAbsD, node);
}

void InstructionSelector::VisitFloat32Sqrt(Node* node) {
  VisitRR(this, kRiscvSqrtS, node);
}

void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  VisitRR(this, kRiscvSqrtD, node);
}

void InstructionSelector::VisitFloat32RoundDown(Node* node) {
  VisitRR(this, kRiscvFloat32RoundDown, node);
}

void InstructionSelector::VisitFloat32Add(Node* node) {
  VisitRRR(this, kRiscvAddS, node);
}

void InstructionSelector::VisitFloat64Add(Node* node) {
  VisitRRR(this, kRiscvAddD, node);
}

void InstructionSelector::VisitFloat32Sub(Node* node) {
  VisitRRR(this, kRiscvSubS, node);
}

void InstructionSelector::VisitFloat64Sub(Node* node) {
  VisitRRR(this, kRiscvSubD, node);
}

void InstructionSelector::VisitFloat32Mul(Node* node) {
  VisitRRR(this, kRiscvMulS, node);
}

void InstructionSelector::VisitFloat64Mul(Node* node) {
  VisitRRR(this, kRiscvMulD, node);
}

void InstructionSelector::VisitFloat32Div(Node* node) {
  VisitRRR(this, kRiscvDivS, node);
}

void InstructionSelector::VisitFloat64Div(Node* node) {
  VisitRRR(this, kRiscvDivD, node);
}

void InstructionSelector::VisitFloat64Mod(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvModD, g.DefineAsFixed(node, fa0),
       g.UseFixed(node->InputAt(0), fa0), g.UseFixed(node->InputAt(1), fa1))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat32Max(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvFloat32Max, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitFloat64Max(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvFloat64Max, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitFloat32Min(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvFloat32Min, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitFloat64Min(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvFloat64Min, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitTruncateFloat64ToWord32(Node* node) {
  VisitRR(this, kArchTruncateDoubleToI, node);
}

void InstructionSelector::VisitRoundFloat64ToInt32(Node* node) {
  VisitRR(this, kRiscvTruncWD, node);
}

void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  RiscvOperandGenerator g(this);
  Node* value = node->InputAt(0);
  // Match TruncateFloat64ToFloat32(ChangeInt32ToFloat64) to corresponding
  // instruction.
  if (CanCover(node, value) &&
      value->opcode() == IrOpcode::kChangeInt32ToFloat64) {
    Emit(kRiscvCvtSW, g.DefineAsRegister(node),
         g.UseRegister(value->InputAt(0)));
    return;
  }
  VisitRR(this, kRiscvCvtSD, node);
}

void InstructionSelector::VisitWord32Shl(Node* node) {
  Int32BinopMatcher m(node);
  if (m.left().IsWord32And() && CanCover(node, m.left().node()) &&
      m.right().IsInRange(1, 31)) {
    RiscvOperandGenerator g(this);
    Int32BinopMatcher mleft(m.left().node());
    // Match Word32Shl(Word32And(x, mask), imm) to Shl where the mask is
    // contiguous, and the shift immediate non-zero.
    if (mleft.right().HasResolvedValue()) {
      uint32_t mask = mleft.right().ResolvedValue();
      uint32_t mask_width = base::bits::CountPopulation(mask);
      uint32_t mask_msb = base::bits::CountLeadingZeros32(mask);
      if ((mask_width != 0) && (mask_msb + mask_width == 32)) {
        uint32_t shift = m.right().ResolvedValue();
        DCHECK_EQ(0u, base::bits::CountTrailingZeros32(mask));
        DCHECK_NE(0u, shift);
        if ((shift + mask_width) >= 32) {
          // If the mask is contiguous and reaches or extends beyond the top
          // bit, only the shift is needed.
          Emit(kRiscvShl32, g.DefineAsRegister(node),
               g.UseRegister(mleft.left().node()),
               g.UseImmediate(m.right().node()));
          return;
        }
      }
    }
  }
  VisitRRO(this, kRiscvShl32, node);
}

void InstructionSelector::VisitWord32Shr(Node* node) {
  VisitRRO(this, kRiscvShr32, node);
}

void InstructionSelector::VisitWord32Sar(Node* node) {
  Int32BinopMatcher m(node);
  if (m.left().IsWord32Shl() && CanCover(node, m.left().node())) {
    Int32BinopMatcher mleft(m.left().node());
    if (m.right().HasResolvedValue() && mleft.right().HasResolvedValue()) {
      RiscvOperandGenerator g(this);
      uint32_t sar = m.right().ResolvedValue();
      uint32_t shl = mleft.right().ResolvedValue();
      if ((sar == shl) && (sar == 16)) {
        Emit(kRiscvSignExtendShort, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()));
        return;
      } else if ((sar == shl) && (sar == 24)) {
        Emit(kRiscvSignExtendByte, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()));
        return;
      } else if ((sar == shl) && (sar == 32)) {
        Emit(kRiscvShl32, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(0));
        return;
      }
    }
  }
  VisitRRO(this, kRiscvSar32, node);
}

void InstructionSelector::VisitI32x4ExtAddPairwiseI16x8S(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionOperand src1 = g.TempSimd128Register();
  InstructionOperand src2 = g.TempSimd128Register();
  InstructionOperand src = g.UseUniqueRegister(node->InputAt(0));
  Emit(kRiscvVrgather, src1, src, g.UseImmediate64(0x0006000400020000),
       g.UseImmediate(int8_t(E16)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVrgather, src2, src, g.UseImmediate64(0x0007000500030001),
       g.UseImmediate(int8_t(E16)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVwadd, g.DefineAsRegister(node), src1, src2,
       g.UseImmediate(int8_t(E16)), g.UseImmediate(int8_t(mf2)));
}

void InstructionSelector::VisitI32x4ExtAddPairwiseI16x8U(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionOperand src1 = g.TempSimd128Register();
  InstructionOperand src2 = g.TempSimd128Register();
  InstructionOperand src = g.UseUniqueRegister(node->InputAt(0));
  Emit(kRiscvVrgather, src1, src, g.UseImmediate64(0x0006000400020000),
       g.UseImmediate(int8_t(E16)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVrgather, src2, src, g.UseImmediate64(0x0007000500030001),
       g.UseImmediate(int8_t(E16)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVwaddu, g.DefineAsRegister(node), src1, src2,
       g.UseImmediate(int8_t(E16)), g.UseImmediate(int8_t(mf2)));
}

void InstructionSelector::VisitI16x8ExtAddPairwiseI8x16S(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionOperand src1 = g.TempSimd128Register();
  InstructionOperand src2 = g.TempSimd128Register();
  InstructionOperand src = g.UseUniqueRegister(node->InputAt(0));
  Emit(kRiscvVrgather, src1, src, g.UseImmediate64(0x0E0C0A0806040200),
       g.UseImmediate(int8_t(E8)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVrgather, src2, src, g.UseImmediate64(0x0F0D0B0907050301),
       g.UseImmediate(int8_t(E8)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVwadd, g.DefineAsRegister(node), src1, src2,
       g.UseImmediate(int8_t(E8)), g.UseImmediate(int8_t(mf2)));
}

void InstructionSelector::VisitI16x8ExtAddPairwiseI8x16U(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionOperand src1 = g.TempSimd128Register();
  InstructionOperand src2 = g.TempSimd128Register();
  InstructionOperand src = g.UseUniqueRegister(node->InputAt(0));
  Emit(kRiscvVrgather, src1, src, g.UseImmediate64(0x0E0C0A0806040200),
       g.UseImmediate(int8_t(E8)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVrgather, src2, src, g.UseImmediate64(0x0F0D0B0907050301),
       g.UseImmediate(int8_t(E8)), g.UseImmediate(int8_t(m1)));
  Emit(kRiscvVwaddu, g.DefineAsRegister(node), src1, src2,
       g.UseImmediate(int8_t(E8)), g.UseImmediate(int8_t(mf2)));
}

#define SIMD_TYPE_LIST(V) \
  V(F32x4)                \
  V(I64x2)                \
  V(I32x4)                \
  V(I16x8)                \
  V(I8x16)

#define SIMD_UNOP_LIST(V)                                       \
  V(F64x2Abs, kRiscvF64x2Abs)                                   \
  V(F64x2Neg, kRiscvF64x2Neg)                                   \
  V(F64x2Sqrt, kRiscvF64x2Sqrt)                                 \
  V(F64x2ConvertLowI32x4S, kRiscvF64x2ConvertLowI32x4S)         \
  V(F64x2ConvertLowI32x4U, kRiscvF64x2ConvertLowI32x4U)         \
  V(F64x2PromoteLowF32x4, kRiscvF64x2PromoteLowF32x4)           \
  V(F64x2Ceil, kRiscvF64x2Ceil)                                 \
  V(F64x2Floor, kRiscvF64x2Floor)                               \
  V(F64x2Trunc, kRiscvF64x2Trunc)                               \
  V(F64x2NearestInt, kRiscvF64x2NearestInt)                     \
  V(I64x2Neg, kRiscvI64x2Neg)                                   \
  V(I64x2Abs, kRiscvI64x2Abs)                                   \
  V(I64x2BitMask, kRiscvI64x2BitMask)                           \
  V(F32x4SConvertI32x4, kRiscvF32x4SConvertI32x4)               \
  V(F32x4UConvertI32x4, kRiscvF32x4UConvertI32x4)               \
  V(F32x4Abs, kRiscvF32x4Abs)                                   \
  V(F32x4Neg, kRiscvF32x4Neg)                                   \
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
  V(I32x4Neg, kRiscvI32x4Neg)                                   \
  V(I32x4SConvertI16x8Low, kRiscvI32x4SConvertI16x8Low)         \
  V(I32x4SConvertI16x8High, kRiscvI32x4SConvertI16x8High)       \
  V(I32x4UConvertI16x8Low, kRiscvI32x4UConvertI16x8Low)         \
  V(I32x4UConvertI16x8High, kRiscvI32x4UConvertI16x8High)       \
  V(I32x4Abs, kRiscvI32x4Abs)                                   \
  V(I32x4BitMask, kRiscvI32x4BitMask)                           \
  V(I32x4TruncSatF64x2SZero, kRiscvI32x4TruncSatF64x2SZero)     \
  V(I32x4TruncSatF64x2UZero, kRiscvI32x4TruncSatF64x2UZero)     \
  V(I16x8Neg, kRiscvI16x8Neg)                                   \
  V(I16x8SConvertI8x16Low, kRiscvI16x8SConvertI8x16Low)         \
  V(I16x8SConvertI8x16High, kRiscvI16x8SConvertI8x16High)       \
  V(I16x8UConvertI8x16Low, kRiscvI16x8UConvertI8x16Low)         \
  V(I16x8UConvertI8x16High, kRiscvI16x8UConvertI8x16High)       \
  V(I16x8Abs, kRiscvI16x8Abs)                                   \
  V(I16x8BitMask, kRiscvI16x8BitMask)                           \
  V(I8x16Neg, kRiscvI8x16Neg)                                   \
  V(I8x16Abs, kRiscvI8x16Abs)                                   \
  V(I8x16BitMask, kRiscvI8x16BitMask)                           \
  V(I8x16Popcnt, kRiscvI8x16Popcnt)                             \
  V(S128Not, kRiscvS128Not)                                     \
  V(V128AnyTrue, kRiscvV128AnyTrue)                             \
  V(I32x4AllTrue, kRiscvI32x4AllTrue)                           \
  V(I16x8AllTrue, kRiscvI16x8AllTrue)                           \
  V(I8x16AllTrue, kRiscvI8x16AllTrue)                           \
  V(I64x2AllTrue, kRiscvI64x2AllTrue)

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

#define SIMD_BINOP_LIST(V)                              \
  V(F64x2Add, kRiscvF64x2Add)                           \
  V(F64x2Sub, kRiscvF64x2Sub)                           \
  V(F64x2Mul, kRiscvF64x2Mul)                           \
  V(F64x2Div, kRiscvF64x2Div)                           \
  V(F64x2Min, kRiscvF64x2Min)                           \
  V(F64x2Max, kRiscvF64x2Max)                           \
  V(F64x2Eq, kRiscvF64x2Eq)                             \
  V(F64x2Ne, kRiscvF64x2Ne)                             \
  V(F64x2Lt, kRiscvF64x2Lt)                             \
  V(F64x2Le, kRiscvF64x2Le)                             \
  V(I64x2Eq, kRiscvI64x2Eq)                             \
  V(I64x2Ne, kRiscvI64x2Ne)                             \
  V(I64x2GtS, kRiscvI64x2GtS)                           \
  V(I64x2GeS, kRiscvI64x2GeS)                           \
  V(I64x2Add, kRiscvI64x2Add)                           \
  V(I64x2Sub, kRiscvI64x2Sub)                           \
  V(I64x2Mul, kRiscvI64x2Mul)                           \
  V(F32x4Add, kRiscvF32x4Add)                           \
  V(F32x4Sub, kRiscvF32x4Sub)                           \
  V(F32x4Mul, kRiscvF32x4Mul)                           \
  V(F32x4Div, kRiscvF32x4Div)                           \
  V(F32x4Max, kRiscvF32x4Max)                           \
  V(F32x4Min, kRiscvF32x4Min)                           \
  V(F32x4Eq, kRiscvF32x4Eq)                             \
  V(F32x4Ne, kRiscvF32x4Ne)                             \
  V(F32x4Lt, kRiscvF32x4Lt)                             \
  V(F32x4Le, kRiscvF32x4Le)                             \
  V(F32x4RelaxedMin, kRiscvF32x4Min)                    \
  V(F32x4RelaxedMax, kRiscvF32x4Max)                    \
  V(F64x2RelaxedMin, kRiscvF64x2Min)                    \
  V(F64x2RelaxedMax, kRiscvF64x2Max)                    \
  V(I32x4Add, kRiscvI32x4Add)                           \
  V(I32x4Sub, kRiscvI32x4Sub)                           \
  V(I32x4Mul, kRiscvI32x4Mul)                           \
  V(I32x4MaxS, kRiscvI32x4MaxS)                         \
  V(I32x4MinS, kRiscvI32x4MinS)                         \
  V(I32x4MaxU, kRiscvI32x4MaxU)                         \
  V(I32x4MinU, kRiscvI32x4MinU)                         \
  V(I32x4Eq, kRiscvI32x4Eq)                             \
  V(I32x4Ne, kRiscvI32x4Ne)                             \
  V(I32x4GtS, kRiscvI32x4GtS)                           \
  V(I32x4GeS, kRiscvI32x4GeS)                           \
  V(I32x4GtU, kRiscvI32x4GtU)                           \
  V(I32x4GeU, kRiscvI32x4GeU)                           \
  V(I16x8Add, kRiscvI16x8Add)                           \
  V(I16x8AddSatS, kRiscvI16x8AddSatS)                   \
  V(I16x8AddSatU, kRiscvI16x8AddSatU)                   \
  V(I16x8Sub, kRiscvI16x8Sub)                           \
  V(I16x8SubSatS, kRiscvI16x8SubSatS)                   \
  V(I16x8SubSatU, kRiscvI16x8SubSatU)                   \
  V(I16x8Mul, kRiscvI16x8Mul)                           \
  V(I16x8MaxS, kRiscvI16x8MaxS)                         \
  V(I16x8MinS, kRiscvI16x8MinS)                         \
  V(I16x8MaxU, kRiscvI16x8MaxU)                         \
  V(I16x8MinU, kRiscvI16x8MinU)                         \
  V(I16x8Eq, kRiscvI16x8Eq)                             \
  V(I16x8Ne, kRiscvI16x8Ne)                             \
  V(I16x8GtS, kRiscvI16x8GtS)                           \
  V(I16x8GeS, kRiscvI16x8GeS)                           \
  V(I16x8GtU, kRiscvI16x8GtU)                           \
  V(I16x8GeU, kRiscvI16x8GeU)                           \
  V(I16x8RoundingAverageU, kRiscvI16x8RoundingAverageU) \
  V(I16x8Q15MulRSatS, kRiscvI16x8Q15MulRSatS)           \
  V(I16x8RelaxedQ15MulRS, kRiscvI16x8Q15MulRSatS)       \
  V(I16x8SConvertI32x4, kRiscvI16x8SConvertI32x4)       \
  V(I16x8UConvertI32x4, kRiscvI16x8UConvertI32x4)       \
  V(I8x16Add, kRiscvI8x16Add)                           \
  V(I8x16AddSatS, kRiscvI8x16AddSatS)                   \
  V(I8x16AddSatU, kRiscvI8x16AddSatU)                   \
  V(I8x16Sub, kRiscvI8x16Sub)                           \
  V(I8x16SubSatS, kRiscvI8x16SubSatS)                   \
  V(I8x16SubSatU, kRiscvI8x16SubSatU)                   \
  V(I8x16MaxS, kRiscvI8x16MaxS)                         \
  V(I8x16MinS, kRiscvI8x16MinS)                         \
  V(I8x16MaxU, kRiscvI8x16MaxU)                         \
  V(I8x16MinU, kRiscvI8x16MinU)                         \
  V(I8x16Eq, kRiscvI8x16Eq)                             \
  V(I8x16Ne, kRiscvI8x16Ne)                             \
  V(I8x16GtS, kRiscvI8x16GtS)                           \
  V(I8x16GeS, kRiscvI8x16GeS)                           \
  V(I8x16GtU, kRiscvI8x16GtU)                           \
  V(I8x16GeU, kRiscvI8x16GeU)                           \
  V(I8x16RoundingAverageU, kRiscvI8x16RoundingAverageU) \
  V(I8x16SConvertI16x8, kRiscvI8x16SConvertI16x8)       \
  V(I8x16UConvertI16x8, kRiscvI8x16UConvertI16x8)       \
  V(S128And, kRiscvS128And)                             \
  V(S128Or, kRiscvS128Or)                               \
  V(S128Xor, kRiscvS128Xor)                             \
  V(S128AndNot, kRiscvS128AndNot)

void InstructionSelector::VisitS128Const(Node* node) {
  RiscvOperandGenerator g(this);
  static const int kUint32Immediates = kSimd128Size / sizeof(uint32_t);
  uint32_t val[kUint32Immediates];
  memcpy(val, S128ImmediateParameterOf(node->op()).data(), kSimd128Size);
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

void InstructionSelector::VisitS128Zero(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvS128Zero, g.DefineAsRegister(node));
}

#define SIMD_VISIT_SPLAT(Type)                               \
  void InstructionSelector::Visit##Type##Splat(Node* node) { \
    VisitRR(this, kRiscv##Type##Splat, node);                \
  }
SIMD_TYPE_LIST(SIMD_VISIT_SPLAT)
SIMD_VISIT_SPLAT(F64x2)
#undef SIMD_VISIT_SPLAT

#define SIMD_VISIT_EXTRACT_LANE(Type, Sign)                              \
  void InstructionSelector::Visit##Type##ExtractLane##Sign(Node* node) { \
    VisitRRI(this, kRiscv##Type##ExtractLane##Sign, node);               \
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

#define SIMD_VISIT_REPLACE_LANE(Type)                              \
  void InstructionSelector::Visit##Type##ReplaceLane(Node* node) { \
    VisitRRIR(this, kRiscv##Type##ReplaceLane, node);              \
  }
SIMD_TYPE_LIST(SIMD_VISIT_REPLACE_LANE)
SIMD_VISIT_REPLACE_LANE(F64x2)
#undef SIMD_VISIT_REPLACE_LANE

#define SIMD_VISIT_UNOP(Name, instruction)            \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRR(this, instruction, node);                 \
  }
SIMD_UNOP_LIST(SIMD_VISIT_UNOP)
#undef SIMD_VISIT_UNOP

#define SIMD_VISIT_SHIFT_OP(Name)                     \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitSimdShift(this, kRiscv##Name, node);         \
  }
SIMD_SHIFT_OP_LIST(SIMD_VISIT_SHIFT_OP)
#undef SIMD_VISIT_SHIFT_OP

#define SIMD_VISIT_BINOP(Name, instruction)           \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRRR(this, instruction, node);                \
  }
SIMD_BINOP_LIST(SIMD_VISIT_BINOP)
#undef SIMD_VISIT_BINOP

void InstructionSelector::VisitS128Select(Node* node) {
  VisitRRRR(this, kRiscvS128Select, node);
}

#define SIMD_VISIT_SELECT_LANE(Name)                  \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRRRR(this, kRiscvS128Select, node);          \
  }
SIMD_VISIT_SELECT_LANE(I8x16RelaxedLaneSelect)
SIMD_VISIT_SELECT_LANE(I16x8RelaxedLaneSelect)
SIMD_VISIT_SELECT_LANE(I32x4RelaxedLaneSelect)
SIMD_VISIT_SELECT_LANE(I64x2RelaxedLaneSelect)
#undef SIMD_VISIT_SELECT_LANE

#define VISIT_SIMD_QFMOP(Name, instruction)           \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRRRR(this, instruction, node);               \
  }
VISIT_SIMD_QFMOP(F64x2Qfma, kRiscvF64x2Qfma)
VISIT_SIMD_QFMOP(F64x2Qfms, kRiscvF64x2Qfms)
VISIT_SIMD_QFMOP(F32x4Qfma, kRiscvF32x4Qfma)
VISIT_SIMD_QFMOP(F32x4Qfms, kRiscvF32x4Qfms)
#undef VISIT_SIMD_QFMOP

void InstructionSelector::VisitI32x4DotI16x8S(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionOperand temp = g.TempFpRegister(v16);
  InstructionOperand temp1 = g.TempFpRegister(v14);
  InstructionOperand temp2 = g.TempFpRegister(v30);
  InstructionOperand dst = g.DefineAsRegister(node);
  this->Emit(kRiscvVwmul, temp, g.UseRegister(node->InputAt(0)),
             g.UseRegister(node->InputAt(1)), g.UseImmediate(E16),
             g.UseImmediate(m1));
  this->Emit(kRiscvVcompress, temp2, temp, g.UseImmediate(0b01010101),
             g.UseImmediate(E32), g.UseImmediate(m2));
  this->Emit(kRiscvVcompress, temp1, temp, g.UseImmediate(0b10101010),
             g.UseImmediate(E32), g.UseImmediate(m2));
  this->Emit(kRiscvVaddVv, dst, temp1, temp2, g.UseImmediate(E32),
             g.UseImmediate(m1));
}

void InstructionSelector::VisitI8x16Shuffle(Node* node) {
  uint8_t shuffle[kSimd128Size];
  bool is_swizzle;
  CanonicalizeShuffle(node, shuffle, &is_swizzle);
  Node* input0 = node->InputAt(0);
  Node* input1 = node->InputAt(1);
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
  //   Emit(kRiscvS8x16Concat, g.DefineSameAsFirst(node), g.UseRegister(input1),
  //        g.UseRegister(input0), g.UseImmediate(offset));
  //   return;
  // }
  // if (wasm::SimdShuffle::TryMatch32x4Shuffle(shuffle, shuffle32x4)) {
  //   Emit(kRiscvS32x4Shuffle, g.DefineAsRegister(node), g.UseRegister(input0),
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

void InstructionSelector::VisitI8x16Swizzle(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionOperand temps[] = {g.TempSimd128Register()};
  // We don't want input 0 or input 1 to be the same as output, since we will
  // modify output before do the calculation.
  Emit(kRiscvVrgather, g.DefineAsRegister(node),
       g.UseUniqueRegister(node->InputAt(0)),
       g.UseUniqueRegister(node->InputAt(1)), g.UseImmediate(E8),
       g.UseImmediate(m1), arraysize(temps), temps);
}

void InstructionSelector::VisitSignExtendWord8ToInt32(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvSignExtendByte, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitSignExtendWord16ToInt32(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvSignExtendShort, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

#define VISIT_EXT_MUL(OPCODE1, OPCODE2, TYPE)                            \
  void InstructionSelector::Visit##OPCODE1##ExtMulLow##OPCODE2##S(       \
      Node* node) {                                                      \
    RiscvOperandGenerator g(this);                                       \
    Emit(kRiscvVwmul, g.DefineAsRegister(node),                          \
         g.UseUniqueRegister(node->InputAt(0)),                          \
         g.UseUniqueRegister(node->InputAt(1)), g.UseImmediate(E##TYPE), \
         g.UseImmediate(mf2));                                           \
  }                                                                      \
  void InstructionSelector::Visit##OPCODE1##ExtMulHigh##OPCODE2##S(      \
      Node* node) {                                                      \
    RiscvOperandGenerator g(this);                                       \
    InstructionOperand t1 = g.TempFpRegister(v16);                       \
    Emit(kRiscvVslidedown, t1, g.UseUniqueRegister(node->InputAt(0)),    \
         g.UseImmediate(kRvvVLEN / TYPE / 2), g.UseImmediate(E##TYPE),   \
         g.UseImmediate(m1));                                            \
    InstructionOperand t2 = g.TempFpRegister(v17);                       \
    Emit(kRiscvVslidedown, t2, g.UseUniqueRegister(node->InputAt(1)),    \
         g.UseImmediate(kRvvVLEN / TYPE / 2), g.UseImmediate(E##TYPE),   \
         g.UseImmediate(m1));                                            \
    Emit(kRiscvVwmul, g.DefineAsRegister(node), t1, t2,                  \
         g.UseImmediate(E##TYPE), g.UseImmediate(mf2));                  \
  }                                                                      \
  void InstructionSelector::Visit##OPCODE1##ExtMulLow##OPCODE2##U(       \
      Node* node) {                                                      \
    RiscvOperandGenerator g(this);                                       \
    Emit(kRiscvVwmulu, g.DefineAsRegister(node),                         \
         g.UseUniqueRegister(node->InputAt(0)),                          \
         g.UseUniqueRegister(node->InputAt(1)), g.UseImmediate(E##TYPE), \
         g.UseImmediate(mf2));                                           \
  }                                                                      \
  void InstructionSelector::Visit##OPCODE1##ExtMulHigh##OPCODE2##U(      \
      Node* node) {                                                      \
    RiscvOperandGenerator g(this);                                       \
    InstructionOperand t1 = g.TempFpRegister(v16);                       \
    Emit(kRiscvVslidedown, t1, g.UseUniqueRegister(node->InputAt(0)),    \
         g.UseImmediate(kRvvVLEN / TYPE / 2), g.UseImmediate(E##TYPE),   \
         g.UseImmediate(m1));                                            \
    InstructionOperand t2 = g.TempFpRegister(v17);                       \
    Emit(kRiscvVslidedown, t2, g.UseUniqueRegister(node->InputAt(1)),    \
         g.UseImmediate(kRvvVLEN / TYPE / 2), g.UseImmediate(E##TYPE),   \
         g.UseImmediate(m1));                                            \
    Emit(kRiscvVwmulu, g.DefineAsRegister(node), t1, t2,                 \
         g.UseImmediate(E##TYPE), g.UseImmediate(mf2));                  \
  }

VISIT_EXT_MUL(I64x2, I32x4, 32)
VISIT_EXT_MUL(I32x4, I16x8, 16)
VISIT_EXT_MUL(I16x8, I8x16, 8)
#undef VISIT_EXT_MUL

void InstructionSelector::VisitF32x4Pmin(Node* node) {
  VisitUniqueRRR(this, kRiscvF32x4Pmin, node);
}

void InstructionSelector::VisitF32x4Pmax(Node* node) {
  VisitUniqueRRR(this, kRiscvF32x4Pmax, node);
}

void InstructionSelector::VisitF64x2Pmin(Node* node) {
  VisitUniqueRRR(this, kRiscvF64x2Pmin, node);
}

void InstructionSelector::VisitF64x2Pmax(Node* node) {
  VisitUniqueRRR(this, kRiscvF64x2Pmax, node);
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
                                                        Node* node) {
  UNREACHABLE();
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#undef SIMD_BINOP_LIST
#undef SIMD_SHIFT_OP_LIST
#undef SIMD_UNOP_LIST
#undef SIMD_TYPE_LIST
#endif  // V8_COMPILER_BACKEND_RISCV_INSTRUCTION_SELECTOR_RISCV_H_
