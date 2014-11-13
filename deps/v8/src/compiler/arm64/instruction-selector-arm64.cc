// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"

namespace v8 {
namespace internal {
namespace compiler {

enum ImmediateMode {
  kArithmeticImm,  // 12 bit unsigned immediate shifted left 0 or 12 bits
  kShift32Imm,     // 0 - 31
  kShift64Imm,     // 0 - 63
  kLogical32Imm,
  kLogical64Imm,
  kLoadStoreImm8,   // signed 8 bit or 12 bit unsigned scaled by access size
  kLoadStoreImm16,
  kLoadStoreImm32,
  kLoadStoreImm64,
  kNoImmediate
};


// Adds Arm64-specific methods for generating operands.
class Arm64OperandGenerator FINAL : public OperandGenerator {
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
    if (node->opcode() == IrOpcode::kInt32Constant)
      value = OpParameter<int32_t>(node);
    else if (node->opcode() == IrOpcode::kInt64Constant)
      value = OpParameter<int64_t>(node);
    else
      return false;
    return CanBeImmediate(value, mode);
  }

  bool CanBeImmediate(int64_t value, ImmediateMode mode) {
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
      case kArithmeticImm:
        return Assembler::IsImmAddSub(value);
      case kShift32Imm:
        return 0 <= value && value < 32;
      case kShift64Imm:
        return 0 <= value && value < 64;
      case kLoadStoreImm8:
        return IsLoadStoreImmediate(value, LSByte);
      case kLoadStoreImm16:
        return IsLoadStoreImmediate(value, LSHalfword);
      case kLoadStoreImm32:
        return IsLoadStoreImmediate(value, LSWord);
      case kLoadStoreImm64:
        return IsLoadStoreImmediate(value, LSDoubleWord);
      case kNoImmediate:
        return false;
    }
    return false;
  }

 private:
  bool IsLoadStoreImmediate(int64_t value, LSDataSize size) {
    return Assembler::IsImmLSScaled(value, size) ||
           Assembler::IsImmLSUnscaled(value);
  }
};


static void VisitRRFloat64(InstructionSelector* selector, ArchOpcode opcode,
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
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
}


static void VisitRRO(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node, ImmediateMode operand_mode) {
  Arm64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseOperand(node->InputAt(1), operand_mode));
}


template <typename Matcher>
static bool TryMatchShift(InstructionSelector* selector, Node* node,
                          InstructionCode* opcode, IrOpcode::Value shift_opcode,
                          ImmediateMode imm_mode,
                          AddressingMode addressing_mode) {
  if (node->opcode() != shift_opcode) return false;
  Arm64OperandGenerator g(selector);
  Matcher m(node);
  if (g.CanBeImmediate(m.right().node(), imm_mode)) {
    *opcode |= AddressingModeField::encode(addressing_mode);
    return true;
  }
  return false;
}


static bool TryMatchAnyShift(InstructionSelector* selector, Node* node,
                             InstructionCode* opcode, bool try_ror) {
  return TryMatchShift<Int32BinopMatcher>(selector, node, opcode,
                                          IrOpcode::kWord32Shl, kShift32Imm,
                                          kMode_Operand2_R_LSL_I) ||
         TryMatchShift<Int32BinopMatcher>(selector, node, opcode,
                                          IrOpcode::kWord32Shr, kShift32Imm,
                                          kMode_Operand2_R_LSR_I) ||
         TryMatchShift<Int32BinopMatcher>(selector, node, opcode,
                                          IrOpcode::kWord32Sar, kShift32Imm,
                                          kMode_Operand2_R_ASR_I) ||
         (try_ror && TryMatchShift<Int32BinopMatcher>(
                         selector, node, opcode, IrOpcode::kWord32Ror,
                         kShift32Imm, kMode_Operand2_R_ROR_I)) ||
         TryMatchShift<Int64BinopMatcher>(selector, node, opcode,
                                          IrOpcode::kWord64Shl, kShift64Imm,
                                          kMode_Operand2_R_LSL_I) ||
         TryMatchShift<Int64BinopMatcher>(selector, node, opcode,
                                          IrOpcode::kWord64Shr, kShift64Imm,
                                          kMode_Operand2_R_LSR_I) ||
         TryMatchShift<Int64BinopMatcher>(selector, node, opcode,
                                          IrOpcode::kWord64Sar, kShift64Imm,
                                          kMode_Operand2_R_ASR_I) ||
         (try_ror && TryMatchShift<Int64BinopMatcher>(
                         selector, node, opcode, IrOpcode::kWord64Ror,
                         kShift64Imm, kMode_Operand2_R_ROR_I));
}


