// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/generic-node-inl.h"
#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Adds X64-specific methods for generating operands.
class X64OperandGenerator FINAL : public OperandGenerator {
 public:
  explicit X64OperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand* TempRegister(Register reg) {
    return new (zone()) UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                                           Register::ToAllocationIndex(reg));
  }

  bool CanBeImmediate(Node* node) {
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
        return true;
      case IrOpcode::kInt64Constant: {
        const int64_t value = OpParameter<int64_t>(node);
        return value == static_cast<int64_t>(static_cast<int32_t>(value));
      }
      default:
        return false;
    }
  }

  bool CanBeBetterLeftOperand(Node* node) const {
    return !selector()->IsLive(node);
  }
};


void InstructionSelector::VisitLoad(Node* node) {
  MachineType rep = RepresentationOf(OpParameter<LoadRepresentation>(node));
  MachineType typ = TypeOf(OpParameter<LoadRepresentation>(node));
  X64OperandGenerator g(this);
  Node* const base = node->InputAt(0);
  Node* const index = node->InputAt(1);

  ArchOpcode opcode;
  switch (rep) {
    case kRepFloat32:
      opcode = kX64Movss;
      break;
    case kRepFloat64:
      opcode = kX64Movsd;
      break;
    case kRepBit:  // Fall through.
    case kRepWord8:
      opcode = typ == kTypeInt32 ? kX64Movsxbl : kX64Movzxbl;
      break;
    case kRepWord16:
      opcode = typ == kTypeInt32 ? kX64Movsxwl : kX64Movzxwl;
      break;
    case kRepWord32:
      opcode = kX64Movl;
      break;
    case kRepTagged:  // Fall through.
    case kRepWord64:
      opcode = kX64Movq;
      break;
    default:
      UNREACHABLE();
      return;
  }
  if (g.CanBeImmediate(base)) {
    // load [#base + %index]
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(index), g.UseImmediate(base));
  } else if (g.CanBeImmediate(index)) {
    // load [%base + #index]
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseImmediate(index));
  } else {
    // load [%base + %index*1]
    Emit(opcode | AddressingModeField::encode(kMode_MR1),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseRegister(index));
  }
}


void InstructionSelector::VisitStore(Node* node) {
  X64OperandGenerator g(this);
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
    InstructionOperand* temps[] = {g.TempRegister(rcx), g.TempRegister(rdx)};
    Emit(kX64StoreWriteBarrier, NULL, g.UseFixed(base, rbx),
         g.UseFixed(index, rcx), g.UseFixed(value, rdx), arraysize(temps),
         temps);
    return;
  }
  DCHECK_EQ(kNoWriteBarrier, store_rep.write_barrier_kind());
  ArchOpcode opcode;
  switch (rep) {
    case kRepFloat32:
      opcode = kX64Movss;
      break;
    case kRepFloat64:
      opcode = kX64Movsd;
      break;
    case kRepBit:  // Fall through.
    case kRepWord8:
      opcode = kX64Movb;
      break;
    case kRepWord16:
      opcode = kX64Movw;
      break;
    case kRepWord32:
      opcode = kX64Movl;
      break;
    case kRepTagged:  // Fall through.
    case kRepWord64:
      opcode = kX64Movq;
      break;
    default:
      UNREACHABLE();
      return;
  }
  InstructionOperand* value_operand =
      g.CanBeImmediate(value) ? g.UseImmediate(value) : g.UseRegister(value);
  if (g.CanBeImmediate(base)) {
    // store [#base + %index], %|#value
    Emit(opcode | AddressingModeField::encode(kMode_MRI), nullptr,
         g.UseRegister(index), g.UseImmediate(base), value_operand);
  } else if (g.CanBeImmediate(index)) {
    // store [%base + #index], %|#value
    Emit(opcode | AddressingModeField::encode(kMode_MRI), nullptr,
         g.UseRegister(base), g.UseImmediate(index), value_operand);
  } else {
    // store [%base + %index*1], %|#value
    Emit(opcode | AddressingModeField::encode(kMode_MR1), nullptr,
         g.UseRegister(base), g.UseRegister(index), value_operand);
  }
}


