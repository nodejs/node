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

template <IrOpcode::Value kOpcode, int kImmMin, int kImmMax,
          AddressingMode kImmMode>
bool TryMatchShiftImmediate(InstructionSelector* selector,
                            InstructionCode* opcode_return, Node* node,
                            InstructionOperand* value_return,
                            InstructionOperand* shift_return) {
  ArmOperandGenerator g(selector);
  if (node->opcode() == kOpcode) {
    Int32BinopMatcher m(node);
    if (m.right().IsInRange(kImmMin, kImmMax)) {
      *opcode_return |= AddressingModeField::encode(kImmMode);
      *value_return = g.UseRegister(m.left().node());
      *shift_return = g.UseImmediate(m.right().node());
      return true;
    }
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

bool TryMatchLSLImmediate(InstructionSelector* selector,
                          InstructionCode* opcode_return, Node* node,
                          InstructionOperand* value_return,
                          InstructionOperand* shift_return) {
  return TryMatchShiftImmediate<IrOpcode::kWord32Shl, 0, 31,
                                kMode_Operand2_R_LSL_I>(
      selector, opcode_return, node, value_return, shift_return);
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

  opcode = cont->Encode(opcode);
  if (cont->IsDeoptimize()) {
    selector->EmitDeoptimize(opcode, output_count, outputs, input_count, inputs,
                             cont->reason(), cont->frame_state());
  } else if (cont->IsTrap()) {
    inputs[input_count++] = g.UseImmediate(cont->trap_id());
    selector->Emit(opcode, output_count, outputs, input_count, inputs);
  } else {
    selector->Emit(opcode, output_count, outputs, input_count, inputs);
  }
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
  if (selector->IsSupported(ARMv7)) {
    selector->Emit(kArmMls, result_operand, div_operand, right_operand,
                   left_operand);
  } else {
    InstructionOperand mul_operand = g.TempRegister();
    selector->Emit(kArmMul, mul_operand, div_operand, right_operand);
    selector->Emit(kArmSub | AddressingModeField::encode(kMode_Operand2_R),
                   result_operand, left_operand, mul_operand);
  }
}

void EmitLoad(InstructionSelector* selector, InstructionCode opcode,
              InstructionOperand* output, Node* base, Node* index) {
  ArmOperandGenerator g(selector);
  InstructionOperand inputs[3];
  size_t input_count = 2;

  inputs[0] = g.UseRegister(base);
  if (g.CanBeImmediate(index, opcode)) {
    inputs[1] = g.UseImmediate(index);
    opcode |= AddressingModeField::encode(kMode_Offset_RI);
  } else if ((opcode == kArmLdr) &&
             TryMatchLSLImmediate(selector, &opcode, index, &inputs[1],
                                  &inputs[2])) {
    input_count = 3;
  } else {
    inputs[1] = g.UseRegister(index);
    opcode |= AddressingModeField::encode(kMode_Offset_RR);
  }
  selector->Emit(opcode, 1, output, input_count, inputs);
}

void EmitStore(InstructionSelector* selector, InstructionCode opcode,
               size_t input_count, InstructionOperand* inputs,
               Node* index) {
  ArmOperandGenerator g(selector);

  if (g.CanBeImmediate(index, opcode)) {
    inputs[input_count++] = g.UseImmediate(index);
    opcode |= AddressingModeField::encode(kMode_Offset_RI);
  } else if ((opcode == kArmStr) &&
             TryMatchLSLImmediate(selector, &opcode, index, &inputs[2],
                                  &inputs[3])) {
    input_count = 4;
  } else {
    inputs[input_count++] = g.UseRegister(index);
    opcode |= AddressingModeField::encode(kMode_Offset_RR);
  }
  selector->Emit(opcode, 0, nullptr, input_count, inputs);
}

}  // namespace


void InstructionSelector::VisitLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  InstructionCode opcode = kArchNop;
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
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:  // Fall through.
    case MachineRepresentation::kWord32:
      opcode = kArmLdr;
      break;
    case MachineRepresentation::kWord64:   // Fall through.
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }

  InstructionOperand output = g.DefineAsRegister(node);
  EmitLoad(this, opcode, &output, base, index);
}

void InstructionSelector::VisitProtectedLoad(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
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
    DCHECK(CanBeTaggedPointer(rep));
    AddressingMode addressing_mode;
    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    // OutOfLineRecordWrite uses the index in an 'add' instruction as well as
    // for the store itself, so we must check compatibility with both.
    if (g.CanBeImmediate(index, kArmAdd) && g.CanBeImmediate(index, kArmStr)) {
      inputs[input_count++] = g.UseImmediate(index);
      addressing_mode = kMode_Offset_RI;
    } else {
      inputs[input_count++] = g.UseUniqueRegister(index);
      addressing_mode = kMode_Offset_RR;
    }
    inputs[input_count++] = g.UseUniqueRegister(value);
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
    code |= AddressingModeField::encode(addressing_mode);
    code |= MiscField::encode(static_cast<int>(record_write_mode));
    Emit(code, 0, nullptr, input_count, inputs, temp_count, temps);
  } else {
    InstructionCode opcode = kArchNop;
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
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:  // Fall through.
      case MachineRepresentation::kWord32:
        opcode = kArmStr;
        break;
      case MachineRepresentation::kWord64:   // Fall through.
      case MachineRepresentation::kSimd128:  // Fall through.
      case MachineRepresentation::kNone:
        UNREACHABLE();
        return;
    }

    InstructionOperand inputs[4];
    size_t input_count = 0;
    inputs[input_count++] = g.UseRegister(value);
    inputs[input_count++] = g.UseRegister(base);
    EmitStore(this, opcode, input_count, inputs, index);
  }
}

