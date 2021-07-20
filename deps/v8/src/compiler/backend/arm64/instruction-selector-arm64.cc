// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#include "src/base/platform/wrappers.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/machine-type.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction-codes.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/machine-operator.h"
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
  kLoadStoreImm8,  // signed 8 bit or 12 bit unsigned scaled by access size
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
         (bit_cast<int64_t>(GetFloatConstantValue(node)) == 0))) {
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
      return OpParameter<int32_t>(node->op());
    }
    DCHECK_EQ(IrOpcode::kInt64Constant, node->opcode());
    return OpParameter<int64_t>(node->op());
  }

  bool IsFloatConstant(Node* node) {
    return (node->opcode() == IrOpcode::kFloat32Constant) ||
           (node->opcode() == IrOpcode::kFloat64Constant);
  }

  double GetFloatConstantValue(Node* node) {
    if (node->opcode() == IrOpcode::kFloat32Constant) {
      return OpParameter<float>(node->op());
    }
    DCHECK_EQ(IrOpcode::kFloat64Constant, node->opcode());
    return OpParameter<double>(node->op());
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
        return IsLoadStoreImmediate(value, 0);
      case kLoadStoreImm16:
        return IsLoadStoreImmediate(value, 1);
      case kLoadStoreImm32:
        return IsLoadStoreImmediate(value, 2);
      case kLoadStoreImm64:
        return IsLoadStoreImmediate(value, 3);
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
    DCHECK_GT(MachineRepresentation::kSimd128, rep);
    return IsIntegerConstant(node) &&
           (GetIntegerConstantValue(node) == ElementSizeLog2Of(rep));
  }

 private:
  bool IsLoadStoreImmediate(int64_t value, unsigned size) {
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

void VisitRR(InstructionSelector* selector, InstructionCode opcode,
             Node* node) {
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

void VisitRRR(InstructionSelector* selector, InstructionCode opcode,
              Node* node) {
  Arm64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
}

void VisitSimdShiftRRR(InstructionSelector* selector, ArchOpcode opcode,
                       Node* node, int width) {
  Arm64OperandGenerator g(selector);
  if (g.IsIntegerConstant(node->InputAt(1))) {
    if (g.GetIntegerConstantValue(node->InputAt(1)) % width == 0) {
      selector->EmitIdentity(node);
    } else {
      selector->Emit(opcode, g.DefineAsRegister(node),
                     g.UseRegister(node->InputAt(0)),
                     g.UseImmediate(node->InputAt(1)));
    }
  } else {
    selector->Emit(opcode, g.DefineAsRegister(node),
                   g.UseRegister(node->InputAt(0)),
                   g.UseRegister(node->InputAt(1)));
  }
}

void VisitRRI(InstructionSelector* selector, ArchOpcode opcode, Node* node) {
  Arm64OperandGenerator g(selector);
  int32_t imm = OpParameter<int32_t>(node->op());
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)), g.UseImmediate(imm));
}

void VisitRRO(InstructionSelector* selector, ArchOpcode opcode, Node* node,
              ImmediateMode operand_mode) {
  Arm64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseOperand(node->InputAt(1), operand_mode));
}

void VisitRRIR(InstructionSelector* selector, ArchOpcode opcode, Node* node) {
  Arm64OperandGenerator g(selector);
  int32_t imm = OpParameter<int32_t>(node->op());
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)), g.UseImmediate(imm),
                 g.UseUniqueRegister(node->InputAt(1)));
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
    if (mright.right().Is(0xFF) || mright.right().Is(0xFFFF)) {
      int32_t mask = mright.right().ResolvedValue();
      *left_op = g->UseRegister(left_node);
      *right_op = g->UseRegister(mright.left().node());
      *opcode |= AddressingModeField::encode(
          (mask == 0xFF) ? kMode_Operand2_R_UXTB : kMode_Operand2_R_UXTH);
      return true;
    }
  } else if (nm.IsWord32Sar()) {
    Int32BinopMatcher mright(right_node);
    if (selector->CanCover(mright.node(), mright.left().node()) &&
        mright.left().IsWord32Shl()) {
      Int32BinopMatcher mleft_of_right(mright.left().node());
      if ((mright.right().Is(16) && mleft_of_right.right().Is(16)) ||
          (mright.right().Is(24) && mleft_of_right.right().Is(24))) {
        int32_t shift = mright.right().ResolvedValue();
        *left_op = g->UseRegister(left_node);
        *right_op = g->UseRegister(mleft_of_right.left().node());
        *opcode |= AddressingModeField::encode(
            (shift == 24) ? kMode_Operand2_R_SXTB : kMode_Operand2_R_SXTH);
        return true;
      }
    }
  } else if (nm.IsChangeInt32ToInt64()) {
    // Use extended register form.
    *opcode |= AddressingModeField::encode(kMode_Operand2_R_SXTW);
    *left_op = g->UseRegister(left_node);
    *right_op = g->UseRegister(right_node->InputAt(0));
    return true;
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
using CanCommuteField = base::BitField8<bool, 1, 1>;
// MustCommuteCondField is true when we need to commute the flags continuation
// condition in order to switch the operands.
using MustCommuteCondField = base::BitField8<bool, 2, 1>;
// IsComparisonField is true when the operation is a comparison and has no other
// result other than the condition.
using IsComparisonField = base::BitField8<bool, 3, 1>;
// IsAddSubField is true when an instruction is encoded as ADD or SUB.
using IsAddSubField = base::BitField8<bool, 4, 1>;

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
  InstructionOperand outputs[1];
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
    // We only need at most the last 6 bits of the shift.
    inputs[input_count++] = g.UseImmediate(
        static_cast<int>(m_shift.right().ResolvedValue() & 0x3F));
  } else if (can_commute && TryMatchAnyShift(selector, node, left_node, &opcode,
                                             !is_add_sub)) {
    if (must_commute_cond) cont->Commute();
    Matcher m_shift(left_node);
    inputs[input_count++] = g.UseRegisterOrImmediateZero(right_node);
    inputs[input_count++] = g.UseRegister(m_shift.left().node());
    // We only need at most the last 6 bits of the shift.
    inputs[input_count++] = g.UseImmediate(
        static_cast<int>(m_shift.right().ResolvedValue() & 0x3F));
  } else {
    inputs[input_count++] = g.UseRegisterOrImmediateZero(left_node);
    inputs[input_count++] = g.UseRegister(right_node);
  }

  if (!IsComparisonField::decode(properties)) {
    outputs[output_count++] = g.DefineAsRegister(node);
  }

  if (cont->IsSelect()) {
    inputs[input_count++] = g.UseRegister(cont->true_value());
    inputs[input_count++] = g.UseRegister(cont->false_value());
  }

  DCHECK_NE(0u, input_count);
  DCHECK((output_count != 0) || IsComparisonField::decode(properties));
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
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
  if (m.right().HasResolvedValue() && (m.right().ResolvedValue() < 0) &&
      (m.right().ResolvedValue() > std::numeric_limits<int>::min()) &&
      g.CanBeImmediate(-m.right().ResolvedValue(), kArithmeticImm)) {
    selector->Emit(
        negate_opcode, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
        g.TempImmediate(static_cast<int32_t>(-m.right().ResolvedValue())));
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
  if (m->right().HasResolvedValue() && m->right().ResolvedValue() >= 3) {
    uint64_t value_minus_one = m->right().ResolvedValue() - 1;
    if (base::bits::IsPowerOfTwo(value_minus_one)) {
      return base::bits::WhichPowerOfTwo(value_minus_one);
    }
  }
  return 0;
}

}  // namespace

void InstructionSelector::VisitStackSlot(Node* node) {
  StackSlotRepresentation rep = StackSlotRepresentationOf(node->op());
  int slot = frame_->AllocateSpillSlot(rep.size(), rep.alignment());
  OperandGenerator g(this);

  Emit(kArchStackSlot, g.DefineAsRegister(node),
       sequence()->AddImmediate(Constant(slot)), 0, nullptr);
}

void InstructionSelector::VisitAbortCSAAssert(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArchAbortCSAAssert, g.NoOutput(), g.UseFixed(node->InputAt(0), x1));
}

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

  ExternalReferenceMatcher m(base);
  if (m.HasResolvedValue() && g.IsIntegerConstant(index) &&
      selector->CanAddressRelativeToRootsRegister(m.ResolvedValue())) {
    ptrdiff_t const delta =
        g.GetIntegerConstantValue(index) +
        TurboAssemblerBase::RootRegisterOffsetForExternalReference(
            selector->isolate(), m.ResolvedValue());
    input_count = 1;
    // Check that the delta is a 32-bit integer due to the limitations of
    // immediate operands.
    if (is_int32(delta)) {
      inputs[0] = g.UseImmediate(static_cast<int32_t>(delta));
      opcode |= AddressingModeField::encode(kMode_Root);
      selector->Emit(opcode, arraysize(outputs), outputs, input_count, inputs);
      return;
    }
  }

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

namespace {
// Manually add base and index into a register to get the actual address.
// This should be used prior to instructions that only support
// immediate/post-index addressing, like ld1 and st1.
InstructionOperand EmitAddBeforeLoadOrStore(InstructionSelector* selector,
                                            Node* node,
                                            InstructionCode* opcode) {
  Arm64OperandGenerator g(selector);
  InstructionOperand addr = g.TempRegister();
  selector->Emit(kArm64Add, addr, g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
  *opcode |= AddressingModeField::encode(kMode_MRI);
  return addr;
}
}  // namespace

void InstructionSelector::VisitLoadLane(Node* node) {
  LoadLaneParameters params = LoadLaneParametersOf(node->op());
  DCHECK(
      params.rep == MachineType::Int8() || params.rep == MachineType::Int16() ||
      params.rep == MachineType::Int32() || params.rep == MachineType::Int64());

  InstructionCode opcode = kArm64LoadLane;
  opcode |= LaneSizeField::encode(params.rep.MemSize() * kBitsPerByte);
  if (params.kind == MemoryAccessKind::kProtected) {
    opcode |= AccessModeField::encode(kMemoryAccessProtected);
  }

  Arm64OperandGenerator g(this);
  InstructionOperand addr = EmitAddBeforeLoadOrStore(this, node, &opcode);
  Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(2)),
       g.UseImmediate(params.laneidx), addr, g.TempImmediate(0));
}

void InstructionSelector::VisitStoreLane(Node* node) {
  StoreLaneParameters params = StoreLaneParametersOf(node->op());
  DCHECK_LE(MachineRepresentation::kWord8, params.rep);
  DCHECK_GE(MachineRepresentation::kWord64, params.rep);

  InstructionCode opcode = kArm64StoreLane;
  opcode |=
      LaneSizeField::encode(ElementSizeInBytes(params.rep) * kBitsPerByte);
  if (params.kind == MemoryAccessKind::kProtected) {
    opcode |= AccessModeField::encode(kMemoryAccessProtected);
  }

  Arm64OperandGenerator g(this);
  InstructionOperand addr = EmitAddBeforeLoadOrStore(this, node, &opcode);
  InstructionOperand inputs[4] = {
      g.UseRegister(node->InputAt(2)),
      g.UseImmediate(params.laneidx),
      addr,
      g.TempImmediate(0),
  };

  Emit(opcode, 0, nullptr, 4, inputs);
}