// Shared routine for multiple binary operations.
template <typename Matcher>
static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, ImmediateMode operand_mode,
                       FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
  Matcher m(node);
  InstructionOperand* inputs[4];
  size_t input_count = 0;
  InstructionOperand* outputs[2];
  size_t output_count = 0;
  bool try_ror_operand = true;

  if (m.IsInt32Add() || m.IsInt64Add() || m.IsInt32Sub() || m.IsInt64Sub()) {
    try_ror_operand = false;
  }

  if (g.CanBeImmediate(m.right().node(), operand_mode)) {
    inputs[input_count++] = g.UseRegister(m.left().node());
    inputs[input_count++] = g.UseImmediate(m.right().node());
  } else if (TryMatchAnyShift(selector, m.right().node(), &opcode,
                              try_ror_operand)) {
    Matcher m_shift(m.right().node());
    inputs[input_count++] = g.UseRegister(m.left().node());
    inputs[input_count++] = g.UseRegister(m_shift.left().node());
    inputs[input_count++] = g.UseImmediate(m_shift.right().node());
  } else if (m.HasProperty(Operator::kCommutative) &&
             TryMatchAnyShift(selector, m.left().node(), &opcode,
                              try_ror_operand)) {
    Matcher m_shift(m.left().node());
    inputs[input_count++] = g.UseRegister(m.right().node());
    inputs[input_count++] = g.UseRegister(m_shift.left().node());
    inputs[input_count++] = g.UseImmediate(m_shift.right().node());
  } else {
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

  Instruction* instr = selector->Emit(cont->Encode(opcode), output_count,
                                      outputs, input_count, inputs);
  if (cont->IsBranch()) instr->MarkAsControl();
}


// Shared routine for multiple binary operations.
template <typename Matcher>
static void VisitBinop(InstructionSelector* selector, Node* node,
                       ArchOpcode opcode, ImmediateMode operand_mode) {
  FlagsContinuation cont;
  VisitBinop<Matcher>(selector, node, opcode, operand_mode, &cont);
}


template <typename Matcher>
static void VisitAddSub(InstructionSelector* selector, Node* node,
                        ArchOpcode opcode, ArchOpcode negate_opcode) {
  Arm64OperandGenerator g(selector);
  Matcher m(node);
  if (m.right().HasValue() && (m.right().Value() < 0) &&
      g.CanBeImmediate(-m.right().Value(), kArithmeticImm)) {
    selector->Emit(negate_opcode, g.DefineAsRegister(node),
                   g.UseRegister(m.left().node()),
                   g.TempImmediate(-m.right().Value()));
  } else {
    VisitBinop<Matcher>(selector, node, opcode, kArithmeticImm);
  }
}


void InstructionSelector::VisitLoad(Node* node) {
  MachineType rep = RepresentationOf(OpParameter<LoadRepresentation>(node));
  MachineType typ = TypeOf(OpParameter<LoadRepresentation>(node));
  Arm64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  ArchOpcode opcode;
  ImmediateMode immediate_mode = kNoImmediate;
  switch (rep) {
    case kRepFloat32:
      opcode = kArm64LdrS;
      immediate_mode = kLoadStoreImm32;
      break;
    case kRepFloat64:
      opcode = kArm64LdrD;
      immediate_mode = kLoadStoreImm64;
      break;
    case kRepBit:  // Fall through.
    case kRepWord8:
      opcode = typ == kTypeInt32 ? kArm64Ldrsb : kArm64Ldrb;
      immediate_mode = kLoadStoreImm8;
      break;
    case kRepWord16:
      opcode = typ == kTypeInt32 ? kArm64Ldrsh : kArm64Ldrh;
      immediate_mode = kLoadStoreImm16;
      break;
    case kRepWord32:
      opcode = kArm64LdrW;
      immediate_mode = kLoadStoreImm32;
      break;
    case kRepTagged:  // Fall through.
    case kRepWord64:
      opcode = kArm64Ldr;
      immediate_mode = kLoadStoreImm64;
      break;
    default:
      UNREACHABLE();
      return;
  }
  if (g.CanBeImmediate(index, immediate_mode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseImmediate(index));
  } else {
    Emit(opcode | AddressingModeField::encode(kMode_MRR),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseRegister(index));
  }
}


