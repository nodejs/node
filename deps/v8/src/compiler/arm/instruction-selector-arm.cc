// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler-intrinsics.h"

namespace v8 {
namespace internal {
namespace compiler {

// Adds Arm-specific methods for generating InstructionOperands.
class ArmOperandGenerator V8_FINAL : public OperandGenerator {
 public:
  explicit ArmOperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand* UseOperand(Node* node, InstructionCode opcode) {
    if (CanBeImmediate(node, opcode)) {
      return UseImmediate(node);
    }
    return UseRegister(node);
  }

  bool CanBeImmediate(Node* node, InstructionCode opcode) {
    int32_t value;
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
      case IrOpcode::kNumberConstant:
        value = ValueOf<int32_t>(node->op());
        break;
      default:
        return false;
    }
    switch (ArchOpcodeField::decode(opcode)) {
      case kArmAnd:
      case kArmMov:
      case kArmMvn:
      case kArmBic:
        return ImmediateFitsAddrMode1Instruction(value) ||
               ImmediateFitsAddrMode1Instruction(~value);

      case kArmAdd:
      case kArmSub:
      case kArmCmp:
      case kArmCmn:
        return ImmediateFitsAddrMode1Instruction(value) ||
               ImmediateFitsAddrMode1Instruction(-value);

      case kArmTst:
      case kArmTeq:
      case kArmOrr:
      case kArmEor:
      case kArmRsb:
        return ImmediateFitsAddrMode1Instruction(value);

      case kArmFloat64Load:
      case kArmFloat64Store:
        return value >= -1020 && value <= 1020 && (value % 4) == 0;

      case kArmLoadWord8:
      case kArmStoreWord8:
      case kArmLoadWord32:
      case kArmStoreWord32:
      case kArmStoreWriteBarrier:
        return value >= -4095 && value <= 4095;

      case kArmLoadWord16:
      case kArmStoreWord16:
        return value >= -255 && value <= 255;

      case kArchJmp:
      case kArchNop:
      case kArchRet:
      case kArchDeoptimize:
      case kArmMul:
      case kArmMla:
      case kArmMls:
      case kArmSdiv:
      case kArmUdiv:
      case kArmBfc:
      case kArmUbfx:
      case kArmCallCodeObject:
      case kArmCallJSFunction:
      case kArmCallAddress:
      case kArmPush:
      case kArmDrop:
      case kArmVcmpF64:
      case kArmVaddF64:
      case kArmVsubF64:
      case kArmVmulF64:
      case kArmVmlaF64:
      case kArmVmlsF64:
      case kArmVdivF64:
      case kArmVmodF64:
      case kArmVnegF64:
      case kArmVcvtF64S32:
      case kArmVcvtF64U32:
      case kArmVcvtS32F64:
      case kArmVcvtU32F64:
        return false;
    }
    UNREACHABLE();
    return false;
  }