void InstructionSelector::VisitLoadTransform(Node* node) {
  LoadTransformParameters params = LoadTransformParametersOf(node->op());
  InstructionCode opcode = kArchNop;
  bool require_add = false;
  switch (params.transformation) {
    case LoadTransformation::kS128Load8Splat:
      opcode = kArm64LoadSplat;
      opcode |= LaneSizeField::encode(8);
      require_add = true;
      break;
    case LoadTransformation::kS128Load16Splat:
      opcode = kArm64LoadSplat;
      opcode |= LaneSizeField::encode(16);
      require_add = true;
      break;
    case LoadTransformation::kS128Load32Splat:
      opcode = kArm64LoadSplat;
      opcode |= LaneSizeField::encode(32);
      require_add = true;
      break;
    case LoadTransformation::kS128Load64Splat:
      opcode = kArm64LoadSplat;
      opcode |= LaneSizeField::encode(64);
      require_add = true;
      break;
    case LoadTransformation::kS128Load8x8S:
      opcode = kArm64S128Load8x8S;
      break;
    case LoadTransformation::kS128Load8x8U:
      opcode = kArm64S128Load8x8U;
      break;
    case LoadTransformation::kS128Load16x4S:
      opcode = kArm64S128Load16x4S;
      break;
    case LoadTransformation::kS128Load16x4U:
      opcode = kArm64S128Load16x4U;
      break;
    case LoadTransformation::kS128Load32x2S:
      opcode = kArm64S128Load32x2S;
      break;
    case LoadTransformation::kS128Load32x2U:
      opcode = kArm64S128Load32x2U;
      break;
    case LoadTransformation::kS128Load32Zero:
      opcode = kArm64LdrS;
      break;
    case LoadTransformation::kS128Load64Zero:
      opcode = kArm64LdrD;
      break;
    default:
      UNIMPLEMENTED();
  }
  // ARM64 supports unaligned loads
  DCHECK_NE(params.kind, MemoryAccessKind::kUnaligned);

  Arm64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  InstructionOperand inputs[2];
  InstructionOperand outputs[1];

  inputs[0] = g.UseRegister(base);
  inputs[1] = g.UseRegister(index);
  outputs[0] = g.DefineAsRegister(node);

  if (require_add) {
    // ld1r uses post-index, so construct address first.
    // TODO(v8:9886) If index can be immediate, use vldr without this add.
    inputs[0] = EmitAddBeforeLoadOrStore(this, node, &opcode);
    inputs[1] = g.TempImmediate(0);
    opcode |= AddressingModeField::encode(kMode_MRI);
  } else {
    opcode |= AddressingModeField::encode(kMode_MRR);
  }
  if (params.kind == MemoryAccessKind::kProtected) {
    opcode |= AccessModeField::encode(kMemoryAccessProtected);
  }
  Emit(opcode, 1, outputs, 2, inputs);
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
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:
#ifdef V8_COMPRESS_POINTERS
      opcode = kArm64LdrW;
      immediate_mode = kLoadStoreImm32;
      break;
#else
      UNREACHABLE();
#endif
#ifdef V8_COMPRESS_POINTERS
    case MachineRepresentation::kTaggedSigned:
      opcode = kArm64LdrDecompressTaggedSigned;
      immediate_mode = kLoadStoreImm32;
      break;
    case MachineRepresentation::kTaggedPointer:
      opcode = kArm64LdrDecompressTaggedPointer;
      immediate_mode = kLoadStoreImm32;
      break;
    case MachineRepresentation::kTagged:
      opcode = kArm64LdrDecompressAnyTagged;
      immediate_mode = kLoadStoreImm32;
      break;
#else
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
#endif
    case MachineRepresentation::kWord64:
      opcode = kArm64Ldr;
      immediate_mode = kLoadStoreImm64;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kArm64LdrQ;
      immediate_mode = kNoImmediate;
      break;
    case MachineRepresentation::kMapWord:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }
  if (node->opcode() == IrOpcode::kPoisonedLoad) {
    CHECK_NE(poisoning_level_, PoisoningMitigationLevel::kDontPoison);
    opcode |= AccessModeField::encode(kMemoryAccessPoisoned);
  }
  if (node->opcode() == IrOpcode::kProtectedLoad) {
    opcode |= AccessModeField::encode(kMemoryAccessProtected);
  }

  EmitLoad(this, node, opcode, immediate_mode, rep);
}

void InstructionSelector::VisitPoisonedLoad(Node* node) { VisitLoad(node); }

void InstructionSelector::VisitProtectedLoad(Node* node) { VisitLoad(node); }

void InstructionSelector::VisitStore(Node* node) {
  Arm64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());
  WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();
  MachineRepresentation rep = store_rep.representation();

  if (FLAG_enable_unconditional_write_barriers &&
      CanBeTaggedOrCompressedPointer(rep)) {
    write_barrier_kind = kFullWriteBarrier;
  }

  // TODO(arm64): I guess this could be done in a better way.
  if (write_barrier_kind != kNoWriteBarrier && !FLAG_disable_write_barriers) {
    DCHECK(CanBeTaggedOrCompressedPointer(rep));
    AddressingMode addressing_mode;
    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    // OutOfLineRecordWrite uses the index in an add or sub instruction, but we
    // can trust the assembler to generate extra instructions if the index does
    // not fit into add or sub. So here only check the immediate for a store.
    if (g.CanBeImmediate(index, COMPRESS_POINTERS_BOOL ? kLoadStoreImm32
                                                       : kLoadStoreImm64)) {
      inputs[input_count++] = g.UseImmediate(index);
      addressing_mode = kMode_MRI;
    } else {
      inputs[input_count++] = g.UseUniqueRegister(index);
      addressing_mode = kMode_MRR;
    }
    inputs[input_count++] = g.UseUniqueRegister(value);
    RecordWriteMode record_write_mode =
        WriteBarrierKindToRecordWriteMode(write_barrier_kind);
    InstructionCode code = kArchStoreWithWriteBarrier;
    code |= AddressingModeField::encode(addressing_mode);
    code |= MiscField::encode(static_cast<int>(record_write_mode));
    Emit(code, 0, nullptr, input_count, inputs);
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
      case MachineRepresentation::kCompressedPointer:  // Fall through.
      case MachineRepresentation::kCompressed:
#ifdef V8_COMPRESS_POINTERS
        opcode = kArm64StrCompressTagged;
        immediate_mode = kLoadStoreImm32;
        break;
#else
        UNREACHABLE();
#endif
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:
        opcode = kArm64StrCompressTagged;
        immediate_mode =
            COMPRESS_POINTERS_BOOL ? kLoadStoreImm32 : kLoadStoreImm64;
        break;
      case MachineRepresentation::kWord64:
        opcode = kArm64Str;
        immediate_mode = kLoadStoreImm64;
        break;
      case MachineRepresentation::kSimd128:
        opcode = kArm64StrQ;
        immediate_mode = kNoImmediate;
        break;
      case MachineRepresentation::kMapWord:  // Fall through.
      case MachineRepresentation::kNone:
        UNREACHABLE();
    }

    ExternalReferenceMatcher m(base);
    if (m.HasResolvedValue() && g.IsIntegerConstant(index) &&
        CanAddressRelativeToRootsRegister(m.ResolvedValue())) {
      ptrdiff_t const delta =
          g.GetIntegerConstantValue(index) +
          TurboAssemblerBase::RootRegisterOffsetForExternalReference(
              isolate(), m.ResolvedValue());
      if (is_int32(delta)) {
        input_count = 2;
        InstructionOperand inputs[2];
        inputs[0] = g.UseRegister(value);
        inputs[1] = g.UseImmediate(static_cast<int32_t>(delta));
        opcode |= AddressingModeField::encode(kMode_Root);
        Emit(opcode, 0, nullptr, input_count, inputs);
        return;
      }
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

    if (node->opcode() == IrOpcode::kProtectedStore) {
      opcode |= AccessModeField::encode(kMemoryAccessProtected);
    }

    Emit(opcode, 0, nullptr, input_count, inputs);
  }
}

void InstructionSelector::VisitProtectedStore(Node* node) { VisitStore(node); }

void InstructionSelector::VisitSimd128ReverseBytes(Node* node) {
  UNREACHABLE();
}

// Architecture supports unaligned access, therefore VisitLoad is used instead
void InstructionSelector::VisitUnalignedLoad(Node* node) { UNREACHABLE(); }

