// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

// Adds IA32-specific methods for generating operands.
class IA32OperandGenerator FINAL : public OperandGenerator {
 public:
  explicit IA32OperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand* UseByteRegister(Node* node) {
    // TODO(dcarney): relax constraint.
    return UseFixed(node, edx);
  }

  bool CanBeImmediate(Node* node) {
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
      case IrOpcode::kNumberConstant:
      case IrOpcode::kExternalConstant:
        return true;
      case IrOpcode::kHeapConstant: {
        // Constants in new space cannot be used as immediates in V8 because
        // the GC does not scan code objects when collecting the new generation.
        Unique<HeapObject> value = OpParameter<Unique<HeapObject> >(node);
        return !isolate()->heap()->InNewSpace(*value.handle());
      }
      default:
        return false;
    }
  }

  AddressingMode GenerateMemoryOperandInputs(Node* index, int scale, Node* base,
                                             Node* displacement_node,
                                             InstructionOperand* inputs[],
                                             size_t* input_count) {
    AddressingMode mode = kMode_MRI;
    int32_t displacement = (displacement_node == NULL)
                               ? 0
                               : OpParameter<int32_t>(displacement_node);
    if (base != NULL) {
      if (base->opcode() == IrOpcode::kInt32Constant) {
        displacement += OpParameter<int32_t>(base);
        base = NULL;
      }
    }
    if (base != NULL) {
      inputs[(*input_count)++] = UseRegister(base);
      if (index != NULL) {
        DCHECK(scale >= 0 && scale <= 3);
        inputs[(*input_count)++] = UseRegister(index);
        if (displacement != 0) {
          inputs[(*input_count)++] = TempImmediate(displacement);
          static const AddressingMode kMRnI_modes[] = {kMode_MR1I, kMode_MR2I,
                                                       kMode_MR4I, kMode_MR8I};
          mode = kMRnI_modes[scale];
        } else {
          static const AddressingMode kMRn_modes[] = {kMode_MR1, kMode_MR2,
                                                      kMode_MR4, kMode_MR8};
          mode = kMRn_modes[scale];
        }
      } else {
        if (displacement == 0) {
          mode = kMode_MR;
        } else {
          inputs[(*input_count)++] = TempImmediate(displacement);
          mode = kMode_MRI;
        }
      }
    } else {
      DCHECK(scale >= 0 && scale <= 3);
      if (index != NULL) {
        inputs[(*input_count)++] = UseRegister(index);
        if (displacement != 0) {
          inputs[(*input_count)++] = TempImmediate(displacement);
          static const AddressingMode kMnI_modes[] = {kMode_MRI, kMode_M2I,
                                                      kMode_M4I, kMode_M8I};
          mode = kMnI_modes[scale];
        } else {
          static const AddressingMode kMn_modes[] = {kMode_MR, kMode_M2,
                                                     kMode_M4, kMode_M8};
          mode = kMn_modes[scale];
        }
      } else {
        inputs[(*input_count)++] = TempImmediate(displacement);
        return kMode_MI;
      }
    }
    return mode;
  }

  AddressingMode GetEffectiveAddressMemoryOperand(Node* node,
                                                  InstructionOperand* inputs[],
                                                  size_t* input_count) {
    BaseWithIndexAndDisplacement32Matcher m(node, true);
    DCHECK(m.matches());
    if ((m.displacement() == NULL || CanBeImmediate(m.displacement()))) {
      return GenerateMemoryOperandInputs(m.index(), m.scale(), m.base(),
                                         m.displacement(), inputs, input_count);
    } else {
      inputs[(*input_count)++] = UseRegister(node->InputAt(0));
      inputs[(*input_count)++] = UseRegister(node->InputAt(1));
      return kMode_MR1;
    }
  }

  bool CanBeBetterLeftOperand(Node* node) const {
    return !selector()->IsLive(node);
  }
};