 private:
  bool ImmediateFitsAddrMode1Instruction(int32_t imm) const {
    return Assembler::ImmediateFitsAddrMode1Instruction(imm);
  }
};


static void VisitRRRFloat64(InstructionSelector* selector, ArchOpcode opcode,
                            Node* node) {
  ArmOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsDoubleRegister(node),
                 g.UseDoubleRegister(node->InputAt(0)),
                 g.UseDoubleRegister(node->InputAt(1)));
}


static bool TryMatchROR(InstructionSelector* selector,
                        InstructionCode* opcode_return, Node* node,
                        InstructionOperand** value_return,
                        InstructionOperand** shift_return) {
  ArmOperandGenerator g(selector);
  if (node->opcode() != IrOpcode::kWord32Or) return false;
  Int32BinopMatcher m(node);
  Node* shl = m.left().node();
  Node* shr = m.right().node();
  if (m.left().IsWord32Shr() && m.right().IsWord32Shl()) {
    std::swap(shl, shr);
  } else if (!m.left().IsWord32Shl() || !m.right().IsWord32Shr()) {
    return false;
  }
  Int32BinopMatcher mshr(shr);
  Int32BinopMatcher mshl(shl);
  Node* value = mshr.left().node();
  if (value != mshl.left().node()) return false;
  Node* shift = mshr.right().node();
  Int32Matcher mshift(shift);
  if (mshift.IsInRange(1, 31) && mshl.right().Is(32 - mshift.Value())) {
    *opcode_return |= AddressingModeField::encode(kMode_Operand2_R_ROR_I);
    *value_return = g.UseRegister(value);
    *shift_return = g.UseImmediate(shift);
    return true;
  }
  if (mshl.right().IsInt32Sub()) {
    Int32BinopMatcher mshlright(mshl.right().node());
    if (!mshlright.left().Is(32)) return false;
    if (mshlright.right().node() != shift) return false;
    *opcode_return |= AddressingModeField::encode(kMode_Operand2_R_ROR_R);
    *value_return = g.UseRegister(value);
    *shift_return = g.UseRegister(shift);
    return true;
  }
  return false;
}


static inline bool TryMatchASR(InstructionSelector* selector,
                               InstructionCode* opcode_return, Node* node,
                               InstructionOperand** value_return,
                               InstructionOperand** shift_return) {
  ArmOperandGenerator g(selector);
  if (node->opcode() != IrOpcode::kWord32Sar) return false;
  Int32BinopMatcher m(node);
  *value_return = g.UseRegister(m.left().node());
  if (m.right().IsInRange(1, 32)) {
    *opcode_return |= AddressingModeField::encode(kMode_Operand2_R_ASR_I);
    *shift_return = g.UseImmediate(m.right().node());
  } else {
    *opcode_return |= AddressingModeField::encode(kMode_Operand2_R_ASR_R);
    *shift_return = g.UseRegister(m.right().node());
  }
  return true;
}


static inline bool TryMatchLSL(InstructionSelector* selector,
                               InstructionCode* opcode_return, Node* node,
                               InstructionOperand** value_return,
                               InstructionOperand** shift_return) {
  ArmOperandGenerator g(selector);
  if (node->opcode() != IrOpcode::kWord32Shl) return false;
  Int32BinopMatcher m(node);
  *value_return = g.UseRegister(m.left().node());
  if (m.right().IsInRange(0, 31)) {
    *opcode_return |= AddressingModeField::encode(kMode_Operand2_R_LSL_I);
    *shift_return = g.UseImmediate(m.right().node());
  } else {
    *opcode_return |= AddressingModeField::encode(kMode_Operand2_R_LSL_R);
    *shift_return = g.UseRegister(m.right().node());
  }
  return true;
}


static inline bool TryMatchLSR(InstructionSelector* selector,
                               InstructionCode* opcode_return, Node* node,
                               InstructionOperand** value_return,
                               InstructionOperand** shift_return) {
  ArmOperandGenerator g(selector);
  if (node->opcode() != IrOpcode::kWord32Shr) return false;
  Int32BinopMatcher m(node);
  *value_return = g.UseRegister(m.left().node());
  if (m.right().IsInRange(1, 32)) {
    *opcode_return |= AddressingModeField::encode(kMode_Operand2_R_LSR_I);
    *shift_return = g.UseImmediate(m.right().node());
  } else {
    *opcode_return |= AddressingModeField::encode(kMode_Operand2_R_LSR_R);
    *shift_return = g.UseRegister(m.right().node());
  }
  return true;
}


static inline bool TryMatchShift(InstructionSelector* selector,
                                 InstructionCode* opcode_return, Node* node,
                                 InstructionOperand** value_return,
                                 InstructionOperand** shift_return) {
  return (
      TryMatchASR(selector, opcode_return, node, value_return, shift_return) ||
      TryMatchLSL(selector, opcode_return, node, value_return, shift_return) ||
      TryMatchLSR(selector, opcode_return, node, value_return, shift_return) ||
      TryMatchROR(selector, opcode_return, node, value_return, shift_return));
}


static inline bool TryMatchImmediateOrShift(InstructionSelector* selector,
                                            InstructionCode* opcode_return,
                                            Node* node,
                                            size_t* input_count_return,
                                            InstructionOperand** inputs) {
  ArmOperandGenerator g(selector);
  if (g.CanBeImmediate(node, *opcode_return)) {
    *opcode_return |= AddressingModeField::encode(kMode_Operand2_I);
    inputs[0] = g.UseImmediate(node);
    *input_count_return = 1;
    return true;
  }
  if (TryMatchShift(selector, opcode_return, node, &inputs[0], &inputs[1])) {
    *input_count_return = 2;
    return true;
  }
  return false;
}


static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, InstructionCode reverse_opcode,
                       FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand* inputs[5];
  size_t input_count = 0;
  InstructionOperand* outputs[2];
  size_t output_count = 0;