void InstructionSelector::VisitStore(Node* node) {
  Arm64OperandGenerator g(this);
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
    InstructionOperand* temps[] = {g.TempRegister(x11), g.TempRegister(x12)};
    Emit(kArm64StoreWriteBarrier, NULL, g.UseFixed(base, x10),
         g.UseFixed(index, x11), g.UseFixed(value, x12), arraysize(temps),
         temps);
    return;
  }
  DCHECK_EQ(kNoWriteBarrier, store_rep.write_barrier_kind());
  ArchOpcode opcode;
  ImmediateMode immediate_mode = kNoImmediate;
  switch (rep) {
    case kRepFloat32:
      opcode = kArm64StrS;
      immediate_mode = kLoadStoreImm32;
      break;
    case kRepFloat64:
      opcode = kArm64StrD;
      immediate_mode = kLoadStoreImm64;
      break;
    case kRepBit:  // Fall through.
    case kRepWord8:
      opcode = kArm64Strb;
      immediate_mode = kLoadStoreImm8;
      break;
    case kRepWord16:
      opcode = kArm64Strh;
      immediate_mode = kLoadStoreImm16;
      break;
    case kRepWord32:
      opcode = kArm64StrW;
      immediate_mode = kLoadStoreImm32;
      break;
    case kRepTagged:  // Fall through.
    case kRepWord64:
      opcode = kArm64Str;
      immediate_mode = kLoadStoreImm64;
      break;
    default:
      UNREACHABLE();
      return;
  }
  if (g.CanBeImmediate(index, immediate_mode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), NULL,
         g.UseRegister(base), g.UseImmediate(index), g.UseRegister(value));
  } else {
    Emit(opcode | AddressingModeField::encode(kMode_MRR), NULL,
         g.UseRegister(base), g.UseRegister(index), g.UseRegister(value));
  }
}


template <typename Matcher>
static void VisitLogical(InstructionSelector* selector, Node* node, Matcher* m,
                         ArchOpcode opcode, bool left_can_cover,
                         bool right_can_cover, ImmediateMode imm_mode) {
  Arm64OperandGenerator g(selector);

  // Map instruction to equivalent operation with inverted right input.
  ArchOpcode inv_opcode = opcode;
  switch (opcode) {
    case kArm64And32:
      inv_opcode = kArm64Bic32;
      break;
    case kArm64And:
      inv_opcode = kArm64Bic;
      break;
    case kArm64Or32:
      inv_opcode = kArm64Orn32;
      break;
    case kArm64Or:
      inv_opcode = kArm64Orn;
      break;
    case kArm64Eor32:
      inv_opcode = kArm64Eon32;
      break;
    case kArm64Eor:
      inv_opcode = kArm64Eon;
      break;
    default:
      UNREACHABLE();
  }

  // Select Logical(y, ~x) for Logical(Xor(x, -1), y).
  if ((m->left().IsWord32Xor() || m->left().IsWord64Xor()) && left_can_cover) {
    Matcher mleft(m->left().node());
    if (mleft.right().Is(-1)) {
      // TODO(all): support shifted operand on right.
      selector->Emit(inv_opcode, g.DefineAsRegister(node),
                     g.UseRegister(m->right().node()),
                     g.UseRegister(mleft.left().node()));
      return;
    }
  }

  // Select Logical(x, ~y) for Logical(x, Xor(y, -1)).
  if ((m->right().IsWord32Xor() || m->right().IsWord64Xor()) &&
      right_can_cover) {
    Matcher mright(m->right().node());
    if (mright.right().Is(-1)) {
      // TODO(all): support shifted operand on right.
      selector->Emit(inv_opcode, g.DefineAsRegister(node),
                     g.UseRegister(m->left().node()),
                     g.UseRegister(mright.left().node()));
      return;
    }
  }

  if (m->IsWord32Xor() && m->right().Is(-1)) {
    selector->Emit(kArm64Not32, g.DefineAsRegister(node),
                   g.UseRegister(m->left().node()));
  } else if (m->IsWord64Xor() && m->right().Is(-1)) {
    selector->Emit(kArm64Not, g.DefineAsRegister(node),
                   g.UseRegister(m->left().node()));
  } else {
    VisitBinop<Matcher>(selector, node, opcode, imm_mode);
  }
}