// Architecture supports unaligned access, therefore VisitStore is used instead
void InstructionSelector::VisitUnalignedStore(Node* node) { UNREACHABLE(); }

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
      m.right().HasResolvedValue()) {
    uint32_t mask = m.right().ResolvedValue();
    uint32_t mask_width = base::bits::CountPopulation(mask);
    uint32_t mask_msb = base::bits::CountLeadingZeros32(mask);
    if ((mask_width != 0) && (mask_width != 32) &&
        (mask_msb + mask_width == 32)) {
      // The mask must be contiguous, and occupy the least-significant bits.
      DCHECK_EQ(0u, base::bits::CountTrailingZeros32(mask));

      // Select Ubfx for And(Shr(x, imm), mask) where the mask is in the least
      // significant bits.
      Int32BinopMatcher mleft(m.left().node());
      if (mleft.right().HasResolvedValue()) {
        // Any shift value can match; int32 shifts use `value % 32`.
        uint32_t lsb = mleft.right().ResolvedValue() & 0x1F;

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
      m.right().HasResolvedValue()) {
    uint64_t mask = m.right().ResolvedValue();
    uint64_t mask_width = base::bits::CountPopulation(mask);
    uint64_t mask_msb = base::bits::CountLeadingZeros64(mask);
    if ((mask_width != 0) && (mask_width != 64) &&
        (mask_msb + mask_width == 64)) {
      // The mask must be contiguous, and occupy the least-significant bits.
      DCHECK_EQ(0u, base::bits::CountTrailingZeros64(mask));

      // Select Ubfx for And(Shr(x, imm), mask) where the mask is in the least
      // significant bits.
      Int64BinopMatcher mleft(m.left().node());
      if (mleft.right().HasResolvedValue()) {
        // Any shift value can match; int64 shifts use `value % 64`.
        uint32_t lsb =
            static_cast<uint32_t>(mleft.right().ResolvedValue() & 0x3F);

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
    if (mleft.right().HasResolvedValue()) {
      uint32_t mask = mleft.right().ResolvedValue();
      uint32_t mask_width = base::bits::CountPopulation(mask);
      uint32_t mask_msb = base::bits::CountLeadingZeros32(mask);
      if ((mask_width != 0) && (mask_msb + mask_width == 32)) {
        uint32_t shift = m.right().ResolvedValue();
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

void InstructionSelector::VisitStackPointerGreaterThan(
    Node* node, FlagsContinuation* cont) {
  StackCheckKind kind = StackCheckKindOf(node->op());
  InstructionCode opcode =
      kArchStackPointerGreaterThan | MiscField::encode(static_cast<int>(kind));

  Arm64OperandGenerator g(this);

  // No outputs.
  InstructionOperand* const outputs = nullptr;
  const int output_count = 0;

  // Applying an offset to this stack check requires a temp register. Offsets
  // are only applied to the first stack check. If applying an offset, we must
  // ensure the input and temp registers do not alias, thus kUniqueRegister.
  InstructionOperand temps[] = {g.TempRegister()};
  const int temp_count = (kind == StackCheckKind::kJSFunctionEntry) ? 1 : 0;
  const auto register_mode = (kind == StackCheckKind::kJSFunctionEntry)
                                 ? OperandGenerator::kUniqueRegister
                                 : OperandGenerator::kRegister;

  Node* const value = node->InputAt(0);
  InstructionOperand inputs[] = {g.UseRegisterWithMode(value, register_mode)};
  static constexpr int input_count = arraysize(inputs);

  EmitWithContinuation(opcode, output_count, outputs, input_count, inputs,
                       temp_count, temps, cont);
}

namespace {

bool TryEmitBitfieldExtract32(InstructionSelector* selector, Node* node) {
  Arm64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  if (selector->CanCover(node, m.left().node()) && m.left().IsWord32Shl()) {
    // Select Ubfx or Sbfx for (x << (K & 0x1F)) OP (K & 0x1F), where
    // OP is >>> or >> and (K & 0x1F) != 0.
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasResolvedValue() && m.right().HasResolvedValue() &&
        (mleft.right().ResolvedValue() & 0x1F) != 0 &&
        (mleft.right().ResolvedValue() & 0x1F) ==
            (m.right().ResolvedValue() & 0x1F)) {
      DCHECK(m.IsWord32Shr() || m.IsWord32Sar());
      ArchOpcode opcode = m.IsWord32Sar() ? kArm64Sbfx32 : kArm64Ubfx32;

      int right_val = m.right().ResolvedValue() & 0x1F;
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
  if (m.left().IsWord32And() && m.right().HasResolvedValue()) {
    uint32_t lsb = m.right().ResolvedValue() & 0x1F;
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasResolvedValue() &&
        mleft.right().ResolvedValue() != 0) {
      // Select Ubfx for Shr(And(x, mask), imm) where the result of the mask is
      // shifted into the least-significant bits.
      uint32_t mask =
          static_cast<uint32_t>(mleft.right().ResolvedValue() >> lsb) << lsb;
      unsigned mask_width = base::bits::CountPopulation(mask);
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

  if (m.left().IsUint32MulHigh() && m.right().HasResolvedValue() &&
      CanCover(node, node->InputAt(0))) {
    // Combine this shift with the multiply and shift that would be generated
    // by Uint32MulHigh.
    Arm64OperandGenerator g(this);
    Node* left = m.left().node();
    int shift = m.right().ResolvedValue() & 0x1F;
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
  if (m.left().IsWord64And() && m.right().HasResolvedValue()) {
    uint32_t lsb = m.right().ResolvedValue() & 0x3F;
    Int64BinopMatcher mleft(m.left().node());
    if (mleft.right().HasResolvedValue() &&
        mleft.right().ResolvedValue() != 0) {
      // Select Ubfx for Shr(And(x, mask), imm) where the result of the mask is
      // shifted into the least-significant bits.
      uint64_t mask =
          static_cast<uint64_t>(mleft.right().ResolvedValue() >> lsb) << lsb;
      unsigned mask_width = base::bits::CountPopulation(mask);
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
  if (m.left().IsInt32MulHigh() && m.right().HasResolvedValue() &&
      CanCover(node, node->InputAt(0))) {
    // Combine this shift with the multiply and shift that would be generated
    // by Int32MulHigh.
    Arm64OperandGenerator g(this);
    Node* left = m.left().node();
    int shift = m.right().ResolvedValue() & 0x1F;
    InstructionOperand const smull_operand = g.TempRegister();
    Emit(kArm64Smull, smull_operand, g.UseRegister(left->InputAt(0)),
         g.UseRegister(left->InputAt(1)));
    Emit(kArm64Asr, g.DefineAsRegister(node), smull_operand,
         g.TempImmediate(32 + shift));
    return;
  }

  if (m.left().IsInt32Add() && m.right().HasResolvedValue() &&
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

void InstructionSelector::VisitWord32Rol(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord64Rol(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitRRO(this, kArm64Ror32, node, kShift32Imm);
}

void InstructionSelector::VisitWord64Ror(Node* node) {
  VisitRRO(this, kArm64Ror, node, kShift64Imm);
}

#define RR_OP_LIST(V)                                         \
  V(Word64Clz, kArm64Clz)                                     \
  V(Word32Clz, kArm64Clz32)                                   \
  V(Word32ReverseBits, kArm64Rbit32)                          \
  V(Word64ReverseBits, kArm64Rbit)                            \
  V(Word32ReverseBytes, kArm64Rev32)                          \
  V(Word64ReverseBytes, kArm64Rev)                            \
  V(ChangeFloat32ToFloat64, kArm64Float32ToFloat64)           \
  V(RoundInt32ToFloat32, kArm64Int32ToFloat32)                \
  V(RoundUint32ToFloat32, kArm64Uint32ToFloat32)              \
  V(ChangeInt32ToFloat64, kArm64Int32ToFloat64)               \
  V(ChangeInt64ToFloat64, kArm64Int64ToFloat64)               \
  V(ChangeUint32ToFloat64, kArm64Uint32ToFloat64)             \
  V(ChangeFloat64ToInt32, kArm64Float64ToInt32)               \
  V(ChangeFloat64ToInt64, kArm64Float64ToInt64)               \
  V(ChangeFloat64ToUint32, kArm64Float64ToUint32)             \
  V(ChangeFloat64ToUint64, kArm64Float64ToUint64)             \
  V(TruncateFloat64ToUint32, kArm64Float64ToUint32)           \
  V(TruncateFloat64ToFloat32, kArm64Float64ToFloat32)         \
  V(TruncateFloat64ToWord32, kArchTruncateDoubleToI)          \
  V(RoundFloat64ToInt32, kArm64Float64ToInt32)                \
  V(RoundInt64ToFloat32, kArm64Int64ToFloat32)                \
  V(RoundInt64ToFloat64, kArm64Int64ToFloat64)                \
  V(RoundUint64ToFloat32, kArm64Uint64ToFloat32)              \
  V(RoundUint64ToFloat64, kArm64Uint64ToFloat64)              \
  V(BitcastFloat32ToInt32, kArm64Float64ExtractLowWord32)     \
  V(BitcastFloat64ToInt64, kArm64U64MoveFloat64)              \
  V(BitcastInt32ToFloat32, kArm64Float64MoveU64)              \
  V(BitcastInt64ToFloat64, kArm64Float64MoveU64)              \
  V(Float32Sqrt, kArm64Float32Sqrt)                           \
  V(Float64Sqrt, kArm64Float64Sqrt)                           \
  V(Float32RoundDown, kArm64Float32RoundDown)                 \
  V(Float64RoundDown, kArm64Float64RoundDown)                 \
  V(Float32RoundUp, kArm64Float32RoundUp)                     \
  V(Float64RoundUp, kArm64Float64RoundUp)                     \
  V(Float32RoundTruncate, kArm64Float32RoundTruncate)         \
  V(Float64RoundTruncate, kArm64Float64RoundTruncate)         \
  V(Float64RoundTiesAway, kArm64Float64RoundTiesAway)         \
  V(Float32RoundTiesEven, kArm64Float32RoundTiesEven)         \
  V(Float64RoundTiesEven, kArm64Float64RoundTiesEven)         \
  V(Float64ExtractLowWord32, kArm64Float64ExtractLowWord32)   \
  V(Float64ExtractHighWord32, kArm64Float64ExtractHighWord32) \
  V(Float64SilenceNaN, kArm64Float64SilenceNaN)               \
  V(F32x4Ceil, kArm64Float32RoundUp)                          \
  V(F32x4Floor, kArm64Float32RoundDown)                       \
  V(F32x4Trunc, kArm64Float32RoundTruncate)                   \
  V(F32x4NearestInt, kArm64Float32RoundTiesEven)              \
  V(F64x2Ceil, kArm64Float64RoundUp)                          \
  V(F64x2Floor, kArm64Float64RoundDown)                       \
  V(F64x2Trunc, kArm64Float64RoundTruncate)                   \
  V(F64x2NearestInt, kArm64Float64RoundTiesEven)

#define RRR_OP_LIST(V)            \
  V(Int32Div, kArm64Idiv32)       \
  V(Int64Div, kArm64Idiv)         \
  V(Uint32Div, kArm64Udiv32)      \
  V(Uint64Div, kArm64Udiv)        \
  V(Int32Mod, kArm64Imod32)       \
  V(Int64Mod, kArm64Imod)         \
  V(Uint32Mod, kArm64Umod32)      \
  V(Uint64Mod, kArm64Umod)        \
  V(Float32Add, kArm64Float32Add) \
  V(Float64Add, kArm64Float64Add) \
  V(Float32Sub, kArm64Float32Sub) \
  V(Float64Sub, kArm64Float64Sub) \
  V(Float32Div, kArm64Float32Div) \
  V(Float64Div, kArm64Float64Div) \
  V(Float32Max, kArm64Float32Max) \
  V(Float64Max, kArm64Float64Max) \
  V(Float32Min, kArm64Float32Min) \
  V(Float64Min, kArm64Float64Min) \
  V(I8x16Swizzle, kArm64I8x16Swizzle)

#define RR_VISITOR(Name, opcode)                      \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRR(this, opcode, node);                      \
  }
RR_OP_LIST(RR_VISITOR)
#undef RR_VISITOR
#undef RR_OP_LIST

#define RRR_VISITOR(Name, opcode)                     \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRRR(this, opcode, node);                     \
  }
RRR_OP_LIST(RRR_VISITOR)
#undef RRR_VISITOR
#undef RRR_OP_LIST

void InstructionSelector::VisitWord32Ctz(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord64Ctz(Node* node) { UNREACHABLE(); }

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

  InstructionCode opcode =
      kArm64Cmp | AddressingModeField::encode(kMode_Operand2_R_SXTW);
  selector->EmitWithContinuation(opcode, result, result, cont);
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

namespace {
void VisitExtMul(InstructionSelector* selector, ArchOpcode opcode, Node* node,
                 int dst_lane_size) {
  InstructionCode code = opcode;
  code |= LaneSizeField::encode(dst_lane_size);
  VisitRRR(selector, code, node);
}
}  // namespace

void InstructionSelector::VisitI16x8ExtMulLowI8x16S(Node* node) {
  VisitExtMul(this, kArm64Smull, node, 16);
}

void InstructionSelector::VisitI16x8ExtMulHighI8x16S(Node* node) {
  VisitExtMul(this, kArm64Smull2, node, 16);
}

void InstructionSelector::VisitI16x8ExtMulLowI8x16U(Node* node) {
  VisitExtMul(this, kArm64Umull, node, 16);
}

void InstructionSelector::VisitI16x8ExtMulHighI8x16U(Node* node) {
  VisitExtMul(this, kArm64Umull2, node, 16);
}

void InstructionSelector::VisitI32x4ExtMulLowI16x8S(Node* node) {
  VisitExtMul(this, kArm64Smull, node, 32);
}

void InstructionSelector::VisitI32x4ExtMulHighI16x8S(Node* node) {
  VisitExtMul(this, kArm64Smull2, node, 32);
}

void InstructionSelector::VisitI32x4ExtMulLowI16x8U(Node* node) {
  VisitExtMul(this, kArm64Umull, node, 32);
}

void InstructionSelector::VisitI32x4ExtMulHighI16x8U(Node* node) {
  VisitExtMul(this, kArm64Umull2, node, 32);
}

void InstructionSelector::VisitI64x2ExtMulLowI32x4S(Node* node) {
  VisitExtMul(this, kArm64Smull, node, 64);
}

void InstructionSelector::VisitI64x2ExtMulHighI32x4S(Node* node) {
  VisitExtMul(this, kArm64Smull2, node, 64);
}

void InstructionSelector::VisitI64x2ExtMulLowI32x4U(Node* node) {
  VisitExtMul(this, kArm64Umull, node, 64);
}

void InstructionSelector::VisitI64x2ExtMulHighI32x4U(Node* node) {
  VisitExtMul(this, kArm64Umull2, node, 64);
}

namespace {
void VisitExtAddPairwise(InstructionSelector* selector, ArchOpcode opcode,
                         Node* node, int dst_lane_size) {
  InstructionCode code = opcode;
  code |= LaneSizeField::encode(dst_lane_size);
  VisitRR(selector, code, node);
}
}  // namespace

void InstructionSelector::VisitI32x4ExtAddPairwiseI16x8S(Node* node) {
  VisitExtAddPairwise(this, kArm64Saddlp, node, 32);
}

void InstructionSelector::VisitI32x4ExtAddPairwiseI16x8U(Node* node) {
  VisitExtAddPairwise(this, kArm64Uaddlp, node, 32);
}

void InstructionSelector::VisitI16x8ExtAddPairwiseI8x16S(Node* node) {
  VisitExtAddPairwise(this, kArm64Saddlp, node, 16);
}

void InstructionSelector::VisitI16x8ExtAddPairwiseI8x16U(Node* node) {
  VisitExtAddPairwise(this, kArm64Uaddlp, node, 16);
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

void InstructionSelector::VisitTruncateFloat32ToInt32(Node* node) {
  Arm64OperandGenerator g(this);

  InstructionCode opcode = kArm64Float32ToInt32;
  TruncateKind kind = OpParameter<TruncateKind>(node->op());
  opcode |= MiscField::encode(kind == TruncateKind::kSetOverflowToMin);

  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitTruncateFloat32ToUint32(Node* node) {
  Arm64OperandGenerator g(this);

  InstructionCode opcode = kArm64Float32ToUint32;
  TruncateKind kind = OpParameter<TruncateKind>(node->op());
  if (kind == TruncateKind::kSetOverflowToMin) {
    opcode |= MiscField::encode(true);
  }

  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
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

void InstructionSelector::VisitTruncateFloat64ToInt64(Node* node) {
  Arm64OperandGenerator g(this);

  InstructionCode opcode = kArm64Float64ToInt64;
  TruncateKind kind = OpParameter<TruncateKind>(node->op());
  if (kind == TruncateKind::kSetOverflowToMin) {
    opcode |= MiscField::encode(true);
  }

  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
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

void InstructionSelector::VisitBitcastWord32ToWord64(Node* node) {
  DCHECK(SmiValuesAre31Bits());
  DCHECK(COMPRESS_POINTERS_BOOL);
  EmitIdentity(node);
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
    }
    EmitLoad(this, value, opcode, immediate_mode, rep, node);
    return;
  }

  if (value->opcode() == IrOpcode::kWord32Sar && CanCover(node, value)) {
    Int32BinopMatcher m(value);
    if (m.right().HasResolvedValue()) {
      Arm64OperandGenerator g(this);
      // Mask the shift amount, to keep the same semantics as Word32Sar.
      int right = m.right().ResolvedValue() & 0x1F;
      Emit(kArm64Sbfx, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(right), g.TempImmediate(32 - right));
      return;
    }
  }

  VisitRR(this, kArm64Sxtw, node);
}

bool InstructionSelector::ZeroExtendsWord32ToWord64NoPhis(Node* node) {
  DCHECK_NE(node->opcode(), IrOpcode::kPhi);
  switch (node->opcode()) {
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
      return true;
    }
    case IrOpcode::kLoad:
    case IrOpcode::kLoadImmutable: {
      // As for the operations above, a 32-bit load will implicitly clear the
      // top 32 bits of the destination register.
      LoadRepresentation load_rep = LoadRepresentationOf(node->op());
      switch (load_rep.representation()) {
        case MachineRepresentation::kWord8:
        case MachineRepresentation::kWord16:
        case MachineRepresentation::kWord32:
          return true;
        default:
          return false;
      }
    }
    default:
      return false;
  }
}

void InstructionSelector::VisitChangeUint32ToUint64(Node* node) {
  Arm64OperandGenerator g(this);
  Node* value = node->InputAt(0);
  if (ZeroExtendsWord32ToWord64(value)) {
    return EmitIdentity(node);
  }
  Emit(kArm64Mov32, g.DefineAsRegister(node), g.UseRegister(value));
}

void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  Arm64OperandGenerator g(this);
  // The top 32 bits in the 64-bit register will be undefined, and
  // must not be used by a dependent node.
  EmitIdentity(node);
}

void InstructionSelector::VisitFloat64Mod(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64Float64Mod, g.DefineAsFixed(node, d0),
       g.UseFixed(node->InputAt(0), d0), g.UseFixed(node->InputAt(1), d1))
      ->MarkAsCall();
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
    ZoneVector<PushParameter>* arguments, const CallDescriptor* call_descriptor,
    Node* node) {
  Arm64OperandGenerator g(this);

  // `arguments` includes alignment "holes". This means that slots bigger than
  // kSystemPointerSize, e.g. Simd128, will span across multiple arguments.
  int claim_count = static_cast<int>(arguments->size());
  bool needs_padding = claim_count % 2 != 0;
  int slot = claim_count - 1;
  claim_count = RoundUp(claim_count, 2);
  // Bump the stack pointer.
  if (claim_count > 0) {
    // TODO(titzer): claim and poke probably take small immediates.
    // TODO(titzer): it would be better to bump the sp here only
    //               and emit paired stores with increment for non c frames.
    Emit(kArm64Claim, g.NoOutput(), g.TempImmediate(claim_count));

    if (needs_padding) {
      Emit(kArm64Poke, g.NoOutput(), g.UseImmediate(0),
           g.TempImmediate(claim_count - 1));
    }
  }

  // Poke the arguments into the stack.
  while (slot >= 0) {
    PushParameter input0 = (*arguments)[slot];
    // Skip holes in the param array. These represent both extra slots for
    // multi-slot values and padding slots for alignment.
    if (input0.node == nullptr) {
      slot--;
      continue;
    }
    PushParameter input1 = slot > 0 ? (*arguments)[slot - 1] : PushParameter();
    // Emit a poke-pair if consecutive parameters have the same type.
    // TODO(arm): Support consecutive Simd128 parameters.
    if (input1.node != nullptr &&
        input0.location.GetType() == input1.location.GetType()) {
      Emit(kArm64PokePair, g.NoOutput(), g.UseRegister(input0.node),
           g.UseRegister(input1.node), g.TempImmediate(slot));
      slot -= 2;
    } else {
      Emit(kArm64Poke, g.NoOutput(), g.UseRegister(input0.node),
           g.TempImmediate(slot));
      slot--;
    }
  }
}

void InstructionSelector::EmitPrepareResults(
    ZoneVector<PushParameter>* results, const CallDescriptor* call_descriptor,
    Node* node) {
  Arm64OperandGenerator g(this);

  for (PushParameter output : *results) {
    if (!output.location.IsCallerFrameSlot()) continue;
    // Skip any alignment holes in nodes.
    if (output.node != nullptr) {
      DCHECK(!call_descriptor->IsCFunctionCall());

      if (output.location.GetType() == MachineType::Float32()) {
        MarkAsFloat32(output.node);
      } else if (output.location.GetType() == MachineType::Float64()) {
        MarkAsFloat64(output.node);
      } else if (output.location.GetType() == MachineType::Simd128()) {
        MarkAsSimd128(output.node);
      }

      int offset = call_descriptor->GetOffsetToReturns();
      int reverse_slot = -output.location.GetLocation() - offset;
      Emit(kArm64Peek, g.DefineAsRegister(output.node),
           g.UseImmediate(reverse_slot));
    }
  }
}

bool InstructionSelector::IsTailCallAddressImmediate() { return false; }

namespace {

// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  InstructionOperand left, InstructionOperand right,
                  FlagsContinuation* cont) {
  if (cont->IsSelect()) {
    Arm64OperandGenerator g(selector);
    InstructionOperand inputs[] = { left, right,
                                    g.UseRegister(cont->true_value()),
                                    g.UseRegister(cont->false_value()) };
    selector->EmitWithContinuation(opcode, 0, nullptr, 4, inputs, cont);
  } else {
    selector->EmitWithContinuation(opcode, left, right, cont);
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
  }
}

void EmitBranchOrDeoptimize(InstructionSelector* selector,
                            InstructionCode opcode, InstructionOperand value,
                            FlagsContinuation* cont) {
  DCHECK(cont->IsBranch() || cont->IsDeoptimize());
  selector->EmitWithContinuation(opcode, value, cont);
}

template <int N>
struct CbzOrTbzMatchTrait {};

template <>
struct CbzOrTbzMatchTrait<32> {
  using IntegralType = uint32_t;
  using BinopMatcher = Int32BinopMatcher;
  static constexpr IrOpcode::Value kAndOpcode = IrOpcode::kWord32And;
  static constexpr ArchOpcode kTestAndBranchOpcode = kArm64TestAndBranch32;
  static constexpr ArchOpcode kCompareAndBranchOpcode =
      kArm64CompareAndBranch32;
  static constexpr unsigned kSignBit = kWSignBit;
};

template <>
struct CbzOrTbzMatchTrait<64> {
  using IntegralType = uint64_t;
  using BinopMatcher = Int64BinopMatcher;
  static constexpr IrOpcode::Value kAndOpcode = IrOpcode::kWord64And;
  static constexpr ArchOpcode kTestAndBranchOpcode = kArm64TestAndBranch;
  static constexpr ArchOpcode kCompareAndBranchOpcode = kArm64CompareAndBranch;
  static constexpr unsigned kSignBit = kXSignBit;
};

// Try to emit TBZ, TBNZ, CBZ or CBNZ for certain comparisons of {node}
// against {value}, depending on the condition.
template <int N>
bool TryEmitCbzOrTbz(InstructionSelector* selector, Node* node,
                     typename CbzOrTbzMatchTrait<N>::IntegralType value,
                     Node* user, FlagsCondition cond, FlagsContinuation* cont) {
  // Branch poisoning requires flags to be set, so when it's enabled for
  // a particular branch, we shouldn't be applying the cbz/tbz optimization.
  DCHECK(!cont->IsPoisoned());
  // Only handle branches and deoptimisations.
  if (!cont->IsBranch() && !cont->IsDeoptimize()) return false;

  switch (cond) {
    case kSignedLessThan:
    case kSignedGreaterThanOrEqual: {
      // Here we handle sign tests, aka. comparisons with zero.
      if (value != 0) return false;
      // We don't generate TBZ/TBNZ for deoptimisations, as they have a
      // shorter range than conditional branches and generating them for
      // deoptimisations results in more veneers.
      if (cont->IsDeoptimize()) return false;
      Arm64OperandGenerator g(selector);
      cont->Overwrite(MapForTbz(cond));

      if (N == 32) {
        Int32Matcher m(node);
        if (m.IsFloat64ExtractHighWord32() && selector->CanCover(user, node)) {
          // SignedLessThan(Float64ExtractHighWord32(x), 0) and
          // SignedGreaterThanOrEqual(Float64ExtractHighWord32(x), 0)
          // essentially check the sign bit of a 64-bit floating point value.
          InstructionOperand temp = g.TempRegister();
          selector->Emit(kArm64U64MoveFloat64, temp,
                         g.UseRegister(node->InputAt(0)));
          selector->EmitWithContinuation(kArm64TestAndBranch, temp,
                                         g.TempImmediate(kDSignBit), cont);
          return true;
        }
      }

      selector->EmitWithContinuation(
          CbzOrTbzMatchTrait<N>::kTestAndBranchOpcode, g.UseRegister(node),
          g.TempImmediate(CbzOrTbzMatchTrait<N>::kSignBit), cont);
      return true;
    }
    case kEqual:
    case kNotEqual: {
      if (node->opcode() == CbzOrTbzMatchTrait<N>::kAndOpcode) {
        // Emit a tbz/tbnz if we are comparing with a single-bit mask:
        //   Branch(WordEqual(WordAnd(x, 1 << N), 1 << N), true, false)
        typename CbzOrTbzMatchTrait<N>::BinopMatcher m_and(node);
        if (cont->IsBranch() && base::bits::IsPowerOfTwo(value) &&
            m_and.right().Is(value) && selector->CanCover(user, node)) {
          Arm64OperandGenerator g(selector);
          // In the code generator, Equal refers to a bit being cleared. We want
          // the opposite here so negate the condition.
          cont->Negate();
          selector->EmitWithContinuation(
              CbzOrTbzMatchTrait<N>::kTestAndBranchOpcode,
              g.UseRegister(m_and.left().node()),
              g.TempImmediate(base::bits::CountTrailingZeros(value)), cont);
          return true;
        }
      }
      V8_FALLTHROUGH;
    }
    case kUnsignedLessThanOrEqual:
    case kUnsignedGreaterThan: {
      if (value != 0) return false;
      Arm64OperandGenerator g(selector);
      cont->Overwrite(MapForCbz(cond));
      EmitBranchOrDeoptimize(selector,
                             CbzOrTbzMatchTrait<N>::kCompareAndBranchOpcode,
                             g.UseRegister(node), cont);
      return true;
    }
    default:
      return false;
  }
}

// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont,
                      ImmediateMode immediate_mode) {
  Arm64OperandGenerator g(selector);

  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  // If one of the two inputs is an immediate, make sure it's on the right.
  if (!g.CanBeImmediate(right, immediate_mode) &&
      g.CanBeImmediate(left, immediate_mode)) {
    cont->Commute();
    std::swap(left, right);
  }

  if (opcode == kArm64Cmp && !cont->IsPoisoned()) {
    Int64Matcher m(right);
    if (m.HasResolvedValue()) {
      if (TryEmitCbzOrTbz<64>(selector, left, m.ResolvedValue(), node,
                              cont->condition(), cont)) {
        return;
      }
    }
  }

  VisitCompare(selector, opcode, g.UseRegister(left),
               g.UseOperand(right, immediate_mode), cont);
}

void VisitWord32Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  Int32BinopMatcher m(node);
  FlagsCondition cond = cont->condition();
  if (!cont->IsPoisoned()) {
    if (m.right().HasResolvedValue()) {
      if (TryEmitCbzOrTbz<32>(selector, m.left().node(),
                              m.right().ResolvedValue(), node, cond, cont)) {
        return;
      }
    } else if (m.left().HasResolvedValue()) {
      FlagsCondition commuted_cond = CommuteFlagsCondition(cond);
      if (TryEmitCbzOrTbz<32>(selector, m.right().node(),
                              m.left().ResolvedValue(), node, commuted_cond,
                              cont)) {
        return;
      }
    }
  }
  ArchOpcode opcode = kArm64Cmp32;
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

template <typename Matcher>
struct TestAndBranchMatcher {
  TestAndBranchMatcher(Node* node, FlagsContinuation* cont)
      : matches_(false), cont_(cont), matcher_(node) {
    Initialize();
  }
  bool Matches() const { return matches_; }

  unsigned bit() const {
    DCHECK(Matches());
    return base::bits::CountTrailingZeros(matcher_.right().ResolvedValue());
  }

  Node* input() const {
    DCHECK(Matches());
    return matcher_.left().node();
  }

 private:
  bool matches_;
  FlagsContinuation* cont_;
  Matcher matcher_;

  void Initialize() {
    if (cont_->IsBranch() && !cont_->IsPoisoned() &&
        matcher_.right().HasResolvedValue() &&
        base::bits::IsPowerOfTwo(matcher_.right().ResolvedValue())) {
      // If the mask has only one bit set, we can use tbz/tbnz.
      DCHECK((cont_->condition() == kEqual) ||
             (cont_->condition() == kNotEqual));
      matches_ = true;
    } else {
      matches_ = false;
    }
  }
};

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

void VisitAtomicExchange(InstructionSelector* selector, Node* node,
                         ArchOpcode opcode) {
  Arm64OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index),
                                 g.UseUniqueRegister(value)};
  InstructionOperand outputs[] = {g.DefineAsRegister(node)};
  InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
  InstructionCode code = opcode | AddressingModeField::encode(kMode_MRR);
  selector->Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
                 arraysize(temps), temps);
}

void VisitAtomicCompareExchange(InstructionSelector* selector, Node* node,
                                ArchOpcode opcode) {
  Arm64OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* old_value = node->InputAt(2);
  Node* new_value = node->InputAt(3);
  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index),
                                 g.UseUniqueRegister(old_value),
                                 g.UseUniqueRegister(new_value)};
  InstructionOperand outputs[] = {g.DefineAsRegister(node)};
  InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
  InstructionCode code = opcode | AddressingModeField::encode(kMode_MRR);
  selector->Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
                 arraysize(temps), temps);
}

void VisitAtomicLoad(InstructionSelector* selector, Node* node,
                     ArchOpcode opcode) {
  Arm64OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index)};
  InstructionOperand outputs[] = {g.DefineAsRegister(node)};
  InstructionOperand temps[] = {g.TempRegister()};
  InstructionCode code = opcode | AddressingModeField::encode(kMode_MRR);
  selector->Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
                 arraysize(temps), temps);
}

void VisitAtomicStore(InstructionSelector* selector, Node* node,
                      ArchOpcode opcode) {
  Arm64OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index),
                                 g.UseUniqueRegister(value)};
  InstructionOperand temps[] = {g.TempRegister()};
  InstructionCode code = opcode | AddressingModeField::encode(kMode_MRR);
  selector->Emit(code, 0, nullptr, arraysize(inputs), inputs, arraysize(temps),
                 temps);
}

void VisitAtomicBinop(InstructionSelector* selector, Node* node,
                      ArchOpcode opcode) {
  Arm64OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  AddressingMode addressing_mode = kMode_MRR;
  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index),
                                 g.UseUniqueRegister(value)};
  InstructionOperand outputs[] = {g.DefineAsRegister(node)};
  InstructionOperand temps[] = {g.TempRegister(), g.TempRegister(),
                                g.TempRegister()};
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  selector->Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
                 arraysize(temps), temps);
}

}  // namespace

