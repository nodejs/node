// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

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
class Arm64OperandGenerator final : public OperandGenerator {
 public:
  explicit Arm64OperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand UseOperand(Node* node, ImmediateMode mode) {
    if (CanBeImmediate(node, mode)) {
      return UseImmediate(node);
    }
    return UseRegister(node);
  }

  // Use the zero register if the node has the immediate value zero, otherwise
  // assign a register.
  InstructionOperand UseRegisterOrImmediateZero(Node* node) {
    if ((IsIntegerConstant(node) && (GetIntegerConstantValue(node) == 0)) ||
        (IsFloatConstant(node) &&
         (bit_cast<int64_t>(GetFloatConstantValue(node)) == V8_INT64_C(0)))) {
      return UseImmediate(node);
    }
    return UseRegister(node);
  }

  // Use the provided node if it has the required value, or create a
  // TempImmediate otherwise.
  InstructionOperand UseImmediateOrTemp(Node* node, int32_t value) {
    if (GetIntegerConstantValue(node) == value) {
      return UseImmediate(node);
    }
    return TempImmediate(value);
  }

  bool IsIntegerConstant(Node* node) {
    return (node->opcode() == IrOpcode::kInt32Constant) ||
           (node->opcode() == IrOpcode::kInt64Constant);
  }

  int64_t GetIntegerConstantValue(Node* node) {
    if (node->opcode() == IrOpcode::kInt32Constant) {
      return OpParameter<int32_t>(node);
    }
    DCHECK(node->opcode() == IrOpcode::kInt64Constant);
    return OpParameter<int64_t>(node);
  }

  bool IsFloatConstant(Node* node) {
    return (node->opcode() == IrOpcode::kFloat32Constant) ||
           (node->opcode() == IrOpcode::kFloat64Constant);
  }

  double GetFloatConstantValue(Node* node) {
    if (node->opcode() == IrOpcode::kFloat32Constant) {
      return OpParameter<float>(node);
    }
    DCHECK_EQ(IrOpcode::kFloat64Constant, node->opcode());
    return OpParameter<double>(node);
  }

  bool CanBeImmediate(Node* node, ImmediateMode mode) {
    return IsIntegerConstant(node) &&
           CanBeImmediate(GetIntegerConstantValue(node), mode);
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
      case kShift32Imm:  // Fall through.
      case kShift64Imm:
        // Shift operations only observe the bottom 5 or 6 bits of the value.
        // All possible shifts can be encoded by discarding bits which have no
        // effect.
        return true;
    }
    return false;
  }

  bool CanBeLoadStoreShiftImmediate(Node* node, MachineRepresentation rep) {
    // TODO(arm64): Load and Store on 128 bit Q registers is not supported yet.
    DCHECK_NE(MachineRepresentation::kSimd128, rep);
    return IsIntegerConstant(node) &&
           (GetIntegerConstantValue(node) == ElementSizeLog2Of(rep));
  }

 private:
  bool IsLoadStoreImmediate(int64_t value, LSDataSize size) {
    return Assembler::IsImmLSScaled(value, size) ||
           Assembler::IsImmLSUnscaled(value);
  }
};


namespace {

void VisitRR(InstructionSelector* selector, ArchOpcode opcode, Node* node) {
  Arm64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}


void VisitRRR(InstructionSelector* selector, ArchOpcode opcode, Node* node) {
  Arm64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
}


void VisitRRO(InstructionSelector* selector, ArchOpcode opcode, Node* node,
              ImmediateMode operand_mode) {
  Arm64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseOperand(node->InputAt(1), operand_mode));
}

struct ExtendingLoadMatcher {
  ExtendingLoadMatcher(Node* node, InstructionSelector* selector)
      : matches_(false), selector_(selector), base_(nullptr), immediate_(0) {
    Initialize(node);
  }

  bool Matches() const { return matches_; }

  Node* base() const {
    DCHECK(Matches());
    return base_;
  }
  int64_t immediate() const {
    DCHECK(Matches());
    return immediate_;
  }
  ArchOpcode opcode() const {
    DCHECK(Matches());
    return opcode_;
  }

 private:
  bool matches_;
  InstructionSelector* selector_;
  Node* base_;
  int64_t immediate_;
  ArchOpcode opcode_;

  void Initialize(Node* node) {
    Int64BinopMatcher m(node);
    // When loading a 64-bit value and shifting by 32, we should
    // just load and sign-extend the interesting 4 bytes instead.
    // This happens, for example, when we're loading and untagging SMIs.
    DCHECK(m.IsWord64Sar());
    if (m.left().IsLoad() && m.right().Is(32) &&
        selector_->CanCover(m.node(), m.left().node())) {
      Arm64OperandGenerator g(selector_);
      Node* load = m.left().node();
      Node* offset = load->InputAt(1);
      base_ = load->InputAt(0);
      opcode_ = kArm64Ldrsw;
      if (g.IsIntegerConstant(offset)) {
        immediate_ = g.GetIntegerConstantValue(offset) + 4;
        matches_ = g.CanBeImmediate(immediate_, kLoadStoreImm32);
      }
    }
  }
};

bool TryMatchExtendingLoad(InstructionSelector* selector, Node* node) {
  ExtendingLoadMatcher m(node, selector);
  return m.Matches();
}

bool TryEmitExtendingLoad(InstructionSelector* selector, Node* node) {
  ExtendingLoadMatcher m(node, selector);
  Arm64OperandGenerator g(selector);
  if (m.Matches()) {
    InstructionOperand inputs[2];
    inputs[0] = g.UseRegister(m.base());
    InstructionCode opcode =
        m.opcode() | AddressingModeField::encode(kMode_MRI);
    DCHECK(is_int32(m.immediate()));
    inputs[1] = g.TempImmediate(static_cast<int32_t>(m.immediate()));
    InstructionOperand outputs[] = {g.DefineAsRegister(node)};
    selector->Emit(opcode, arraysize(outputs), outputs, arraysize(inputs),
                   inputs);
    return true;
  }
  return false;
}

bool TryMatchAnyShift(InstructionSelector* selector, Node* node,
                      Node* input_node, InstructionCode* opcode, bool try_ror) {
  Arm64OperandGenerator g(selector);

  if (!selector->CanCover(node, input_node)) return false;
  if (input_node->InputCount() != 2) return false;
  if (!g.IsIntegerConstant(input_node->InputAt(1))) return false;

  switch (input_node->opcode()) {
    case IrOpcode::kWord32Shl:
    case IrOpcode::kWord64Shl:
      *opcode |= AddressingModeField::encode(kMode_Operand2_R_LSL_I);
      return true;
    case IrOpcode::kWord32Shr:
    case IrOpcode::kWord64Shr:
      *opcode |= AddressingModeField::encode(kMode_Operand2_R_LSR_I);
      return true;
    case IrOpcode::kWord32Sar:
      *opcode |= AddressingModeField::encode(kMode_Operand2_R_ASR_I);
      return true;
    case IrOpcode::kWord64Sar:
      if (TryMatchExtendingLoad(selector, input_node)) return false;
      *opcode |= AddressingModeField::encode(kMode_Operand2_R_ASR_I);
      return true;
    case IrOpcode::kWord32Ror:
    case IrOpcode::kWord64Ror:
      if (try_ror) {
        *opcode |= AddressingModeField::encode(kMode_Operand2_R_ROR_I);
        return true;
      }
      return false;
    default:
      return false;
  }
}


bool TryMatchAnyExtend(Arm64OperandGenerator* g, InstructionSelector* selector,
                       Node* node, Node* left_node, Node* right_node,
                       InstructionOperand* left_op,
                       InstructionOperand* right_op, InstructionCode* opcode) {
  if (!selector->CanCover(node, right_node)) return false;

  NodeMatcher nm(right_node);

  if (nm.IsWord32And()) {
    Int32BinopMatcher mright(right_node);
    if (mright.right().Is(0xff) || mright.right().Is(0xffff)) {
      int32_t mask = mright.right().Value();
      *left_op = g->UseRegister(left_node);
      *right_op = g->UseRegister(mright.left().node());
      *opcode |= AddressingModeField::encode(
          (mask == 0xff) ? kMode_Operand2_R_UXTB : kMode_Operand2_R_UXTH);
      return true;
    }
  } else if (nm.IsWord32Sar()) {
    Int32BinopMatcher mright(right_node);
    if (selector->CanCover(mright.node(), mright.left().node()) &&
        mright.left().IsWord32Shl()) {
      Int32BinopMatcher mleft_of_right(mright.left().node());
      if ((mright.right().Is(16) && mleft_of_right.right().Is(16)) ||
          (mright.right().Is(24) && mleft_of_right.right().Is(24))) {
        int32_t shift = mright.right().Value();
        *left_op = g->UseRegister(left_node);
        *right_op = g->UseRegister(mleft_of_right.left().node());
        *opcode |= AddressingModeField::encode(
            (shift == 24) ? kMode_Operand2_R_SXTB : kMode_Operand2_R_SXTH);
        return true;
      }
    }
  }
  return false;
}

bool TryMatchLoadStoreShift(Arm64OperandGenerator* g,
                            InstructionSelector* selector,
                            MachineRepresentation rep, Node* node, Node* index,
                            InstructionOperand* index_op,
                            InstructionOperand* shift_immediate_op) {
  if (!selector->CanCover(node, index)) return false;
  if (index->InputCount() != 2) return false;
  Node* left = index->InputAt(0);
  Node* right = index->InputAt(1);
  switch (index->opcode()) {
    case IrOpcode::kWord32Shl:
    case IrOpcode::kWord64Shl:
      if (!g->CanBeLoadStoreShiftImmediate(right, rep)) {
        return false;
      }
      *index_op = g->UseRegister(left);
      *shift_immediate_op = g->UseImmediate(right);
      return true;
    default:
      return false;
  }
}