void InstructionSelector::VisitWord32And(Node* node) {
  Arm64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().IsWord32Shr() && CanCover(node, m.left().node()) &&
      m.right().HasValue()) {
    uint32_t mask = m.right().Value();
    uint32_t mask_width = base::bits::CountPopulation32(mask);
    uint32_t mask_msb = base::bits::CountLeadingZeros32(mask);
    if ((mask_width != 0) && (mask_msb + mask_width == 32)) {
      // The mask must be contiguous, and occupy the least-significant bits.
      DCHECK_EQ(0, base::bits::CountTrailingZeros32(mask));

      // Select Ubfx for And(Shr(x, imm), mask) where the mask is in the least
      // significant bits.
      Int32BinopMatcher mleft(m.left().node());
      if (mleft.right().IsInRange(0, 31)) {
        // Ubfx cannot extract bits past the register size, however since
        // shifting the original value would have introduced some zeros we can
        // still use ubfx with a smaller mask and the remaining bits will be
        // zeros.
        uint32_t lsb = mleft.right().Value();
        if (lsb + mask_width > 32) mask_width = 32 - lsb;

        Emit(kArm64Ubfx32, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()),
             g.UseImmediate(mleft.right().node()), g.TempImmediate(mask_width));
        return;
      }
      // Other cases fall through to the normal And operation.
    }
  }
  VisitLogical<Int32BinopMatcher>(
      this, node, &m, kArm64And32, CanCover(node, m.left().node()),
      CanCover(node, m.right().node()), kLogical32Imm);
}


void InstructionSelector::VisitWord64And(Node* node) {
  Arm64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  if (m.left().IsWord64Shr() && CanCover(node, m.left().node()) &&
      m.right().HasValue()) {
    uint64_t mask = m.right().Value();
    uint64_t mask_width = base::bits::CountPopulation64(mask);
    uint64_t mask_msb = base::bits::CountLeadingZeros64(mask);
    if ((mask_width != 0) && (mask_msb + mask_width == 64)) {
      // The mask must be contiguous, and occupy the least-significant bits.
      DCHECK_EQ(0, base::bits::CountTrailingZeros64(mask));

      // Select Ubfx for And(Shr(x, imm), mask) where the mask is in the least
      // significant bits.
      Int64BinopMatcher mleft(m.left().node());
      if (mleft.right().IsInRange(0, 63)) {
        // Ubfx cannot extract bits past the register size, however since
        // shifting the original value would have introduced some zeros we can
        // still use ubfx with a smaller mask and the remaining bits will be
        // zeros.
        uint64_t lsb = mleft.right().Value();
        if (lsb + mask_width > 64) mask_width = 64 - lsb;

        Emit(kArm64Ubfx, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()),
             g.UseImmediate(mleft.right().node()), g.TempImmediate(mask_width));
        return;
      }
      // Other cases fall through to the normal And operation.
    }
  }
  VisitLogical<Int64BinopMatcher>(
      this, node, &m, kArm64And, CanCover(node, m.left().node()),
      CanCover(node, m.right().node()), kLogical64Imm);
}


void InstructionSelector::VisitWord32Or(Node* node) {
  Int32BinopMatcher m(node);
  VisitLogical<Int32BinopMatcher>(
      this, node, &m, kArm64Or32, CanCover(node, m.left().node()),
      CanCover(node, m.right().node()), kLogical32Imm);
}


void InstructionSelector::VisitWord64Or(Node* node) {
  Int64BinopMatcher m(node);
  VisitLogical<Int64BinopMatcher>(
      this, node, &m, kArm64Or, CanCover(node, m.left().node()),
      CanCover(node, m.right().node()), kLogical64Imm);
}


void InstructionSelector::VisitWord32Xor(Node* node) {
  Int32BinopMatcher m(node);
  VisitLogical<Int32BinopMatcher>(
      this, node, &m, kArm64Eor32, CanCover(node, m.left().node()),
      CanCover(node, m.right().node()), kLogical32Imm);
}


void InstructionSelector::VisitWord64Xor(Node* node) {
  Int64BinopMatcher m(node);
  VisitLogical<Int64BinopMatcher>(
      this, node, &m, kArm64Eor, CanCover(node, m.left().node()),
      CanCover(node, m.right().node()), kLogical64Imm);
}


void InstructionSelector::VisitWord32Shl(Node* node) {
  VisitRRO(this, kArm64Lsl32, node, kShift32Imm);
}


void InstructionSelector::VisitWord64Shl(Node* node) {
  VisitRRO(this, kArm64Lsl, node, kShift64Imm);
}


void InstructionSelector::VisitWord32Shr(Node* node) {
  Arm64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().IsWord32And() && m.right().IsInRange(0, 31)) {
    int32_t lsb = m.right().Value();
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      uint32_t mask = (mleft.right().Value() >> lsb) << lsb;
      uint32_t mask_width = base::bits::CountPopulation32(mask);
      uint32_t mask_msb = base::bits::CountLeadingZeros32(mask);
      // Select Ubfx for Shr(And(x, mask), imm) where the result of the mask is
      // shifted into the least-significant bits.
      if ((mask_msb + mask_width + lsb) == 32) {
        DCHECK_EQ(lsb, base::bits::CountTrailingZeros32(mask));
        Emit(kArm64Ubfx32, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(lsb),
             g.TempImmediate(mask_width));
        return;
      }
    }
  }
  VisitRRO(this, kArm64Lsr32, node, kShift32Imm);
}