static void VisitRRFloat64(InstructionSelector* selector, ArchOpcode opcode,
                           Node* node) {
  IA32OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitLoad(Node* node) {
  MachineType rep = RepresentationOf(OpParameter<LoadRepresentation>(node));
  MachineType typ = TypeOf(OpParameter<LoadRepresentation>(node));

  ArchOpcode opcode;
  // TODO(titzer): signed/unsigned small loads
  switch (rep) {
    case kRepFloat32:
      opcode = kIA32Movss;
      break;
    case kRepFloat64:
      opcode = kIA32Movsd;
      break;
    case kRepBit:  // Fall through.
    case kRepWord8:
      opcode = typ == kTypeInt32 ? kIA32Movsxbl : kIA32Movzxbl;
      break;
    case kRepWord16:
      opcode = typ == kTypeInt32 ? kIA32Movsxwl : kIA32Movzxwl;
      break;
    case kRepTagged:  // Fall through.
    case kRepWord32:
      opcode = kIA32Movl;
      break;
    default:
      UNREACHABLE();
      return;
  }

  IA32OperandGenerator g(this);
  InstructionOperand* outputs[1];
  outputs[0] = g.DefineAsRegister(node);
  InstructionOperand* inputs[3];
  size_t input_count = 0;
  AddressingMode mode =
      g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
  InstructionCode code = opcode | AddressingModeField::encode(mode);
  Emit(code, 1, outputs, input_count, inputs);
}


void InstructionSelector::VisitStore(Node* node) {
  IA32OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = OpParameter<StoreRepresentation>(node);
  MachineType rep = RepresentationOf(store_rep.machine_type());
  if (store_rep.write_barrier_kind() == kFullWriteBarrier) {
    DCHECK_EQ(kRepTagged, rep);
    // TODO(dcarney): refactor RecordWrite function to take temp registers
    //                and pass them here instead of using fixed regs
    // TODO(dcarney): handle immediate indices.
    InstructionOperand* temps[] = {g.TempRegister(ecx), g.TempRegister(edx)};
    Emit(kIA32StoreWriteBarrier, NULL, g.UseFixed(base, ebx),
         g.UseFixed(index, ecx), g.UseFixed(value, edx), arraysize(temps),
         temps);
    return;
  }
  DCHECK_EQ(kNoWriteBarrier, store_rep.write_barrier_kind());

  ArchOpcode opcode;
  switch (rep) {
    case kRepFloat32:
      opcode = kIA32Movss;
      break;
    case kRepFloat64:
      opcode = kIA32Movsd;
      break;
    case kRepBit:  // Fall through.
    case kRepWord8:
      opcode = kIA32Movb;
      break;
    case kRepWord16:
      opcode = kIA32Movw;
      break;
    case kRepTagged:  // Fall through.
    case kRepWord32:
      opcode = kIA32Movl;
      break;
    default:
      UNREACHABLE();
      return;
  }

  InstructionOperand* val;
  if (g.CanBeImmediate(value)) {
    val = g.UseImmediate(value);
  } else if (rep == kRepWord8 || rep == kRepBit) {
    val = g.UseByteRegister(value);
  } else {
    val = g.UseRegister(value);
  }

  InstructionOperand* inputs[4];
  size_t input_count = 0;
  AddressingMode mode =
      g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
  InstructionCode code = opcode | AddressingModeField::encode(mode);
  inputs[input_count++] = val;
  Emit(code, 0, static_cast<InstructionOperand**>(NULL), input_count, inputs);
}


void InstructionSelector::VisitCheckedLoad(Node* node) {
  MachineType rep = RepresentationOf(OpParameter<MachineType>(node));
  MachineType typ = TypeOf(OpParameter<MachineType>(node));
  IA32OperandGenerator g(this);
  Node* const buffer = node->InputAt(0);
  Node* const offset = node->InputAt(1);
  Node* const length = node->InputAt(2);
  ArchOpcode opcode;
  switch (rep) {
    case kRepWord8:
      opcode = typ == kTypeInt32 ? kCheckedLoadInt8 : kCheckedLoadUint8;
      break;
    case kRepWord16:
      opcode = typ == kTypeInt32 ? kCheckedLoadInt16 : kCheckedLoadUint16;
      break;
    case kRepWord32:
      opcode = kCheckedLoadWord32;
      break;
    case kRepFloat32:
      opcode = kCheckedLoadFloat32;
      break;
    case kRepFloat64:
      opcode = kCheckedLoadFloat64;
      break;
    default:
      UNREACHABLE();
      return;
  }
  InstructionOperand* offset_operand = g.UseRegister(offset);
  InstructionOperand* length_operand =
      g.CanBeImmediate(length) ? g.UseImmediate(length) : g.UseRegister(length);
  if (g.CanBeImmediate(buffer)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), offset_operand, length_operand,
         offset_operand, g.UseImmediate(buffer));
  } else {
    Emit(opcode | AddressingModeField::encode(kMode_MR1),
         g.DefineAsRegister(node), offset_operand, length_operand,
         g.UseRegister(buffer), offset_operand);
  }
}


