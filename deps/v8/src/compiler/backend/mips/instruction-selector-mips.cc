// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE_UNIMPL() \
  PrintF("UNIMPLEMENTED instr_sel: %s at line %d\n", __FUNCTION__, __LINE__)

#define TRACE() PrintF("instr_sel: %s at line %d\n", __FUNCTION__, __LINE__)

// Adds Mips-specific methods for generating InstructionOperands.
class MipsOperandGenerator final : public OperandGenerator {
 public:
  explicit MipsOperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand UseOperand(Node* node, InstructionCode opcode) {
    if (CanBeImmediate(node, opcode)) {
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

  bool IsIntegerConstant(Node* node) {
    return (node->opcode() == IrOpcode::kInt32Constant);
  }

  int64_t GetIntegerConstantValue(Node* node) {
    DCHECK_EQ(IrOpcode::kInt32Constant, node->opcode());
    return OpParameter<int32_t>(node->op());
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

  bool CanBeImmediate(Node* node, InstructionCode opcode) {
    Int32Matcher m(node);
    if (!m.HasValue()) return false;
    int32_t value = m.Value();
    switch (ArchOpcodeField::decode(opcode)) {
      case kMipsShl:
      case kMipsSar:
      case kMipsShr:
        return is_uint5(value);
      case kMipsAdd:
      case kMipsAnd:
      case kMipsOr:
      case kMipsTst:
      case kMipsSub:
      case kMipsXor:
        return is_uint16(value);
      case kMipsLb:
      case kMipsLbu:
      case kMipsSb:
      case kMipsLh:
      case kMipsLhu:
      case kMipsSh:
      case kMipsLw:
      case kMipsSw:
      case kMipsLwc1:
      case kMipsSwc1:
      case kMipsLdc1:
      case kMipsSdc1:
        // true even for 32b values, offsets > 16b
        // are handled in assembler-mips.cc
        return is_int32(value);
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

static void VisitRRR(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node) {
  MipsOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
}

void VisitRRRR(InstructionSelector* selector, ArchOpcode opcode, Node* node) {
  MipsOperandGenerator g(selector);
  selector->Emit(
      opcode, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
      g.UseRegister(node->InputAt(1)), g.UseRegister(node->InputAt(2)));
}

static void VisitRR(InstructionSelector* selector, ArchOpcode opcode,
                    Node* node) {
  MipsOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}

static void VisitRRI(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node) {
  MipsOperandGenerator g(selector);
  int32_t imm = OpParameter<int32_t>(node->op());
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)), g.UseImmediate(imm));
}

static void VisitRRIR(InstructionSelector* selector, ArchOpcode opcode,
                      Node* node) {
  MipsOperandGenerator g(selector);
  int32_t imm = OpParameter<int32_t>(node->op());
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)), g.UseImmediate(imm),
                 g.UseRegister(node->InputAt(1)));
}

static void VisitRRO(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node) {
  MipsOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseOperand(node->InputAt(1), opcode));
}

bool TryMatchImmediate(InstructionSelector* selector,
                       InstructionCode* opcode_return, Node* node,
                       size_t* input_count_return, InstructionOperand* inputs) {
  MipsOperandGenerator g(selector);
  if (g.CanBeImmediate(node, *opcode_return)) {
    *opcode_return |= AddressingModeField::encode(kMode_MRI);
    inputs[0] = g.UseImmediate(node);
    *input_count_return = 1;
    return true;
  }
  return false;
}

static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, bool has_reverse_opcode,
                       InstructionCode reverse_opcode,
                       FlagsContinuation* cont) {
  MipsOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand inputs[2];
  size_t input_count = 0;
  InstructionOperand outputs[1];
  size_t output_count = 0;

  if (TryMatchImmediate(selector, &opcode, m.right().node(), &input_count,
                        &inputs[1])) {
    inputs[0] = g.UseRegister(m.left().node());
    input_count++;
  } else if (has_reverse_opcode &&
             TryMatchImmediate(selector, &reverse_opcode, m.left().node(),
                               &input_count, &inputs[1])) {
    inputs[0] = g.UseRegister(m.right().node());
    opcode = reverse_opcode;
    input_count++;
  } else {
    inputs[input_count++] = g.UseRegister(m.left().node());
    inputs[input_count++] = g.UseOperand(m.right().node(), opcode);
  }

  if (cont->IsDeoptimize()) {
    // If we can deoptimize as a result of the binop, we need to make sure that
    // the deopt inputs are not overwritten by the binop result. One way
    // to achieve that is to declare the output register as same-as-first.
    outputs[output_count++] = g.DefineSameAsFirst(node);
  } else {
    outputs[output_count++] = g.DefineAsRegister(node);
  }

  DCHECK_NE(0u, input_count);
  DCHECK_EQ(1u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}

static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, bool has_reverse_opcode,
                       InstructionCode reverse_opcode) {
  FlagsContinuation cont;
  VisitBinop(selector, node, opcode, has_reverse_opcode, reverse_opcode, &cont);
}

static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, FlagsContinuation* cont) {
  VisitBinop(selector, node, opcode, false, kArchNop, cont);
}

static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode) {
  VisitBinop(selector, node, opcode, false, kArchNop);
}

static void VisitPairAtomicBinop(InstructionSelector* selector, Node* node,
                                 ArchOpcode opcode) {
  MipsOperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  Node* value_high = node->InputAt(3);
  AddressingMode addressing_mode = kMode_None;
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index),
                                 g.UseFixed(value, a1),
                                 g.UseFixed(value_high, a2)};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  InstructionOperand temps[3];
  size_t temp_count = 0;
  temps[temp_count++] = g.TempRegister(a0);

  Node* projection0 = NodeProperties::FindProjection(node, 0);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection0) {
    outputs[output_count++] = g.DefineAsFixed(projection0, v0);
  } else {
    temps[temp_count++] = g.TempRegister(v0);
  }
  if (projection1) {
    outputs[output_count++] = g.DefineAsFixed(projection1, v1);
  } else {
    temps[temp_count++] = g.TempRegister(v1);
  }
  selector->Emit(code, output_count, outputs, arraysize(inputs), inputs,
                 temp_count, temps);
}

void InstructionSelector::VisitStackSlot(Node* node) {
  StackSlotRepresentation rep = StackSlotRepresentationOf(node->op());
  int alignment = rep.alignment();
  int slot = frame_->AllocateSpillSlot(rep.size(), alignment);
  OperandGenerator g(this);

  Emit(kArchStackSlot, g.DefineAsRegister(node),
       sequence()->AddImmediate(Constant(slot)),
       sequence()->AddImmediate(Constant(alignment)), 0, nullptr);
}

void InstructionSelector::VisitAbortCSAAssert(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kArchAbortCSAAssert, g.NoOutput(), g.UseFixed(node->InputAt(0), a0));
}

