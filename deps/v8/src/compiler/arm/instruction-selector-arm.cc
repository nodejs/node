// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/adapters.h"
#include "src/base/bits.h"
#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

// Adds Arm-specific methods for generating InstructionOperands.
class ArmOperandGenerator : public OperandGenerator {
 public:
  explicit ArmOperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

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
        return value >= -4095 && value <= 4095;

      case kArmLdrh:
      case kArmLdrsh:
      case kArmStrh:
        return value >= -255 && value <= 255;

      default:
        break;
    }
    return false;
  }
};


namespace {

void VisitRR(InstructionSelector* selector, ArchOpcode opcode, Node* node) {
  ArmOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}


void VisitRRR(InstructionSelector* selector, ArchOpcode opcode, Node* node) {
  ArmOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
}


template <IrOpcode::Value kOpcode, int kImmMin, int kImmMax,
          AddressingMode kImmMode, AddressingMode kRegMode>
bool TryMatchShift(InstructionSelector* selector,
                   InstructionCode* opcode_return, Node* node,
                   InstructionOperand* value_return,
                   InstructionOperand* shift_return) {
  ArmOperandGenerator g(selector);
  if (node->opcode() == kOpcode) {
    Int32BinopMatcher m(node);
    *value_return = g.UseRegister(m.left().node());
    if (m.right().IsInRange(kImmMin, kImmMax)) {
      *opcode_return |= AddressingModeField::encode(kImmMode);
      *shift_return = g.UseImmediate(m.right().node());
    } else {
      *opcode_return |= AddressingModeField::encode(kRegMode);
      *shift_return = g.UseRegister(m.right().node());
    }
    return true;
  }
  return false;
}


bool TryMatchROR(InstructionSelector* selector, InstructionCode* opcode_return,
                 Node* node, InstructionOperand* value_return,
                 InstructionOperand* shift_return) {
  return TryMatchShift<IrOpcode::kWord32Ror, 1, 31, kMode_Operand2_R_ROR_I,
                       kMode_Operand2_R_ROR_R>(selector, opcode_return, node,
                                               value_return, shift_return);
}


bool TryMatchASR(InstructionSelector* selector, InstructionCode* opcode_return,
                 Node* node, InstructionOperand* value_return,
                 InstructionOperand* shift_return) {
  return TryMatchShift<IrOpcode::kWord32Sar, 1, 32, kMode_Operand2_R_ASR_I,
                       kMode_Operand2_R_ASR_R>(selector, opcode_return, node,
                                               value_return, shift_return);
}


bool TryMatchLSL(InstructionSelector* selector, InstructionCode* opcode_return,
                 Node* node, InstructionOperand* value_return,
                 InstructionOperand* shift_return) {
  return TryMatchShift<IrOpcode::kWord32Shl, 0, 31, kMode_Operand2_R_LSL_I,
                       kMode_Operand2_R_LSL_R>(selector, opcode_return, node,
                                               value_return, shift_return);
}


bool TryMatchLSR(InstructionSelector* selector, InstructionCode* opcode_return,
                 Node* node, InstructionOperand* value_return,
                 InstructionOperand* shift_return) {
  return TryMatchShift<IrOpcode::kWord32Shr, 1, 32, kMode_Operand2_R_LSR_I,
                       kMode_Operand2_R_LSR_R>(selector, opcode_return, node,
                                               value_return, shift_return);
}


bool TryMatchShift(InstructionSelector* selector,
                   InstructionCode* opcode_return, Node* node,
                   InstructionOperand* value_return,
                   InstructionOperand* shift_return) {
  return (
      TryMatchASR(selector, opcode_return, node, value_return, shift_return) ||
      TryMatchLSL(selector, opcode_return, node, value_return, shift_return) ||
      TryMatchLSR(selector, opcode_return, node, value_return, shift_return) ||
      TryMatchROR(selector, opcode_return, node, value_return, shift_return));
}


bool TryMatchImmediateOrShift(InstructionSelector* selector,
                              InstructionCode* opcode_return, Node* node,
                              size_t* input_count_return,
                              InstructionOperand* inputs) {
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


void VisitBinop(InstructionSelector* selector, Node* node,
                InstructionCode opcode, InstructionCode reverse_opcode,
                FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand inputs[5];
  size_t input_count = 0;
  InstructionOperand outputs[2];
  size_t output_count = 0;

  if (m.left().node() == m.right().node()) {
    // If both inputs refer to the same operand, enforce allocating a register
    // for both of them to ensure that we don't end up generating code like
    // this:
    //
    //   mov r0, r1, asr #16
    //   adds r0, r0, r1, asr #16
    //   bvs label
    InstructionOperand const input = g.UseRegister(m.left().node());
    opcode |= AddressingModeField::encode(kMode_Operand2_R);
    inputs[input_count++] = input;
    inputs[input_count++] = input;
  } else if (TryMatchImmediateOrShift(selector, &opcode, m.right().node(),
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

  DCHECK_NE(0u, input_count);
  DCHECK_NE(0u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);
  DCHECK_NE(kMode_None, AddressingModeField::decode(opcode));

  selector->Emit(cont->Encode(opcode), output_count, outputs, input_count,
                 inputs);
}


void VisitBinop(InstructionSelector* selector, Node* node,
                InstructionCode opcode, InstructionCode reverse_opcode) {
  FlagsContinuation cont;
  VisitBinop(selector, node, opcode, reverse_opcode, &cont);
}


void EmitDiv(InstructionSelector* selector, ArchOpcode div_opcode,
             ArchOpcode f64i32_opcode, ArchOpcode i32f64_opcode,
             InstructionOperand result_operand, InstructionOperand left_operand,
             InstructionOperand right_operand) {
  ArmOperandGenerator g(selector);
  if (selector->IsSupported(SUDIV)) {
    selector->Emit(div_opcode, result_operand, left_operand, right_operand);
    return;
  }
  InstructionOperand left_double_operand = g.TempDoubleRegister();
  InstructionOperand right_double_operand = g.TempDoubleRegister();
  InstructionOperand result_double_operand = g.TempDoubleRegister();
  selector->Emit(f64i32_opcode, left_double_operand, left_operand);
  selector->Emit(f64i32_opcode, right_double_operand, right_operand);
  selector->Emit(kArmVdivF64, result_double_operand, left_double_operand,
                 right_double_operand);
  selector->Emit(i32f64_opcode, result_operand, result_double_operand);
}


void VisitDiv(InstructionSelector* selector, Node* node, ArchOpcode div_opcode,
              ArchOpcode f64i32_opcode, ArchOpcode i32f64_opcode) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  EmitDiv(selector, div_opcode, f64i32_opcode, i32f64_opcode,
          g.DefineAsRegister(node), g.UseRegister(m.left().node()),
          g.UseRegister(m.right().node()));
}


void VisitMod(InstructionSelector* selector, Node* node, ArchOpcode div_opcode,
              ArchOpcode f64i32_opcode, ArchOpcode i32f64_opcode) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand div_operand = g.TempRegister();
  InstructionOperand result_operand = g.DefineAsRegister(node);
  InstructionOperand left_operand = g.UseRegister(m.left().node());
  InstructionOperand right_operand = g.UseRegister(m.right().node());
  EmitDiv(selector, div_opcode, f64i32_opcode, i32f64_opcode, div_operand,
          left_operand, right_operand);
  if (selector->IsSupported(MLS)) {
    selector->Emit(kArmMls, result_operand, div_operand, right_operand,
                   left_operand);
  } else {
    InstructionOperand mul_operand = g.TempRegister();
    selector->Emit(kArmMul, mul_operand, div_operand, right_operand);
    selector->Emit(kArmSub, result_operand, left_operand, mul_operand);
  }
}

}  // namespace


void InstructionSelector::VisitLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  ArchOpcode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kArmVldrF32;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kArmVldrF64;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsUnsigned() ? kArmLdrb : kArmLdrsb;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsUnsigned() ? kArmLdrh : kArmLdrsh;
      break;
    case MachineRepresentation::kTagged:  // Fall through.
    case MachineRepresentation::kWord32:
      opcode = kArmLdr;
      break;
    case MachineRepresentation::kNone:  // Fall through.
    case MachineRepresentation::kWord64:
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

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());
  WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();
  MachineRepresentation rep = store_rep.representation();

  if (write_barrier_kind != kNoWriteBarrier) {
    DCHECK_EQ(MachineRepresentation::kTagged, rep);
    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    inputs[input_count++] = g.UseUniqueRegister(index);
    inputs[input_count++] = (write_barrier_kind == kMapWriteBarrier)
                                ? g.UseRegister(value)
                                : g.UseUniqueRegister(value);
    RecordWriteMode record_write_mode = RecordWriteMode::kValueIsAny;
    switch (write_barrier_kind) {
      case kNoWriteBarrier:
        UNREACHABLE();
        break;
      case kMapWriteBarrier:
        record_write_mode = RecordWriteMode::kValueIsMap;
        break;
      case kPointerWriteBarrier:
        record_write_mode = RecordWriteMode::kValueIsPointer;
        break;
      case kFullWriteBarrier:
        record_write_mode = RecordWriteMode::kValueIsAny;
        break;
    }
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    size_t const temp_count = arraysize(temps);
    InstructionCode code = kArchStoreWithWriteBarrier;
    code |= MiscField::encode(static_cast<int>(record_write_mode));
    Emit(code, 0, nullptr, input_count, inputs, temp_count, temps);
  } else {
    ArchOpcode opcode = kArchNop;
    switch (rep) {
      case MachineRepresentation::kFloat32:
        opcode = kArmVstrF32;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kArmVstrF64;
        break;
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = kArmStrb;
        break;
      case MachineRepresentation::kWord16:
        opcode = kArmStrh;
        break;
      case MachineRepresentation::kTagged:  // Fall through.
      case MachineRepresentation::kWord32:
        opcode = kArmStr;
        break;
      case MachineRepresentation::kNone:  // Fall through.
      case MachineRepresentation::kWord64:
        UNREACHABLE();
        return;
    }

    if (g.CanBeImmediate(index, opcode)) {
      Emit(opcode | AddressingModeField::encode(kMode_Offset_RI), g.NoOutput(),
           g.UseRegister(base), g.UseImmediate(index), g.UseRegister(value));
    } else {
      Emit(opcode | AddressingModeField::encode(kMode_Offset_RR), g.NoOutput(),
           g.UseRegister(base), g.UseRegister(index), g.UseRegister(value));
    }
  }
}


