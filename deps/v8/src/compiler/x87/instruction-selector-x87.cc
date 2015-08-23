// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/adapters.h"
#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

// Adds X87-specific methods for generating operands.
class X87OperandGenerator final : public OperandGenerator {
 public:
  explicit X87OperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand UseByteRegister(Node* node) {
    // TODO(titzer): encode byte register use constraints.
    return UseFixed(node, edx);
  }

  InstructionOperand DefineAsByteRegister(Node* node) {
    // TODO(titzer): encode byte register def constraints.
    return DefineAsRegister(node);
  }

  InstructionOperand CreateImmediate(int imm) {
    return sequence()->AddImmediate(Constant(imm));
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
        Isolate* isolate = value.handle()->GetIsolate();
        return !isolate->heap()->InNewSpace(*value.handle());
      }
      default:
        return false;
    }
  }

  AddressingMode GenerateMemoryOperandInputs(Node* index, int scale, Node* base,
                                             Node* displacement_node,
                                             InstructionOperand inputs[],
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
                                                  InstructionOperand inputs[],
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


void InstructionSelector::VisitLoad(Node* node) {
  MachineType rep = RepresentationOf(OpParameter<LoadRepresentation>(node));
  MachineType typ = TypeOf(OpParameter<LoadRepresentation>(node));

  ArchOpcode opcode;
  switch (rep) {
    case kRepFloat32:
      opcode = kX87Movss;
      break;
    case kRepFloat64:
      opcode = kX87Movsd;
      break;
    case kRepBit:  // Fall through.
    case kRepWord8:
      opcode = typ == kTypeInt32 ? kX87Movsxbl : kX87Movzxbl;
      break;
    case kRepWord16:
      opcode = typ == kTypeInt32 ? kX87Movsxwl : kX87Movzxwl;
      break;
    case kRepTagged:  // Fall through.
    case kRepWord32:
      opcode = kX87Movl;
      break;
    default:
      UNREACHABLE();
      return;
  }

  X87OperandGenerator g(this);
  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(node);
  InstructionOperand inputs[3];
  size_t input_count = 0;
  AddressingMode mode =
      g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
  InstructionCode code = opcode | AddressingModeField::encode(mode);
  Emit(code, 1, outputs, input_count, inputs);
}


void InstructionSelector::VisitStore(Node* node) {
  X87OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = OpParameter<StoreRepresentation>(node);
  MachineType rep = RepresentationOf(store_rep.machine_type());
  if (store_rep.write_barrier_kind() == kFullWriteBarrier) {
    DCHECK_EQ(kRepTagged, rep);
    // TODO(dcarney): refactor RecordWrite function to take temp registers
    //                and pass them here instead of using fixed regs
    if (g.CanBeImmediate(index)) {
      InstructionOperand temps[] = {g.TempRegister(ecx), g.TempRegister()};
      Emit(kX87StoreWriteBarrier, g.NoOutput(), g.UseFixed(base, ebx),
           g.UseImmediate(index), g.UseFixed(value, ecx), arraysize(temps),
           temps);
    } else {
      InstructionOperand temps[] = {g.TempRegister(ecx), g.TempRegister(edx)};
      Emit(kX87StoreWriteBarrier, g.NoOutput(), g.UseFixed(base, ebx),
           g.UseFixed(index, ecx), g.UseFixed(value, edx), arraysize(temps),
           temps);
    }
    return;
  }
  DCHECK_EQ(kNoWriteBarrier, store_rep.write_barrier_kind());

  ArchOpcode opcode;
  switch (rep) {
    case kRepFloat32:
      opcode = kX87Movss;
      break;
    case kRepFloat64:
      opcode = kX87Movsd;
      break;
    case kRepBit:  // Fall through.
    case kRepWord8:
      opcode = kX87Movb;
      break;
    case kRepWord16:
      opcode = kX87Movw;
      break;
    case kRepTagged:  // Fall through.
    case kRepWord32:
      opcode = kX87Movl;
      break;
    default:
      UNREACHABLE();
      return;
  }

  InstructionOperand val;
  if (g.CanBeImmediate(value)) {
    val = g.UseImmediate(value);
  } else if (rep == kRepWord8 || rep == kRepBit) {
    val = g.UseByteRegister(value);
  } else {
    val = g.UseRegister(value);
  }

  InstructionOperand inputs[4];
  size_t input_count = 0;
  AddressingMode mode =
      g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
  InstructionCode code = opcode | AddressingModeField::encode(mode);
  inputs[input_count++] = val;
  Emit(code, 0, static_cast<InstructionOperand*>(NULL), input_count, inputs);
}


void InstructionSelector::VisitCheckedLoad(Node* node) {
  MachineType rep = RepresentationOf(OpParameter<MachineType>(node));
  MachineType typ = TypeOf(OpParameter<MachineType>(node));
  X87OperandGenerator g(this);
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
  InstructionOperand offset_operand = g.UseRegister(offset);
  InstructionOperand length_operand =
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
  X87OperandGenerator g(this);
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
  InstructionOperand value_operand =
      g.CanBeImmediate(value)
          ? g.UseImmediate(value)
          : ((rep == kRepWord8 || rep == kRepBit) ? g.UseByteRegister(value)
                                                  : g.UseRegister(value));
  InstructionOperand offset_operand = g.UseRegister(offset);
  InstructionOperand length_operand =
      g.CanBeImmediate(length) ? g.UseImmediate(length) : g.UseRegister(length);
  if (g.CanBeImmediate(buffer)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         offset_operand, length_operand, value_operand, offset_operand,
         g.UseImmediate(buffer));
  } else {
    Emit(opcode | AddressingModeField::encode(kMode_MR1), g.NoOutput(),
         offset_operand, length_operand, value_operand, g.UseRegister(buffer),
         offset_operand);
  }
}