  if (TryMatchImmediateOrShift(selector, &opcode, m.right().node(),
                               &input_count, &inputs[1])) {
    inputs[0] = g.UseRegister(m.left().node());
    input_count++;
  } else if (TryMatchImmediateOrShift(selector, &reverse_opcode,
                                      m.left().node(), &input_count,
                                      &inputs[1])) {
    inputs[0] = g.UseRegister(m.right().node());
    opcode = reverse_opcode;
    input_count++;
  } else {
    opcode |= AddressingModeField::encode(kMode_Operand2_R);
    inputs[input_count++] = g.UseRegister(m.left().node());
    inputs[input_count++] = g.UseRegister(m.right().node());
  }

  if (cont->IsBranch()) {
    inputs[input_count++] = g.Label(cont->true_block());
    inputs[input_count++] = g.Label(cont->false_block());
  }

  outputs[output_count++] = g.DefineAsRegister(node);
  if (cont->IsSet()) {
    outputs[output_count++] = g.DefineAsRegister(cont->result());
  }

  DCHECK_NE(0, input_count);
  DCHECK_NE(0, output_count);
  DCHECK_GE(ARRAY_SIZE(inputs), input_count);
  DCHECK_GE(ARRAY_SIZE(outputs), output_count);
  DCHECK_NE(kMode_None, AddressingModeField::decode(opcode));

  Instruction* instr = selector->Emit(cont->Encode(opcode), output_count,
                                      outputs, input_count, inputs);
  if (cont->IsBranch()) instr->MarkAsControl();
}


static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, InstructionCode reverse_opcode) {
  FlagsContinuation cont;
  VisitBinop(selector, node, opcode, reverse_opcode, &cont);
}


void InstructionSelector::VisitLoad(Node* node) {
  MachineType rep = OpParameter<MachineType>(node);
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  InstructionOperand* result = rep == kMachineFloat64
                                   ? g.DefineAsDoubleRegister(node)
                                   : g.DefineAsRegister(node);

  ArchOpcode opcode;
  switch (rep) {
    case kMachineFloat64:
      opcode = kArmFloat64Load;
      break;
    case kMachineWord8:
      opcode = kArmLoadWord8;
      break;
    case kMachineWord16:
      opcode = kArmLoadWord16;
      break;
    case kMachineTagged:  // Fall through.
    case kMachineWord32:
      opcode = kArmLoadWord32;
      break;
    default:
      UNREACHABLE();
      return;
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_Offset_RI), result,
         g.UseRegister(base), g.UseImmediate(index));
  } else if (g.CanBeImmediate(base, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_Offset_RI), result,
         g.UseRegister(index), g.UseImmediate(base));
  } else {
    Emit(opcode | AddressingModeField::encode(kMode_Offset_RR), result,
         g.UseRegister(base), g.UseRegister(index));
  }
}