void InstructionSelector::VisitCheckedLoad(Node* node) {
  CheckedLoadRepresentation load_rep = CheckedLoadRepresentationOf(node->op());
  ArmOperandGenerator g(this);
  Node* const buffer = node->InputAt(0);
  Node* const offset = node->InputAt(1);
  Node* const length = node->InputAt(2);
  ArchOpcode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsSigned() ? kCheckedLoadInt8 : kCheckedLoadUint8;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsSigned() ? kCheckedLoadInt16 : kCheckedLoadUint16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kCheckedLoadWord32;
      break;
    case MachineRepresentation::kFloat32:
      opcode = kCheckedLoadFloat32;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kCheckedLoadFloat64;
      break;
    case MachineRepresentation::kBit:     // Fall through.
    case MachineRepresentation::kTagged:  // Fall through.
    case MachineRepresentation::kWord64:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }
  InstructionOperand offset_operand = g.UseRegister(offset);
  InstructionOperand length_operand = g.CanBeImmediate(length, kArmCmp)
                                          ? g.UseImmediate(length)
                                          : g.UseRegister(length);
  Emit(opcode | AddressingModeField::encode(kMode_Offset_RR),
       g.DefineAsRegister(node), offset_operand, length_operand,
       g.UseRegister(buffer), offset_operand);
}


