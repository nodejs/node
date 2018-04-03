// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/adapters.h"
#include "src/compiler/instruction-selector-impl.h"
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
        // Constants in new space cannot be used as immediates in V8 because
        // the GC does not scan code objects when collecting the new generation.
        Handle<HeapObject> value = OpParameter<Handle<HeapObject>>(node);
        Isolate* isolate = value->GetIsolate();
        return !isolate->heap()->InNewSpace(*value);
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
                               : OpParameter<int32_t>(displacement_node);
    if (displacement_mode == kNegativeDisplacement) {
      displacement = -displacement;
    }
    if (base != nullptr) {
      if (base->opcode() == IrOpcode::kInt32Constant) {
        displacement += OpParameter<int32_t>(base);
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

  bool CanBeBetterLeftOperand(Node* node) const {
    return !selector()->IsLive(node);
  }
};


namespace {

void VisitRO(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  IA32OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
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

void InstructionSelector::VisitSpeculationFence(Node* node) {
  IA32OperandGenerator g(this);
  Emit(kLFence, g.NoOutput());
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
    case MachineRepresentation::kWord64:   // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }

  IA32OperandGenerator g(this);
  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(node);
  InstructionOperand inputs[3];
  size_t input_count = 0;
  AddressingMode mode =
      g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
  InstructionCode code = opcode | AddressingModeField::encode(mode);
  Emit(code, 1, outputs, input_count, inputs);
}

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
    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    if (g.CanBeImmediate(index)) {
      inputs[input_count++] = g.UseImmediate(index);
      addressing_mode = kMode_MRI;
    } else {
      inputs[input_count++] = g.UseUniqueRegister(index);
      addressing_mode = kMode_MR1;
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
      case MachineRepresentation::kWord64:   // Fall through.
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
  InstructionOperand outputs[2];
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

  if (cont->IsBranch()) {
    inputs[input_count++] = g.Label(cont->true_block());
    inputs[input_count++] = g.Label(cont->false_block());
  }

  outputs[output_count++] = g.DefineSameAsFirst(node);
  if (cont->IsSet()) {
    outputs[output_count++] = g.DefineAsByteRegister(cont->result());
  }

  DCHECK_NE(0u, input_count);
  DCHECK_NE(0u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  opcode = cont->Encode(opcode);
  if (cont->IsDeoptimize()) {
    selector->EmitDeoptimize(opcode, output_count, outputs, input_count, inputs,
                             cont->kind(), cont->reason(), cont->feedback(),
                             cont->frame_state());
  } else {
    selector->Emit(opcode, output_count, outputs, input_count, inputs);
  }
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
        g.UseRegister(node->InputAt(0)), g.UseUniqueRegister(node->InputAt(1)),
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
        g.UseRegister(node->InputAt(0)), g.UseUniqueRegister(node->InputAt(1)),
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
    InstructionOperand inputs[] = {g.UseUnique(node->InputAt(0)),
                                   g.UseUnique(node->InputAt(1)),
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

#define RO_OP_LIST(V)                                     \
  V(Word32Clz, kIA32Lzcnt)                                \
  V(Word32Ctz, kIA32Tzcnt)                                \
  V(Word32Popcnt, kIA32Popcnt)                            \
  V(ChangeFloat32ToFloat64, kSSEFloat32ToFloat64)         \
  V(RoundInt32ToFloat32, kSSEInt32ToFloat32)              \
  V(ChangeInt32ToFloat64, kSSEInt32ToFloat64)             \
  V(ChangeUint32ToFloat64, kSSEUint32ToFloat64)           \
  V(TruncateFloat32ToInt32, kSSEFloat32ToInt32)           \
  V(TruncateFloat32ToUint32, kSSEFloat32ToUint32)         \
  V(ChangeFloat64ToInt32, kSSEFloat64ToInt32)             \
  V(ChangeFloat64ToUint32, kSSEFloat64ToUint32)           \
  V(TruncateFloat64ToUint32, kSSEFloat64ToUint32)         \
  V(TruncateFloat64ToFloat32, kSSEFloat64ToFloat32)       \
  V(RoundFloat64ToInt32, kSSEFloat64ToInt32)              \
  V(BitcastFloat32ToInt32, kIA32BitcastFI)                \
  V(BitcastInt32ToFloat32, kIA32BitcastIF)                \
  V(Float32Sqrt, kSSEFloat32Sqrt)                         \
  V(Float64Sqrt, kSSEFloat64Sqrt)                         \
  V(Float64ExtractLowWord32, kSSEFloat64ExtractLowWord32) \
  V(Float64ExtractHighWord32, kSSEFloat64ExtractHighWord32)

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

#define RR_VISITOR(Name, opcode)                      \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRR(this, node, opcode);                      \
  }
RR_OP_LIST(RR_VISITOR)
#undef RR_VISITOR

#define RRO_FLOAT_VISITOR(Name, avx, sse)             \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRROFloat(this, node, avx, sse);              \
  }
RRO_FLOAT_OP_LIST(RRO_FLOAT_VISITOR)
#undef RRO_FLOAT_VISITOR

#define FLOAT_UNOP_VISITOR(Name, avx, sse)                  \
  void InstructionSelector::Visit##Name(Node* node) {       \
    VisitFloatUnop(this, node, node->InputAt(0), avx, sse); \
  }
FLOAT_UNOP_LIST(FLOAT_UNOP_VISITOR)
#undef FLOAT_UNOP_VISITOR

void InstructionSelector::VisitWord32ReverseBits(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord64ReverseBytes(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord32ReverseBytes(Node* node) { UNREACHABLE(); }

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
  InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
  Emit(kSSEUint32ToFloat32, g.DefineAsRegister(node), g.Use(node->InputAt(0)),
       arraysize(temps), temps);
}

void InstructionSelector::VisitFloat64Mod(Node* node) {
  IA32OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister(eax)};
  Emit(kSSEFloat64Mod, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)), 1,
       temps);
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
    ZoneVector<PushParameter>* arguments, const CallDescriptor* descriptor,
    Node* node) {
  IA32OperandGenerator g(this);

  // Prepare for C function call.
  if (descriptor->IsCFunctionCall()) {
    InstructionOperand temps[] = {g.TempRegister()};
    size_t const temp_count = arraysize(temps);
    Emit(kArchPrepareCallCFunction |
             MiscField::encode(static_cast<int>(descriptor->ParameterCount())),
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

void InstructionSelector::EmitPrepareResults(ZoneVector<PushParameter>* results,
                                             const CallDescriptor* descriptor,
                                             Node* node) {
  IA32OperandGenerator g(this);

  int reverse_slot = 0;
  for (PushParameter output : *results) {
    if (!output.location.IsCallerFrameSlot()) continue;
    // Skip any alignment holes in nodes.
    if (output.node != nullptr) {
      DCHECK(!descriptor->IsCFunctionCall());
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
  InstructionOperand inputs[6];
  AddressingMode addressing_mode =
      g.GetEffectiveAddressMemoryOperand(left, inputs, &input_count);
  opcode |= AddressingModeField::encode(addressing_mode);
  opcode = cont->Encode(opcode);
  inputs[input_count++] = right;

  if (cont->IsBranch()) {
    inputs[input_count++] = g.Label(cont->true_block());
    inputs[input_count++] = g.Label(cont->false_block());
    selector->Emit(opcode, 0, nullptr, input_count, inputs);
  } else if (cont->IsDeoptimize()) {
    selector->EmitDeoptimize(opcode, 0, nullptr, input_count, inputs,
                             cont->kind(), cont->reason(), cont->feedback(),
                             cont->frame_state());
  } else if (cont->IsSet()) {
    InstructionOperand output = g.DefineAsRegister(cont->result());
    selector->Emit(opcode, 1, &output, input_count, inputs);
  } else {
    DCHECK(cont->IsTrap());
    inputs[input_count++] = g.UseImmediate(cont->trap_id());
    selector->Emit(opcode, 0, nullptr, input_count, inputs);
  }
}

// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  InstructionOperand left, InstructionOperand right,
                  FlagsContinuation* cont) {
  IA32OperandGenerator g(selector);
  opcode = cont->Encode(opcode);
  if (cont->IsBranch()) {
    selector->Emit(opcode, g.NoOutput(), left, right,
                   g.Label(cont->true_block()), g.Label(cont->false_block()));
  } else if (cont->IsDeoptimize()) {
    selector->EmitDeoptimize(opcode, g.NoOutput(), left, right, cont->kind(),
                             cont->reason(), cont->feedback(),
                             cont->frame_state());
  } else if (cont->IsSet()) {
    selector->Emit(opcode, g.DefineAsByteRegister(cont->result()), left, right);
  } else {
    DCHECK(cont->IsTrap());
    selector->Emit(opcode, g.NoOutput(), left, right,
                   g.UseImmediate(cont->trap_id()));
  }
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
                             ? OpParameter<int32_t>(node)
                             : OpParameter<int64_t>(node);
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
  IA32OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  if (m.left().IsLoad() && m.right().IsLoadStackPointer()) {
    LoadMatcher<ExternalReferenceMatcher> mleft(m.left().node());
    ExternalReference js_stack_limit =
        ExternalReference::address_of_stack_limit(selector->isolate());
    if (mleft.object().Is(js_stack_limit) && mleft.index().Is(0)) {
      // Compare(Load(js_stack_limit), LoadStackPointer)
      if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
      InstructionCode opcode = cont->Encode(kIA32StackCheck);
      if (cont->IsBranch()) {
        selector->Emit(opcode, g.NoOutput(), g.Label(cont->true_block()),
                       g.Label(cont->false_block()));
      } else if (cont->IsDeoptimize()) {
        selector->EmitDeoptimize(opcode, 0, nullptr, 0, nullptr, cont->kind(),
                                 cont->reason(), cont->feedback(),
                                 cont->frame_state());
      } else {
        DCHECK(cont->IsSet());
        selector->Emit(opcode, g.DefineAsRegister(cont->result()));
      }
      return;
    }
  }
  VisitWordCompare(selector, node, kIA32Cmp, cont);
}


// Shared routine for word comparison with zero.
void VisitWordCompareZero(InstructionSelector* selector, Node* user,
                          Node* value, FlagsContinuation* cont) {
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
        return VisitWordCompare(selector, value, cont);
      case IrOpcode::kInt32LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWordCompare(selector, value, cont);
      case IrOpcode::kInt32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWordCompare(selector, value, cont);
      case IrOpcode::kUint32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWordCompare(selector, value, cont);
      case IrOpcode::kUint32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWordCompare(selector, value, cont);
      case IrOpcode::kFloat32Equal:
        cont->OverwriteAndNegateIfEqual(kUnorderedEqual);
        return VisitFloat32Compare(selector, value, cont);
      case IrOpcode::kFloat32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThan);
        return VisitFloat32Compare(selector, value, cont);
      case IrOpcode::kFloat32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThanOrEqual);
        return VisitFloat32Compare(selector, value, cont);
      case IrOpcode::kFloat64Equal:
        cont->OverwriteAndNegateIfEqual(kUnorderedEqual);
        return VisitFloat64Compare(selector, value, cont);
      case IrOpcode::kFloat64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThan);
        return VisitFloat64Compare(selector, value, cont);
      case IrOpcode::kFloat64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThanOrEqual);
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
                return VisitBinop(selector, node, kIA32Add, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kIA32Sub, cont);
              case IrOpcode::kInt32MulWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kIA32Imul, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Sub:
        return VisitWordCompare(selector, value, cont);
      case IrOpcode::kWord32And:
        return VisitWordCompare(selector, value, kIA32Test, cont);
      default:
        break;
    }
  }

  // Continuation could not be combined with a compare, emit compare against 0.
  IA32OperandGenerator g(selector);
  VisitCompare(selector, kIA32Cmp, g.Use(value), g.TempImmediate(0), cont);
}

}  // namespace

