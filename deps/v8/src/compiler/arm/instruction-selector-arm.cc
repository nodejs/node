// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Adds Arm-specific methods for generating InstructionOperands.
class ArmOperandGenerator : public OperandGenerator {
 public:
  explicit ArmOperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand* UseOperand(Node* node, InstructionCode opcode) {
    if (CanBeImmediate(node, opcode)) {
      return UseImmediate(node);
    }
    return UseRegister(node);
  }

  bool CanBeImmediate(int32_t value) const {
    return Assembler::ImmediateFitsAddrMode1Instruction(value);
  }

  bool CanBeImmediate(uint32_t value) const {
    return CanBeImmediate(bit_cast<int32_t>(value));
  }

  bool CanBeImmediate(Node* node, InstructionCode opcode) {
    Int32Matcher m(node);
    if (!m.HasValue()) return false;
    int32_t value = m.Value();
    switch (ArchOpcodeField::decode(opcode)) {
      case kArmAnd:
      case kArmMov:
      case kArmMvn:
      case kArmBic:
        return CanBeImmediate(value) || CanBeImmediate(~value);

      case kArmAdd:
      case kArmSub:
      case kArmCmp:
      case kArmCmn:
        return CanBeImmediate(value) || CanBeImmediate(-value);

      case kArmTst:
      case kArmTeq:
      case kArmOrr:
      case kArmEor:
      case kArmRsb:
        return CanBeImmediate(value);

      case kArmVldrF32:
      case kArmVstrF32:
      case kArmVldrF64:
      case kArmVstrF64:
        return value >= -1020 && value <= 1020 && (value % 4) == 0;

      case kArmLdrb:
      case kArmLdrsb:
      case kArmStrb:
      case kArmLdr:
      case kArmStr:
      case kArmStoreWriteBarrier:
        return value >= -4095 && value <= 4095;

      case kArmLdrh:
      case kArmLdrsh:
      case kArmStrh:
        return value >= -255 && value <= 255;

      case kArchCallCodeObject:
      case kArchCallJSFunction:
      case kArchJmp:
      case kArchNop:
      case kArchRet:
      case kArchStackPointer:
      case kArchTruncateDoubleToI:
      case kArmMul:
      case kArmMla:
      case kArmMls:
      case kArmSmmul:
      case kArmSmmla:
      case kArmUmull:
      case kArmSdiv:
      case kArmUdiv:
      case kArmBfc:
      case kArmUbfx:
      case kArmVcmpF64:
      case kArmVaddF64:
      case kArmVsubF64:
      case kArmVmulF64:
      case kArmVmlaF64:
      case kArmVmlsF64:
      case kArmVdivF64:
      case kArmVmodF64:
      case kArmVnegF64:
      case kArmVsqrtF64:
      case kArmVfloorF64:
      case kArmVceilF64:
      case kArmVroundTruncateF64:
      case kArmVroundTiesAwayF64:
      case kArmVcvtF32F64:
      case kArmVcvtF64F32:
      case kArmVcvtF64S32:
      case kArmVcvtF64U32:
      case kArmVcvtS32F64:
      case kArmVcvtU32F64:
      case kArmPush:
        return false;
    }
    UNREACHABLE();
    return false;
  }
};


static void VisitRRFloat64(InstructionSelector* selector, ArchOpcode opcode,
                           Node* node) {
  ArmOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}


static void VisitRRRFloat64(InstructionSelector* selector, ArchOpcode opcode,
                            Node* node) {
  ArmOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
}


