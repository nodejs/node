// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  InstructionOperand* UseImmediate64(Node* node) { return UseImmediate(node); }

  bool CanBeImmediate(Node* node) {
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
        return true;
      default:
        return false;
    }
  }

  bool CanBeImmediate64(Node* node) {
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
        return true;
      case IrOpcode::kNumberConstant:
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

  bool CanBeBetterLeftOperand(Node* node) const {
    return !selector()->IsLive(node);
  }
};


class AddressingModeMatcher {
 public:
  AddressingModeMatcher(X64OperandGenerator* g, Node* base, Node* index)
      : base_operand_(NULL),
        index_operand_(NULL),
        displacement_operand_(NULL),
        mode_(kMode_None) {
    Int32Matcher index_imm(index);
    if (index_imm.HasValue()) {
      int32_t value = index_imm.Value();
      if (value == 0) {
        mode_ = kMode_MR;
      } else {
        mode_ = kMode_MRI;
        index_operand_ = g->UseImmediate(index);
      }
      base_operand_ = g->UseRegister(base);
    } else {
      // Compute base operand.
      Int64Matcher base_imm(base);
      if (!base_imm.HasValue() || base_imm.Value() != 0) {
        base_operand_ = g->UseRegister(base);
      }
      // Compute index and displacement.
      IndexAndDisplacementMatcher matcher(index);
      index_operand_ = g->UseRegister(matcher.index_node());
      if (matcher.displacement() != 0) {
        displacement_operand_ = g->TempImmediate(matcher.displacement());
      }
      // Compute mode with scale factor one.
      if (base_operand_ == NULL) {
        if (displacement_operand_ == NULL) {
          mode_ = kMode_M1;
        } else {
          mode_ = kMode_M1I;
        }
      } else {
        if (displacement_operand_ == NULL) {
          mode_ = kMode_MR1;
        } else {
          mode_ = kMode_MR1I;
        }
      }
      // Adjust mode to actual scale factor.
      mode_ = GetMode(mode_, matcher.power());
    }
    DCHECK_NE(kMode_None, mode_);
  }

  AddressingMode GetMode(AddressingMode one, int power) {
    return static_cast<AddressingMode>(static_cast<int>(one) + power);
  }

  size_t SetInputs(InstructionOperand** inputs) {
    size_t input_count = 0;
    // Compute inputs_ and input_count.
    if (base_operand_ != NULL) {
      inputs[input_count++] = base_operand_;
    }
    if (index_operand_ != NULL) {
      inputs[input_count++] = index_operand_;
    }
    if (displacement_operand_ != NULL) {
      // Pure displacement mode not supported by x64.
      DCHECK_NE(input_count, 0);
      inputs[input_count++] = displacement_operand_;
    }
    DCHECK_NE(input_count, 0);
    return input_count;
  }

  static const int kMaxInputCount = 3;
  InstructionOperand* base_operand_;
  InstructionOperand* index_operand_;
  InstructionOperand* displacement_operand_;
  AddressingMode mode_;
};


void InstructionSelector::VisitLoad(Node* node) {
  MachineType rep = RepresentationOf(OpParameter<LoadRepresentation>(node));
  MachineType typ = TypeOf(OpParameter<LoadRepresentation>(node));
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  ArchOpcode opcode;
  // TODO(titzer): signed/unsigned small loads
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

  X64OperandGenerator g(this);
  AddressingModeMatcher matcher(&g, base, index);
  InstructionCode code = opcode | AddressingModeField::encode(matcher.mode_);
  InstructionOperand* outputs[] = {g.DefineAsRegister(node)};
  InstructionOperand* inputs[AddressingModeMatcher::kMaxInputCount];
  size_t input_count = matcher.SetInputs(inputs);
  Emit(code, 1, outputs, input_count, inputs);
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

  InstructionOperand* val;
  if (g.CanBeImmediate(value)) {
    val = g.UseImmediate(value);
  } else {
    val = g.UseRegister(value);
  }

  AddressingModeMatcher matcher(&g, base, index);
  InstructionCode code = opcode | AddressingModeField::encode(matcher.mode_);
  InstructionOperand* inputs[AddressingModeMatcher::kMaxInputCount + 1];
  size_t input_count = matcher.SetInputs(inputs);
  inputs[input_count++] = val;
  Emit(code, 0, static_cast<InstructionOperand**>(NULL), input_count, inputs);
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
  if (g.CanBeImmediate(right)) {
    inputs[input_count++] = g.Use(left);
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
    Emit(kX64Not32, g.DefineSameAsFirst(node), g.Use(m.left().node()));
  } else {
    VisitBinop(this, node, kX64Xor32);
  }
}


