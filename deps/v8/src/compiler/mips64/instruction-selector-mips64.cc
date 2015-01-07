// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE_UNIMPL() \
  PrintF("UNIMPLEMENTED instr_sel: %s at line %d\n", __FUNCTION__, __LINE__)

#define TRACE() PrintF("instr_sel: %s at line %d\n", __FUNCTION__, __LINE__)


// Adds Mips-specific methods for generating InstructionOperands.
class Mips64OperandGenerator FINAL : public OperandGenerator {
 public:
  explicit Mips64OperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand* UseOperand(Node* node, InstructionCode opcode) {
    if (CanBeImmediate(node, opcode)) {
      return UseImmediate(node);
    }
    return UseRegister(node);
  }

  bool CanBeImmediate(Node* node, InstructionCode opcode) {
    int64_t value;
    if (node->opcode() == IrOpcode::kInt32Constant)
      value = OpParameter<int32_t>(node);
    else if (node->opcode() == IrOpcode::kInt64Constant)
      value = OpParameter<int64_t>(node);
    else
      return false;
    switch (ArchOpcodeField::decode(opcode)) {
      case kMips64Shl:
      case kMips64Sar:
      case kMips64Shr:
        return is_uint5(value);
      case kMips64Dshl:
      case kMips64Dsar:
      case kMips64Dshr:
        return is_uint6(value);
      case kMips64Xor:
        return is_uint16(value);
      case kMips64Ldc1:
      case kMips64Sdc1:
        return is_int16(value + kIntSize);
      default:
        return is_int16(value);
    }
  }


  bool CanBeImmediate(Node* node, InstructionCode opcode,
                      FlagsContinuation* cont) {
    int64_t value;
    if (node->opcode() == IrOpcode::kInt32Constant)
      value = OpParameter<int32_t>(node);
    else if (node->opcode() == IrOpcode::kInt64Constant)
      value = OpParameter<int64_t>(node);
    else
      return false;
    switch (ArchOpcodeField::decode(opcode)) {
      case kMips64Cmp32:
        switch (cont->condition()) {
          case kUnsignedLessThan:
          case kUnsignedGreaterThanOrEqual:
          case kUnsignedLessThanOrEqual:
          case kUnsignedGreaterThan:
            // Immediate operands for unsigned 32-bit compare operations
            // should not be sign-extended.
            return is_uint15(value);
          default:
            return false;
        }
      default:
        return is_int16(value);
    }
  }


 private:
  bool ImmediateFitsAddrMode1Instruction(int32_t imm) const {
    TRACE_UNIMPL();
    return false;
  }
};


static void VisitRR(InstructionSelector* selector, ArchOpcode opcode,
                    Node* node) {
  Mips64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}


static void VisitRRR(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node) {
  Mips64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
}


static void VisitRRO(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node) {
  Mips64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseOperand(node->InputAt(1), opcode));
}


static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, FlagsContinuation* cont) {
  Mips64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand* inputs[4];
  size_t input_count = 0;
  InstructionOperand* outputs[2];
  size_t output_count = 0;

  inputs[input_count++] = g.UseRegister(m.left().node());
  inputs[input_count++] = g.UseOperand(m.right().node(), opcode);

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

  Instruction* instr = selector->Emit(cont->Encode(opcode), output_count,
                                      outputs, input_count, inputs);
  if (cont->IsBranch()) instr->MarkAsControl();
}


static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode) {
  FlagsContinuation cont;
  VisitBinop(selector, node, opcode, &cont);
}