// Bitfields describing binary operator properties:
// CanCommuteField is true if we can switch the two operands, potentially
// requiring commuting the flags continuation condition.
typedef BitField8<bool, 1, 1> CanCommuteField;
// MustCommuteCondField is true when we need to commute the flags continuation
// condition in order to switch the operands.
typedef BitField8<bool, 2, 1> MustCommuteCondField;
// IsComparisonField is true when the operation is a comparison and has no other
// result other than the condition.
typedef BitField8<bool, 3, 1> IsComparisonField;
// IsAddSubField is true when an instruction is encoded as ADD or SUB.
typedef BitField8<bool, 4, 1> IsAddSubField;

// Get properties of a binary operator.
uint8_t GetBinopProperties(InstructionCode opcode) {
  uint8_t result = 0;
  switch (opcode) {
    case kArm64Cmp32:
    case kArm64Cmp:
      // We can commute CMP by switching the inputs and commuting
      // the flags continuation.
      result = CanCommuteField::update(result, true);
      result = MustCommuteCondField::update(result, true);
      result = IsComparisonField::update(result, true);
      // The CMP and CMN instructions are encoded as SUB or ADD
      // with zero output register, and therefore support the same
      // operand modes.
      result = IsAddSubField::update(result, true);
      break;
    case kArm64Cmn32:
    case kArm64Cmn:
      result = CanCommuteField::update(result, true);
      result = IsComparisonField::update(result, true);
      result = IsAddSubField::update(result, true);
      break;
    case kArm64Add32:
    case kArm64Add:
      result = CanCommuteField::update(result, true);
      result = IsAddSubField::update(result, true);
      break;
    case kArm64Sub32:
    case kArm64Sub:
      result = IsAddSubField::update(result, true);
      break;
    case kArm64Tst32:
    case kArm64Tst:
      result = CanCommuteField::update(result, true);
      result = IsComparisonField::update(result, true);
      break;
    case kArm64And32:
    case kArm64And:
    case kArm64Or32:
    case kArm64Or:
    case kArm64Eor32:
    case kArm64Eor:
      result = CanCommuteField::update(result, true);
      break;
    default:
      UNREACHABLE();
      return 0;
  }
  DCHECK_IMPLIES(MustCommuteCondField::decode(result),
                 CanCommuteField::decode(result));
  return result;
}

// Shared routine for multiple binary operations.
template <typename Matcher>
void VisitBinop(InstructionSelector* selector, Node* node,
                InstructionCode opcode, ImmediateMode operand_mode,
                FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
  InstructionOperand inputs[5];
  size_t input_count = 0;
  InstructionOperand outputs[2];
  size_t output_count = 0;

  Node* left_node = node->InputAt(0);
  Node* right_node = node->InputAt(1);

  uint8_t properties = GetBinopProperties(opcode);
  bool can_commute = CanCommuteField::decode(properties);
  bool must_commute_cond = MustCommuteCondField::decode(properties);
  bool is_add_sub = IsAddSubField::decode(properties);

  if (g.CanBeImmediate(right_node, operand_mode)) {
    inputs[input_count++] = g.UseRegister(left_node);
    inputs[input_count++] = g.UseImmediate(right_node);
  } else if (can_commute && g.CanBeImmediate(left_node, operand_mode)) {
    if (must_commute_cond) cont->Commute();
    inputs[input_count++] = g.UseRegister(right_node);
    inputs[input_count++] = g.UseImmediate(left_node);
  } else if (is_add_sub &&
             TryMatchAnyExtend(&g, selector, node, left_node, right_node,
                               &inputs[0], &inputs[1], &opcode)) {
    input_count += 2;
  } else if (is_add_sub && can_commute &&
             TryMatchAnyExtend(&g, selector, node, right_node, left_node,
                               &inputs[0], &inputs[1], &opcode)) {
    if (must_commute_cond) cont->Commute();
    input_count += 2;
  } else if (TryMatchAnyShift(selector, node, right_node, &opcode,
                              !is_add_sub)) {
    Matcher m_shift(right_node);
    inputs[input_count++] = g.UseRegisterOrImmediateZero(left_node);
    inputs[input_count++] = g.UseRegister(m_shift.left().node());
    inputs[input_count++] = g.UseImmediate(m_shift.right().node());
  } else if (can_commute && TryMatchAnyShift(selector, node, left_node, &opcode,
                                             !is_add_sub)) {
    if (must_commute_cond) cont->Commute();
    Matcher m_shift(left_node);
    inputs[input_count++] = g.UseRegisterOrImmediateZero(right_node);
    inputs[input_count++] = g.UseRegister(m_shift.left().node());
    inputs[input_count++] = g.UseImmediate(m_shift.right().node());
  } else {
    inputs[input_count++] = g.UseRegisterOrImmediateZero(left_node);
    inputs[input_count++] = g.UseRegister(right_node);
  }

  if (cont->IsBranch()) {
    inputs[input_count++] = g.Label(cont->true_block());
    inputs[input_count++] = g.Label(cont->false_block());
  }

  if (!IsComparisonField::decode(properties)) {
    outputs[output_count++] = g.DefineAsRegister(node);
  }

  if (cont->IsSet()) {
    outputs[output_count++] = g.DefineAsRegister(cont->result());
  }

  DCHECK_NE(0u, input_count);
  DCHECK((output_count != 0) || IsComparisonField::decode(properties));
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


// Shared routine for multiple binary operations.
template <typename Matcher>
void VisitBinop(InstructionSelector* selector, Node* node, ArchOpcode opcode,
                ImmediateMode operand_mode) {
  FlagsContinuation cont;
  VisitBinop<Matcher>(selector, node, opcode, operand_mode, &cont);
}


template <typename Matcher>
void VisitAddSub(InstructionSelector* selector, Node* node, ArchOpcode opcode,
                 ArchOpcode negate_opcode) {
  Arm64OperandGenerator g(selector);
  Matcher m(node);
  if (m.right().HasValue() && (m.right().Value() < 0) &&
      g.CanBeImmediate(-m.right().Value(), kArithmeticImm)) {
    selector->Emit(negate_opcode, g.DefineAsRegister(node),
                   g.UseRegister(m.left().node()),
                   g.TempImmediate(static_cast<int32_t>(-m.right().Value())));
  } else {
    VisitBinop<Matcher>(selector, node, opcode, kArithmeticImm);
  }
}


// For multiplications by immediate of the form x * (2^k + 1), where k > 0,
// return the value of k, otherwise return zero. This is used to reduce the
// multiplication to addition with left shift: x + (x << k).
template <typename Matcher>
int32_t LeftShiftForReducedMultiply(Matcher* m) {
  DCHECK(m->IsInt32Mul() || m->IsInt64Mul());
  if (m->right().HasValue() && m->right().Value() >= 3) {
    uint64_t value_minus_one = m->right().Value() - 1;
    if (base::bits::IsPowerOfTwo64(value_minus_one)) {
      return WhichPowerOf2_64(value_minus_one);
    }
  }
  return 0;
}

}  // namespace

void EmitLoad(InstructionSelector* selector, Node* node, InstructionCode opcode,
              ImmediateMode immediate_mode, MachineRepresentation rep,
              Node* output = nullptr) {
  Arm64OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  InstructionOperand inputs[3];
  size_t input_count = 0;
  InstructionOperand outputs[1];

  // If output is not nullptr, use that as the output register. This
  // is used when we merge a conversion into the load.
  outputs[0] = g.DefineAsRegister(output == nullptr ? node : output);
  inputs[0] = g.UseRegister(base);

  if (g.CanBeImmediate(index, immediate_mode)) {
    input_count = 2;
    inputs[1] = g.UseImmediate(index);
    opcode |= AddressingModeField::encode(kMode_MRI);
  } else if (TryMatchLoadStoreShift(&g, selector, rep, node, index, &inputs[1],
                                    &inputs[2])) {
    input_count = 3;
    opcode |= AddressingModeField::encode(kMode_Operand2_R_LSL_I);
  } else {
    input_count = 2;
    inputs[1] = g.UseRegister(index);
    opcode |= AddressingModeField::encode(kMode_MRR);
  }

  selector->Emit(opcode, arraysize(outputs), outputs, input_count, inputs);
}

void InstructionSelector::VisitLoad(Node* node) {
  InstructionCode opcode = kArchNop;
  ImmediateMode immediate_mode = kNoImmediate;
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  MachineRepresentation rep = load_rep.representation();
  switch (rep) {
    case MachineRepresentation::kFloat32:
      opcode = kArm64LdrS;
      immediate_mode = kLoadStoreImm32;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kArm64LdrD;
      immediate_mode = kLoadStoreImm64;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsSigned() ? kArm64Ldrsb : kArm64Ldrb;
      immediate_mode = kLoadStoreImm8;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsSigned() ? kArm64Ldrsh : kArm64Ldrh;
      immediate_mode = kLoadStoreImm16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kArm64LdrW;
      immediate_mode = kLoadStoreImm32;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:  // Fall through.
    case MachineRepresentation::kWord64:
      opcode = kArm64Ldr;
      immediate_mode = kLoadStoreImm64;
      break;
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }
  EmitLoad(this, node, opcode, immediate_mode, rep);
}