// Shared routine for multiple binary operations.
static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
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
    //   mov rax, [rbp-0x10]
    //   add rax, [rbp-0x10]
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
    outputs[output_count++] = g.DefineAsRegister(cont->result());
  }

  DCHECK_NE(0, static_cast<int>(input_count));
  DCHECK_NE(0, static_cast<int>(output_count));
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
  VisitBinop(this, node, kX64And32);
}


void InstructionSelector::VisitWord64And(Node* node) {
  VisitBinop(this, node, kX64And);
}


void InstructionSelector::VisitWord32Or(Node* node) {
  VisitBinop(this, node, kX64Or32);
}


void InstructionSelector::VisitWord64Or(Node* node) {
  VisitBinop(this, node, kX64Or);
}


void InstructionSelector::VisitWord32Xor(Node* node) {
  X64OperandGenerator g(this);
  Uint32BinopMatcher m(node);
  if (m.right().Is(-1)) {
    Emit(kX64Not32, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()));
  } else {
    VisitBinop(this, node, kX64Xor32);
  }
}


void InstructionSelector::VisitWord64Xor(Node* node) {
  X64OperandGenerator g(this);
  Uint64BinopMatcher m(node);
  if (m.right().Is(-1)) {
    Emit(kX64Not, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()));
  } else {
    VisitBinop(this, node, kX64Xor);
  }
}


namespace {

// Shared routine for multiple 32-bit shift operations.
// TODO(bmeurer): Merge this with VisitWord64Shift using template magic?
void VisitWord32Shift(InstructionSelector* selector, Node* node,
                      ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();

  if (g.CanBeImmediate(right)) {
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.UseImmediate(right));
  } else {
    if (m.right().IsWord32And()) {
      Int32BinopMatcher mright(right);
      if (mright.right().Is(0x1F)) {
        right = mright.left().node();
      }
    }
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.UseFixed(right, rcx));
  }
}


// Shared routine for multiple 64-bit shift operations.
// TODO(bmeurer): Merge this with VisitWord32Shift using template magic?
void VisitWord64Shift(InstructionSelector* selector, Node* node,
                      ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Int64BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();

  if (g.CanBeImmediate(right)) {
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.UseImmediate(right));
  } else {
    if (m.right().IsWord64And()) {
      Int64BinopMatcher mright(right);
      if (mright.right().Is(0x3F)) {
        right = mright.left().node();
      }
    }
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.UseFixed(right, rcx));
  }
}

}  // namespace


void InstructionSelector::VisitWord32Shl(Node* node) {
  VisitWord32Shift(this, node, kX64Shl32);
}


void InstructionSelector::VisitWord64Shl(Node* node) {
  X64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  if ((m.left().IsChangeInt32ToInt64() || m.left().IsChangeUint32ToUint64()) &&
      m.right().IsInRange(32, 63)) {
    // There's no need to sign/zero-extend to 64-bit if we shift out the upper
    // 32 bits anyway.
    Emit(kX64Shl, g.DefineSameAsFirst(node),
         g.UseRegister(m.left().node()->InputAt(0)),
         g.UseImmediate(m.right().node()));
    return;
  }
  VisitWord64Shift(this, node, kX64Shl);
}


void InstructionSelector::VisitWord32Shr(Node* node) {
  VisitWord32Shift(this, node, kX64Shr32);
}


void InstructionSelector::VisitWord64Shr(Node* node) {
  VisitWord64Shift(this, node, kX64Shr);
}


void InstructionSelector::VisitWord32Sar(Node* node) {
  VisitWord32Shift(this, node, kX64Sar32);
}


void InstructionSelector::VisitWord64Sar(Node* node) {
  VisitWord64Shift(this, node, kX64Sar);
}


void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitWord32Shift(this, node, kX64Ror32);
}


