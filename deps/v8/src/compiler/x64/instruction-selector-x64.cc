// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Adds X64-specific methods for generating operands.
class X64OperandGenerator V8_FINAL : public OperandGenerator {
 public:
  explicit X64OperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand* TempRegister(Register reg) {
    return new (zone()) UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                                           Register::ToAllocationIndex(reg));
  }

  InstructionOperand* UseByteRegister(Node* node) {
    // TODO(dcarney): relax constraint.
    return UseFixed(node, rdx);
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
        Handle<HeapObject> value = ValueOf<Handle<HeapObject> >(node->op());
        return !isolate()->heap()->InNewSpace(*value);
      }
      default:
        return false;
    }
  }
};


void InstructionSelector::VisitLoad(Node* node) {
  MachineType rep = OpParameter<MachineType>(node);
  X64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  InstructionOperand* output = rep == kMachineFloat64
                                   ? g.DefineAsDoubleRegister(node)
                                   : g.DefineAsRegister(node);
  ArchOpcode opcode;
  switch (rep) {
    case kMachineFloat64:
      opcode = kSSELoad;
      break;
    case kMachineWord8:
      opcode = kX64LoadWord8;
      break;
    case kMachineWord16:
      opcode = kX64LoadWord16;
      break;
    case kMachineWord32:
      opcode = kX64LoadWord32;
      break;
    case kMachineTagged:  // Fall through.
    case kMachineWord64:
      opcode = kX64LoadWord64;
      break;
    default:
      UNREACHABLE();
      return;
  }
  if (g.CanBeImmediate(base)) {
    // load [#base + %index]
    Emit(opcode | AddressingModeField::encode(kMode_MRI), output,
         g.UseRegister(index), g.UseImmediate(base));
  } else if (g.CanBeImmediate(index)) {  // load [%base + #index]
    Emit(opcode | AddressingModeField::encode(kMode_MRI), output,
         g.UseRegister(base), g.UseImmediate(index));
  } else {  // load [%base + %index + K]
    Emit(opcode | AddressingModeField::encode(kMode_MR1I), output,
         g.UseRegister(base), g.UseRegister(index));
  }
  // TODO(turbofan): addressing modes [r+r*{2,4,8}+K]
}


void InstructionSelector::VisitStore(Node* node) {
  X64OperandGenerator g(this);
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
    InstructionOperand* temps[] = {g.TempRegister(rcx), g.TempRegister(rdx)};
    Emit(kX64StoreWriteBarrier, NULL, g.UseFixed(base, rbx),
         g.UseFixed(index, rcx), g.UseFixed(value, rdx), ARRAY_SIZE(temps),
         temps);
    return;
  }
  DCHECK_EQ(kNoWriteBarrier, store_rep.write_barrier_kind);
  bool is_immediate = false;
  InstructionOperand* val;
  if (rep == kMachineFloat64) {
    val = g.UseDoubleRegister(value);
  } else {
    is_immediate = g.CanBeImmediate(value);
    if (is_immediate) {
      val = g.UseImmediate(value);
    } else if (rep == kMachineWord8) {
      val = g.UseByteRegister(value);
    } else {
      val = g.UseRegister(value);
    }
  }
  ArchOpcode opcode;
  switch (rep) {
    case kMachineFloat64:
      opcode = kSSEStore;
      break;
    case kMachineWord8:
      opcode = is_immediate ? kX64StoreWord8I : kX64StoreWord8;
      break;
    case kMachineWord16:
      opcode = is_immediate ? kX64StoreWord16I : kX64StoreWord16;
      break;
    case kMachineWord32:
      opcode = is_immediate ? kX64StoreWord32I : kX64StoreWord32;
      break;
    case kMachineTagged:  // Fall through.
    case kMachineWord64:
      opcode = is_immediate ? kX64StoreWord64I : kX64StoreWord64;
      break;
    default:
      UNREACHABLE();
      return;
  }
  if (g.CanBeImmediate(base)) {
    // store [#base + %index], %|#value
    Emit(opcode | AddressingModeField::encode(kMode_MRI), NULL,
         g.UseRegister(index), g.UseImmediate(base), val);
  } else if (g.CanBeImmediate(index)) {  // store [%base + #index], %|#value
    Emit(opcode | AddressingModeField::encode(kMode_MRI), NULL,
         g.UseRegister(base), g.UseImmediate(index), val);
  } else {  // store [%base + %index], %|#value
    Emit(opcode | AddressingModeField::encode(kMode_MR1I), NULL,
         g.UseRegister(base), g.UseRegister(index), val);
  }
  // TODO(turbofan): addressing modes [r+r*{2,4,8}+K]
}