void InstructionSelector::VisitProtectedLoad(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelector::VisitStore(Node* node) {
  Arm64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());
  WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();
  MachineRepresentation rep = store_rep.representation();

  // TODO(arm64): I guess this could be done in a better way.
  if (write_barrier_kind != kNoWriteBarrier) {
    DCHECK(CanBeTaggedPointer(rep));
    AddressingMode addressing_mode;
    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    // OutOfLineRecordWrite uses the index in an arithmetic instruction, so we
    // must check kArithmeticImm as well as kLoadStoreImm64.
    if (g.CanBeImmediate(index, kArithmeticImm) &&
        g.CanBeImmediate(index, kLoadStoreImm64)) {
      inputs[input_count++] = g.UseImmediate(index);
      addressing_mode = kMode_MRI;
    } else {
      inputs[input_count++] = g.UseUniqueRegister(index);
      addressing_mode = kMode_MRR;
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
    InstructionOperand inputs[4];
    size_t input_count = 0;
    InstructionCode opcode = kArchNop;
    ImmediateMode immediate_mode = kNoImmediate;
    switch (rep) {
      case MachineRepresentation::kFloat32:
        opcode = kArm64StrS;
        immediate_mode = kLoadStoreImm32;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kArm64StrD;
        immediate_mode = kLoadStoreImm64;
        break;
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = kArm64Strb;
        immediate_mode = kLoadStoreImm8;
        break;
      case MachineRepresentation::kWord16:
        opcode = kArm64Strh;
        immediate_mode = kLoadStoreImm16;
        break;
      case MachineRepresentation::kWord32:
        opcode = kArm64StrW;
        immediate_mode = kLoadStoreImm32;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:  // Fall through.
      case MachineRepresentation::kWord64:
        opcode = kArm64Str;
        immediate_mode = kLoadStoreImm64;
        break;
      case MachineRepresentation::kSimd128:  // Fall through.
      case MachineRepresentation::kNone:
        UNREACHABLE();
        return;
    }

    inputs[0] = g.UseRegisterOrImmediateZero(value);
    inputs[1] = g.UseRegister(base);

    if (g.CanBeImmediate(index, immediate_mode)) {
      input_count = 3;
      inputs[2] = g.UseImmediate(index);
      opcode |= AddressingModeField::encode(kMode_MRI);
    } else if (TryMatchLoadStoreShift(&g, this, rep, node, index, &inputs[2],
                                      &inputs[3])) {
      input_count = 4;
      opcode |= AddressingModeField::encode(kMode_Operand2_R_LSL_I);
    } else {
      input_count = 3;
      inputs[2] = g.UseRegister(index);
      opcode |= AddressingModeField::encode(kMode_MRR);
    }

    Emit(opcode, 0, nullptr, input_count, inputs);
  }
}

void InstructionSelector::VisitProtectedStore(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

// Architecture supports unaligned access, therefore VisitLoad is used instead
void InstructionSelector::VisitUnalignedLoad(Node* node) { UNREACHABLE(); }

// Architecture supports unaligned access, therefore VisitStore is used instead
void InstructionSelector::VisitUnalignedStore(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitCheckedLoad(Node* node) {
  CheckedLoadRepresentation load_rep = CheckedLoadRepresentationOf(node->op());
  Arm64OperandGenerator g(this);
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
    case MachineRepresentation::kWord64:
      opcode = kCheckedLoadWord64;
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
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }
  // If the length is a constant power of two, allow the code generator to
  // pick a more efficient bounds check sequence by passing the length as an
  // immediate.
  if (length->opcode() == IrOpcode::kInt32Constant) {
    Int32Matcher m(length);
    if (m.IsPowerOf2()) {
      Emit(opcode, g.DefineAsRegister(node), g.UseRegister(buffer),
           g.UseRegister(offset), g.UseImmediate(length));
      return;
    }
  }
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(buffer),
       g.UseRegister(offset), g.UseOperand(length, kArithmeticImm));
}


void InstructionSelector::VisitCheckedStore(Node* node) {
  MachineRepresentation rep = CheckedStoreRepresentationOf(node->op());
  Arm64OperandGenerator g(this);
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
    case MachineRepresentation::kWord64:
      opcode = kCheckedStoreWord64;
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
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }
  // If the length is a constant power of two, allow the code generator to
  // pick a more efficient bounds check sequence by passing the length as an
  // immediate.
  if (length->opcode() == IrOpcode::kInt32Constant) {
    Int32Matcher m(length);
    if (m.IsPowerOf2()) {
      Emit(opcode, g.NoOutput(), g.UseRegister(buffer), g.UseRegister(offset),
           g.UseImmediate(length), g.UseRegisterOrImmediateZero(value));
      return;
    }
  }
  Emit(opcode, g.NoOutput(), g.UseRegister(buffer), g.UseRegister(offset),
       g.UseOperand(length, kArithmeticImm),
       g.UseRegisterOrImmediateZero(value));
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
      DCHECK_EQ(0u, base::bits::CountTrailingZeros32(mask));

      // Select Ubfx for And(Shr(x, imm), mask) where the mask is in the least
      // significant bits.
      Int32BinopMatcher mleft(m.left().node());
      if (mleft.right().HasValue()) {
        // Any shift value can match; int32 shifts use `value % 32`.
        uint32_t lsb = mleft.right().Value() & 0x1f;

        // Ubfx cannot extract bits past the register size, however since
        // shifting the original value would have introduced some zeros we can
        // still use ubfx with a smaller mask and the remaining bits will be
        // zeros.
        if (lsb + mask_width > 32) mask_width = 32 - lsb;

        Emit(kArm64Ubfx32, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()),
             g.UseImmediateOrTemp(mleft.right().node(), lsb),
             g.TempImmediate(mask_width));
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
      DCHECK_EQ(0u, base::bits::CountTrailingZeros64(mask));

      // Select Ubfx for And(Shr(x, imm), mask) where the mask is in the least
      // significant bits.
      Int64BinopMatcher mleft(m.left().node());
      if (mleft.right().HasValue()) {
        // Any shift value can match; int64 shifts use `value % 64`.
        uint32_t lsb = static_cast<uint32_t>(mleft.right().Value() & 0x3f);

        // Ubfx cannot extract bits past the register size, however since
        // shifting the original value would have introduced some zeros we can
        // still use ubfx with a smaller mask and the remaining bits will be
        // zeros.
        if (lsb + mask_width > 64) mask_width = 64 - lsb;

        Emit(kArm64Ubfx, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()),
             g.UseImmediateOrTemp(mleft.right().node(), lsb),
             g.TempImmediate(static_cast<int32_t>(mask_width)));
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
  Int32BinopMatcher m(node);
  if (m.left().IsWord32And() && CanCover(node, m.left().node()) &&
      m.right().IsInRange(1, 31)) {
    Arm64OperandGenerator g(this);
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      uint32_t mask = mleft.right().Value();
      uint32_t mask_width = base::bits::CountPopulation32(mask);
      uint32_t mask_msb = base::bits::CountLeadingZeros32(mask);
      if ((mask_width != 0) && (mask_msb + mask_width == 32)) {
        uint32_t shift = m.right().Value();
        DCHECK_EQ(0u, base::bits::CountTrailingZeros32(mask));
        DCHECK_NE(0u, shift);

        if ((shift + mask_width) >= 32) {
          // If the mask is contiguous and reaches or extends beyond the top
          // bit, only the shift is needed.
          Emit(kArm64Lsl32, g.DefineAsRegister(node),
               g.UseRegister(mleft.left().node()),
               g.UseImmediate(m.right().node()));
          return;
        } else {
          // Select Ubfiz for Shl(And(x, mask), imm) where the mask is
          // contiguous, and the shift immediate non-zero.
          Emit(kArm64Ubfiz32, g.DefineAsRegister(node),
               g.UseRegister(mleft.left().node()),
               g.UseImmediate(m.right().node()), g.TempImmediate(mask_width));
          return;
        }
      }
    }
  }
  VisitRRO(this, kArm64Lsl32, node, kShift32Imm);
}


void InstructionSelector::VisitWord64Shl(Node* node) {
  Arm64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  if ((m.left().IsChangeInt32ToInt64() || m.left().IsChangeUint32ToUint64()) &&
      m.right().IsInRange(32, 63) && CanCover(node, m.left().node())) {
    // There's no need to sign/zero-extend to 64-bit if we shift out the upper
    // 32 bits anyway.
    Emit(kArm64Lsl, g.DefineAsRegister(node),
         g.UseRegister(m.left().node()->InputAt(0)),
         g.UseImmediate(m.right().node()));
    return;
  }
  VisitRRO(this, kArm64Lsl, node, kShift64Imm);
}


namespace {

bool TryEmitBitfieldExtract32(InstructionSelector* selector, Node* node) {
  Arm64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  if (selector->CanCover(node, m.left().node()) && m.left().IsWord32Shl()) {
    // Select Ubfx or Sbfx for (x << (K & 0x1f)) OP (K & 0x1f), where
    // OP is >>> or >> and (K & 0x1f) != 0.
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue() && m.right().HasValue() &&
        (mleft.right().Value() & 0x1f) != 0 &&
        (mleft.right().Value() & 0x1f) == (m.right().Value() & 0x1f)) {
      DCHECK(m.IsWord32Shr() || m.IsWord32Sar());
      ArchOpcode opcode = m.IsWord32Sar() ? kArm64Sbfx32 : kArm64Ubfx32;

      int right_val = m.right().Value() & 0x1f;
      DCHECK_NE(right_val, 0);

      selector->Emit(opcode, g.DefineAsRegister(node),
                     g.UseRegister(mleft.left().node()), g.TempImmediate(0),
                     g.TempImmediate(32 - right_val));
      return true;
    }
  }
  return false;
}

}  // namespace


