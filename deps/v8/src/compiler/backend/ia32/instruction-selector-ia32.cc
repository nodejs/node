// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/adapters.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

// Adds IA32-specific methods for generating operands.
class IA32OperandGenerator final : public OperandGenerator {
 public:
  explicit IA32OperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand UseByteRegister(Node* node) {
    // TODO(titzer): encode byte register use constraints.
    return UseFixed(node, edx);
  }

  InstructionOperand DefineAsByteRegister(Node* node) {
    // TODO(titzer): encode byte register def constraints.
    return DefineAsRegister(node);
  }

  bool CanBeMemoryOperand(InstructionCode opcode, Node* node, Node* input,
                          int effect_level) {
    if (input->opcode() != IrOpcode::kLoad ||
        !selector()->CanCover(node, input)) {
      return false;
    }
    if (effect_level != selector()->GetEffectLevel(input)) {
      return false;
    }
    MachineRepresentation rep =
        LoadRepresentationOf(input->op()).representation();
    switch (opcode) {
      case kIA32And:
      case kIA32Or:
      case kIA32Xor:
      case kIA32Add:
      case kIA32Sub:
      case kIA32Cmp:
      case kIA32Test:
        return rep == MachineRepresentation::kWord32 || IsAnyTagged(rep);
      case kIA32Cmp16:
      case kIA32Test16:
        return rep == MachineRepresentation::kWord16;
      case kIA32Cmp8:
      case kIA32Test8:
        return rep == MachineRepresentation::kWord8;
      default:
        break;
    }
    return false;
  }

  bool CanBeImmediate(Node* node) {
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
      case IrOpcode::kNumberConstant:
      case IrOpcode::kExternalConstant:
      case IrOpcode::kRelocatableInt32Constant:
      case IrOpcode::kRelocatableInt64Constant:
        return true;
      case IrOpcode::kHeapConstant: {
// TODO(bmeurer): We must not dereference handles concurrently. If we
// really have to this here, then we need to find a way to put this
// information on the HeapConstant node already.
#if 0
        // Constants in young generation cannot be used as immediates in V8
        // because the GC does not scan code objects when collecting the young
        // generation.
        Handle<HeapObject> value = HeapConstantOf(node->op());
        return !Heap::InYoungGeneration(*value);
#else
        return false;
#endif
      }
      default:
        return false;
    }
  }

  AddressingMode GenerateMemoryOperandInputs(Node* index, int scale, Node* base,
                                             Node* displacement_node,
                                             DisplacementMode displacement_mode,
                                             InstructionOperand inputs[],
                                             size_t* input_count) {
    AddressingMode mode = kMode_MRI;
    int32_t displacement = (displacement_node == nullptr)
                               ? 0
                               : OpParameter<int32_t>(displacement_node->op());
    if (displacement_mode == kNegativeDisplacement) {
      displacement = -displacement;
    }
    if (base != nullptr) {
      if (base->opcode() == IrOpcode::kInt32Constant) {
        displacement += OpParameter<int32_t>(base->op());
        base = nullptr;
      }
    }
    if (base != nullptr) {
      inputs[(*input_count)++] = UseRegister(base);
      if (index != nullptr) {
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
      if (index != nullptr) {
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
    BaseWithIndexAndDisplacement32Matcher m(node, AddressOption::kAllowAll);
    DCHECK(m.matches());
    if ((m.displacement() == nullptr || CanBeImmediate(m.displacement()))) {
      return GenerateMemoryOperandInputs(
          m.index(), m.scale(), m.base(), m.displacement(),
          m.displacement_mode(), inputs, input_count);
    } else {
      inputs[(*input_count)++] = UseRegister(node->InputAt(0));
      inputs[(*input_count)++] = UseRegister(node->InputAt(1));
      return kMode_MR1;
    }
  }

  InstructionOperand GetEffectiveIndexOperand(Node* index,
                                              AddressingMode* mode) {
    if (CanBeImmediate(index)) {
      *mode = kMode_MRI;
      return UseImmediate(index);
    } else {
      *mode = kMode_MR1;
      return UseUniqueRegister(index);
    }
  }

  bool CanBeBetterLeftOperand(Node* node) const {
    return !selector()->IsLive(node);
  }
};

namespace {

void VisitRO(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  IA32OperandGenerator g(selector);
  InstructionOperand temps[] = {g.TempRegister()};
  Node* input = node->InputAt(0);
  // We have to use a byte register as input to movsxb.
  InstructionOperand input_op =
      opcode == kIA32Movsxbl ? g.UseFixed(input, eax) : g.Use(input);
  selector->Emit(opcode, g.DefineAsRegister(node), input_op, arraysize(temps),
                 temps);
}

void VisitRR(InstructionSelector* selector, Node* node,
             InstructionCode opcode) {
  IA32OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}

void VisitRROFloat(InstructionSelector* selector, Node* node,
                   ArchOpcode avx_opcode, ArchOpcode sse_opcode) {
  IA32OperandGenerator g(selector);
  InstructionOperand operand0 = g.UseRegister(node->InputAt(0));
  InstructionOperand operand1 = g.Use(node->InputAt(1));
  if (selector->IsSupported(AVX)) {
    selector->Emit(avx_opcode, g.DefineAsRegister(node), operand0, operand1);
  } else {
    selector->Emit(sse_opcode, g.DefineSameAsFirst(node), operand0, operand1);
  }
}

void VisitFloatUnop(InstructionSelector* selector, Node* node, Node* input,
                    ArchOpcode avx_opcode, ArchOpcode sse_opcode) {
  IA32OperandGenerator g(selector);
  if (selector->IsSupported(AVX)) {
    selector->Emit(avx_opcode, g.DefineAsRegister(node), g.Use(input));
  } else {
    selector->Emit(sse_opcode, g.DefineSameAsFirst(node), g.UseRegister(input));
  }
}

void VisitRRSimd(InstructionSelector* selector, Node* node,
                 ArchOpcode avx_opcode, ArchOpcode sse_opcode) {
  IA32OperandGenerator g(selector);
  InstructionOperand operand0 = g.UseRegister(node->InputAt(0));
  if (selector->IsSupported(AVX)) {
    selector->Emit(avx_opcode, g.DefineAsRegister(node), operand0);
  } else {
    selector->Emit(sse_opcode, g.DefineSameAsFirst(node), operand0);
  }
}

void VisitRRISimd(InstructionSelector* selector, Node* node,
                  ArchOpcode opcode) {
  IA32OperandGenerator g(selector);
  InstructionOperand operand0 = g.UseRegister(node->InputAt(0));
  InstructionOperand operand1 =
      g.UseImmediate(OpParameter<int32_t>(node->op()));
  // 8x16 uses movsx_b on dest to extract a byte, which only works
  // if dest is a byte register.
  InstructionOperand dest = opcode == kIA32I8x16ExtractLane
                                ? g.DefineAsFixed(node, eax)
                                : g.DefineAsRegister(node);
  selector->Emit(opcode, dest, operand0, operand1);
}

void VisitRRISimd(InstructionSelector* selector, Node* node,
                  ArchOpcode avx_opcode, ArchOpcode sse_opcode) {
  IA32OperandGenerator g(selector);
  InstructionOperand operand0 = g.UseRegister(node->InputAt(0));
  InstructionOperand operand1 =
      g.UseImmediate(OpParameter<int32_t>(node->op()));
  if (selector->IsSupported(AVX)) {
    selector->Emit(avx_opcode, g.DefineAsRegister(node), operand0, operand1);
  } else {
    selector->Emit(sse_opcode, g.DefineSameAsFirst(node), operand0, operand1);
  }
}

}  // namespace

void InstructionSelector::VisitStackSlot(Node* node) {
  StackSlotRepresentation rep = StackSlotRepresentationOf(node->op());
  int slot = frame_->AllocateSpillSlot(rep.size());
  OperandGenerator g(this);

  Emit(kArchStackSlot, g.DefineAsRegister(node),
       sequence()->AddImmediate(Constant(slot)), 0, nullptr);
}

void InstructionSelector::VisitDebugAbort(Node* node) {
  IA32OperandGenerator g(this);
  Emit(kArchDebugAbort, g.NoOutput(), g.UseFixed(node->InputAt(0), edx));
}

void InstructionSelector::VisitLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());

  ArchOpcode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kIA32Movss;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kIA32Movsd;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsSigned() ? kIA32Movsxbl : kIA32Movzxbl;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsSigned() ? kIA32Movsxwl : kIA32Movzxwl;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord32:
      opcode = kIA32Movl;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kIA32Movdqu;
      break;
    case MachineRepresentation::kCompressedSigned:   // Fall through.
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:         // Fall through.
    case MachineRepresentation::kWord64:             // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }

  IA32OperandGenerator g(this);
  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(node);
  InstructionOperand inputs[3];
  size_t input_count = 0;
  AddressingMode mode =
      g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
  InstructionCode code = opcode | AddressingModeField::encode(mode);
  if (node->opcode() == IrOpcode::kPoisonedLoad) {
    CHECK_NE(poisoning_level_, PoisoningMitigationLevel::kDontPoison);
    code |= MiscField::encode(kMemoryAccessPoisoned);
  }
  Emit(code, 1, outputs, input_count, inputs);
}

void InstructionSelector::VisitPoisonedLoad(Node* node) { VisitLoad(node); }

