// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"

namespace v8 {
namespace internal {
namespace compiler {

enum ImmediateMode {
  kArithimeticImm,  // 12 bit unsigned immediate shifted left 0 or 12 bits
  kShift32Imm,      // 0 - 31
  kShift64Imm,      // 0 -63
  kLogical32Imm,
  kLogical64Imm,
  kLoadStoreImm,  // unsigned 9 bit or signed 7 bit
  kNoImmediate
};


// Adds Arm64-specific methods for generating operands.
class Arm64OperandGenerator V8_FINAL : public OperandGenerator {
 public:
  explicit Arm64OperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand* UseOperand(Node* node, ImmediateMode mode) {
    if (CanBeImmediate(node, mode)) {
      return UseImmediate(node);
    }
    return UseRegister(node);
  }

  bool CanBeImmediate(Node* node, ImmediateMode mode) {
    int64_t value;
    switch (node->opcode()) {
      // TODO(turbofan): SMI number constants as immediates.
      case IrOpcode::kInt32Constant:
        value = ValueOf<int32_t>(node->op());
        break;
      default:
        return false;
    }
    unsigned ignored;
    switch (mode) {
      case kLogical32Imm:
        // TODO(dcarney): some unencodable values can be handled by
        // switching instructions.
        return Assembler::IsImmLogical(static_cast<uint64_t>(value), 32,
                                       &ignored, &ignored, &ignored);
      case kLogical64Imm:
        return Assembler::IsImmLogical(static_cast<uint64_t>(value), 64,
                                       &ignored, &ignored, &ignored);
      case kArithimeticImm:
        // TODO(dcarney): -values can be handled by instruction swapping
        return Assembler::IsImmAddSub(value);
      case kShift32Imm:
        return 0 <= value && value < 31;
      case kShift64Imm:
        return 0 <= value && value < 63;
      case kLoadStoreImm:
        return (0 <= value && value < (1 << 9)) ||
               (-(1 << 6) <= value && value < (1 << 6));
      case kNoImmediate:
        return false;
    }
    return false;
  }
};


static void VisitRR(InstructionSelector* selector, ArchOpcode opcode,
                    Node* node) {
  Arm64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}


static void VisitRRR(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node) {
  Arm64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
}


static void VisitRRRFloat64(InstructionSelector* selector, ArchOpcode opcode,
                            Node* node) {
  Arm64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsDoubleRegister(node),
                 g.UseDoubleRegister(node->InputAt(0)),
                 g.UseDoubleRegister(node->InputAt(1)));
}


static void VisitRRO(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node, ImmediateMode operand_mode) {
  Arm64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseOperand(node->InputAt(1), operand_mode));
}


// Shared routine for multiple binary operations.
static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, ImmediateMode operand_mode,
                       FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand* inputs[4];
  size_t input_count = 0;
  InstructionOperand* outputs[2];
  size_t output_count = 0;

  inputs[input_count++] = g.UseRegister(m.left().node());
  inputs[input_count++] = g.UseOperand(m.right().node(), operand_mode);

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

  Instruction* instr = selector->Emit(cont->Encode(opcode), output_count,
                                      outputs, input_count, inputs);
  if (cont->IsBranch()) instr->MarkAsControl();
}


// Shared routine for multiple binary operations.
static void VisitBinop(InstructionSelector* selector, Node* node,
                       ArchOpcode opcode, ImmediateMode operand_mode) {
  FlagsContinuation cont;
  VisitBinop(selector, node, opcode, operand_mode, &cont);
}


void InstructionSelector::VisitLoad(Node* node) {
  MachineType rep = OpParameter<MachineType>(node);
  Arm64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  InstructionOperand* result = rep == kMachineFloat64
                                   ? g.DefineAsDoubleRegister(node)
                                   : g.DefineAsRegister(node);

  ArchOpcode opcode;
  switch (rep) {
    case kMachineFloat64:
      opcode = kArm64Float64Load;
      break;
    case kMachineWord8:
      opcode = kArm64LoadWord8;
      break;
    case kMachineWord16:
      opcode = kArm64LoadWord16;
      break;
    case kMachineWord32:
      opcode = kArm64LoadWord32;
      break;
    case kMachineTagged:  // Fall through.
    case kMachineWord64:
      opcode = kArm64LoadWord64;
      break;
    default:
      UNREACHABLE();
      return;
  }
  if (g.CanBeImmediate(index, kLoadStoreImm)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), result,
         g.UseRegister(base), g.UseImmediate(index));
  } else if (g.CanBeImmediate(base, kLoadStoreImm)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), result,
         g.UseRegister(index), g.UseImmediate(base));
  } else {
    Emit(opcode | AddressingModeField::encode(kMode_MRR), result,
         g.UseRegister(base), g.UseRegister(index));
  }
}