void InstructionSelector::VisitStore(Node* node) {
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = OpParameter<StoreRepresentation>(node);
  MachineType rep = store_rep.rep;
  if (store_rep.write_barrier_kind == kFullWriteBarrier) {
    DCHECK(rep == kMachineTagged);
    // TODO(dcarney): refactor RecordWrite function to take temp registers
    //                and pass them here instead of using fixed regs
    // TODO(dcarney): handle immediate indices.
    InstructionOperand* temps[] = {g.TempRegister(r5), g.TempRegister(r6)};
    Emit(kArmStoreWriteBarrier, NULL, g.UseFixed(base, r4),
         g.UseFixed(index, r5), g.UseFixed(value, r6), ARRAY_SIZE(temps),
         temps);
    return;
  }
  DCHECK_EQ(kNoWriteBarrier, store_rep.write_barrier_kind);
  InstructionOperand* val = rep == kMachineFloat64 ? g.UseDoubleRegister(value)
                                                   : g.UseRegister(value);

  ArchOpcode opcode;
  switch (rep) {
    case kMachineFloat64:
      opcode = kArmFloat64Store;
      break;
    case kMachineWord8:
      opcode = kArmStoreWord8;
      break;
    case kMachineWord16:
      opcode = kArmStoreWord16;
      break;
    case kMachineTagged:  // Fall through.
    case kMachineWord32:
      opcode = kArmStoreWord32;
      break;
    default:
      UNREACHABLE();
      return;
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_Offset_RI), NULL,
         g.UseRegister(base), g.UseImmediate(index), val);
  } else if (g.CanBeImmediate(base, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_Offset_RI), NULL,
         g.UseRegister(index), g.UseImmediate(base), val);
  } else {
    Emit(opcode | AddressingModeField::encode(kMode_Offset_RR), NULL,
         g.UseRegister(base), g.UseRegister(index), val);
  }
}


static inline void EmitBic(InstructionSelector* selector, Node* node,
                           Node* left, Node* right) {
  ArmOperandGenerator g(selector);
  InstructionCode opcode = kArmBic;
  InstructionOperand* value_operand;
  InstructionOperand* shift_operand;
  if (TryMatchShift(selector, &opcode, right, &value_operand, &shift_operand)) {
    selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(left),
                   value_operand, shift_operand);
    return;
  }
  selector->Emit(opcode | AddressingModeField::encode(kMode_Operand2_R),
                 g.DefineAsRegister(node), g.UseRegister(left),
                 g.UseRegister(right));
}


void InstructionSelector::VisitWord32And(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().IsWord32Xor() && CanCover(node, m.left().node())) {
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().Is(-1)) {
      EmitBic(this, node, m.right().node(), mleft.left().node());
      return;
    }
  }
  if (m.right().IsWord32Xor() && CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    if (mright.right().Is(-1)) {
      EmitBic(this, node, m.left().node(), mright.left().node());
      return;
    }
  }
  if (IsSupported(ARMv7) && m.right().HasValue()) {
    uint32_t value = m.right().Value();
    uint32_t width = CompilerIntrinsics::CountSetBits(value);
    uint32_t msb = CompilerIntrinsics::CountLeadingZeros(value);
    if (width != 0 && msb + width == 32) {
      DCHECK_EQ(0, CompilerIntrinsics::CountTrailingZeros(value));
      if (m.left().IsWord32Shr()) {
        Int32BinopMatcher mleft(m.left().node());
        if (mleft.right().IsInRange(0, 31)) {
          Emit(kArmUbfx, g.DefineAsRegister(node),
               g.UseRegister(mleft.left().node()),
               g.UseImmediate(mleft.right().node()), g.TempImmediate(width));
          return;
        }
      }
      Emit(kArmUbfx, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(0), g.TempImmediate(width));
      return;
    }
    // Try to interpret this AND as BFC.
    width = 32 - width;
    msb = CompilerIntrinsics::CountLeadingZeros(~value);
    uint32_t lsb = CompilerIntrinsics::CountTrailingZeros(~value);
    if (msb + width + lsb == 32) {
      Emit(kArmBfc, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
           g.TempImmediate(lsb), g.TempImmediate(width));
      return;
    }
  }
  VisitBinop(this, node, kArmAnd, kArmAnd);
}