void InstructionSelector::VisitProtectedStore(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelector::VisitUnalignedLoad(Node* node) {
  UnalignedLoadRepresentation load_rep =
      UnalignedLoadRepresentationOf(node->op());
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  InstructionCode opcode = kArmLdr;
  // Only floating point loads need to be specially handled; integer loads
  // support unaligned access. We support unaligned FP loads by loading to
  // integer registers first, then moving to the destination FP register.
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32: {
      InstructionOperand temp = g.TempRegister();
      EmitLoad(this, opcode, &temp, base, index);
      Emit(kArmVmovF32U32, g.DefineAsRegister(node), temp);
      return;
    }
    case MachineRepresentation::kFloat64: {
      // TODO(arm): use vld1.8 for this when NEON is available.
      // Compute the address of the least-significant half of the FP value.
      // We assume that the base node is unlikely to be an encodable immediate
      // or the result of a shift operation, so only consider the addressing
      // mode that should be used for the index node.
      InstructionCode add_opcode = kArmAdd;
      InstructionOperand inputs[3];
      inputs[0] = g.UseRegister(base);

      size_t input_count;
      if (TryMatchImmediateOrShift(this, &add_opcode, index, &input_count,
                                   &inputs[1])) {
        // input_count has been set by TryMatchImmediateOrShift(), so increment
        // it to account for the base register in inputs[0].
        input_count++;
      } else {
        add_opcode |= AddressingModeField::encode(kMode_Operand2_R);
        inputs[1] = g.UseRegister(index);
        input_count = 2;  // Base register and index.
      }

      InstructionOperand addr = g.TempRegister();
      Emit(add_opcode, 1, &addr, input_count, inputs);

      // Load both halves and move to an FP register.
      InstructionOperand fp_lo = g.TempRegister();
      InstructionOperand fp_hi = g.TempRegister();
      opcode |= AddressingModeField::encode(kMode_Offset_RI);
      Emit(opcode, fp_lo, addr, g.TempImmediate(0));
      Emit(opcode, fp_hi, addr, g.TempImmediate(4));
      Emit(kArmVmovF64U32U32, g.DefineAsRegister(node), fp_lo, fp_hi);
      return;
    }
    default:
      // All other cases should support unaligned accesses.
      UNREACHABLE();
      return;
  }
}