static bool TryMatchROR(InstructionSelector* selector,
                        InstructionCode* opcode_return, Node* node,
                        InstructionOperand** value_return,
                        InstructionOperand** shift_return) {
  ArmOperandGenerator g(selector);
  if (node->opcode() != IrOpcode::kWord32Ror) return false;
  Int32BinopMatcher m(node);
  *value_return = g.UseRegister(m.left().node());
  if (m.right().IsInRange(1, 31)) {
    *opcode_return |= AddressingModeField::encode(kMode_Operand2_R_ROR_I);
    *shift_return = g.UseImmediate(m.right().node());
  } else {
    *opcode_return |= AddressingModeField::encode(kMode_Operand2_R_ROR_R);
    *shift_return = g.UseRegister(m.right().node());
  }
  return true;
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
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);
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
  MachineType rep = RepresentationOf(OpParameter<LoadRepresentation>(node));
  MachineType typ = TypeOf(OpParameter<LoadRepresentation>(node));
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  ArchOpcode opcode;
  switch (rep) {
    case kRepFloat32:
      opcode = kArmVldrF32;
      break;
    case kRepFloat64:
      opcode = kArmVldrF64;
      break;
    case kRepBit:  // Fall through.
    case kRepWord8:
      opcode = typ == kTypeUint32 ? kArmLdrb : kArmLdrsb;
      break;
    case kRepWord16:
      opcode = typ == kTypeUint32 ? kArmLdrh : kArmLdrsh;
      break;
    case kRepTagged:  // Fall through.
    case kRepWord32:
      opcode = kArmLdr;
      break;
    default:
      UNREACHABLE();
      return;
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_Offset_RI),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseImmediate(index));
  } else {
    Emit(opcode | AddressingModeField::encode(kMode_Offset_RR),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseRegister(index));
  }
}


void InstructionSelector::VisitStore(Node* node) {
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = OpParameter<StoreRepresentation>(node);
  MachineType rep = RepresentationOf(store_rep.machine_type());
  if (store_rep.write_barrier_kind() == kFullWriteBarrier) {
    DCHECK(rep == kRepTagged);
    // TODO(dcarney): refactor RecordWrite function to take temp registers
    //                and pass them here instead of using fixed regs
    // TODO(dcarney): handle immediate indices.
    InstructionOperand* temps[] = {g.TempRegister(r5), g.TempRegister(r6)};
    Emit(kArmStoreWriteBarrier, NULL, g.UseFixed(base, r4),
         g.UseFixed(index, r5), g.UseFixed(value, r6), arraysize(temps),
         temps);
    return;
  }
  DCHECK_EQ(kNoWriteBarrier, store_rep.write_barrier_kind());

  ArchOpcode opcode;
  switch (rep) {
    case kRepFloat32:
      opcode = kArmVstrF32;
      break;
    case kRepFloat64:
      opcode = kArmVstrF64;
      break;
    case kRepBit:  // Fall through.
    case kRepWord8:
      opcode = kArmStrb;
      break;
    case kRepWord16:
      opcode = kArmStrh;
      break;
    case kRepTagged:  // Fall through.
    case kRepWord32:
      opcode = kArmStr;
      break;
    default:
      UNREACHABLE();
      return;
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_Offset_RI), NULL,
         g.UseRegister(base), g.UseImmediate(index), g.UseRegister(value));
  } else {
    Emit(opcode | AddressingModeField::encode(kMode_Offset_RR), NULL,
         g.UseRegister(base), g.UseRegister(index), g.UseRegister(value));
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
    // Try to interpret this AND as UBFX.
    uint32_t const value = m.right().Value();
    uint32_t width = base::bits::CountPopulation32(value);
    uint32_t msb = base::bits::CountLeadingZeros32(value);
    if (width != 0 && msb + width == 32) {
      DCHECK_EQ(0, base::bits::CountTrailingZeros32(value));
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

    // Try to interpret this AND as BIC.
    if (g.CanBeImmediate(~value)) {
      Emit(kArmBic | AddressingModeField::encode(kMode_Operand2_I),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(~value));
      return;
    }

    // Try to interpret this AND as BFC.
    width = 32 - width;
    msb = base::bits::CountLeadingZeros32(~value);
    uint32_t lsb = base::bits::CountTrailingZeros32(~value);
    if (msb + width + lsb == 32) {
      Emit(kArmBfc, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
           g.TempImmediate(lsb), g.TempImmediate(width));
      return;
    }
  }
  VisitBinop(this, node, kArmAnd, kArmAnd);
}


void InstructionSelector::VisitWord32Or(Node* node) {
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
                              TryMatchShift try_match_shift,
                              FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  InstructionCode opcode = kArmMov;
  InstructionOperand* inputs[4];
  size_t input_count = 2;
  InstructionOperand* outputs[2];
  size_t output_count = 0;

  CHECK(try_match_shift(selector, &opcode, node, &inputs[0], &inputs[1]));

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
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);
  DCHECK_NE(kMode_None, AddressingModeField::decode(opcode));

  Instruction* instr = selector->Emit(cont->Encode(opcode), output_count,
                                      outputs, input_count, inputs);
  if (cont->IsBranch()) instr->MarkAsControl();
}