void InstructionSelector::VisitCheckedStore(Node* node) {
  MachineRepresentation rep = CheckedStoreRepresentationOf(node->op());
  ArmOperandGenerator g(this);
  Node* const buffer = node->InputAt(0);
  Node* const offset = node->InputAt(1);
  Node* const length = node->InputAt(2);
  Node* const value = node->InputAt(3);
  ArchOpcode opcode = kArchNop;
  switch (rep) {
    case MachineRepresentation::kWord8:
      opcode = kCheckedStoreWord8;
      break;
    case MachineRepresentation::kWord16:
      opcode = kCheckedStoreWord16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kCheckedStoreWord32;
      break;
    case MachineRepresentation::kFloat32:
      opcode = kCheckedStoreFloat32;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kCheckedStoreFloat64;
      break;
    case MachineRepresentation::kBit:     // Fall through.
    case MachineRepresentation::kTagged:  // Fall through.
    case MachineRepresentation::kWord64:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }
  InstructionOperand offset_operand = g.UseRegister(offset);
  InstructionOperand length_operand = g.CanBeImmediate(length, kArmCmp)
                                          ? g.UseImmediate(length)
                                          : g.UseRegister(length);
  Emit(opcode | AddressingModeField::encode(kMode_Offset_RR), g.NoOutput(),
       offset_operand, length_operand, g.UseRegister(value),
       g.UseRegister(buffer), offset_operand);
}


namespace {

void EmitBic(InstructionSelector* selector, Node* node, Node* left,
             Node* right) {
  ArmOperandGenerator g(selector);
  InstructionCode opcode = kArmBic;
  InstructionOperand value_operand;
  InstructionOperand shift_operand;
  if (TryMatchShift(selector, &opcode, right, &value_operand, &shift_operand)) {
    selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(left),
                   value_operand, shift_operand);
    return;
  }
  selector->Emit(opcode | AddressingModeField::encode(kMode_Operand2_R),
                 g.DefineAsRegister(node), g.UseRegister(left),
                 g.UseRegister(right));
}