void InstructionSelector::VisitWord32Shr(Node* node) {
  Int32BinopMatcher m(node);
  if (m.left().IsWord32And() && m.right().HasValue()) {
    uint32_t lsb = m.right().Value() & 0x1f;
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      // Select Ubfx for Shr(And(x, mask), imm) where the result of the mask is
      // shifted into the least-significant bits.
      uint32_t mask = (mleft.right().Value() >> lsb) << lsb;
      unsigned mask_width = base::bits::CountPopulation32(mask);
      unsigned mask_msb = base::bits::CountLeadingZeros32(mask);
      if ((mask_msb + mask_width + lsb) == 32) {
        Arm64OperandGenerator g(this);
        DCHECK_EQ(lsb, base::bits::CountTrailingZeros32(mask));
        Emit(kArm64Ubfx32, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()),
             g.UseImmediateOrTemp(m.right().node(), lsb),
             g.TempImmediate(mask_width));
        return;
      }
    }
  } else if (TryEmitBitfieldExtract32(this, node)) {
    return;
  }

  if (m.left().IsUint32MulHigh() && m.right().HasValue() &&
      CanCover(node, node->InputAt(0))) {
    // Combine this shift with the multiply and shift that would be generated
    // by Uint32MulHigh.
    Arm64OperandGenerator g(this);
    Node* left = m.left().node();
    int shift = m.right().Value() & 0x1f;
    InstructionOperand const smull_operand = g.TempRegister();
    Emit(kArm64Umull, smull_operand, g.UseRegister(left->InputAt(0)),
         g.UseRegister(left->InputAt(1)));
    Emit(kArm64Lsr, g.DefineAsRegister(node), smull_operand,
         g.TempImmediate(32 + shift));
    return;
  }

  VisitRRO(this, kArm64Lsr32, node, kShift32Imm);
}


void InstructionSelector::VisitWord64Shr(Node* node) {
  Int64BinopMatcher m(node);
  if (m.left().IsWord64And() && m.right().HasValue()) {
    uint32_t lsb = m.right().Value() & 0x3f;
    Int64BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      // Select Ubfx for Shr(And(x, mask), imm) where the result of the mask is
      // shifted into the least-significant bits.
      uint64_t mask = (mleft.right().Value() >> lsb) << lsb;
      unsigned mask_width = base::bits::CountPopulation64(mask);
      unsigned mask_msb = base::bits::CountLeadingZeros64(mask);
      if ((mask_msb + mask_width + lsb) == 64) {
        Arm64OperandGenerator g(this);
        DCHECK_EQ(lsb, base::bits::CountTrailingZeros64(mask));
        Emit(kArm64Ubfx, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()),
             g.UseImmediateOrTemp(m.right().node(), lsb),
             g.TempImmediate(mask_width));
        return;
      }
    }
  }
  VisitRRO(this, kArm64Lsr, node, kShift64Imm);
}


void InstructionSelector::VisitWord32Sar(Node* node) {
  if (TryEmitBitfieldExtract32(this, node)) {
    return;
  }

  Int32BinopMatcher m(node);
  if (m.left().IsInt32MulHigh() && m.right().HasValue() &&
      CanCover(node, node->InputAt(0))) {
    // Combine this shift with the multiply and shift that would be generated
    // by Int32MulHigh.
    Arm64OperandGenerator g(this);
    Node* left = m.left().node();
    int shift = m.right().Value() & 0x1f;
    InstructionOperand const smull_operand = g.TempRegister();
    Emit(kArm64Smull, smull_operand, g.UseRegister(left->InputAt(0)),
         g.UseRegister(left->InputAt(1)));
    Emit(kArm64Asr, g.DefineAsRegister(node), smull_operand,
         g.TempImmediate(32 + shift));
    return;
  }

  if (m.left().IsInt32Add() && m.right().HasValue() &&
      CanCover(node, node->InputAt(0))) {
    Node* add_node = m.left().node();
    Int32BinopMatcher madd_node(add_node);
    if (madd_node.left().IsInt32MulHigh() &&
        CanCover(add_node, madd_node.left().node())) {
      // Combine the shift that would be generated by Int32MulHigh with the add
      // on the left of this Sar operation. We do it here, as the result of the
      // add potentially has 33 bits, so we have to ensure the result is
      // truncated by being the input to this 32-bit Sar operation.
      Arm64OperandGenerator g(this);
      Node* mul_node = madd_node.left().node();

      InstructionOperand const smull_operand = g.TempRegister();
      Emit(kArm64Smull, smull_operand, g.UseRegister(mul_node->InputAt(0)),
           g.UseRegister(mul_node->InputAt(1)));

      InstructionOperand const add_operand = g.TempRegister();
      Emit(kArm64Add | AddressingModeField::encode(kMode_Operand2_R_ASR_I),
           add_operand, g.UseRegister(add_node->InputAt(1)), smull_operand,
           g.TempImmediate(32));

      Emit(kArm64Asr32, g.DefineAsRegister(node), add_operand,
           g.UseImmediate(node->InputAt(1)));
      return;
    }
  }

  VisitRRO(this, kArm64Asr32, node, kShift32Imm);
}


void InstructionSelector::VisitWord64Sar(Node* node) {
  if (TryEmitExtendingLoad(this, node)) return;
  VisitRRO(this, kArm64Asr, node, kShift64Imm);
}


void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitRRO(this, kArm64Ror32, node, kShift32Imm);
}


void InstructionSelector::VisitWord64Ror(Node* node) {
  VisitRRO(this, kArm64Ror, node, kShift64Imm);
}