void InstructionSelector::VisitCheckedStore(Node* node) {
  MachineType rep = RepresentationOf(OpParameter<MachineType>(node));
  IA32OperandGenerator g(this);
  Node* const buffer = node->InputAt(0);
  Node* const offset = node->InputAt(1);
  Node* const length = node->InputAt(2);
  Node* const value = node->InputAt(3);
  ArchOpcode opcode;
  switch (rep) {
    case kRepWord8:
      opcode = kCheckedStoreWord8;
      break;
    case kRepWord16:
      opcode = kCheckedStoreWord16;
      break;
    case kRepWord32:
      opcode = kCheckedStoreWord32;
      break;
    case kRepFloat32:
      opcode = kCheckedStoreFloat32;
      break;
    case kRepFloat64:
      opcode = kCheckedStoreFloat64;
      break;
    default:
      UNREACHABLE();
      return;
  }
  InstructionOperand* value_operand =
      g.CanBeImmediate(value)
          ? g.UseImmediate(value)
          : ((rep == kRepWord8 || rep == kRepBit) ? g.UseByteRegister(value)
                                                  : g.UseRegister(value));
  InstructionOperand* offset_operand = g.UseRegister(offset);
  InstructionOperand* length_operand =
      g.CanBeImmediate(length) ? g.UseImmediate(length) : g.UseRegister(length);
  if (g.CanBeImmediate(buffer)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), nullptr,
         offset_operand, length_operand, value_operand, offset_operand,
         g.UseImmediate(buffer));
  } else {
    Emit(opcode | AddressingModeField::encode(kMode_MR1), nullptr,
         offset_operand, length_operand, value_operand, g.UseRegister(buffer),
         offset_operand);
  }
}


// Shared routine for multiple binary operations.
static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, FlagsContinuation* cont) {
  IA32OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();
  InstructionOperand* inputs[4];
  size_t input_count = 0;
  InstructionOperand* outputs[2];
  size_t output_count = 0;

  // TODO(turbofan): match complex addressing modes.
  if (left == right) {
    // If both inputs refer to the same operand, enforce allocating a register
    // for both of them to ensure that we don't end up generating code like
    // this:
    //
    //   mov eax, [ebp-0x10]
    //   add eax, [ebp-0x10]
    //   jo label
    InstructionOperand* const input = g.UseRegister(left);
    inputs[input_count++] = input;
    inputs[input_count++] = input;
  } else if (g.CanBeImmediate(right)) {
    inputs[input_count++] = g.UseRegister(left);
    inputs[input_count++] = g.UseImmediate(right);
  } else {
    if (node->op()->HasProperty(Operator::kCommutative) &&
        g.CanBeBetterLeftOperand(right)) {
      std::swap(left, right);
    }
    inputs[input_count++] = g.UseRegister(left);
    inputs[input_count++] = g.Use(right);
  }

  if (cont->IsBranch()) {
    inputs[input_count++] = g.Label(cont->true_block());
    inputs[input_count++] = g.Label(cont->false_block());
  }

  outputs[output_count++] = g.DefineSameAsFirst(node);
  if (cont->IsSet()) {
    // TODO(turbofan): Use byte register here.
    outputs[output_count++] = g.DefineAsRegister(cont->result());
  }

  DCHECK_NE(0, input_count);
  DCHECK_NE(0, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  Instruction* instr = selector->Emit(cont->Encode(opcode), output_count,
                                      outputs, input_count, inputs);
  if (cont->IsBranch()) instr->MarkAsControl();
}


// Shared routine for multiple binary operations.
static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode) {
  FlagsContinuation cont;
  VisitBinop(selector, node, opcode, &cont);
}


void InstructionSelector::VisitWord32And(Node* node) {
  VisitBinop(this, node, kIA32And);
}


void InstructionSelector::VisitWord32Or(Node* node) {
  VisitBinop(this, node, kIA32Or);
}


void InstructionSelector::VisitWord32Xor(Node* node) {
  IA32OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.right().Is(-1)) {
    Emit(kIA32Not, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()));
  } else {
    VisitBinop(this, node, kIA32Xor);
  }
}