void InstructionSelector::VisitWord64Ror(Node* node) {
  VisitWord64Shift(this, node, kX64Ror);
}


void InstructionSelector::VisitInt32Add(Node* node) {
  VisitBinop(this, node, kX64Add32);
}


void InstructionSelector::VisitInt64Add(Node* node) {
  VisitBinop(this, node, kX64Add);
}


void InstructionSelector::VisitInt32Sub(Node* node) {
  X64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().Is(0)) {
    Emit(kX64Neg32, g.DefineSameAsFirst(node), g.UseRegister(m.right().node()));
  } else {
    VisitBinop(this, node, kX64Sub32);
  }
}


void InstructionSelector::VisitInt64Sub(Node* node) {
  X64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  if (m.left().Is(0)) {
    Emit(kX64Neg, g.DefineSameAsFirst(node), g.UseRegister(m.right().node()));
  } else {
    VisitBinop(this, node, kX64Sub);
  }
}


namespace {

void VisitMul(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();
  if (g.CanBeImmediate(right)) {
    selector->Emit(opcode, g.DefineAsRegister(node), g.Use(left),
                   g.UseImmediate(right));
  } else {
    if (g.CanBeBetterLeftOperand(right)) {
      std::swap(left, right);
    }
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.Use(right));
  }
}


void VisitMulHigh(InstructionSelector* selector, Node* node,
                  ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (selector->IsLive(left) && !selector->IsLive(right)) {
    std::swap(left, right);
  }
  // TODO(turbofan): We use UseUniqueRegister here to improve register
  // allocation.
  selector->Emit(opcode, g.DefineAsFixed(node, rdx), g.UseFixed(left, rax),
                 g.UseUniqueRegister(right));
}


void VisitDiv(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  InstructionOperand* temps[] = {g.TempRegister(rdx)};
  selector->Emit(
      opcode, g.DefineAsFixed(node, rax), g.UseFixed(node->InputAt(0), rax),
      g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps);
}


void VisitMod(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsFixed(node, rdx),
                 g.UseFixed(node->InputAt(0), rax),
                 g.UseUniqueRegister(node->InputAt(1)));
}

}  // namespace


void InstructionSelector::VisitInt32Mul(Node* node) {
  VisitMul(this, node, kX64Imul32);
}


void InstructionSelector::VisitInt64Mul(Node* node) {
  VisitMul(this, node, kX64Imul);
}


void InstructionSelector::VisitInt32MulHigh(Node* node) {
  VisitMulHigh(this, node, kX64ImulHigh32);
}


void InstructionSelector::VisitInt32Div(Node* node) {
  VisitDiv(this, node, kX64Idiv32);
}


void InstructionSelector::VisitInt64Div(Node* node) {
  VisitDiv(this, node, kX64Idiv);
}


void InstructionSelector::VisitUint32Div(Node* node) {
  VisitDiv(this, node, kX64Udiv32);
}


void InstructionSelector::VisitUint64Div(Node* node) {
  VisitDiv(this, node, kX64Udiv);
}


void InstructionSelector::VisitInt32Mod(Node* node) {
  VisitMod(this, node, kX64Idiv32);
}


void InstructionSelector::VisitInt64Mod(Node* node) {
  VisitMod(this, node, kX64Idiv);
}


void InstructionSelector::VisitUint32Mod(Node* node) {
  VisitMod(this, node, kX64Udiv32);
}


void InstructionSelector::VisitUint64Mod(Node* node) {
  VisitMod(this, node, kX64Udiv);
}


void InstructionSelector::VisitUint32MulHigh(Node* node) {
  VisitMulHigh(this, node, kX64UmulHigh32);
}