void InstructionSelector::VisitUnalignedStore(Node* node) {
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  InstructionOperand inputs[4];
  size_t input_count = 0;

  UnalignedStoreRepresentation store_rep =
      UnalignedStoreRepresentationOf(node->op());

  // Only floating point stores need to be specially handled; integer stores
  // support unaligned access. We support unaligned FP stores by moving the
  // value to integer registers first, then storing to the destination address.
  switch (store_rep) {
    case MachineRepresentation::kFloat32: {
      inputs[input_count++] = g.TempRegister();
      Emit(kArmVmovU32F32, inputs[0], g.UseRegister(value));
      inputs[input_count++] = g.UseRegister(base);
      EmitStore(this, kArmStr, input_count, inputs, index);
      return;
    }
    case MachineRepresentation::kFloat64: {
      // TODO(arm): use vst1.8 for this when NEON is available.
      // Store a 64-bit floating point value using two 32-bit integer stores.
      // Computing the store address here would require three live temporary
      // registers (fp<63:32>, fp<31:0>, address), so compute base + 4 after
      // storing the least-significant half of the value.

      // First, move the 64-bit FP value into two temporary integer registers.
      InstructionOperand fp[] = {g.TempRegister(), g.TempRegister()};
      inputs[input_count++] = g.UseRegister(value);
      Emit(kArmVmovU32U32F64, arraysize(fp), fp, input_count,
           inputs);

      // Store the least-significant half.
      inputs[0] = fp[0];  // Low 32-bits of FP value.
      inputs[input_count++] = g.UseRegister(base);  // First store base address.
      EmitStore(this, kArmStr, input_count, inputs, index);

      // Store the most-significant half.
      InstructionOperand base4 = g.TempRegister();
      Emit(kArmAdd | AddressingModeField::encode(kMode_Operand2_I), base4,
           g.UseRegister(base), g.TempImmediate(4));  // Compute base + 4.
      inputs[0] = fp[1];  // High 32-bits of FP value.
      inputs[1] = base4;  // Second store base + 4 address.
      EmitStore(this, kArmStr, input_count, inputs, index);
      return;
    }
    default:
      // All other cases should support unaligned accesses.
      UNREACHABLE();
      return;
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
    case MachineRepresentation::kBit:      // Fall through.
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:   // Fall through.
    case MachineRepresentation::kWord64:   // Fall through.
    case MachineRepresentation::kSimd128:  // Fall through.
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
    case MachineRepresentation::kBit:      // Fall through.
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:   // Fall through.
    case MachineRepresentation::kWord64:   // Fall through.
    case MachineRepresentation::kSimd128:  // Fall through.
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
    uint32_t leading_zeros = base::bits::CountLeadingZeros32(value);

    // Try to merge SHR operations on the left hand input into this AND.
    if (m.left().IsWord32Shr()) {
      Int32BinopMatcher mshr(m.left().node());
      if (mshr.right().HasValue()) {
        uint32_t const shift = mshr.right().Value();

        if (((shift == 8) || (shift == 16) || (shift == 24)) &&
            ((value == 0xff) || (value == 0xffff))) {
          // Merge SHR into AND by emitting a UXTB or UXTH instruction with a
          // bytewise rotation.
          Emit((value == 0xff) ? kArmUxtb : kArmUxth,
               g.DefineAsRegister(m.node()), g.UseRegister(mshr.left().node()),
               g.TempImmediate(mshr.right().Value()));
          return;
        } else if (IsSupported(ARMv7) && (width != 0) &&
                   ((leading_zeros + width) == 32)) {
          // Merge Shr into And by emitting a UBFX instruction.
          DCHECK_EQ(0u, base::bits::CountTrailingZeros32(value));
          if ((1 <= shift) && (shift <= 31)) {
            // UBFX cannot extract bits past the register size, however since
            // shifting the original value would have introduced some zeros we
            // can still use UBFX with a smaller mask and the remaining bits
            // will be zeros.
            EmitUbfx(this, node, mshr.left().node(), shift,
                     std::min(width, 32 - shift));
            return;
          }
        }
      }
    } else if (value == 0xffff) {
      // Emit UXTH for this AND. We don't bother testing for UXTB, as it's no
      // better than AND 0xff for this operation.
      Emit(kArmUxth, g.DefineAsRegister(m.node()),
           g.UseRegister(m.left().node()), g.TempImmediate(0));
      return;
    }
    if (g.CanBeImmediate(~value)) {
      // Emit BIC for this AND by inverting the immediate value first.
      Emit(kArmBic | AddressingModeField::encode(kMode_Operand2_I),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(~value));
      return;
    }
    if (!g.CanBeImmediate(value) && IsSupported(ARMv7)) {
      // If value has 9 to 23 contiguous set bits, and has the lsb set, we can
      // replace this AND with UBFX. Other contiguous bit patterns have already
      // been handled by BIC or will be handled by AND.
      if ((width != 0) && ((leading_zeros + width) == 32) &&
          (9 <= leading_zeros) && (leading_zeros <= 23)) {
        DCHECK_EQ(0u, base::bits::CountTrailingZeros32(value));
        EmitUbfx(this, node, m.left().node(), 0, width);
        return;
      }

      width = 32 - width;
      leading_zeros = base::bits::CountLeadingZeros32(~value);
      uint32_t lsb = base::bits::CountTrailingZeros32(~value);
      if ((leading_zeros + width + lsb) == 32) {
        // This AND can be replaced with BFC.
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

  opcode = cont->Encode(opcode);
  if (cont->IsDeoptimize()) {
    selector->EmitDeoptimize(opcode, output_count, outputs, input_count, inputs,
                             cont->reason(), cont->frame_state());
  } else if (cont->IsTrap()) {
    inputs[input_count++] = g.UseImmediate(cont->trap_id());
    selector->Emit(opcode, output_count, outputs, input_count, inputs);
  } else {
    selector->Emit(opcode, output_count, outputs, input_count, inputs);
  }
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
    if (m.right().HasValue() && mleft.right().HasValue()) {
      uint32_t sar = m.right().Value();
      uint32_t shl = mleft.right().Value();
      if ((sar == shl) && (sar == 16)) {
        Emit(kArmSxth, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(0));
        return;
      } else if ((sar == shl) && (sar == 24)) {
        Emit(kArmSxtb, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(0));
        return;
      } else if (IsSupported(ARMv7) && (sar >= shl)) {
        Emit(kArmSbfx, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(sar - shl),
             g.TempImmediate(32 - sar));
        return;
      }
    }
  }
  VisitShift(this, node, TryMatchASR);
}

void InstructionSelector::VisitInt32PairAdd(Node* node) {
  ArmOperandGenerator g(this);

  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    // We use UseUniqueRegister here to avoid register sharing with the output
    // registers.
    InstructionOperand inputs[] = {
        g.UseRegister(node->InputAt(0)), g.UseUniqueRegister(node->InputAt(1)),
        g.UseRegister(node->InputAt(2)), g.UseUniqueRegister(node->InputAt(3))};

    InstructionOperand outputs[] = {
        g.DefineAsRegister(node),
        g.DefineAsRegister(NodeProperties::FindProjection(node, 1))};

    Emit(kArmAddPair, 2, outputs, 4, inputs);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    Emit(kArmAdd | AddressingModeField::encode(kMode_Operand2_R),
         g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
         g.UseRegister(node->InputAt(2)));
  }
}

void InstructionSelector::VisitInt32PairSub(Node* node) {
  ArmOperandGenerator g(this);

  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    // We use UseUniqueRegister here to avoid register sharing with the output
    // register.
    InstructionOperand inputs[] = {
        g.UseRegister(node->InputAt(0)), g.UseUniqueRegister(node->InputAt(1)),
        g.UseRegister(node->InputAt(2)), g.UseUniqueRegister(node->InputAt(3))};

    InstructionOperand outputs[] = {
        g.DefineAsRegister(node),
        g.DefineAsRegister(NodeProperties::FindProjection(node, 1))};

    Emit(kArmSubPair, 2, outputs, 4, inputs);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    Emit(kArmSub | AddressingModeField::encode(kMode_Operand2_R),
         g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
         g.UseRegister(node->InputAt(2)));
  }
}