void InstructionSelector::VisitWord64Clz(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Clz, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitWord32Clz(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Clz32, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitWord32Ctz(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitWord64Ctz(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitWord32ReverseBits(Node* node) {
  VisitRR(this, kArm64Rbit32, node);
}


void InstructionSelector::VisitWord64ReverseBits(Node* node) {
  VisitRR(this, kArm64Rbit, node);
}

void InstructionSelector::VisitWord64ReverseBytes(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord32ReverseBytes(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord32Popcnt(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitWord64Popcnt(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitInt32Add(Node* node) {
  Arm64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  // Select Madd(x, y, z) for Add(Mul(x, y), z).
  if (m.left().IsInt32Mul() && CanCover(node, m.left().node())) {
    Int32BinopMatcher mleft(m.left().node());
    // Check multiply can't be later reduced to addition with shift.
    if (LeftShiftForReducedMultiply(&mleft) == 0) {
      Emit(kArm64Madd32, g.DefineAsRegister(node),
           g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()),
           g.UseRegister(m.right().node()));
      return;
    }
  }
  // Select Madd(x, y, z) for Add(z, Mul(x, y)).
  if (m.right().IsInt32Mul() && CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    // Check multiply can't be later reduced to addition with shift.
    if (LeftShiftForReducedMultiply(&mright) == 0) {
      Emit(kArm64Madd32, g.DefineAsRegister(node),
           g.UseRegister(mright.left().node()),
           g.UseRegister(mright.right().node()),
           g.UseRegister(m.left().node()));
      return;
    }
  }
  VisitAddSub<Int32BinopMatcher>(this, node, kArm64Add32, kArm64Sub32);
}


void InstructionSelector::VisitInt64Add(Node* node) {
  Arm64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  // Select Madd(x, y, z) for Add(Mul(x, y), z).
  if (m.left().IsInt64Mul() && CanCover(node, m.left().node())) {
    Int64BinopMatcher mleft(m.left().node());
    // Check multiply can't be later reduced to addition with shift.
    if (LeftShiftForReducedMultiply(&mleft) == 0) {
      Emit(kArm64Madd, g.DefineAsRegister(node),
           g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()),
           g.UseRegister(m.right().node()));
      return;
    }
  }
  // Select Madd(x, y, z) for Add(z, Mul(x, y)).
  if (m.right().IsInt64Mul() && CanCover(node, m.right().node())) {
    Int64BinopMatcher mright(m.right().node());
    // Check multiply can't be later reduced to addition with shift.
    if (LeftShiftForReducedMultiply(&mright) == 0) {
      Emit(kArm64Madd, g.DefineAsRegister(node),
           g.UseRegister(mright.left().node()),
           g.UseRegister(mright.right().node()),
           g.UseRegister(m.left().node()));
      return;
    }
  }
  VisitAddSub<Int64BinopMatcher>(this, node, kArm64Add, kArm64Sub);
}


void InstructionSelector::VisitInt32Sub(Node* node) {
  Arm64OperandGenerator g(this);
  Int32BinopMatcher m(node);

  // Select Msub(x, y, a) for Sub(a, Mul(x, y)).
  if (m.right().IsInt32Mul() && CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    // Check multiply can't be later reduced to addition with shift.
    if (LeftShiftForReducedMultiply(&mright) == 0) {
      Emit(kArm64Msub32, g.DefineAsRegister(node),
           g.UseRegister(mright.left().node()),
           g.UseRegister(mright.right().node()),
           g.UseRegister(m.left().node()));
      return;
    }
  }

  VisitAddSub<Int32BinopMatcher>(this, node, kArm64Sub32, kArm64Add32);
}


void InstructionSelector::VisitInt64Sub(Node* node) {
  Arm64OperandGenerator g(this);
  Int64BinopMatcher m(node);

  // Select Msub(x, y, a) for Sub(a, Mul(x, y)).
  if (m.right().IsInt64Mul() && CanCover(node, m.right().node())) {
    Int64BinopMatcher mright(m.right().node());
    // Check multiply can't be later reduced to addition with shift.
    if (LeftShiftForReducedMultiply(&mright) == 0) {
      Emit(kArm64Msub, g.DefineAsRegister(node),
           g.UseRegister(mright.left().node()),
           g.UseRegister(mright.right().node()),
           g.UseRegister(m.left().node()));
      return;
    }
  }

  VisitAddSub<Int64BinopMatcher>(this, node, kArm64Sub, kArm64Add);
}

namespace {

void EmitInt32MulWithOverflow(InstructionSelector* selector, Node* node,
                              FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand result = g.DefineAsRegister(node);
  InstructionOperand left = g.UseRegister(m.left().node());
  InstructionOperand right = g.UseRegister(m.right().node());
  selector->Emit(kArm64Smull, result, left, right);

  InstructionCode opcode = cont->Encode(kArm64Cmp) |
                           AddressingModeField::encode(kMode_Operand2_R_SXTW);
  if (cont->IsBranch()) {
    selector->Emit(opcode, g.NoOutput(), result, result,
                   g.Label(cont->true_block()), g.Label(cont->false_block()));
  } else if (cont->IsDeoptimize()) {
    InstructionOperand in[] = {result, result};
    selector->EmitDeoptimize(opcode, 0, nullptr, 2, in, cont->reason(),
                             cont->frame_state());
  } else if (cont->IsSet()) {
    selector->Emit(opcode, g.DefineAsRegister(cont->result()), result, result);
  } else {
    DCHECK(cont->IsTrap());
    selector->Emit(opcode, g.NoOutput(), result, result,
                   g.UseImmediate(cont->trap_id()));
  }
}

}  // namespace

void InstructionSelector::VisitInt32Mul(Node* node) {
  Arm64OperandGenerator g(this);
  Int32BinopMatcher m(node);

  // First, try to reduce the multiplication to addition with left shift.
  // x * (2^k + 1) -> x + (x << k)
  int32_t shift = LeftShiftForReducedMultiply(&m);
  if (shift > 0) {
    Emit(kArm64Add32 | AddressingModeField::encode(kMode_Operand2_R_LSL_I),
         g.DefineAsRegister(node), g.UseRegister(m.left().node()),
         g.UseRegister(m.left().node()), g.TempImmediate(shift));
    return;
  }

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

  // First, try to reduce the multiplication to addition with left shift.
  // x * (2^k + 1) -> x + (x << k)
  int32_t shift = LeftShiftForReducedMultiply(&m);
  if (shift > 0) {
    Emit(kArm64Add | AddressingModeField::encode(kMode_Operand2_R_LSL_I),
         g.DefineAsRegister(node), g.UseRegister(m.left().node()),
         g.UseRegister(m.left().node()), g.TempImmediate(shift));
    return;
  }

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
  Arm64OperandGenerator g(this);
  InstructionOperand const smull_operand = g.TempRegister();
  Emit(kArm64Smull, smull_operand, g.UseRegister(node->InputAt(0)),
       g.UseRegister(node->InputAt(1)));
  Emit(kArm64Asr, g.DefineAsRegister(node), smull_operand, g.TempImmediate(32));
}


void InstructionSelector::VisitUint32MulHigh(Node* node) {
  Arm64OperandGenerator g(this);
  InstructionOperand const smull_operand = g.TempRegister();
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
  VisitRR(this, kArm64Float32ToFloat64, node);
}


void InstructionSelector::VisitRoundInt32ToFloat32(Node* node) {
  VisitRR(this, kArm64Int32ToFloat32, node);
}


void InstructionSelector::VisitRoundUint32ToFloat32(Node* node) {
  VisitRR(this, kArm64Uint32ToFloat32, node);
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  VisitRR(this, kArm64Int32ToFloat64, node);
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  VisitRR(this, kArm64Uint32ToFloat64, node);
}


void InstructionSelector::VisitTruncateFloat32ToInt32(Node* node) {
  VisitRR(this, kArm64Float32ToInt32, node);
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  VisitRR(this, kArm64Float64ToInt32, node);
}


void InstructionSelector::VisitTruncateFloat32ToUint32(Node* node) {
  VisitRR(this, kArm64Float32ToUint32, node);
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  VisitRR(this, kArm64Float64ToUint32, node);
}

void InstructionSelector::VisitTruncateFloat64ToUint32(Node* node) {
  VisitRR(this, kArm64Float64ToUint32, node);
}

void InstructionSelector::VisitTryTruncateFloat32ToInt64(Node* node) {
  Arm64OperandGenerator g(this);

  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kArm64Float32ToInt64, output_count, outputs, 1, inputs);
}


void InstructionSelector::VisitTryTruncateFloat64ToInt64(Node* node) {
  Arm64OperandGenerator g(this);

  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kArm64Float64ToInt64, output_count, outputs, 1, inputs);
}


void InstructionSelector::VisitTryTruncateFloat32ToUint64(Node* node) {
  Arm64OperandGenerator g(this);

  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kArm64Float32ToUint64, output_count, outputs, 1, inputs);
}


void InstructionSelector::VisitTryTruncateFloat64ToUint64(Node* node) {
  Arm64OperandGenerator g(this);

  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kArm64Float64ToUint64, output_count, outputs, 1, inputs);
}


void InstructionSelector::VisitChangeInt32ToInt64(Node* node) {
  Node* value = node->InputAt(0);
  if (value->opcode() == IrOpcode::kLoad && CanCover(node, value)) {
    // Generate sign-extending load.
    LoadRepresentation load_rep = LoadRepresentationOf(value->op());
    MachineRepresentation rep = load_rep.representation();
    InstructionCode opcode = kArchNop;
    ImmediateMode immediate_mode = kNoImmediate;
    switch (rep) {
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = load_rep.IsSigned() ? kArm64Ldrsb : kArm64Ldrb;
        immediate_mode = kLoadStoreImm8;
        break;
      case MachineRepresentation::kWord16:
        opcode = load_rep.IsSigned() ? kArm64Ldrsh : kArm64Ldrh;
        immediate_mode = kLoadStoreImm16;
        break;
      case MachineRepresentation::kWord32:
        opcode = kArm64Ldrsw;
        immediate_mode = kLoadStoreImm32;
        break;
      default:
        UNREACHABLE();
        return;
    }
    EmitLoad(this, value, opcode, immediate_mode, rep, node);
  } else {
    VisitRR(this, kArm64Sxtw, node);
  }
}


void InstructionSelector::VisitChangeUint32ToUint64(Node* node) {
  Arm64OperandGenerator g(this);
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
    case IrOpcode::kInt32AddWithOverflow:
    case IrOpcode::kInt32Sub:
    case IrOpcode::kInt32SubWithOverflow:
    case IrOpcode::kInt32Mul:
    case IrOpcode::kInt32MulHigh:
    case IrOpcode::kInt32Div:
    case IrOpcode::kInt32Mod:
    case IrOpcode::kInt32LessThan:
    case IrOpcode::kInt32LessThanOrEqual:
    case IrOpcode::kUint32Div:
    case IrOpcode::kUint32LessThan:
    case IrOpcode::kUint32LessThanOrEqual:
    case IrOpcode::kUint32Mod:
    case IrOpcode::kUint32MulHigh: {
      // 32-bit operations will write their result in a W register (implicitly
      // clearing the top 32-bit of the corresponding X register) so the
      // zero-extension is a no-op.
      Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(value));
      return;
    }
    case IrOpcode::kLoad: {
      // As for the operations above, a 32-bit load will implicitly clear the
      // top 32 bits of the destination register.
      LoadRepresentation load_rep = LoadRepresentationOf(value->op());
      switch (load_rep.representation()) {
        case MachineRepresentation::kWord8:
        case MachineRepresentation::kWord16:
        case MachineRepresentation::kWord32:
          Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(value));
          return;
        default:
          break;
      }
    }
    default:
      break;
  }
  Emit(kArm64Mov32, g.DefineAsRegister(node), g.UseRegister(value));
}


void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  VisitRR(this, kArm64Float64ToFloat32, node);
}

void InstructionSelector::VisitTruncateFloat64ToWord32(Node* node) {
  VisitRR(this, kArchTruncateDoubleToI, node);
}

void InstructionSelector::VisitRoundFloat64ToInt32(Node* node) {
  VisitRR(this, kArm64Float64ToInt32, node);
}


void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  Arm64OperandGenerator g(this);
  Node* value = node->InputAt(0);
  // The top 32 bits in the 64-bit register will be undefined, and
  // must not be used by a dependent node.
  Emit(kArchNop, g.DefineSameAsFirst(node), g.UseRegister(value));
}


void InstructionSelector::VisitRoundInt64ToFloat32(Node* node) {
  VisitRR(this, kArm64Int64ToFloat32, node);
}


void InstructionSelector::VisitRoundInt64ToFloat64(Node* node) {
  VisitRR(this, kArm64Int64ToFloat64, node);
}


void InstructionSelector::VisitRoundUint64ToFloat32(Node* node) {
  VisitRR(this, kArm64Uint64ToFloat32, node);
}


void InstructionSelector::VisitRoundUint64ToFloat64(Node* node) {
  VisitRR(this, kArm64Uint64ToFloat64, node);
}


void InstructionSelector::VisitBitcastFloat32ToInt32(Node* node) {
  VisitRR(this, kArm64Float64ExtractLowWord32, node);
}


void InstructionSelector::VisitBitcastFloat64ToInt64(Node* node) {
  VisitRR(this, kArm64U64MoveFloat64, node);
}


void InstructionSelector::VisitBitcastInt32ToFloat32(Node* node) {
  VisitRR(this, kArm64Float64MoveU64, node);
}


void InstructionSelector::VisitBitcastInt64ToFloat64(Node* node) {
  VisitRR(this, kArm64Float64MoveU64, node);
}


void InstructionSelector::VisitFloat32Add(Node* node) {
  VisitRRR(this, kArm64Float32Add, node);
}


void InstructionSelector::VisitFloat64Add(Node* node) {
  VisitRRR(this, kArm64Float64Add, node);
}