void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSECvtss2sd, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEInt32ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEUint32ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64ToInt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64ToUint32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeInt32ToInt64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Movsxlq, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToUint64(Node* node) {
  X64OperandGenerator g(this);
  Node* value = node->InputAt(0);
  switch (value->opcode()) {
    case IrOpcode::kWord32And:
    case IrOpcode::kWord32Or:
    case IrOpcode::kWord32Xor:
    case IrOpcode::kWord32Shl:
    case IrOpcode::kWord32Shr:
    case IrOpcode::kWord32Sar:
    case IrOpcode::kWord32Ror:
    case IrOpcode::kWord32Equal:
    case IrOpcode::kInt32Add:
    case IrOpcode::kInt32Sub:
    case IrOpcode::kInt32Mul:
    case IrOpcode::kInt32MulHigh:
    case IrOpcode::kInt32Div:
    case IrOpcode::kInt32LessThan:
    case IrOpcode::kInt32LessThanOrEqual:
    case IrOpcode::kInt32Mod:
    case IrOpcode::kUint32Div:
    case IrOpcode::kUint32LessThan:
    case IrOpcode::kUint32LessThanOrEqual:
    case IrOpcode::kUint32Mod:
    case IrOpcode::kUint32MulHigh: {
      // These 32-bit operations implicitly zero-extend to 64-bit on x64, so the
      // zero-extension is a no-op.
      Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(value));
      return;
    }
    default:
      break;
  }
  Emit(kX64Movl, g.DefineAsRegister(node), g.Use(value));
}


void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSECvtsd2ss, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  X64OperandGenerator g(this);
  Node* value = node->InputAt(0);
  if (CanCover(node, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord64Sar:
      case IrOpcode::kWord64Shr: {
        Int64BinopMatcher m(value);
        if (m.right().Is(32)) {
          Emit(kX64Shr, g.DefineSameAsFirst(node),
               g.UseRegister(m.left().node()), g.TempImmediate(32));
          return;
        }
        break;
      }
      default:
        break;
    }
  }
  Emit(kX64Movl, g.DefineAsRegister(node), g.Use(value));
}


void InstructionSelector::VisitFloat64Add(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64Add, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)));
}


void InstructionSelector::VisitFloat64Sub(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64Sub, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)));
}


void InstructionSelector::VisitFloat64Mul(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64Mul, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)));
}


void InstructionSelector::VisitFloat64Div(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64Div, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)));
}


void InstructionSelector::VisitFloat64Mod(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand* temps[] = {g.TempRegister(rax)};
  Emit(kSSEFloat64Mod, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)), 1,
       temps);
}


void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64Sqrt, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


namespace {

void VisitRRFloat64(InstructionSelector* selector, ArchOpcode opcode,
                    Node* node) {
  X64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}

}  // namespace


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
  X64OperandGenerator g(this);
  CallDescriptor* descriptor = OpParameter<CallDescriptor*>(node);

  FrameStateDescriptor* frame_state_descriptor = NULL;
  if (descriptor->NeedsFrameState()) {
    frame_state_descriptor = GetFrameStateDescriptor(
        node->InputAt(static_cast<int>(descriptor->InputCount())));
  }

  CallBuffer buffer(zone(), descriptor, frame_state_descriptor);

  // Compute InstructionOperands for inputs and outputs.
  InitializeCallBuffer(node, &buffer, true, true);

  // Push any stack arguments.
  for (NodeVectorRIter input = buffer.pushed_nodes.rbegin();
       input != buffer.pushed_nodes.rend(); input++) {
    // TODO(titzer): handle pushing double parameters.
    Emit(kX64Push, NULL,
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


// Shared routine for multiple compare operations.
static void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                         InstructionOperand* left, InstructionOperand* right,
                         FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  opcode = cont->Encode(opcode);
  if (cont->IsBranch()) {
    selector->Emit(opcode, NULL, left, right, g.Label(cont->true_block()),
                   g.Label(cont->false_block()))->MarkAsControl();
  } else {
    DCHECK(cont->IsSet());
    selector->Emit(opcode, g.DefineAsRegister(cont->result()), left, right);
  }
}


// Shared routine for multiple compare operations.
static void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                         Node* left, Node* right, FlagsContinuation* cont,
                         bool commutative) {
  X64OperandGenerator g(selector);
  if (commutative && g.CanBeBetterLeftOperand(right)) {
    std::swap(left, right);
  }
  VisitCompare(selector, opcode, g.UseRegister(left), g.Use(right), cont);
}