void InstructionSelector::VisitInt32PairMul(Node* node) {
  ArmOperandGenerator g(this);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    InstructionOperand inputs[] = {g.UseUniqueRegister(node->InputAt(0)),
                                   g.UseUniqueRegister(node->InputAt(1)),
                                   g.UseUniqueRegister(node->InputAt(2)),
                                   g.UseUniqueRegister(node->InputAt(3))};

    InstructionOperand outputs[] = {
        g.DefineAsRegister(node),
        g.DefineAsRegister(NodeProperties::FindProjection(node, 1))};

    Emit(kArmMulPair, 2, outputs, 4, inputs);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    Emit(kArmMul | AddressingModeField::encode(kMode_Operand2_R),
         g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
         g.UseRegister(node->InputAt(2)));
  }
}

namespace {
// Shared routine for multiple shift operations.
void VisitWord32PairShift(InstructionSelector* selector, InstructionCode opcode,
                          Node* node) {
  ArmOperandGenerator g(selector);
  // We use g.UseUniqueRegister here to guarantee that there is
  // no register aliasing of input registers with output registers.
  Int32Matcher m(node->InputAt(2));
  InstructionOperand shift_operand;
  if (m.HasValue()) {
    shift_operand = g.UseImmediate(m.node());
  } else {
    shift_operand = g.UseUniqueRegister(m.node());
  }

  InstructionOperand inputs[] = {g.UseUniqueRegister(node->InputAt(0)),
                                 g.UseUniqueRegister(node->InputAt(1)),
                                 shift_operand};

  Node* projection1 = NodeProperties::FindProjection(node, 1);

  InstructionOperand outputs[2];
  InstructionOperand temps[1];
  int32_t output_count = 0;
  int32_t temp_count = 0;

  outputs[output_count++] = g.DefineAsRegister(node);
  if (projection1) {
    outputs[output_count++] = g.DefineAsRegister(projection1);
  } else {
    temps[temp_count++] = g.TempRegister();
  }

  selector->Emit(opcode, output_count, outputs, 3, inputs, temp_count, temps);
}
}  // namespace
void InstructionSelector::VisitWord32PairShl(Node* node) {
  VisitWord32PairShift(this, kArmLslPair, node);
}

void InstructionSelector::VisitWord32PairShr(Node* node) {
  VisitWord32PairShift(this, kArmLsrPair, node);
}

void InstructionSelector::VisitWord32PairSar(Node* node) {
  VisitWord32PairShift(this, kArmAsrPair, node);
}

void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitShift(this, node, TryMatchROR);
}


void InstructionSelector::VisitWord32Clz(Node* node) {
  VisitRR(this, kArmClz, node);
}


void InstructionSelector::VisitWord32Ctz(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitWord32ReverseBits(Node* node) {
  DCHECK(IsSupported(ARMv7));
  VisitRR(this, kArmRbit, node);
}

void InstructionSelector::VisitWord64ReverseBytes(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord32ReverseBytes(Node* node) { UNREACHABLE(); }

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
  if (IsSupported(ARMv7) && m.right().IsInt32Mul() &&
      CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    Emit(kArmMls, g.DefineAsRegister(node), g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()), g.UseRegister(m.left().node()));
    return;
  }
  VisitBinop(this, node, kArmSub, kArmRsb);
}

namespace {

void EmitInt32MulWithOverflow(InstructionSelector* selector, Node* node,
                              FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand result_operand = g.DefineAsRegister(node);
  InstructionOperand temp_operand = g.TempRegister();
  InstructionOperand outputs[] = {result_operand, temp_operand};
  InstructionOperand inputs[] = {g.UseRegister(m.left().node()),
                                 g.UseRegister(m.right().node())};
  selector->Emit(kArmSmull, 2, outputs, 2, inputs);

  // result operand needs shift operator.
  InstructionOperand shift_31 = g.UseImmediate(31);
  InstructionCode opcode = cont->Encode(kArmCmp) |
                           AddressingModeField::encode(kMode_Operand2_R_ASR_I);
  if (cont->IsBranch()) {
    selector->Emit(opcode, g.NoOutput(), temp_operand, result_operand, shift_31,
                   g.Label(cont->true_block()), g.Label(cont->false_block()));
  } else if (cont->IsDeoptimize()) {
    InstructionOperand in[] = {temp_operand, result_operand, shift_31};
    selector->EmitDeoptimize(opcode, 0, nullptr, 3, in, cont->reason(),
                             cont->frame_state());
  } else if (cont->IsSet()) {
    selector->Emit(opcode, g.DefineAsRegister(cont->result()), temp_operand,
                   result_operand, shift_31);
  } else {
    DCHECK(cont->IsTrap());
    InstructionOperand in[] = {temp_operand, result_operand, shift_31,
                               g.UseImmediate(cont->trap_id())};
    selector->Emit(opcode, 0, nullptr, 4, in);
  }
}

}  // namespace

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