// Shared routine for multiple binary operations.
static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, FlagsContinuation* cont) {
  X87OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();
  InstructionOperand inputs[4];
  size_t input_count = 0;
  InstructionOperand outputs[2];
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
    InstructionOperand const input = g.UseRegister(left);
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
    outputs[output_count++] = g.DefineAsRegister(cont->result());
  }

  DCHECK_NE(0u, input_count);
  DCHECK_NE(0u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->Emit(cont->Encode(opcode), output_count, outputs, input_count,
                 inputs);
}


// Shared routine for multiple binary operations.
static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode) {
  FlagsContinuation cont;
  VisitBinop(selector, node, opcode, &cont);
}


void InstructionSelector::VisitWord32And(Node* node) {
  VisitBinop(this, node, kX87And);
}


void InstructionSelector::VisitWord32Or(Node* node) {
  VisitBinop(this, node, kX87Or);
}


void InstructionSelector::VisitWord32Xor(Node* node) {
  X87OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.right().Is(-1)) {
    Emit(kX87Not, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()));
  } else {
    VisitBinop(this, node, kX87Xor);
  }
}


// Shared routine for multiple shift operations.
static inline void VisitShift(InstructionSelector* selector, Node* node,
                              ArchOpcode opcode) {
  X87OperandGenerator g(selector);
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
  X87OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsFixed(node, edx),
                 g.UseFixed(node->InputAt(0), eax),
                 g.UseUniqueRegister(node->InputAt(1)));
}


void VisitDiv(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  X87OperandGenerator g(selector);
  InstructionOperand temps[] = {g.TempRegister(edx)};
  selector->Emit(opcode, g.DefineAsFixed(node, eax),
                 g.UseFixed(node->InputAt(0), eax),
                 g.UseUnique(node->InputAt(1)), arraysize(temps), temps);
}


void VisitMod(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  X87OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsFixed(node, edx),
                 g.UseFixed(node->InputAt(0), eax),
                 g.UseUnique(node->InputAt(1)));
}