void InstructionSelector::VisitStore(Node* node) {
  Arm64OperandGenerator g(this);
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
    InstructionOperand* temps[] = {g.TempRegister(x11), g.TempRegister(x12)};
    Emit(kArm64StoreWriteBarrier, NULL, g.UseFixed(base, x10),
         g.UseFixed(index, x11), g.UseFixed(value, x12), ARRAY_SIZE(temps),
         temps);
    return;
  }
  DCHECK_EQ(kNoWriteBarrier, store_rep.write_barrier_kind);
  InstructionOperand* val;
  if (rep == kMachineFloat64) {
    val = g.UseDoubleRegister(value);
  } else {
    val = g.UseRegister(value);
  }
  ArchOpcode opcode;
  switch (rep) {
    case kMachineFloat64:
      opcode = kArm64Float64Store;
      break;
    case kMachineWord8:
      opcode = kArm64StoreWord8;
      break;
    case kMachineWord16:
      opcode = kArm64StoreWord16;
      break;
    case kMachineWord32:
      opcode = kArm64StoreWord32;
      break;
    case kMachineTagged:  // Fall through.
    case kMachineWord64:
      opcode = kArm64StoreWord64;
      break;
    default:
      UNREACHABLE();
      return;
  }
  if (g.CanBeImmediate(index, kLoadStoreImm)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), NULL,
         g.UseRegister(base), g.UseImmediate(index), val);
  } else if (g.CanBeImmediate(base, kLoadStoreImm)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), NULL,
         g.UseRegister(index), g.UseImmediate(base), val);
  } else {
    Emit(opcode | AddressingModeField::encode(kMode_MRR), NULL,
         g.UseRegister(base), g.UseRegister(index), val);
  }
}


void InstructionSelector::VisitWord32And(Node* node) {
  VisitBinop(this, node, kArm64And32, kLogical32Imm);
}


void InstructionSelector::VisitWord64And(Node* node) {
  VisitBinop(this, node, kArm64And, kLogical64Imm);
}


void InstructionSelector::VisitWord32Or(Node* node) {
  VisitBinop(this, node, kArm64Or32, kLogical32Imm);
}


void InstructionSelector::VisitWord64Or(Node* node) {
  VisitBinop(this, node, kArm64Or, kLogical64Imm);
}


template <typename T>
static void VisitXor(InstructionSelector* selector, Node* node,
                     ArchOpcode xor_opcode, ArchOpcode not_opcode) {
  Arm64OperandGenerator g(selector);
  BinopMatcher<IntMatcher<T>, IntMatcher<T> > m(node);
  if (m.right().Is(-1)) {
    selector->Emit(not_opcode, g.DefineAsRegister(node),
                   g.UseRegister(m.left().node()));
  } else {
    VisitBinop(selector, node, xor_opcode, kLogical32Imm);
  }
}


void InstructionSelector::VisitWord32Xor(Node* node) {
  VisitXor<int32_t>(this, node, kArm64Xor32, kArm64Not32);
}


void InstructionSelector::VisitWord64Xor(Node* node) {
  VisitXor<int64_t>(this, node, kArm64Xor, kArm64Not);
}


void InstructionSelector::VisitWord32Shl(Node* node) {
  VisitRRO(this, kArm64Shl32, node, kShift32Imm);
}


void InstructionSelector::VisitWord64Shl(Node* node) {
  VisitRRO(this, kArm64Shl, node, kShift64Imm);
}


void InstructionSelector::VisitWord32Shr(Node* node) {
  VisitRRO(this, kArm64Shr32, node, kShift32Imm);
}


void InstructionSelector::VisitWord64Shr(Node* node) {
  VisitRRO(this, kArm64Shr, node, kShift64Imm);
}


void InstructionSelector::VisitWord32Sar(Node* node) {
  VisitRRO(this, kArm64Sar32, node, kShift32Imm);
}


void InstructionSelector::VisitWord64Sar(Node* node) {
  VisitRRO(this, kArm64Sar, node, kShift64Imm);
}


void InstructionSelector::VisitInt32Add(Node* node) {
  VisitBinop(this, node, kArm64Add32, kArithimeticImm);
}


void InstructionSelector::VisitInt64Add(Node* node) {
  VisitBinop(this, node, kArm64Add, kArithimeticImm);
}