void InstructionSelector::VisitProtectedLoad(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelector::VisitStore(Node* node) {
  IA32OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());
  WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();
  MachineRepresentation rep = store_rep.representation();

  if (write_barrier_kind != kNoWriteBarrier) {
    DCHECK(CanBeTaggedPointer(rep));
    AddressingMode addressing_mode;
    InstructionOperand inputs[] = {
        g.UseUniqueRegister(base),
        g.GetEffectiveIndexOperand(index, &addressing_mode),
        g.UseUniqueRegister(value)};
    RecordWriteMode record_write_mode =
        WriteBarrierKindToRecordWriteMode(write_barrier_kind);
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    size_t const temp_count = arraysize(temps);
    InstructionCode code = kArchStoreWithWriteBarrier;
    code |= AddressingModeField::encode(addressing_mode);
    code |= MiscField::encode(static_cast<int>(record_write_mode));
    Emit(code, 0, nullptr, arraysize(inputs), inputs, temp_count, temps);
  } else {
    ArchOpcode opcode = kArchNop;
    switch (rep) {
      case MachineRepresentation::kFloat32:
        opcode = kIA32Movss;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kIA32Movsd;
        break;
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = kIA32Movb;
        break;
      case MachineRepresentation::kWord16:
        opcode = kIA32Movw;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:         // Fall through.
      case MachineRepresentation::kWord32:
        opcode = kIA32Movl;
        break;
      case MachineRepresentation::kSimd128:
        opcode = kIA32Movdqu;
        break;
      case MachineRepresentation::kCompressedSigned:   // Fall through.
      case MachineRepresentation::kCompressedPointer:  // Fall through.
      case MachineRepresentation::kCompressed:         // Fall through.
      case MachineRepresentation::kWord64:             // Fall through.
      case MachineRepresentation::kNone:
        UNREACHABLE();
        return;
    }

    InstructionOperand val;
    if (g.CanBeImmediate(value)) {
      val = g.UseImmediate(value);
    } else if (rep == MachineRepresentation::kWord8 ||
               rep == MachineRepresentation::kBit) {
      val = g.UseByteRegister(value);
    } else {
      val = g.UseRegister(value);
    }

    InstructionOperand inputs[4];
    size_t input_count = 0;
    AddressingMode addressing_mode =
        g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
    InstructionCode code =
        opcode | AddressingModeField::encode(addressing_mode);
    inputs[input_count++] = val;
    Emit(code, 0, static_cast<InstructionOperand*>(nullptr), input_count,
         inputs);
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

namespace {

// Shared routine for multiple binary operations.
void VisitBinop(InstructionSelector* selector, Node* node,
                InstructionCode opcode, FlagsContinuation* cont) {
  IA32OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();
  InstructionOperand inputs[6];
  size_t input_count = 0;
  InstructionOperand outputs[1];
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
    int effect_level = selector->GetEffectLevel(node);
    if (cont->IsBranch()) {
      effect_level = selector->GetEffectLevel(
          cont->true_block()->PredecessorAt(0)->control_input());
    }
    if (node->op()->HasProperty(Operator::kCommutative) &&
        g.CanBeBetterLeftOperand(right) &&
        (!g.CanBeBetterLeftOperand(left) ||
         !g.CanBeMemoryOperand(opcode, node, right, effect_level))) {
      std::swap(left, right);
    }
    if (g.CanBeMemoryOperand(opcode, node, right, effect_level)) {
      inputs[input_count++] = g.UseRegister(left);
      AddressingMode addressing_mode =
          g.GetEffectiveAddressMemoryOperand(right, inputs, &input_count);
      opcode |= AddressingModeField::encode(addressing_mode);
    } else {
      inputs[input_count++] = g.UseRegister(left);
      inputs[input_count++] = g.Use(right);
    }
  }

  outputs[output_count++] = g.DefineSameAsFirst(node);

  DCHECK_NE(0u, input_count);
  DCHECK_EQ(1u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}

// Shared routine for multiple binary operations.
void VisitBinop(InstructionSelector* selector, Node* node,
                InstructionCode opcode) {
  FlagsContinuation cont;
  VisitBinop(selector, node, opcode, &cont);
}

}  // namespace

void InstructionSelector::VisitWord32And(Node* node) {
  VisitBinop(this, node, kIA32And);
}

void InstructionSelector::VisitWord32Or(Node* node) {
  VisitBinop(this, node, kIA32Or);
}

void InstructionSelector::VisitWord32Xor(Node* node) {
  IA32OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.right().Is(-1)) {
    Emit(kIA32Not, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()));
  } else {
    VisitBinop(this, node, kIA32Xor);
  }
}

// Shared routine for multiple shift operations.
static inline void VisitShift(InstructionSelector* selector, Node* node,
                              ArchOpcode opcode) {
  IA32OperandGenerator g(selector);
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
  IA32OperandGenerator g(selector);
  InstructionOperand temps[] = {g.TempRegister(eax)};
  selector->Emit(
      opcode, g.DefineAsFixed(node, edx), g.UseFixed(node->InputAt(0), eax),
      g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps);
}

void VisitDiv(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  IA32OperandGenerator g(selector);
  InstructionOperand temps[] = {g.TempRegister(edx)};
  selector->Emit(opcode, g.DefineAsFixed(node, eax),
                 g.UseFixed(node->InputAt(0), eax),
                 g.UseUnique(node->InputAt(1)), arraysize(temps), temps);
}

void VisitMod(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  IA32OperandGenerator g(selector);
  InstructionOperand temps[] = {g.TempRegister(eax)};
  selector->Emit(opcode, g.DefineAsFixed(node, edx),
                 g.UseFixed(node->InputAt(0), eax),
                 g.UseUnique(node->InputAt(1)), arraysize(temps), temps);
}

void EmitLea(InstructionSelector* selector, Node* result, Node* index,
             int scale, Node* base, Node* displacement,
             DisplacementMode displacement_mode) {
  IA32OperandGenerator g(selector);
  InstructionOperand inputs[4];
  size_t input_count = 0;
  AddressingMode mode =
      g.GenerateMemoryOperandInputs(index, scale, base, displacement,
                                    displacement_mode, inputs, &input_count);

  DCHECK_NE(0u, input_count);
  DCHECK_GE(arraysize(inputs), input_count);

  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(result);

  InstructionCode opcode = AddressingModeField::encode(mode) | kIA32Lea;

  selector->Emit(opcode, 1, outputs, input_count, inputs);
}

}  // namespace

void InstructionSelector::VisitWord32Shl(Node* node) {
  Int32ScaleMatcher m(node, true);
  if (m.matches()) {
    Node* index = node->InputAt(0);
    Node* base = m.power_of_two_plus_one() ? index : nullptr;
    EmitLea(this, node, index, m.scale(), base, nullptr, kPositiveDisplacement);
    return;
  }
  VisitShift(this, node, kIA32Shl);
}

void InstructionSelector::VisitWord32Shr(Node* node) {
  VisitShift(this, node, kIA32Shr);
}

void InstructionSelector::VisitWord32Sar(Node* node) {
  VisitShift(this, node, kIA32Sar);
}

void InstructionSelector::VisitInt32PairAdd(Node* node) {
  IA32OperandGenerator g(this);

  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    // We use UseUniqueRegister here to avoid register sharing with the temp
    // register.
    InstructionOperand inputs[] = {
        g.UseRegister(node->InputAt(0)),
        g.UseUniqueRegisterOrSlotOrConstant(node->InputAt(1)),
        g.UseRegister(node->InputAt(2)), g.UseUniqueRegister(node->InputAt(3))};

    InstructionOperand outputs[] = {g.DefineSameAsFirst(node),
                                    g.DefineAsRegister(projection1)};

    InstructionOperand temps[] = {g.TempRegister()};

    Emit(kIA32AddPair, 2, outputs, 4, inputs, 1, temps);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    Emit(kIA32Add, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
         g.Use(node->InputAt(2)));
  }
}

void InstructionSelector::VisitInt32PairSub(Node* node) {
  IA32OperandGenerator g(this);

  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    // We use UseUniqueRegister here to avoid register sharing with the temp
    // register.
    InstructionOperand inputs[] = {
        g.UseRegister(node->InputAt(0)),
        g.UseUniqueRegisterOrSlotOrConstant(node->InputAt(1)),
        g.UseRegister(node->InputAt(2)), g.UseUniqueRegister(node->InputAt(3))};

    InstructionOperand outputs[] = {g.DefineSameAsFirst(node),
                                    g.DefineAsRegister(projection1)};

    InstructionOperand temps[] = {g.TempRegister()};

    Emit(kIA32SubPair, 2, outputs, 4, inputs, 1, temps);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    Emit(kIA32Sub, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
         g.Use(node->InputAt(2)));
  }
}

void InstructionSelector::VisitInt32PairMul(Node* node) {
  IA32OperandGenerator g(this);

  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    // InputAt(3) explicitly shares ecx with OutputRegister(1) to save one
    // register and one mov instruction.
    InstructionOperand inputs[] = {
        g.UseUnique(node->InputAt(0)),
        g.UseUniqueRegisterOrSlotOrConstant(node->InputAt(1)),
        g.UseUniqueRegister(node->InputAt(2)),
        g.UseFixed(node->InputAt(3), ecx)};

    InstructionOperand outputs[] = {
        g.DefineAsFixed(node, eax),
        g.DefineAsFixed(NodeProperties::FindProjection(node, 1), ecx)};

    InstructionOperand temps[] = {g.TempRegister(edx)};

    Emit(kIA32MulPair, 2, outputs, 4, inputs, 1, temps);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    Emit(kIA32Imul, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
         g.Use(node->InputAt(2)));
  }
}

void VisitWord32PairShift(InstructionSelector* selector, InstructionCode opcode,
                          Node* node) {
  IA32OperandGenerator g(selector);

  Node* shift = node->InputAt(2);
  InstructionOperand shift_operand;
  if (g.CanBeImmediate(shift)) {
    shift_operand = g.UseImmediate(shift);
  } else {
    shift_operand = g.UseFixed(shift, ecx);
  }
  InstructionOperand inputs[] = {g.UseFixed(node->InputAt(0), eax),
                                 g.UseFixed(node->InputAt(1), edx),
                                 shift_operand};

  InstructionOperand outputs[2];
  InstructionOperand temps[1];
  int32_t output_count = 0;
  int32_t temp_count = 0;
  outputs[output_count++] = g.DefineAsFixed(node, eax);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    outputs[output_count++] = g.DefineAsFixed(projection1, edx);
  } else {
    temps[temp_count++] = g.TempRegister(edx);
  }

  selector->Emit(opcode, output_count, outputs, 3, inputs, temp_count, temps);
}