void InstructionSelector::VisitLoadTransform(Node* node) {
  LoadTransformParameters params = LoadTransformParametersOf(node->op());
  MipsOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  InstructionCode opcode = kArchNop;
  switch (params.transformation) {
    case LoadTransformation::kS8x16LoadSplat:
      opcode = kMipsS8x16LoadSplat;
      break;
    case LoadTransformation::kS16x8LoadSplat:
      opcode = kMipsS16x8LoadSplat;
      break;
    case LoadTransformation::kS32x4LoadSplat:
      opcode = kMipsS32x4LoadSplat;
      break;
    case LoadTransformation::kS64x2LoadSplat:
      opcode = kMipsS64x2LoadSplat;
      break;
    case LoadTransformation::kI16x8Load8x8S:
      opcode = kMipsI16x8Load8x8S;
      break;
    case LoadTransformation::kI16x8Load8x8U:
      opcode = kMipsI16x8Load8x8U;
      break;
    case LoadTransformation::kI32x4Load16x4S:
      opcode = kMipsI32x4Load16x4S;
      break;
    case LoadTransformation::kI32x4Load16x4U:
      opcode = kMipsI32x4Load16x4U;
      break;
    case LoadTransformation::kI64x2Load32x2S:
      opcode = kMipsI64x2Load32x2S;
      break;
    case LoadTransformation::kI64x2Load32x2U:
      opcode = kMipsI64x2Load32x2U;
      break;
    default:
      UNIMPLEMENTED();
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kMipsAdd | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

void InstructionSelector::VisitLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  MipsOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  InstructionCode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kMipsLwc1;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kMipsLdc1;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsUnsigned() ? kMipsLbu : kMipsLb;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsUnsigned() ? kMipsLhu : kMipsLh;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord32:
      opcode = kMipsLw;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kMipsMsaLd;
      break;
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:         // Fall through.
    case MachineRepresentation::kWord64:             // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }
  if (node->opcode() == IrOpcode::kPoisonedLoad) {
    CHECK_NE(poisoning_level_, PoisoningMitigationLevel::kDontPoison);
    opcode |= MiscField::encode(kMemoryAccessPoisoned);
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kMipsAdd | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

void InstructionSelector::VisitPoisonedLoad(Node* node) { VisitLoad(node); }

void InstructionSelector::VisitProtectedLoad(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelector::VisitStore(Node* node) {
  MipsOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());
  WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();
  MachineRepresentation rep = store_rep.representation();

  // TODO(mips): I guess this could be done in a better way.
  if (write_barrier_kind != kNoWriteBarrier &&
      V8_LIKELY(!FLAG_disable_write_barriers)) {
    DCHECK(CanBeTaggedPointer(rep));
    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    inputs[input_count++] = g.UseUniqueRegister(index);
    inputs[input_count++] = g.UseUniqueRegister(value);
    RecordWriteMode record_write_mode =
        WriteBarrierKindToRecordWriteMode(write_barrier_kind);
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    size_t const temp_count = arraysize(temps);
    InstructionCode code = kArchStoreWithWriteBarrier;
    code |= MiscField::encode(static_cast<int>(record_write_mode));
    Emit(code, 0, nullptr, input_count, inputs, temp_count, temps);
  } else {
    ArchOpcode opcode = kArchNop;
    switch (rep) {
      case MachineRepresentation::kFloat32:
        opcode = kMipsSwc1;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kMipsSdc1;
        break;
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = kMipsSb;
        break;
      case MachineRepresentation::kWord16:
        opcode = kMipsSh;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:         // Fall through.
      case MachineRepresentation::kWord32:
        opcode = kMipsSw;
        break;
      case MachineRepresentation::kSimd128:
        opcode = kMipsMsaSt;
        break;
      case MachineRepresentation::kCompressedPointer:  // Fall through.
      case MachineRepresentation::kCompressed:         // Fall through.
      case MachineRepresentation::kWord64:             // Fall through.
      case MachineRepresentation::kNone:
        UNREACHABLE();
        return;
    }

    if (g.CanBeImmediate(index, opcode)) {
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           g.UseRegister(base), g.UseImmediate(index),
           g.UseRegisterOrImmediateZero(value));
    } else {
      InstructionOperand addr_reg = g.TempRegister();
      Emit(kMipsAdd | AddressingModeField::encode(kMode_None), addr_reg,
           g.UseRegister(index), g.UseRegister(base));
      // Emit desired store opcode, using temp addr_reg.
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           addr_reg, g.TempImmediate(0), g.UseRegisterOrImmediateZero(value));
    }
  }
}

void InstructionSelector::VisitProtectedStore(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelector::VisitWord32And(Node* node) {
  MipsOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().IsWord32Shr() && CanCover(node, m.left().node()) &&
      m.right().HasValue()) {
    uint32_t mask = m.right().Value();
    uint32_t mask_width = base::bits::CountPopulation(mask);
    uint32_t mask_msb = base::bits::CountLeadingZeros32(mask);
    if ((mask_width != 0) && (mask_msb + mask_width == 32)) {
      // The mask must be contiguous, and occupy the least-significant bits.
      DCHECK_EQ(0u, base::bits::CountTrailingZeros32(mask));

      // Select Ext for And(Shr(x, imm), mask) where the mask is in the least
      // significant bits.
      Int32BinopMatcher mleft(m.left().node());
      if (mleft.right().HasValue()) {
        // Any shift value can match; int32 shifts use `value % 32`.
        uint32_t lsb = mleft.right().Value() & 0x1F;

        // Ext cannot extract bits past the register size, however since
        // shifting the original value would have introduced some zeros we can
        // still use Ext with a smaller mask and the remaining bits will be
        // zeros.
        if (lsb + mask_width > 32) mask_width = 32 - lsb;

        if (lsb == 0 && mask_width == 32) {
          Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(mleft.left().node()));
        } else {
          Emit(kMipsExt, g.DefineAsRegister(node),
               g.UseRegister(mleft.left().node()), g.TempImmediate(lsb),
               g.TempImmediate(mask_width));
        }
        return;
      }
      // Other cases fall through to the normal And operation.
    }
  }
  if (m.right().HasValue()) {
    uint32_t mask = m.right().Value();
    uint32_t shift = base::bits::CountPopulation(~mask);
    uint32_t msb = base::bits::CountLeadingZeros32(~mask);
    if (shift != 0 && shift != 32 && msb + shift == 32) {
      // Insert zeros for (x >> K) << K => x & ~(2^K - 1) expression reduction
      // and remove constant loading of invereted mask.
      Emit(kMipsIns, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
           g.TempImmediate(0), g.TempImmediate(shift));
      return;
    }
  }
  VisitBinop(this, node, kMipsAnd, true, kMipsAnd);
}

void InstructionSelector::VisitWord32Or(Node* node) {
  VisitBinop(this, node, kMipsOr, true, kMipsOr);
}

void InstructionSelector::VisitWord32Xor(Node* node) {
  Int32BinopMatcher m(node);
  if (m.left().IsWord32Or() && CanCover(node, m.left().node()) &&
      m.right().Is(-1)) {
    Int32BinopMatcher mleft(m.left().node());
    if (!mleft.right().HasValue()) {
      MipsOperandGenerator g(this);
      Emit(kMipsNor, g.DefineAsRegister(node),
           g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    }
  }
  if (m.right().Is(-1)) {
    // Use Nor for bit negation and eliminate constant loading for xori.
    MipsOperandGenerator g(this);
    Emit(kMipsNor, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
         g.TempImmediate(0));
    return;
  }
  VisitBinop(this, node, kMipsXor, true, kMipsXor);
}

void InstructionSelector::VisitWord32Shl(Node* node) {
  Int32BinopMatcher m(node);
  if (m.left().IsWord32And() && CanCover(node, m.left().node()) &&
      m.right().IsInRange(1, 31)) {
    MipsOperandGenerator g(this);
    Int32BinopMatcher mleft(m.left().node());
    // Match Word32Shl(Word32And(x, mask), imm) to Shl where the mask is
    // contiguous, and the shift immediate non-zero.
    if (mleft.right().HasValue()) {
      uint32_t mask = mleft.right().Value();
      uint32_t mask_width = base::bits::CountPopulation(mask);
      uint32_t mask_msb = base::bits::CountLeadingZeros32(mask);
      if ((mask_width != 0) && (mask_msb + mask_width == 32)) {
        uint32_t shift = m.right().Value();
        DCHECK_EQ(0u, base::bits::CountTrailingZeros32(mask));
        DCHECK_NE(0u, shift);
        if ((shift + mask_width) >= 32) {
          // If the mask is contiguous and reaches or extends beyond the top
          // bit, only the shift is needed.
          Emit(kMipsShl, g.DefineAsRegister(node),
               g.UseRegister(mleft.left().node()),
               g.UseImmediate(m.right().node()));
          return;
        }
      }
    }
  }
  VisitRRO(this, kMipsShl, node);
}

void InstructionSelector::VisitWord32Shr(Node* node) {
  Int32BinopMatcher m(node);
  if (m.left().IsWord32And() && m.right().HasValue()) {
    uint32_t lsb = m.right().Value() & 0x1F;
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue() && mleft.right().Value() != 0) {
      // Select Ext for Shr(And(x, mask), imm) where the result of the mask is
      // shifted into the least-significant bits.
      uint32_t mask = (mleft.right().Value() >> lsb) << lsb;
      unsigned mask_width = base::bits::CountPopulation(mask);
      unsigned mask_msb = base::bits::CountLeadingZeros32(mask);
      if ((mask_msb + mask_width + lsb) == 32) {
        MipsOperandGenerator g(this);
        DCHECK_EQ(lsb, base::bits::CountTrailingZeros32(mask));
        Emit(kMipsExt, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(lsb),
             g.TempImmediate(mask_width));
        return;
      }
    }
  }
  VisitRRO(this, kMipsShr, node);
}

void InstructionSelector::VisitWord32Sar(Node* node) {
  Int32BinopMatcher m(node);
  if ((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
      m.left().IsWord32Shl() && CanCover(node, m.left().node())) {
    Int32BinopMatcher mleft(m.left().node());
    if (m.right().HasValue() && mleft.right().HasValue()) {
      MipsOperandGenerator g(this);
      uint32_t sar = m.right().Value();
      uint32_t shl = mleft.right().Value();
      if ((sar == shl) && (sar == 16)) {
        Emit(kMipsSeh, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()));
        return;
      } else if ((sar == shl) && (sar == 24)) {
        Emit(kMipsSeb, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()));
        return;
      }
    }
  }
  VisitRRO(this, kMipsSar, node);
}

static void VisitInt32PairBinop(InstructionSelector* selector,
                                InstructionCode pair_opcode,
                                InstructionCode single_opcode, Node* node) {
  MipsOperandGenerator g(selector);

  Node* projection1 = NodeProperties::FindProjection(node, 1);

  if (projection1) {
    // We use UseUniqueRegister here to avoid register sharing with the output
    // register.
    InstructionOperand inputs[] = {g.UseUniqueRegister(node->InputAt(0)),
                                   g.UseUniqueRegister(node->InputAt(1)),
                                   g.UseUniqueRegister(node->InputAt(2)),
                                   g.UseUniqueRegister(node->InputAt(3))};

    InstructionOperand outputs[] = {
        g.DefineAsRegister(node),
        g.DefineAsRegister(NodeProperties::FindProjection(node, 1))};
    selector->Emit(pair_opcode, 2, outputs, 4, inputs);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    selector->Emit(single_opcode, g.DefineSameAsFirst(node),
                   g.UseRegister(node->InputAt(0)),
                   g.UseRegister(node->InputAt(2)));
  }
}

void InstructionSelector::VisitInt32PairAdd(Node* node) {
  VisitInt32PairBinop(this, kMipsAddPair, kMipsAdd, node);
}