// Shared routine for multiple word compare operations.
static void VisitWordCompare(InstructionSelector* selector, Node* node,
                             InstructionCode opcode, FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
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


// Shared routine for comparison with zero.
static void VisitCompareZero(InstructionSelector* selector, Node* node,
                             InstructionCode opcode, FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  VisitCompare(selector, opcode, g.Use(node), g.TempImmediate(0), cont);
}


// Shared routine for multiple float64 compare operations.
static void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                                FlagsContinuation* cont) {
  VisitCompare(selector, kSSEFloat64Cmp, node->InputAt(0), node->InputAt(1),
               cont, node->op()->HasProperty(Operator::kCommutative));
}


void InstructionSelector::VisitBranch(Node* branch, BasicBlock* tbranch,
                                      BasicBlock* fbranch) {
  X64OperandGenerator g(this);
  Node* user = branch;
  Node* value = branch->InputAt(0);

  FlagsContinuation cont(kNotEqual, tbranch, fbranch);

  // If we can fall through to the true block, invert the branch.
  if (IsNextInAssemblyOrder(tbranch)) {
    cont.Negate();
    cont.SwapBlocks();
  }

  // Try to combine with comparisons against 0 by simply inverting the branch.
  while (CanCover(user, value)) {
    if (value->opcode() == IrOpcode::kWord32Equal) {
      Int32BinopMatcher m(value);
      if (m.right().Is(0)) {
        user = value;
        value = m.left().node();
        cont.Negate();
      } else {
        break;
      }
    } else if (value->opcode() == IrOpcode::kWord64Equal) {
      Int64BinopMatcher m(value);
      if (m.right().Is(0)) {
        user = value;
        value = m.left().node();
        cont.Negate();
      } else {
        break;
      }
    } else {
      break;
    }
  }

  // Try to combine the branch with a comparison.
  if (CanCover(user, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord32Equal:
        cont.OverwriteAndNegateIfEqual(kEqual);
        return VisitWordCompare(this, value, kX64Cmp32, &cont);
      case IrOpcode::kInt32LessThan:
        cont.OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWordCompare(this, value, kX64Cmp32, &cont);
      case IrOpcode::kInt32LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWordCompare(this, value, kX64Cmp32, &cont);
      case IrOpcode::kUint32LessThan:
        cont.OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWordCompare(this, value, kX64Cmp32, &cont);
      case IrOpcode::kUint32LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWordCompare(this, value, kX64Cmp32, &cont);
      case IrOpcode::kWord64Equal:
        cont.OverwriteAndNegateIfEqual(kEqual);
        return VisitWordCompare(this, value, kX64Cmp, &cont);
      case IrOpcode::kInt64LessThan:
        cont.OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWordCompare(this, value, kX64Cmp, &cont);
      case IrOpcode::kInt64LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWordCompare(this, value, kX64Cmp, &cont);
      case IrOpcode::kUint64LessThan:
        cont.OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWordCompare(this, value, kX64Cmp, &cont);
      case IrOpcode::kFloat64Equal:
        cont.OverwriteAndNegateIfEqual(kUnorderedEqual);
        return VisitFloat64Compare(this, value, &cont);
      case IrOpcode::kFloat64LessThan:
        cont.OverwriteAndNegateIfEqual(kUnorderedLessThan);
        return VisitFloat64Compare(this, value, &cont);
      case IrOpcode::kFloat64LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kUnorderedLessThanOrEqual);
        return VisitFloat64Compare(this, value, &cont);
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
          if (result == NULL || IsDefined(result)) {
            switch (node->opcode()) {
              case IrOpcode::kInt32AddWithOverflow:
                cont.OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kX64Add32, &cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont.OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kX64Sub32, &cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Sub:
        return VisitWordCompare(this, value, kX64Cmp32, &cont);
      case IrOpcode::kInt64Sub:
        return VisitWordCompare(this, value, kX64Cmp, &cont);
      case IrOpcode::kWord32And:
        return VisitWordCompare(this, value, kX64Test32, &cont);
      case IrOpcode::kWord64And:
        return VisitWordCompare(this, value, kX64Test, &cont);
      default:
        break;
    }
  }

  // Branch could not be combined with a compare, emit compare against 0.
  VisitCompareZero(this, value, kX64Cmp32, &cont);
}


void InstructionSelector::VisitWord32Equal(Node* const node) {
  Node* user = node;
  FlagsContinuation cont(kEqual, node);
  Int32BinopMatcher m(user);
  if (m.right().Is(0)) {
    Node* value = m.left().node();

    // Try to combine with comparisons against 0 by simply inverting the branch.
    while (CanCover(user, value) && value->opcode() == IrOpcode::kWord32Equal) {
      Int32BinopMatcher m(value);
      if (m.right().Is(0)) {
        user = value;
        value = m.left().node();
        cont.Negate();
      } else {
        break;
      }
    }

    // Try to combine the branch with a comparison.
    if (CanCover(user, value)) {
      switch (value->opcode()) {
        case IrOpcode::kInt32Sub:
          return VisitWordCompare(this, value, kX64Cmp32, &cont);
        case IrOpcode::kWord32And:
          return VisitWordCompare(this, value, kX64Test32, &cont);
        default:
          break;
      }
    }
    return VisitCompareZero(this, value, kX64Cmp32, &cont);
  }
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}


void InstructionSelector::VisitInt32LessThan(Node* node) {
  FlagsContinuation cont(kSignedLessThan, node);
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}


void InstructionSelector::VisitInt32LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}