template <typename TryMatchShift>
static inline void VisitShift(InstructionSelector* selector, Node* node,
                              TryMatchShift try_match_shift) {
  FlagsContinuation cont;
  VisitShift(selector, node, try_match_shift, &cont);
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
      uint32_t width = base::bits::CountPopulation32(value);
      uint32_t msb = base::bits::CountLeadingZeros32(value);
      if (msb + width + lsb == 32) {
        DCHECK_EQ(lsb, base::bits::CountTrailingZeros32(value));
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


void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitShift(this, node, TryMatchROR);
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
  if (m.left().IsInt32MulHigh() && CanCover(node, m.left().node())) {
    Int32BinopMatcher mleft(m.left().node());
    Emit(kArmSmmla, g.DefineAsRegister(node),
         g.UseRegister(mleft.left().node()),
         g.UseRegister(mleft.right().node()), g.UseRegister(m.right().node()));
    return;
  }
  if (m.right().IsInt32MulHigh() && CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    Emit(kArmSmmla, g.DefineAsRegister(node),
         g.UseRegister(mright.left().node()),
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
    if (base::bits::IsPowerOfTwo32(value - 1)) {
      Emit(kArmAdd | AddressingModeField::encode(kMode_Operand2_R_LSL_I),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value - 1)));
      return;
    }
    if (value < kMaxInt && base::bits::IsPowerOfTwo32(value + 1)) {
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


void InstructionSelector::VisitInt32MulHigh(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmSmmul, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)),
       g.UseRegister(node->InputAt(1)));
}


void InstructionSelector::VisitUint32MulHigh(Node* node) {
  ArmOperandGenerator g(this);
  InstructionOperand* outputs[] = {g.TempRegister(), g.DefineAsRegister(node)};
  InstructionOperand* inputs[] = {g.UseRegister(node->InputAt(0)),
                                  g.UseRegister(node->InputAt(1))};
  Emit(kArmUmull, arraysize(outputs), outputs, arraysize(inputs), inputs);
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


void InstructionSelector::VisitUint32Div(Node* node) {
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


void InstructionSelector::VisitUint32Mod(Node* node) {
  VisitMod(this, node, kArmUdiv, kArmVcvtF64U32, kArmVcvtU32F64);
}


void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVcvtF64F32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVcvtF64S32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVcvtF64U32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVcvtS32F64, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVcvtU32F64, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVcvtF32F64, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
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
    Emit(kArmVnegF64, g.DefineAsRegister(node), g.UseRegister(m.left().node()));
  } else {
    VisitRRRFloat64(this, kArmVmulF64, node);
  }
}


void InstructionSelector::VisitFloat64Div(Node* node) {
  VisitRRRFloat64(this, kArmVdivF64, node);
}


void InstructionSelector::VisitFloat64Mod(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVmodF64, g.DefineAsFixed(node, d0), g.UseFixed(node->InputAt(0), d0),
       g.UseFixed(node->InputAt(1), d1))->MarkAsCall();
}


void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVsqrtF64, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64Floor(Node* node) {
  DCHECK(CpuFeatures::IsSupported(ARMv8));
  VisitRRFloat64(this, kArmVfloorF64, node);
}


void InstructionSelector::VisitFloat64Ceil(Node* node) {
  DCHECK(CpuFeatures::IsSupported(ARMv8));
  VisitRRFloat64(this, kArmVceilF64, node);
}


void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  DCHECK(CpuFeatures::IsSupported(ARMv8));
  VisitRRFloat64(this, kArmVroundTruncateF64, node);
}


void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  DCHECK(CpuFeatures::IsSupported(ARMv8));
  VisitRRFloat64(this, kArmVroundTiesAwayF64, node);
}