void InstructionSelector::VisitRoundInt32ToFloat32(Node* node) {
  VisitRR(this, kArmVcvtF32S32, node);
}


void InstructionSelector::VisitRoundUint32ToFloat32(Node* node) {
  VisitRR(this, kArmVcvtF32U32, node);
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  VisitRR(this, kArmVcvtF64S32, node);
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  VisitRR(this, kArmVcvtF64U32, node);
}


void InstructionSelector::VisitTruncateFloat32ToInt32(Node* node) {
  VisitRR(this, kArmVcvtS32F32, node);
}


void InstructionSelector::VisitTruncateFloat32ToUint32(Node* node) {
  VisitRR(this, kArmVcvtU32F32, node);
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  VisitRR(this, kArmVcvtS32F64, node);
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  VisitRR(this, kArmVcvtU32F64, node);
}

void InstructionSelector::VisitTruncateFloat64ToUint32(Node* node) {
  VisitRR(this, kArmVcvtU32F64, node);
}
void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  VisitRR(this, kArmVcvtF32F64, node);
}

void InstructionSelector::VisitTruncateFloat64ToWord32(Node* node) {
  VisitRR(this, kArchTruncateDoubleToI, node);
}

void InstructionSelector::VisitRoundFloat64ToInt32(Node* node) {
  VisitRR(this, kArmVcvtS32F64, node);
}

void InstructionSelector::VisitBitcastFloat32ToInt32(Node* node) {
  VisitRR(this, kArmVmovU32F32, node);
}

void InstructionSelector::VisitBitcastInt32ToFloat32(Node* node) {
  VisitRR(this, kArmVmovF32U32, node);
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

void InstructionSelector::VisitFloat32Max(Node* node) {
  VisitRRR(this, kArmFloat32Max, node);
}

void InstructionSelector::VisitFloat64Max(Node* node) {
  VisitRRR(this, kArmFloat64Max, node);
}

void InstructionSelector::VisitFloat64SilenceNaN(Node* node) {
  VisitRR(this, kArmFloat64SilenceNaN, node);
}

void InstructionSelector::VisitFloat32Min(Node* node) {
  VisitRRR(this, kArmFloat32Min, node);
}

void InstructionSelector::VisitFloat64Min(Node* node) {
  VisitRRR(this, kArmFloat64Min, node);
}

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
  DCHECK(CpuFeatures::IsSupported(ARMv8));
  VisitRR(this, kArmVrintmF32, node);
}


void InstructionSelector::VisitFloat64RoundDown(Node* node) {
  DCHECK(CpuFeatures::IsSupported(ARMv8));
  VisitRR(this, kArmVrintmF64, node);
}


void InstructionSelector::VisitFloat32RoundUp(Node* node) {
  DCHECK(CpuFeatures::IsSupported(ARMv8));
  VisitRR(this, kArmVrintpF32, node);
}


void InstructionSelector::VisitFloat64RoundUp(Node* node) {
  DCHECK(CpuFeatures::IsSupported(ARMv8));
  VisitRR(this, kArmVrintpF64, node);
}


void InstructionSelector::VisitFloat32RoundTruncate(Node* node) {
  DCHECK(CpuFeatures::IsSupported(ARMv8));
  VisitRR(this, kArmVrintzF32, node);
}


void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  DCHECK(CpuFeatures::IsSupported(ARMv8));
  VisitRR(this, kArmVrintzF64, node);
}


void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  DCHECK(CpuFeatures::IsSupported(ARMv8));
  VisitRR(this, kArmVrintaF64, node);
}


void InstructionSelector::VisitFloat32RoundTiesEven(Node* node) {
  DCHECK(CpuFeatures::IsSupported(ARMv8));
  VisitRR(this, kArmVrintnF32, node);
}


void InstructionSelector::VisitFloat64RoundTiesEven(Node* node) {
  DCHECK(CpuFeatures::IsSupported(ARMv8));
  VisitRR(this, kArmVrintnF64, node);
}

void InstructionSelector::VisitFloat32Neg(Node* node) {
  VisitRR(this, kArmVnegF32, node);
}

void InstructionSelector::VisitFloat64Neg(Node* node) {
  VisitRR(this, kArmVnegF64, node);
}

void InstructionSelector::VisitFloat64Ieee754Binop(Node* node,
                                                   InstructionCode opcode) {
  ArmOperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, d0), g.UseFixed(node->InputAt(0), d0),
       g.UseFixed(node->InputAt(1), d1))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat64Ieee754Unop(Node* node,
                                                  InstructionCode opcode) {
  ArmOperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, d0), g.UseFixed(node->InputAt(0), d0))
      ->MarkAsCall();
}