void InstructionSelector::VisitWord32PairShl(Node* node) {
  VisitWord32PairShift(this, kIA32ShlPair, node);
}

void InstructionSelector::VisitWord32PairShr(Node* node) {
  VisitWord32PairShift(this, kIA32ShrPair, node);
}

void InstructionSelector::VisitWord32PairSar(Node* node) {
  VisitWord32PairShift(this, kIA32SarPair, node);
}

void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitShift(this, node, kIA32Ror);
}

#define RO_OP_LIST(V)                                       \
  V(Word32Clz, kIA32Lzcnt)                                  \
  V(Word32Ctz, kIA32Tzcnt)                                  \
  V(Word32Popcnt, kIA32Popcnt)                              \
  V(ChangeFloat32ToFloat64, kSSEFloat32ToFloat64)           \
  V(RoundInt32ToFloat32, kSSEInt32ToFloat32)                \
  V(ChangeInt32ToFloat64, kSSEInt32ToFloat64)               \
  V(ChangeUint32ToFloat64, kSSEUint32ToFloat64)             \
  V(TruncateFloat32ToInt32, kSSEFloat32ToInt32)             \
  V(TruncateFloat32ToUint32, kSSEFloat32ToUint32)           \
  V(ChangeFloat64ToInt32, kSSEFloat64ToInt32)               \
  V(ChangeFloat64ToUint32, kSSEFloat64ToUint32)             \
  V(TruncateFloat64ToUint32, kSSEFloat64ToUint32)           \
  V(TruncateFloat64ToFloat32, kSSEFloat64ToFloat32)         \
  V(RoundFloat64ToInt32, kSSEFloat64ToInt32)                \
  V(BitcastFloat32ToInt32, kIA32BitcastFI)                  \
  V(BitcastInt32ToFloat32, kIA32BitcastIF)                  \
  V(Float32Sqrt, kSSEFloat32Sqrt)                           \
  V(Float64Sqrt, kSSEFloat64Sqrt)                           \
  V(Float64ExtractLowWord32, kSSEFloat64ExtractLowWord32)   \
  V(Float64ExtractHighWord32, kSSEFloat64ExtractHighWord32) \
  V(SignExtendWord8ToInt32, kIA32Movsxbl)                   \
  V(SignExtendWord16ToInt32, kIA32Movsxwl)

#define RR_OP_LIST(V)                                                         \
  V(TruncateFloat64ToWord32, kArchTruncateDoubleToI)                          \
  V(Float32RoundDown, kSSEFloat32Round | MiscField::encode(kRoundDown))       \
  V(Float64RoundDown, kSSEFloat64Round | MiscField::encode(kRoundDown))       \
  V(Float32RoundUp, kSSEFloat32Round | MiscField::encode(kRoundUp))           \
  V(Float64RoundUp, kSSEFloat64Round | MiscField::encode(kRoundUp))           \
  V(Float32RoundTruncate, kSSEFloat32Round | MiscField::encode(kRoundToZero)) \
  V(Float64RoundTruncate, kSSEFloat64Round | MiscField::encode(kRoundToZero)) \
  V(Float32RoundTiesEven,                                                     \
    kSSEFloat32Round | MiscField::encode(kRoundToNearest))                    \
  V(Float64RoundTiesEven, kSSEFloat64Round | MiscField::encode(kRoundToNearest))

#define RRO_FLOAT_OP_LIST(V)                    \
  V(Float32Add, kAVXFloat32Add, kSSEFloat32Add) \
  V(Float64Add, kAVXFloat64Add, kSSEFloat64Add) \
  V(Float32Sub, kAVXFloat32Sub, kSSEFloat32Sub) \
  V(Float64Sub, kAVXFloat64Sub, kSSEFloat64Sub) \
  V(Float32Mul, kAVXFloat32Mul, kSSEFloat32Mul) \
  V(Float64Mul, kAVXFloat64Mul, kSSEFloat64Mul) \
  V(Float32Div, kAVXFloat32Div, kSSEFloat32Div) \
  V(Float64Div, kAVXFloat64Div, kSSEFloat64Div)

#define FLOAT_UNOP_LIST(V)                      \
  V(Float32Abs, kAVXFloat32Abs, kSSEFloat32Abs) \
  V(Float64Abs, kAVXFloat64Abs, kSSEFloat64Abs) \
  V(Float32Neg, kAVXFloat32Neg, kSSEFloat32Neg) \
  V(Float64Neg, kAVXFloat64Neg, kSSEFloat64Neg)

#define RO_VISITOR(Name, opcode)                      \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRO(this, node, opcode);                      \
  }
RO_OP_LIST(RO_VISITOR)
#undef RO_VISITOR
#undef RO_OP_LIST

#define RR_VISITOR(Name, opcode)                      \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRR(this, node, opcode);                      \
  }
RR_OP_LIST(RR_VISITOR)
#undef RR_VISITOR
#undef RR_OP_LIST

#define RRO_FLOAT_VISITOR(Name, avx, sse)             \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRROFloat(this, node, avx, sse);              \
  }
RRO_FLOAT_OP_LIST(RRO_FLOAT_VISITOR)
#undef RRO_FLOAT_VISITOR
#undef RRO_FLOAT_OP_LIST

#define FLOAT_UNOP_VISITOR(Name, avx, sse)                  \
  void InstructionSelector::Visit##Name(Node* node) {       \
    VisitFloatUnop(this, node, node->InputAt(0), avx, sse); \
  }
FLOAT_UNOP_LIST(FLOAT_UNOP_VISITOR)
#undef FLOAT_UNOP_VISITOR
#undef FLOAT_UNOP_LIST

void InstructionSelector::VisitWord32ReverseBits(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord64ReverseBytes(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord32ReverseBytes(Node* node) {
  IA32OperandGenerator g(this);
  Emit(kIA32Bswap, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitInt32Add(Node* node) {
  IA32OperandGenerator g(this);

  // Try to match the Add to a lea pattern
  BaseWithIndexAndDisplacement32Matcher m(node);
  if (m.matches() &&
      (m.displacement() == nullptr || g.CanBeImmediate(m.displacement()))) {
    InstructionOperand inputs[4];
    size_t input_count = 0;
    AddressingMode mode = g.GenerateMemoryOperandInputs(
        m.index(), m.scale(), m.base(), m.displacement(), m.displacement_mode(),
        inputs, &input_count);

    DCHECK_NE(0u, input_count);
    DCHECK_GE(arraysize(inputs), input_count);

    InstructionOperand outputs[1];
    outputs[0] = g.DefineAsRegister(node);

    InstructionCode opcode = AddressingModeField::encode(mode) | kIA32Lea;
    Emit(opcode, 1, outputs, input_count, inputs);
    return;
  }

  // No lea pattern match, use add
  VisitBinop(this, node, kIA32Add);
}

void InstructionSelector::VisitInt32Sub(Node* node) {
  IA32OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().Is(0)) {
    Emit(kIA32Neg, g.DefineSameAsFirst(node), g.Use(m.right().node()));
  } else {
    VisitBinop(this, node, kIA32Sub);
  }
}

void InstructionSelector::VisitInt32Mul(Node* node) {
  Int32ScaleMatcher m(node, true);
  if (m.matches()) {
    Node* index = node->InputAt(0);
    Node* base = m.power_of_two_plus_one() ? index : nullptr;
    EmitLea(this, node, index, m.scale(), base, nullptr, kPositiveDisplacement);
    return;
  }
  IA32OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (g.CanBeImmediate(right)) {
    Emit(kIA32Imul, g.DefineAsRegister(node), g.Use(left),
         g.UseImmediate(right));
  } else {
    if (g.CanBeBetterLeftOperand(right)) {
      std::swap(left, right);
    }
    Emit(kIA32Imul, g.DefineSameAsFirst(node), g.UseRegister(left),
         g.Use(right));
  }
}

void InstructionSelector::VisitInt32MulHigh(Node* node) {
  VisitMulHigh(this, node, kIA32ImulHigh);
}

void InstructionSelector::VisitUint32MulHigh(Node* node) {
  VisitMulHigh(this, node, kIA32UmulHigh);
}

void InstructionSelector::VisitInt32Div(Node* node) {
  VisitDiv(this, node, kIA32Idiv);
}

void InstructionSelector::VisitUint32Div(Node* node) {
  VisitDiv(this, node, kIA32Udiv);
}

void InstructionSelector::VisitInt32Mod(Node* node) {
  VisitMod(this, node, kIA32Idiv);
}

void InstructionSelector::VisitUint32Mod(Node* node) {
  VisitMod(this, node, kIA32Udiv);
}

void InstructionSelector::VisitRoundUint32ToFloat32(Node* node) {
  IA32OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister()};
  Emit(kSSEUint32ToFloat32, g.DefineAsRegister(node), g.Use(node->InputAt(0)),
       arraysize(temps), temps);
}

void InstructionSelector::VisitFloat64Mod(Node* node) {
  IA32OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister(eax), g.TempRegister()};
  Emit(kSSEFloat64Mod, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)),
       arraysize(temps), temps);
}

void InstructionSelector::VisitFloat32Max(Node* node) {
  IA32OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister()};
  Emit(kSSEFloat32Max, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)),
       arraysize(temps), temps);
}

void InstructionSelector::VisitFloat64Max(Node* node) {
  IA32OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister()};
  Emit(kSSEFloat64Max, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)),
       arraysize(temps), temps);
}

void InstructionSelector::VisitFloat32Min(Node* node) {
  IA32OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister()};
  Emit(kSSEFloat32Min, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)),
       arraysize(temps), temps);
}

void InstructionSelector::VisitFloat64Min(Node* node) {
  IA32OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister()};
  Emit(kSSEFloat64Min, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)),
       arraysize(temps), temps);
}