void InstructionSelector::VisitInt32PairSub(Node* node) {
  VisitInt32PairBinop(this, kMipsSubPair, kMipsSub, node);
}

void InstructionSelector::VisitInt32PairMul(Node* node) {
  VisitInt32PairBinop(this, kMipsMulPair, kMipsMul, node);
}

// Shared routine for multiple shift operations.
static void VisitWord32PairShift(InstructionSelector* selector,
                                 InstructionCode opcode, Node* node) {
  MipsOperandGenerator g(selector);
  Int32Matcher m(node->InputAt(2));
  InstructionOperand shift_operand;
  if (m.HasValue()) {
    shift_operand = g.UseImmediate(m.node());
  } else {
    shift_operand = g.UseUniqueRegister(m.node());
  }

  // We use UseUniqueRegister here to avoid register sharing with the output
  // register.
  InstructionOperand inputs[] = {g.UseUniqueRegister(node->InputAt(0)),
                                 g.UseUniqueRegister(node->InputAt(1)),
                                 shift_operand};

  Node* projection1 = NodeProperties::FindProjection(node, 1);

  InstructionOperand outputs[2];
  InstructionOperand temps[1];
  int32_t output_count = 0;
  int32_t temp_count = 0;

  outputs[output_count++] = g.DefineAsRegister(node);
  if (projection1) {
    outputs[output_count++] = g.DefineAsRegister(projection1);
  } else {
    temps[temp_count++] = g.TempRegister();
  }

  selector->Emit(opcode, output_count, outputs, 3, inputs, temp_count, temps);
}

void InstructionSelector::VisitWord32PairShl(Node* node) {
  VisitWord32PairShift(this, kMipsShlPair, node);
}

void InstructionSelector::VisitWord32PairShr(Node* node) {
  VisitWord32PairShift(this, kMipsShrPair, node);
}

void InstructionSelector::VisitWord32PairSar(Node* node) {
  VisitWord32PairShift(this, kMipsSarPair, node);
}

void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitRRO(this, kMipsRor, node);
}

void InstructionSelector::VisitWord32Clz(Node* node) {
  VisitRR(this, kMipsClz, node);
}

void InstructionSelector::VisitWord32AtomicPairLoad(Node* node) {
  MipsOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  ArchOpcode opcode = kMipsWord32AtomicPairLoad;
  AddressingMode addressing_mode = kMode_MRI;
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index)};
  InstructionOperand temps[3];
  size_t temp_count = 0;
  temps[temp_count++] = g.TempRegister(a0);
  InstructionOperand outputs[2];
  size_t output_count = 0;

  Node* projection0 = NodeProperties::FindProjection(node, 0);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection0) {
    outputs[output_count++] = g.DefineAsFixed(projection0, v0);
  } else {
    temps[temp_count++] = g.TempRegister(v0);
  }
  if (projection1) {
    outputs[output_count++] = g.DefineAsFixed(projection1, v1);
  } else {
    temps[temp_count++] = g.TempRegister(v1);
  }
  Emit(code, output_count, outputs, arraysize(inputs), inputs, temp_count,
       temps);
}

void InstructionSelector::VisitWord32AtomicPairStore(Node* node) {
  MipsOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value_low = node->InputAt(2);
  Node* value_high = node->InputAt(3);

  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index),
                                 g.UseFixed(value_low, a1),
                                 g.UseFixed(value_high, a2)};
  InstructionOperand temps[] = {g.TempRegister(a0), g.TempRegister(),
                                g.TempRegister()};
  Emit(kMipsWord32AtomicPairStore | AddressingModeField::encode(kMode_MRI), 0,
       nullptr, arraysize(inputs), inputs, arraysize(temps), temps);
}

void InstructionSelector::VisitWord32AtomicPairAdd(Node* node) {
  VisitPairAtomicBinop(this, node, kMipsWord32AtomicPairAdd);
}

void InstructionSelector::VisitWord32AtomicPairSub(Node* node) {
  VisitPairAtomicBinop(this, node, kMipsWord32AtomicPairSub);
}

void InstructionSelector::VisitWord32AtomicPairAnd(Node* node) {
  VisitPairAtomicBinop(this, node, kMipsWord32AtomicPairAnd);
}

void InstructionSelector::VisitWord32AtomicPairOr(Node* node) {
  VisitPairAtomicBinop(this, node, kMipsWord32AtomicPairOr);
}

void InstructionSelector::VisitWord32AtomicPairXor(Node* node) {
  VisitPairAtomicBinop(this, node, kMipsWord32AtomicPairXor);
}

void InstructionSelector::VisitWord32AtomicPairExchange(Node* node) {
  VisitPairAtomicBinop(this, node, kMipsWord32AtomicPairExchange);
}

void InstructionSelector::VisitWord32AtomicPairCompareExchange(Node* node) {
  MipsOperandGenerator g(this);
  InstructionOperand inputs[] = {
      g.UseRegister(node->InputAt(0)),  g.UseRegister(node->InputAt(1)),
      g.UseFixed(node->InputAt(2), a1), g.UseFixed(node->InputAt(3), a2),
      g.UseFixed(node->InputAt(4), a3), g.UseUniqueRegister(node->InputAt(5))};

  InstructionCode code = kMipsWord32AtomicPairCompareExchange |
                         AddressingModeField::encode(kMode_MRI);
  Node* projection0 = NodeProperties::FindProjection(node, 0);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  InstructionOperand outputs[2];
  size_t output_count = 0;
  InstructionOperand temps[3];
  size_t temp_count = 0;
  temps[temp_count++] = g.TempRegister(a0);
  if (projection0) {
    outputs[output_count++] = g.DefineAsFixed(projection0, v0);
  } else {
    temps[temp_count++] = g.TempRegister(v0);
  }
  if (projection1) {
    outputs[output_count++] = g.DefineAsFixed(projection1, v1);
  } else {
    temps[temp_count++] = g.TempRegister(v1);
  }
  Emit(code, output_count, outputs, arraysize(inputs), inputs, temp_count,
       temps);
}