void InstructionSelector::VisitLoad(Node* node) {
  MachineType rep = RepresentationOf(OpParameter<LoadRepresentation>(node));
  MachineType typ = TypeOf(OpParameter<LoadRepresentation>(node));
  Mips64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  ArchOpcode opcode;
  switch (rep) {
    case kRepFloat32:
      opcode = kMips64Lwc1;
      break;
    case kRepFloat64:
      opcode = kMips64Ldc1;
      break;
    case kRepBit:  // Fall through.
    case kRepWord8:
      opcode = typ == kTypeUint32 ? kMips64Lbu : kMips64Lb;
      break;
    case kRepWord16:
      opcode = typ == kTypeUint32 ? kMips64Lhu : kMips64Lh;
      break;
    case kRepWord32:
      opcode = kMips64Lw;
      break;
    case kRepTagged:  // Fall through.
    case kRepWord64:
      opcode = kMips64Ld;
      break;
    default:
      UNREACHABLE();
      return;
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand* addr_reg = g.TempRegister();
    Emit(kMips64Dadd | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}


void InstructionSelector::VisitStore(Node* node) {
  Mips64OperandGenerator g(this);
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
    InstructionOperand* temps[] = {g.TempRegister(t1), g.TempRegister(t2)};
    Emit(kMips64StoreWriteBarrier, NULL, g.UseFixed(base, t0),
         g.UseFixed(index, t1), g.UseFixed(value, t2), arraysize(temps), temps);
    return;
  }
  DCHECK_EQ(kNoWriteBarrier, store_rep.write_barrier_kind());

  ArchOpcode opcode;
  switch (rep) {
    case kRepFloat32:
      opcode = kMips64Swc1;
      break;
    case kRepFloat64:
      opcode = kMips64Sdc1;
      break;
    case kRepBit:  // Fall through.
    case kRepWord8:
      opcode = kMips64Sb;
      break;
    case kRepWord16:
      opcode = kMips64Sh;
      break;
    case kRepWord32:
      opcode = kMips64Sw;
      break;
    case kRepTagged:  // Fall through.
    case kRepWord64:
      opcode = kMips64Sd;
      break;
    default:
      UNREACHABLE();
      return;
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), NULL,
         g.UseRegister(base), g.UseImmediate(index), g.UseRegister(value));
  } else {
    InstructionOperand* addr_reg = g.TempRegister();
    Emit(kMips64Dadd | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired store opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI), NULL, addr_reg,
         g.TempImmediate(0), g.UseRegister(value));
  }
}


void InstructionSelector::VisitWord32And(Node* node) {
  VisitBinop(this, node, kMips64And);
}


void InstructionSelector::VisitWord64And(Node* node) {
  VisitBinop(this, node, kMips64And);
}


void InstructionSelector::VisitWord32Or(Node* node) {
  VisitBinop(this, node, kMips64Or);
}


void InstructionSelector::VisitWord64Or(Node* node) {
  VisitBinop(this, node, kMips64Or);
}


void InstructionSelector::VisitWord32Xor(Node* node) {
  VisitBinop(this, node, kMips64Xor);
}


void InstructionSelector::VisitWord64Xor(Node* node) {
  VisitBinop(this, node, kMips64Xor);
}


void InstructionSelector::VisitWord32Shl(Node* node) {
  VisitRRO(this, kMips64Shl, node);
}


void InstructionSelector::VisitWord32Shr(Node* node) {
  VisitRRO(this, kMips64Shr, node);
}


void InstructionSelector::VisitWord32Sar(Node* node) {
  VisitRRO(this, kMips64Sar, node);
}


void InstructionSelector::VisitWord64Shl(Node* node) {
  VisitRRO(this, kMips64Dshl, node);
}


void InstructionSelector::VisitWord64Shr(Node* node) {
  VisitRRO(this, kMips64Dshr, node);
}


void InstructionSelector::VisitWord64Sar(Node* node) {
  VisitRRO(this, kMips64Dsar, node);
}


void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitRRO(this, kMips64Ror, node);
}


void InstructionSelector::VisitWord64Ror(Node* node) {
  VisitRRO(this, kMips64Dror, node);
}


void InstructionSelector::VisitInt32Add(Node* node) {
  Mips64OperandGenerator g(this);
  // TODO(plind): Consider multiply & add optimization from arm port.
  VisitBinop(this, node, kMips64Add);
}


void InstructionSelector::VisitInt64Add(Node* node) {
  Mips64OperandGenerator g(this);
  // TODO(plind): Consider multiply & add optimization from arm port.
  VisitBinop(this, node, kMips64Dadd);
}


void InstructionSelector::VisitInt32Sub(Node* node) {
  VisitBinop(this, node, kMips64Sub);
}


void InstructionSelector::VisitInt64Sub(Node* node) {
  VisitBinop(this, node, kMips64Dsub);
}