void EmitUbfx(InstructionSelector* selector, Node* node, Node* left,
              uint32_t lsb, uint32_t width) {
  DCHECK_LE(1u, width);
  DCHECK_LE(width, 32u - lsb);
  ArmOperandGenerator g(selector);
  selector->Emit(kArmUbfx, g.DefineAsRegister(node), g.UseRegister(left),
                 g.TempImmediate(lsb), g.TempImmediate(width));
}

}  // namespace


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
  if (m.right().HasValue()) {
    uint32_t const value = m.right().Value();
    uint32_t width = base::bits::CountPopulation32(value);
    uint32_t msb = base::bits::CountLeadingZeros32(value);
    // Try to interpret this AND as UBFX.
    if (IsSupported(ARMv7) && width != 0 && msb + width == 32) {
      DCHECK_EQ(0u, base::bits::CountTrailingZeros32(value));
      if (m.left().IsWord32Shr()) {
        Int32BinopMatcher mleft(m.left().node());
        if (mleft.right().IsInRange(0, 31)) {
          // UBFX cannot extract bits past the register size, however since
          // shifting the original value would have introduced some zeros we can
          // still use UBFX with a smaller mask and the remaining bits will be
          // zeros.
          uint32_t const lsb = mleft.right().Value();
          return EmitUbfx(this, node, mleft.left().node(), lsb,
                          std::min(width, 32 - lsb));
        }
      }
      return EmitUbfx(this, node, m.left().node(), 0, width);
    }
    // Try to interpret this AND as BIC.
    if (g.CanBeImmediate(~value)) {
      Emit(kArmBic | AddressingModeField::encode(kMode_Operand2_I),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(~value));
      return;
    }
    // Try to interpret this AND as UXTH.
    if (value == 0xffff) {
      Emit(kArmUxth, g.DefineAsRegister(m.node()),
           g.UseRegister(m.left().node()), g.TempImmediate(0));
      return;
    }
    // Try to interpret this AND as BFC.
    if (IsSupported(ARMv7)) {
      width = 32 - width;
      msb = base::bits::CountLeadingZeros32(~value);
      uint32_t lsb = base::bits::CountTrailingZeros32(~value);
      if (msb + width + lsb == 32) {
        Emit(kArmBfc, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
             g.TempImmediate(lsb), g.TempImmediate(width));
        return;
      }
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
    InstructionOperand value_operand;
    InstructionOperand shift_operand;
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


namespace {

template <typename TryMatchShift>
void VisitShift(InstructionSelector* selector, Node* node,
                TryMatchShift try_match_shift, FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  InstructionCode opcode = kArmMov;
  InstructionOperand inputs[4];
  size_t input_count = 2;
  InstructionOperand outputs[2];
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

  DCHECK_NE(0u, input_count);
  DCHECK_NE(0u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);
  DCHECK_NE(kMode_None, AddressingModeField::decode(opcode));

  selector->Emit(cont->Encode(opcode), output_count, outputs, input_count,
                 inputs);
}


template <typename TryMatchShift>
void VisitShift(InstructionSelector* selector, Node* node,
                              TryMatchShift try_match_shift) {
  FlagsContinuation cont;
  VisitShift(selector, node, try_match_shift, &cont);
}

}  // namespace


void InstructionSelector::VisitWord32Shl(Node* node) {
  VisitShift(this, node, TryMatchLSL);
}


void InstructionSelector::VisitWord32Shr(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (IsSupported(ARMv7) && m.left().IsWord32And() &&
      m.right().IsInRange(0, 31)) {
    uint32_t lsb = m.right().Value();
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      uint32_t value = (mleft.right().Value() >> lsb) << lsb;
      uint32_t width = base::bits::CountPopulation32(value);
      uint32_t msb = base::bits::CountLeadingZeros32(value);
      if (msb + width + lsb == 32) {
        DCHECK_EQ(lsb, base::bits::CountTrailingZeros32(value));
        return EmitUbfx(this, node, mleft.left().node(), lsb, width);
      }
    }
  }
  VisitShift(this, node, TryMatchLSR);
}


void InstructionSelector::VisitWord32Sar(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (CanCover(m.node(), m.left().node()) && m.left().IsWord32Shl()) {
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().Is(16) && m.right().Is(16)) {
      Emit(kArmSxth, g.DefineAsRegister(node),
           g.UseRegister(mleft.left().node()), g.TempImmediate(0));
      return;
    } else if (mleft.right().Is(24) && m.right().Is(24)) {
      Emit(kArmSxtb, g.DefineAsRegister(node),
           g.UseRegister(mleft.left().node()), g.TempImmediate(0));
      return;
    }
  }
  VisitShift(this, node, TryMatchASR);
}


void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitShift(this, node, TryMatchROR);
}


void InstructionSelector::VisitWord32Clz(Node* node) {
  VisitRR(this, kArmClz, node);
}