template <typename T>
static void VisitSub(InstructionSelector* selector, Node* node,
                     ArchOpcode sub_opcode, ArchOpcode neg_opcode) {
  Arm64OperandGenerator g(selector);
  BinopMatcher<IntMatcher<T>, IntMatcher<T> > m(node);
  if (m.left().Is(0)) {
    selector->Emit(neg_opcode, g.DefineAsRegister(node),
                   g.UseRegister(m.right().node()));
  } else {
    VisitBinop(selector, node, sub_opcode, kArithimeticImm);
  }
}


void InstructionSelector::VisitInt32Sub(Node* node) {
  VisitSub<int32_t>(this, node, kArm64Sub32, kArm64Neg32);
}


void InstructionSelector::VisitInt64Sub(Node* node) {
  VisitSub<int64_t>(this, node, kArm64Sub, kArm64Neg);
}


void InstructionSelector::VisitInt32Mul(Node* node) {
  VisitRRR(this, kArm64Mul32, node);
}


void InstructionSelector::VisitInt64Mul(Node* node) {
  VisitRRR(this, kArm64Mul, node);
}


void InstructionSelector::VisitInt32Div(Node* node) {
  VisitRRR(this, kArm64Idiv32, node);
}


void InstructionSelector::VisitInt64Div(Node* node) {
  VisitRRR(this, kArm64Idiv, node);
}


void InstructionSelector::VisitInt32UDiv(Node* node) {
  VisitRRR(this, kArm64Udiv32, node);
}


void InstructionSelector::VisitInt64UDiv(Node* node) {
  VisitRRR(this, kArm64Udiv, node);
}


void InstructionSelector::VisitInt32Mod(Node* node) {
  VisitRRR(this, kArm64Imod32, node);
}


void InstructionSelector::VisitInt64Mod(Node* node) {
  VisitRRR(this, kArm64Imod, node);
}


void InstructionSelector::VisitInt32UMod(Node* node) {
  VisitRRR(this, kArm64Umod32, node);
}


void InstructionSelector::VisitInt64UMod(Node* node) {
  VisitRRR(this, kArm64Umod, node);
}


void InstructionSelector::VisitConvertInt32ToInt64(Node* node) {
  VisitRR(this, kArm64Int32ToInt64, node);
}