void InstructionSelector::VisitInt32Mul(Node* node) {
  Mips64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.right().HasValue() && m.right().Value() > 0) {
    int32_t value = m.right().Value();
    if (base::bits::IsPowerOfTwo32(value)) {
      Emit(kMips64Shl | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value)));
      return;
    }
    if (base::bits::IsPowerOfTwo32(value - 1)) {
      InstructionOperand* temp = g.TempRegister();
      Emit(kMips64Shl | AddressingModeField::encode(kMode_None), temp,
           g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value - 1)));
      Emit(kMips64Add | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()), temp);
      return;
    }
    if (base::bits::IsPowerOfTwo32(value + 1)) {
      InstructionOperand* temp = g.TempRegister();
      Emit(kMips64Shl | AddressingModeField::encode(kMode_None), temp,
           g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value + 1)));
      Emit(kMips64Sub | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), temp, g.UseRegister(m.left().node()));
      return;
    }
  }
  Emit(kMips64Mul, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitInt32MulHigh(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64MulHigh, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}


void InstructionSelector::VisitUint32MulHigh(Node* node) {
  Mips64OperandGenerator g(this);
  InstructionOperand* const dmul_operand = g.TempRegister();
  Emit(kMips64MulHighU, dmul_operand, g.UseRegister(node->InputAt(0)),
       g.UseRegister(node->InputAt(1)));
  Emit(kMips64Ext, g.DefineAsRegister(node), dmul_operand, g.TempImmediate(0),
       g.TempImmediate(32));
}


void InstructionSelector::VisitInt64Mul(Node* node) {
  Mips64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  // TODO(dusmil): Add optimization for shifts larger than 32.
  if (m.right().HasValue() && m.right().Value() > 0) {
    int64_t value = m.right().Value();
    if (base::bits::IsPowerOfTwo32(value)) {
      Emit(kMips64Dshl | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value)));
      return;
    }
    if (base::bits::IsPowerOfTwo32(value - 1)) {
      InstructionOperand* temp = g.TempRegister();
      Emit(kMips64Dshl | AddressingModeField::encode(kMode_None), temp,
           g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value - 1)));
      Emit(kMips64Dadd | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()), temp);
      return;
    }
    if (base::bits::IsPowerOfTwo32(value + 1)) {
      InstructionOperand* temp = g.TempRegister();
      Emit(kMips64Dshl | AddressingModeField::encode(kMode_None), temp,
           g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value + 1)));
      Emit(kMips64Dsub | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), temp, g.UseRegister(m.left().node()));
      return;
    }
  }
  Emit(kMips64Dmul, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitInt32Div(Node* node) {
  Mips64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  Emit(kMips64Div, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitUint32Div(Node* node) {
  Mips64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  Emit(kMips64DivU, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitInt32Mod(Node* node) {
  Mips64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  Emit(kMips64Mod, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitUint32Mod(Node* node) {
  Mips64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  Emit(kMips64ModU, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitInt64Div(Node* node) {
  Mips64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  Emit(kMips64Ddiv, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitUint64Div(Node* node) {
  Mips64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  Emit(kMips64DdivU, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitInt64Mod(Node* node) {
  Mips64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  Emit(kMips64Dmod, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitUint64Mod(Node* node) {
  Mips64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  Emit(kMips64DmodU, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64CvtDS, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64CvtDW, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64CvtDUw, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64TruncWD, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64TruncUwD, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeInt32ToInt64(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64Shl, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)),
       g.TempImmediate(0));
}


void InstructionSelector::VisitChangeUint32ToUint64(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64Dext, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)),
       g.TempImmediate(0), g.TempImmediate(32));
}


void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64Ext, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)),
       g.TempImmediate(0), g.TempImmediate(32));
}


void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64CvtSD, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64Add(Node* node) {
  VisitRRR(this, kMips64AddD, node);
}


void InstructionSelector::VisitFloat64Sub(Node* node) {
  VisitRRR(this, kMips64SubD, node);
}


void InstructionSelector::VisitFloat64Mul(Node* node) {
  VisitRRR(this, kMips64MulD, node);
}


void InstructionSelector::VisitFloat64Div(Node* node) {
  VisitRRR(this, kMips64DivD, node);
}


void InstructionSelector::VisitFloat64Mod(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64ModD, g.DefineAsFixed(node, f0),
       g.UseFixed(node->InputAt(0), f12),
       g.UseFixed(node->InputAt(1), f14))->MarkAsCall();
}


void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64SqrtD, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64Floor(Node* node) {
  VisitRR(this, kMips64Float64Floor, node);
}


void InstructionSelector::VisitFloat64Ceil(Node* node) {
  VisitRR(this, kMips64Float64Ceil, node);
}


void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  VisitRR(this, kMips64Float64RoundTruncate, node);
}


void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  UNREACHABLE();
}


void InstructionSelector::VisitCall(Node* node) {
  Mips64OperandGenerator g(this);
  const CallDescriptor* descriptor = OpParameter<const CallDescriptor*>(node);

  FrameStateDescriptor* frame_state_descriptor = NULL;
  if (descriptor->NeedsFrameState()) {
    frame_state_descriptor =
        GetFrameStateDescriptor(node->InputAt(descriptor->InputCount()));
  }

  CallBuffer buffer(zone(), descriptor, frame_state_descriptor);

  // Compute InstructionOperands for inputs and outputs.
  InitializeCallBuffer(node, &buffer, true, false);

  int push_count = buffer.pushed_nodes.size();
  if (push_count > 0) {
    Emit(kMips64StackClaim | MiscField::encode(push_count), NULL);
  }
  int slot = buffer.pushed_nodes.size() - 1;
  for (NodeVectorRIter input = buffer.pushed_nodes.rbegin();
       input != buffer.pushed_nodes.rend(); input++) {
    Emit(kMips64StoreToStackSlot | MiscField::encode(slot), NULL,
         g.UseRegister(*input));
    slot--;
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
  Instruction* call_instr =
      Emit(opcode, buffer.outputs.size(), &buffer.outputs.front(),
           buffer.instruction_args.size(), &buffer.instruction_args.front());

  call_instr->MarkAsCall();
}


void InstructionSelector::VisitCheckedLoad(Node* node) {
  MachineType rep = RepresentationOf(OpParameter<MachineType>(node));
  MachineType typ = TypeOf(OpParameter<MachineType>(node));
  Mips64OperandGenerator g(this);
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
  InstructionOperand* offset_operand = g.CanBeImmediate(offset, opcode)
                                           ? g.UseImmediate(offset)
                                           : g.UseRegister(offset);

  InstructionOperand* length_operand =
      (!g.CanBeImmediate(offset, opcode)) ? g.CanBeImmediate(length, opcode)
      ? g.UseImmediate(length)
      : g.UseRegister(length)
      : g.UseRegister(length);

  Emit(opcode | AddressingModeField::encode(kMode_MRI),
       g.DefineAsRegister(node), offset_operand, length_operand,
       g.UseRegister(buffer));
}


void InstructionSelector::VisitCheckedStore(Node* node) {
  MachineType rep = RepresentationOf(OpParameter<MachineType>(node));
  Mips64OperandGenerator g(this);
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
  InstructionOperand* offset_operand = g.CanBeImmediate(offset, opcode)
                                           ? g.UseImmediate(offset)
                                           : g.UseRegister(offset);

  InstructionOperand* length_operand =
      (!g.CanBeImmediate(offset, opcode)) ? g.CanBeImmediate(length, opcode)
      ? g.UseImmediate(length)
      : g.UseRegister(length)
      : g.UseRegister(length);

  Emit(opcode | AddressingModeField::encode(kMode_MRI), nullptr, offset_operand,
       length_operand, g.UseRegister(value), g.UseRegister(buffer));
}


namespace {

// Shared routine for multiple compare operations.
static void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                         InstructionOperand* left, InstructionOperand* right,
                         FlagsContinuation* cont) {
  Mips64OperandGenerator g(selector);
  opcode = cont->Encode(opcode);
  if (cont->IsBranch()) {
    selector->Emit(opcode, NULL, left, right, g.Label(cont->true_block()),
                   g.Label(cont->false_block()))->MarkAsControl();
  } else {
    DCHECK(cont->IsSet());
    selector->Emit(opcode, g.DefineAsRegister(cont->result()), left, right);
  }
}


// Shared routine for multiple float compare operations.
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  Mips64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  VisitCompare(selector, kMips64CmpD, g.UseRegister(left), g.UseRegister(right),
               cont);
}


// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont,
                      bool commutative) {
  Mips64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  // Match immediates on left or right side of comparison.
  if (g.CanBeImmediate(right, opcode, cont)) {
    VisitCompare(selector, opcode, g.UseRegister(left), g.UseImmediate(right),
                 cont);
  } else if (g.CanBeImmediate(left, opcode, cont)) {
    if (!commutative) cont->Commute();
    VisitCompare(selector, opcode, g.UseRegister(right), g.UseImmediate(left),
                 cont);
  } else {
    VisitCompare(selector, opcode, g.UseRegister(left), g.UseRegister(right),
                 cont);
  }
}


void VisitWord32Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  VisitWordCompare(selector, node, kMips64Cmp32, cont, false);
}


void VisitWord64Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  VisitWordCompare(selector, node, kMips64Cmp, cont, false);
}

}  // namespace