void InstructionSelector::VisitWord32Or(Node* node) {
  ArmOperandGenerator g(this);
  InstructionCode opcode = kArmMov;
  InstructionOperand* value_operand;
  InstructionOperand* shift_operand;
  if (TryMatchROR(this, &opcode, node, &value_operand, &shift_operand)) {
    Emit(opcode, g.DefineAsRegister(node), value_operand, shift_operand);
    return;
  }
  VisitBinop(this, node, kArmOrr, kArmOrr);
}


void InstructionSelector::VisitWord32Xor(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.right().Is(-1)) {
    InstructionCode opcode = kArmMvn;
    InstructionOperand* value_operand;
    InstructionOperand* shift_operand;
    if (TryMatchShift(this, &opcode, m.left().node(), &value_operand,
                      &shift_operand)) {
      Emit(opcode, g.DefineAsRegister(node), value_operand, shift_operand);
      return;
    }
    Emit(opcode | AddressingModeField::encode(kMode_Operand2_R),
         g.DefineAsRegister(node), g.UseRegister(m.left().node()));
    return;
  }
  VisitBinop(this, node, kArmEor, kArmEor);
}


template <typename TryMatchShift>
static inline void VisitShift(InstructionSelector* selector, Node* node,
                              TryMatchShift try_match_shift) {
  ArmOperandGenerator g(selector);
  InstructionCode opcode = kArmMov;
  InstructionOperand* value_operand = NULL;
  InstructionOperand* shift_operand = NULL;
  CHECK(
      try_match_shift(selector, &opcode, node, &value_operand, &shift_operand));
  selector->Emit(opcode, g.DefineAsRegister(node), value_operand,
                 shift_operand);
}


void InstructionSelector::VisitWord32Shl(Node* node) {
  VisitShift(this, node, TryMatchLSL);
}


void InstructionSelector::VisitWord32Shr(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (IsSupported(ARMv7) && m.left().IsWord32And() &&
      m.right().IsInRange(0, 31)) {
    int32_t lsb = m.right().Value();
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      uint32_t value = (mleft.right().Value() >> lsb) << lsb;
      uint32_t width = CompilerIntrinsics::CountSetBits(value);
      uint32_t msb = CompilerIntrinsics::CountLeadingZeros(value);
      if (msb + width + lsb == 32) {
        DCHECK_EQ(lsb, CompilerIntrinsics::CountTrailingZeros(value));
        Emit(kArmUbfx, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(lsb),
             g.TempImmediate(width));
        return;
      }
    }
  }
  VisitShift(this, node, TryMatchLSR);
}


void InstructionSelector::VisitWord32Sar(Node* node) {
  VisitShift(this, node, TryMatchASR);
}


void InstructionSelector::VisitInt32Add(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().IsInt32Mul() && CanCover(node, m.left().node())) {
    Int32BinopMatcher mleft(m.left().node());
    Emit(kArmMla, g.DefineAsRegister(node), g.UseRegister(mleft.left().node()),
         g.UseRegister(mleft.right().node()), g.UseRegister(m.right().node()));
    return;
  }
  if (m.right().IsInt32Mul() && CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    Emit(kArmMla, g.DefineAsRegister(node), g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()), g.UseRegister(m.left().node()));
    return;
  }
  VisitBinop(this, node, kArmAdd, kArmAdd);
}


void InstructionSelector::VisitInt32Sub(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (IsSupported(MLS) && m.right().IsInt32Mul() &&
      CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    Emit(kArmMls, g.DefineAsRegister(node), g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()), g.UseRegister(m.left().node()));
    return;
  }
  VisitBinop(this, node, kArmSub, kArmRsb);
}