void InstructionSelector::VisitConvertInt64ToInt32(Node* node) {
  VisitRR(this, kArm64Int64ToInt32, node);
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Int32ToFloat64, g.DefineAsDoubleRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Uint32ToFloat64, g.DefineAsDoubleRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Float64ToInt32, g.DefineAsRegister(node),
       g.UseDoubleRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Float64ToUint32, g.DefineAsRegister(node),
       g.UseDoubleRegister(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64Add(Node* node) {
  VisitRRRFloat64(this, kArm64Float64Add, node);
}


void InstructionSelector::VisitFloat64Sub(Node* node) {
  VisitRRRFloat64(this, kArm64Float64Sub, node);
}


void InstructionSelector::VisitFloat64Mul(Node* node) {
  VisitRRRFloat64(this, kArm64Float64Mul, node);
}


void InstructionSelector::VisitFloat64Div(Node* node) {
  VisitRRRFloat64(this, kArm64Float64Div, node);
}


void InstructionSelector::VisitFloat64Mod(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Float64Mod, g.DefineAsFixedDouble(node, d0),
       g.UseFixedDouble(node->InputAt(0), d0),
       g.UseFixedDouble(node->InputAt(1), d1))->MarkAsCall();
}


void InstructionSelector::VisitInt32AddWithOverflow(Node* node,
                                                    FlagsContinuation* cont) {
  VisitBinop(this, node, kArm64Add32, kArithimeticImm, cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node,
                                                    FlagsContinuation* cont) {
  VisitBinop(this, node, kArm64Sub32, kArithimeticImm, cont);
}


// Shared routine for multiple compare operations.
static void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                         InstructionOperand* left, InstructionOperand* right,
                         FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
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
  Arm64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  // Match immediates on left or right side of comparison.
  if (g.CanBeImmediate(right, kArithimeticImm)) {
    VisitCompare(selector, opcode, g.UseRegister(left), g.UseImmediate(right),
                 cont);
  } else if (g.CanBeImmediate(left, kArithimeticImm)) {
    if (!commutative) cont->Commute();
    VisitCompare(selector, opcode, g.UseRegister(right), g.UseImmediate(left),
                 cont);
  } else {
    VisitCompare(selector, opcode, g.UseRegister(left), g.UseRegister(right),
                 cont);
  }
}


void InstructionSelector::VisitWord32Test(Node* node, FlagsContinuation* cont) {
  switch (node->opcode()) {
    case IrOpcode::kWord32And:
      return VisitWordCompare(this, node, kArm64Tst32, cont, true);
    default:
      break;
  }

  Arm64OperandGenerator g(this);
  VisitCompare(this, kArm64Tst32, g.UseRegister(node), g.UseRegister(node),
               cont);
}


void InstructionSelector::VisitWord64Test(Node* node, FlagsContinuation* cont) {
  switch (node->opcode()) {
    case IrOpcode::kWord64And:
      return VisitWordCompare(this, node, kArm64Tst, cont, true);
    default:
      break;
  }

  Arm64OperandGenerator g(this);
  VisitCompare(this, kArm64Tst, g.UseRegister(node), g.UseRegister(node), cont);
}


void InstructionSelector::VisitWord32Compare(Node* node,
                                             FlagsContinuation* cont) {
  VisitWordCompare(this, node, kArm64Cmp32, cont, false);
}


void InstructionSelector::VisitWord64Compare(Node* node,
                                             FlagsContinuation* cont) {
  VisitWordCompare(this, node, kArm64Cmp, cont, false);
}


void InstructionSelector::VisitFloat64Compare(Node* node,
                                              FlagsContinuation* cont) {
  Arm64OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  VisitCompare(this, kArm64Float64Cmp, g.UseDoubleRegister(left),
               g.UseDoubleRegister(right), cont);
}


void InstructionSelector::VisitCall(Node* call, BasicBlock* continuation,
                                    BasicBlock* deoptimization) {
  Arm64OperandGenerator g(this);
  CallDescriptor* descriptor = OpParameter<CallDescriptor*>(call);
  CallBuffer buffer(zone(), descriptor);  // TODO(turbofan): temp zone here?

  // Compute InstructionOperands for inputs and outputs.
  // TODO(turbofan): on ARM64 it's probably better to use the code object in a
  // register if there are multiple uses of it. Improve constant pool and the
  // heuristics in the register allocator for where to emit constants.
  InitializeCallBuffer(call, &buffer, true, false, continuation,
                       deoptimization);

  // Push the arguments to the stack.
  bool is_c_frame = descriptor->kind() == CallDescriptor::kCallAddress;
  bool pushed_count_uneven = buffer.pushed_count & 1;
  int aligned_push_count = buffer.pushed_count;
  if (is_c_frame && pushed_count_uneven) {
    aligned_push_count++;
  }
  // TODO(dcarney): claim and poke probably take small immediates,
  //                loop here or whatever.
  // Bump the stack pointer(s).
  if (aligned_push_count > 0) {
    // TODO(dcarney): it would be better to bump the csp here only
    //                and emit paired stores with increment for non c frames.
    Emit(kArm64Claim | MiscField::encode(aligned_push_count), NULL);
  }
  // Move arguments to the stack.
  {
    int slot = buffer.pushed_count - 1;
    // Emit the uneven pushes.
    if (pushed_count_uneven) {
      Node* input = buffer.pushed_nodes[slot];
      ArchOpcode opcode = is_c_frame ? kArm64PokePairZero : kArm64Poke;
      Emit(opcode | MiscField::encode(slot), NULL, g.UseRegister(input));
      slot--;
    }
    // Now all pushes can be done in pairs.
    for (; slot >= 0; slot -= 2) {
      Emit(kArm64PokePair | MiscField::encode(slot), NULL,
           g.UseRegister(buffer.pushed_nodes[slot]),
           g.UseRegister(buffer.pushed_nodes[slot - 1]));
    }
  }

  // Select the appropriate opcode based on the call type.
  InstructionCode opcode;
  switch (descriptor->kind()) {
    case CallDescriptor::kCallCodeObject: {
      bool lazy_deopt = descriptor->CanLazilyDeoptimize();
      opcode = kArm64CallCodeObject | MiscField::encode(lazy_deopt ? 1 : 0);
      break;
    }
    case CallDescriptor::kCallAddress:
      opcode = kArm64CallAddress;
      break;
    case CallDescriptor::kCallJSFunction:
      opcode = kArm64CallJSFunction;
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
  if (is_c_frame && aligned_push_count > 0) {
    DCHECK(deoptimization == NULL && continuation == NULL);
    Emit(kArm64Drop | MiscField::encode(aligned_push_count), NULL);
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