void InstructionSelector::VisitWord64Xor(Node* node) {
  X64OperandGenerator g(this);
  Uint64BinopMatcher m(node);
  if (m.right().Is(-1)) {
    Emit(kX64Not, g.DefineSameAsFirst(node), g.Use(m.left().node()));
  } else {
    VisitBinop(this, node, kX64Xor);
  }
}


// Shared routine for multiple 32-bit shift operations.
// TODO(bmeurer): Merge this with VisitWord64Shift using template magic?
static void VisitWord32Shift(InstructionSelector* selector, Node* node,
                             ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  // TODO(turbofan): assembler only supports some addressing modes for shifts.
  if (g.CanBeImmediate(right)) {
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.UseImmediate(right));
  } else {
    Int32BinopMatcher m(node);
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
static void VisitWord64Shift(InstructionSelector* selector, Node* node,
                             ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  // TODO(turbofan): assembler only supports some addressing modes for shifts.
  if (g.CanBeImmediate(right)) {
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.UseImmediate(right));
  } else {
    Int64BinopMatcher m(node);
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


void InstructionSelector::VisitWord32Shl(Node* node) {
  VisitWord32Shift(this, node, kX64Shl32);
}


void InstructionSelector::VisitWord64Shl(Node* node) {
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
    Emit(kX64Neg32, g.DefineSameAsFirst(node), g.Use(m.right().node()));
  } else {
    VisitBinop(this, node, kX64Sub32);
  }
}


void InstructionSelector::VisitInt64Sub(Node* node) {
  X64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  if (m.left().Is(0)) {
    Emit(kX64Neg, g.DefineSameAsFirst(node), g.Use(m.right().node()));
  } else {
    VisitBinop(this, node, kX64Sub);
  }
}


static void VisitMul(InstructionSelector* selector, Node* node,
                     ArchOpcode opcode) {
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


void InstructionSelector::VisitInt32Mul(Node* node) {
  VisitMul(this, node, kX64Imul32);
}


void InstructionSelector::VisitInt64Mul(Node* node) {
  VisitMul(this, node, kX64Imul);
}


static void VisitDiv(InstructionSelector* selector, Node* node,
                     ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  InstructionOperand* temps[] = {g.TempRegister(rdx)};
  selector->Emit(
      opcode, g.DefineAsFixed(node, rax), g.UseFixed(node->InputAt(0), rax),
      g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps);
}


void InstructionSelector::VisitInt32Div(Node* node) {
  VisitDiv(this, node, kX64Idiv32);
}


void InstructionSelector::VisitInt64Div(Node* node) {
  VisitDiv(this, node, kX64Idiv);
}


void InstructionSelector::VisitInt32UDiv(Node* node) {
  VisitDiv(this, node, kX64Udiv32);
}


void InstructionSelector::VisitInt64UDiv(Node* node) {
  VisitDiv(this, node, kX64Udiv);
}


static void VisitMod(InstructionSelector* selector, Node* node,
                     ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  InstructionOperand* temps[] = {g.TempRegister(rax), g.TempRegister(rdx)};
  selector->Emit(
      opcode, g.DefineAsFixed(node, rdx), g.UseFixed(node->InputAt(0), rax),
      g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps);
}


void InstructionSelector::VisitInt32Mod(Node* node) {
  VisitMod(this, node, kX64Idiv32);
}


void InstructionSelector::VisitInt64Mod(Node* node) {
  VisitMod(this, node, kX64Idiv);
}


void InstructionSelector::VisitInt32UMod(Node* node) {
  VisitMod(this, node, kX64Udiv32);
}


void InstructionSelector::VisitInt64UMod(Node* node) {
  VisitMod(this, node, kX64Udiv);
}


void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  // TODO(turbofan): X64 SSE conversions should take an operand.
  Emit(kSSECvtss2sd, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEInt32ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  // TODO(turbofan): X64 SSE cvtqsi2sd should support operands.
  Emit(kSSEUint32ToFloat64, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
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
  Emit(kX64Movl, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  X64OperandGenerator g(this);
  // TODO(turbofan): X64 SSE conversions should take an operand.
  Emit(kSSECvtsd2ss, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Movl, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64Add(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64Add, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}


void InstructionSelector::VisitFloat64Sub(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64Sub, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}


void InstructionSelector::VisitFloat64Mul(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64Mul, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}


void InstructionSelector::VisitFloat64Div(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64Div, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
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


void InstructionSelector::VisitInt32AddWithOverflow(Node* node,
                                                    FlagsContinuation* cont) {
  VisitBinop(this, node, kX64Add32, cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node,
                                                    FlagsContinuation* cont) {
  VisitBinop(this, node, kX64Sub32, cont);
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


// Shared routine for multiple word compare operations.
static void VisitWordCompare(InstructionSelector* selector, Node* node,
                             InstructionCode opcode, FlagsContinuation* cont,
                             bool commutative) {
  X64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  // Match immediates on left or right side of comparison.
  if (g.CanBeImmediate(right)) {
    VisitCompare(selector, opcode, g.Use(left), g.UseImmediate(right), cont);
  } else if (g.CanBeImmediate(left)) {
    if (!commutative) cont->Commute();
    VisitCompare(selector, opcode, g.Use(right), g.UseImmediate(left), cont);
  } else {
    VisitCompare(selector, opcode, g.UseRegister(left), g.Use(right), cont);
  }
}


void InstructionSelector::VisitWord32Test(Node* node, FlagsContinuation* cont) {
  switch (node->opcode()) {
    case IrOpcode::kInt32Sub:
      return VisitWordCompare(this, node, kX64Cmp32, cont, false);
    case IrOpcode::kWord32And:
      return VisitWordCompare(this, node, kX64Test32, cont, true);
    default:
      break;
  }

  X64OperandGenerator g(this);
  VisitCompare(this, kX64Test32, g.Use(node), g.TempImmediate(-1), cont);
}


void InstructionSelector::VisitWord64Test(Node* node, FlagsContinuation* cont) {
  switch (node->opcode()) {
    case IrOpcode::kInt64Sub:
      return VisitWordCompare(this, node, kX64Cmp, cont, false);
    case IrOpcode::kWord64And:
      return VisitWordCompare(this, node, kX64Test, cont, true);
    default:
      break;
  }

  X64OperandGenerator g(this);
  VisitCompare(this, kX64Test, g.Use(node), g.TempImmediate(-1), cont);
}


void InstructionSelector::VisitWord32Compare(Node* node,
                                             FlagsContinuation* cont) {
  VisitWordCompare(this, node, kX64Cmp32, cont, false);
}


void InstructionSelector::VisitWord64Compare(Node* node,
                                             FlagsContinuation* cont) {
  VisitWordCompare(this, node, kX64Cmp, cont, false);
}


void InstructionSelector::VisitFloat64Compare(Node* node,
                                              FlagsContinuation* cont) {
  X64OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  VisitCompare(this, kSSEFloat64Cmp, g.UseRegister(left), g.Use(right), cont);
}


void InstructionSelector::VisitCall(Node* call, BasicBlock* continuation,
                                    BasicBlock* deoptimization) {
  X64OperandGenerator g(this);
  CallDescriptor* descriptor = OpParameter<CallDescriptor*>(call);

  FrameStateDescriptor* frame_state_descriptor = NULL;
  if (descriptor->NeedsFrameState()) {
    frame_state_descriptor = GetFrameStateDescriptor(
        call->InputAt(static_cast<int>(descriptor->InputCount())));
  }

  CallBuffer buffer(zone(), descriptor, frame_state_descriptor);

  // Compute InstructionOperands for inputs and outputs.
  InitializeCallBuffer(call, &buffer, true, true);

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
  Instruction* call_instr =
      Emit(opcode, buffer.outputs.size(), &buffer.outputs.front(),
           buffer.instruction_args.size(), &buffer.instruction_args.front());

  call_instr->MarkAsCall();
  if (deoptimization != NULL) {
    DCHECK(continuation != NULL);
    call_instr->MarkAsControl();
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