// Shared routine for multiple shift operations.
static inline void VisitShift(InstructionSelector* selector, Node* node,
                              ArchOpcode opcode) {
  IA32OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  if (g.CanBeImmediate(right)) {
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.UseImmediate(right));
  } else {
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.UseFixed(right, ecx));
  }
}


namespace {

void VisitMulHigh(InstructionSelector* selector, Node* node,
                  ArchOpcode opcode) {
  IA32OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsFixed(node, edx),
                 g.UseFixed(node->InputAt(0), eax),
                 g.UseUniqueRegister(node->InputAt(1)));
}


void VisitDiv(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  IA32OperandGenerator g(selector);
  InstructionOperand* temps[] = {g.TempRegister(edx)};
  selector->Emit(opcode, g.DefineAsFixed(node, eax),
                 g.UseFixed(node->InputAt(0), eax),
                 g.UseUnique(node->InputAt(1)), arraysize(temps), temps);
}


void VisitMod(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  IA32OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsFixed(node, edx),
                 g.UseFixed(node->InputAt(0), eax),
                 g.UseUnique(node->InputAt(1)));
}

void EmitLea(InstructionSelector* selector, Node* result, Node* index,
             int scale, Node* base, Node* displacement) {
  IA32OperandGenerator g(selector);
  InstructionOperand* inputs[4];
  size_t input_count = 0;
  AddressingMode mode = g.GenerateMemoryOperandInputs(
      index, scale, base, displacement, inputs, &input_count);

  DCHECK_NE(0, static_cast<int>(input_count));
  DCHECK_GE(arraysize(inputs), input_count);

  InstructionOperand* outputs[1];
  outputs[0] = g.DefineAsRegister(result);

  InstructionCode opcode = AddressingModeField::encode(mode) | kIA32Lea;

  selector->Emit(opcode, 1, outputs, input_count, inputs);
}

}  // namespace


void InstructionSelector::VisitWord32Shl(Node* node) {
  Int32ScaleMatcher m(node, true);
  if (m.matches()) {
    Node* index = node->InputAt(0);
    Node* base = m.power_of_two_plus_one() ? index : NULL;
    EmitLea(this, node, index, m.scale(), base, NULL);
    return;
  }
  VisitShift(this, node, kIA32Shl);
}


void InstructionSelector::VisitWord32Shr(Node* node) {
  VisitShift(this, node, kIA32Shr);
}


void InstructionSelector::VisitWord32Sar(Node* node) {
  VisitShift(this, node, kIA32Sar);
}


void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitShift(this, node, kIA32Ror);
}


void InstructionSelector::VisitInt32Add(Node* node) {
  IA32OperandGenerator g(this);

  // Try to match the Add to a lea pattern
  BaseWithIndexAndDisplacement32Matcher m(node);
  if (m.matches() &&
      (m.displacement() == NULL || g.CanBeImmediate(m.displacement()))) {
    InstructionOperand* inputs[4];
    size_t input_count = 0;
    AddressingMode mode = g.GenerateMemoryOperandInputs(
        m.index(), m.scale(), m.base(), m.displacement(), inputs, &input_count);

    DCHECK_NE(0, static_cast<int>(input_count));
    DCHECK_GE(arraysize(inputs), input_count);

    InstructionOperand* outputs[1];
    outputs[0] = g.DefineAsRegister(node);

    InstructionCode opcode = AddressingModeField::encode(mode) | kIA32Lea;
    Emit(opcode, 1, outputs, input_count, inputs);
    return;
  }

  // No lea pattern match, use add
  VisitBinop(this, node, kIA32Add);
}


void InstructionSelector::VisitInt32Sub(Node* node) {
  IA32OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().Is(0)) {
    Emit(kIA32Neg, g.DefineSameAsFirst(node), g.Use(m.right().node()));
  } else {
    VisitBinop(this, node, kIA32Sub);
  }
}


void InstructionSelector::VisitInt32Mul(Node* node) {
  Int32ScaleMatcher m(node, true);
  if (m.matches()) {
    Node* index = node->InputAt(0);
    Node* base = m.power_of_two_plus_one() ? index : NULL;
    EmitLea(this, node, index, m.scale(), base, NULL);
    return;
  }
  IA32OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (g.CanBeImmediate(right)) {
    Emit(kIA32Imul, g.DefineAsRegister(node), g.Use(left),
         g.UseImmediate(right));
  } else {
    if (g.CanBeBetterLeftOperand(right)) {
      std::swap(left, right);
    }
    Emit(kIA32Imul, g.DefineSameAsFirst(node), g.UseRegister(left),
         g.Use(right));
  }
}