void InstructionSelector::VisitWord32ReverseBits(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord64ReverseBytes(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord32ReverseBytes(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsByteSwap32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitSimd128ReverseBytes(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitWord32Ctz(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsCtz, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitWord32Popcnt(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsPopcnt, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitInt32Add(Node* node) {
  MipsOperandGenerator g(this);
  Int32BinopMatcher m(node);

  if (IsMipsArchVariant(kMips32r6)) {
    // Select Lsa for (left + (left_of_right << imm)).
    if (m.right().opcode() == IrOpcode::kWord32Shl &&
        CanCover(node, m.left().node()) && CanCover(node, m.right().node())) {
      Int32BinopMatcher mright(m.right().node());
      if (mright.right().HasValue() && !m.left().HasValue()) {
        int32_t shift_value = static_cast<int32_t>(mright.right().Value());
        if (shift_value > 0 && shift_value <= 31) {
          Emit(kMipsLsa, g.DefineAsRegister(node),
               g.UseRegister(m.left().node()),
               g.UseRegister(mright.left().node()),
               g.TempImmediate(shift_value));
          return;
        }
      }
    }

    // Select Lsa for ((left_of_left << imm) + right).
    if (m.left().opcode() == IrOpcode::kWord32Shl &&
        CanCover(node, m.right().node()) && CanCover(node, m.left().node())) {
      Int32BinopMatcher mleft(m.left().node());
      if (mleft.right().HasValue() && !m.right().HasValue()) {
        int32_t shift_value = static_cast<int32_t>(mleft.right().Value());
        if (shift_value > 0 && shift_value <= 31) {
          Emit(kMipsLsa, g.DefineAsRegister(node),
               g.UseRegister(m.right().node()),
               g.UseRegister(mleft.left().node()),
               g.TempImmediate(shift_value));
          return;
        }
      }
    }
  }

  VisitBinop(this, node, kMipsAdd, true, kMipsAdd);
}

void InstructionSelector::VisitInt32Sub(Node* node) {
  VisitBinop(this, node, kMipsSub);
}

void InstructionSelector::VisitInt32Mul(Node* node) {
  MipsOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.right().HasValue() && m.right().Value() > 0) {
    uint32_t value = static_cast<uint32_t>(m.right().Value());
    if (base::bits::IsPowerOfTwo(value)) {
      Emit(kMipsShl | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(base::bits::WhichPowerOfTwo(value)));
      return;
    }
    if (base::bits::IsPowerOfTwo(value - 1) && IsMipsArchVariant(kMips32r6) &&
        value - 1 > 0 && value - 1 <= 31) {
      Emit(kMipsLsa, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.UseRegister(m.left().node()),
           g.TempImmediate(base::bits::WhichPowerOfTwo(value - 1)));
      return;
    }
    if (base::bits::IsPowerOfTwo(value + 1)) {
      InstructionOperand temp = g.TempRegister();
      Emit(kMipsShl | AddressingModeField::encode(kMode_None), temp,
           g.UseRegister(m.left().node()),
           g.TempImmediate(base::bits::WhichPowerOfTwo(value + 1)));
      Emit(kMipsSub | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), temp, g.UseRegister(m.left().node()));
      return;
    }
  }
  VisitRRR(this, kMipsMul, node);
}

void InstructionSelector::VisitInt32MulHigh(Node* node) {
  VisitRRR(this, kMipsMulHigh, node);
}

void InstructionSelector::VisitUint32MulHigh(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsMulHighU, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)),
       g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitInt32Div(Node* node) {
  MipsOperandGenerator g(this);
  Int32BinopMatcher m(node);
  Emit(kMipsDiv, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}

void InstructionSelector::VisitUint32Div(Node* node) {
  MipsOperandGenerator g(this);
  Int32BinopMatcher m(node);
  Emit(kMipsDivU, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}

void InstructionSelector::VisitInt32Mod(Node* node) {
  MipsOperandGenerator g(this);
  Int32BinopMatcher m(node);
  Emit(kMipsMod, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}

void InstructionSelector::VisitUint32Mod(Node* node) {
  MipsOperandGenerator g(this);
  Int32BinopMatcher m(node);
  Emit(kMipsModU, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}

void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  VisitRR(this, kMipsCvtDS, node);
}

void InstructionSelector::VisitRoundInt32ToFloat32(Node* node) {
  VisitRR(this, kMipsCvtSW, node);
}

void InstructionSelector::VisitRoundUint32ToFloat32(Node* node) {
  VisitRR(this, kMipsCvtSUw, node);
}

void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  VisitRR(this, kMipsCvtDW, node);
}

void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  VisitRR(this, kMipsCvtDUw, node);
}

void InstructionSelector::VisitTruncateFloat32ToInt32(Node* node) {
  VisitRR(this, kMipsTruncWS, node);
}

void InstructionSelector::VisitTruncateFloat32ToUint32(Node* node) {
  VisitRR(this, kMipsTruncUwS, node);
}

void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  MipsOperandGenerator g(this);
  Node* value = node->InputAt(0);
  // Match ChangeFloat64ToInt32(Float64Round##OP) to corresponding instruction
  // which does rounding and conversion to integer format.
  if (CanCover(node, value)) {
    switch (value->opcode()) {
      case IrOpcode::kFloat64RoundDown:
        Emit(kMipsFloorWD, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      case IrOpcode::kFloat64RoundUp:
        Emit(kMipsCeilWD, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      case IrOpcode::kFloat64RoundTiesEven:
        Emit(kMipsRoundWD, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      case IrOpcode::kFloat64RoundTruncate:
        Emit(kMipsTruncWD, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      default:
        break;
    }
    if (value->opcode() == IrOpcode::kChangeFloat32ToFloat64) {
      Node* next = value->InputAt(0);
      if (CanCover(value, next)) {
        // Match ChangeFloat64ToInt32(ChangeFloat32ToFloat64(Float64Round##OP))
        switch (next->opcode()) {
          case IrOpcode::kFloat32RoundDown:
            Emit(kMipsFloorWS, g.DefineAsRegister(node),
                 g.UseRegister(next->InputAt(0)));
            return;
          case IrOpcode::kFloat32RoundUp:
            Emit(kMipsCeilWS, g.DefineAsRegister(node),
                 g.UseRegister(next->InputAt(0)));
            return;
          case IrOpcode::kFloat32RoundTiesEven:
            Emit(kMipsRoundWS, g.DefineAsRegister(node),
                 g.UseRegister(next->InputAt(0)));
            return;
          case IrOpcode::kFloat32RoundTruncate:
            Emit(kMipsTruncWS, g.DefineAsRegister(node),
                 g.UseRegister(next->InputAt(0)));
            return;
          default:
            Emit(kMipsTruncWS, g.DefineAsRegister(node),
                 g.UseRegister(value->InputAt(0)));
            return;
        }
      } else {
        // Match float32 -> float64 -> int32 representation change path.
        Emit(kMipsTruncWS, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      }
    }
  }
  VisitRR(this, kMipsTruncWD, node);
}

void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  VisitRR(this, kMipsTruncUwD, node);
}

void InstructionSelector::VisitTruncateFloat64ToUint32(Node* node) {
  VisitRR(this, kMipsTruncUwD, node);
}

void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  MipsOperandGenerator g(this);
  Node* value = node->InputAt(0);
  // Match TruncateFloat64ToFloat32(ChangeInt32ToFloat64) to corresponding
  // instruction.
  if (CanCover(node, value) &&
      value->opcode() == IrOpcode::kChangeInt32ToFloat64) {
    Emit(kMipsCvtSW, g.DefineAsRegister(node),
         g.UseRegister(value->InputAt(0)));
    return;
  }
  VisitRR(this, kMipsCvtSD, node);
}

void InstructionSelector::VisitTruncateFloat64ToWord32(Node* node) {
  VisitRR(this, kArchTruncateDoubleToI, node);
}

void InstructionSelector::VisitRoundFloat64ToInt32(Node* node) {
  VisitRR(this, kMipsTruncWD, node);
}

void InstructionSelector::VisitBitcastFloat32ToInt32(Node* node) {
  VisitRR(this, kMipsFloat64ExtractLowWord32, node);
}

void InstructionSelector::VisitBitcastInt32ToFloat32(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsFloat64InsertLowWord32, g.DefineAsRegister(node),
       ImmediateOperand(ImmediateOperand::INLINE, 0),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitFloat32Add(Node* node) {
  MipsOperandGenerator g(this);
  if (IsMipsArchVariant(kMips32r2)) {  // Select Madd.S(z, x, y).
    Float32BinopMatcher m(node);
    if (m.left().IsFloat32Mul() && CanCover(node, m.left().node())) {
      // For Add.S(Mul.S(x, y), z):
      Float32BinopMatcher mleft(m.left().node());
      Emit(kMipsMaddS, g.DefineAsRegister(node),
           g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    }
    if (m.right().IsFloat32Mul() && CanCover(node, m.right().node())) {
      // For Add.S(x, Mul.S(y, z)):
      Float32BinopMatcher mright(m.right().node());
      Emit(kMipsMaddS, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.UseRegister(mright.left().node()),
           g.UseRegister(mright.right().node()));
      return;
    }
  }
  VisitRRR(this, kMipsAddS, node);
}

void InstructionSelector::VisitFloat64Add(Node* node) {
  MipsOperandGenerator g(this);
  if (IsMipsArchVariant(kMips32r2)) {  // Select Madd.S(z, x, y).
    Float64BinopMatcher m(node);
    if (m.left().IsFloat64Mul() && CanCover(node, m.left().node())) {
      // For Add.D(Mul.D(x, y), z):
      Float64BinopMatcher mleft(m.left().node());
      Emit(kMipsMaddD, g.DefineAsRegister(node),
           g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    }
    if (m.right().IsFloat64Mul() && CanCover(node, m.right().node())) {
      // For Add.D(x, Mul.D(y, z)):
      Float64BinopMatcher mright(m.right().node());
      Emit(kMipsMaddD, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.UseRegister(mright.left().node()),
           g.UseRegister(mright.right().node()));
      return;
    }
  }
  VisitRRR(this, kMipsAddD, node);
}

void InstructionSelector::VisitFloat32Sub(Node* node) {
  MipsOperandGenerator g(this);
  if (IsMipsArchVariant(kMips32r2)) {  // Select Madd.S(z, x, y).
    Float32BinopMatcher m(node);
    if (m.left().IsFloat32Mul() && CanCover(node, m.left().node())) {
      // For Sub.S(Mul.S(x,y), z) select Msub.S(z, x, y).
      Float32BinopMatcher mleft(m.left().node());
      Emit(kMipsMsubS, g.DefineAsRegister(node),
           g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    }
  }
  VisitRRR(this, kMipsSubS, node);
}

void InstructionSelector::VisitFloat64Sub(Node* node) {
  MipsOperandGenerator g(this);
  if (IsMipsArchVariant(kMips32r2)) {  // Select Madd.S(z, x, y).
    Float64BinopMatcher m(node);
    if (m.left().IsFloat64Mul() && CanCover(node, m.left().node())) {
      // For Sub.D(Mul.S(x,y), z) select Msub.D(z, x, y).
      Float64BinopMatcher mleft(m.left().node());
      Emit(kMipsMsubD, g.DefineAsRegister(node),
           g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    }
  }
  VisitRRR(this, kMipsSubD, node);
}

void InstructionSelector::VisitFloat32Mul(Node* node) {
  VisitRRR(this, kMipsMulS, node);
}

void InstructionSelector::VisitFloat64Mul(Node* node) {
  VisitRRR(this, kMipsMulD, node);
}

void InstructionSelector::VisitFloat32Div(Node* node) {
  VisitRRR(this, kMipsDivS, node);
}

void InstructionSelector::VisitFloat64Div(Node* node) {
  VisitRRR(this, kMipsDivD, node);
}

void InstructionSelector::VisitFloat64Mod(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsModD, g.DefineAsFixed(node, f0), g.UseFixed(node->InputAt(0), f12),
       g.UseFixed(node->InputAt(1), f14))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat32Max(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsFloat32Max, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitFloat64Max(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsFloat64Max, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitFloat32Min(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsFloat32Min, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitFloat64Min(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsFloat64Min, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitFloat32Abs(Node* node) {
  VisitRR(this, kMipsAbsS, node);
}

void InstructionSelector::VisitFloat64Abs(Node* node) {
  VisitRR(this, kMipsAbsD, node);
}

void InstructionSelector::VisitFloat32Sqrt(Node* node) {
  VisitRR(this, kMipsSqrtS, node);
}

void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  VisitRR(this, kMipsSqrtD, node);
}

void InstructionSelector::VisitFloat32RoundDown(Node* node) {
  VisitRR(this, kMipsFloat32RoundDown, node);
}

void InstructionSelector::VisitFloat64RoundDown(Node* node) {
  VisitRR(this, kMipsFloat64RoundDown, node);
}

void InstructionSelector::VisitFloat32RoundUp(Node* node) {
  VisitRR(this, kMipsFloat32RoundUp, node);
}

void InstructionSelector::VisitFloat64RoundUp(Node* node) {
  VisitRR(this, kMipsFloat64RoundUp, node);
}

void InstructionSelector::VisitFloat32RoundTruncate(Node* node) {
  VisitRR(this, kMipsFloat32RoundTruncate, node);
}

void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  VisitRR(this, kMipsFloat64RoundTruncate, node);
}

void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitFloat32RoundTiesEven(Node* node) {
  VisitRR(this, kMipsFloat32RoundTiesEven, node);
}

void InstructionSelector::VisitFloat64RoundTiesEven(Node* node) {
  VisitRR(this, kMipsFloat64RoundTiesEven, node);
}

void InstructionSelector::VisitFloat32Neg(Node* node) {
  VisitRR(this, kMipsNegS, node);
}

void InstructionSelector::VisitFloat64Neg(Node* node) {
  VisitRR(this, kMipsNegD, node);
}

void InstructionSelector::VisitFloat64Ieee754Binop(Node* node,
                                                   InstructionCode opcode) {
  MipsOperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, f0), g.UseFixed(node->InputAt(0), f2),
       g.UseFixed(node->InputAt(1), f4))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat64Ieee754Unop(Node* node,
                                                  InstructionCode opcode) {
  MipsOperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, f0), g.UseFixed(node->InputAt(0), f12))
      ->MarkAsCall();
}

void InstructionSelector::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* call_descriptor,
    Node* node) {
  MipsOperandGenerator g(this);

  // Prepare for C function call.
  if (call_descriptor->IsCFunctionCall()) {
    Emit(kArchPrepareCallCFunction | MiscField::encode(static_cast<int>(
                                         call_descriptor->ParameterCount())),
         0, nullptr, 0, nullptr);

    // Poke any stack arguments.
    int slot = kCArgSlotCount;
    for (PushParameter input : (*arguments)) {
      if (input.node) {
        Emit(kMipsStoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
             g.TempImmediate(slot << kSystemPointerSizeLog2));
        ++slot;
      }
    }
  } else {
    // Possibly align stack here for functions.
    int push_count = static_cast<int>(call_descriptor->StackParameterCount());
    if (push_count > 0) {
      // Calculate needed space
      int stack_size = 0;
      for (size_t n = 0; n < arguments->size(); ++n) {
        PushParameter input = (*arguments)[n];
        if (input.node) {
          stack_size += input.location.GetSizeInPointers();
        }
      }
      Emit(kMipsStackClaim, g.NoOutput(),
           g.TempImmediate(stack_size << kSystemPointerSizeLog2));
    }
    for (size_t n = 0; n < arguments->size(); ++n) {
      PushParameter input = (*arguments)[n];
      if (input.node) {
        Emit(kMipsStoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
             g.TempImmediate(n << kSystemPointerSizeLog2));
      }
    }
  }
}

void InstructionSelector::EmitPrepareResults(
    ZoneVector<PushParameter>* results, const CallDescriptor* call_descriptor,
    Node* node) {
  MipsOperandGenerator g(this);

  int reverse_slot = 0;
  for (PushParameter output : *results) {
    if (!output.location.IsCallerFrameSlot()) continue;
    // Skip any alignment holes in nodes.
    if (output.node != nullptr) {
      DCHECK(!call_descriptor->IsCFunctionCall());
      if (output.location.GetType() == MachineType::Float32()) {
        MarkAsFloat32(output.node);
      } else if (output.location.GetType() == MachineType::Float64()) {
        MarkAsFloat64(output.node);
      }
      Emit(kMipsPeek, g.DefineAsRegister(output.node),
           g.UseImmediate(reverse_slot));
    }
    reverse_slot += output.location.GetSizeInPointers();
  }
}

bool InstructionSelector::IsTailCallAddressImmediate() { return false; }

int InstructionSelector::GetTempsCountForTailCallFromJSFunction() { return 3; }

void InstructionSelector::VisitUnalignedLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  MipsOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  ArchOpcode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      UNREACHABLE();
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsUnsigned() ? kMipsUlhu : kMipsUlh;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord32:
      opcode = kMipsUlw;
      break;
    case MachineRepresentation::kFloat32:
      opcode = kMipsUlwc1;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kMipsUldc1;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kMipsMsaLd;
      break;
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:         // Fall through.
    case MachineRepresentation::kWord64:             // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kMipsAdd | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

void InstructionSelector::VisitUnalignedStore(Node* node) {
  MipsOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  UnalignedStoreRepresentation rep = UnalignedStoreRepresentationOf(node->op());

  // TODO(mips): I guess this could be done in a better way.
  ArchOpcode opcode = kArchNop;
  switch (rep) {
    case MachineRepresentation::kFloat32:
      opcode = kMipsUswc1;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kMipsUsdc1;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      UNREACHABLE();
    case MachineRepresentation::kWord16:
      opcode = kMipsUsh;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord32:
      opcode = kMipsUsw;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kMipsMsaSt;
      break;
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:         // Fall through.
    case MachineRepresentation::kWord64:             // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         g.UseRegister(base), g.UseImmediate(index),
         g.UseRegisterOrImmediateZero(value));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kMipsAdd | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired store opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         addr_reg, g.TempImmediate(0), g.UseRegisterOrImmediateZero(value));
  }
}

namespace {
// Shared routine for multiple compare operations.
static void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                         InstructionOperand left, InstructionOperand right,
                         FlagsContinuation* cont) {
  selector->EmitWithContinuation(opcode, left, right, cont);
}

// Shared routine for multiple float32 compare operations.
void VisitFloat32Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  MipsOperandGenerator g(selector);
  Float32BinopMatcher m(node);
  InstructionOperand lhs, rhs;

  lhs = m.left().IsZero() ? g.UseImmediate(m.left().node())
                          : g.UseRegister(m.left().node());
  rhs = m.right().IsZero() ? g.UseImmediate(m.right().node())
                           : g.UseRegister(m.right().node());
  VisitCompare(selector, kMipsCmpS, lhs, rhs, cont);
}

// Shared routine for multiple float64 compare operations.
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  MipsOperandGenerator g(selector);
  Float64BinopMatcher m(node);
  InstructionOperand lhs, rhs;

  lhs = m.left().IsZero() ? g.UseImmediate(m.left().node())
                          : g.UseRegister(m.left().node());
  rhs = m.right().IsZero() ? g.UseImmediate(m.right().node())
                           : g.UseRegister(m.right().node());
  VisitCompare(selector, kMipsCmpD, lhs, rhs, cont);
}

// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont,
                      bool commutative) {
  MipsOperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  // Match immediates on left or right side of comparison.
  if (g.CanBeImmediate(right, opcode)) {
    if (opcode == kMipsTst) {
      VisitCompare(selector, opcode, g.UseRegister(left), g.UseImmediate(right),
                   cont);
    } else {
      switch (cont->condition()) {
        case kEqual:
        case kNotEqual:
          if (cont->IsSet()) {
            VisitCompare(selector, opcode, g.UseRegister(left),
                         g.UseImmediate(right), cont);
          } else {
            VisitCompare(selector, opcode, g.UseRegister(left),
                         g.UseRegister(right), cont);
          }
          break;
        case kSignedLessThan:
        case kSignedGreaterThanOrEqual:
        case kUnsignedLessThan:
        case kUnsignedGreaterThanOrEqual:
          VisitCompare(selector, opcode, g.UseRegister(left),
                       g.UseImmediate(right), cont);
          break;
        default:
          VisitCompare(selector, opcode, g.UseRegister(left),
                       g.UseRegister(right), cont);
      }
    }
  } else if (g.CanBeImmediate(left, opcode)) {
    if (!commutative) cont->Commute();
    if (opcode == kMipsTst) {
      VisitCompare(selector, opcode, g.UseRegister(right), g.UseImmediate(left),
                   cont);
    } else {
      switch (cont->condition()) {
        case kEqual:
        case kNotEqual:
          if (cont->IsSet()) {
            VisitCompare(selector, opcode, g.UseRegister(right),
                         g.UseImmediate(left), cont);
          } else {
            VisitCompare(selector, opcode, g.UseRegister(right),
                         g.UseRegister(left), cont);
          }
          break;
        case kSignedLessThan:
        case kSignedGreaterThanOrEqual:
        case kUnsignedLessThan:
        case kUnsignedGreaterThanOrEqual:
          VisitCompare(selector, opcode, g.UseRegister(right),
                       g.UseImmediate(left), cont);
          break;
        default:
          VisitCompare(selector, opcode, g.UseRegister(right),
                       g.UseRegister(left), cont);
      }
    }
  } else {
    VisitCompare(selector, opcode, g.UseRegister(left), g.UseRegister(right),
                 cont);
  }
}

void VisitWordCompare(InstructionSelector* selector, Node* node,
                      FlagsContinuation* cont) {
  VisitWordCompare(selector, node, kMipsCmp, cont, false);
}

}  // namespace

void InstructionSelector::VisitStackPointerGreaterThan(
    Node* node, FlagsContinuation* cont) {
  StackCheckKind kind = StackCheckKindOf(node->op());
  InstructionCode opcode =
      kArchStackPointerGreaterThan | MiscField::encode(static_cast<int>(kind));

  MipsOperandGenerator g(this);

  // No outputs.
  InstructionOperand* const outputs = nullptr;
  const int output_count = 0;

  // Applying an offset to this stack check requires a temp register. Offsets
  // are only applied to the first stack check. If applying an offset, we must
  // ensure the input and temp registers do not alias, thus kUniqueRegister.
  InstructionOperand temps[] = {g.TempRegister()};
  const int temp_count = (kind == StackCheckKind::kJSFunctionEntry ? 1 : 0);
  const auto register_mode = (kind == StackCheckKind::kJSFunctionEntry)
                                 ? OperandGenerator::kUniqueRegister
                                 : OperandGenerator::kRegister;

  Node* const value = node->InputAt(0);
  InstructionOperand inputs[] = {g.UseRegisterWithMode(value, register_mode)};
  static constexpr int input_count = arraysize(inputs);

  EmitWithContinuation(opcode, output_count, outputs, input_count, inputs,
                       temp_count, temps, cont);
}

// Shared routine for word comparisons against zero.
void InstructionSelector::VisitWordCompareZero(Node* user, Node* value,
                                               FlagsContinuation* cont) {
  if (CanCover(user, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord32Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitWordCompare(this, value, cont);
      case IrOpcode::kInt32LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWordCompare(this, value, cont);
      case IrOpcode::kInt32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWordCompare(this, value, cont);
      case IrOpcode::kUint32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWordCompare(this, value, cont);
      case IrOpcode::kUint32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWordCompare(this, value, cont);
      case IrOpcode::kFloat32Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat64Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitFloat64Compare(this, value, cont);
      case IrOpcode::kFloat64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitFloat64Compare(this, value, cont);
      case IrOpcode::kFloat64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
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
          if (!result || IsDefined(result)) {
            switch (node->opcode()) {
              case IrOpcode::kInt32AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kMipsAddOvf, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kMipsSubOvf, cont);
              case IrOpcode::kInt32MulWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kMipsMulOvf, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kWord32And:
        return VisitWordCompare(this, value, kMipsTst, cont, true);
      case IrOpcode::kStackPointerGreaterThan:
        cont->OverwriteAndNegateIfEqual(kStackPointerGreaterThanCondition);
        return VisitStackPointerGreaterThan(value, cont);
      default:
        break;
    }
  }

  // Continuation could not be combined with a compare, emit compare against 0.
  MipsOperandGenerator g(this);
  InstructionOperand const value_operand = g.UseRegister(value);
  EmitWithContinuation(kMipsCmp, value_operand, g.TempImmediate(0), cont);
}

void InstructionSelector::VisitSwitch(Node* node, const SwitchInfo& sw) {
  MipsOperandGenerator g(this);
  InstructionOperand value_operand = g.UseRegister(node->InputAt(0));

  // Emit either ArchTableSwitch or ArchBinarySearchSwitch.
  if (enable_switch_jump_table_ == kEnableSwitchJumpTable) {
    static const size_t kMaxTableSwitchValueRange = 2 << 16;
    size_t table_space_cost = 9 + sw.value_range();
    size_t table_time_cost = 3;
    size_t lookup_space_cost = 2 + 2 * sw.case_count();
    size_t lookup_time_cost = sw.case_count();
    if (sw.case_count() > 0 &&
        table_space_cost + 3 * table_time_cost <=
            lookup_space_cost + 3 * lookup_time_cost &&
        sw.min_value() > std::numeric_limits<int32_t>::min() &&
        sw.value_range() <= kMaxTableSwitchValueRange) {
      InstructionOperand index_operand = value_operand;
      if (sw.min_value()) {
        index_operand = g.TempRegister();
        Emit(kMipsSub, index_operand, value_operand,
             g.TempImmediate(sw.min_value()));
      }
      // Generate a table lookup.
      return EmitTableSwitch(sw, index_operand);
    }
  }

  // Generate a tree of conditional jumps.
  return EmitBinarySearchSwitch(std::move(sw), value_operand);
}

void InstructionSelector::VisitWord32Equal(Node* const node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) {
    return VisitWordCompareZero(m.node(), m.left().node(), &cont);
  }
  VisitWordCompare(this, node, &cont);
}

void InstructionSelector::VisitInt32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWordCompare(this, node, &cont);
}

void InstructionSelector::VisitInt32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, &cont);
}