void InstructionSelector::VisitBranch(Node* branch, BasicBlock* tbranch,
                                      BasicBlock* fbranch) {
  FlagsContinuation cont(kNotEqual, tbranch, fbranch);
  VisitWordCompareZero(this, branch, branch->InputAt(0), &cont);
}

void InstructionSelector::VisitDeoptimizeIf(Node* node) {
  DeoptimizeParameters p = DeoptimizeParametersOf(node->op());
  FlagsContinuation cont = FlagsContinuation::ForDeoptimize(
      kNotEqual, p.kind(), p.reason(), p.feedback(), node->InputAt(1));
  VisitWordCompareZero(this, node, node->InputAt(0), &cont);
}

void InstructionSelector::VisitDeoptimizeUnless(Node* node) {
  DeoptimizeParameters p = DeoptimizeParametersOf(node->op());
  FlagsContinuation cont = FlagsContinuation::ForDeoptimize(
      kEqual, p.kind(), p.reason(), p.feedback(), node->InputAt(1));
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
  IA32OperandGenerator g(this);
  InstructionOperand value_operand = g.UseRegister(node->InputAt(0));

  // Emit either ArchTableSwitch or ArchLookupSwitch.
  static const size_t kMaxTableSwitchValueRange = 2 << 16;
  size_t table_space_cost = 4 + sw.value_range;
  size_t table_time_cost = 3;
  size_t lookup_space_cost = 3 + 2 * sw.case_count;
  size_t lookup_time_cost = sw.case_count;
  if (sw.case_count > 4 &&
      table_space_cost + 3 * table_time_cost <=
          lookup_space_cost + 3 * lookup_time_cost &&
      sw.min_value > std::numeric_limits<int32_t>::min() &&
      sw.value_range <= kMaxTableSwitchValueRange) {
    InstructionOperand index_operand = value_operand;
    if (sw.min_value) {
      index_operand = g.TempRegister();
      Emit(kIA32Lea | AddressingModeField::encode(kMode_MRI), index_operand,
           value_operand, g.TempImmediate(-sw.min_value));
    }
    // Generate a table lookup.
    return EmitTableSwitch(sw, index_operand);
  }

  // Generate a sequence of conditional jumps.
  return EmitLookupSwitch(sw, value_operand);
}