void InstructionSelector::VisitInt32MulHigh(Node* node) {
  VisitMulHigh(this, node, kIA32ImulHigh);
}


void InstructionSelector::VisitUint32MulHigh(Node* node) {
  VisitMulHigh(this, node, kIA32UmulHigh);
}


void InstructionSelector::VisitInt32Div(Node* node) {
  VisitDiv(this, node, kIA32Idiv);
}


void InstructionSelector::VisitUint32Div(Node* node) {
  VisitDiv(this, node, kIA32Udiv);
}


void InstructionSelector::VisitInt32Mod(Node* node) {
  VisitMod(this, node, kIA32Idiv);
}


void InstructionSelector::VisitUint32Mod(Node* node) {
  VisitMod(this, node, kIA32Udiv);
}


void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  IA32OperandGenerator g(this);
  Emit(kSSECvtss2sd, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  IA32OperandGenerator g(this);
  Emit(kSSEInt32ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  IA32OperandGenerator g(this);
  Emit(kSSEUint32ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  IA32OperandGenerator g(this);
  Emit(kSSEFloat64ToInt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  IA32OperandGenerator g(this);
  Emit(kSSEFloat64ToUint32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  IA32OperandGenerator g(this);
  Emit(kSSECvtsd2ss, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64Add(Node* node) {
  IA32OperandGenerator g(this);
  if (IsSupported(AVX)) {
    Emit(kAVXFloat64Add, g.DefineAsRegister(node),
         g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)));
  } else {
    Emit(kSSEFloat64Add, g.DefineSameAsFirst(node),
         g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)));
  }
}


void InstructionSelector::VisitFloat64Sub(Node* node) {
  IA32OperandGenerator g(this);
  if (IsSupported(AVX)) {
    Emit(kAVXFloat64Sub, g.DefineAsRegister(node),
         g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)));
  } else {
    Emit(kSSEFloat64Sub, g.DefineSameAsFirst(node),
         g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)));
  }
}


void InstructionSelector::VisitFloat64Mul(Node* node) {
  IA32OperandGenerator g(this);
  if (IsSupported(AVX)) {
    Emit(kAVXFloat64Mul, g.DefineAsRegister(node),
         g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)));
  } else {
    Emit(kSSEFloat64Mul, g.DefineSameAsFirst(node),
         g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)));
  }
}


void InstructionSelector::VisitFloat64Div(Node* node) {
  IA32OperandGenerator g(this);
  if (IsSupported(AVX)) {
    Emit(kAVXFloat64Div, g.DefineAsRegister(node),
         g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)));
  } else {
    Emit(kSSEFloat64Div, g.DefineSameAsFirst(node),
         g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)));
  }
}


void InstructionSelector::VisitFloat64Mod(Node* node) {
  IA32OperandGenerator g(this);
  InstructionOperand* temps[] = {g.TempRegister(eax)};
  Emit(kSSEFloat64Mod, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)), 1,
       temps);
}


void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  IA32OperandGenerator g(this);
  Emit(kSSEFloat64Sqrt, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64Floor(Node* node) {
  DCHECK(CpuFeatures::IsSupported(SSE4_1));
  VisitRRFloat64(this, kSSEFloat64Floor, node);
}


void InstructionSelector::VisitFloat64Ceil(Node* node) {
  DCHECK(CpuFeatures::IsSupported(SSE4_1));
  VisitRRFloat64(this, kSSEFloat64Ceil, node);
}


void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  DCHECK(CpuFeatures::IsSupported(SSE4_1));
  VisitRRFloat64(this, kSSEFloat64RoundTruncate, node);
}


void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  UNREACHABLE();
}