void InstructionSelector::VisitCall(Node* node) {
  ArmOperandGenerator g(this);
  CallDescriptor* descriptor = OpParameter<CallDescriptor*>(node);

  FrameStateDescriptor* frame_state_descriptor = NULL;
  if (descriptor->NeedsFrameState()) {
    frame_state_descriptor =
        GetFrameStateDescriptor(node->InputAt(descriptor->InputCount()));
  }

  CallBuffer buffer(zone(), descriptor, frame_state_descriptor);

  // Compute InstructionOperands for inputs and outputs.
  // TODO(turbofan): on ARM64 it's probably better to use the code object in a
  // register if there are multiple uses of it. Improve constant pool and the
  // heuristics in the register allocator for where to emit constants.
  InitializeCallBuffer(node, &buffer, true, false);

  // TODO(dcarney): might be possible to use claim/poke instead
  // Push any stack arguments.
  for (NodeVectorRIter input = buffer.pushed_nodes.rbegin();
       input != buffer.pushed_nodes.rend(); input++) {
    Emit(kArmPush, NULL, g.UseRegister(*input));
  }

  // Select the appropriate opcode based on the call type.
  InstructionCode opcode;
  switch (descriptor->kind()) {
    case CallDescriptor::kCallCodeObject: {
      opcode = kArchCallCodeObject;
      break;
    }
    case CallDescriptor::kCallJSFunction:
      opcode = kArchCallJSFunction;
      break;
    default:
      UNREACHABLE();
      return;
  }
  opcode |= MiscField::encode(descriptor->flags());

  // Emit the call instruction.
  InstructionOperand** first_output =
      buffer.outputs.size() > 0 ? &buffer.outputs.front() : NULL;
  Instruction* call_instr =
      Emit(opcode, buffer.outputs.size(), first_output,
           buffer.instruction_args.size(), &buffer.instruction_args.front());
  call_instr->MarkAsCall();
}


namespace {

// Shared routine for multiple float compare operations.
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  Float64BinopMatcher m(node);
  if (cont->IsBranch()) {
    selector->Emit(cont->Encode(kArmVcmpF64), nullptr,
                   g.UseRegister(m.left().node()),
                   g.UseRegister(m.right().node()), g.Label(cont->true_block()),
                   g.Label(cont->false_block()))->MarkAsControl();
  } else {
    DCHECK(cont->IsSet());
    selector->Emit(
        cont->Encode(kArmVcmpF64), g.DefineAsRegister(cont->result()),
        g.UseRegister(m.left().node()), g.UseRegister(m.right().node()));
  }
}


// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont) {
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
    if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
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
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  Instruction* instr = selector->Emit(cont->Encode(opcode), output_count,
                                      outputs, input_count, inputs);
  if (cont->IsBranch()) instr->MarkAsControl();
}


void VisitWordCompare(InstructionSelector* selector, Node* node,
                      FlagsContinuation* cont) {
  VisitWordCompare(selector, node, kArmCmp, cont);
}