void InstructionSelector::VisitUint32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWordCompare(this, node, &cont);
}

void InstructionSelector::VisitUint32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWordCompare(this, node, &cont);
}

void InstructionSelector::VisitInt32AddWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kMipsAddOvf, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kMipsAddOvf, &cont);
}

void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kMipsSubOvf, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kMipsSubOvf, &cont);
}

void InstructionSelector::VisitInt32MulWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kMipsMulOvf, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kMipsMulOvf, &cont);
}

void InstructionSelector::VisitFloat32Equal(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64Equal(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64ExtractLowWord32(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsFloat64ExtractLowWord32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitFloat64ExtractHighWord32(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsFloat64ExtractHighWord32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitFloat64InsertLowWord32(Node* node) {
  MipsOperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Emit(kMipsFloat64InsertLowWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.UseRegister(right));
}

void InstructionSelector::VisitFloat64InsertHighWord32(Node* node) {
  MipsOperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Emit(kMipsFloat64InsertHighWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.UseRegister(right));
}

void InstructionSelector::VisitFloat64SilenceNaN(Node* node) {
  MipsOperandGenerator g(this);
  Node* left = node->InputAt(0);
  InstructionOperand temps[] = {g.TempRegister()};
  Emit(kMipsFloat64SilenceNaN, g.DefineSameAsFirst(node), g.UseRegister(left),
       arraysize(temps), temps);
}

void InstructionSelector::VisitMemoryBarrier(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsSync, g.NoOutput());
}

void InstructionSelector::VisitWord32AtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  MipsOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  ArchOpcode opcode = kArchNop;
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

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kMipsAdd | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

void InstructionSelector::VisitWord32AtomicStore(Node* node) {
  MachineRepresentation rep = AtomicStoreRepresentationOf(node->op());
  MipsOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  ArchOpcode opcode = kArchNop;
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

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         g.UseRegister(base), g.UseImmediate(index),
         g.UseRegisterOrImmediateZero(value));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kMipsAdd | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired store opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         addr_reg, g.TempImmediate(0), g.UseRegisterOrImmediateZero(value));
  }
}