void InstructionSelector::VisitInt32Mul(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.right().HasValue() && m.right().Value() > 0) {
    int32_t value = m.right().Value();
    if (IsPowerOf2(value - 1)) {
      Emit(kArmAdd | AddressingModeField::encode(kMode_Operand2_R_LSL_I),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value - 1)));
      return;
    }
    if (value < kMaxInt && IsPowerOf2(value + 1)) {
      Emit(kArmRsb | AddressingModeField::encode(kMode_Operand2_R_LSL_I),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value + 1)));
      return;
    }
  }
  Emit(kArmMul, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


static void EmitDiv(InstructionSelector* selector, ArchOpcode div_opcode,
                    ArchOpcode f64i32_opcode, ArchOpcode i32f64_opcode,
                    InstructionOperand* result_operand,
                    InstructionOperand* left_operand,
                    InstructionOperand* right_operand) {
  ArmOperandGenerator g(selector);
  if (selector->IsSupported(SUDIV)) {
    selector->Emit(div_opcode, result_operand, left_operand, right_operand);
    return;
  }
  InstructionOperand* left_double_operand = g.TempDoubleRegister();
  InstructionOperand* right_double_operand = g.TempDoubleRegister();
  InstructionOperand* result_double_operand = g.TempDoubleRegister();
  selector->Emit(f64i32_opcode, left_double_operand, left_operand);
  selector->Emit(f64i32_opcode, right_double_operand, right_operand);
  selector->Emit(kArmVdivF64, result_double_operand, left_double_operand,
                 right_double_operand);
  selector->Emit(i32f64_opcode, result_operand, result_double_operand);
}


static void VisitDiv(InstructionSelector* selector, Node* node,
                     ArchOpcode div_opcode, ArchOpcode f64i32_opcode,
                     ArchOpcode i32f64_opcode) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  EmitDiv(selector, div_opcode, f64i32_opcode, i32f64_opcode,
          g.DefineAsRegister(node), g.UseRegister(m.left().node()),
          g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitInt32Div(Node* node) {
  VisitDiv(this, node, kArmSdiv, kArmVcvtF64S32, kArmVcvtS32F64);
}


void InstructionSelector::VisitInt32UDiv(Node* node) {
  VisitDiv(this, node, kArmUdiv, kArmVcvtF64U32, kArmVcvtU32F64);
}


static void VisitMod(InstructionSelector* selector, Node* node,
                     ArchOpcode div_opcode, ArchOpcode f64i32_opcode,
                     ArchOpcode i32f64_opcode) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand* div_operand = g.TempRegister();
  InstructionOperand* result_operand = g.DefineAsRegister(node);
  InstructionOperand* left_operand = g.UseRegister(m.left().node());
  InstructionOperand* right_operand = g.UseRegister(m.right().node());
  EmitDiv(selector, div_opcode, f64i32_opcode, i32f64_opcode, div_operand,
          left_operand, right_operand);
  if (selector->IsSupported(MLS)) {
    selector->Emit(kArmMls, result_operand, div_operand, right_operand,
                   left_operand);
    return;
  }
  InstructionOperand* mul_operand = g.TempRegister();
  selector->Emit(kArmMul, mul_operand, div_operand, right_operand);
  selector->Emit(kArmSub, result_operand, left_operand, mul_operand);
}


void InstructionSelector::VisitInt32Mod(Node* node) {
  VisitMod(this, node, kArmSdiv, kArmVcvtF64S32, kArmVcvtS32F64);
}


void InstructionSelector::VisitInt32UMod(Node* node) {
  VisitMod(this, node, kArmUdiv, kArmVcvtF64U32, kArmVcvtU32F64);
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVcvtF64S32, g.DefineAsDoubleRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVcvtF64U32, g.DefineAsDoubleRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVcvtS32F64, g.DefineAsRegister(node),
       g.UseDoubleRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVcvtU32F64, g.DefineAsRegister(node),
       g.UseDoubleRegister(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64Add(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().IsFloat64Mul() && CanCover(node, m.left().node())) {
    Int32BinopMatcher mleft(m.left().node());
    Emit(kArmVmlaF64, g.DefineSameAsFirst(node),
         g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
         g.UseRegister(mleft.right().node()));
    return;
  }
  if (m.right().IsFloat64Mul() && CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    Emit(kArmVmlaF64, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()));
    return;
  }
  VisitRRRFloat64(this, kArmVaddF64, node);
}


void InstructionSelector::VisitFloat64Sub(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.right().IsFloat64Mul() && CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    Emit(kArmVmlsF64, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()));
    return;
  }
  VisitRRRFloat64(this, kArmVsubF64, node);
}