void InstructionSelector::VisitWord64Shr(Node* node) {
  Arm64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  if (m.left().IsWord64And() && m.right().IsInRange(0, 63)) {
    int64_t lsb = m.right().Value();
    Int64BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      // Select Ubfx for Shr(And(x, mask), imm) where the result of the mask is
      // shifted into the least-significant bits.
      uint64_t mask = (mleft.right().Value() >> lsb) << lsb;
      uint64_t mask_width = base::bits::CountPopulation64(mask);
      uint64_t mask_msb = base::bits::CountLeadingZeros64(mask);
      if ((mask_msb + mask_width + lsb) == 64) {
        DCHECK_EQ(lsb, base::bits::CountTrailingZeros64(mask));
        Emit(kArm64Ubfx, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(lsb),
             g.TempImmediate(mask_width));
        return;
      }
    }
  }
  VisitRRO(this, kArm64Lsr, node, kShift64Imm);
}


void InstructionSelector::VisitWord32Sar(Node* node) {
  VisitRRO(this, kArm64Asr32, node, kShift32Imm);
}


void InstructionSelector::VisitWord64Sar(Node* node) {
  VisitRRO(this, kArm64Asr, node, kShift64Imm);
}


void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitRRO(this, kArm64Ror32, node, kShift32Imm);
}


void InstructionSelector::VisitWord64Ror(Node* node) {
  VisitRRO(this, kArm64Ror, node, kShift64Imm);
}


void InstructionSelector::VisitInt32Add(Node* node) {
  Arm64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  // Select Madd(x, y, z) for Add(Mul(x, y), z).
  if (m.left().IsInt32Mul() && CanCover(node, m.left().node())) {
    Int32BinopMatcher mleft(m.left().node());
    Emit(kArm64Madd32, g.DefineAsRegister(node),
         g.UseRegister(mleft.left().node()),
         g.UseRegister(mleft.right().node()), g.UseRegister(m.right().node()));
    return;
  }
  // Select Madd(x, y, z) for Add(x, Mul(x, y)).
  if (m.right().IsInt32Mul() && CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    Emit(kArm64Madd32, g.DefineAsRegister(node),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()), g.UseRegister(m.left().node()));
    return;
  }
  VisitAddSub<Int32BinopMatcher>(this, node, kArm64Add32, kArm64Sub32);
}


void InstructionSelector::VisitInt64Add(Node* node) {
  Arm64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  // Select Madd(x, y, z) for Add(Mul(x, y), z).
  if (m.left().IsInt64Mul() && CanCover(node, m.left().node())) {
    Int64BinopMatcher mleft(m.left().node());
    Emit(kArm64Madd, g.DefineAsRegister(node),
         g.UseRegister(mleft.left().node()),
         g.UseRegister(mleft.right().node()), g.UseRegister(m.right().node()));
    return;
  }
  // Select Madd(x, y, z) for Add(x, Mul(x, y)).
  if (m.right().IsInt64Mul() && CanCover(node, m.right().node())) {
    Int64BinopMatcher mright(m.right().node());
    Emit(kArm64Madd, g.DefineAsRegister(node),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()), g.UseRegister(m.left().node()));
    return;
  }
  VisitAddSub<Int64BinopMatcher>(this, node, kArm64Add, kArm64Sub);
}


void InstructionSelector::VisitInt32Sub(Node* node) {
  Arm64OperandGenerator g(this);
  Int32BinopMatcher m(node);

  // Select Msub(a, x, y) for Sub(a, Mul(x, y)).
  if (m.right().IsInt32Mul() && CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    Emit(kArm64Msub32, g.DefineAsRegister(node),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()), g.UseRegister(m.left().node()));
    return;
  }

  if (m.left().Is(0)) {
    Emit(kArm64Neg32, g.DefineAsRegister(node),
         g.UseRegister(m.right().node()));
  } else {
    VisitAddSub<Int32BinopMatcher>(this, node, kArm64Sub32, kArm64Add32);
  }
}


void InstructionSelector::VisitInt64Sub(Node* node) {
  Arm64OperandGenerator g(this);
  Int64BinopMatcher m(node);

  // Select Msub(a, x, y) for Sub(a, Mul(x, y)).
  if (m.right().IsInt64Mul() && CanCover(node, m.right().node())) {
    Int64BinopMatcher mright(m.right().node());
    Emit(kArm64Msub, g.DefineAsRegister(node),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()), g.UseRegister(m.left().node()));
    return;
  }

  if (m.left().Is(0)) {
    Emit(kArm64Neg, g.DefineAsRegister(node), g.UseRegister(m.right().node()));
  } else {
    VisitAddSub<Int64BinopMatcher>(this, node, kArm64Sub, kArm64Add);
  }
}