void InstructionSelector::VisitWord32Equal(Node* const node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) {
    return VisitWordCompareZero(this, m.node(), m.left().node(), &cont);
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

void InstructionSelector::VisitAtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  DCHECK(load_rep.representation() == MachineRepresentation::kWord8 ||
         load_rep.representation() == MachineRepresentation::kWord16 ||
         load_rep.representation() == MachineRepresentation::kWord32);
  USE(load_rep);
  VisitLoad(node);
}

void InstructionSelector::VisitAtomicStore(Node* node) {
  IA32OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  MachineRepresentation rep = AtomicStoreRepresentationOf(node->op());
  ArchOpcode opcode = kArchNop;
  switch (rep) {
    case MachineRepresentation::kWord8:
      opcode = kAtomicExchangeInt8;
      break;
    case MachineRepresentation::kWord16:
      opcode = kAtomicExchangeInt16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kAtomicExchangeWord32;
      break;
    default:
      UNREACHABLE();
      break;
  }
  AddressingMode addressing_mode;
  InstructionOperand inputs[4];
  size_t input_count = 0;
  if (rep == MachineRepresentation::kWord8) {
    inputs[input_count++] = g.UseByteRegister(value);
  } else {
    inputs[input_count++] = g.UseUniqueRegister(value);
  }
  inputs[input_count++] = g.UseUniqueRegister(base);
  if (g.CanBeImmediate(index)) {
    inputs[input_count++] = g.UseImmediate(index);
    addressing_mode = kMode_MRI;
  } else {
    inputs[input_count++] = g.UseUniqueRegister(index);
    addressing_mode = kMode_MR1;
  }
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 0, nullptr, input_count, inputs);
}