void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitFloat64Ieee754Binop(Node* node,
                                                   InstructionCode opcode) {
  IA32OperandGenerator g(this);
  Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
       g.UseRegister(node->InputAt(1)))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat64Ieee754Unop(Node* node,
                                                  InstructionCode opcode) {
  IA32OperandGenerator g(this);
  Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)))
      ->MarkAsCall();
}

void InstructionSelector::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* call_descriptor,
    Node* node) {
  IA32OperandGenerator g(this);

  // Prepare for C function call.
  if (call_descriptor->IsCFunctionCall()) {
    InstructionOperand temps[] = {g.TempRegister()};
    size_t const temp_count = arraysize(temps);
    Emit(kArchPrepareCallCFunction | MiscField::encode(static_cast<int>(
                                         call_descriptor->ParameterCount())),
         0, nullptr, 0, nullptr, temp_count, temps);

    // Poke any stack arguments.
    for (size_t n = 0; n < arguments->size(); ++n) {
      PushParameter input = (*arguments)[n];
      if (input.node) {
        int const slot = static_cast<int>(n);
        InstructionOperand value = g.CanBeImmediate(node)
                                       ? g.UseImmediate(input.node)
                                       : g.UseRegister(input.node);
        Emit(kIA32Poke | MiscField::encode(slot), g.NoOutput(), value);
      }
    }
  } else {
    // Push any stack arguments.
    int effect_level = GetEffectLevel(node);
    for (PushParameter input : base::Reversed(*arguments)) {
      // Skip any alignment holes in pushed nodes.
      if (input.node == nullptr) continue;
      if (g.CanBeMemoryOperand(kIA32Push, node, input.node, effect_level)) {
        InstructionOperand outputs[1];
        InstructionOperand inputs[4];
        size_t input_count = 0;
        InstructionCode opcode = kIA32Push;
        AddressingMode mode = g.GetEffectiveAddressMemoryOperand(
            input.node, inputs, &input_count);
        opcode |= AddressingModeField::encode(mode);
        Emit(opcode, 0, outputs, input_count, inputs);
      } else {
        InstructionOperand value =
            g.CanBeImmediate(input.node)
                ? g.UseImmediate(input.node)
                : IsSupported(ATOM) ||
                          sequence()->IsFP(GetVirtualRegister(input.node))
                      ? g.UseRegister(input.node)
                      : g.Use(input.node);
        if (input.location.GetType() == MachineType::Float32()) {
          Emit(kIA32PushFloat32, g.NoOutput(), value);
        } else if (input.location.GetType() == MachineType::Float64()) {
          Emit(kIA32PushFloat64, g.NoOutput(), value);
        } else if (input.location.GetType() == MachineType::Simd128()) {
          Emit(kIA32PushSimd128, g.NoOutput(), value);
        } else {
          Emit(kIA32Push, g.NoOutput(), value);
        }
      }
    }
  }
}

void InstructionSelector::EmitPrepareResults(
    ZoneVector<PushParameter>* results, const CallDescriptor* call_descriptor,
    Node* node) {
  IA32OperandGenerator g(this);

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
      Emit(kIA32Peek, g.DefineAsRegister(output.node),
           g.UseImmediate(reverse_slot));
    }
    reverse_slot += output.location.GetSizeInPointers();
  }
}

bool InstructionSelector::IsTailCallAddressImmediate() { return true; }

int InstructionSelector::GetTempsCountForTailCallFromJSFunction() { return 0; }

namespace {

void VisitCompareWithMemoryOperand(InstructionSelector* selector,
                                   InstructionCode opcode, Node* left,
                                   InstructionOperand right,
                                   FlagsContinuation* cont) {
  DCHECK_EQ(IrOpcode::kLoad, left->opcode());
  IA32OperandGenerator g(selector);
  size_t input_count = 0;
  InstructionOperand inputs[4];
  AddressingMode addressing_mode =
      g.GetEffectiveAddressMemoryOperand(left, inputs, &input_count);
  opcode |= AddressingModeField::encode(addressing_mode);
  inputs[input_count++] = right;

  selector->EmitWithContinuation(opcode, 0, nullptr, input_count, inputs, cont);
}

// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  InstructionOperand left, InstructionOperand right,
                  FlagsContinuation* cont) {
  selector->EmitWithContinuation(opcode, left, right, cont);
}

// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  Node* left, Node* right, FlagsContinuation* cont,
                  bool commutative) {
  IA32OperandGenerator g(selector);
  if (commutative && g.CanBeBetterLeftOperand(right)) {
    std::swap(left, right);
  }
  VisitCompare(selector, opcode, g.UseRegister(left), g.Use(right), cont);
}

MachineType MachineTypeForNarrow(Node* node, Node* hint_node) {
  if (hint_node->opcode() == IrOpcode::kLoad) {
    MachineType hint = LoadRepresentationOf(hint_node->op());
    if (node->opcode() == IrOpcode::kInt32Constant ||
        node->opcode() == IrOpcode::kInt64Constant) {
      int64_t constant = node->opcode() == IrOpcode::kInt32Constant
                             ? OpParameter<int32_t>(node->op())
                             : OpParameter<int64_t>(node->op());
      if (hint == MachineType::Int8()) {
        if (constant >= std::numeric_limits<int8_t>::min() &&
            constant <= std::numeric_limits<int8_t>::max()) {
          return hint;
        }
      } else if (hint == MachineType::Uint8()) {
        if (constant >= std::numeric_limits<uint8_t>::min() &&
            constant <= std::numeric_limits<uint8_t>::max()) {
          return hint;
        }
      } else if (hint == MachineType::Int16()) {
        if (constant >= std::numeric_limits<int16_t>::min() &&
            constant <= std::numeric_limits<int16_t>::max()) {
          return hint;
        }
      } else if (hint == MachineType::Uint16()) {
        if (constant >= std::numeric_limits<uint16_t>::min() &&
            constant <= std::numeric_limits<uint16_t>::max()) {
          return hint;
        }
      } else if (hint == MachineType::Int32()) {
        return hint;
      } else if (hint == MachineType::Uint32()) {
        if (constant >= 0) return hint;
      }
    }
  }
  return node->opcode() == IrOpcode::kLoad ? LoadRepresentationOf(node->op())
                                           : MachineType::None();
}

// Tries to match the size of the given opcode to that of the operands, if
// possible.
InstructionCode TryNarrowOpcodeSize(InstructionCode opcode, Node* left,
                                    Node* right, FlagsContinuation* cont) {
  // TODO(epertoso): we can probably get some size information out of phi nodes.
  // If the load representations don't match, both operands will be
  // zero/sign-extended to 32bit.
  MachineType left_type = MachineTypeForNarrow(left, right);
  MachineType right_type = MachineTypeForNarrow(right, left);
  if (left_type == right_type) {
    switch (left_type.representation()) {
      case MachineRepresentation::kBit:
      case MachineRepresentation::kWord8: {
        if (opcode == kIA32Test) return kIA32Test8;
        if (opcode == kIA32Cmp) {
          if (left_type.semantic() == MachineSemantic::kUint32) {
            cont->OverwriteUnsignedIfSigned();
          } else {
            CHECK_EQ(MachineSemantic::kInt32, left_type.semantic());
          }
          return kIA32Cmp8;
        }
        break;
      }
      case MachineRepresentation::kWord16:
        if (opcode == kIA32Test) return kIA32Test16;
        if (opcode == kIA32Cmp) {
          if (left_type.semantic() == MachineSemantic::kUint32) {
            cont->OverwriteUnsignedIfSigned();
          } else {
            CHECK_EQ(MachineSemantic::kInt32, left_type.semantic());
          }
          return kIA32Cmp16;
        }
        break;
      default:
        break;
    }
  }
  return opcode;
}

// Shared routine for multiple float32 compare operations (inputs commuted).
void VisitFloat32Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  Node* const left = node->InputAt(0);
  Node* const right = node->InputAt(1);
  VisitCompare(selector, kSSEFloat32Cmp, right, left, cont, false);
}

// Shared routine for multiple float64 compare operations (inputs commuted).
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  Node* const left = node->InputAt(0);
  Node* const right = node->InputAt(1);
  VisitCompare(selector, kSSEFloat64Cmp, right, left, cont, false);
}

// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont) {
  IA32OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  InstructionCode narrowed_opcode =
      TryNarrowOpcodeSize(opcode, left, right, cont);

  int effect_level = selector->GetEffectLevel(node);
  if (cont->IsBranch()) {
    effect_level = selector->GetEffectLevel(
        cont->true_block()->PredecessorAt(0)->control_input());
  }

  // If one of the two inputs is an immediate, make sure it's on the right, or
  // if one of the two inputs is a memory operand, make sure it's on the left.
  if ((!g.CanBeImmediate(right) && g.CanBeImmediate(left)) ||
      (g.CanBeMemoryOperand(narrowed_opcode, node, right, effect_level) &&
       !g.CanBeMemoryOperand(narrowed_opcode, node, left, effect_level))) {
    if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
    std::swap(left, right);
  }

  // Match immediates on right side of comparison.
  if (g.CanBeImmediate(right)) {
    if (g.CanBeMemoryOperand(narrowed_opcode, node, left, effect_level)) {
      return VisitCompareWithMemoryOperand(selector, narrowed_opcode, left,
                                           g.UseImmediate(right), cont);
    }
    return VisitCompare(selector, opcode, g.Use(left), g.UseImmediate(right),
                        cont);
  }

  // Match memory operands on left side of comparison.
  if (g.CanBeMemoryOperand(narrowed_opcode, node, left, effect_level)) {
    bool needs_byte_register =
        narrowed_opcode == kIA32Test8 || narrowed_opcode == kIA32Cmp8;
    return VisitCompareWithMemoryOperand(
        selector, narrowed_opcode, left,
        needs_byte_register ? g.UseByteRegister(right) : g.UseRegister(right),
        cont);
  }

  return VisitCompare(selector, opcode, left, right, cont,
                      node->op()->HasProperty(Operator::kCommutative));
}