void EmitWordCompareZero(InstructionSelector* selector, InstructionCode opcode,
                         Node* value, FlagsContinuation* cont) {
  Mips64OperandGenerator g(selector);
  opcode = cont->Encode(opcode);
  InstructionOperand* const value_operand = g.UseRegister(value);
  if (cont->IsBranch()) {
    selector->Emit(opcode, nullptr, value_operand, g.TempImmediate(0),
                   g.Label(cont->true_block()),
                   g.Label(cont->false_block()))->MarkAsControl();
  } else {
    selector->Emit(opcode, g.DefineAsRegister(cont->result()), value_operand,
                   g.TempImmediate(0));
  }
}


// Shared routine for word comparisons against zero.
void VisitWordCompareZero(InstructionSelector* selector, Node* user,
                          Node* value, FlagsContinuation* cont) {
  // Initially set comparison against 0 to be 64-bit variant for branches that
  // cannot combine.
  InstructionCode opcode = kMips64Cmp;
  while (selector->CanCover(user, value)) {
    if (user->opcode() == IrOpcode::kWord32Equal) {
      opcode = kMips64Cmp32;
    }
    switch (value->opcode()) {
      case IrOpcode::kWord32Equal: {
        // Combine with comparisons against 0 by simply inverting the
        // continuation.
        Int32BinopMatcher m(value);
        if (m.right().Is(0)) {
          user = value;
          value = m.left().node();
          cont->Negate();
          opcode = kMips64Cmp32;
          continue;
        }
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitWord32Compare(selector, value, cont);
      }
      case IrOpcode::kInt32LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWord32Compare(selector, value, cont);
      case IrOpcode::kInt32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWord32Compare(selector, value, cont);
      case IrOpcode::kUint32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWord32Compare(selector, value, cont);
      case IrOpcode::kUint32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWord32Compare(selector, value, cont);
      case IrOpcode::kWord64Equal: {
        // Combine with comparisons against 0 by simply inverting the
        // continuation.
        Int64BinopMatcher m(value);
        if (m.right().Is(0)) {
          user = value;
          value = m.left().node();
          cont->Negate();
          continue;
        }
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitWord64Compare(selector, value, cont);
      }
      case IrOpcode::kInt64LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWord64Compare(selector, value, cont);
      case IrOpcode::kInt64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWord64Compare(selector, value, cont);
      case IrOpcode::kUint64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWord64Compare(selector, value, cont);
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
                return VisitBinop(selector, node, kMips64Dadd, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kMips64Dsub, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kWord32And:
        return VisitWordCompare(selector, value, kMips64Tst32, cont, true);
      case IrOpcode::kWord64And:
        return VisitWordCompare(selector, value, kMips64Tst, cont, true);
      default:
        break;
    }
    break;
  }

  // Continuation could not be combined with a compare, emit compare against 0.
  EmitWordCompareZero(selector, opcode, value, cont);
}


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

  VisitWord32Compare(this, node, &cont);
}