void InstructionSelector::VisitWord32Ctz(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitWord32Popcnt(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitInt32Add(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (CanCover(node, m.left().node())) {
    switch (m.left().opcode()) {
      case IrOpcode::kInt32Mul: {
        Int32BinopMatcher mleft(m.left().node());
        Emit(kArmMla, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()),
             g.UseRegister(mleft.right().node()),
             g.UseRegister(m.right().node()));
        return;
      }
      case IrOpcode::kInt32MulHigh: {
        Int32BinopMatcher mleft(m.left().node());
        Emit(kArmSmmla, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()),
             g.UseRegister(mleft.right().node()),
             g.UseRegister(m.right().node()));
        return;
      }
      case IrOpcode::kWord32And: {
        Int32BinopMatcher mleft(m.left().node());
        if (mleft.right().Is(0xff)) {
          Emit(kArmUxtab, g.DefineAsRegister(node),
               g.UseRegister(m.right().node()),
               g.UseRegister(mleft.left().node()), g.TempImmediate(0));
          return;
        } else if (mleft.right().Is(0xffff)) {
          Emit(kArmUxtah, g.DefineAsRegister(node),
               g.UseRegister(m.right().node()),
               g.UseRegister(mleft.left().node()), g.TempImmediate(0));
          return;
        }
      }
      case IrOpcode::kWord32Sar: {
        Int32BinopMatcher mleft(m.left().node());
        if (CanCover(mleft.node(), mleft.left().node()) &&
            mleft.left().IsWord32Shl()) {
          Int32BinopMatcher mleftleft(mleft.left().node());
          if (mleft.right().Is(24) && mleftleft.right().Is(24)) {
            Emit(kArmSxtab, g.DefineAsRegister(node),
                 g.UseRegister(m.right().node()),
                 g.UseRegister(mleftleft.left().node()), g.TempImmediate(0));
            return;
          } else if (mleft.right().Is(16) && mleftleft.right().Is(16)) {
            Emit(kArmSxtah, g.DefineAsRegister(node),
                 g.UseRegister(m.right().node()),
                 g.UseRegister(mleftleft.left().node()), g.TempImmediate(0));
            return;
          }
        }
      }
      default:
        break;
    }
  }
  if (CanCover(node, m.right().node())) {
    switch (m.right().opcode()) {
      case IrOpcode::kInt32Mul: {
        Int32BinopMatcher mright(m.right().node());
        Emit(kArmMla, g.DefineAsRegister(node),
             g.UseRegister(mright.left().node()),
             g.UseRegister(mright.right().node()),
             g.UseRegister(m.left().node()));
        return;
      }
      case IrOpcode::kInt32MulHigh: {
        Int32BinopMatcher mright(m.right().node());
        Emit(kArmSmmla, g.DefineAsRegister(node),
             g.UseRegister(mright.left().node()),
             g.UseRegister(mright.right().node()),
             g.UseRegister(m.left().node()));
        return;
      }
      case IrOpcode::kWord32And: {
        Int32BinopMatcher mright(m.right().node());
        if (mright.right().Is(0xff)) {
          Emit(kArmUxtab, g.DefineAsRegister(node),
               g.UseRegister(m.left().node()),
               g.UseRegister(mright.left().node()), g.TempImmediate(0));
          return;
        } else if (mright.right().Is(0xffff)) {
          Emit(kArmUxtah, g.DefineAsRegister(node),
               g.UseRegister(m.left().node()),
               g.UseRegister(mright.left().node()), g.TempImmediate(0));
          return;
        }
      }
      case IrOpcode::kWord32Sar: {
        Int32BinopMatcher mright(m.right().node());
        if (CanCover(mright.node(), mright.left().node()) &&
            mright.left().IsWord32Shl()) {
          Int32BinopMatcher mrightleft(mright.left().node());
          if (mright.right().Is(24) && mrightleft.right().Is(24)) {
            Emit(kArmSxtab, g.DefineAsRegister(node),
                 g.UseRegister(m.left().node()),
                 g.UseRegister(mrightleft.left().node()), g.TempImmediate(0));
            return;
          } else if (mright.right().Is(16) && mrightleft.right().Is(16)) {
            Emit(kArmSxtah, g.DefineAsRegister(node),
                 g.UseRegister(m.left().node()),
                 g.UseRegister(mrightleft.left().node()), g.TempImmediate(0));
            return;
          }
        }
      }
      default:
        break;
    }
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
  VisitRRR(this, kArmMul, node);
}


void InstructionSelector::VisitInt32MulHigh(Node* node) {
  VisitRRR(this, kArmSmmul, node);
}


void InstructionSelector::VisitUint32MulHigh(Node* node) {
  ArmOperandGenerator g(this);
  InstructionOperand outputs[] = {g.TempRegister(), g.DefineAsRegister(node)};
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0)),
                                 g.UseRegister(node->InputAt(1))};
  Emit(kArmUmull, arraysize(outputs), outputs, arraysize(inputs), inputs);
}


void InstructionSelector::VisitInt32Div(Node* node) {
  VisitDiv(this, node, kArmSdiv, kArmVcvtF64S32, kArmVcvtS32F64);
}


void InstructionSelector::VisitUint32Div(Node* node) {
  VisitDiv(this, node, kArmUdiv, kArmVcvtF64U32, kArmVcvtU32F64);
}


void InstructionSelector::VisitInt32Mod(Node* node) {
  VisitMod(this, node, kArmSdiv, kArmVcvtF64S32, kArmVcvtS32F64);
}


void InstructionSelector::VisitUint32Mod(Node* node) {
  VisitMod(this, node, kArmUdiv, kArmVcvtF64U32, kArmVcvtU32F64);
}


void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  VisitRR(this, kArmVcvtF64F32, node);
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  VisitRR(this, kArmVcvtF64S32, node);
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  VisitRR(this, kArmVcvtF64U32, node);
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  VisitRR(this, kArmVcvtS32F64, node);
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  VisitRR(this, kArmVcvtU32F64, node);
}


void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  VisitRR(this, kArmVcvtF32F64, node);
}