void VisitWordCompare(InstructionSelector* selector, Node* node,
                      FlagsContinuation* cont) {
  if (selector->isolate() != nullptr) {
    StackCheckMatcher<Int32BinopMatcher, IrOpcode::kUint32LessThan> m(
        selector->isolate(), node);
    if (m.Matched()) {
      // Compare(Load(js_stack_limit), LoadStackPointer)
      if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
      InstructionCode opcode = cont->Encode(kIA32StackCheck);
      CHECK(cont->IsBranch());
      selector->EmitWithContinuation(opcode, cont);
      return;
    }
  }
  WasmStackCheckMatcher<Int32BinopMatcher, IrOpcode::kUint32LessThan> wasm_m(
      node);
  if (wasm_m.Matched()) {
    // This is a wasm stack check. By structure, we know that we can use the
    // stack pointer directly, as wasm code does not modify the stack at points
    // where stack checks are performed.
    Node* left = node->InputAt(0);
    LocationOperand esp(InstructionOperand::EXPLICIT, LocationOperand::REGISTER,
                        InstructionSequence::DefaultRepresentation(),
                        RegisterCode::kRegCode_esp);
    return VisitCompareWithMemoryOperand(selector, kIA32Cmp, left, esp, cont);
  }
  VisitWordCompare(selector, node, kIA32Cmp, cont);
}

void VisitAtomicExchange(InstructionSelector* selector, Node* node,
                         ArchOpcode opcode, MachineRepresentation rep) {
  IA32OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  AddressingMode addressing_mode;
  InstructionOperand value_operand = (rep == MachineRepresentation::kWord8)
                                         ? g.UseFixed(value, edx)
                                         : g.UseUniqueRegister(value);
  InstructionOperand inputs[] = {
      value_operand, g.UseUniqueRegister(base),
      g.GetEffectiveIndexOperand(index, &addressing_mode)};
  InstructionOperand outputs[] = {
      (rep == MachineRepresentation::kWord8)
          // Using DefineSameAsFirst requires the register to be unallocated.
          ? g.DefineAsFixed(node, edx)
          : g.DefineSameAsFirst(node)};
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  selector->Emit(code, 1, outputs, arraysize(inputs), inputs);
}

void VisitAtomicBinOp(InstructionSelector* selector, Node* node,
                      ArchOpcode opcode, MachineRepresentation rep) {
  AddressingMode addressing_mode;
  IA32OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  InstructionOperand inputs[] = {
      g.UseUniqueRegister(value), g.UseUniqueRegister(base),
      g.GetEffectiveIndexOperand(index, &addressing_mode)};
  InstructionOperand outputs[] = {g.DefineAsFixed(node, eax)};
  InstructionOperand temp[] = {(rep == MachineRepresentation::kWord8)
                                   ? g.UseByteRegister(node)
                                   : g.TempRegister()};
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  selector->Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
                 arraysize(temp), temp);
}

void VisitPairAtomicBinOp(InstructionSelector* selector, Node* node,
                          ArchOpcode opcode) {
  IA32OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  // For Word64 operations, the value input is split into the a high node,
  // and a low node in the int64-lowering phase.
  Node* value_high = node->InputAt(3);

  // Wasm lives in 32-bit address space, so we do not need to worry about
  // base/index lowering. This will need to be fixed for Wasm64.
  AddressingMode addressing_mode;
  InstructionOperand inputs[] = {
      g.UseUniqueRegisterOrSlotOrConstant(value), g.UseFixed(value_high, ecx),
      g.UseUniqueRegister(base),
      g.GetEffectiveIndexOperand(index, &addressing_mode)};
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Node* projection0 = NodeProperties::FindProjection(node, 0);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    InstructionOperand outputs[] = {g.DefineAsFixed(projection0, eax),
                                    g.DefineAsFixed(projection1, edx)};
    selector->Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
                   0, {});
  } else if (projection0) {
    InstructionOperand outputs[] = {g.DefineAsFixed(projection0, eax)};
    InstructionOperand temps[] = {g.TempRegister(edx)};
    const int num_temps = arraysize(temps);
    selector->Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
                   num_temps, temps);
  } else {
    InstructionOperand temps[] = {g.TempRegister(eax), g.TempRegister(edx)};
    const int num_temps = arraysize(temps);
    selector->Emit(code, 0, nullptr, arraysize(inputs), inputs, num_temps,
                   temps);
  }
}

}  // namespace

// Shared routine for word comparison with zero.
void InstructionSelector::VisitWordCompareZero(Node* user, Node* value,
                                               FlagsContinuation* cont) {
  // Try to combine with comparisons against 0 by simply inverting the branch.
  while (value->opcode() == IrOpcode::kWord32Equal && CanCover(user, value)) {
    Int32BinopMatcher m(value);
    if (!m.right().Is(0)) break;

    user = value;
    value = m.left().node();
    cont->Negate();
  }

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
        cont->OverwriteAndNegateIfEqual(kUnorderedEqual);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThan);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThanOrEqual);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat64Equal:
        cont->OverwriteAndNegateIfEqual(kUnorderedEqual);
        return VisitFloat64Compare(this, value, cont);
      case IrOpcode::kFloat64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThan);
        return VisitFloat64Compare(this, value, cont);
      case IrOpcode::kFloat64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThanOrEqual);
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
                return VisitBinop(this, node, kIA32Add, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kIA32Sub, cont);
              case IrOpcode::kInt32MulWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kIA32Imul, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Sub:
        return VisitWordCompare(this, value, cont);
      case IrOpcode::kWord32And:
        return VisitWordCompare(this, value, kIA32Test, cont);
      default:
        break;
    }
  }

  // Continuation could not be combined with a compare, emit compare against 0.
  IA32OperandGenerator g(this);
  VisitCompare(this, kIA32Cmp, g.Use(value), g.TempImmediate(0), cont);
}

void InstructionSelector::VisitSwitch(Node* node, const SwitchInfo& sw) {
  IA32OperandGenerator g(this);
  InstructionOperand value_operand = g.UseRegister(node->InputAt(0));

  // Emit either ArchTableSwitch or ArchLookupSwitch.
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
        Emit(kIA32Lea | AddressingModeField::encode(kMode_MRI), index_operand,
             value_operand, g.TempImmediate(-sw.min_value()));
      }
      // Generate a table lookup.
      return EmitTableSwitch(sw, index_operand);
    }
  }

  // Generate a tree of conditional jumps.
  return EmitBinarySearchSwitch(sw, value_operand);
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
    return VisitBinop(this, node, kIA32Add, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kIA32Add, &cont);
}

void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kIA32Sub, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kIA32Sub, &cont);
}

void InstructionSelector::VisitInt32MulWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kIA32Imul, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kIA32Imul, &cont);
}