void InstructionSelector::VisitAtomicExchange(Node* node) {
  IA32OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  MachineType type = AtomicOpRepresentationOf(node->op());
  ArchOpcode opcode = kArchNop;
  if (type == MachineType::Int8()) {
    opcode = kAtomicExchangeInt8;
  } else if (type == MachineType::Uint8()) {
    opcode = kAtomicExchangeUint8;
  } else if (type == MachineType::Int16()) {
    opcode = kAtomicExchangeInt16;
  } else if (type == MachineType::Uint16()) {
    opcode = kAtomicExchangeUint16;
  } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
    opcode = kAtomicExchangeWord32;
  } else {
    UNREACHABLE();
    return;
  }
  InstructionOperand outputs[1];
  AddressingMode addressing_mode;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  if (type == MachineType::Int8() || type == MachineType::Uint8()) {
    inputs[input_count++] = g.UseFixed(value, edx);
  } else {
    inputs[input_count++] = g.UseUniqueRegister(value);
  }
  inputs[input_count++] = g.UseUniqueRegister(base);
  if (g.CanBeImmediate(index)) {
    inputs[input_count++] = g.UseImmediate(index);
    addressing_mode = kMode_MRI;
  } else {
    inputs[input_count++] = g.UseUniqueRegister(index);
    addressing_mode = kMode_MR1;
  }
  if (type == MachineType::Int8() || type == MachineType::Uint8()) {
    // Using DefineSameAsFirst requires the register to be unallocated.
    outputs[0] = g.DefineAsFixed(node, edx);
  } else {
    outputs[0] = g.DefineSameAsFirst(node);
  }
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 1, outputs, input_count, inputs);
}