void EmitLea(InstructionSelector* selector, Node* result, Node* index,
             int scale, Node* base, Node* displacement) {
  X87OperandGenerator g(selector);
  InstructionOperand inputs[4];
  size_t input_count = 0;
  AddressingMode mode = g.GenerateMemoryOperandInputs(
      index, scale, base, displacement, inputs, &input_count);

  DCHECK_NE(0u, input_count);
  DCHECK_GE(arraysize(inputs), input_count);

  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(result);

  InstructionCode opcode = AddressingModeField::encode(mode) | kX87Lea;

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
  VisitShift(this, node, kX87Shl);
}


void InstructionSelector::VisitWord32Shr(Node* node) {
  VisitShift(this, node, kX87Shr);
}


void InstructionSelector::VisitWord32Sar(Node* node) {
  VisitShift(this, node, kX87Sar);
}


void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitShift(this, node, kX87Ror);
}


void InstructionSelector::VisitWord32Clz(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87Lzcnt, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitInt32Add(Node* node) {
  X87OperandGenerator g(this);

  // Try to match the Add to a lea pattern
  BaseWithIndexAndDisplacement32Matcher m(node);
  if (m.matches() &&
      (m.displacement() == NULL || g.CanBeImmediate(m.displacement()))) {
    InstructionOperand inputs[4];
    size_t input_count = 0;
    AddressingMode mode = g.GenerateMemoryOperandInputs(
        m.index(), m.scale(), m.base(), m.displacement(), inputs, &input_count);

    DCHECK_NE(0u, input_count);
    DCHECK_GE(arraysize(inputs), input_count);

    InstructionOperand outputs[1];
    outputs[0] = g.DefineAsRegister(node);

    InstructionCode opcode = AddressingModeField::encode(mode) | kX87Lea;
    Emit(opcode, 1, outputs, input_count, inputs);
    return;
  }

  // No lea pattern match, use add
  VisitBinop(this, node, kX87Add);
}


void InstructionSelector::VisitInt32Sub(Node* node) {
  X87OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().Is(0)) {
    Emit(kX87Neg, g.DefineSameAsFirst(node), g.Use(m.right().node()));
  } else {
    VisitBinop(this, node, kX87Sub);
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
  X87OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (g.CanBeImmediate(right)) {
    Emit(kX87Imul, g.DefineAsRegister(node), g.Use(left),
         g.UseImmediate(right));
  } else {
    if (g.CanBeBetterLeftOperand(right)) {
      std::swap(left, right);
    }
    Emit(kX87Imul, g.DefineSameAsFirst(node), g.UseRegister(left),
         g.Use(right));
  }
}


void InstructionSelector::VisitInt32MulHigh(Node* node) {
  VisitMulHigh(this, node, kX87ImulHigh);
}


void InstructionSelector::VisitUint32MulHigh(Node* node) {
  VisitMulHigh(this, node, kX87UmulHigh);
}


void InstructionSelector::VisitInt32Div(Node* node) {
  VisitDiv(this, node, kX87Idiv);
}


void InstructionSelector::VisitUint32Div(Node* node) {
  VisitDiv(this, node, kX87Udiv);
}


void InstructionSelector::VisitInt32Mod(Node* node) {
  VisitMod(this, node, kX87Idiv);
}


void InstructionSelector::VisitUint32Mod(Node* node) {
  VisitMod(this, node, kX87Udiv);
}


void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87Float32ToFloat64, g.DefineAsFixed(node, stX_0),
       g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87Int32ToFloat64, g.DefineAsFixed(node, stX_0),
       g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87Uint32ToFloat64, g.DefineAsFixed(node, stX_0),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87Float64ToInt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87Float64ToUint32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87Float64ToFloat32, g.DefineAsFixed(node, stX_0),
       g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitTruncateFloat64ToInt32(Node* node) {
  X87OperandGenerator g(this);

  switch (TruncationModeOf(node->op())) {
    case TruncationMode::kJavaScript:
      Emit(kArchTruncateDoubleToI, g.DefineAsRegister(node),
           g.Use(node->InputAt(0)));
      return;
    case TruncationMode::kRoundToZero:
      Emit(kX87Float64ToInt32, g.DefineAsRegister(node),
           g.Use(node->InputAt(0)));
      return;
  }
  UNREACHABLE();
}


void InstructionSelector::VisitFloat32Add(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(1)));
  Emit(kX87Float32Add, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat64Add(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(1)));
  Emit(kX87Float64Add, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat32Sub(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(1)));
  Emit(kX87Float32Sub, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat64Sub(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(1)));
  Emit(kX87Float64Sub, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat32Mul(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(1)));
  Emit(kX87Float32Mul, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat64Mul(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(1)));
  Emit(kX87Float64Mul, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat32Div(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(1)));
  Emit(kX87Float32Div, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat64Div(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(1)));
  Emit(kX87Float64Div, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat64Mod(Node* node) {
  X87OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister(eax)};
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(1)));
  Emit(kX87Float64Mod, g.DefineAsFixed(node, stX_0), 1, temps)->MarkAsCall();
}