void InstructionSelector::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* descriptor,
    Node* node) {
  ArmOperandGenerator g(this);

  // Prepare for C function call.
  if (descriptor->IsCFunctionCall()) {
    Emit(kArchPrepareCallCFunction |
             MiscField::encode(static_cast<int>(descriptor->ParameterCount())),
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

int InstructionSelector::GetTempsCountForTailCallFromJSFunction() { return 3; }

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
  } else if (cont->IsDeoptimize()) {
    selector->EmitDeoptimize(opcode, g.NoOutput(), left, right, cont->reason(),
                             cont->frame_state());
  } else if (cont->IsSet()) {
    selector->Emit(opcode, g.DefineAsRegister(cont->result()), left, right);
  } else {
    DCHECK(cont->IsTrap());
    selector->Emit(opcode, g.NoOutput(), left, right,
                   g.UseImmediate(cont->trap_id()));
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

// Check whether we can convert:
// ((a <op> b) cmp 0), b.<cond>
// to:
// (a <ops> b), b.<cond'>
// where <ops> is the flag setting version of <op>.
// We only generate conditions <cond'> that are a combination of the N
// and Z flags. This avoids the need to make this function dependent on
// the flag-setting operation.
bool CanUseFlagSettingBinop(FlagsCondition cond) {
  switch (cond) {
    case kEqual:
    case kNotEqual:
    case kSignedLessThan:
    case kSignedGreaterThanOrEqual:
    case kUnsignedLessThanOrEqual:  // x <= 0 -> x == 0
    case kUnsignedGreaterThan:      // x > 0 -> x != 0
      return true;
    default:
      return false;
  }
}

// Map <cond> to <cond'> so that the following transformation is possible:
// ((a <op> b) cmp 0), b.<cond>
// to:
// (a <ops> b), b.<cond'>
// where <ops> is the flag setting version of <op>.
FlagsCondition MapForFlagSettingBinop(FlagsCondition cond) {
  DCHECK(CanUseFlagSettingBinop(cond));
  switch (cond) {
    case kEqual:
    case kNotEqual:
      return cond;
    case kSignedLessThan:
      return kNegative;
    case kSignedGreaterThanOrEqual:
      return kPositiveOrZero;
    case kUnsignedLessThanOrEqual:  // x <= 0 -> x == 0
      return kEqual;
    case kUnsignedGreaterThan:  // x > 0 -> x != 0
      return kNotEqual;
    default:
      UNREACHABLE();
      return cond;
  }
}

// Check if we can perform the transformation:
// ((a <op> b) cmp 0), b.<cond>
// to:
// (a <ops> b), b.<cond'>
// where <ops> is the flag setting version of <op>, and if so,
// updates {node}, {opcode} and {cont} accordingly.
void MaybeReplaceCmpZeroWithFlagSettingBinop(InstructionSelector* selector,
                                             Node** node, Node* binop,
                                             InstructionCode* opcode,
                                             FlagsCondition cond,
                                             FlagsContinuation* cont) {
  InstructionCode binop_opcode;
  InstructionCode no_output_opcode;
  switch (binop->opcode()) {
    case IrOpcode::kInt32Add:
      binop_opcode = kArmAdd;
      no_output_opcode = kArmCmn;
      break;
    case IrOpcode::kWord32And:
      binop_opcode = kArmAnd;
      no_output_opcode = kArmTst;
      break;
    case IrOpcode::kWord32Or:
      binop_opcode = kArmOrr;
      no_output_opcode = kArmOrr;
      break;
    case IrOpcode::kWord32Xor:
      binop_opcode = kArmEor;
      no_output_opcode = kArmTeq;
      break;
    default:
      UNREACHABLE();
      return;
  }
  if (selector->CanCover(*node, binop)) {
    // The comparison is the only user of {node}.
    cont->Overwrite(MapForFlagSettingBinop(cond));
    *opcode = no_output_opcode;
    *node = binop;
  } else if (selector->IsOnlyUserOfNodeInSameBlock(*node, binop)) {
    // We can also handle the case where the {node} and the comparison are in
    // the same basic block, and the comparison is the only user of {node} in
    // this basic block ({node} has users in other basic blocks).
    cont->Overwrite(MapForFlagSettingBinop(cond));
    *opcode = binop_opcode;
    *node = binop;
  }
}

// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand inputs[5];
  size_t input_count = 0;
  InstructionOperand outputs[2];
  size_t output_count = 0;
  bool has_result = (opcode != kArmCmp) && (opcode != kArmCmn) &&
                    (opcode != kArmTst) && (opcode != kArmTeq);

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

  if (has_result) {
    if (cont->IsDeoptimize()) {
      // If we can deoptimize as a result of the binop, we need to make sure
      // that the deopt inputs are not overwritten by the binop result. One way
      // to achieve that is to declare the output register as same-as-first.
      outputs[output_count++] = g.DefineSameAsFirst(node);
    } else {
      outputs[output_count++] = g.DefineAsRegister(node);
    }
  }

  if (cont->IsBranch()) {
    inputs[input_count++] = g.Label(cont->true_block());
    inputs[input_count++] = g.Label(cont->false_block());
  } else if (cont->IsSet()) {
    outputs[output_count++] = g.DefineAsRegister(cont->result());
  }

  DCHECK_NE(0u, input_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  opcode = cont->Encode(opcode);
  if (cont->IsDeoptimize()) {
    selector->EmitDeoptimize(opcode, output_count, outputs, input_count, inputs,
                             cont->reason(), cont->frame_state());
  } else if (cont->IsTrap()) {
    inputs[input_count++] = g.UseImmediate(cont->trap_id());
    selector->Emit(opcode, output_count, outputs, input_count, inputs);
  } else {
    selector->Emit(opcode, output_count, outputs, input_count, inputs);
  }
}


void VisitWordCompare(InstructionSelector* selector, Node* node,
                      FlagsContinuation* cont) {
  InstructionCode opcode = kArmCmp;
  Int32BinopMatcher m(node);

  FlagsCondition cond = cont->condition();
  if (m.right().Is(0) && (m.left().IsInt32Add() || m.left().IsWord32Or() ||
                          m.left().IsWord32And() || m.left().IsWord32Xor())) {
    // Emit flag setting instructions for comparisons against zero.
    if (CanUseFlagSettingBinop(cond)) {
      Node* binop = m.left().node();
      MaybeReplaceCmpZeroWithFlagSettingBinop(selector, &node, binop, &opcode,
                                              cond, cont);
    }
  } else if (m.left().Is(0) &&
             (m.right().IsInt32Add() || m.right().IsWord32Or() ||
              m.right().IsWord32And() || m.right().IsWord32Xor())) {
    // Same as above, but we need to commute the condition before we
    // continue with the rest of the checks.
    cond = CommuteFlagsCondition(cond);
    if (CanUseFlagSettingBinop(cond)) {
      Node* binop = m.right().node();
      MaybeReplaceCmpZeroWithFlagSettingBinop(selector, &node, binop, &opcode,
                                              cond, cont);
    }
  }

  VisitWordCompare(selector, node, opcode, cont);
}


// Shared routine for word comparisons against zero.
void VisitWordCompareZero(InstructionSelector* selector, Node* user,
                          Node* value, FlagsContinuation* cont) {
  // Try to combine with comparisons against 0 by simply inverting the branch.
  while (value->opcode() == IrOpcode::kWord32Equal &&
         selector->CanCover(user, value)) {
    Int32BinopMatcher m(value);
    if (!m.right().Is(0)) break;

    user = value;
    value = m.left().node();
    cont->Negate();
  }

  if (selector->CanCover(user, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord32Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitWordCompare(selector, value, cont);
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
              case IrOpcode::kInt32MulWithOverflow:
                // ARM doesn't set the overflow flag for multiplication, so we
                // need to test on kNotEqual. Here is the code sequence used:
                //   smull resultlow, resulthigh, left, right
                //   cmp resulthigh, Operand(resultlow, ASR, 31)
                cont->OverwriteAndNegateIfEqual(kNotEqual);
                return EmitInt32MulWithOverflow(selector, node, cont);
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
  }

  if (user->opcode() == IrOpcode::kWord32Equal) {
    return VisitWordCompare(selector, user, cont);
  }

  // Continuation could not be combined with a compare, emit compare against 0.
  ArmOperandGenerator g(selector);
  InstructionCode const opcode =
      cont->Encode(kArmTst) | AddressingModeField::encode(kMode_Operand2_R);
  InstructionOperand const value_operand = g.UseRegister(value);
  if (cont->IsBranch()) {
    selector->Emit(opcode, g.NoOutput(), value_operand, value_operand,
                   g.Label(cont->true_block()), g.Label(cont->false_block()));
  } else if (cont->IsDeoptimize()) {
    selector->EmitDeoptimize(opcode, g.NoOutput(), value_operand, value_operand,
                             cont->reason(), cont->frame_state());
  } else if (cont->IsSet()) {
    selector->Emit(opcode, g.DefineAsRegister(cont->result()), value_operand,
                   value_operand);
  } else {
    DCHECK(cont->IsTrap());
    selector->Emit(opcode, g.NoOutput(), value_operand, value_operand,
                   g.UseImmediate(cont->trap_id()));
  }
}

}  // namespace

void InstructionSelector::VisitBranch(Node* branch, BasicBlock* tbranch,
                                      BasicBlock* fbranch) {
  FlagsContinuation cont(kNotEqual, tbranch, fbranch);
  VisitWordCompareZero(this, branch, branch->InputAt(0), &cont);
}

void InstructionSelector::VisitDeoptimizeIf(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForDeoptimize(
      kNotEqual, DeoptimizeReasonOf(node->op()), node->InputAt(1));
  VisitWordCompareZero(this, node, node->InputAt(0), &cont);
}

void InstructionSelector::VisitDeoptimizeUnless(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForDeoptimize(
      kEqual, DeoptimizeReasonOf(node->op()), node->InputAt(1));
  VisitWordCompareZero(this, node, node->InputAt(0), &cont);
}

void InstructionSelector::VisitTrapIf(Node* node, Runtime::FunctionId func_id) {
  FlagsContinuation cont =
      FlagsContinuation::ForTrap(kNotEqual, func_id, node->InputAt(1));
  VisitWordCompareZero(this, node, node->InputAt(0), &cont);
}

void InstructionSelector::VisitTrapUnless(Node* node,
                                          Runtime::FunctionId func_id) {
  FlagsContinuation cont =
      FlagsContinuation::ForTrap(kEqual, func_id, node->InputAt(1));
  VisitWordCompareZero(this, node, node->InputAt(0), &cont);
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
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) {
    return VisitWordCompareZero(this, m.node(), m.left().node(), &cont);
  }
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitInt32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitInt32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitUint32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitUint32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitInt32AddWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kArmAdd, kArmAdd, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kArmAdd, kArmAdd, &cont);
}

void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kArmSub, kArmRsb, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kArmSub, kArmRsb, &cont);
}