void InstructionSelector::VisitWordCompareZero(Node* user, Node* value,
                                               FlagsContinuation* cont) {
  Arm64OperandGenerator g(this);
  // Try to combine with comparisons against 0 by simply inverting the branch.
  while (value->opcode() == IrOpcode::kWord32Equal && CanCover(user, value)) {
    Int32BinopMatcher m(value);
    if (!m.right().Is(0)) break;

    user = value;
    value = m.left().node();
    cont->Negate();
  }

  // Try to match bit checks to create TBZ/TBNZ instructions.
  // Unlike the switch below, CanCover check is not needed here.
  // If there are several uses of the given operation, we will generate a TBZ
  // instruction for each. This is useful even if there are other uses of the
  // arithmetic result, because it moves dependencies further back.
  switch (value->opcode()) {
    case IrOpcode::kWord64Equal: {
      Int64BinopMatcher m(value);
      if (m.right().Is(0)) {
        Node* const left = m.left().node();
        if (left->opcode() == IrOpcode::kWord64And) {
          // Attempt to merge the Word64Equal(Word64And(x, y), 0) comparison
          // into a tbz/tbnz instruction.
          TestAndBranchMatcher<Uint64BinopMatcher> tbm(left, cont);
          if (tbm.Matches()) {
            Arm64OperandGenerator gen(this);
            cont->OverwriteAndNegateIfEqual(kEqual);
            this->EmitWithContinuation(kArm64TestAndBranch,
                                       gen.UseRegister(tbm.input()),
                                       gen.TempImmediate(tbm.bit()), cont);
            return;
          }
        }
      }
      break;
    }
    case IrOpcode::kWord32And: {
      TestAndBranchMatcher<Uint32BinopMatcher> tbm(value, cont);
      if (tbm.Matches()) {
        Arm64OperandGenerator gen(this);
        this->EmitWithContinuation(kArm64TestAndBranch32,
                                   gen.UseRegister(tbm.input()),
                                   gen.TempImmediate(tbm.bit()), cont);
        return;
      }
      break;
    }
    case IrOpcode::kWord64And: {
      TestAndBranchMatcher<Uint64BinopMatcher> tbm(value, cont);
      if (tbm.Matches()) {
        Arm64OperandGenerator gen(this);
        this->EmitWithContinuation(kArm64TestAndBranch,
                                   gen.UseRegister(tbm.input()),
                                   gen.TempImmediate(tbm.bit()), cont);
        return;
      }
      break;
    }
    default:
      break;
  }

  if (CanCover(user, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord32Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitWord32Compare(this, value, cont);
      case IrOpcode::kInt32LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWord32Compare(this, value, cont);
      case IrOpcode::kInt32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWord32Compare(this, value, cont);
      case IrOpcode::kUint32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWord32Compare(this, value, cont);
      case IrOpcode::kUint32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWord32Compare(this, value, cont);
      case IrOpcode::kWord64Equal: {
        cont->OverwriteAndNegateIfEqual(kEqual);
        Int64BinopMatcher m(value);
        if (m.right().Is(0)) {
          Node* const left = m.left().node();
          if (CanCover(value, left) && left->opcode() == IrOpcode::kWord64And) {
            return VisitWordCompare(this, left, kArm64Tst, cont, kLogical64Imm);
          }
        }
        return VisitWordCompare(this, value, kArm64Cmp, cont, kArithmeticImm);
      }
      case IrOpcode::kInt64LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWordCompare(this, value, kArm64Cmp, cont, kArithmeticImm);
      case IrOpcode::kInt64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWordCompare(this, value, kArm64Cmp, cont, kArithmeticImm);
      case IrOpcode::kUint64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWordCompare(this, value, kArm64Cmp, cont, kArithmeticImm);
      case IrOpcode::kUint64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWordCompare(this, value, kArm64Cmp, cont, kArithmeticImm);
      case IrOpcode::kFloat32Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat32LessThan:
        cont->OverwriteAndNegateIfEqual(kFloatLessThan);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kFloatLessThanOrEqual);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat64Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitFloat64Compare(this, value, cont);
      case IrOpcode::kFloat64LessThan:
        cont->OverwriteAndNegateIfEqual(kFloatLessThan);
        return VisitFloat64Compare(this, value, cont);
      case IrOpcode::kFloat64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kFloatLessThanOrEqual);
        return VisitFloat64Compare(this, value, cont);
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
          if (result == nullptr || IsDefined(result)) {
            switch (node->opcode()) {
              case IrOpcode::kInt32AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int32BinopMatcher>(this, node, kArm64Add32,
                                                     kArithmeticImm, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int32BinopMatcher>(this, node, kArm64Sub32,
                                                     kArithmeticImm, cont);
              case IrOpcode::kInt32MulWithOverflow:
                // ARM64 doesn't set the overflow flag for multiplication, so we
                // need to test on kNotEqual. Here is the code sequence used:
                //   smull result, left, right
                //   cmp result.X(), Operand(result, SXTW)
                cont->OverwriteAndNegateIfEqual(kNotEqual);
                return EmitInt32MulWithOverflow(this, node, cont);
              case IrOpcode::kInt64AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int64BinopMatcher>(this, node, kArm64Add,
                                                     kArithmeticImm, cont);
              case IrOpcode::kInt64SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int64BinopMatcher>(this, node, kArm64Sub,
                                                     kArithmeticImm, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Add:
        return VisitWordCompare(this, value, kArm64Cmn32, cont, kArithmeticImm);
      case IrOpcode::kInt32Sub:
        return VisitWord32Compare(this, value, cont);
      case IrOpcode::kWord32And:
        return VisitWordCompare(this, value, kArm64Tst32, cont, kLogical32Imm);
      case IrOpcode::kWord64And:
        return VisitWordCompare(this, value, kArm64Tst, cont, kLogical64Imm);
      case IrOpcode::kStackPointerGreaterThan:
        cont->OverwriteAndNegateIfEqual(kStackPointerGreaterThanCondition);
        return VisitStackPointerGreaterThan(value, cont);
      default:
        break;
    }
  }

  // Branch could not be combined with a compare, compare against 0 and branch.
  if (!cont->IsPoisoned() && cont->IsBranch()) {
    Emit(cont->Encode(kArm64CompareAndBranch32), g.NoOutput(),
         g.UseRegister(value), g.Label(cont->true_block()),
         g.Label(cont->false_block()));
  } else {
    VisitCompare(this, cont->Encode(kArm64Tst32), g.UseRegister(value),
                 g.UseRegister(value), cont);
  }
}

void InstructionSelector::VisitSwitch(Node* node, const SwitchInfo& sw) {
  Arm64OperandGenerator g(this);
  InstructionOperand value_operand = g.UseRegister(node->InputAt(0));

  // Emit either ArchTableSwitch or ArchBinarySearchSwitch.
  if (enable_switch_jump_table_ == kEnableSwitchJumpTable) {
    static const size_t kMaxTableSwitchValueRange = 2 << 16;
    size_t table_space_cost = 4 + sw.value_range();
    size_t table_time_cost = 3;
    size_t lookup_space_cost = 3 + 2 * sw.case_count();
    size_t lookup_time_cost = sw.case_count();
    if (sw.case_count() > 4 &&
        table_space_cost + 3 * table_time_cost <=
            lookup_space_cost + 3 * lookup_time_cost &&
        sw.min_value() > std::numeric_limits<int32_t>::min() &&
        sw.value_range() <= kMaxTableSwitchValueRange) {
      InstructionOperand index_operand = value_operand;
      if (sw.min_value()) {
        index_operand = g.TempRegister();
        Emit(kArm64Sub32, index_operand, value_operand,
             g.TempImmediate(sw.min_value()));
      }
      // Generate a table lookup.
      return EmitTableSwitch(sw, index_operand);
    }
  }

  // Generate a tree of conditional jumps.
  return EmitBinarySearchSwitch(sw, value_operand);
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
          return VisitWordCompare(this, value, kArm64Cmp32, &cont,
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
          return VisitWordCompare(this, value, kArm64Tst, &cont, kLogical64Imm);
        default:
          break;
      }
      return VisitWord64Test(this, value, &cont);
    }
  }
  VisitWordCompare(this, node, kArm64Cmp, &cont, kArithmeticImm);
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
  VisitWordCompare(this, node, kArm64Cmp, &cont, kArithmeticImm);
}