void InstructionSelector::VisitCall(Node* node) {
  IA32OperandGenerator g(this);
  const CallDescriptor* descriptor = OpParameter<const CallDescriptor*>(node);

  FrameStateDescriptor* frame_state_descriptor = NULL;

  if (descriptor->NeedsFrameState()) {
    frame_state_descriptor =
        GetFrameStateDescriptor(node->InputAt(descriptor->InputCount()));
  }

  CallBuffer buffer(zone(), descriptor, frame_state_descriptor);

  // Compute InstructionOperands for inputs and outputs.
  InitializeCallBuffer(node, &buffer, true, true);

  // Push any stack arguments.
  for (NodeVectorRIter input = buffer.pushed_nodes.rbegin();
       input != buffer.pushed_nodes.rend(); input++) {
    // TODO(titzer): handle pushing double parameters.
    Emit(kIA32Push, NULL,
         g.CanBeImmediate(*input) ? g.UseImmediate(*input) : g.Use(*input));
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

// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  InstructionOperand* left, InstructionOperand* right,
                  FlagsContinuation* cont) {
  IA32OperandGenerator g(selector);
  if (cont->IsBranch()) {
    selector->Emit(cont->Encode(opcode), NULL, left, right,
                   g.Label(cont->true_block()),
                   g.Label(cont->false_block()))->MarkAsControl();
  } else {
    DCHECK(cont->IsSet());
    // TODO(titzer): Needs byte register.
    selector->Emit(cont->Encode(opcode), g.DefineAsRegister(cont->result()),
                   left, right);
  }
}


// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  Node* left, Node* right, FlagsContinuation* cont,
                  bool commutative) {
  IA32OperandGenerator g(selector);
  if (commutative && g.CanBeBetterLeftOperand(right)) {
    std::swap(left, right);
  }
  VisitCompare(selector, opcode, g.UseRegister(left), g.Use(right), cont);
}


// Shared routine for multiple float compare operations.
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  VisitCompare(selector, kSSEFloat64Cmp, node->InputAt(0), node->InputAt(1),
               cont, node->op()->HasProperty(Operator::kCommutative));
}


// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont) {
  IA32OperandGenerator g(selector);
  Node* const left = node->InputAt(0);
  Node* const right = node->InputAt(1);

  // Match immediates on left or right side of comparison.
  if (g.CanBeImmediate(right)) {
    VisitCompare(selector, opcode, g.Use(left), g.UseImmediate(right), cont);
  } else if (g.CanBeImmediate(left)) {
    if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
    VisitCompare(selector, opcode, g.Use(right), g.UseImmediate(left), cont);
  } else {
    VisitCompare(selector, opcode, left, right, cont,
                 node->op()->HasProperty(Operator::kCommutative));
  }
}


void VisitWordCompare(InstructionSelector* selector, Node* node,
                      FlagsContinuation* cont) {
  VisitWordCompare(selector, node, kIA32Cmp, cont);
}


// Shared routine for word comparison with zero.
void VisitWordCompareZero(InstructionSelector* selector, Node* user,
                          Node* value, FlagsContinuation* cont) {
  // Try to combine the branch with a comparison.
  while (selector->CanCover(user, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord32Equal: {
        // Try to combine with comparisons against 0 by simply inverting the
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
          Node* node = value->InputAt(0);
          Node* result = node->FindProjection(0);
          if (result == NULL || selector->IsDefined(result)) {
            switch (node->opcode()) {
              case IrOpcode::kInt32AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kIA32Add, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kIA32Sub, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Sub:
        return VisitWordCompare(selector, value, cont);
      case IrOpcode::kWord32And:
        return VisitWordCompare(selector, value, kIA32Test, cont);
      default:
        break;
    }
    break;
  }

  // Continuation could not be combined with a compare, emit compare against 0.
  IA32OperandGenerator g(selector);
  VisitCompare(selector, kIA32Cmp, g.Use(value), g.TempImmediate(0), cont);
}

}  // namespace


void InstructionSelector::VisitBranch(Node* branch, BasicBlock* tbranch,
                                      BasicBlock* fbranch) {
  FlagsContinuation cont(kNotEqual, tbranch, fbranch);
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
    return VisitBinop(this, node, kIA32Add, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kIA32Add, &cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = node->FindProjection(1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitBinop(this, node, kIA32Sub, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kIA32Sub, &cont);
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
  if (CpuFeatures::IsSupported(SSE4_1)) {
    return MachineOperatorBuilder::kFloat64Floor |
           MachineOperatorBuilder::kFloat64Ceil |
           MachineOperatorBuilder::kFloat64RoundTruncate |
           MachineOperatorBuilder::kWord32ShiftIsSafe;
  }
  return MachineOperatorBuilder::Flag::kNoFlags;
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8