void InstructionSelector::VisitFloat32Equal(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnorderedEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat32LessThan(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedGreaterThan, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedGreaterThanOrEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64Equal(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnorderedEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64LessThan(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedGreaterThan, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedGreaterThanOrEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64InsertLowWord32(Node* node) {
  IA32OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Float64Matcher mleft(left);
  if (mleft.HasValue() && (bit_cast<uint64_t>(mleft.Value()) >> 32) == 0u) {
    Emit(kSSEFloat64LoadLowWord32, g.DefineAsRegister(node), g.Use(right));
    return;
  }
  Emit(kSSEFloat64InsertLowWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.Use(right));
}

void InstructionSelector::VisitFloat64InsertHighWord32(Node* node) {
  IA32OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Emit(kSSEFloat64InsertHighWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.Use(right));
}

void InstructionSelector::VisitFloat64SilenceNaN(Node* node) {
  IA32OperandGenerator g(this);
  Emit(kSSEFloat64SilenceNaN, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitWord32AtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  DCHECK(load_rep.representation() == MachineRepresentation::kWord8 ||
         load_rep.representation() == MachineRepresentation::kWord16 ||
         load_rep.representation() == MachineRepresentation::kWord32);
  USE(load_rep);
  VisitLoad(node);
}

void InstructionSelector::VisitWord32AtomicStore(Node* node) {
  IA32OperandGenerator g(this);
  MachineRepresentation rep = AtomicStoreRepresentationOf(node->op());
  ArchOpcode opcode = kArchNop;
  switch (rep) {
    case MachineRepresentation::kWord8:
      opcode = kWord32AtomicExchangeInt8;
      break;
    case MachineRepresentation::kWord16:
      opcode = kWord32AtomicExchangeInt16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kWord32AtomicExchangeWord32;
      break;
    default:
      UNREACHABLE();
  }
  VisitAtomicExchange(this, node, opcode, rep);
}

void InstructionSelector::VisitWord32AtomicExchange(Node* node) {
  IA32OperandGenerator g(this);
  MachineType type = AtomicOpType(node->op());
  ArchOpcode opcode = kArchNop;
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
  VisitAtomicExchange(this, node, opcode, type.representation());
}

void InstructionSelector::VisitWord32AtomicCompareExchange(Node* node) {
  IA32OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* old_value = node->InputAt(2);
  Node* new_value = node->InputAt(3);

  MachineType type = AtomicOpType(node->op());
  ArchOpcode opcode = kArchNop;
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
  AddressingMode addressing_mode;
  InstructionOperand new_val_operand =
      (type.representation() == MachineRepresentation::kWord8)
          ? g.UseByteRegister(new_value)
          : g.UseUniqueRegister(new_value);
  InstructionOperand inputs[] = {
      g.UseFixed(old_value, eax), new_val_operand, g.UseUniqueRegister(base),
      g.GetEffectiveIndexOperand(index, &addressing_mode)};
  InstructionOperand outputs[] = {g.DefineAsFixed(node, eax)};
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 1, outputs, arraysize(inputs), inputs);
}

void InstructionSelector::VisitWord32AtomicBinaryOperation(
    Node* node, ArchOpcode int8_op, ArchOpcode uint8_op, ArchOpcode int16_op,
    ArchOpcode uint16_op, ArchOpcode word32_op) {
  MachineType type = AtomicOpType(node->op());
  ArchOpcode opcode = kArchNop;
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
  VisitAtomicBinOp(this, node, opcode, type.representation());
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

void InstructionSelector::VisitWord32AtomicPairLoad(Node* node) {
  IA32OperandGenerator g(this);
  AddressingMode mode;
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  InstructionOperand inputs[] = {g.UseUniqueRegister(base),
                                 g.GetEffectiveIndexOperand(index, &mode)};
  Node* projection0 = NodeProperties::FindProjection(node, 0);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  InstructionCode code =
      kIA32Word32AtomicPairLoad | AddressingModeField::encode(mode);

  if (projection1) {
    InstructionOperand temps[] = {g.TempDoubleRegister()};
    InstructionOperand outputs[] = {g.DefineAsRegister(projection0),
                                    g.DefineAsRegister(projection1)};
    Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
         arraysize(temps), temps);
  } else if (projection0) {
    InstructionOperand temps[] = {g.TempDoubleRegister(), g.TempRegister()};
    InstructionOperand outputs[] = {g.DefineAsRegister(projection0)};
    Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
         arraysize(temps), temps);
  } else {
    InstructionOperand temps[] = {g.TempDoubleRegister(), g.TempRegister(),
                                  g.TempRegister()};
    Emit(code, 0, nullptr, arraysize(inputs), inputs, arraysize(temps), temps);
  }
}

void InstructionSelector::VisitWord32AtomicPairStore(Node* node) {
  IA32OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  Node* value_high = node->InputAt(3);

  AddressingMode addressing_mode;
  InstructionOperand inputs[] = {
      g.UseUniqueRegisterOrSlotOrConstant(value), g.UseFixed(value_high, ecx),
      g.UseUniqueRegister(base),
      g.GetEffectiveIndexOperand(index, &addressing_mode)};
  // Allocating temp registers here as stores are performed using an atomic
  // exchange, the output of which is stored in edx:eax, which should be saved
  // and restored at the end of the instruction.
  InstructionOperand temps[] = {g.TempRegister(eax), g.TempRegister(edx)};
  const int num_temps = arraysize(temps);
  InstructionCode code =
      kIA32Word32AtomicPairStore | AddressingModeField::encode(addressing_mode);
  Emit(code, 0, nullptr, arraysize(inputs), inputs, num_temps, temps);
}

void InstructionSelector::VisitWord32AtomicPairAdd(Node* node) {
  VisitPairAtomicBinOp(this, node, kIA32Word32AtomicPairAdd);
}

void InstructionSelector::VisitWord32AtomicPairSub(Node* node) {
  VisitPairAtomicBinOp(this, node, kIA32Word32AtomicPairSub);
}

void InstructionSelector::VisitWord32AtomicPairAnd(Node* node) {
  VisitPairAtomicBinOp(this, node, kIA32Word32AtomicPairAnd);
}

void InstructionSelector::VisitWord32AtomicPairOr(Node* node) {
  VisitPairAtomicBinOp(this, node, kIA32Word32AtomicPairOr);
}

void InstructionSelector::VisitWord32AtomicPairXor(Node* node) {
  VisitPairAtomicBinOp(this, node, kIA32Word32AtomicPairXor);
}

void InstructionSelector::VisitWord32AtomicPairExchange(Node* node) {
  VisitPairAtomicBinOp(this, node, kIA32Word32AtomicPairExchange);
}

void InstructionSelector::VisitWord32AtomicPairCompareExchange(Node* node) {
  IA32OperandGenerator g(this);
  Node* index = node->InputAt(1);
  AddressingMode addressing_mode;

  InstructionOperand inputs[] = {
      // High, Low values of old value
      g.UseFixed(node->InputAt(2), eax), g.UseFixed(node->InputAt(3), edx),
      // High, Low values of new value
      g.UseUniqueRegisterOrSlotOrConstant(node->InputAt(4)),
      g.UseFixed(node->InputAt(5), ecx),
      // InputAt(0) => base
      g.UseUniqueRegister(node->InputAt(0)),
      g.GetEffectiveIndexOperand(index, &addressing_mode)};
  Node* projection0 = NodeProperties::FindProjection(node, 0);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  InstructionCode code = kIA32Word32AtomicPairCompareExchange |
                         AddressingModeField::encode(addressing_mode);

  if (projection1) {
    InstructionOperand outputs[] = {g.DefineAsFixed(projection0, eax),
                                    g.DefineAsFixed(projection1, edx)};
    Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs, 0, {});
  } else if (projection0) {
    InstructionOperand outputs[] = {g.DefineAsFixed(projection0, eax)};
    InstructionOperand temps[] = {g.TempRegister(edx)};
    const int num_temps = arraysize(temps);
    Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
         num_temps, temps);
  } else {
    InstructionOperand temps[] = {g.TempRegister(eax), g.TempRegister(edx)};
    const int num_temps = arraysize(temps);
    Emit(code, 0, nullptr, arraysize(inputs), inputs, num_temps, temps);
  }
}

#define SIMD_INT_TYPES(V) \
  V(I32x4)                \
  V(I16x8)                \
  V(I8x16)

#define SIMD_BINOP_LIST(V) \
  V(F32x4Add)              \
  V(F32x4AddHoriz)         \
  V(F32x4Sub)              \
  V(F32x4Mul)              \
  V(F32x4Min)              \
  V(F32x4Max)              \
  V(F32x4Eq)               \
  V(F32x4Ne)               \
  V(F32x4Lt)               \
  V(F32x4Le)               \
  V(I32x4Add)              \
  V(I32x4AddHoriz)         \
  V(I32x4Sub)              \
  V(I32x4Mul)              \
  V(I32x4MinS)             \
  V(I32x4MaxS)             \
  V(I32x4Eq)               \
  V(I32x4Ne)               \
  V(I32x4GtS)              \
  V(I32x4GeS)              \
  V(I32x4MinU)             \
  V(I32x4MaxU)             \
  V(I32x4GtU)              \
  V(I32x4GeU)              \
  V(I16x8SConvertI32x4)    \
  V(I16x8Add)              \
  V(I16x8AddSaturateS)     \
  V(I16x8AddHoriz)         \
  V(I16x8Sub)              \
  V(I16x8SubSaturateS)     \
  V(I16x8Mul)              \
  V(I16x8MinS)             \
  V(I16x8MaxS)             \
  V(I16x8Eq)               \
  V(I16x8Ne)               \
  V(I16x8GtS)              \
  V(I16x8GeS)              \
  V(I16x8AddSaturateU)     \
  V(I16x8SubSaturateU)     \
  V(I16x8MinU)             \
  V(I16x8MaxU)             \
  V(I16x8GtU)              \
  V(I16x8GeU)              \
  V(I8x16SConvertI16x8)    \
  V(I8x16Add)              \
  V(I8x16AddSaturateS)     \
  V(I8x16Sub)              \
  V(I8x16SubSaturateS)     \
  V(I8x16MinS)             \
  V(I8x16MaxS)             \
  V(I8x16Eq)               \
  V(I8x16Ne)               \
  V(I8x16GtS)              \
  V(I8x16GeS)              \
  V(I8x16AddSaturateU)     \
  V(I8x16SubSaturateU)     \
  V(I8x16MinU)             \
  V(I8x16MaxU)             \
  V(I8x16GtU)              \
  V(I8x16GeU)              \
  V(S128And)               \
  V(S128Or)                \
  V(S128Xor)

#define SIMD_UNOP_LIST(V)   \
  V(F32x4SConvertI32x4)     \
  V(F32x4RecipApprox)       \
  V(F32x4RecipSqrtApprox)   \
  V(I32x4SConvertI16x8Low)  \
  V(I32x4SConvertI16x8High) \
  V(I32x4Neg)               \
  V(I32x4UConvertI16x8Low)  \
  V(I32x4UConvertI16x8High) \
  V(I16x8SConvertI8x16Low)  \
  V(I16x8SConvertI8x16High) \
  V(I16x8Neg)               \
  V(I16x8UConvertI8x16Low)  \
  V(I16x8UConvertI8x16High) \
  V(I8x16Neg)

#define SIMD_UNOP_PREFIX_LIST(V) \
  V(F32x4Abs)                    \
  V(F32x4Neg)                    \
  V(S128Not)

#define SIMD_ANYTRUE_LIST(V) \
  V(S1x4AnyTrue)             \
  V(S1x8AnyTrue)             \
  V(S1x16AnyTrue)

#define SIMD_ALLTRUE_LIST(V) \
  V(S1x4AllTrue)             \
  V(S1x8AllTrue)             \
  V(S1x16AllTrue)

#define SIMD_SHIFT_OPCODES(V) \
  V(I32x4Shl)                 \
  V(I32x4ShrS)                \
  V(I32x4ShrU)                \
  V(I16x8Shl)                 \
  V(I16x8ShrS)                \
  V(I16x8ShrU)                \
  V(I8x16Shl)

#define SIMD_I8X16_RIGHT_SHIFT_OPCODES(V) \
  V(I8x16ShrS)                            \
  V(I8x16ShrU)

void InstructionSelector::VisitF32x4Splat(Node* node) {
  VisitRRSimd(this, node, kAVXF32x4Splat, kSSEF32x4Splat);
}

void InstructionSelector::VisitF32x4ExtractLane(Node* node) {
  VisitRRISimd(this, node, kAVXF32x4ExtractLane, kSSEF32x4ExtractLane);
}

void InstructionSelector::VisitF32x4UConvertI32x4(Node* node) {
  VisitRRSimd(this, node, kAVXF32x4UConvertI32x4, kSSEF32x4UConvertI32x4);
}

void InstructionSelector::VisitI32x4SConvertF32x4(Node* node) {
  VisitRRSimd(this, node, kAVXI32x4SConvertF32x4, kSSEI32x4SConvertF32x4);
}

void InstructionSelector::VisitI32x4UConvertF32x4(Node* node) {
  IA32OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempSimd128Register()};
  InstructionCode opcode =
      IsSupported(AVX) ? kAVXI32x4UConvertF32x4 : kSSEI32x4UConvertF32x4;
  Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
       arraysize(temps), temps);
}