void InstructionSelector::VisitInt64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kArm64Cmp, &cont, kArithmeticImm);
}

void InstructionSelector::VisitUint64LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWordCompare(this, node, kArm64Cmp, &cont, kArithmeticImm);
}

void InstructionSelector::VisitUint64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kArm64Cmp, &cont, kArithmeticImm);
}

void InstructionSelector::VisitFloat32Neg(Node* node) {
  Arm64OperandGenerator g(this);
  Node* in = node->InputAt(0);
  if (in->opcode() == IrOpcode::kFloat32Mul && CanCover(node, in)) {
    Float32BinopMatcher m(in);
    Emit(kArm64Float32Fnmul, g.DefineAsRegister(node),
         g.UseRegister(m.left().node()), g.UseRegister(m.right().node()));
    return;
  }
  VisitRR(this, kArm64Float32Neg, node);
}

void InstructionSelector::VisitFloat32Mul(Node* node) {
  Arm64OperandGenerator g(this);
  Float32BinopMatcher m(node);

  if (m.left().IsFloat32Neg() && CanCover(node, m.left().node())) {
    Emit(kArm64Float32Fnmul, g.DefineAsRegister(node),
         g.UseRegister(m.left().node()->InputAt(0)),
         g.UseRegister(m.right().node()));
    return;
  }

  if (m.right().IsFloat32Neg() && CanCover(node, m.right().node())) {
    Emit(kArm64Float32Fnmul, g.DefineAsRegister(node),
         g.UseRegister(m.right().node()->InputAt(0)),
         g.UseRegister(m.left().node()));
    return;
  }
  return VisitRRR(this, kArm64Float32Mul, node);
}

void InstructionSelector::VisitFloat32Abs(Node* node) {
  Arm64OperandGenerator g(this);
  Node* in = node->InputAt(0);
  if (in->opcode() == IrOpcode::kFloat32Sub && CanCover(node, in)) {
    Emit(kArm64Float32Abd, g.DefineAsRegister(node),
         g.UseRegister(in->InputAt(0)), g.UseRegister(in->InputAt(1)));
    return;
  }

  return VisitRR(this, kArm64Float32Abs, node);
}

void InstructionSelector::VisitFloat64Abs(Node* node) {
  Arm64OperandGenerator g(this);
  Node* in = node->InputAt(0);
  if (in->opcode() == IrOpcode::kFloat64Sub && CanCover(node, in)) {
    Emit(kArm64Float64Abd, g.DefineAsRegister(node),
         g.UseRegister(in->InputAt(0)), g.UseRegister(in->InputAt(1)));
    return;
  }

  return VisitRR(this, kArm64Float64Abs, node);
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
  Emit(kArm64Float64InsertLowWord32, g.DefineSameAsFirst(node),
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
  Emit(kArm64Float64InsertHighWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.UseRegister(right));
}

void InstructionSelector::VisitFloat64Neg(Node* node) {
  Arm64OperandGenerator g(this);
  Node* in = node->InputAt(0);
  if (in->opcode() == IrOpcode::kFloat64Mul && CanCover(node, in)) {
    Float64BinopMatcher m(in);
    Emit(kArm64Float64Fnmul, g.DefineAsRegister(node),
         g.UseRegister(m.left().node()), g.UseRegister(m.right().node()));
    return;
  }
  VisitRR(this, kArm64Float64Neg, node);
}

void InstructionSelector::VisitFloat64Mul(Node* node) {
  Arm64OperandGenerator g(this);
  Float64BinopMatcher m(node);

  if (m.left().IsFloat64Neg() && CanCover(node, m.left().node())) {
    Emit(kArm64Float64Fnmul, g.DefineAsRegister(node),
         g.UseRegister(m.left().node()->InputAt(0)),
         g.UseRegister(m.right().node()));
    return;
  }

  if (m.right().IsFloat64Neg() && CanCover(node, m.right().node())) {
    Emit(kArm64Float64Fnmul, g.DefineAsRegister(node),
         g.UseRegister(m.right().node()->InputAt(0)),
         g.UseRegister(m.left().node()));
    return;
  }
  return VisitRRR(this, kArm64Float64Mul, node);
}

void InstructionSelector::VisitMemoryBarrier(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64DmbIsh, g.NoOutput());
}