void InstructionSelector::VisitFloat32Sub(Node* node) {
  VisitRRR(this, kArm64Float32Sub, node);
}

void InstructionSelector::VisitFloat64Sub(Node* node) {
  VisitRRR(this, kArm64Float64Sub, node);
}

void InstructionSelector::VisitFloat32Mul(Node* node) {
  VisitRRR(this, kArm64Float32Mul, node);
}


void InstructionSelector::VisitFloat64Mul(Node* node) {
  VisitRRR(this, kArm64Float64Mul, node);
}


void InstructionSelector::VisitFloat32Div(Node* node) {
  VisitRRR(this, kArm64Float32Div, node);
}


void InstructionSelector::VisitFloat64Div(Node* node) {
  VisitRRR(this, kArm64Float64Div, node);
}


void InstructionSelector::VisitFloat64Mod(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Float64Mod, g.DefineAsFixed(node, d0),
       g.UseFixed(node->InputAt(0), d0),
       g.UseFixed(node->InputAt(1), d1))->MarkAsCall();
}

void InstructionSelector::VisitFloat32Max(Node* node) {
  VisitRRR(this, kArm64Float32Max, node);
}

void InstructionSelector::VisitFloat64Max(Node* node) {
  VisitRRR(this, kArm64Float64Max, node);
}

void InstructionSelector::VisitFloat32Min(Node* node) {
  VisitRRR(this, kArm64Float32Min, node);
}

void InstructionSelector::VisitFloat64Min(Node* node) {
  VisitRRR(this, kArm64Float64Min, node);
}


void InstructionSelector::VisitFloat32Abs(Node* node) {
  VisitRR(this, kArm64Float32Abs, node);
}


void InstructionSelector::VisitFloat64Abs(Node* node) {
  VisitRR(this, kArm64Float64Abs, node);
}

void InstructionSelector::VisitFloat32Sqrt(Node* node) {
  VisitRR(this, kArm64Float32Sqrt, node);
}


void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  VisitRR(this, kArm64Float64Sqrt, node);
}


void InstructionSelector::VisitFloat32RoundDown(Node* node) {
  VisitRR(this, kArm64Float32RoundDown, node);
}


void InstructionSelector::VisitFloat64RoundDown(Node* node) {
  VisitRR(this, kArm64Float64RoundDown, node);
}


void InstructionSelector::VisitFloat32RoundUp(Node* node) {
  VisitRR(this, kArm64Float32RoundUp, node);
}


void InstructionSelector::VisitFloat64RoundUp(Node* node) {
  VisitRR(this, kArm64Float64RoundUp, node);
}


void InstructionSelector::VisitFloat32RoundTruncate(Node* node) {
  VisitRR(this, kArm64Float32RoundTruncate, node);
}


void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  VisitRR(this, kArm64Float64RoundTruncate, node);
}


void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  VisitRR(this, kArm64Float64RoundTiesAway, node);
}


void InstructionSelector::VisitFloat32RoundTiesEven(Node* node) {
  VisitRR(this, kArm64Float32RoundTiesEven, node);
}


void InstructionSelector::VisitFloat64RoundTiesEven(Node* node) {
  VisitRR(this, kArm64Float64RoundTiesEven, node);
}

void InstructionSelector::VisitFloat32Neg(Node* node) {
  VisitRR(this, kArm64Float32Neg, node);
}

void InstructionSelector::VisitFloat64Neg(Node* node) {
  VisitRR(this, kArm64Float64Neg, node);
}

void InstructionSelector::VisitFloat64Ieee754Binop(Node* node,
                                                   InstructionCode opcode) {
  Arm64OperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, d0), g.UseFixed(node->InputAt(0), d0),
       g.UseFixed(node->InputAt(1), d1))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat64Ieee754Unop(Node* node,
                                                  InstructionCode opcode) {
  Arm64OperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, d0), g.UseFixed(node->InputAt(0), d0))
      ->MarkAsCall();
}

void InstructionSelector::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* descriptor,
    Node* node) {
  Arm64OperandGenerator g(this);

  bool from_native_stack = linkage()->GetIncomingDescriptor()->UseNativeStack();
  bool to_native_stack = descriptor->UseNativeStack();

  bool always_claim = to_native_stack != from_native_stack;

  int claim_count = static_cast<int>(arguments->size());
  int slot = claim_count - 1;
  // Bump the stack pointer(s).
  if (claim_count > 0 || always_claim) {
    // TODO(titzer): claim and poke probably take small immediates.
    // TODO(titzer): it would be better to bump the csp here only
    //                and emit paired stores with increment for non c frames.
    ArchOpcode claim = to_native_stack ? kArm64ClaimCSP : kArm64ClaimJSSP;
    // Claim(0) isn't a nop if there is a mismatch between CSP and JSSP.
    Emit(claim, g.NoOutput(), g.TempImmediate(claim_count));
  }

  // Poke the arguments into the stack.
  ArchOpcode poke = to_native_stack ? kArm64PokeCSP : kArm64PokeJSSP;
  while (slot >= 0) {
    Emit(poke, g.NoOutput(), g.UseRegister((*arguments)[slot].node()),
         g.TempImmediate(slot));
    slot--;
    // TODO(ahaas): Poke arguments in pairs if two subsequent arguments have the
    //              same type.
    // Emit(kArm64PokePair, g.NoOutput(), g.UseRegister((*arguments)[slot]),
    //      g.UseRegister((*arguments)[slot - 1]), g.TempImmediate(slot));
    // slot -= 2;
  }
}


bool InstructionSelector::IsTailCallAddressImmediate() { return false; }

int InstructionSelector::GetTempsCountForTailCallFromJSFunction() { return 3; }