void InstructionSelector::VisitInt32MulWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    // ARM doesn't set the overflow flag for multiplication, so we need to test
    // on kNotEqual. Here is the code sequence used:
    //   smull resultlow, resulthigh, left, right
    //   cmp resulthigh, Operand(resultlow, ASR, 31)
    FlagsContinuation cont = FlagsContinuation::ForSet(kNotEqual, ovf);
    return EmitInt32MulWithOverflow(this, node, &cont);
  }
  FlagsContinuation cont;
  EmitInt32MulWithOverflow(this, node, &cont);
}

void InstructionSelector::VisitFloat32Equal(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kFloatLessThan, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kFloatLessThanOrEqual, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64Equal(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kFloatLessThan, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kFloatLessThanOrEqual, node);
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

void InstructionSelector::VisitAtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  ArchOpcode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsSigned() ? kAtomicLoadInt8 : kAtomicLoadUint8;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsSigned() ? kAtomicLoadInt16 : kAtomicLoadUint16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kAtomicLoadWord32;
      break;
    default:
      UNREACHABLE();
      return;
  }
  Emit(opcode | AddressingModeField::encode(kMode_Offset_RR),
       g.DefineAsRegister(node), g.UseRegister(base), g.UseRegister(index));
}

void InstructionSelector::VisitAtomicStore(Node* node) {
  MachineRepresentation rep = AtomicStoreRepresentationOf(node->op());
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  ArchOpcode opcode = kArchNop;
  switch (rep) {
    case MachineRepresentation::kWord8:
      opcode = kAtomicStoreWord8;
      break;
    case MachineRepresentation::kWord16:
      opcode = kAtomicStoreWord16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kAtomicStoreWord32;
      break;
    default:
      UNREACHABLE();
      return;
  }

  AddressingMode addressing_mode = kMode_Offset_RR;
  InstructionOperand inputs[4];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 0, nullptr, input_count, inputs);
}