void InstructionSelector::VisitWord32AtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  ArchOpcode opcode;
  switch (load_rep.representation()) {
    case MachineRepresentation::kWord8:
      opcode =
          load_rep.IsSigned() ? kWord32AtomicLoadInt8 : kWord32AtomicLoadUint8;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsSigned() ? kWord32AtomicLoadInt16
                                   : kWord32AtomicLoadUint16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kWord32AtomicLoadWord32;
      break;
    default:
      UNREACHABLE();
  }
  VisitAtomicLoad(this, node, opcode);
}

void InstructionSelector::VisitWord64AtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  ArchOpcode opcode;
  switch (load_rep.representation()) {
    case MachineRepresentation::kWord8:
      opcode = kArm64Word64AtomicLoadUint8;
      break;
    case MachineRepresentation::kWord16:
      opcode = kArm64Word64AtomicLoadUint16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kArm64Word64AtomicLoadUint32;
      break;
    case MachineRepresentation::kWord64:
      opcode = kArm64Word64AtomicLoadUint64;
      break;
    default:
      UNREACHABLE();
  }
  VisitAtomicLoad(this, node, opcode);
}

void InstructionSelector::VisitWord32AtomicStore(Node* node) {
  MachineRepresentation rep = AtomicStoreRepresentationOf(node->op());
  ArchOpcode opcode;
  switch (rep) {
    case MachineRepresentation::kWord8:
      opcode = kWord32AtomicStoreWord8;
      break;
    case MachineRepresentation::kWord16:
      opcode = kWord32AtomicStoreWord16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kWord32AtomicStoreWord32;
      break;
    default:
      UNREACHABLE();
  }
  VisitAtomicStore(this, node, opcode);
}

void InstructionSelector::VisitWord64AtomicStore(Node* node) {
  MachineRepresentation rep = AtomicStoreRepresentationOf(node->op());
  ArchOpcode opcode;
  switch (rep) {
    case MachineRepresentation::kWord8:
      opcode = kArm64Word64AtomicStoreWord8;
      break;
    case MachineRepresentation::kWord16:
      opcode = kArm64Word64AtomicStoreWord16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kArm64Word64AtomicStoreWord32;
      break;
    case MachineRepresentation::kWord64:
      opcode = kArm64Word64AtomicStoreWord64;
      break;
    default:
      UNREACHABLE();
  }
  VisitAtomicStore(this, node, opcode);
}

void InstructionSelector::VisitWord32AtomicExchange(Node* node) {
  ArchOpcode opcode;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Int8()) {
    opcode = kWord32AtomicExchangeInt8;
  } else if (type == MachineType::Uint8()) {
    opcode = kWord32AtomicExchangeUint8;
  } else if (type == MachineType::Int16()) {
    opcode = kWord32AtomicExchangeInt16;
  } else if (type == MachineType::Uint16()) {
    opcode = kWord32AtomicExchangeUint16;
  } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
    opcode = kWord32AtomicExchangeWord32;
  } else {
    UNREACHABLE();
  }
  VisitAtomicExchange(this, node, opcode);
}

void InstructionSelector::VisitWord64AtomicExchange(Node* node) {
  ArchOpcode opcode;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Uint8()) {
    opcode = kArm64Word64AtomicExchangeUint8;
  } else if (type == MachineType::Uint16()) {
    opcode = kArm64Word64AtomicExchangeUint16;
  } else if (type == MachineType::Uint32()) {
    opcode = kArm64Word64AtomicExchangeUint32;
  } else if (type == MachineType::Uint64()) {
    opcode = kArm64Word64AtomicExchangeUint64;
  } else {
    UNREACHABLE();
  }
  VisitAtomicExchange(this, node, opcode);
}

void InstructionSelector::VisitWord32AtomicCompareExchange(Node* node) {
  ArchOpcode opcode;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Int8()) {
    opcode = kWord32AtomicCompareExchangeInt8;
  } else if (type == MachineType::Uint8()) {
    opcode = kWord32AtomicCompareExchangeUint8;
  } else if (type == MachineType::Int16()) {
    opcode = kWord32AtomicCompareExchangeInt16;
  } else if (type == MachineType::Uint16()) {
    opcode = kWord32AtomicCompareExchangeUint16;
  } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
    opcode = kWord32AtomicCompareExchangeWord32;
  } else {
    UNREACHABLE();
  }
  VisitAtomicCompareExchange(this, node, opcode);
}

void InstructionSelector::VisitWord64AtomicCompareExchange(Node* node) {
  ArchOpcode opcode;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Uint8()) {
    opcode = kArm64Word64AtomicCompareExchangeUint8;
  } else if (type == MachineType::Uint16()) {
    opcode = kArm64Word64AtomicCompareExchangeUint16;
  } else if (type == MachineType::Uint32()) {
    opcode = kArm64Word64AtomicCompareExchangeUint32;
  } else if (type == MachineType::Uint64()) {
    opcode = kArm64Word64AtomicCompareExchangeUint64;
  } else {
    UNREACHABLE();
  }
  VisitAtomicCompareExchange(this, node, opcode);
}

void InstructionSelector::VisitWord32AtomicBinaryOperation(
    Node* node, ArchOpcode int8_op, ArchOpcode uint8_op, ArchOpcode int16_op,
    ArchOpcode uint16_op, ArchOpcode word32_op) {
  ArchOpcode opcode;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Int8()) {
    opcode = int8_op;
  } else if (type == MachineType::Uint8()) {
    opcode = uint8_op;
  } else if (type == MachineType::Int16()) {
    opcode = int16_op;
  } else if (type == MachineType::Uint16()) {
    opcode = uint16_op;
  } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
    opcode = word32_op;
  } else {
    UNREACHABLE();
  }
  VisitAtomicBinop(this, node, opcode);
}

#define VISIT_ATOMIC_BINOP(op)                                   \
  void InstructionSelector::VisitWord32Atomic##op(Node* node) {  \
    VisitWord32AtomicBinaryOperation(                            \
        node, kWord32Atomic##op##Int8, kWord32Atomic##op##Uint8, \
        kWord32Atomic##op##Int16, kWord32Atomic##op##Uint16,     \
        kWord32Atomic##op##Word32);                              \
  }
VISIT_ATOMIC_BINOP(Add)
VISIT_ATOMIC_BINOP(Sub)
VISIT_ATOMIC_BINOP(And)
VISIT_ATOMIC_BINOP(Or)
VISIT_ATOMIC_BINOP(Xor)
#undef VISIT_ATOMIC_BINOP

void InstructionSelector::VisitWord64AtomicBinaryOperation(
    Node* node, ArchOpcode uint8_op, ArchOpcode uint16_op, ArchOpcode uint32_op,
    ArchOpcode uint64_op) {
  ArchOpcode opcode;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Uint8()) {
    opcode = uint8_op;
  } else if (type == MachineType::Uint16()) {
    opcode = uint16_op;
  } else if (type == MachineType::Uint32()) {
    opcode = uint32_op;
  } else if (type == MachineType::Uint64()) {
    opcode = uint64_op;
  } else {
    UNREACHABLE();
  }
  VisitAtomicBinop(this, node, opcode);
}

#define VISIT_ATOMIC_BINOP(op)                                               \
  void InstructionSelector::VisitWord64Atomic##op(Node* node) {              \
    VisitWord64AtomicBinaryOperation(                                        \
        node, kArm64Word64Atomic##op##Uint8, kArm64Word64Atomic##op##Uint16, \
        kArm64Word64Atomic##op##Uint32, kArm64Word64Atomic##op##Uint64);     \
  }
VISIT_ATOMIC_BINOP(Add)
VISIT_ATOMIC_BINOP(Sub)
VISIT_ATOMIC_BINOP(And)
VISIT_ATOMIC_BINOP(Or)
VISIT_ATOMIC_BINOP(Xor)
#undef VISIT_ATOMIC_BINOP

void InstructionSelector::VisitInt32AbsWithOverflow(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitInt64AbsWithOverflow(Node* node) {
  UNREACHABLE();
}

#define SIMD_TYPE_LIST(V) \
  V(F64x2)                \
  V(F32x4)                \
  V(I64x2)                \
  V(I32x4)                \
  V(I16x8)                \
  V(I8x16)

#define SIMD_UNOP_LIST(V)                                   \
  V(F64x2Abs, kArm64F64x2Abs)                               \
  V(F64x2Neg, kArm64F64x2Neg)                               \
  V(F64x2Sqrt, kArm64F64x2Sqrt)                             \
  V(F64x2ConvertLowI32x4S, kArm64F64x2ConvertLowI32x4S)     \
  V(F64x2ConvertLowI32x4U, kArm64F64x2ConvertLowI32x4U)     \
  V(F64x2PromoteLowF32x4, kArm64F64x2PromoteLowF32x4)       \
  V(F32x4SConvertI32x4, kArm64F32x4SConvertI32x4)           \
  V(F32x4UConvertI32x4, kArm64F32x4UConvertI32x4)           \
  V(F32x4Abs, kArm64F32x4Abs)                               \
  V(F32x4Neg, kArm64F32x4Neg)                               \
  V(F32x4Sqrt, kArm64F32x4Sqrt)                             \
  V(F32x4RecipApprox, kArm64F32x4RecipApprox)               \
  V(F32x4RecipSqrtApprox, kArm64F32x4RecipSqrtApprox)       \
  V(F32x4DemoteF64x2Zero, kArm64F32x4DemoteF64x2Zero)       \
  V(I64x2Abs, kArm64I64x2Abs)                               \
  V(I64x2Neg, kArm64I64x2Neg)                               \
  V(I64x2BitMask, kArm64I64x2BitMask)                       \
  V(I32x4SConvertF32x4, kArm64I32x4SConvertF32x4)           \
  V(I32x4Neg, kArm64I32x4Neg)                               \
  V(I32x4UConvertF32x4, kArm64I32x4UConvertF32x4)           \
  V(I32x4Abs, kArm64I32x4Abs)                               \
  V(I32x4BitMask, kArm64I32x4BitMask)                       \
  V(I32x4TruncSatF64x2SZero, kArm64I32x4TruncSatF64x2SZero) \
  V(I32x4TruncSatF64x2UZero, kArm64I32x4TruncSatF64x2UZero) \
  V(I16x8Neg, kArm64I16x8Neg)                               \
  V(I16x8Abs, kArm64I16x8Abs)                               \
  V(I16x8BitMask, kArm64I16x8BitMask)                       \
  V(I8x16Neg, kArm64I8x16Neg)                               \
  V(I8x16Abs, kArm64I8x16Abs)                               \
  V(I8x16BitMask, kArm64I8x16BitMask)                       \
  V(S128Not, kArm64S128Not)                                 \
  V(V128AnyTrue, kArm64V128AnyTrue)                         \
  V(I64x2AllTrue, kArm64I64x2AllTrue)                       \
  V(I32x4AllTrue, kArm64I32x4AllTrue)                       \
  V(I16x8AllTrue, kArm64I16x8AllTrue)                       \
  V(I8x16AllTrue, kArm64I8x16AllTrue)

#define SIMD_SHIFT_OP_LIST(V) \
  V(I64x2Shl, 64)             \
  V(I64x2ShrS, 64)            \
  V(I64x2ShrU, 64)            \
  V(I32x4Shl, 32)             \
  V(I32x4ShrS, 32)            \
  V(I32x4ShrU, 32)            \
  V(I16x8Shl, 16)             \
  V(I16x8ShrS, 16)            \
  V(I16x8ShrU, 16)            \
  V(I8x16Shl, 8)              \
  V(I8x16ShrS, 8)             \
  V(I8x16ShrU, 8)

#define SIMD_BINOP_LIST(V)                              \
  V(F64x2Add, kArm64F64x2Add)                           \
  V(F64x2Sub, kArm64F64x2Sub)                           \
  V(F64x2Div, kArm64F64x2Div)                           \
  V(F64x2Min, kArm64F64x2Min)                           \
  V(F64x2Max, kArm64F64x2Max)                           \
  V(F64x2Eq, kArm64F64x2Eq)                             \
  V(F64x2Ne, kArm64F64x2Ne)                             \
  V(F64x2Lt, kArm64F64x2Lt)                             \
  V(F64x2Le, kArm64F64x2Le)                             \
  V(F32x4Add, kArm64F32x4Add)                           \
  V(F32x4Sub, kArm64F32x4Sub)                           \
  V(F32x4Div, kArm64F32x4Div)                           \
  V(F32x4Min, kArm64F32x4Min)                           \
  V(F32x4Max, kArm64F32x4Max)                           \
  V(F32x4Eq, kArm64F32x4Eq)                             \
  V(F32x4Ne, kArm64F32x4Ne)                             \
  V(F32x4Lt, kArm64F32x4Lt)                             \
  V(F32x4Le, kArm64F32x4Le)                             \
  V(I64x2Add, kArm64I64x2Add)                           \
  V(I64x2Sub, kArm64I64x2Sub)                           \
  V(I64x2Eq, kArm64I64x2Eq)                             \
  V(I64x2Ne, kArm64I64x2Ne)                             \
  V(I64x2GtS, kArm64I64x2GtS)                           \
  V(I64x2GeS, kArm64I64x2GeS)                           \
  V(I32x4Mul, kArm64I32x4Mul)                           \
  V(I32x4MinS, kArm64I32x4MinS)                         \
  V(I32x4MaxS, kArm64I32x4MaxS)                         \
  V(I32x4Eq, kArm64I32x4Eq)                             \
  V(I32x4Ne, kArm64I32x4Ne)                             \
  V(I32x4GtS, kArm64I32x4GtS)                           \
  V(I32x4GeS, kArm64I32x4GeS)                           \
  V(I32x4MinU, kArm64I32x4MinU)                         \
  V(I32x4MaxU, kArm64I32x4MaxU)                         \
  V(I32x4GtU, kArm64I32x4GtU)                           \
  V(I32x4GeU, kArm64I32x4GeU)                           \
  V(I32x4DotI16x8S, kArm64I32x4DotI16x8S)               \
  V(I16x8SConvertI32x4, kArm64I16x8SConvertI32x4)       \
  V(I16x8AddSatS, kArm64I16x8AddSatS)                   \
  V(I16x8SubSatS, kArm64I16x8SubSatS)                   \
  V(I16x8Mul, kArm64I16x8Mul)                           \
  V(I16x8MinS, kArm64I16x8MinS)                         \
  V(I16x8MaxS, kArm64I16x8MaxS)                         \
  V(I16x8Eq, kArm64I16x8Eq)                             \
  V(I16x8Ne, kArm64I16x8Ne)                             \
  V(I16x8GtS, kArm64I16x8GtS)                           \
  V(I16x8GeS, kArm64I16x8GeS)                           \
  V(I16x8UConvertI32x4, kArm64I16x8UConvertI32x4)       \
  V(I16x8AddSatU, kArm64I16x8AddSatU)                   \
  V(I16x8SubSatU, kArm64I16x8SubSatU)                   \
  V(I16x8MinU, kArm64I16x8MinU)                         \
  V(I16x8MaxU, kArm64I16x8MaxU)                         \
  V(I16x8GtU, kArm64I16x8GtU)                           \
  V(I16x8GeU, kArm64I16x8GeU)                           \
  V(I16x8RoundingAverageU, kArm64I16x8RoundingAverageU) \
  V(I16x8Q15MulRSatS, kArm64I16x8Q15MulRSatS)           \
  V(I8x16Add, kArm64I8x16Add)                           \
  V(I8x16Sub, kArm64I8x16Sub)                           \
  V(I8x16SConvertI16x8, kArm64I8x16SConvertI16x8)       \
  V(I8x16AddSatS, kArm64I8x16AddSatS)                   \
  V(I8x16SubSatS, kArm64I8x16SubSatS)                   \
  V(I8x16MinS, kArm64I8x16MinS)                         \
  V(I8x16MaxS, kArm64I8x16MaxS)                         \
  V(I8x16Eq, kArm64I8x16Eq)                             \
  V(I8x16Ne, kArm64I8x16Ne)                             \
  V(I8x16GtS, kArm64I8x16GtS)                           \
  V(I8x16GeS, kArm64I8x16GeS)                           \
  V(I8x16UConvertI16x8, kArm64I8x16UConvertI16x8)       \
  V(I8x16AddSatU, kArm64I8x16AddSatU)                   \
  V(I8x16SubSatU, kArm64I8x16SubSatU)                   \
  V(I8x16MinU, kArm64I8x16MinU)                         \
  V(I8x16MaxU, kArm64I8x16MaxU)                         \
  V(I8x16GtU, kArm64I8x16GtU)                           \
  V(I8x16GeU, kArm64I8x16GeU)                           \
  V(I8x16RoundingAverageU, kArm64I8x16RoundingAverageU) \
  V(S128And, kArm64S128And)                             \
  V(S128Or, kArm64S128Or)                               \
  V(S128Xor, kArm64S128Xor)                             \
  V(S128AndNot, kArm64S128AndNot)

void InstructionSelector::VisitS128Const(Node* node) {
  Arm64OperandGenerator g(this);
  static const int kUint32Immediates = 4;
  uint32_t val[kUint32Immediates];
  STATIC_ASSERT(sizeof(val) == kSimd128Size);
  base::Memcpy(val, S128ImmediateParameterOf(node->op()).data(), kSimd128Size);
  // If all bytes are zeros, avoid emitting code for generic constants
  bool all_zeros = !(val[0] || val[1] || val[2] || val[3]);
  InstructionOperand dst = g.DefineAsRegister(node);
  if (all_zeros) {
    Emit(kArm64S128Zero, dst);
  } else {
    Emit(kArm64S128Const, g.DefineAsRegister(node), g.UseImmediate(val[0]),
         g.UseImmediate(val[1]), g.UseImmediate(val[2]),
         g.UseImmediate(val[3]));
  }
}

void InstructionSelector::VisitS128Zero(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64S128Zero, g.DefineAsRegister(node));
}