namespace {

// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  InstructionOperand left, InstructionOperand right,
                  FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
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


// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
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

// This function checks whether we can convert:
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

// This function checks if we can perform the transformation:
// ((a <op> b) cmp 0), b.<cond>
// to:
// (a <ops> b), b.<cond'>
// where <ops> is the flag setting version of <op>, and if so,
// updates {node}, {opcode} and {cont} accordingly.
void MaybeReplaceCmpZeroWithFlagSettingBinop(InstructionSelector* selector,
                                             Node** node, Node* binop,
                                             ArchOpcode* opcode,
                                             FlagsCondition cond,
                                             FlagsContinuation* cont,
                                             ImmediateMode* immediate_mode) {
  ArchOpcode binop_opcode;
  ArchOpcode no_output_opcode;
  ImmediateMode binop_immediate_mode;
  switch (binop->opcode()) {
    case IrOpcode::kInt32Add:
      binop_opcode = kArm64Add32;
      no_output_opcode = kArm64Cmn32;
      binop_immediate_mode = kArithmeticImm;
      break;
    case IrOpcode::kWord32And:
      binop_opcode = kArm64And32;
      no_output_opcode = kArm64Tst32;
      binop_immediate_mode = kLogical32Imm;
      break;
    default:
      UNREACHABLE();
      return;
  }
  if (selector->CanCover(*node, binop)) {
    // The comparison is the only user of the add or and, so we can generate
    // a cmn or tst instead.
    cont->Overwrite(MapForFlagSettingBinop(cond));
    *opcode = no_output_opcode;
    *node = binop;
    *immediate_mode = binop_immediate_mode;
  } else if (selector->IsOnlyUserOfNodeInSameBlock(*node, binop)) {
    // We can also handle the case where the add and the compare are in the
    // same basic block, and the compare is the only use of add in this basic
    // block (the add has users in other basic blocks).
    cont->Overwrite(MapForFlagSettingBinop(cond));
    *opcode = binop_opcode;
    *node = binop;
    *immediate_mode = binop_immediate_mode;
  }
}

// Map {cond} to kEqual or kNotEqual, so that we can select
// either TBZ or TBNZ when generating code for:
// (x cmp 0), b.{cond}
FlagsCondition MapForTbz(FlagsCondition cond) {
  switch (cond) {
    case kSignedLessThan:  // generate TBNZ
      return kNotEqual;
    case kSignedGreaterThanOrEqual:  // generate TBZ
      return kEqual;
    default:
      UNREACHABLE();
      return cond;
  }
}

// Map {cond} to kEqual or kNotEqual, so that we can select
// either CBZ or CBNZ when generating code for:
// (x cmp 0), b.{cond}
FlagsCondition MapForCbz(FlagsCondition cond) {
  switch (cond) {
    case kEqual:     // generate CBZ
    case kNotEqual:  // generate CBNZ
      return cond;
    case kUnsignedLessThanOrEqual:  // generate CBZ
      return kEqual;
    case kUnsignedGreaterThan:  // generate CBNZ
      return kNotEqual;
    default:
      UNREACHABLE();
      return cond;
  }
}

void EmitBranchOrDeoptimize(InstructionSelector* selector,
                            InstructionCode opcode, InstructionOperand value,
                            FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
  if (cont->IsBranch()) {
    selector->Emit(cont->Encode(opcode), g.NoOutput(), value,
                   g.Label(cont->true_block()), g.Label(cont->false_block()));
  } else {
    DCHECK(cont->IsDeoptimize());
    selector->EmitDeoptimize(cont->Encode(opcode), g.NoOutput(), value,
                             cont->reason(), cont->frame_state());
  }
}

// Try to emit TBZ, TBNZ, CBZ or CBNZ for certain comparisons of {node}
// against zero, depending on the condition.
bool TryEmitCbzOrTbz(InstructionSelector* selector, Node* node, Node* user,
                     FlagsCondition cond, FlagsContinuation* cont) {
  Int32BinopMatcher m_user(user);
  USE(m_user);
  DCHECK(m_user.right().Is(0) || m_user.left().Is(0));

  // Only handle branches and deoptimisations.
  if (!cont->IsBranch() && !cont->IsDeoptimize()) return false;

  switch (cond) {
    case kSignedLessThan:
    case kSignedGreaterThanOrEqual: {
      // We don't generate TBZ/TBNZ for deoptimisations, as they have a
      // shorter range than conditional branches and generating them for
      // deoptimisations results in more veneers.
      if (cont->IsDeoptimize()) return false;
      Arm64OperandGenerator g(selector);
      cont->Overwrite(MapForTbz(cond));
      Int32Matcher m(node);
      if (m.IsFloat64ExtractHighWord32() && selector->CanCover(user, node)) {
        // SignedLessThan(Float64ExtractHighWord32(x), 0) and
        // SignedGreaterThanOrEqual(Float64ExtractHighWord32(x), 0) essentially
        // check the sign bit of a 64-bit floating point value.
        InstructionOperand temp = g.TempRegister();
        selector->Emit(kArm64U64MoveFloat64, temp,
                       g.UseRegister(node->InputAt(0)));
        selector->Emit(cont->Encode(kArm64TestAndBranch), g.NoOutput(), temp,
                       g.TempImmediate(63), g.Label(cont->true_block()),
                       g.Label(cont->false_block()));
        return true;
      }
      selector->Emit(cont->Encode(kArm64TestAndBranch32), g.NoOutput(),
                     g.UseRegister(node), g.TempImmediate(31),
                     g.Label(cont->true_block()), g.Label(cont->false_block()));
      return true;
    }
    case kEqual:
    case kNotEqual:
    case kUnsignedLessThanOrEqual:
    case kUnsignedGreaterThan: {
      Arm64OperandGenerator g(selector);
      cont->Overwrite(MapForCbz(cond));
      EmitBranchOrDeoptimize(selector, kArm64CompareAndBranch32,
                             g.UseRegister(node), cont);
      return true;
    }
    default:
      return false;
  }
}

void VisitWord32Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  Int32BinopMatcher m(node);
  ArchOpcode opcode = kArm64Cmp32;
  FlagsCondition cond = cont->condition();
  if (m.right().Is(0)) {
    if (TryEmitCbzOrTbz(selector, m.left().node(), node, cond, cont)) return;
  } else if (m.left().Is(0)) {
    FlagsCondition commuted_cond = CommuteFlagsCondition(cond);
    if (TryEmitCbzOrTbz(selector, m.right().node(), node, commuted_cond, cont))
      return;
  }
  ImmediateMode immediate_mode = kArithmeticImm;
  if (m.right().Is(0) && (m.left().IsInt32Add() || m.left().IsWord32And())) {
    // Emit flag setting add/and instructions for comparisons against zero.
    if (CanUseFlagSettingBinop(cond)) {
      Node* binop = m.left().node();
      MaybeReplaceCmpZeroWithFlagSettingBinop(selector, &node, binop, &opcode,
                                              cond, cont, &immediate_mode);
    }
  } else if (m.left().Is(0) &&
             (m.right().IsInt32Add() || m.right().IsWord32And())) {
    // Same as above, but we need to commute the condition before we
    // continue with the rest of the checks.
    FlagsCondition commuted_cond = CommuteFlagsCondition(cond);
    if (CanUseFlagSettingBinop(commuted_cond)) {
      Node* binop = m.right().node();
      MaybeReplaceCmpZeroWithFlagSettingBinop(selector, &node, binop, &opcode,
                                              commuted_cond, cont,
                                              &immediate_mode);
    }
  } else if (m.right().IsInt32Sub() && (cond == kEqual || cond == kNotEqual)) {
    // Select negated compare for comparisons with negated right input.
    // Only do this for kEqual and kNotEqual, which do not depend on the
    // C and V flags, as those flags will be different with CMN when the
    // right-hand side of the original subtraction is INT_MIN.
    Node* sub = m.right().node();
    Int32BinopMatcher msub(sub);
    if (msub.left().Is(0)) {
      bool can_cover = selector->CanCover(node, sub);
      node->ReplaceInput(1, msub.right().node());
      // Even if the comparison node covers the subtraction, after the input
      // replacement above, the node still won't cover the input to the
      // subtraction; the subtraction still uses it.
      // In order to get shifted operations to work, we must remove the rhs
      // input to the subtraction, as TryMatchAnyShift requires this node to
      // cover the input shift. We do this by setting it to the lhs input,
      // as we know it's zero, and the result of the subtraction isn't used by
      // any other node.
      if (can_cover) sub->ReplaceInput(1, msub.left().node());
      opcode = kArm64Cmn32;
    }
  }
  VisitBinop<Int32BinopMatcher>(selector, node, opcode, immediate_mode, cont);
}


void VisitWordTest(InstructionSelector* selector, Node* node,
                   InstructionCode opcode, FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
  VisitCompare(selector, opcode, g.UseRegister(node), g.UseRegister(node),
               cont);
}


void VisitWord32Test(InstructionSelector* selector, Node* node,
                     FlagsContinuation* cont) {
  VisitWordTest(selector, node, kArm64Tst32, cont);
}


void VisitWord64Test(InstructionSelector* selector, Node* node,
                     FlagsContinuation* cont) {
  VisitWordTest(selector, node, kArm64Tst, cont);
}

template <typename Matcher, ArchOpcode kOpcode>
bool TryEmitTestAndBranch(InstructionSelector* selector, Node* node,
                          FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
  Matcher m(node);
  if (cont->IsBranch() && m.right().HasValue() &&
      (base::bits::CountPopulation(m.right().Value()) == 1)) {
    // If the mask has only one bit set, we can use tbz/tbnz.
    DCHECK((cont->condition() == kEqual) || (cont->condition() == kNotEqual));
    selector->Emit(
        cont->Encode(kOpcode), g.NoOutput(), g.UseRegister(m.left().node()),
        g.TempImmediate(base::bits::CountTrailingZeros(m.right().Value())),
        g.Label(cont->true_block()), g.Label(cont->false_block()));
    return true;
  }
  return false;
}

// Shared routine for multiple float32 compare operations.
void VisitFloat32Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
  Float32BinopMatcher m(node);
  if (m.right().Is(0.0f)) {
    VisitCompare(selector, kArm64Float32Cmp, g.UseRegister(m.left().node()),
                 g.UseImmediate(m.right().node()), cont);
  } else if (m.left().Is(0.0f)) {
    cont->Commute();
    VisitCompare(selector, kArm64Float32Cmp, g.UseRegister(m.right().node()),
                 g.UseImmediate(m.left().node()), cont);
  } else {
    VisitCompare(selector, kArm64Float32Cmp, g.UseRegister(m.left().node()),
                 g.UseRegister(m.right().node()), cont);
  }
}


// Shared routine for multiple float64 compare operations.
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
  Float64BinopMatcher m(node);
  if (m.right().Is(0.0)) {
    VisitCompare(selector, kArm64Float64Cmp, g.UseRegister(m.left().node()),
                 g.UseImmediate(m.right().node()), cont);
  } else if (m.left().Is(0.0)) {
    cont->Commute();
    VisitCompare(selector, kArm64Float64Cmp, g.UseRegister(m.right().node()),
                 g.UseImmediate(m.left().node()), cont);
  } else {
    VisitCompare(selector, kArm64Float64Cmp, g.UseRegister(m.left().node()),
                 g.UseRegister(m.right().node()), cont);
  }
}