void InstructionSelector::VisitWord32AtomicExchange(Node* node) {
  MipsOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  ArchOpcode opcode = kArchNop;
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
    return;
  }

  AddressingMode addressing_mode = kMode_MRI;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);
  InstructionOperand outputs[1];
  outputs[0] = g.UseUniqueRegister(node);
  InstructionOperand temp[3];
  temp[0] = g.TempRegister();
  temp[1] = g.TempRegister();
  temp[2] = g.TempRegister();
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 1, outputs, input_count, inputs, 3, temp);
}

void InstructionSelector::VisitWord32AtomicCompareExchange(Node* node) {
  MipsOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* old_value = node->InputAt(2);
  Node* new_value = node->InputAt(3);
  ArchOpcode opcode = kArchNop;
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
    return;
  }

  AddressingMode addressing_mode = kMode_MRI;
  InstructionOperand inputs[4];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(old_value);
  inputs[input_count++] = g.UseUniqueRegister(new_value);
  InstructionOperand outputs[1];
  outputs[0] = g.UseUniqueRegister(node);
  InstructionOperand temp[3];
  temp[0] = g.TempRegister();
  temp[1] = g.TempRegister();
  temp[2] = g.TempRegister();
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 1, outputs, input_count, inputs, 3, temp);
}

void InstructionSelector::VisitWord32AtomicBinaryOperation(
    Node* node, ArchOpcode int8_op, ArchOpcode uint8_op, ArchOpcode int16_op,
    ArchOpcode uint16_op, ArchOpcode word32_op) {
  MipsOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  ArchOpcode opcode = kArchNop;
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
    return;
  }

  AddressingMode addressing_mode = kMode_MRI;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);
  InstructionOperand outputs[1];
  outputs[0] = g.UseUniqueRegister(node);
  InstructionOperand temps[4];
  temps[0] = g.TempRegister();
  temps[1] = g.TempRegister();
  temps[2] = g.TempRegister();
  temps[3] = g.TempRegister();
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 1, outputs, input_count, inputs, 4, temps);
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