void InstructionSelector::VisitAtomicCompareExchange(Node* node) {
  IA32OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* old_value = node->InputAt(2);
  Node* new_value = node->InputAt(3);

  MachineType type = AtomicOpRepresentationOf(node->op());
  ArchOpcode opcode = kArchNop;
  if (type == MachineType::Int8()) {
    opcode = kAtomicCompareExchangeInt8;
  } else if (type == MachineType::Uint8()) {
    opcode = kAtomicCompareExchangeUint8;
  } else if (type == MachineType::Int16()) {
    opcode = kAtomicCompareExchangeInt16;
  } else if (type == MachineType::Uint16()) {
    opcode = kAtomicCompareExchangeUint16;
  } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
    opcode = kAtomicCompareExchangeWord32;
  } else {
    UNREACHABLE();
    return;
  }
  InstructionOperand outputs[1];
  AddressingMode addressing_mode;
  InstructionOperand inputs[4];
  size_t input_count = 0;
  inputs[input_count++] = g.UseFixed(old_value, eax);
  if (type == MachineType::Int8() || type == MachineType::Uint8()) {
    inputs[input_count++] = g.UseByteRegister(new_value);
  } else {
    inputs[input_count++] = g.UseUniqueRegister(new_value);
  }
  inputs[input_count++] = g.UseUniqueRegister(base);
  if (g.CanBeImmediate(index)) {
    inputs[input_count++] = g.UseImmediate(index);
    addressing_mode = kMode_MRI;
  } else {
    inputs[input_count++] = g.UseUniqueRegister(index);
    addressing_mode = kMode_MR1;
  }
  outputs[0] = g.DefineAsFixed(node, eax);
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 1, outputs, input_count, inputs);
}

void InstructionSelector::VisitAtomicBinaryOperation(
    Node* node, ArchOpcode int8_op, ArchOpcode uint8_op, ArchOpcode int16_op,
    ArchOpcode uint16_op, ArchOpcode word32_op) {
  IA32OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  MachineType type = AtomicOpRepresentationOf(node->op());
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
  InstructionOperand outputs[1];
  AddressingMode addressing_mode;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(value);
  inputs[input_count++] = g.UseUniqueRegister(base);
  if (g.CanBeImmediate(index)) {
    inputs[input_count++] = g.UseImmediate(index);
    addressing_mode = kMode_MRI;
  } else {
    inputs[input_count++] = g.UseUniqueRegister(index);
    addressing_mode = kMode_MR1;
  }
  outputs[0] = g.DefineAsFixed(node, eax);
  InstructionOperand temp[1];
  if (type == MachineType::Int8() || type == MachineType::Uint8()) {
    temp[0] = g.UseByteRegister(node);
  } else {
    temp[0] = g.TempRegister();
  }
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 1, outputs, input_count, inputs, 1, temp);
}