void VisitWordCompareZero(InstructionSelector* selector, Node* user,
                          Node* value, FlagsContinuation* cont) {
  Arm64OperandGenerator g(selector);
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
        return VisitWord32Compare(selector, value, cont);
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
        cont->OverwriteAndNegateIfEqual(kEqual);
        Int64BinopMatcher m(value);
        if (m.right().Is(0)) {
          Node* const left = m.left().node();
          if (selector->CanCover(value, left) &&
              left->opcode() == IrOpcode::kWord64And) {
            // Attempt to merge the Word64Equal(Word64And(x, y), 0) comparison
            // into a tbz/tbnz instruction.
            if (TryEmitTestAndBranch<Uint64BinopMatcher, kArm64TestAndBranch>(
                    selector, left, cont)) {
              return;
            }
            return VisitWordCompare(selector, left, kArm64Tst, cont, true,
                                    kLogical64Imm);
          }
          // Merge the Word64Equal(x, 0) comparison into a cbz instruction.
          if (cont->IsBranch() || cont->IsDeoptimize()) {
            EmitBranchOrDeoptimize(selector,
                                   cont->Encode(kArm64CompareAndBranch),
                                   g.UseRegister(left), cont);
            return;
          }
        }
        return VisitWordCompare(selector, value, kArm64Cmp, cont, false,
                                kArithmeticImm);
      }
      case IrOpcode::kInt64LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWordCompare(selector, value, kArm64Cmp, cont, false,
                                kArithmeticImm);
      case IrOpcode::kInt64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWordCompare(selector, value, kArm64Cmp, cont, false,
                                kArithmeticImm);
      case IrOpcode::kUint64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWordCompare(selector, value, kArm64Cmp, cont, false,
                                kArithmeticImm);
      case IrOpcode::kUint64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWordCompare(selector, value, kArm64Cmp, cont, false,
                                kArithmeticImm);
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
          if (result == nullptr || selector->IsDefined(result)) {
            switch (node->opcode()) {
              case IrOpcode::kInt32AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int32BinopMatcher>(
                    selector, node, kArm64Add32, kArithmeticImm, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int32BinopMatcher>(
                    selector, node, kArm64Sub32, kArithmeticImm, cont);
              case IrOpcode::kInt32MulWithOverflow:
                // ARM64 doesn't set the overflow flag for multiplication, so we
                // need to test on kNotEqual. Here is the code sequence used:
                //   smull result, left, right
                //   cmp result.X(), Operand(result, SXTW)
                cont->OverwriteAndNegateIfEqual(kNotEqual);
                return EmitInt32MulWithOverflow(selector, node, cont);
              case IrOpcode::kInt64AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int64BinopMatcher>(selector, node, kArm64Add,
                                                     kArithmeticImm, cont);
              case IrOpcode::kInt64SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int64BinopMatcher>(selector, node, kArm64Sub,
                                                     kArithmeticImm, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Add:
        return VisitWordCompare(selector, value, kArm64Cmn32, cont, true,
                                kArithmeticImm);
      case IrOpcode::kInt32Sub:
        return VisitWord32Compare(selector, value, cont);
      case IrOpcode::kWord32And:
        if (TryEmitTestAndBranch<Uint32BinopMatcher, kArm64TestAndBranch32>(
                selector, value, cont)) {
          return;
        }
        return VisitWordCompare(selector, value, kArm64Tst32, cont, true,
                                kLogical32Imm);
      case IrOpcode::kWord64And:
        if (TryEmitTestAndBranch<Uint64BinopMatcher, kArm64TestAndBranch>(
                selector, value, cont)) {
          return;
        }
        return VisitWordCompare(selector, value, kArm64Tst, cont, true,
                                kLogical64Imm);
      default:
        break;
    }
  }

  // Branch could not be combined with a compare, compare against 0 and branch.
  if (cont->IsBranch()) {
    selector->Emit(cont->Encode(kArm64CompareAndBranch32), g.NoOutput(),
                   g.UseRegister(value), g.Label(cont->true_block()),
                   g.Label(cont->false_block()));
  } else if (cont->IsDeoptimize()) {
    selector->EmitDeoptimize(cont->Encode(kArm64Tst32), g.NoOutput(),
                             g.UseRegister(value), g.UseRegister(value),
                             cont->reason(), cont->frame_state());
  } else {
    DCHECK(cont->IsTrap());
    selector->Emit(cont->Encode(kArm64Tst32), g.NoOutput(),
                   g.UseRegister(value), g.UseRegister(value),
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
  Arm64OperandGenerator g(this);
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
      Emit(kArm64Sub32, index_operand, value_operand,
           g.TempImmediate(sw.min_value));
    }
    // Generate a table lookup.
    return EmitTableSwitch(sw, index_operand);
  }

  // Generate a sequence of conditional jumps.
  return EmitLookupSwitch(sw, value_operand);
}


void InstructionSelector::VisitWord32Equal(Node* const node) {
  Node* const user = node;
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int32BinopMatcher m(user);
  if (m.right().Is(0)) {
    Node* const value = m.left().node();
    if (CanCover(user, value)) {
      switch (value->opcode()) {
        case IrOpcode::kInt32Add:
        case IrOpcode::kWord32And:
          return VisitWord32Compare(this, node, &cont);
        case IrOpcode::kInt32Sub:
          return VisitWordCompare(this, value, kArm64Cmp32, &cont, false,
                                  kArithmeticImm);
        case IrOpcode::kWord32Equal: {
          // Word32Equal(Word32Equal(x, y), 0) => Word32Compare(x, y, ne).
          Int32BinopMatcher mequal(value);
          node->ReplaceInput(0, mequal.left().node());
          node->ReplaceInput(1, mequal.right().node());
          cont.Negate();
          // {node} still does not cover its new operands, because {mequal} is
          // still using them.
          // Since we won't generate any more code for {mequal}, set its
          // operands to zero to make sure {node} can cover them.
          // This improves pattern matching in VisitWord32Compare.
          mequal.node()->ReplaceInput(0, m.right().node());
          mequal.node()->ReplaceInput(1, m.right().node());
          return VisitWord32Compare(this, node, &cont);
        }
        default:
          break;
      }
      return VisitWord32Test(this, value, &cont);
    }
  }
  VisitWord32Compare(this, node, &cont);
}


void InstructionSelector::VisitInt32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWord32Compare(this, node, &cont);
}


void InstructionSelector::VisitInt32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWord32Compare(this, node, &cont);
}


void InstructionSelector::VisitUint32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWord32Compare(this, node, &cont);
}


void InstructionSelector::VisitUint32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWord32Compare(this, node, &cont);
}


void InstructionSelector::VisitWord64Equal(Node* const node) {
  Node* const user = node;
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
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
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop<Int32BinopMatcher>(this, node, kArm64Add32,
                                         kArithmeticImm, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int32BinopMatcher>(this, node, kArm64Add32, kArithmeticImm, &cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop<Int32BinopMatcher>(this, node, kArm64Sub32,
                                         kArithmeticImm, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int32BinopMatcher>(this, node, kArm64Sub32, kArithmeticImm, &cont);
}

void InstructionSelector::VisitInt32MulWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    // ARM64 doesn't set the overflow flag for multiplication, so we need to
    // test on kNotEqual. Here is the code sequence used:
    //   smull result, left, right
    //   cmp result.X(), Operand(result, SXTW)
    FlagsContinuation cont = FlagsContinuation::ForSet(kNotEqual, ovf);
    return EmitInt32MulWithOverflow(this, node, &cont);
  }
  FlagsContinuation cont;
  EmitInt32MulWithOverflow(this, node, &cont);
}

void InstructionSelector::VisitInt64AddWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop<Int64BinopMatcher>(this, node, kArm64Add, kArithmeticImm,
                                         &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int64BinopMatcher>(this, node, kArm64Add, kArithmeticImm, &cont);
}


void InstructionSelector::VisitInt64SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop<Int64BinopMatcher>(this, node, kArm64Sub, kArithmeticImm,
                                         &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int64BinopMatcher>(this, node, kArm64Sub, kArithmeticImm, &cont);
}


void InstructionSelector::VisitInt64LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWordCompare(this, node, kArm64Cmp, &cont, false, kArithmeticImm);
}


void InstructionSelector::VisitInt64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kArm64Cmp, &cont, false, kArithmeticImm);
}


void InstructionSelector::VisitUint64LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWordCompare(this, node, kArm64Cmp, &cont, false, kArithmeticImm);
}


void InstructionSelector::VisitUint64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kArm64Cmp, &cont, false, kArithmeticImm);
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
  Arm64OperandGenerator g(this);
  Emit(kArm64Float64ExtractLowWord32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64ExtractHighWord32(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Float64ExtractHighWord32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64InsertLowWord32(Node* node) {
  Arm64OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (left->opcode() == IrOpcode::kFloat64InsertHighWord32 &&
      CanCover(node, left)) {
    Node* right_of_left = left->InputAt(1);
    Emit(kArm64Bfi, g.DefineSameAsFirst(right), g.UseRegister(right),
         g.UseRegister(right_of_left), g.TempImmediate(32),
         g.TempImmediate(32));
    Emit(kArm64Float64MoveU64, g.DefineAsRegister(node), g.UseRegister(right));
    return;
  }
  Emit(kArm64Float64InsertLowWord32, g.DefineAsRegister(node),
       g.UseRegister(left), g.UseRegister(right));
}


void InstructionSelector::VisitFloat64InsertHighWord32(Node* node) {
  Arm64OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (left->opcode() == IrOpcode::kFloat64InsertLowWord32 &&
      CanCover(node, left)) {
    Node* right_of_left = left->InputAt(1);
    Emit(kArm64Bfi, g.DefineSameAsFirst(left), g.UseRegister(right_of_left),
         g.UseRegister(right), g.TempImmediate(32), g.TempImmediate(32));
    Emit(kArm64Float64MoveU64, g.DefineAsRegister(node), g.UseRegister(left));
    return;
  }
  Emit(kArm64Float64InsertHighWord32, g.DefineAsRegister(node),
       g.UseRegister(left), g.UseRegister(right));
}

void InstructionSelector::VisitFloat64SilenceNaN(Node* node) {
  VisitRR(this, kArm64Float64SilenceNaN, node);
}

void InstructionSelector::VisitAtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  Arm64OperandGenerator g(this);
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
  Emit(opcode | AddressingModeField::encode(kMode_MRR),
       g.DefineAsRegister(node), g.UseRegister(base), g.UseRegister(index));
}

void InstructionSelector::VisitAtomicStore(Node* node) {
  MachineRepresentation rep = AtomicStoreRepresentationOf(node->op());
  Arm64OperandGenerator g(this);
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

  AddressingMode addressing_mode = kMode_MRR;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 0, nullptr, input_count, inputs);
}

// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  return MachineOperatorBuilder::kFloat32RoundDown |
         MachineOperatorBuilder::kFloat64RoundDown |
         MachineOperatorBuilder::kFloat32RoundUp |
         MachineOperatorBuilder::kFloat64RoundUp |
         MachineOperatorBuilder::kFloat32RoundTruncate |
         MachineOperatorBuilder::kFloat64RoundTruncate |
         MachineOperatorBuilder::kFloat64RoundTiesAway |
         MachineOperatorBuilder::kFloat32RoundTiesEven |
         MachineOperatorBuilder::kFloat64RoundTiesEven |
         MachineOperatorBuilder::kWord32ShiftIsSafe |
         MachineOperatorBuilder::kInt32DivIsSafe |
         MachineOperatorBuilder::kUint32DivIsSafe |
         MachineOperatorBuilder::kWord32ReverseBits |
         MachineOperatorBuilder::kWord64ReverseBits;
}

// static
MachineOperatorBuilder::AlignmentRequirements
InstructionSelector::AlignmentRequirements() {
  return MachineOperatorBuilder::AlignmentRequirements::
      FullUnalignedAccessSupport();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