void InstructionSelector::VisitTruncateFloat64ToInt32(Node* node) {
  switch (TruncationModeOf(node->op())) {
    case TruncationMode::kJavaScript:
      return VisitRR(this, kArchTruncateDoubleToI, node);
    case TruncationMode::kRoundToZero:
      return VisitRR(this, kArmVcvtS32F64, node);
  }
  UNREACHABLE();
}


void InstructionSelector::VisitBitcastFloat32ToInt32(Node* node) {
  VisitRR(this, kArmVmovLowU32F64, node);
}


void InstructionSelector::VisitBitcastInt32ToFloat32(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVmovLowF64U32, g.DefineAsRegister(node),
       ImmediateOperand(ImmediateOperand::INLINE, 0),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitFloat32Add(Node* node) {
  ArmOperandGenerator g(this);
  Float32BinopMatcher m(node);
  if (m.left().IsFloat32Mul() && CanCover(node, m.left().node())) {
    Float32BinopMatcher mleft(m.left().node());
    Emit(kArmVmlaF32, g.DefineSameAsFirst(node),
         g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
         g.UseRegister(mleft.right().node()));
    return;
  }
  if (m.right().IsFloat32Mul() && CanCover(node, m.right().node())) {
    Float32BinopMatcher mright(m.right().node());
    Emit(kArmVmlaF32, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()));
    return;
  }
  VisitRRR(this, kArmVaddF32, node);
}


void InstructionSelector::VisitFloat64Add(Node* node) {
  ArmOperandGenerator g(this);
  Float64BinopMatcher m(node);
  if (m.left().IsFloat64Mul() && CanCover(node, m.left().node())) {
    Float64BinopMatcher mleft(m.left().node());
    Emit(kArmVmlaF64, g.DefineSameAsFirst(node),
         g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
         g.UseRegister(mleft.right().node()));
    return;
  }
  if (m.right().IsFloat64Mul() && CanCover(node, m.right().node())) {
    Float64BinopMatcher mright(m.right().node());
    Emit(kArmVmlaF64, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()));
    return;
  }
  VisitRRR(this, kArmVaddF64, node);
}


void InstructionSelector::VisitFloat32Sub(Node* node) {
  ArmOperandGenerator g(this);
  Float32BinopMatcher m(node);
  if (m.left().IsMinusZero()) {
    Emit(kArmVnegF32, g.DefineAsRegister(node),
         g.UseRegister(m.right().node()));
    return;
  }
  if (m.right().IsFloat32Mul() && CanCover(node, m.right().node())) {
    Float32BinopMatcher mright(m.right().node());
    Emit(kArmVmlsF32, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()));
    return;
  }
  VisitRRR(this, kArmVsubF32, node);
}


void InstructionSelector::VisitFloat64Sub(Node* node) {
  ArmOperandGenerator g(this);
  Float64BinopMatcher m(node);
  if (m.left().IsMinusZero()) {
    if (m.right().IsFloat64RoundDown() &&
        CanCover(m.node(), m.right().node())) {
      if (m.right().InputAt(0)->opcode() == IrOpcode::kFloat64Sub &&
          CanCover(m.right().node(), m.right().InputAt(0))) {
        Float64BinopMatcher mright0(m.right().InputAt(0));
        if (mright0.left().IsMinusZero()) {
          Emit(kArmVrintpF64, g.DefineAsRegister(node),
               g.UseRegister(mright0.right().node()));
          return;
        }
      }
    }
    Emit(kArmVnegF64, g.DefineAsRegister(node),
         g.UseRegister(m.right().node()));
    return;
  }
  if (m.right().IsFloat64Mul() && CanCover(node, m.right().node())) {
    Float64BinopMatcher mright(m.right().node());
    Emit(kArmVmlsF64, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()));
    return;
  }
  VisitRRR(this, kArmVsubF64, node);
}


void InstructionSelector::VisitFloat32Mul(Node* node) {
  VisitRRR(this, kArmVmulF32, node);
}


void InstructionSelector::VisitFloat64Mul(Node* node) {
  VisitRRR(this, kArmVmulF64, node);
}


void InstructionSelector::VisitFloat32Div(Node* node) {
  VisitRRR(this, kArmVdivF32, node);
}


void InstructionSelector::VisitFloat64Div(Node* node) {
  VisitRRR(this, kArmVdivF64, node);
}


void InstructionSelector::VisitFloat64Mod(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVmodF64, g.DefineAsFixed(node, d0), g.UseFixed(node->InputAt(0), d0),
       g.UseFixed(node->InputAt(1), d1))->MarkAsCall();
}