void InstructionSelector::VisitFloat32Max(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(1)));
  Emit(kX87Float32Max, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat64Max(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(1)));
  Emit(kX87Float64Max, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat32Min(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(1)));
  Emit(kX87Float32Min, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat64Min(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(1)));
  Emit(kX87Float64Min, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat32Abs(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87Float32Abs, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat64Abs(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87Float64Abs, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat32Sqrt(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87Float32Sqrt, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(0)));
  Emit(kX87Float64Sqrt, g.DefineAsFixed(node, stX_0), 0, NULL);
}


void InstructionSelector::VisitFloat64RoundDown(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87Float64Round | MiscField::encode(kRoundDown),
       g.UseFixed(node, stX_0), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87Float64Round | MiscField::encode(kRoundToZero),
       g.UseFixed(node, stX_0), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  UNREACHABLE();
}


void InstructionSelector::VisitCall(Node* node, BasicBlock* handler) {
  X87OperandGenerator g(this);
  const CallDescriptor* descriptor = OpParameter<const CallDescriptor*>(node);

  FrameStateDescriptor* frame_state_descriptor = nullptr;
  if (descriptor->NeedsFrameState()) {
    frame_state_descriptor =
        GetFrameStateDescriptor(node->InputAt(descriptor->InputCount()));
  }

  CallBuffer buffer(zone(), descriptor, frame_state_descriptor);

  // Compute InstructionOperands for inputs and outputs.
  InitializeCallBuffer(node, &buffer, true, true);

  // Prepare for C function call.
  if (descriptor->IsCFunctionCall()) {
    InstructionOperand temps[] = {g.TempRegister()};
    size_t const temp_count = arraysize(temps);
    Emit(kArchPrepareCallCFunction |
             MiscField::encode(static_cast<int>(descriptor->CParameterCount())),
         0, nullptr, 0, nullptr, temp_count, temps);

    // Poke any stack arguments.
    for (size_t n = 0; n < buffer.pushed_nodes.size(); ++n) {
      if (Node* node = buffer.pushed_nodes[n]) {
        int const slot = static_cast<int>(n);
        InstructionOperand value =
            g.CanBeImmediate(node) ? g.UseImmediate(node) : g.UseRegister(node);
        Emit(kX87Poke | MiscField::encode(slot), g.NoOutput(), value);
      }
    }
  } else {
    // Push any stack arguments.
    for (Node* node : base::Reversed(buffer.pushed_nodes)) {
      // TODO(titzer): handle pushing double parameters.
      InstructionOperand value =
          g.CanBeImmediate(node)
              ? g.UseImmediate(node)
              : IsSupported(ATOM) ? g.UseRegister(node) : g.Use(node);
      Emit(kX87Push, g.NoOutput(), value);
    }
  }

  // Pass label of exception handler block.
  CallDescriptor::Flags flags = descriptor->flags();
  if (handler) {
    DCHECK_EQ(IrOpcode::kIfException, handler->front()->opcode());
    IfExceptionHint hint = OpParameter<IfExceptionHint>(handler->front());
    if (hint == IfExceptionHint::kLocallyCaught) {
      flags |= CallDescriptor::kHasLocalCatchHandler;
    }
    flags |= CallDescriptor::kHasExceptionHandler;
    buffer.instruction_args.push_back(g.Label(handler));
  }

  // Select the appropriate opcode based on the call type.
  InstructionCode opcode;
  switch (descriptor->kind()) {
    case CallDescriptor::kCallAddress:
      opcode =
          kArchCallCFunction |
          MiscField::encode(static_cast<int>(descriptor->CParameterCount()));
      break;
    case CallDescriptor::kCallCodeObject:
      opcode = kArchCallCodeObject | MiscField::encode(flags);
      break;
    case CallDescriptor::kCallJSFunction:
      opcode = kArchCallJSFunction | MiscField::encode(flags);
      break;
    default:
      UNREACHABLE();
      return;
  }

  // Emit the call instruction.
  size_t const output_count = buffer.outputs.size();
  auto* outputs = output_count ? &buffer.outputs.front() : nullptr;
  Emit(opcode, output_count, outputs, buffer.instruction_args.size(),
       &buffer.instruction_args.front())->MarkAsCall();
}


void InstructionSelector::VisitTailCall(Node* node) {
  X87OperandGenerator g(this);
  CallDescriptor const* descriptor = OpParameter<CallDescriptor const*>(node);
  DCHECK_NE(0, descriptor->flags() & CallDescriptor::kSupportsTailCalls);
  DCHECK_EQ(0, descriptor->flags() & CallDescriptor::kPatchableCallSite);
  DCHECK_EQ(0, descriptor->flags() & CallDescriptor::kNeedsNopAfterCall);

  // TODO(turbofan): Relax restriction for stack parameters.

  if (linkage()->GetIncomingDescriptor()->CanTailCall(node)) {
    CallBuffer buffer(zone(), descriptor, nullptr);

    // Compute InstructionOperands for inputs and outputs.
    InitializeCallBuffer(node, &buffer, true, true);

    // Select the appropriate opcode based on the call type.
    InstructionCode opcode;
    switch (descriptor->kind()) {
      case CallDescriptor::kCallCodeObject:
        opcode = kArchTailCallCodeObject;
        break;
      case CallDescriptor::kCallJSFunction:
        opcode = kArchTailCallJSFunction;
        break;
      default:
        UNREACHABLE();
        return;
    }
    opcode |= MiscField::encode(descriptor->flags());

    // Emit the tailcall instruction.
    Emit(opcode, 0, nullptr, buffer.instruction_args.size(),
         &buffer.instruction_args.front());
  } else {
    FrameStateDescriptor* frame_state_descriptor =
        descriptor->NeedsFrameState()
            ? GetFrameStateDescriptor(
                  node->InputAt(static_cast<int>(descriptor->InputCount())))
            : nullptr;

    CallBuffer buffer(zone(), descriptor, frame_state_descriptor);

    // Compute InstructionOperands for inputs and outputs.
    InitializeCallBuffer(node, &buffer, true, true);

    // Push any stack arguments.
    for (Node* node : base::Reversed(buffer.pushed_nodes)) {
      // TODO(titzer): Handle pushing double parameters.
      InstructionOperand value =
          g.CanBeImmediate(node)
              ? g.UseImmediate(node)
              : IsSupported(ATOM) ? g.UseRegister(node) : g.Use(node);
      Emit(kX87Push, g.NoOutput(), value);
    }

    // Select the appropriate opcode based on the call type.
    InstructionCode opcode;
    switch (descriptor->kind()) {
      case CallDescriptor::kCallCodeObject:
        opcode = kArchCallCodeObject;
        break;
      case CallDescriptor::kCallJSFunction:
        opcode = kArchCallJSFunction;
        break;
      default:
        UNREACHABLE();
        return;
    }
    opcode |= MiscField::encode(descriptor->flags());

    // Emit the call instruction.
    size_t output_count = buffer.outputs.size();
    auto* outputs = &buffer.outputs.front();
    Emit(opcode, output_count, outputs, buffer.instruction_args.size(),
         &buffer.instruction_args.front())->MarkAsCall();
    Emit(kArchRet, 0, nullptr, output_count, outputs);
  }
}


namespace {

// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  InstructionOperand left, InstructionOperand right,
                  FlagsContinuation* cont) {
  X87OperandGenerator g(selector);
  if (cont->IsBranch()) {
    selector->Emit(cont->Encode(opcode), g.NoOutput(), left, right,
                   g.Label(cont->true_block()), g.Label(cont->false_block()));
  } else {
    DCHECK(cont->IsSet());
    selector->Emit(cont->Encode(opcode), g.DefineAsByteRegister(cont->result()),
                   left, right);
  }
}


// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  Node* left, Node* right, FlagsContinuation* cont,
                  bool commutative) {
  X87OperandGenerator g(selector);
  if (commutative && g.CanBeBetterLeftOperand(right)) {
    std::swap(left, right);
  }
  VisitCompare(selector, opcode, g.UseRegister(left), g.Use(right), cont);
}