#define SIMD_VISIT_SPLAT(Type)                               \
  void InstructionSelector::Visit##Type##Splat(Node* node) { \
    VisitRR(this, kArm64##Type##Splat, node);                \
  }
SIMD_TYPE_LIST(SIMD_VISIT_SPLAT)
#undef SIMD_VISIT_SPLAT

#define SIMD_VISIT_EXTRACT_LANE(Type, Sign)                              \
  void InstructionSelector::Visit##Type##ExtractLane##Sign(Node* node) { \
    VisitRRI(this, kArm64##Type##ExtractLane##Sign, node);               \
  }
SIMD_VISIT_EXTRACT_LANE(F64x2, )
SIMD_VISIT_EXTRACT_LANE(F32x4, )
SIMD_VISIT_EXTRACT_LANE(I64x2, )
SIMD_VISIT_EXTRACT_LANE(I32x4, )
SIMD_VISIT_EXTRACT_LANE(I16x8, U)
SIMD_VISIT_EXTRACT_LANE(I16x8, S)
SIMD_VISIT_EXTRACT_LANE(I8x16, U)
SIMD_VISIT_EXTRACT_LANE(I8x16, S)
#undef SIMD_VISIT_EXTRACT_LANE

#define SIMD_VISIT_REPLACE_LANE(Type)                              \
  void InstructionSelector::Visit##Type##ReplaceLane(Node* node) { \
    VisitRRIR(this, kArm64##Type##ReplaceLane, node);              \
  }
SIMD_TYPE_LIST(SIMD_VISIT_REPLACE_LANE)
#undef SIMD_VISIT_REPLACE_LANE
#undef SIMD_TYPE_LIST

#define SIMD_VISIT_UNOP(Name, instruction)            \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRR(this, instruction, node);                 \
  }
SIMD_UNOP_LIST(SIMD_VISIT_UNOP)
#undef SIMD_VISIT_UNOP
#undef SIMD_UNOP_LIST

#define SIMD_VISIT_SHIFT_OP(Name, width)                \
  void InstructionSelector::Visit##Name(Node* node) {   \
    VisitSimdShiftRRR(this, kArm64##Name, node, width); \
  }
SIMD_SHIFT_OP_LIST(SIMD_VISIT_SHIFT_OP)
#undef SIMD_VISIT_SHIFT_OP
#undef SIMD_SHIFT_OP_LIST

#define SIMD_VISIT_BINOP(Name, instruction)           \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRRR(this, instruction, node);                \
  }
SIMD_BINOP_LIST(SIMD_VISIT_BINOP)
#undef SIMD_VISIT_BINOP
#undef SIMD_BINOP_LIST

using ShuffleMatcher =
    ValueMatcher<S128ImmediateParameter, IrOpcode::kI8x16Shuffle>;
using BinopWithShuffleMatcher = BinopMatcher<ShuffleMatcher, ShuffleMatcher>;

namespace {
// Struct holding the result of pattern-matching a mul+dup.
struct MulWithDupResult {
  Node* input;     // Node holding the vector elements.
  Node* dup_node;  // Node holding the lane to multiply.
  int index;
  // Pattern-match is successful if dup_node is set.
  explicit operator bool() const { return dup_node != nullptr; }
};

template <int LANES>
MulWithDupResult TryMatchMulWithDup(Node* node) {
  // Pattern match:
  //   f32x4.mul(x, shuffle(x, y, indices)) => f32x4.mul(x, y, laneidx)
  //   f64x2.mul(x, shuffle(x, y, indices)) => f64x2.mul(x, y, laneidx)
  //   where shuffle(x, y, indices) = dup(x[laneidx]) or dup(y[laneidx])
  // f32x4.mul and f64x2.mul are commutative, so use BinopMatcher.
  Node* input = nullptr;
  Node* dup_node = nullptr;

  int index = 0;
#if V8_ENABLE_WEBASSEMBLY
  BinopWithShuffleMatcher m = BinopWithShuffleMatcher(node);
  ShuffleMatcher left = m.left();
  ShuffleMatcher right = m.right();

  // TODO(zhin): We can canonicalize first to avoid checking index < LANES.
  // e.g. shuffle(x, y, [16, 17, 18, 19...]) => shuffle(y, y, [0, 1, 2,
  // 3]...). But doing so can mutate the inputs of the shuffle node without
  // updating the shuffle immediates themselves. Fix that before we
  // canonicalize here. We don't want CanCover here because in many use cases,
  // the shuffle is generated early in the function, but the f32x4.mul happens
  // in a loop, which won't cover the shuffle since they are different basic
  // blocks.
  if (left.HasResolvedValue() && wasm::SimdShuffle::TryMatchSplat<LANES>(
                                     left.ResolvedValue().data(), &index)) {
    dup_node = left.node()->InputAt(index < LANES ? 0 : 1);
    input = right.node();
  } else if (right.HasResolvedValue() &&
             wasm::SimdShuffle::TryMatchSplat<LANES>(
                 right.ResolvedValue().data(), &index)) {
    dup_node = right.node()->InputAt(index < LANES ? 0 : 1);
    input = left.node();
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  // Canonicalization would get rid of this too.
  index %= LANES;

  return {input, dup_node, index};
}
}  // namespace

void InstructionSelector::VisitF32x4Mul(Node* node) {
  if (MulWithDupResult result = TryMatchMulWithDup<4>(node)) {
    Arm64OperandGenerator g(this);
    Emit(kArm64F32x4MulElement, g.DefineAsRegister(node),
         g.UseRegister(result.input), g.UseRegister(result.dup_node),
         g.UseImmediate(result.index));
  } else {
    return VisitRRR(this, kArm64F32x4Mul, node);
  }
}

void InstructionSelector::VisitF64x2Mul(Node* node) {
  if (MulWithDupResult result = TryMatchMulWithDup<2>(node)) {
    Arm64OperandGenerator g(this);
    Emit(kArm64F64x2MulElement, g.DefineAsRegister(node),
         g.UseRegister(result.input), g.UseRegister(result.dup_node),
         g.UseImmediate(result.index));
  } else {
    return VisitRRR(this, kArm64F64x2Mul, node);
  }
}

void InstructionSelector::VisitI64x2Mul(Node* node) {
  Arm64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempSimd128Register()};
  Emit(kArm64I64x2Mul, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)),
       arraysize(temps), temps);
}

#define VISIT_SIMD_ADD(Type)                                                   \
  void InstructionSelector::Visit##Type##Add(Node* node) {                     \
    Arm64OperandGenerator g(this);                                             \
    Node* left = node->InputAt(0);                                             \
    Node* right = node->InputAt(1);                                            \
    /* Select Mla(z, x, y) for Add(Mul(x, y), z). */                           \
    if (left->opcode() == IrOpcode::k##Type##Mul && CanCover(node, left)) {    \
      Emit(kArm64##Type##Mla, g.DefineSameAsFirst(node), g.UseRegister(right), \
           g.UseRegister(left->InputAt(0)), g.UseRegister(left->InputAt(1)));  \
      return;                                                                  \
    }                                                                          \
    /* Select Mla(z, x, y) for Add(z, Mul(x, y)). */                           \
    if (right->opcode() == IrOpcode::k##Type##Mul && CanCover(node, right)) {  \
      Emit(kArm64##Type##Mla, g.DefineSameAsFirst(node), g.UseRegister(left),  \
           g.UseRegister(right->InputAt(0)),                                   \
           g.UseRegister(right->InputAt(1)));                                  \
      return;                                                                  \
    }                                                                          \
    VisitRRR(this, kArm64##Type##Add, node);                                   \
  }

VISIT_SIMD_ADD(I32x4)
VISIT_SIMD_ADD(I16x8)
#undef VISIT_SIMD_ADD

#define VISIT_SIMD_SUB(Type)                                                  \
  void InstructionSelector::Visit##Type##Sub(Node* node) {                    \
    Arm64OperandGenerator g(this);                                            \
    Node* left = node->InputAt(0);                                            \
    Node* right = node->InputAt(1);                                           \
    /* Select Mls(z, x, y) for Sub(z, Mul(x, y)). */                          \
    if (right->opcode() == IrOpcode::k##Type##Mul && CanCover(node, right)) { \
      Emit(kArm64##Type##Mls, g.DefineSameAsFirst(node), g.UseRegister(left), \
           g.UseRegister(right->InputAt(0)),                                  \
           g.UseRegister(right->InputAt(1)));                                 \
      return;                                                                 \
    }                                                                         \
    VisitRRR(this, kArm64##Type##Sub, node);                                  \
  }