void InstructionSelector::VisitFloat32Max(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitFloat64Max(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitFloat32Min(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitFloat64Min(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitFloat32Abs(Node* node) {
  VisitRR(this, kArmVabsF32, node);
}


void InstructionSelector::VisitFloat64Abs(Node* node) {
  VisitRR(this, kArmVabsF64, node);
}


void InstructionSelector::VisitFloat32Sqrt(Node* node) {
  VisitRR(this, kArmVsqrtF32, node);
}


void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  VisitRR(this, kArmVsqrtF64, node);
}


void InstructionSelector::VisitFloat32RoundDown(Node* node) {
  VisitRR(this, kArmVrintmF32, node);
}


void InstructionSelector::VisitFloat64RoundDown(Node* node) {
  VisitRR(this, kArmVrintmF64, node);
}


void InstructionSelector::VisitFloat32RoundUp(Node* node) {
  VisitRR(this, kArmVrintpF32, node);
}


void InstructionSelector::VisitFloat64RoundUp(Node* node) {
  VisitRR(this, kArmVrintpF64, node);
}


void InstructionSelector::VisitFloat32RoundTruncate(Node* node) {
  VisitRR(this, kArmVrintzF32, node);
}


void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  VisitRR(this, kArmVrintzF64, node);
}


void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  VisitRR(this, kArmVrintaF64, node);
}


void InstructionSelector::VisitFloat32RoundTiesEven(Node* node) {
  VisitRR(this, kArmVrintnF32, node);
}


void InstructionSelector::VisitFloat64RoundTiesEven(Node* node) {
  VisitRR(this, kArmVrintnF64, node);
}


void InstructionSelector::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* descriptor,
    Node* node) {
  ArmOperandGenerator g(this);

  // Prepare for C function call.
  if (descriptor->IsCFunctionCall()) {
    Emit(kArchPrepareCallCFunction |
             MiscField::encode(static_cast<int>(descriptor->CParameterCount())),
         0, nullptr, 0, nullptr);

    // Poke any stack arguments.
    for (size_t n = 0; n < arguments->size(); ++n) {
      PushParameter input = (*arguments)[n];
      if (input.node()) {
        int slot = static_cast<int>(n);
        Emit(kArmPoke | MiscField::encode(slot), g.NoOutput(),
             g.UseRegister(input.node()));
      }
    }
  } else {
    // Push any stack arguments.
    for (PushParameter input : base::Reversed(*arguments)) {
      // Skip any alignment holes in pushed nodes.
      if (input.node() == nullptr) continue;
      Emit(kArmPush, g.NoOutput(), g.UseRegister(input.node()));
    }
  }
}


bool InstructionSelector::IsTailCallAddressImmediate() { return false; }


namespace {

// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  InstructionOperand left, InstructionOperand right,
                  FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  opcode = cont->Encode(opcode);
  if (cont->IsBranch()) {
    selector->Emit(opcode, g.NoOutput(), left, right,
                   g.Label(cont->true_block()), g.Label(cont->false_block()));
  } else {
    DCHECK(cont->IsSet());
    selector->Emit(opcode, g.DefineAsRegister(cont->result()), left, right);
  }
}


// Shared routine for multiple float32 compare operations.
void VisitFloat32Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  Float32BinopMatcher m(node);
  if (m.right().Is(0.0f)) {
    VisitCompare(selector, kArmVcmpF32, g.UseRegister(m.left().node()),
                 g.UseImmediate(m.right().node()), cont);
  } else if (m.left().Is(0.0f)) {
    cont->Commute();
    VisitCompare(selector, kArmVcmpF32, g.UseRegister(m.right().node()),
                 g.UseImmediate(m.left().node()), cont);
  } else {
    VisitCompare(selector, kArmVcmpF32, g.UseRegister(m.left().node()),
                 g.UseRegister(m.right().node()), cont);
  }
}


// Shared routine for multiple float64 compare operations.
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  Float64BinopMatcher m(node);
  if (m.right().Is(0.0)) {
    VisitCompare(selector, kArmVcmpF64, g.UseRegister(m.left().node()),
                 g.UseImmediate(m.right().node()), cont);
  } else if (m.left().Is(0.0)) {
    cont->Commute();
    VisitCompare(selector, kArmVcmpF64, g.UseRegister(m.right().node()),
                 g.UseImmediate(m.left().node()), cont);
  } else {
    VisitCompare(selector, kArmVcmpF64, g.UseRegister(m.left().node()),
                 g.UseRegister(m.right().node()), cont);
  }
}


// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand inputs[5];
  size_t input_count = 0;
  InstructionOperand outputs[1];
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

  DCHECK_NE(0u, input_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->Emit(cont->Encode(opcode), output_count, outputs, input_count,
                 inputs);
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
      case IrOpcode::kFloat32Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitFloat32Compare(selector, value, cont);
      case IrOpcode::kFloat32LessThan:
        cont->OverwriteAndNegateIfEqual(kFloatLessThan);
        return VisitFloat32Compare(selector, value, cont);
      case IrOpcode::kFloat32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kFloatLessThanOrEqual);
        return VisitFloat32Compare(selector, value, cont);
      case IrOpcode::kFloat64Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitFloat64Compare(selector, value, cont);
      case IrOpcode::kFloat64LessThan:
        cont->OverwriteAndNegateIfEqual(kFloatLessThan);
        return VisitFloat64Compare(selector, value, cont);
      case IrOpcode::kFloat64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kFloatLessThanOrEqual);
        return VisitFloat64Compare(selector, value, cont);
      case IrOpcode::kProjection:
        // Check if this is the overflow output projection of an
        // <Operation>WithOverflow node.
        if (ProjectionIndexOf(value->op()) == 1u) {
          // We cannot combine the <Operation>WithOverflow with this branch
          // unless the 0th projection (the use of the actual value of the
          // <Operation> is either nullptr, which means there's no use of the
          // actual value, or was already defined, which means it is scheduled
          // *AFTER* this branch).
          Node* const node = value->InputAt(0);
          Node* const result = NodeProperties::FindProjection(node, 0);
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
  InstructionOperand const value_operand = g.UseRegister(value);
  if (cont->IsBranch()) {
    selector->Emit(opcode, g.NoOutput(), value_operand, value_operand,
                   g.Label(cont->true_block()), g.Label(cont->false_block()));
  } else {
    selector->Emit(opcode, g.DefineAsRegister(cont->result()), value_operand,
                   value_operand);
  }
}

}  // namespace