void InstructionSelector::VisitInt32LessThan(Node* node) {
  FlagsContinuation cont(kSignedLessThan, node);
  VisitWord32Compare(this, node, &cont);
}


void InstructionSelector::VisitInt32LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kSignedLessThanOrEqual, node);
  VisitWord32Compare(this, node, &cont);
}


void InstructionSelector::VisitUint32LessThan(Node* node) {
  FlagsContinuation cont(kUnsignedLessThan, node);
  VisitWord32Compare(this, node, &cont);
}


void InstructionSelector::VisitUint32LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kUnsignedLessThanOrEqual, node);
  VisitWord32Compare(this, node, &cont);
}


void InstructionSelector::VisitInt32AddWithOverflow(Node* node) {
  if (Node* ovf = node->FindProjection(1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitBinop(this, node, kMips64Dadd, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kMips64Dadd, &cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = node->FindProjection(1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitBinop(this, node, kMips64Dsub, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kMips64Dsub, &cont);
}


void InstructionSelector::VisitWord64Equal(Node* const node) {
  FlagsContinuation cont(kEqual, node);
  Int64BinopMatcher m(node);
  if (m.right().Is(0)) {
    return VisitWordCompareZero(this, m.node(), m.left().node(), &cont);
  }

  VisitWord64Compare(this, node, &cont);
}


void InstructionSelector::VisitInt64LessThan(Node* node) {
  FlagsContinuation cont(kSignedLessThan, node);
  VisitWord64Compare(this, node, &cont);
}


void InstructionSelector::VisitInt64LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kSignedLessThanOrEqual, node);
  VisitWord64Compare(this, node, &cont);
}


void InstructionSelector::VisitUint64LessThan(Node* node) {
  FlagsContinuation cont(kUnsignedLessThan, node);
  VisitWord64Compare(this, node, &cont);
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
  return MachineOperatorBuilder::kFloat64Floor |
         MachineOperatorBuilder::kFloat64Ceil |
         MachineOperatorBuilder::kFloat64RoundTruncate;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