// Shared routine for multiple float32 compare operations (inputs commuted).
void VisitFloat32Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  X87OperandGenerator g(selector);
  selector->Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(0)));
  selector->Emit(kX87PushFloat32, g.NoOutput(), g.Use(node->InputAt(1)));
  if (cont->IsBranch()) {
    selector->Emit(cont->Encode(kX87Float32Cmp), g.NoOutput(),
                   g.Label(cont->true_block()), g.Label(cont->false_block()));
  } else {
    DCHECK(cont->IsSet());
    selector->Emit(cont->Encode(kX87Float32Cmp),
                   g.DefineAsByteRegister(cont->result()));
  }
}


// Shared routine for multiple float64 compare operations (inputs commuted).
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  X87OperandGenerator g(selector);
  selector->Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(0)));
  selector->Emit(kX87PushFloat64, g.NoOutput(), g.Use(node->InputAt(1)));
  if (cont->IsBranch()) {
    selector->Emit(cont->Encode(kX87Float64Cmp), g.NoOutput(),
                   g.Label(cont->true_block()), g.Label(cont->false_block()));
  } else {
    DCHECK(cont->IsSet());
    selector->Emit(cont->Encode(kX87Float64Cmp),
                   g.DefineAsByteRegister(cont->result()));
  }
}


// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont) {
  X87OperandGenerator g(selector);
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
  X87OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  if (m.left().IsLoad() && m.right().IsLoadStackPointer()) {
    LoadMatcher<ExternalReferenceMatcher> mleft(m.left().node());
    ExternalReference js_stack_limit =
        ExternalReference::address_of_stack_limit(selector->isolate());
    if (mleft.object().Is(js_stack_limit) && mleft.index().Is(0)) {
      // Compare(Load(js_stack_limit), LoadStackPointer)
      if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
      InstructionCode opcode = cont->Encode(kX87StackCheck);
      if (cont->IsBranch()) {
        selector->Emit(opcode, g.NoOutput(), g.Label(cont->true_block()),
                       g.Label(cont->false_block()));
      } else {
        DCHECK(cont->IsSet());
        selector->Emit(opcode, g.DefineAsRegister(cont->result()));
      }
      return;
    }
  }
  VisitWordCompare(selector, node, kX87Cmp, cont);
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
      case IrOpcode::kFloat32Equal:
        cont->OverwriteAndNegateIfEqual(kUnorderedEqual);
        return VisitFloat32Compare(selector, value, cont);
      case IrOpcode::kFloat32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThan);
        return VisitFloat32Compare(selector, value, cont);
      case IrOpcode::kFloat32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThanOrEqual);
        return VisitFloat32Compare(selector, value, cont);
      case IrOpcode::kFloat64Equal:
        cont->OverwriteAndNegateIfEqual(kUnorderedEqual);
        return VisitFloat64Compare(selector, value, cont);
      case IrOpcode::kFloat64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThan);
        return VisitFloat64Compare(selector, value, cont);
      case IrOpcode::kFloat64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThanOrEqual);
        return VisitFloat64Compare(selector, value, cont);
      case IrOpcode::kProjection:
        // Check if this is the overflow output projection of an
        // <Operation>WithOverflow node.
        if (ProjectionIndexOf(value->op()) == 1u) {
          // We cannot combine the <Operation>WithOverflow with this branch
          // unless the 0th projection (the use of the actual value of the
          // <Operation> is either NULL, which means there's no use of the
          // actual value, or was already defined, which means it is scheduled
          // *AFTER* this branch).
          Node* const node = value->InputAt(0);
          Node* const result = NodeProperties::FindProjection(node, 0);
          if (result == NULL || selector->IsDefined(result)) {
            switch (node->opcode()) {
              case IrOpcode::kInt32AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kX87Add, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kX87Sub, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Sub:
        return VisitWordCompare(selector, value, cont);
      case IrOpcode::kWord32And:
        return VisitWordCompare(selector, value, kX87Test, cont);
      default:
        break;
    }
    break;
  }

  // Continuation could not be combined with a compare, emit compare against 0.
  X87OperandGenerator g(selector);
  VisitCompare(selector, kX87Cmp, g.Use(value), g.TempImmediate(0), cont);
}

}  // namespace