VISIT_SIMD_SUB(I32x4)
VISIT_SIMD_SUB(I16x8)
#undef VISIT_SIMD_SUB

void InstructionSelector::VisitS128Select(Node* node) {
  Arm64OperandGenerator g(this);
  Emit(kArm64S128Select, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)),
       g.UseRegister(node->InputAt(2)));
}

#define VISIT_SIMD_QFMOP(op)                                               \
  void InstructionSelector::Visit##op(Node* node) {                        \
    Arm64OperandGenerator g(this);                                         \
    Emit(kArm64##op, g.DefineSameAsFirst(node),                            \
         g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)), \
         g.UseRegister(node->InputAt(2)));                                 \
  }
VISIT_SIMD_QFMOP(F64x2Qfma)
VISIT_SIMD_QFMOP(F64x2Qfms)
VISIT_SIMD_QFMOP(F32x4Qfma)
VISIT_SIMD_QFMOP(F32x4Qfms)
#undef VISIT_SIMD_QFMOP

#if V8_ENABLE_WEBASSEMBLY
namespace {

struct ShuffleEntry {
  uint8_t shuffle[kSimd128Size];
  ArchOpcode opcode;
};

static const ShuffleEntry arch_shuffles[] = {
    {{0, 1, 2, 3, 16, 17, 18, 19, 4, 5, 6, 7, 20, 21, 22, 23},
     kArm64S32x4ZipLeft},
    {{8, 9, 10, 11, 24, 25, 26, 27, 12, 13, 14, 15, 28, 29, 30, 31},
     kArm64S32x4ZipRight},
    {{0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27},
     kArm64S32x4UnzipLeft},
    {{4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31},
     kArm64S32x4UnzipRight},
    {{0, 1, 2, 3, 16, 17, 18, 19, 8, 9, 10, 11, 24, 25, 26, 27},
     kArm64S32x4TransposeLeft},
    {{4, 5, 6, 7, 20, 21, 22, 23, 12, 13, 14, 15, 21, 22, 23, 24},
     kArm64S32x4TransposeRight},
    {{4, 5, 6, 7, 0, 1, 2, 3, 12, 13, 14, 15, 8, 9, 10, 11},
     kArm64S32x2Reverse},

    {{0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21, 6, 7, 22, 23},
     kArm64S16x8ZipLeft},
    {{8, 9, 24, 25, 10, 11, 26, 27, 12, 13, 28, 29, 14, 15, 30, 31},
     kArm64S16x8ZipRight},
    {{0, 1, 4, 5, 8, 9, 12, 13, 16, 17, 20, 21, 24, 25, 28, 29},
     kArm64S16x8UnzipLeft},
    {{2, 3, 6, 7, 10, 11, 14, 15, 18, 19, 22, 23, 26, 27, 30, 31},
     kArm64S16x8UnzipRight},
    {{0, 1, 16, 17, 4, 5, 20, 21, 8, 9, 24, 25, 12, 13, 28, 29},
     kArm64S16x8TransposeLeft},
    {{2, 3, 18, 19, 6, 7, 22, 23, 10, 11, 26, 27, 14, 15, 30, 31},
     kArm64S16x8TransposeRight},
    {{6, 7, 4, 5, 2, 3, 0, 1, 14, 15, 12, 13, 10, 11, 8, 9},
     kArm64S16x4Reverse},
    {{2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13},
     kArm64S16x2Reverse},

    {{0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23},
     kArm64S8x16ZipLeft},
    {{8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31},
     kArm64S8x16ZipRight},
    {{0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30},
     kArm64S8x16UnzipLeft},
    {{1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31},
     kArm64S8x16UnzipRight},
    {{0, 16, 2, 18, 4, 20, 6, 22, 8, 24, 10, 26, 12, 28, 14, 30},
     kArm64S8x16TransposeLeft},
    {{1, 17, 3, 19, 5, 21, 7, 23, 9, 25, 11, 27, 13, 29, 15, 31},
     kArm64S8x16TransposeRight},
    {{7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8}, kArm64S8x8Reverse},
    {{3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12}, kArm64S8x4Reverse},
    {{1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14},
     kArm64S8x2Reverse}};

bool TryMatchArchShuffle(const uint8_t* shuffle, const ShuffleEntry* table,
                         size_t num_entries, bool is_swizzle,
                         ArchOpcode* opcode) {
  uint8_t mask = is_swizzle ? kSimd128Size - 1 : 2 * kSimd128Size - 1;
  for (size_t i = 0; i < num_entries; i++) {
    const ShuffleEntry& entry = table[i];
    int j = 0;
    for (; j < kSimd128Size; j++) {
      if ((entry.shuffle[j] & mask) != (shuffle[j] & mask)) {
        break;
      }
    }
    if (j == kSimd128Size) {
      *opcode = entry.opcode;
      return true;
    }
  }
  return false;
}

void ArrangeShuffleTable(Arm64OperandGenerator* g, Node* input0, Node* input1,
                         InstructionOperand* src0, InstructionOperand* src1) {
  if (input0 == input1) {
    // Unary, any q-register can be the table.
    *src0 = *src1 = g->UseRegister(input0);
  } else {
    // Binary, table registers must be consecutive.
    *src0 = g->UseFixed(input0, fp_fixed1);
    *src1 = g->UseFixed(input1, fp_fixed2);
  }
}

}  // namespace

void InstructionSelector::VisitI8x16Shuffle(Node* node) {
  uint8_t shuffle[kSimd128Size];
  bool is_swizzle;
  CanonicalizeShuffle(node, shuffle, &is_swizzle);
  uint8_t shuffle32x4[4];
  Arm64OperandGenerator g(this);
  ArchOpcode opcode;
  if (TryMatchArchShuffle(shuffle, arch_shuffles, arraysize(arch_shuffles),
                          is_swizzle, &opcode)) {
    VisitRRR(this, opcode, node);
    return;
  }
  Node* input0 = node->InputAt(0);
  Node* input1 = node->InputAt(1);
  uint8_t offset;
  if (wasm::SimdShuffle::TryMatchConcat(shuffle, &offset)) {
    Emit(kArm64S8x16Concat, g.DefineAsRegister(node), g.UseRegister(input0),
         g.UseRegister(input1), g.UseImmediate(offset));
    return;
  }
  int index = 0;
  if (wasm::SimdShuffle::TryMatch32x4Shuffle(shuffle, shuffle32x4)) {
    if (wasm::SimdShuffle::TryMatchSplat<4>(shuffle, &index)) {
      DCHECK_GT(4, index);
      Emit(kArm64S128Dup, g.DefineAsRegister(node), g.UseRegister(input0),
           g.UseImmediate(4), g.UseImmediate(index % 4));
    } else if (wasm::SimdShuffle::TryMatchIdentity(shuffle)) {
      EmitIdentity(node);
    } else {
      Emit(kArm64S32x4Shuffle, g.DefineAsRegister(node), g.UseRegister(input0),
           g.UseRegister(input1),
           g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle32x4)));
    }
    return;
  }
  if (wasm::SimdShuffle::TryMatchSplat<8>(shuffle, &index)) {
    DCHECK_GT(8, index);
    Emit(kArm64S128Dup, g.DefineAsRegister(node), g.UseRegister(input0),
         g.UseImmediate(8), g.UseImmediate(index % 8));
    return;
  }
  if (wasm::SimdShuffle::TryMatchSplat<16>(shuffle, &index)) {
    DCHECK_GT(16, index);
    Emit(kArm64S128Dup, g.DefineAsRegister(node), g.UseRegister(input0),
         g.UseImmediate(16), g.UseImmediate(index % 16));
    return;
  }
  // Code generator uses vtbl, arrange sources to form a valid lookup table.
  InstructionOperand src0, src1;
  ArrangeShuffleTable(&g, input0, input1, &src0, &src1);
  Emit(kArm64I8x16Shuffle, g.DefineAsRegister(node), src0, src1,
       g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle)),
       g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle + 4)),
       g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle + 8)),
       g.UseImmediate(wasm::SimdShuffle::Pack4Lanes(shuffle + 12)));
}
#else
void InstructionSelector::VisitI8x16Shuffle(Node* node) { UNREACHABLE(); }
#endif  // V8_ENABLE_WEBASSEMBLY

void InstructionSelector::VisitSignExtendWord8ToInt32(Node* node) {
  VisitRR(this, kArm64Sxtb32, node);
}

void InstructionSelector::VisitSignExtendWord16ToInt32(Node* node) {
  VisitRR(this, kArm64Sxth32, node);
}

void InstructionSelector::VisitSignExtendWord8ToInt64(Node* node) {
  VisitRR(this, kArm64Sxtb, node);
}

void InstructionSelector::VisitSignExtendWord16ToInt64(Node* node) {
  VisitRR(this, kArm64Sxth, node);
}

void InstructionSelector::VisitSignExtendWord32ToInt64(Node* node) {
  VisitRR(this, kArm64Sxtw, node);
}

namespace {
void VisitPminOrPmax(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node) {
  Arm64OperandGenerator g(selector);
  // Need all unique registers because we first compare the two inputs, then we
  // need the inputs to remain unchanged for the bitselect later.
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseUniqueRegister(node->InputAt(0)),
                 g.UseUniqueRegister(node->InputAt(1)));
}
}  // namespace

void InstructionSelector::VisitF32x4Pmin(Node* node) {
  VisitPminOrPmax(this, kArm64F32x4Pmin, node);
}

void InstructionSelector::VisitF32x4Pmax(Node* node) {
  VisitPminOrPmax(this, kArm64F32x4Pmax, node);
}

void InstructionSelector::VisitF64x2Pmin(Node* node) {
  VisitPminOrPmax(this, kArm64F64x2Pmin, node);
}

void InstructionSelector::VisitF64x2Pmax(Node* node) {
  VisitPminOrPmax(this, kArm64F64x2Pmax, node);
}

namespace {
void VisitSignExtendLong(InstructionSelector* selector, ArchOpcode opcode,
                         Node* node, int lane_size) {
  InstructionCode code = opcode;
  code |= LaneSizeField::encode(lane_size);
  VisitRR(selector, code, node);
}
}  // namespace

void InstructionSelector::VisitI64x2SConvertI32x4Low(Node* node) {
  VisitSignExtendLong(this, kArm64Sxtl, node, 64);
}

void InstructionSelector::VisitI64x2SConvertI32x4High(Node* node) {
  VisitSignExtendLong(this, kArm64Sxtl2, node, 64);
}

void InstructionSelector::VisitI64x2UConvertI32x4Low(Node* node) {
  VisitSignExtendLong(this, kArm64Uxtl, node, 64);
}

void InstructionSelector::VisitI64x2UConvertI32x4High(Node* node) {
  VisitSignExtendLong(this, kArm64Uxtl2, node, 64);
}

void InstructionSelector::VisitI32x4SConvertI16x8Low(Node* node) {
  VisitSignExtendLong(this, kArm64Sxtl, node, 32);
}

void InstructionSelector::VisitI32x4SConvertI16x8High(Node* node) {
  VisitSignExtendLong(this, kArm64Sxtl2, node, 32);
}

void InstructionSelector::VisitI32x4UConvertI16x8Low(Node* node) {
  VisitSignExtendLong(this, kArm64Uxtl, node, 32);
}

void InstructionSelector::VisitI32x4UConvertI16x8High(Node* node) {
  VisitSignExtendLong(this, kArm64Uxtl2, node, 32);
}

void InstructionSelector::VisitI16x8SConvertI8x16Low(Node* node) {
  VisitSignExtendLong(this, kArm64Sxtl, node, 16);
}

void InstructionSelector::VisitI16x8SConvertI8x16High(Node* node) {
  VisitSignExtendLong(this, kArm64Sxtl2, node, 16);
}

void InstructionSelector::VisitI16x8UConvertI8x16Low(Node* node) {
  VisitSignExtendLong(this, kArm64Uxtl, node, 16);
}

void InstructionSelector::VisitI16x8UConvertI8x16High(Node* node) {
  VisitSignExtendLong(this, kArm64Uxtl2, node, 16);
}

void InstructionSelector::VisitI8x16Popcnt(Node* node) {
  InstructionCode code = kArm64Cnt;
  code |= LaneSizeField::encode(8);
  VisitRR(this, code, node);
}

void InstructionSelector::AddOutputToSelectContinuation(OperandGenerator* g,
                                                        int first_input_index,
                                                        Node* node) {
  continuation_outputs_.push_back(g->DefineAsRegister(node));
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
         MachineOperatorBuilder::kWord64ReverseBits |
         MachineOperatorBuilder::kSatConversionIsSafe |
         MachineOperatorBuilder::kFloat32Select |
         MachineOperatorBuilder::kFloat64Select;
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