void InstructionSelector::VisitFloat64Mul(Node* node) {
  ArmOperandGenerator g(this);
  Float64BinopMatcher m(node);
  if (m.right().Is(-1.0)) {
    Emit(kArmVnegF64, g.DefineAsRegister(node),
         g.UseDoubleRegister(m.left().node()));
  } else {
    VisitRRRFloat64(this, kArmVmulF64, node);
  }
}


void InstructionSelector::VisitFloat64Div(Node* node) {
  VisitRRRFloat64(this, kArmVdivF64, node);
}


void InstructionSelector::VisitFloat64Mod(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVmodF64, g.DefineAsFixedDouble(node, d0),
       g.UseFixedDouble(node->InputAt(0), d0),
       g.UseFixedDouble(node->InputAt(1), d1))->MarkAsCall();
}


void InstructionSelector::VisitCall(Node* call, BasicBlock* continuation,
                                    BasicBlock* deoptimization) {
  ArmOperandGenerator g(this);
  CallDescriptor* descriptor = OpParameter<CallDescriptor*>(call);
  CallBuffer buffer(zone(), descriptor);  // TODO(turbofan): temp zone here?

  // Compute InstructionOperands for inputs and outputs.
  // TODO(turbofan): on ARM64 it's probably better to use the code object in a
  // register if there are multiple uses of it. Improve constant pool and the
  // heuristics in the register allocator for where to emit constants.
  InitializeCallBuffer(call, &buffer, true, false, continuation,
                       deoptimization);

  // TODO(dcarney): might be possible to use claim/poke instead
  // Push any stack arguments.
  for (int i = buffer.pushed_count - 1; i >= 0; --i) {
    Node* input = buffer.pushed_nodes[i];
    Emit(kArmPush, NULL, g.UseRegister(input));
  }

  // Select the appropriate opcode based on the call type.
  InstructionCode opcode;
  switch (descriptor->kind()) {
    case CallDescriptor::kCallCodeObject: {
      bool lazy_deopt = descriptor->CanLazilyDeoptimize();
      opcode = kArmCallCodeObject | MiscField::encode(lazy_deopt ? 1 : 0);
      break;
    }
    case CallDescriptor::kCallAddress:
      opcode = kArmCallAddress;
      break;
    case CallDescriptor::kCallJSFunction:
      opcode = kArmCallJSFunction;
      break;
    default:
      UNREACHABLE();
      return;
  }

  // Emit the call instruction.
  Instruction* call_instr =
      Emit(opcode, buffer.output_count, buffer.outputs,
           buffer.fixed_and_control_count(), buffer.fixed_and_control_args);

  call_instr->MarkAsCall();
  if (deoptimization != NULL) {
    DCHECK(continuation != NULL);
    call_instr->MarkAsControl();
  }

  // Caller clean up of stack for C-style calls.
  if (descriptor->kind() == CallDescriptor::kCallAddress &&
      buffer.pushed_count > 0) {
    DCHECK(deoptimization == NULL && continuation == NULL);
    Emit(kArmDrop | MiscField::encode(buffer.pushed_count), NULL);
  }
}