void InstructionSelector::VisitInt32Mul(Node* node) {
  Arm64OperandGenerator g(this);
  Int32BinopMatcher m(node);

  if (m.left().IsInt32Sub() && CanCover(node, m.left().node())) {
    Int32BinopMatcher mleft(m.left().node());

    // Select Mneg(x, y) for Mul(Sub(0, x), y).
    if (mleft.left().Is(0)) {
      Emit(kArm64Mneg32, g.DefineAsRegister(node),
           g.UseRegister(mleft.right().node()),
           g.UseRegister(m.right().node()));
      return;
    }
  }

  if (m.right().IsInt32Sub() && CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());

    // Select Mneg(x, y) for Mul(x, Sub(0, y)).
    if (mright.left().Is(0)) {
      Emit(kArm64Mneg32, g.DefineAsRegister(node),
           g.UseRegister(m.left().node()),
           g.UseRegister(mright.right().node()));
      return;
    }
  }

  VisitRRR(this, kArm64Mul32, node);
}


void InstructionSelector::VisitInt64Mul(Node* node) {
  Arm64OperandGenerator g(this);
  Int64BinopMatcher m(node);

  if (m.left().IsInt64Sub() && CanCover(node, m.left().node())) {
    Int64BinopMatcher mleft(m.left().node());

    // Select Mneg(x, y) for Mul(Sub(0, x), y).
    if (mleft.left().Is(0)) {
      Emit(kArm64Mneg, g.DefineAsRegister(node),
           g.UseRegister(mleft.right().node()),
           g.UseRegister(m.right().node()));
      return;
    }
  }

  if (m.right().IsInt64Sub() && CanCover(node, m.right().node())) {
    Int64BinopMatcher mright(m.right().node());

    // Select Mneg(x, y) for Mul(x, Sub(0, y)).
    if (mright.left().Is(0)) {
      Emit(kArm64Mneg, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.UseRegister(mright.right().node()));
      return;
    }
  }

  VisitRRR(this, kArm64Mul, node);
}


void InstructionSelector::VisitInt32MulHigh(Node* node) {
  // TODO(arm64): Can we do better here?
  Arm64OperandGenerator g(this);
  InstructionOperand* const smull_operand = g.TempRegister();
  Emit(kArm64Smull, smull_operand, g.UseRegister(node->InputAt(0)),
       g.UseRegister(node->InputAt(1)));
  Emit(kArm64Asr, g.DefineAsRegister(node), smull_operand, g.TempImmediate(32));
}


void InstructionSelector::VisitUint32MulHigh(Node* node) {
  // TODO(arm64): Can we do better here?
  Arm64OperandGenerator g(this);
  InstructionOperand* const smull_operand = g.TempRegister();
  Emit(kArm64Umull, smull_operand, g.UseRegister(node->InputAt(0)),
       g.UseRegister(node->InputAt(1)));
  Emit(kArm64Lsr, g.DefineAsRegister(node), smull_operand, g.TempImmediate(32));
}


void InstructionSelector::VisitInt32Div(Node* node) {
  VisitRRR(this, kArm64Idiv32, node);
}


void InstructionSelector::VisitInt64Div(Node* node) {
  VisitRRR(this, kArm64Idiv, node);
}


void InstructionSelector::VisitUint32Div(Node* node) {
  VisitRRR(this, kArm64Udiv32, node);
}


void InstructionSelector::VisitUint64Div(Node* node) {
  VisitRRR(this, kArm64Udiv, node);
}


void InstructionSelector::VisitInt32Mod(Node* node) {
  VisitRRR(this, kArm64Imod32, node);
}


void InstructionSelector::VisitInt64Mod(Node* node) {
  VisitRRR(this, kArm64Imod, node);
}


void InstructionSelector::VisitUint32Mod(Node* node) {
  VisitRRR(this, kArm64Umod32, node);
}


void InstructionSelector::VisitUint64Mod(Node* node) {
  VisitRRR(this, kArm64Umod, node);
}


void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Float32ToFloat64, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Int32ToFloat64, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Uint32ToFloat64, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Float64ToInt32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Float64ToUint32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeInt32ToInt64(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Sxtw, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToUint64(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Mov32, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Float64ToFloat32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Mov32, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
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
  Emit(kArm64Float64Mod, g.DefineAsFixed(node, d0),
       g.UseFixed(node->InputAt(0), d0),
       g.UseFixed(node->InputAt(1), d1))->MarkAsCall();
}


void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  VisitRRFloat64(this, kArm64Float64Sqrt, node);
}