// Shared routine for multiple binary operations.
static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand* inputs[4];
  size_t input_count = 0;
  InstructionOperand* outputs[2];
  size_t output_count = 0;

  // TODO(turbofan): match complex addressing modes.
  // TODO(turbofan): if commutative, pick the non-live-in operand as the left as
  // this might be the last use and therefore its register can be reused.
  if (g.CanBeImmediate(m.right().node())) {
    inputs[input_count++] = g.Use(m.left().node());
    inputs[input_count++] = g.UseImmediate(m.right().node());
  } else {
    inputs[input_count++] = g.UseRegister(m.left().node());
    inputs[input_count++] = g.Use(m.right().node());
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
  DCHECK_GE(ARRAY_SIZE(inputs), input_count);
  DCHECK_GE(ARRAY_SIZE(outputs), output_count);

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


template <typename T>
static void VisitXor(InstructionSelector* selector, Node* node,
                     ArchOpcode xor_opcode, ArchOpcode not_opcode) {
  X64OperandGenerator g(selector);
  BinopMatcher<IntMatcher<T>, IntMatcher<T> > m(node);
  if (m.right().Is(-1)) {
    selector->Emit(not_opcode, g.DefineSameAsFirst(node),
                   g.Use(m.left().node()));
  } else {
    VisitBinop(selector, node, xor_opcode);
  }
}


void InstructionSelector::VisitWord32Xor(Node* node) {
  VisitXor<int32_t>(this, node, kX64Xor32, kX64Not32);
}


void InstructionSelector::VisitWord64Xor(Node* node) {
  VisitXor<int64_t>(this, node, kX64Xor, kX64Not);
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


void InstructionSelector::VisitInt32Add(Node* node) {
  VisitBinop(this, node, kX64Add32);
}


void InstructionSelector::VisitInt64Add(Node* node) {
  VisitBinop(this, node, kX64Add);
}


template <typename T>
static void VisitSub(InstructionSelector* selector, Node* node,
                     ArchOpcode sub_opcode, ArchOpcode neg_opcode) {
  X64OperandGenerator g(selector);
  BinopMatcher<IntMatcher<T>, IntMatcher<T> > m(node);
  if (m.left().Is(0)) {
    selector->Emit(neg_opcode, g.DefineSameAsFirst(node),
                   g.Use(m.right().node()));
  } else {
    VisitBinop(selector, node, sub_opcode);
  }
}


void InstructionSelector::VisitInt32Sub(Node* node) {
  VisitSub<int32_t>(this, node, kX64Sub32, kX64Neg32);
}


void InstructionSelector::VisitInt64Sub(Node* node) {
  VisitSub<int64_t>(this, node, kX64Sub, kX64Neg);
}


static void VisitMul(InstructionSelector* selector, Node* node,
                     ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (g.CanBeImmediate(right)) {
    selector->Emit(opcode, g.DefineAsRegister(node), g.Use(left),
                   g.UseImmediate(right));
  } else if (g.CanBeImmediate(left)) {
    selector->Emit(opcode, g.DefineAsRegister(node), g.Use(right),
                   g.UseImmediate(left));
  } else {
    // TODO(turbofan): select better left operand.
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
      g.UseUniqueRegister(node->InputAt(1)), ARRAY_SIZE(temps), temps);
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
      g.UseUniqueRegister(node->InputAt(1)), ARRAY_SIZE(temps), temps);
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


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEInt32ToFloat64, g.DefineAsDoubleRegister(node),
       g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  // TODO(turbofan): X64 SSE cvtqsi2sd should support operands.
  Emit(kSSEUint32ToFloat64, g.DefineAsDoubleRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64ToInt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  X64OperandGenerator g(this);
  // TODO(turbofan): X64 SSE cvttsd2siq should support operands.
  Emit(kSSEFloat64ToUint32, g.DefineAsRegister(node),
       g.UseDoubleRegister(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64Add(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64Add, g.DefineSameAsFirst(node),
       g.UseDoubleRegister(node->InputAt(0)),
       g.UseDoubleRegister(node->InputAt(1)));
}


void InstructionSelector::VisitFloat64Sub(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64Sub, g.DefineSameAsFirst(node),
       g.UseDoubleRegister(node->InputAt(0)),
       g.UseDoubleRegister(node->InputAt(1)));
}


void InstructionSelector::VisitFloat64Mul(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64Mul, g.DefineSameAsFirst(node),
       g.UseDoubleRegister(node->InputAt(0)),
       g.UseDoubleRegister(node->InputAt(1)));
}


void InstructionSelector::VisitFloat64Div(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64Div, g.DefineSameAsFirst(node),
       g.UseDoubleRegister(node->InputAt(0)),
       g.UseDoubleRegister(node->InputAt(1)));
}


void InstructionSelector::VisitFloat64Mod(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand* temps[] = {g.TempRegister(rax)};
  Emit(kSSEFloat64Mod, g.DefineSameAsFirst(node),
       g.UseDoubleRegister(node->InputAt(0)),
       g.UseDoubleRegister(node->InputAt(1)), 1, temps);
}


void InstructionSelector::VisitConvertInt64ToInt32(Node* node) {
  X64OperandGenerator g(this);
  // TODO(dcarney): other modes
  Emit(kX64Int64ToInt32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitConvertInt32ToInt64(Node* node) {
  X64OperandGenerator g(this);
  // TODO(dcarney): other modes
  Emit(kX64Int32ToInt64, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
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
  VisitCompare(this, kSSEFloat64Cmp, g.UseDoubleRegister(left), g.Use(right),
               cont);
}


void InstructionSelector::VisitCall(Node* call, BasicBlock* continuation,
                                    BasicBlock* deoptimization) {
  X64OperandGenerator g(this);
  CallDescriptor* descriptor = OpParameter<CallDescriptor*>(call);
  CallBuffer buffer(zone(), descriptor);  // TODO(turbofan): temp zone here?

  // Compute InstructionOperands for inputs and outputs.
  InitializeCallBuffer(call, &buffer, true, true, continuation, deoptimization);

  // TODO(dcarney): stack alignment for c calls.
  // TODO(dcarney): shadow space on window for c calls.
  // Push any stack arguments.
  for (int i = buffer.pushed_count - 1; i >= 0; --i) {
    Node* input = buffer.pushed_nodes[i];
    // TODO(titzer): handle pushing double parameters.
    if (g.CanBeImmediate(input)) {
      Emit(kX64PushI, NULL, g.UseImmediate(input));
    } else {
      Emit(kX64Push, NULL, g.Use(input));
    }
  }

  // Select the appropriate opcode based on the call type.
  InstructionCode opcode;
  switch (descriptor->kind()) {
    case CallDescriptor::kCallCodeObject: {
      bool lazy_deopt = descriptor->CanLazilyDeoptimize();
      opcode = kX64CallCodeObject | MiscField::encode(lazy_deopt ? 1 : 0);
      break;
    }
    case CallDescriptor::kCallAddress:
      opcode = kX64CallAddress;
      break;
    case CallDescriptor::kCallJSFunction:
      opcode = kX64CallJSFunction;
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
    Emit(kPopStack | MiscField::encode(buffer.pushed_count), NULL);
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