void InstructionSelector::VisitI8x16Mul(Node* node) {
  IA32OperandGenerator g(this);
  InstructionOperand operand0 = g.UseUniqueRegister(node->InputAt(0));
  InstructionOperand operand1 = g.UseUniqueRegister(node->InputAt(1));
  InstructionOperand temps[] = {g.TempSimd128Register()};
  if (IsSupported(AVX)) {
    Emit(kAVXI8x16Mul, g.DefineAsRegister(node), operand0, operand1,
         arraysize(temps), temps);
  } else {
    Emit(kSSEI8x16Mul, g.DefineSameAsFirst(node), operand0, operand1,
         arraysize(temps), temps);
  }
}

void InstructionSelector::VisitS128Zero(Node* node) {
  IA32OperandGenerator g(this);
  Emit(kIA32S128Zero, g.DefineAsRegister(node));
}

void InstructionSelector::VisitS128Select(Node* node) {
  IA32OperandGenerator g(this);
  InstructionOperand operand2 = g.UseRegister(node->InputAt(2));
  if (IsSupported(AVX)) {
    Emit(kAVXS128Select, g.DefineAsRegister(node), g.Use(node->InputAt(0)),
         g.Use(node->InputAt(1)), operand2);
  } else {
    Emit(kSSES128Select, g.DefineSameAsFirst(node),
         g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)),
         operand2);
  }
}

#define VISIT_SIMD_SPLAT(Type)                               \
  void InstructionSelector::Visit##Type##Splat(Node* node) { \
    VisitRO(this, node, kIA32##Type##Splat);                 \
  }
SIMD_INT_TYPES(VISIT_SIMD_SPLAT)
#undef VISIT_SIMD_SPLAT

#define VISIT_SIMD_EXTRACT_LANE(Type)                              \
  void InstructionSelector::Visit##Type##ExtractLane(Node* node) { \
    VisitRRISimd(this, node, kIA32##Type##ExtractLane);            \
  }
SIMD_INT_TYPES(VISIT_SIMD_EXTRACT_LANE)
#undef VISIT_SIMD_EXTRACT_LANE

#define VISIT_SIMD_REPLACE_LANE(Type)                                    \
  void InstructionSelector::Visit##Type##ReplaceLane(Node* node) {       \
    IA32OperandGenerator g(this);                                        \
    InstructionOperand operand0 = g.UseRegister(node->InputAt(0));       \
    InstructionOperand operand1 =                                        \
        g.UseImmediate(OpParameter<int32_t>(node->op()));                \
    InstructionOperand operand2 = g.Use(node->InputAt(1));               \
    if (IsSupported(AVX)) {                                              \
      Emit(kAVX##Type##ReplaceLane, g.DefineAsRegister(node), operand0,  \
           operand1, operand2);                                          \
    } else {                                                             \
      Emit(kSSE##Type##ReplaceLane, g.DefineSameAsFirst(node), operand0, \
           operand1, operand2);                                          \
    }                                                                    \
  }
SIMD_INT_TYPES(VISIT_SIMD_REPLACE_LANE)
VISIT_SIMD_REPLACE_LANE(F32x4)
#undef VISIT_SIMD_REPLACE_LANE
#undef SIMD_INT_TYPES

#define VISIT_SIMD_SHIFT(Opcode)                          \
  void InstructionSelector::Visit##Opcode(Node* node) {   \
    VisitRRISimd(this, node, kAVX##Opcode, kSSE##Opcode); \
  }
SIMD_SHIFT_OPCODES(VISIT_SIMD_SHIFT)
#undef VISIT_SIMD_SHIFT
#undef SIMD_SHIFT_OPCODES

#define VISIT_SIMD_I8X16_RIGHT_SHIFT(Op)            \
  void InstructionSelector::Visit##Op(Node* node) { \
    VisitRRISimd(this, node, kIA32##Op);            \
  }

SIMD_I8X16_RIGHT_SHIFT_OPCODES(VISIT_SIMD_I8X16_RIGHT_SHIFT)
#undef SIMD_I8X16_RIGHT_SHIFT_OPCODES
#undef VISIT_SIMD_I8X16_RIGHT_SHIFT

#define VISIT_SIMD_UNOP(Opcode)                                             \
  void InstructionSelector::Visit##Opcode(Node* node) {                     \
    IA32OperandGenerator g(this);                                           \
    Emit(kIA32##Opcode, g.DefineAsRegister(node), g.Use(node->InputAt(0))); \
  }
SIMD_UNOP_LIST(VISIT_SIMD_UNOP)
#undef VISIT_SIMD_UNOP
#undef SIMD_UNOP_LIST

#define VISIT_SIMD_UNOP_PREFIX(Opcode)                                       \
  void InstructionSelector::Visit##Opcode(Node* node) {                      \
    IA32OperandGenerator g(this);                                            \
    InstructionCode opcode = IsSupported(AVX) ? kAVX##Opcode : kSSE##Opcode; \
    Emit(opcode, g.DefineAsRegister(node), g.Use(node->InputAt(0)));         \
  }
SIMD_UNOP_PREFIX_LIST(VISIT_SIMD_UNOP_PREFIX)
#undef VISIT_SIMD_UNOP_PREFIX
#undef SIMD_UNOP_PREFIX_LIST

#define VISIT_SIMD_ANYTRUE(Opcode)                                  \
  void InstructionSelector::Visit##Opcode(Node* node) {             \
    IA32OperandGenerator g(this);                                   \
    InstructionOperand temps[] = {g.TempRegister()};                \
    Emit(kIA32##Opcode, g.DefineAsRegister(node),                   \
         g.UseRegister(node->InputAt(0)), arraysize(temps), temps); \
  }
SIMD_ANYTRUE_LIST(VISIT_SIMD_ANYTRUE)
#undef VISIT_SIMD_ANYTRUE
#undef SIMD_ANYTRUE_LIST

#define VISIT_SIMD_ALLTRUE(Opcode)                                         \
  void InstructionSelector::Visit##Opcode(Node* node) {                    \
    IA32OperandGenerator g(this);                                          \
    InstructionOperand temps[] = {g.TempRegister()};                       \
    Emit(kIA32##Opcode, g.DefineAsRegister(node), g.Use(node->InputAt(0)), \
         arraysize(temps), temps);                                         \
  }
SIMD_ALLTRUE_LIST(VISIT_SIMD_ALLTRUE)
#undef VISIT_SIMD_ALLTRUE
#undef SIMD_ALLTRUE_LIST

#define VISIT_SIMD_BINOP(Opcode)                           \
  void InstructionSelector::Visit##Opcode(Node* node) {    \
    VisitRROFloat(this, node, kAVX##Opcode, kSSE##Opcode); \
  }
SIMD_BINOP_LIST(VISIT_SIMD_BINOP)
#undef VISIT_SIMD_BINOP
#undef SIMD_BINOP_LIST

void VisitPack(InstructionSelector* selector, Node* node, ArchOpcode avx_opcode,
               ArchOpcode sse_opcode) {
  IA32OperandGenerator g(selector);
  InstructionOperand operand0 = g.UseRegister(node->InputAt(0));
  InstructionOperand operand1 = g.Use(node->InputAt(1));
  if (selector->IsSupported(AVX)) {
    selector->Emit(avx_opcode, g.DefineSameAsFirst(node), operand0, operand1);
  } else {
    selector->Emit(sse_opcode, g.DefineSameAsFirst(node), operand0, operand1);
  }
}

void InstructionSelector::VisitI16x8UConvertI32x4(Node* node) {
  VisitPack(this, node, kAVXI16x8UConvertI32x4, kSSEI16x8UConvertI32x4);
}

void InstructionSelector::VisitI8x16UConvertI16x8(Node* node) {
  VisitPack(this, node, kAVXI8x16UConvertI16x8, kSSEI8x16UConvertI16x8);
}