void InstructionSelector::VisitCreateFloat32x4(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmFloat32x4Splat, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}

void InstructionSelector::VisitFloat32x4ExtractLane(Node* node) {
  ArmOperandGenerator g(this);
  int32_t lane = OpParameter<int32_t>(node);
  Emit(kArmFloat32x4ExtractLane, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseImmediate(lane));
}

void InstructionSelector::VisitFloat32x4ReplaceLane(Node* node) {
  ArmOperandGenerator g(this);
  int32_t lane = OpParameter<int32_t>(node);
  Emit(kArmFloat32x4ReplaceLane, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseImmediate(lane),
       g.Use(node->InputAt(1)));
}

void InstructionSelector::VisitFloat32x4FromInt32x4(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmFloat32x4FromInt32x4, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitFloat32x4FromUint32x4(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmFloat32x4FromUint32x4, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitFloat32x4Abs(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmFloat32x4Abs, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitFloat32x4Neg(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmFloat32x4Neg, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitFloat32x4Add(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmFloat32x4Add, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitFloat32x4Sub(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmFloat32x4Sub, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitFloat32x4Equal(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmFloat32x4Eq, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitFloat32x4NotEqual(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmFloat32x4Ne, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitCreateInt32x4(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmInt32x4Splat, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}

void InstructionSelector::VisitInt32x4ExtractLane(Node* node) {
  ArmOperandGenerator g(this);
  int32_t lane = OpParameter<int32_t>(node);
  Emit(kArmInt32x4ExtractLane, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseImmediate(lane));
}

void InstructionSelector::VisitInt32x4ReplaceLane(Node* node) {
  ArmOperandGenerator g(this);
  int32_t lane = OpParameter<int32_t>(node);
  Emit(kArmInt32x4ReplaceLane, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseImmediate(lane),
       g.Use(node->InputAt(1)));
}

void InstructionSelector::VisitInt32x4FromFloat32x4(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmInt32x4FromFloat32x4, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitUint32x4FromFloat32x4(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmUint32x4FromFloat32x4, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitInt32x4Add(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmInt32x4Add, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitInt32x4Sub(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmInt32x4Sub, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitInt32x4Equal(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmInt32x4Eq, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)),
       g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitInt32x4NotEqual(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmInt32x4Ne, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)),
       g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitSimd32x4Select(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmSimd32x4Select, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)),
       g.UseRegister(node->InputAt(2)));
}

// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags;
  if (CpuFeatures::IsSupported(SUDIV)) {
    // The sdiv and udiv instructions correctly return 0 if the divisor is 0,
    // but the fall-back implementation does not.
    flags |= MachineOperatorBuilder::kInt32DivIsSafe |
             MachineOperatorBuilder::kUint32DivIsSafe;
  }
  if (CpuFeatures::IsSupported(ARMv7)) {
    flags |= MachineOperatorBuilder::kWord32ReverseBits;
  }
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

// static
MachineOperatorBuilder::AlignmentRequirements
InstructionSelector::AlignmentRequirements() {
  Vector<MachineType> req_aligned = Vector<MachineType>::New(2);
  req_aligned[0] = MachineType::Float32();
  req_aligned[1] = MachineType::Float64();
  return MachineOperatorBuilder::AlignmentRequirements::
      SomeUnalignedAccessUnsupported(req_aligned, req_aligned);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