void InstructionSelector::VisitInt32AddWithOverflow(Node* node,
                                                    FlagsContinuation* cont) {
  VisitBinop(this, node, kArmAdd, kArmAdd, cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node,
                                                    FlagsContinuation* cont) {
  VisitBinop(this, node, kArmSub, kArmRsb, cont);
}


// Shared routine for multiple compare operations.
static void VisitWordCompare(InstructionSelector* selector, Node* node,
                             InstructionCode opcode, FlagsContinuation* cont,
                             bool commutative) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand* inputs[5];
  size_t input_count = 0;
  InstructionOperand* outputs[1];
  size_t output_count = 0;

  if (TryMatchImmediateOrShift(selector, &opcode, m.right().node(),
                               &input_count, &inputs[1])) {
    inputs[0] = g.UseRegister(m.left().node());
    input_count++;
  } else if (TryMatchImmediateOrShift(selector, &opcode, m.left().node(),
                                      &input_count, &inputs[1])) {
    if (!commutative) cont->Commute();
    inputs[0] = g.UseRegister(m.right().node());
    input_count++;
  } else {
    opcode |= AddressingModeField::encode(kMode_Operand2_R);
    inputs[input_count++] = g.UseRegister(m.left().node());
    inputs[input_count++] = g.UseRegister(m.right().node());
  }

  if (cont->IsBranch()) {
    inputs[input_count++] = g.Label(cont->true_block());
    inputs[input_count++] = g.Label(cont->false_block());
  } else {
    DCHECK(cont->IsSet());
    outputs[output_count++] = g.DefineAsRegister(cont->result());
  }

  DCHECK_NE(0, input_count);
  DCHECK_GE(ARRAY_SIZE(inputs), input_count);
  DCHECK_GE(ARRAY_SIZE(outputs), output_count);

  Instruction* instr = selector->Emit(cont->Encode(opcode), output_count,
                                      outputs, input_count, inputs);
  if (cont->IsBranch()) instr->MarkAsControl();
}


void InstructionSelector::VisitWord32Test(Node* node, FlagsContinuation* cont) {
  switch (node->opcode()) {
    case IrOpcode::kInt32Add:
      return VisitWordCompare(this, node, kArmCmn, cont, true);
    case IrOpcode::kInt32Sub:
      return VisitWordCompare(this, node, kArmCmp, cont, false);
    case IrOpcode::kWord32And:
      return VisitWordCompare(this, node, kArmTst, cont, true);
    case IrOpcode::kWord32Or:
      return VisitBinop(this, node, kArmOrr, kArmOrr, cont);
    case IrOpcode::kWord32Xor:
      return VisitWordCompare(this, node, kArmTeq, cont, true);
    default:
      break;
  }

  ArmOperandGenerator g(this);
  InstructionCode opcode =
      cont->Encode(kArmTst) | AddressingModeField::encode(kMode_Operand2_R);
  if (cont->IsBranch()) {
    Emit(opcode, NULL, g.UseRegister(node), g.UseRegister(node),
         g.Label(cont->true_block()),
         g.Label(cont->false_block()))->MarkAsControl();
  } else {
    Emit(opcode, g.DefineAsRegister(cont->result()), g.UseRegister(node),
         g.UseRegister(node));
  }
}


void InstructionSelector::VisitWord32Compare(Node* node,
                                             FlagsContinuation* cont) {
  VisitWordCompare(this, node, kArmCmp, cont, false);
}


void InstructionSelector::VisitFloat64Compare(Node* node,
                                              FlagsContinuation* cont) {
  ArmOperandGenerator g(this);
  Float64BinopMatcher m(node);
  if (cont->IsBranch()) {
    Emit(cont->Encode(kArmVcmpF64), NULL, g.UseDoubleRegister(m.left().node()),
         g.UseDoubleRegister(m.right().node()), g.Label(cont->true_block()),
         g.Label(cont->false_block()))->MarkAsControl();
  } else {
    DCHECK(cont->IsSet());
    Emit(cont->Encode(kArmVcmpF64), g.DefineAsRegister(cont->result()),
         g.UseDoubleRegister(m.left().node()),
         g.UseDoubleRegister(m.right().node()));
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