void InstructionSelector::VisitInt32AbsWithOverflow(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitInt64AbsWithOverflow(Node* node) {
  UNREACHABLE();
}

#define SIMD_TYPE_LIST(V) \
  V(F32x4)                \
  V(I32x4)                \
  V(I16x8)                \
  V(I8x16)

#define SIMD_UNOP_LIST(V)                                \
  V(F64x2Abs, kMipsF64x2Abs)                             \
  V(F64x2Neg, kMipsF64x2Neg)                             \
  V(F64x2Sqrt, kMipsF64x2Sqrt)                           \
  V(I64x2Neg, kMipsI64x2Neg)                             \
  V(F32x4SConvertI32x4, kMipsF32x4SConvertI32x4)         \
  V(F32x4UConvertI32x4, kMipsF32x4UConvertI32x4)         \
  V(F32x4Abs, kMipsF32x4Abs)                             \
  V(F32x4Neg, kMipsF32x4Neg)                             \
  V(F32x4Sqrt, kMipsF32x4Sqrt)                           \
  V(F32x4RecipApprox, kMipsF32x4RecipApprox)             \
  V(F32x4RecipSqrtApprox, kMipsF32x4RecipSqrtApprox)     \
  V(I32x4SConvertF32x4, kMipsI32x4SConvertF32x4)         \
  V(I32x4UConvertF32x4, kMipsI32x4UConvertF32x4)         \
  V(I32x4Neg, kMipsI32x4Neg)                             \
  V(I32x4SConvertI16x8Low, kMipsI32x4SConvertI16x8Low)   \
  V(I32x4SConvertI16x8High, kMipsI32x4SConvertI16x8High) \
  V(I32x4UConvertI16x8Low, kMipsI32x4UConvertI16x8Low)   \
  V(I32x4UConvertI16x8High, kMipsI32x4UConvertI16x8High) \
  V(I16x8Neg, kMipsI16x8Neg)                             \
  V(I16x8SConvertI8x16Low, kMipsI16x8SConvertI8x16Low)   \
  V(I16x8SConvertI8x16High, kMipsI16x8SConvertI8x16High) \
  V(I16x8UConvertI8x16Low, kMipsI16x8UConvertI8x16Low)   \
  V(I16x8UConvertI8x16High, kMipsI16x8UConvertI8x16High) \
  V(I8x16Neg, kMipsI8x16Neg)                             \
  V(S128Not, kMipsS128Not)                               \
  V(S1x4AnyTrue, kMipsS1x4AnyTrue)                       \
  V(S1x4AllTrue, kMipsS1x4AllTrue)                       \
  V(S1x8AnyTrue, kMipsS1x8AnyTrue)                       \
  V(S1x8AllTrue, kMipsS1x8AllTrue)                       \
  V(S1x16AnyTrue, kMipsS1x16AnyTrue)                     \
  V(S1x16AllTrue, kMipsS1x16AllTrue)

#define SIMD_SHIFT_OP_LIST(V) \
  V(I64x2Shl)                 \
  V(I64x2ShrS)                \
  V(I64x2ShrU)                \
  V(I32x4Shl)                 \
  V(I32x4ShrS)                \
  V(I32x4ShrU)                \
  V(I16x8Shl)                 \
  V(I16x8ShrS)                \
  V(I16x8ShrU)                \
  V(I8x16Shl)                 \
  V(I8x16ShrS)                \
  V(I8x16ShrU)

#define SIMD_BINOP_LIST(V)                             \
  V(F64x2Add, kMipsF64x2Add)                           \
  V(F64x2Sub, kMipsF64x2Sub)                           \
  V(F64x2Mul, kMipsF64x2Mul)                           \
  V(F64x2Div, kMipsF64x2Div)                           \
  V(F64x2Min, kMipsF64x2Min)                           \
  V(F64x2Max, kMipsF64x2Max)                           \
  V(F64x2Eq, kMipsF64x2Eq)                             \
  V(F64x2Ne, kMipsF64x2Ne)                             \
  V(F64x2Lt, kMipsF64x2Lt)                             \
  V(F64x2Le, kMipsF64x2Le)                             \
  V(I64x2Add, kMipsI64x2Add)                           \
  V(I64x2Sub, kMipsI64x2Sub)                           \
  V(I64x2Mul, kMipsI64x2Mul)                           \
  V(F32x4Add, kMipsF32x4Add)                           \
  V(F32x4AddHoriz, kMipsF32x4AddHoriz)                 \
  V(F32x4Sub, kMipsF32x4Sub)                           \
  V(F32x4Mul, kMipsF32x4Mul)                           \
  V(F32x4Div, kMipsF32x4Div)                           \
  V(F32x4Max, kMipsF32x4Max)                           \
  V(F32x4Min, kMipsF32x4Min)                           \
  V(F32x4Eq, kMipsF32x4Eq)                             \
  V(F32x4Ne, kMipsF32x4Ne)                             \
  V(F32x4Lt, kMipsF32x4Lt)                             \
  V(F32x4Le, kMipsF32x4Le)                             \
  V(I32x4Add, kMipsI32x4Add)                           \
  V(I32x4AddHoriz, kMipsI32x4AddHoriz)                 \
  V(I32x4Sub, kMipsI32x4Sub)                           \
  V(I32x4Mul, kMipsI32x4Mul)                           \
  V(I32x4MaxS, kMipsI32x4MaxS)                         \
  V(I32x4MinS, kMipsI32x4MinS)                         \
  V(I32x4MaxU, kMipsI32x4MaxU)                         \
  V(I32x4MinU, kMipsI32x4MinU)                         \
  V(I32x4Eq, kMipsI32x4Eq)                             \
  V(I32x4Ne, kMipsI32x4Ne)                             \
  V(I32x4GtS, kMipsI32x4GtS)                           \
  V(I32x4GeS, kMipsI32x4GeS)                           \
  V(I32x4GtU, kMipsI32x4GtU)                           \
  V(I32x4GeU, kMipsI32x4GeU)                           \
  V(I32x4Abs, kMipsI32x4Abs)                           \
  V(I16x8Add, kMipsI16x8Add)                           \
  V(I16x8AddSaturateS, kMipsI16x8AddSaturateS)         \
  V(I16x8AddSaturateU, kMipsI16x8AddSaturateU)         \
  V(I16x8AddHoriz, kMipsI16x8AddHoriz)                 \
  V(I16x8Sub, kMipsI16x8Sub)                           \
  V(I16x8SubSaturateS, kMipsI16x8SubSaturateS)         \
  V(I16x8SubSaturateU, kMipsI16x8SubSaturateU)         \
  V(I16x8Mul, kMipsI16x8Mul)                           \
  V(I16x8MaxS, kMipsI16x8MaxS)                         \
  V(I16x8MinS, kMipsI16x8MinS)                         \
  V(I16x8MaxU, kMipsI16x8MaxU)                         \
  V(I16x8MinU, kMipsI16x8MinU)                         \
  V(I16x8Eq, kMipsI16x8Eq)                             \
  V(I16x8Ne, kMipsI16x8Ne)                             \
  V(I16x8GtS, kMipsI16x8GtS)                           \
  V(I16x8GeS, kMipsI16x8GeS)                           \
  V(I16x8GtU, kMipsI16x8GtU)                           \
  V(I16x8GeU, kMipsI16x8GeU)                           \
  V(I16x8SConvertI32x4, kMipsI16x8SConvertI32x4)       \
  V(I16x8UConvertI32x4, kMipsI16x8UConvertI32x4)       \
  V(I16x8RoundingAverageU, kMipsI16x8RoundingAverageU) \
  V(I16x8Abs, kMipsI16x8Abs)                           \
  V(I8x16Add, kMipsI8x16Add)                           \
  V(I8x16AddSaturateS, kMipsI8x16AddSaturateS)         \
  V(I8x16AddSaturateU, kMipsI8x16AddSaturateU)         \
  V(I8x16Sub, kMipsI8x16Sub)                           \
  V(I8x16SubSaturateS, kMipsI8x16SubSaturateS)         \
  V(I8x16SubSaturateU, kMipsI8x16SubSaturateU)         \
  V(I8x16Mul, kMipsI8x16Mul)                           \
  V(I8x16MaxS, kMipsI8x16MaxS)                         \
  V(I8x16MinS, kMipsI8x16MinS)                         \
  V(I8x16MaxU, kMipsI8x16MaxU)                         \
  V(I8x16MinU, kMipsI8x16MinU)                         \
  V(I8x16Eq, kMipsI8x16Eq)                             \
  V(I8x16Ne, kMipsI8x16Ne)                             \
  V(I8x16GtS, kMipsI8x16GtS)                           \
  V(I8x16GeS, kMipsI8x16GeS)                           \
  V(I8x16GtU, kMipsI8x16GtU)                           \
  V(I8x16GeU, kMipsI8x16GeU)                           \
  V(I8x16RoundingAverageU, kMipsI8x16RoundingAverageU) \
  V(I8x16SConvertI16x8, kMipsI8x16SConvertI16x8)       \
  V(I8x16UConvertI16x8, kMipsI8x16UConvertI16x8)       \
  V(I8x16Abs, kMipsI8x16Abs)                           \
  V(S128And, kMipsS128And)                             \
  V(S128Or, kMipsS128Or)                               \
  V(S128Xor, kMipsS128Xor)                             \
  V(S128AndNot, kMipsS128AndNot)

void InstructionSelector::VisitS128Zero(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsS128Zero, g.DefineSameAsFirst(node));
}

#define SIMD_VISIT_SPLAT(Type)                               \
  void InstructionSelector::Visit##Type##Splat(Node* node) { \
    VisitRR(this, kMips##Type##Splat, node);                 \
  }
SIMD_TYPE_LIST(SIMD_VISIT_SPLAT)
SIMD_VISIT_SPLAT(F64x2)
#undef SIMD_VISIT_SPLAT

#define SIMD_VISIT_EXTRACT_LANE(Type, Sign)                              \
  void InstructionSelector::Visit##Type##ExtractLane##Sign(Node* node) { \
    VisitRRI(this, kMips##Type##ExtractLane##Sign, node);                \
  }