void InstructionSelector::VisitInt32AbsWithOverflow(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitInt64AbsWithOverflow(Node* node) {
  UNREACHABLE();
}

namespace {

// Packs a 4 lane shuffle into a single imm8 suitable for use by pshufd,
// pshuflw, and pshufhw.
uint8_t PackShuffle4(uint8_t* shuffle) {
  return (shuffle[0] & 3) | ((shuffle[1] & 3) << 2) | ((shuffle[2] & 3) << 4) |
         ((shuffle[3] & 3) << 6);
}

// Gets an 8 bit lane mask suitable for 16x8 pblendw.
uint8_t PackBlend8(const uint8_t* shuffle16x8) {
  int8_t result = 0;
  for (int i = 0; i < 8; ++i) {
    result |= (shuffle16x8[i] >= 8 ? 1 : 0) << i;
  }
  return result;
}

// Gets an 8 bit lane mask suitable for 32x4 pblendw.
uint8_t PackBlend4(const uint8_t* shuffle32x4) {
  int8_t result = 0;
  for (int i = 0; i < 4; ++i) {
    result |= (shuffle32x4[i] >= 4 ? 0x3 : 0) << (i * 2);
  }
  return result;
}

// Returns true if shuffle can be decomposed into two 16x4 half shuffles
// followed by a 16x8 blend.
// E.g. [3 2 1 0 15 14 13 12].
bool TryMatch16x8HalfShuffle(uint8_t* shuffle16x8, uint8_t* blend_mask) {
  *blend_mask = 0;
  for (int i = 0; i < 8; i++) {
    if ((shuffle16x8[i] & 0x4) != (i & 0x4)) return false;
    *blend_mask |= (shuffle16x8[i] > 7 ? 1 : 0) << i;
  }
  return true;
}

struct ShuffleEntry {
  uint8_t shuffle[kSimd128Size];
  ArchOpcode opcode;
  ArchOpcode avx_opcode;
  bool src0_needs_reg;
  bool src1_needs_reg;
};

// Shuffles that map to architecture-specific instruction sequences. These are
// matched very early, so we shouldn't include shuffles that match better in
// later tests, like 32x4 and 16x8 shuffles. In general, these patterns should
// map to either a single instruction, or be finer grained, such as zip/unzip or
// transpose patterns.
static const ShuffleEntry arch_shuffles[] = {
    {{0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23},
     kIA32S64x2UnpackLow,
     kIA32S64x2UnpackLow,
     true,
     false},
    {{8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31},
     kIA32S64x2UnpackHigh,
     kIA32S64x2UnpackHigh,
     true,
     false},
    {{0, 1, 2, 3, 16, 17, 18, 19, 4, 5, 6, 7, 20, 21, 22, 23},
     kIA32S32x4UnpackLow,
     kIA32S32x4UnpackLow,
     true,
     false},
    {{8, 9, 10, 11, 24, 25, 26, 27, 12, 13, 14, 15, 28, 29, 30, 31},
     kIA32S32x4UnpackHigh,
     kIA32S32x4UnpackHigh,
     true,
     false},
    {{0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21, 6, 7, 22, 23},
     kIA32S16x8UnpackLow,
     kIA32S16x8UnpackLow,
     true,
     false},
    {{8, 9, 24, 25, 10, 11, 26, 27, 12, 13, 28, 29, 14, 15, 30, 31},
     kIA32S16x8UnpackHigh,
     kIA32S16x8UnpackHigh,
     true,
     false},
    {{0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23},
     kIA32S8x16UnpackLow,
     kIA32S8x16UnpackLow,
     true,
     false},
    {{8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31},
     kIA32S8x16UnpackHigh,
     kIA32S8x16UnpackHigh,
     true,
     false},

    {{0, 1, 4, 5, 8, 9, 12, 13, 16, 17, 20, 21, 24, 25, 28, 29},
     kSSES16x8UnzipLow,
     kAVXS16x8UnzipLow,
     true,
     false},
    {{2, 3, 6, 7, 10, 11, 14, 15, 18, 19, 22, 23, 26, 27, 30, 31},
     kSSES16x8UnzipHigh,
     kAVXS16x8UnzipHigh,
     true,
     true},
    {{0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30},
     kSSES8x16UnzipLow,
     kAVXS8x16UnzipLow,
     true,
     true},
    {{1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31},
     kSSES8x16UnzipHigh,
     kAVXS8x16UnzipHigh,
     true,
     true},

    {{0, 16, 2, 18, 4, 20, 6, 22, 8, 24, 10, 26, 12, 28, 14, 30},
     kSSES8x16TransposeLow,
     kAVXS8x16TransposeLow,
     true,
     true},
    {{1, 17, 3, 19, 5, 21, 7, 23, 9, 25, 11, 27, 13, 29, 15, 31},
     kSSES8x16TransposeHigh,
     kAVXS8x16TransposeHigh,
     true,
     true},
    {{7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8},
     kSSES8x8Reverse,
     kAVXS8x8Reverse,
     false,
     false},
    {{3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12},
     kSSES8x4Reverse,
     kAVXS8x4Reverse,
     false,
     false},
    {{1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14},
     kSSES8x2Reverse,
     kAVXS8x2Reverse,
     true,
     true}};

bool TryMatchArchShuffle(const uint8_t* shuffle, const ShuffleEntry* table,
                         size_t num_entries, bool is_swizzle,
                         const ShuffleEntry** arch_shuffle) {
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
      *arch_shuffle = &entry;
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

  int imm_count = 0;
  static const int kMaxImms = 6;
  uint32_t imms[kMaxImms];
  int temp_count = 0;
  static const int kMaxTemps = 2;
  InstructionOperand temps[kMaxTemps];

  IA32OperandGenerator g(this);
  bool use_avx = CpuFeatures::IsSupported(AVX);
  // AVX and swizzles don't generally need DefineSameAsFirst to avoid a move.
  bool no_same_as_first = use_avx || is_swizzle;
  // We generally need UseRegister for input0, Use for input1.
  bool src0_needs_reg = true;
  bool src1_needs_reg = false;
  ArchOpcode opcode = kIA32S8x16Shuffle;  // general shuffle is the default

  uint8_t offset;
  uint8_t shuffle32x4[4];
  uint8_t shuffle16x8[8];
  int index;
  const ShuffleEntry* arch_shuffle;
  if (TryMatchConcat(shuffle, &offset)) {
    // Swap inputs from the normal order for (v)palignr.
    SwapShuffleInputs(node);
    is_swizzle = false;  // It's simpler to just handle the general case.
    no_same_as_first = use_avx;  // SSE requires same-as-first.
    opcode = kIA32S8x16Alignr;
    // palignr takes a single imm8 offset.
    imms[imm_count++] = offset;
  } else if (TryMatchArchShuffle(shuffle, arch_shuffles,
                                 arraysize(arch_shuffles), is_swizzle,
                                 &arch_shuffle)) {
    opcode = use_avx ? arch_shuffle->avx_opcode : arch_shuffle->opcode;
    src0_needs_reg = !use_avx || arch_shuffle->src0_needs_reg;
    // SSE can't take advantage of both operands in registers and needs
    // same-as-first.
    src1_needs_reg = use_avx && arch_shuffle->src1_needs_reg;
    no_same_as_first = use_avx;
  } else if (TryMatch32x4Shuffle(shuffle, shuffle32x4)) {
    uint8_t shuffle_mask = PackShuffle4(shuffle32x4);
    if (is_swizzle) {
      if (TryMatchIdentity(shuffle)) {
        // Bypass normal shuffle code generation in this case.
        EmitIdentity(node);
        return;
      } else {
        // pshufd takes a single imm8 shuffle mask.
        opcode = kIA32S32x4Swizzle;
        no_same_as_first = true;
        src0_needs_reg = false;
        imms[imm_count++] = shuffle_mask;
      }
    } else {
      // 2 operand shuffle
      // A blend is more efficient than a general 32x4 shuffle; try it first.
      if (TryMatchBlend(shuffle)) {
        opcode = kIA32S16x8Blend;
        uint8_t blend_mask = PackBlend4(shuffle32x4);
        imms[imm_count++] = blend_mask;
      } else {
        opcode = kIA32S32x4Shuffle;
        no_same_as_first = true;
        src0_needs_reg = false;
        imms[imm_count++] = shuffle_mask;
        int8_t blend_mask = PackBlend4(shuffle32x4);
        imms[imm_count++] = blend_mask;
      }
    }
  } else if (TryMatch16x8Shuffle(shuffle, shuffle16x8)) {
    uint8_t blend_mask;
    if (TryMatchBlend(shuffle)) {
      opcode = kIA32S16x8Blend;
      blend_mask = PackBlend8(shuffle16x8);
      imms[imm_count++] = blend_mask;
    } else if (TryMatchDup<8>(shuffle, &index)) {
      opcode = kIA32S16x8Dup;
      src0_needs_reg = false;
      imms[imm_count++] = index;
    } else if (TryMatch16x8HalfShuffle(shuffle16x8, &blend_mask)) {
      opcode = is_swizzle ? kIA32S16x8HalfShuffle1 : kIA32S16x8HalfShuffle2;
      // Half-shuffles don't need DefineSameAsFirst or UseRegister(src0).
      no_same_as_first = true;
      src0_needs_reg = false;
      uint8_t mask_lo = PackShuffle4(shuffle16x8);
      uint8_t mask_hi = PackShuffle4(shuffle16x8 + 4);
      imms[imm_count++] = mask_lo;
      imms[imm_count++] = mask_hi;
      if (!is_swizzle) imms[imm_count++] = blend_mask;
    }
  } else if (TryMatchDup<16>(shuffle, &index)) {
    opcode = kIA32S8x16Dup;
    no_same_as_first = use_avx;
    src0_needs_reg = true;
    imms[imm_count++] = index;
  }
  if (opcode == kIA32S8x16Shuffle) {
    // Use same-as-first for general swizzle, but not shuffle.
    no_same_as_first = !is_swizzle;
    src0_needs_reg = !no_same_as_first;
    imms[imm_count++] = Pack4Lanes(shuffle);
    imms[imm_count++] = Pack4Lanes(shuffle + 4);
    imms[imm_count++] = Pack4Lanes(shuffle + 8);
    imms[imm_count++] = Pack4Lanes(shuffle + 12);
    temps[temp_count++] = g.TempRegister();
  }

  // Use DefineAsRegister(node) and Use(src0) if we can without forcing an extra
  // move instruction in the CodeGenerator.
  Node* input0 = node->InputAt(0);
  InstructionOperand dst =
      no_same_as_first ? g.DefineAsRegister(node) : g.DefineSameAsFirst(node);
  InstructionOperand src0 =
      src0_needs_reg ? g.UseRegister(input0) : g.Use(input0);

  int input_count = 0;
  InstructionOperand inputs[2 + kMaxImms + kMaxTemps];
  inputs[input_count++] = src0;
  if (!is_swizzle) {
    Node* input1 = node->InputAt(1);
    inputs[input_count++] =
        src1_needs_reg ? g.UseRegister(input1) : g.Use(input1);
  }
  for (int i = 0; i < imm_count; ++i) {
    inputs[input_count++] = g.UseImmediate(imms[i]);
  }
  Emit(opcode, 1, &dst, input_count, inputs, temp_count, temps);
}

// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags =
      MachineOperatorBuilder::kWord32ShiftIsSafe |
      MachineOperatorBuilder::kWord32Ctz;
  if (CpuFeatures::IsSupported(POPCNT)) {
    flags |= MachineOperatorBuilder::kWord32Popcnt;
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    flags |= MachineOperatorBuilder::kFloat32RoundDown |
             MachineOperatorBuilder::kFloat64RoundDown |
             MachineOperatorBuilder::kFloat32RoundUp |
             MachineOperatorBuilder::kFloat64RoundUp |
             MachineOperatorBuilder::kFloat32RoundTruncate |
             MachineOperatorBuilder::kFloat64RoundTruncate |
             MachineOperatorBuilder::kFloat32RoundTiesEven |
             MachineOperatorBuilder::kFloat64RoundTiesEven;
  }
  return flags;
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