// Shared routine for word comparisons against zero.
void VisitWordCompareZero(InstructionSelector* selector, Node* user,
                          Node* value, FlagsContinuation* cont) {
  while (selector->CanCover(user, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord32Equal: {
        // Combine with comparisons against 0 by simply inverting the
        // continuation.
        Int32BinopMatcher m(value);
        if (m.right().Is(0)) {
          user = value;
          value = m.left().node();
          cont->Negate();
          continue;
        }
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitWordCompare(selector, value, cont);
      }
      case IrOpcode::kInt32LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWordCompare(selector, value, cont);
      case IrOpcode::kInt32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWordCompare(selector, value, cont);
      case IrOpcode::kUint32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWordCompare(selector, value, cont);
      case IrOpcode::kUint32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWordCompare(selector, value, cont);
      case IrOpcode::kFloat64Equal:
        cont->OverwriteAndNegateIfEqual(kUnorderedEqual);
        return VisitFloat64Compare(selector, value, cont);
      case IrOpcode::kFloat64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnorderedLessThan);
        return VisitFloat64Compare(selector, value, cont);
      case IrOpcode::kFloat64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnorderedLessThanOrEqual);
        return VisitFloat64Compare(selector, value, cont);
      case IrOpcode::kProjection:
        // Check if this is the overflow output projection of an
        // <Operation>WithOverflow node.
        if (OpParameter<size_t>(value) == 1u) {
          // We cannot combine the <Operation>WithOverflow with this branch
          // unless the 0th projection (the use of the actual value of the
          // <Operation> is either NULL, which means there's no use of the
          // actual value, or was already defined, which means it is scheduled
          // *AFTER* this branch).
          Node* const node = value->InputAt(0);
          Node* const result = node->FindProjection(0);
          if (!result || selector->IsDefined(result)) {
            switch (node->opcode()) {
              case IrOpcode::kInt32AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kArmAdd, kArmAdd, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kArmSub, kArmRsb, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Add:
        return VisitWordCompare(selector, value, kArmCmn, cont);
      case IrOpcode::kInt32Sub:
        return VisitWordCompare(selector, value, kArmCmp, cont);
      case IrOpcode::kWord32And:
        return VisitWordCompare(selector, value, kArmTst, cont);
      case IrOpcode::kWord32Or:
        return VisitBinop(selector, value, kArmOrr, kArmOrr, cont);
      case IrOpcode::kWord32Xor:
        return VisitWordCompare(selector, value, kArmTeq, cont);
      case IrOpcode::kWord32Sar:
        return VisitShift(selector, value, TryMatchASR, cont);
      case IrOpcode::kWord32Shl:
        return VisitShift(selector, value, TryMatchLSL, cont);
      case IrOpcode::kWord32Shr:
        return VisitShift(selector, value, TryMatchLSR, cont);
      case IrOpcode::kWord32Ror:
        return VisitShift(selector, value, TryMatchROR, cont);
      default:
        break;
    }
    break;
  }

  // Continuation could not be combined with a compare, emit compare against 0.
  ArmOperandGenerator g(selector);
  InstructionCode const opcode =
      cont->Encode(kArmTst) | AddressingModeField::encode(kMode_Operand2_R);
  InstructionOperand* const value_operand = g.UseRegister(value);
  if (cont->IsBranch()) {
    selector->Emit(opcode, nullptr, value_operand, value_operand,
                   g.Label(cont->true_block()),
                   g.Label(cont->false_block()))->MarkAsControl();
  } else {
    selector->Emit(opcode, g.DefineAsRegister(cont->result()), value_operand,
                   value_operand);
  }
}

}  // namespace


void InstructionSelector::VisitBranch(Node* branch, BasicBlock* tbranch,
                                      BasicBlock* fbranch) {
  FlagsContinuation cont(kNotEqual, tbranch, fbranch);
  if (IsNextInAssemblyOrder(tbranch)) {  // We can fallthru to the true block.
    cont.Negate();
    cont.SwapBlocks();
  }
  VisitWordCompareZero(this, branch, branch->InputAt(0), &cont);
}


void InstructionSelector::VisitWord32Equal(Node* const node) {
  FlagsContinuation cont(kEqual, node);
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) {
    return VisitWordCompareZero(this, m.node(), m.left().node(), &cont);
  }
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitInt32LessThan(Node* node) {
  FlagsContinuation cont(kSignedLessThan, node);
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitInt32LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitUint32LessThan(Node* node) {
  FlagsContinuation cont(kUnsignedLessThan, node);
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitUint32LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kUnsignedLessThanOrEqual, node);
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitInt32AddWithOverflow(Node* node) {
  if (Node* ovf = node->FindProjection(1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitBinop(this, node, kArmAdd, kArmAdd, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kArmAdd, kArmAdd, &cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = node->FindProjection(1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitBinop(this, node, kArmSub, kArmRsb, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kArmSub, kArmRsb, &cont);
}


void InstructionSelector::VisitFloat64Equal(Node* node) {
  FlagsContinuation cont(kUnorderedEqual, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64LessThan(Node* node) {
  FlagsContinuation cont(kUnorderedLessThan, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kUnorderedLessThanOrEqual, node);
  VisitFloat64Compare(this, node, &cont);
}


// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags =
      MachineOperatorBuilder::kInt32DivIsSafe |
      MachineOperatorBuilder::kInt32ModIsSafe |
      MachineOperatorBuilder::kUint32DivIsSafe |
      MachineOperatorBuilder::kUint32ModIsSafe;

  if (CpuFeatures::IsSupported(ARMv8)) {
    flags |= MachineOperatorBuilder::kFloat64Floor |
             MachineOperatorBuilder::kFloat64Ceil |
             MachineOperatorBuilder::kFloat64RoundTruncate |
             MachineOperatorBuilder::kFloat64RoundTiesAway;
  }
  return flags;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