SIMD_VISIT_EXTRACT_LANE(F64x2, )
SIMD_VISIT_EXTRACT_LANE(F32x4, )
SIMD_VISIT_EXTRACT_LANE(I32x4, )
SIMD_VISIT_EXTRACT_LANE(I16x8, U)
SIMD_VISIT_EXTRACT_LANE(I16x8, S)
SIMD_VISIT_EXTRACT_LANE(I8x16, U)
SIMD_VISIT_EXTRACT_LANE(I8x16, S)
#undef SIMD_VISIT_EXTRACT_LANE

#define SIMD_VISIT_REPLACE_LANE(Type)                              \
  void InstructionSelector::Visit##Type##ReplaceLane(Node* node) { \
    VisitRRIR(this, kMips##Type##ReplaceLane, node);               \
  }
SIMD_TYPE_LIST(SIMD_VISIT_REPLACE_LANE)
SIMD_VISIT_REPLACE_LANE(F64x2)
#undef SIMD_VISIT_REPLACE_LANE

#define SIMD_VISIT_UNOP(Name, instruction)            \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRR(this, instruction, node);                 \
  }
SIMD_UNOP_LIST(SIMD_VISIT_UNOP)
#undef SIMD_VISIT_UNOP

#define SIMD_VISIT_SHIFT_OP(Name)                     \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRRI(this, kMips##Name, node);                \
  }
SIMD_SHIFT_OP_LIST(SIMD_VISIT_SHIFT_OP)
#undef SIMD_VISIT_SHIFT_OP

#define SIMD_VISIT_BINOP(Name, instruction)           \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRRR(this, instruction, node);                \
  }
SIMD_BINOP_LIST(SIMD_VISIT_BINOP)
#undef SIMD_VISIT_BINOP

void InstructionSelector::VisitS128Select(Node* node) {
  VisitRRRR(this, kMipsS128Select, node);
}

namespace {

struct ShuffleEntry {
  uint8_t shuffle[kSimd128Size];
  ArchOpcode opcode;
};

static const ShuffleEntry arch_shuffles[] = {
    {{0, 1, 2, 3, 16, 17, 18, 19, 4, 5, 6, 7, 20, 21, 22, 23},
     kMipsS32x4InterleaveRight},
    {{8, 9, 10, 11, 24, 25, 26, 27, 12, 13, 14, 15, 28, 29, 30, 31},
     kMipsS32x4InterleaveLeft},
    {{0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27},
     kMipsS32x4PackEven},
    {{4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31},
     kMipsS32x4PackOdd},
    {{0, 1, 2, 3, 16, 17, 18, 19, 8, 9, 10, 11, 24, 25, 26, 27},
     kMipsS32x4InterleaveEven},
    {{4, 5, 6, 7, 20, 21, 22, 23, 12, 13, 14, 15, 28, 29, 30, 31},
     kMipsS32x4InterleaveOdd},

    {{0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21, 6, 7, 22, 23},
     kMipsS16x8InterleaveRight},
    {{8, 9, 24, 25, 10, 11, 26, 27, 12, 13, 28, 29, 14, 15, 30, 31},
     kMipsS16x8InterleaveLeft},
    {{0, 1, 4, 5, 8, 9, 12, 13, 16, 17, 20, 21, 24, 25, 28, 29},
     kMipsS16x8PackEven},
    {{2, 3, 6, 7, 10, 11, 14, 15, 18, 19, 22, 23, 26, 27, 30, 31},
     kMipsS16x8PackOdd},
    {{0, 1, 16, 17, 4, 5, 20, 21, 8, 9, 24, 25, 12, 13, 28, 29},
     kMipsS16x8InterleaveEven},
    {{2, 3, 18, 19, 6, 7, 22, 23, 10, 11, 26, 27, 14, 15, 30, 31},
     kMipsS16x8InterleaveOdd},
    {{6, 7, 4, 5, 2, 3, 0, 1, 14, 15, 12, 13, 10, 11, 8, 9}, kMipsS16x4Reverse},
    {{2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13}, kMipsS16x2Reverse},

    {{0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23},
     kMipsS8x16InterleaveRight},
    {{8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31},
     kMipsS8x16InterleaveLeft},
    {{0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30},
     kMipsS8x16PackEven},
    {{1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31},
     kMipsS8x16PackOdd},
    {{0, 16, 2, 18, 4, 20, 6, 22, 8, 24, 10, 26, 12, 28, 14, 30},
     kMipsS8x16InterleaveEven},
    {{1, 17, 3, 19, 5, 21, 7, 23, 9, 25, 11, 27, 13, 29, 15, 31},
     kMipsS8x16InterleaveOdd},
    {{7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8}, kMipsS8x8Reverse},
    {{3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12}, kMipsS8x4Reverse},
    {{1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14}, kMipsS8x2Reverse}};

bool TryMatchArchShuffle(const uint8_t* shuffle, const ShuffleEntry* table,
                         size_t num_entries, bool is_swizzle,
                         ArchOpcode* opcode) {
  uint8_t mask = is_swizzle ? kSimd128Size - 1 : 2 * kSimd128Size - 1;
  for (size_t i = 0; i < num_entries; ++i) {
    const ShuffleEntry& entry = table[i];
    int j = 0;
    for (; j < kSimd128Size; ++j) {
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

}  // namespace

void InstructionSelector::VisitS8x16Shuffle(Node* node) {
  uint8_t shuffle[kSimd128Size];
  bool is_swizzle;
  CanonicalizeShuffle(node, shuffle, &is_swizzle);
  uint8_t shuffle32x4[4];
  ArchOpcode opcode;
  if (TryMatchArchShuffle(shuffle, arch_shuffles, arraysize(arch_shuffles),
                          is_swizzle, &opcode)) {
    VisitRRR(this, opcode, node);
    return;
  }
  Node* input0 = node->InputAt(0);
  Node* input1 = node->InputAt(1);
  uint8_t offset;
  MipsOperandGenerator g(this);
  if (TryMatchConcat(shuffle, &offset)) {
    Emit(kMipsS8x16Concat, g.DefineSameAsFirst(node), g.UseRegister(input1),
         g.UseRegister(input0), g.UseImmediate(offset));
    return;
  }
  if (TryMatch32x4Shuffle(shuffle, shuffle32x4)) {
    Emit(kMipsS32x4Shuffle, g.DefineAsRegister(node), g.UseRegister(input0),
         g.UseRegister(input1), g.UseImmediate(Pack4Lanes(shuffle32x4)));
    return;
  }
  Emit(kMipsS8x16Shuffle, g.DefineAsRegister(node), g.UseRegister(input0),
       g.UseRegister(input1), g.UseImmediate(Pack4Lanes(shuffle)),
       g.UseImmediate(Pack4Lanes(shuffle + 4)),
       g.UseImmediate(Pack4Lanes(shuffle + 8)),
       g.UseImmediate(Pack4Lanes(shuffle + 12)));
}

void InstructionSelector::VisitS8x16Swizzle(Node* node) {
  MipsOperandGenerator g(this);
  InstructionOperand temps[] = {g.TempSimd128Register()};
  // We don't want input 0 or input 1 to be the same as output, since we will
  // modify output before do the calculation.
  Emit(kMipsS8x16Swizzle, g.DefineAsRegister(node),
       g.UseUniqueRegister(node->InputAt(0)),
       g.UseUniqueRegister(node->InputAt(1)),
       arraysize(temps), temps);
}

void InstructionSelector::VisitSignExtendWord8ToInt32(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsSeb, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitSignExtendWord16ToInt32(Node* node) {
  MipsOperandGenerator g(this);
  Emit(kMipsSeh, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}

// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags = MachineOperatorBuilder::kNoFlags;
  if ((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
      IsFp64Mode()) {
    flags |= MachineOperatorBuilder::kFloat64RoundDown |
             MachineOperatorBuilder::kFloat64RoundUp |
             MachineOperatorBuilder::kFloat64RoundTruncate |
             MachineOperatorBuilder::kFloat64RoundTiesEven;
  }

  return flags | MachineOperatorBuilder::kWord32Ctz |
         MachineOperatorBuilder::kWord32Popcnt |
         MachineOperatorBuilder::kInt32DivIsSafe |
         MachineOperatorBuilder::kUint32DivIsSafe |
         MachineOperatorBuilder::kWord32ShiftIsSafe |
         MachineOperatorBuilder::kFloat32RoundDown |
         MachineOperatorBuilder::kFloat32RoundUp |
         MachineOperatorBuilder::kFloat32RoundTruncate |
         MachineOperatorBuilder::kFloat32RoundTiesEven;
}

// static
MachineOperatorBuilder::AlignmentRequirements
InstructionSelector::AlignmentRequirements() {
  if (IsMipsArchVariant(kMips32r6)) {
    return MachineOperatorBuilder::AlignmentRequirements::
        FullUnalignedAccessSupport();
  } else {
    DCHECK(IsMipsArchVariant(kLoongson) || IsMipsArchVariant(kMips32r1) ||
           IsMipsArchVariant(kMips32r2));
    return MachineOperatorBuilder::AlignmentRequirements::
        NoUnalignedAccessSupport();
  }
}

#undef SIMD_BINOP_LIST
#undef SIMD_SHIFT_OP_LIST
#undef SIMD_UNOP_LIST
#undef SIMD_TYPE_LIST
#undef TRACE_UNIMPL
#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