void InstructionSelector::VisitBranch(Node* branch, BasicBlock* tbranch,
                                      BasicBlock* fbranch) {
  FlagsContinuation cont(kNotEqual, tbranch, fbranch);
  VisitWordCompareZero(this, branch, branch->InputAt(0), &cont);
}


void InstructionSelector::VisitSwitch(Node* node, const SwitchInfo& sw) {
  ArmOperandGenerator g(this);
  InstructionOperand value_operand = g.UseRegister(node->InputAt(0));

  // Emit either ArchTableSwitch or ArchLookupSwitch.
  size_t table_space_cost = 4 + sw.value_range;
  size_t table_time_cost = 3;
  size_t lookup_space_cost = 3 + 2 * sw.case_count;
  size_t lookup_time_cost = sw.case_count;
  if (sw.case_count > 0 &&
      table_space_cost + 3 * table_time_cost <=
          lookup_space_cost + 3 * lookup_time_cost &&
      sw.min_value > std::numeric_limits<int32_t>::min()) {
    InstructionOperand index_operand = value_operand;
    if (sw.min_value) {
      index_operand = g.TempRegister();
      Emit(kArmSub | AddressingModeField::encode(kMode_Operand2_I),
           index_operand, value_operand, g.TempImmediate(sw.min_value));
    }
    // Generate a table lookup.
    return EmitTableSwitch(sw, index_operand);
  }

  // Generate a sequence of conditional jumps.
  return EmitLookupSwitch(sw, value_operand);
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
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitBinop(this, node, kArmAdd, kArmAdd, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kArmAdd, kArmAdd, &cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitBinop(this, node, kArmSub, kArmRsb, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kArmSub, kArmRsb, &cont);
}


void InstructionSelector::VisitFloat32Equal(Node* node) {
  FlagsContinuation cont(kEqual, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat32LessThan(Node* node) {
  FlagsContinuation cont(kFloatLessThan, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat32LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kFloatLessThanOrEqual, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64Equal(Node* node) {
  FlagsContinuation cont(kEqual, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64LessThan(Node* node) {
  FlagsContinuation cont(kFloatLessThan, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kFloatLessThanOrEqual, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64ExtractLowWord32(Node* node) {
  VisitRR(this, kArmVmovLowU32F64, node);
}


void InstructionSelector::VisitFloat64ExtractHighWord32(Node* node) {
  VisitRR(this, kArmVmovHighU32F64, node);
}


void InstructionSelector::VisitFloat64InsertLowWord32(Node* node) {
  ArmOperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (left->opcode() == IrOpcode::kFloat64InsertHighWord32 &&
      CanCover(node, left)) {
    left = left->InputAt(1);
    Emit(kArmVmovF64U32U32, g.DefineAsRegister(node), g.UseRegister(right),
         g.UseRegister(left));
    return;
  }
  Emit(kArmVmovLowF64U32, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}


void InstructionSelector::VisitFloat64InsertHighWord32(Node* node) {
  ArmOperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (left->opcode() == IrOpcode::kFloat64InsertLowWord32 &&
      CanCover(node, left)) {
    left = left->InputAt(1);
    Emit(kArmVmovF64U32U32, g.DefineAsRegister(node), g.UseRegister(left),
         g.UseRegister(right));
    return;
  }
  Emit(kArmVmovHighF64U32, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}


// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags =
      MachineOperatorBuilder::kInt32DivIsSafe |
      MachineOperatorBuilder::kUint32DivIsSafe;
  if (CpuFeatures::IsSupported(ARMv8)) {
    flags |= MachineOperatorBuilder::kFloat32RoundDown |
             MachineOperatorBuilder::kFloat64RoundDown |
             MachineOperatorBuilder::kFloat32RoundUp |
             MachineOperatorBuilder::kFloat64RoundUp |
             MachineOperatorBuilder::kFloat32RoundTruncate |
             MachineOperatorBuilder::kFloat64RoundTruncate |
             MachineOperatorBuilder::kFloat64RoundTiesAway |
             MachineOperatorBuilder::kFloat32RoundTiesEven |
             MachineOperatorBuilder::kFloat64RoundTiesEven;
  }
  return flags;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