void InstructionSelector::VisitUint32LessThan(Node* node) {
  FlagsContinuation cont(kUnsignedLessThan, node);
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}


void InstructionSelector::VisitUint32LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kUnsignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}


void InstructionSelector::VisitWord64Equal(Node* const node) {
  Node* user = node;
  FlagsContinuation cont(kEqual, node);
  Int64BinopMatcher m(user);
  if (m.right().Is(0)) {
    Node* value = m.left().node();

    // Try to combine with comparisons against 0 by simply inverting the branch.
    while (CanCover(user, value) && value->opcode() == IrOpcode::kWord64Equal) {
      Int64BinopMatcher m(value);
      if (m.right().Is(0)) {
        user = value;
        value = m.left().node();
        cont.Negate();
      } else {
        break;
      }
    }

    // Try to combine the branch with a comparison.
    if (CanCover(user, value)) {
      switch (value->opcode()) {
        case IrOpcode::kInt64Sub:
          return VisitWordCompare(this, value, kX64Cmp, &cont);
        case IrOpcode::kWord64And:
          return VisitWordCompare(this, value, kX64Test, &cont);
        default:
          break;
      }
    }
    return VisitCompareZero(this, value, kX64Cmp, &cont);
  }
  VisitWordCompare(this, node, kX64Cmp, &cont);
}


void InstructionSelector::VisitInt32AddWithOverflow(Node* node) {
  if (Node* ovf = node->FindProjection(1)) {
    FlagsContinuation cont(kOverflow, ovf);
    VisitBinop(this, node, kX64Add32, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kX64Add32, &cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = node->FindProjection(1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitBinop(this, node, kX64Sub32, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kX64Sub32, &cont);
}


void InstructionSelector::VisitInt64LessThan(Node* node) {
  FlagsContinuation cont(kSignedLessThan, node);
  VisitWordCompare(this, node, kX64Cmp, &cont);
}


void InstructionSelector::VisitInt64LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kX64Cmp, &cont);
}


void InstructionSelector::VisitUint64LessThan(Node* node) {
  FlagsContinuation cont(kUnsignedLessThan, node);
  VisitWordCompare(this, node, kX64Cmp, &cont);
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
           MachineOperatorBuilder::kFloat64RoundTruncate;
  }
  return MachineOperatorBuilder::kNoFlags;
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8