#define VISIT_ATOMIC_BINOP(op)                                              \
  void InstructionSelector::VisitAtomic##op(Node* node) {                   \
    VisitAtomicBinaryOperation(node, kAtomic##op##Int8, kAtomic##op##Uint8, \
                               kAtomic##op##Int16, kAtomic##op##Uint16,     \
                               kAtomic##op##Word32);                        \
  }
VISIT_ATOMIC_BINOP(Add)
VISIT_ATOMIC_BINOP(Sub)
VISIT_ATOMIC_BINOP(And)
VISIT_ATOMIC_BINOP(Or)
VISIT_ATOMIC_BINOP(Xor)
#undef VISIT_ATOMIC_BINOP

#define SIMD_INT_TYPES(V) \
  V(I32x4)                \
  V(I16x8)                \
  V(I8x16)

#define SIMD_BINOP_LIST(V) \
  V(F32x4Add)              \
  V(F32x4Sub)              \
  V(F32x4Mul)              \
  V(F32x4Min)              \
  V(F32x4Max)              \
  V(F32x4Eq)               \
  V(F32x4Ne)               \
  V(F32x4Lt)               \
  V(F32x4Le)               \
  V(I32x4Add)              \
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
  V(I16x8Add)              \
  V(I16x8AddSaturateS)     \
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

#define SIMD_INT_UNOP_LIST(V) \
  V(I32x4Neg)                 \
  V(I16x8Neg)                 \
  V(I8x16Neg)

#define SIMD_OTHER_UNOP_LIST(V) \
  V(F32x4Abs)                   \
  V(F32x4Neg)                   \
  V(S128Not)

#define SIMD_SHIFT_OPCODES(V) \
  V(I32x4Shl)                 \
  V(I32x4ShrS)                \
  V(I32x4ShrU)                \
  V(I16x8Shl)                 \
  V(I16x8ShrS)                \
  V(I16x8ShrU)

void InstructionSelector::VisitF32x4Splat(Node* node) {
  IA32OperandGenerator g(this);
  InstructionOperand operand0 = g.UseRegister(node->InputAt(0));
  if (IsSupported(AVX)) {
    Emit(kAVXF32x4Splat, g.DefineAsRegister(node), operand0);
  } else {
    Emit(kSSEF32x4Splat, g.DefineSameAsFirst(node), operand0);
  }
}

void InstructionSelector::VisitF32x4ExtractLane(Node* node) {
  IA32OperandGenerator g(this);
  InstructionOperand operand0 = g.UseRegister(node->InputAt(0));
  InstructionOperand operand1 = g.UseImmediate(OpParameter<int32_t>(node));
  if (IsSupported(AVX)) {
    Emit(kAVXF32x4ExtractLane, g.DefineAsRegister(node), operand0, operand1);
  } else {
    Emit(kSSEF32x4ExtractLane, g.DefineSameAsFirst(node), operand0, operand1);
  }
}

void InstructionSelector::VisitS128Zero(Node* node) {
  IA32OperandGenerator g(this);
  Emit(kIA32S128Zero, g.DefineAsRegister(node));
}


#define VISIT_SIMD_SPLAT(Type)                               \
  void InstructionSelector::Visit##Type##Splat(Node* node) { \
    VisitRO(this, node, kIA32##Type##Splat);                 \
  }
SIMD_INT_TYPES(VISIT_SIMD_SPLAT)
#undef VISIT_SIMD_SPLAT