void InstructionSelector::VisitFloat64Floor(Node* node) {
  VisitRRFloat64(this, kArm64Float64Floor, node);
}


void InstructionSelector::VisitFloat64Ceil(Node* node) {
  VisitRRFloat64(this, kArm64Float64Ceil, node);
}


void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  VisitRRFloat64(this, kArm64Float64RoundTruncate, node);
}


void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  VisitRRFloat64(this, kArm64Float64RoundTiesAway, node);
}


void InstructionSelector::VisitCall(Node* node) {
  Arm64OperandGenerator g(this);
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

  // Push the arguments to the stack.
  bool pushed_count_uneven = buffer.pushed_nodes.size() & 1;
  int aligned_push_count = buffer.pushed_nodes.size();
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
    int slot = buffer.pushed_nodes.size() - 1;
    // Emit the uneven pushes.
    if (pushed_count_uneven) {
      Node* input = buffer.pushed_nodes[slot];
      Emit(kArm64Poke | MiscField::encode(slot), NULL, g.UseRegister(input));
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
                             bool commutative, ImmediateMode immediate_mode) {
  Arm64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  // Match immediates on left or right side of comparison.
  if (g.CanBeImmediate(right, immediate_mode)) {
    VisitCompare(selector, opcode, g.UseRegister(left), g.UseImmediate(right),
                 cont);
  } else if (g.CanBeImmediate(left, immediate_mode)) {
    if (!commutative) cont->Commute();
    VisitCompare(selector, opcode, g.UseRegister(right), g.UseImmediate(left),
                 cont);
  } else {
    VisitCompare(selector, opcode, g.UseRegister(left), g.UseRegister(right),
                 cont);
  }
}


static void VisitWord32Compare(InstructionSelector* selector, Node* node,
                               FlagsContinuation* cont) {
  VisitWordCompare(selector, node, kArm64Cmp32, cont, false, kArithmeticImm);
}


static void VisitWordTest(InstructionSelector* selector, Node* node,
                          InstructionCode opcode, FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
  VisitCompare(selector, opcode, g.UseRegister(node), g.UseRegister(node),
               cont);
}


static void VisitWord32Test(InstructionSelector* selector, Node* node,
                            FlagsContinuation* cont) {
  VisitWordTest(selector, node, kArm64Tst32, cont);
}


static void VisitWord64Test(InstructionSelector* selector, Node* node,
                            FlagsContinuation* cont) {
  VisitWordTest(selector, node, kArm64Tst, cont);
}


// Shared routine for multiple float compare operations.
static void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                                FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  VisitCompare(selector, kArm64Float64Cmp, g.UseRegister(left),
               g.UseRegister(right), cont);
}


void InstructionSelector::VisitBranch(Node* branch, BasicBlock* tbranch,
                                      BasicBlock* fbranch) {
  OperandGenerator g(this);
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
        return VisitWord32Compare(this, value, &cont);
      case IrOpcode::kInt32LessThan:
        cont.OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWord32Compare(this, value, &cont);
      case IrOpcode::kInt32LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWord32Compare(this, value, &cont);
      case IrOpcode::kUint32LessThan:
        cont.OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWord32Compare(this, value, &cont);
      case IrOpcode::kUint32LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWord32Compare(this, value, &cont);
      case IrOpcode::kWord64Equal:
        cont.OverwriteAndNegateIfEqual(kEqual);
        return VisitWordCompare(this, value, kArm64Cmp, &cont, false,
                                kArithmeticImm);
      case IrOpcode::kInt64LessThan:
        cont.OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWordCompare(this, value, kArm64Cmp, &cont, false,
                                kArithmeticImm);
      case IrOpcode::kInt64LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWordCompare(this, value, kArm64Cmp, &cont, false,
                                kArithmeticImm);
      case IrOpcode::kUint64LessThan:
        cont.OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWordCompare(this, value, kArm64Cmp, &cont, false,
                                kArithmeticImm);
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
                return VisitBinop<Int32BinopMatcher>(this, node, kArm64Add32,
                                                     kArithmeticImm, &cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont.OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int32BinopMatcher>(this, node, kArm64Sub32,
                                                     kArithmeticImm, &cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Add:
        return VisitWordCompare(this, value, kArm64Cmn32, &cont, true,
                                kArithmeticImm);
      case IrOpcode::kInt32Sub:
        return VisitWordCompare(this, value, kArm64Cmp32, &cont, false,
                                kArithmeticImm);
      case IrOpcode::kWord32And: {
        Int32BinopMatcher m(value);
        if (m.right().HasValue() &&
            (base::bits::CountPopulation32(m.right().Value()) == 1)) {
          // If the mask has only one bit set, we can use tbz/tbnz.
          DCHECK((cont.condition() == kEqual) ||
                 (cont.condition() == kNotEqual));
          ArchOpcode opcode =
              (cont.condition() == kEqual) ? kArm64Tbz32 : kArm64Tbnz32;
          Emit(opcode, NULL, g.UseRegister(m.left().node()),
               g.TempImmediate(
                   base::bits::CountTrailingZeros32(m.right().Value())),
               g.Label(cont.true_block()),
               g.Label(cont.false_block()))->MarkAsControl();
          return;
        }
        return VisitWordCompare(this, value, kArm64Tst32, &cont, true,
                                kLogical32Imm);
      }
      case IrOpcode::kWord64And: {
        Int64BinopMatcher m(value);
        if (m.right().HasValue() &&
            (base::bits::CountPopulation64(m.right().Value()) == 1)) {
          // If the mask has only one bit set, we can use tbz/tbnz.
          DCHECK((cont.condition() == kEqual) ||
                 (cont.condition() == kNotEqual));
          ArchOpcode opcode =
              (cont.condition() == kEqual) ? kArm64Tbz : kArm64Tbnz;
          Emit(opcode, NULL, g.UseRegister(m.left().node()),
               g.TempImmediate(
                   base::bits::CountTrailingZeros64(m.right().Value())),
               g.Label(cont.true_block()),
               g.Label(cont.false_block()))->MarkAsControl();
          return;
        }
        return VisitWordCompare(this, value, kArm64Tst, &cont, true,
                                kLogical64Imm);
      }
      default:
        break;
    }
  }

  // Branch could not be combined with a compare, emit compare against 0.
  VisitWord32Test(this, value, &cont);
}