void InstructionSelector::VisitBranch(Node* branch, BasicBlock* tbranch,
                                      BasicBlock* fbranch) {
  FlagsContinuation cont(kNotEqual, tbranch, fbranch);
  VisitWordCompareZero(this, branch, branch->InputAt(0), &cont);
}


void InstructionSelector::VisitSwitch(Node* node, const SwitchInfo& sw) {
  X87OperandGenerator g(this);
  InstructionOperand value_operand = g.UseRegister(node->InputAt(0));

  // Emit either ArchTableSwitch or ArchLookupSwitch.
  size_t table_space_cost = 4 + sw.value_range;
  size_t table_time_cost = 3;
  size_t lookup_space_cost = 3 + 2 * sw.case_count;
  size_t lookup_time_cost = sw.case_count;
  if (sw.case_count > 4 &&
      table_space_cost + 3 * table_time_cost <=
          lookup_space_cost + 3 * lookup_time_cost &&
      sw.min_value > std::numeric_limits<int32_t>::min()) {
    InstructionOperand index_operand = value_operand;
    if (sw.min_value) {
      index_operand = g.TempRegister();
      Emit(kX87Lea | AddressingModeField::encode(kMode_MRI), index_operand,
           value_operand, g.TempImmediate(-sw.min_value));
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
    return VisitBinop(this, node, kX87Add, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kX87Add, &cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitBinop(this, node, kX87Sub, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kX87Sub, &cont);
}


void InstructionSelector::VisitFloat32Equal(Node* node) {
  FlagsContinuation cont(kUnorderedEqual, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat32LessThan(Node* node) {
  FlagsContinuation cont(kUnsignedGreaterThan, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat32LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kUnsignedGreaterThanOrEqual, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64Equal(Node* node) {
  FlagsContinuation cont(kUnorderedEqual, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64LessThan(Node* node) {
  FlagsContinuation cont(kUnsignedGreaterThan, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kUnsignedGreaterThanOrEqual, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64ExtractLowWord32(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87Float64ExtractLowWord32, g.DefineAsRegister(node),
       g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64ExtractHighWord32(Node* node) {
  X87OperandGenerator g(this);
  Emit(kX87Float64ExtractHighWord32, g.DefineAsRegister(node),
       g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64InsertLowWord32(Node* node) {
  X87OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Emit(kX87Float64InsertLowWord32, g.UseFixed(node, stX_0), g.UseRegister(left),
       g.UseRegister(right));
}


void InstructionSelector::VisitFloat64InsertHighWord32(Node* node) {
  X87OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Emit(kX87Float64InsertHighWord32, g.UseFixed(node, stX_0),
       g.UseRegister(left), g.UseRegister(right));
}


// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags =
      MachineOperatorBuilder::kFloat32Max |
      MachineOperatorBuilder::kFloat32Min |
      MachineOperatorBuilder::kFloat64Max |
      MachineOperatorBuilder::kFloat64Min |
      MachineOperatorBuilder::kWord32ShiftIsSafe;
  return flags;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