#define VISIT_SIMD_EXTRACT_LANE(Type)                              \
  void InstructionSelector::Visit##Type##ExtractLane(Node* node) { \
    IA32OperandGenerator g(this);                                  \
    int32_t lane = OpParameter<int32_t>(node);                     \
    Emit(kIA32##Type##ExtractLane, g.DefineAsRegister(node),       \
         g.UseRegister(node->InputAt(0)), g.UseImmediate(lane));   \
  }
SIMD_INT_TYPES(VISIT_SIMD_EXTRACT_LANE)
#undef VISIT_SIMD_EXTRACT_LANE

#define VISIT_SIMD_REPLACE_LANE(Type)                                         \
  void InstructionSelector::Visit##Type##ReplaceLane(Node* node) {            \
    IA32OperandGenerator g(this);                                             \
    InstructionOperand operand0 = g.UseRegister(node->InputAt(0));            \
    InstructionOperand operand1 = g.UseImmediate(OpParameter<int32_t>(node)); \
    InstructionOperand operand2 = g.Use(node->InputAt(1));                    \
    if (IsSupported(AVX)) {                                                   \
      Emit(kAVX##Type##ReplaceLane, g.DefineAsRegister(node), operand0,       \
           operand1, operand2);                                               \
    } else {                                                                  \
      Emit(kSSE##Type##ReplaceLane, g.DefineSameAsFirst(node), operand0,      \
           operand1, operand2);                                               \
    }                                                                         \
  }
SIMD_INT_TYPES(VISIT_SIMD_REPLACE_LANE)
VISIT_SIMD_REPLACE_LANE(F32x4)
#undef VISIT_SIMD_REPLACE_LANE

#define VISIT_SIMD_SHIFT(Opcode)                                              \
  void InstructionSelector::Visit##Opcode(Node* node) {                       \
    IA32OperandGenerator g(this);                                             \
    InstructionOperand operand0 = g.UseRegister(node->InputAt(0));            \
    InstructionOperand operand1 = g.UseImmediate(OpParameter<int32_t>(node)); \
    if (IsSupported(AVX)) {                                                   \
      Emit(kAVX##Opcode, g.DefineAsRegister(node), operand0, operand1);       \
    } else {                                                                  \
      Emit(kSSE##Opcode, g.DefineSameAsFirst(node), operand0, operand1);      \
    }                                                                         \
  }
SIMD_SHIFT_OPCODES(VISIT_SIMD_SHIFT)
#undef VISIT_SIMD_SHIFT

#define VISIT_SIMD_INT_UNOP(Opcode)                                         \
  void InstructionSelector::Visit##Opcode(Node* node) {                     \
    IA32OperandGenerator g(this);                                           \
    Emit(kIA32##Opcode, g.DefineAsRegister(node), g.Use(node->InputAt(0))); \
  }
SIMD_INT_UNOP_LIST(VISIT_SIMD_INT_UNOP)
#undef VISIT_SIMD_INT_UNOP

#define VISIT_SIMD_OTHER_UNOP(Opcode)                                        \
  void InstructionSelector::Visit##Opcode(Node* node) {                      \
    IA32OperandGenerator g(this);                                            \
    InstructionCode opcode = IsSupported(AVX) ? kAVX##Opcode : kSSE##Opcode; \
    Emit(opcode, g.DefineAsRegister(node), g.Use(node->InputAt(0)));         \
  }
SIMD_OTHER_UNOP_LIST(VISIT_SIMD_OTHER_UNOP)
#undef VISIT_SIMD_OTHER_UNOP

#define VISIT_SIMD_BINOP(Opcode)                           \
  void InstructionSelector::Visit##Opcode(Node* node) {    \
    VisitRROFloat(this, node, kAVX##Opcode, kSSE##Opcode); \
  }
SIMD_BINOP_LIST(VISIT_SIMD_BINOP)
#undef VISIT_SIMD_BINOP

void InstructionSelector::VisitInt32AbsWithOverflow(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitInt64AbsWithOverflow(Node* node) {
  UNREACHABLE();
}

// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags =
      MachineOperatorBuilder::kWord32ShiftIsSafe |
      MachineOperatorBuilder::kWord32Ctz |
      MachineOperatorBuilder::kSpeculationFence;
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