void InstructionSelector::VisitWord32Equal(Node* const node) {
  Node* const user = node;
  FlagsContinuation cont(kEqual, node);
  Int32BinopMatcher m(user);
  if (m.right().Is(0)) {
    Node* const value = m.left().node();
    if (CanCover(user, value)) {
      switch (value->opcode()) {
        case IrOpcode::kInt32Add:
          return VisitWordCompare(this, value, kArm64Cmn32, &cont, true,
                                  kArithmeticImm);
        case IrOpcode::kInt32Sub:
          return VisitWordCompare(this, value, kArm64Cmp32, &cont, false,
                                  kArithmeticImm);
        case IrOpcode::kWord32And:
          return VisitWordCompare(this, value, kArm64Tst32, &cont, true,
                                  kLogical32Imm);
        default:
          break;
      }
      return VisitWord32Test(this, value, &cont);
    }
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


void InstructionSelector::VisitWord64Equal(Node* const node) {
  Node* const user = node;
  FlagsContinuation cont(kEqual, node);
  Int64BinopMatcher m(user);
  if (m.right().Is(0)) {
    Node* const value = m.left().node();
    if (CanCover(user, value)) {
      switch (value->opcode()) {
        case IrOpcode::kWord64And:
          return VisitWordCompare(this, value, kArm64Tst, &cont, true,
                                  kLogical64Imm);
        default:
          break;
      }
      return VisitWord64Test(this, value, &cont);
    }
  }
  VisitWordCompare(this, node, kArm64Cmp, &cont, false, kArithmeticImm);
}


void InstructionSelector::VisitInt32AddWithOverflow(Node* node) {
  if (Node* ovf = node->FindProjection(1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitBinop<Int32BinopMatcher>(this, node, kArm64Add32,
                                         kArithmeticImm, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int32BinopMatcher>(this, node, kArm64Add32, kArithmeticImm, &cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = node->FindProjection(1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitBinop<Int32BinopMatcher>(this, node, kArm64Sub32,
                                         kArithmeticImm, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int32BinopMatcher>(this, node, kArm64Sub32, kArithmeticImm, &cont);
}


void InstructionSelector::VisitInt64LessThan(Node* node) {
  FlagsContinuation cont(kSignedLessThan, node);
  VisitWordCompare(this, node, kArm64Cmp, &cont, false, kArithmeticImm);
}


void InstructionSelector::VisitInt64LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kArm64Cmp, &cont, false, kArithmeticImm);
}


void InstructionSelector::VisitUint64LessThan(Node* node) {
  FlagsContinuation cont(kUnsignedLessThan, node);
  VisitWordCompare(this, node, kArm64Cmp, &cont, false, kArithmeticImm);
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
         MachineOperatorBuilder::kFloat64RoundTruncate |
         MachineOperatorBuilder::kFloat64RoundTiesAway;
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8
