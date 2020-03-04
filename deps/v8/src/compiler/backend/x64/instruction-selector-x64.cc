// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "src/base/iterator.h"
#include "src/base/overflowing-math.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/roots/roots-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

// Adds X64-specific methods for generating operands.
class X64OperandGenerator final : public OperandGenerator {
 public:
  explicit X64OperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  bool CanBeImmediate(Node* node) {
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
      case IrOpcode::kRelocatableInt32Constant:
        return true;
      case IrOpcode::kInt64Constant: {
        const int64_t value = OpParameter<int64_t>(node->op());
        return std::numeric_limits<int32_t>::min() < value &&
               value <= std::numeric_limits<int32_t>::max();
      }
      case IrOpcode::kNumberConstant: {
        const double value = OpParameter<double>(node->op());
        return bit_cast<int64_t>(value) == 0;
      }
      default:
        return false;
    }
  }

  int32_t GetImmediateIntegerValue(Node* node) {
    DCHECK(CanBeImmediate(node));
    if (node->opcode() == IrOpcode::kInt32Constant) {
      return OpParameter<int32_t>(node->op());
    }
    DCHECK_EQ(IrOpcode::kInt64Constant, node->opcode());
    return static_cast<int32_t>(OpParameter<int64_t>(node->op()));
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
      case kX64And:
      case kX64Or:
      case kX64Xor:
      case kX64Add:
      case kX64Sub:
      case kX64Push:
      case kX64Cmp:
      case kX64Test:
        // When pointer compression is enabled 64-bit memory operands can't be
        // used for tagged values.
        return rep == MachineRepresentation::kWord64 ||
               (!COMPRESS_POINTERS_BOOL && IsAnyTagged(rep));
      case kX64And32:
      case kX64Or32:
      case kX64Xor32:
      case kX64Add32:
      case kX64Sub32:
      case kX64Cmp32:
      case kX64Test32:
        // When pointer compression is enabled 32-bit memory operands can be
        // used for tagged values.
        return rep == MachineRepresentation::kWord32 ||
               (COMPRESS_POINTERS_BOOL && IsAnyTagged(rep));
      case kX64Cmp16:
      case kX64Test16:
        return rep == MachineRepresentation::kWord16;
      case kX64Cmp8:
      case kX64Test8:
        return rep == MachineRepresentation::kWord8;
      default:
        break;
    }
    return false;
  }

  AddressingMode GenerateMemoryOperandInputs(Node* index, int scale_exponent,
                                             Node* base, Node* displacement,
                                             DisplacementMode displacement_mode,
                                             InstructionOperand inputs[],
                                             size_t* input_count) {
    AddressingMode mode = kMode_MRI;
    if (base != nullptr && (index != nullptr || displacement != nullptr)) {
      if (base->opcode() == IrOpcode::kInt32Constant &&
          OpParameter<int32_t>(base->op()) == 0) {
        base = nullptr;
      } else if (base->opcode() == IrOpcode::kInt64Constant &&
                 OpParameter<int64_t>(base->op()) == 0) {
        base = nullptr;
      }
    }
    if (base != nullptr) {
      inputs[(*input_count)++] = UseRegister(base);
      if (index != nullptr) {
        DCHECK(scale_exponent >= 0 && scale_exponent <= 3);
        inputs[(*input_count)++] = UseRegister(index);
        if (displacement != nullptr) {
          inputs[(*input_count)++] = displacement_mode == kNegativeDisplacement
                                         ? UseNegatedImmediate(displacement)
                                         : UseImmediate(displacement);
          static const AddressingMode kMRnI_modes[] = {kMode_MR1I, kMode_MR2I,
                                                       kMode_MR4I, kMode_MR8I};
          mode = kMRnI_modes[scale_exponent];
        } else {
          static const AddressingMode kMRn_modes[] = {kMode_MR1, kMode_MR2,
                                                      kMode_MR4, kMode_MR8};
          mode = kMRn_modes[scale_exponent];
        }
      } else {
        if (displacement == nullptr) {
          mode = kMode_MR;
        } else {
          inputs[(*input_count)++] = displacement_mode == kNegativeDisplacement
                                         ? UseNegatedImmediate(displacement)
                                         : UseImmediate(displacement);
          mode = kMode_MRI;
        }
      }
    } else {
      DCHECK(scale_exponent >= 0 && scale_exponent <= 3);
      if (displacement != nullptr) {
        if (index == nullptr) {
          inputs[(*input_count)++] = UseRegister(displacement);
          mode = kMode_MR;
        } else {
          inputs[(*input_count)++] = UseRegister(index);
          inputs[(*input_count)++] = displacement_mode == kNegativeDisplacement
                                         ? UseNegatedImmediate(displacement)
                                         : UseImmediate(displacement);
          static const AddressingMode kMnI_modes[] = {kMode_MRI, kMode_M2I,
                                                      kMode_M4I, kMode_M8I};
          mode = kMnI_modes[scale_exponent];
        }
      } else {
        inputs[(*input_count)++] = UseRegister(index);
        static const AddressingMode kMn_modes[] = {kMode_MR, kMode_MR1,
                                                   kMode_M4, kMode_M8};
        mode = kMn_modes[scale_exponent];
        if (mode == kMode_MR1) {
          // [%r1 + %r1*1] has a smaller encoding than [%r1*2+0]
          inputs[(*input_count)++] = UseRegister(index);
        }
      }
    }
    return mode;
  }

  AddressingMode GetEffectiveAddressMemoryOperand(Node* operand,
                                                  InstructionOperand inputs[],
                                                  size_t* input_count) {
    {
      LoadMatcher<ExternalReferenceMatcher> m(operand);
      if (m.index().HasValue() && m.object().HasValue() &&
          selector()->CanAddressRelativeToRootsRegister(m.object().Value())) {
        ptrdiff_t const delta =
            m.index().Value() +
            TurboAssemblerBase::RootRegisterOffsetForExternalReference(
                selector()->isolate(), m.object().Value());
        if (is_int32(delta)) {
          inputs[(*input_count)++] = TempImmediate(static_cast<int32_t>(delta));
          return kMode_Root;
        }
      }
    }
    BaseWithIndexAndDisplacement64Matcher m(operand, AddressOption::kAllowAll);
    DCHECK(m.matches());
    if (m.displacement() == nullptr || CanBeImmediate(m.displacement())) {
      return GenerateMemoryOperandInputs(
          m.index(), m.scale(), m.base(), m.displacement(),
          m.displacement_mode(), inputs, input_count);
    } else if (m.base() == nullptr &&
               m.displacement_mode() == kPositiveDisplacement) {
      // The displacement cannot be an immediate, but we can use the
      // displacement as base instead and still benefit from addressing
      // modes for the scale.
      return GenerateMemoryOperandInputs(m.index(), m.scale(), m.displacement(),
                                         nullptr, m.displacement_mode(), inputs,
                                         input_count);
    } else {
      inputs[(*input_count)++] = UseRegister(operand->InputAt(0));
      inputs[(*input_count)++] = UseRegister(operand->InputAt(1));
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
ArchOpcode GetLoadOpcode(LoadRepresentation load_rep) {
  ArchOpcode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kX64Movss;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kX64Movsd;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsSigned() ? kX64Movsxbl : kX64Movzxbl;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsSigned() ? kX64Movsxwl : kX64Movzxwl;
      break;
    case MachineRepresentation::kWord32:
      opcode = kX64Movl;
      break;
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:
#ifdef V8_COMPRESS_POINTERS
      opcode = kX64Movl;
      break;
#else
      UNREACHABLE();
#endif
#ifdef V8_COMPRESS_POINTERS
    case MachineRepresentation::kTaggedSigned:
      opcode = kX64MovqDecompressTaggedSigned;
      break;
    case MachineRepresentation::kTaggedPointer:
      opcode = kX64MovqDecompressTaggedPointer;
      break;
    case MachineRepresentation::kTagged:
      opcode = kX64MovqDecompressAnyTagged;
      break;
#else
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
#endif
    case MachineRepresentation::kWord64:
      opcode = kX64Movq;
      break;
    case MachineRepresentation::kSimd128:  // Fall through.
      opcode = kX64Movdqu;
      break;
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }
  return opcode;
}

ArchOpcode GetStoreOpcode(StoreRepresentation store_rep) {
  switch (store_rep.representation()) {
    case MachineRepresentation::kFloat32:
      return kX64Movss;
    case MachineRepresentation::kFloat64:
      return kX64Movsd;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      return kX64Movb;
    case MachineRepresentation::kWord16:
      return kX64Movw;
    case MachineRepresentation::kWord32:
      return kX64Movl;
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:
#ifdef V8_COMPRESS_POINTERS
      return kX64MovqCompressTagged;
#else
      UNREACHABLE();
#endif
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:
      return kX64MovqCompressTagged;
    case MachineRepresentation::kWord64:
      return kX64Movq;
    case MachineRepresentation::kSimd128:  // Fall through.
      return kX64Movdqu;
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }
  UNREACHABLE();
}

}  // namespace

void InstructionSelector::VisitStackSlot(Node* node) {
  StackSlotRepresentation rep = StackSlotRepresentationOf(node->op());
  int slot = frame_->AllocateSpillSlot(rep.size());
  OperandGenerator g(this);

  Emit(kArchStackSlot, g.DefineAsRegister(node),
       sequence()->AddImmediate(Constant(slot)), 0, nullptr);
}

void InstructionSelector::VisitAbortCSAAssert(Node* node) {
  X64OperandGenerator g(this);
  Emit(kArchAbortCSAAssert, g.NoOutput(), g.UseFixed(node->InputAt(0), rdx));
}

void InstructionSelector::VisitLoadTransform(Node* node) {
  LoadTransformParameters params = LoadTransformParametersOf(node->op());
  ArchOpcode opcode = kArchNop;
  switch (params.transformation) {
    case LoadTransformation::kS8x16LoadSplat:
      opcode = kX64S8x16LoadSplat;
      break;
    case LoadTransformation::kS16x8LoadSplat:
      opcode = kX64S16x8LoadSplat;
      break;
    case LoadTransformation::kS32x4LoadSplat:
      opcode = kX64S32x4LoadSplat;
      break;
    case LoadTransformation::kS64x2LoadSplat:
      opcode = kX64S64x2LoadSplat;
      break;
    case LoadTransformation::kI16x8Load8x8S:
      opcode = kX64I16x8Load8x8S;
      break;
    case LoadTransformation::kI16x8Load8x8U:
      opcode = kX64I16x8Load8x8U;
      break;
    case LoadTransformation::kI32x4Load16x4S:
      opcode = kX64I32x4Load16x4S;
      break;
    case LoadTransformation::kI32x4Load16x4U:
      opcode = kX64I32x4Load16x4U;
      break;
    case LoadTransformation::kI64x2Load32x2S:
      opcode = kX64I64x2Load32x2S;
      break;
    case LoadTransformation::kI64x2Load32x2U:
      opcode = kX64I64x2Load32x2U;
      break;
    default:
      UNREACHABLE();
  }
  // x64 supports unaligned loads
  DCHECK_NE(params.kind, LoadKind::kUnaligned);
  InstructionCode code = opcode;
  if (params.kind == LoadKind::kProtected) {
    code |= MiscField::encode(kMemoryAccessProtected);
  }
  VisitLoad(node, node, code);
}

void InstructionSelector::VisitLoad(Node* node, Node* value,
                                    InstructionCode opcode) {
  X64OperandGenerator g(this);
  InstructionOperand outputs[] = {g.DefineAsRegister(node)};
  InstructionOperand inputs[3];
  size_t input_count = 0;
  AddressingMode mode =
      g.GetEffectiveAddressMemoryOperand(value, inputs, &input_count);
  InstructionCode code = opcode | AddressingModeField::encode(mode);
  if (node->opcode() == IrOpcode::kProtectedLoad) {
    code |= MiscField::encode(kMemoryAccessProtected);
  } else if (node->opcode() == IrOpcode::kPoisonedLoad) {
    CHECK_NE(poisoning_level_, PoisoningMitigationLevel::kDontPoison);
    code |= MiscField::encode(kMemoryAccessPoisoned);
  }
  Emit(code, 1, outputs, input_count, inputs);
}

void InstructionSelector::VisitLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  VisitLoad(node, node, GetLoadOpcode(load_rep));
}

void InstructionSelector::VisitPoisonedLoad(Node* node) { VisitLoad(node); }

void InstructionSelector::VisitProtectedLoad(Node* node) { VisitLoad(node); }

void InstructionSelector::VisitStore(Node* node) {
  X64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());
  WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();

  if (write_barrier_kind != kNoWriteBarrier &&
      V8_LIKELY(!FLAG_disable_write_barriers)) {
    DCHECK(CanBeTaggedOrCompressedPointer(store_rep.representation()));
    AddressingMode addressing_mode;
    InstructionOperand inputs[] = {
        g.UseUniqueRegister(base),
        g.GetEffectiveIndexOperand(index, &addressing_mode),
        g.UseUniqueRegister(value)};
    RecordWriteMode record_write_mode =
        WriteBarrierKindToRecordWriteMode(write_barrier_kind);
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    InstructionCode code = kArchStoreWithWriteBarrier;
    code |= AddressingModeField::encode(addressing_mode);
    code |= MiscField::encode(static_cast<int>(record_write_mode));
    Emit(code, 0, nullptr, arraysize(inputs), inputs, arraysize(temps), temps);
  } else {
    ArchOpcode opcode = GetStoreOpcode(store_rep);
    InstructionOperand inputs[4];
    size_t input_count = 0;
    AddressingMode addressing_mode =
        g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
    InstructionCode code =
        opcode | AddressingModeField::encode(addressing_mode);
    if ((ElementSizeLog2Of(store_rep.representation()) <
         kSystemPointerSizeLog2) &&
        (value->opcode() == IrOpcode::kTruncateInt64ToInt32) &&
        CanCover(node, value)) {
      value = value->InputAt(0);
    }
    InstructionOperand value_operand =
        g.CanBeImmediate(value) ? g.UseImmediate(value) : g.UseRegister(value);
    inputs[input_count++] = value_operand;
    Emit(code, 0, static_cast<InstructionOperand*>(nullptr), input_count,
         inputs);
  }
}

void InstructionSelector::VisitProtectedStore(Node* node) {
  X64OperandGenerator g(this);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());

  ArchOpcode opcode = GetStoreOpcode(store_rep);
  InstructionOperand inputs[4];
  size_t input_count = 0;
  AddressingMode addressing_mode =
      g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode) |
                         MiscField::encode(kMemoryAccessProtected);
  InstructionOperand value_operand =
      g.CanBeImmediate(value) ? g.UseImmediate(value) : g.UseRegister(value);
  inputs[input_count++] = value_operand;
  Emit(code, 0, static_cast<InstructionOperand*>(nullptr), input_count, inputs);
}

// Architecture supports unaligned access, therefore VisitLoad is used instead
void InstructionSelector::VisitUnalignedLoad(Node* node) { UNREACHABLE(); }

// Architecture supports unaligned access, therefore VisitStore is used instead
void InstructionSelector::VisitUnalignedStore(Node* node) { UNREACHABLE(); }

// Shared routine for multiple binary operations.
static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();
  InstructionOperand inputs[8];
  size_t input_count = 0;
  InstructionOperand outputs[1];
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

  DCHECK_NE(0u, input_count);
  DCHECK_EQ(1u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}

// Shared routine for multiple binary operations.
static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode) {
  FlagsContinuation cont;
  VisitBinop(selector, node, opcode, &cont);
}

void InstructionSelector::VisitWord32And(Node* node) {
  X64OperandGenerator g(this);
  Uint32BinopMatcher m(node);
  if (m.right().Is(0xFF)) {
    Emit(kX64Movzxbl, g.DefineAsRegister(node), g.Use(m.left().node()));
  } else if (m.right().Is(0xFFFF)) {
    Emit(kX64Movzxwl, g.DefineAsRegister(node), g.Use(m.left().node()));
  } else {
    VisitBinop(this, node, kX64And32);
  }
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

void InstructionSelector::VisitStackPointerGreaterThan(
    Node* node, FlagsContinuation* cont) {
  StackCheckKind kind = StackCheckKindOf(node->op());
  InstructionCode opcode =
      kArchStackPointerGreaterThan | MiscField::encode(static_cast<int>(kind));

  int effect_level = GetEffectLevel(node);
  if (cont->IsBranch()) {
    effect_level =
        GetEffectLevel(cont->true_block()->PredecessorAt(0)->control_input());
  }

  X64OperandGenerator g(this);
  Node* const value = node->InputAt(0);
  if (g.CanBeMemoryOperand(kX64Cmp, node, value, effect_level)) {
    DCHECK_EQ(IrOpcode::kLoad, value->opcode());

    // GetEffectiveAddressMemoryOperand can create at most 3 inputs.
    static constexpr int kMaxInputCount = 3;

    size_t input_count = 0;
    InstructionOperand inputs[kMaxInputCount];
    AddressingMode addressing_mode =
        g.GetEffectiveAddressMemoryOperand(value, inputs, &input_count);
    opcode |= AddressingModeField::encode(addressing_mode);
    DCHECK_LE(input_count, kMaxInputCount);

    EmitWithContinuation(opcode, 0, nullptr, input_count, inputs, cont);
  } else {
    EmitWithContinuation(opcode, g.UseRegister(value), cont);
  }
}

namespace {

bool TryMergeTruncateInt64ToInt32IntoLoad(InstructionSelector* selector,
                                          Node* node, Node* load) {
  if (load->opcode() == IrOpcode::kLoad && selector->CanCover(node, load)) {
    LoadRepresentation load_rep = LoadRepresentationOf(load->op());
    MachineRepresentation rep = load_rep.representation();
    InstructionCode opcode = kArchNop;
    switch (rep) {
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = load_rep.IsSigned() ? kX64Movsxbl : kX64Movzxbl;
        break;
      case MachineRepresentation::kWord16:
        opcode = load_rep.IsSigned() ? kX64Movsxwl : kX64Movzxwl;
        break;
      case MachineRepresentation::kWord32:
      case MachineRepresentation::kWord64:
      case MachineRepresentation::kTaggedSigned:
      case MachineRepresentation::kTagged:
      case MachineRepresentation::kCompressed:        // Fall through.
        opcode = kX64Movl;
        break;
      default:
        UNREACHABLE();
        return false;
    }
    X64OperandGenerator g(selector);
    InstructionOperand outputs[] = {g.DefineAsRegister(node)};
    size_t input_count = 0;
    InstructionOperand inputs[3];
    AddressingMode mode = g.GetEffectiveAddressMemoryOperand(
        node->InputAt(0), inputs, &input_count);
    opcode |= AddressingModeField::encode(mode);
    selector->Emit(opcode, 1, outputs, input_count, inputs);
    return true;
  }
  return false;
}

// Shared routine for multiple 32-bit shift operations.
// TODO(bmeurer): Merge this with VisitWord64Shift using template magic?
void VisitWord32Shift(InstructionSelector* selector, Node* node,
                      ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();

  if (left->opcode() == IrOpcode::kTruncateInt64ToInt32 &&
      selector->CanCover(node, left)) {
    left = left->InputAt(0);
  }

  if (g.CanBeImmediate(right)) {
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.UseImmediate(right));
  } else {
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

// Shared routine for multiple shift operations with continuation.
template <typename BinopMatcher, int Bits>
bool TryVisitWordShift(InstructionSelector* selector, Node* node,
                       ArchOpcode opcode, FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();

  // If the shift count is 0, the flags are not affected.
  if (!g.CanBeImmediate(right) ||
      (g.GetImmediateIntegerValue(right) & (Bits - 1)) == 0) {
    return false;
  }
  InstructionOperand output = g.DefineSameAsFirst(node);
  InstructionOperand inputs[2];
  inputs[0] = g.UseRegister(left);
  inputs[1] = g.UseImmediate(right);
  selector->EmitWithContinuation(opcode, 1, &output, 2, inputs, cont);
  return true;
}

void EmitLea(InstructionSelector* selector, InstructionCode opcode,
             Node* result, Node* index, int scale, Node* base,
             Node* displacement, DisplacementMode displacement_mode) {
  X64OperandGenerator g(selector);

  InstructionOperand inputs[4];
  size_t input_count = 0;
  AddressingMode mode =
      g.GenerateMemoryOperandInputs(index, scale, base, displacement,
                                    displacement_mode, inputs, &input_count);

  DCHECK_NE(0u, input_count);
  DCHECK_GE(arraysize(inputs), input_count);

  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(result);

  opcode = AddressingModeField::encode(mode) | opcode;

  selector->Emit(opcode, 1, outputs, input_count, inputs);
}

}  // namespace

void InstructionSelector::VisitWord32Shl(Node* node) {
  Int32ScaleMatcher m(node, true);
  if (m.matches()) {
    Node* index = node->InputAt(0);
    Node* base = m.power_of_two_plus_one() ? index : nullptr;
    EmitLea(this, kX64Lea32, node, index, m.scale(), base, nullptr,
            kPositiveDisplacement);
    return;
  }
  VisitWord32Shift(this, node, kX64Shl32);
}

void InstructionSelector::VisitWord64Shl(Node* node) {
  X64OperandGenerator g(this);
  Int64ScaleMatcher m(node, true);
  if (m.matches()) {
    Node* index = node->InputAt(0);
    Node* base = m.power_of_two_plus_one() ? index : nullptr;
    EmitLea(this, kX64Lea, node, index, m.scale(), base, nullptr,
            kPositiveDisplacement);
    return;
  } else {
    Int64BinopMatcher m(node);
    if ((m.left().IsChangeInt32ToInt64() ||
         m.left().IsChangeUint32ToUint64()) &&
        m.right().IsInRange(32, 63)) {
      // There's no need to sign/zero-extend to 64-bit if we shift out the upper
      // 32 bits anyway.
      Emit(kX64Shl, g.DefineSameAsFirst(node),
           g.UseRegister(m.left().node()->InputAt(0)),
           g.UseImmediate(m.right().node()));
      return;
    }
  }
  VisitWord64Shift(this, node, kX64Shl);
}

void InstructionSelector::VisitWord32Shr(Node* node) {
  VisitWord32Shift(this, node, kX64Shr32);
}

namespace {

inline AddressingMode AddDisplacementToAddressingMode(AddressingMode mode) {
  switch (mode) {
    case kMode_MR:
      return kMode_MRI;
      break;
    case kMode_MR1:
      return kMode_MR1I;
      break;
    case kMode_MR2:
      return kMode_MR2I;
      break;
    case kMode_MR4:
      return kMode_MR4I;
      break;
    case kMode_MR8:
      return kMode_MR8I;
      break;
    case kMode_M1:
      return kMode_M1I;
      break;
    case kMode_M2:
      return kMode_M2I;
      break;
    case kMode_M4:
      return kMode_M4I;
      break;
    case kMode_M8:
      return kMode_M8I;
      break;
    case kMode_None:
    case kMode_MRI:
    case kMode_MR1I:
    case kMode_MR2I:
    case kMode_MR4I:
    case kMode_MR8I:
    case kMode_M1I:
    case kMode_M2I:
    case kMode_M4I:
    case kMode_M8I:
    case kMode_Root:
      UNREACHABLE();
  }
  UNREACHABLE();
}

bool TryMatchLoadWord64AndShiftRight(InstructionSelector* selector, Node* node,
                                     InstructionCode opcode) {
  DCHECK(IrOpcode::kWord64Sar == node->opcode() ||
         IrOpcode::kWord64Shr == node->opcode());
  X64OperandGenerator g(selector);
  Int64BinopMatcher m(node);
  if (selector->CanCover(m.node(), m.left().node()) && m.left().IsLoad() &&
      m.right().Is(32)) {
    DCHECK_EQ(selector->GetEffectLevel(node),
              selector->GetEffectLevel(m.left().node()));
    // Just load and sign-extend the interesting 4 bytes instead. This happens,
    // for example, when we're loading and untagging SMIs.
    BaseWithIndexAndDisplacement64Matcher mleft(m.left().node(),
                                                AddressOption::kAllowAll);
    if (mleft.matches() && (mleft.displacement() == nullptr ||
                            g.CanBeImmediate(mleft.displacement()))) {
      size_t input_count = 0;
      InstructionOperand inputs[3];
      AddressingMode mode = g.GetEffectiveAddressMemoryOperand(
          m.left().node(), inputs, &input_count);
      if (mleft.displacement() == nullptr) {
        // Make sure that the addressing mode indicates the presence of an
        // immediate displacement. It seems that we never use M1 and M2, but we
        // handle them here anyways.
        mode = AddDisplacementToAddressingMode(mode);
        inputs[input_count++] = ImmediateOperand(ImmediateOperand::INLINE, 4);
      } else {
        // In the case that the base address was zero, the displacement will be
        // in a register and replacing it with an immediate is not allowed. This
        // usually only happens in dead code anyway.
        if (!inputs[input_count - 1].IsImmediate()) return false;
        int32_t displacement = g.GetImmediateIntegerValue(mleft.displacement());
        inputs[input_count - 1] =
            ImmediateOperand(ImmediateOperand::INLINE, displacement + 4);
      }
      InstructionOperand outputs[] = {g.DefineAsRegister(node)};
      InstructionCode code = opcode | AddressingModeField::encode(mode);
      selector->Emit(code, 1, outputs, input_count, inputs);
      return true;
    }
  }
  return false;
}

}  // namespace

void InstructionSelector::VisitWord64Shr(Node* node) {
  if (TryMatchLoadWord64AndShiftRight(this, node, kX64Movl)) return;
  VisitWord64Shift(this, node, kX64Shr);
}

void InstructionSelector::VisitWord32Sar(Node* node) {
  X64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (CanCover(m.node(), m.left().node()) && m.left().IsWord32Shl()) {
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().Is(16) && m.right().Is(16)) {
      Emit(kX64Movsxwl, g.DefineAsRegister(node), g.Use(mleft.left().node()));
      return;
    } else if (mleft.right().Is(24) && m.right().Is(24)) {
      Emit(kX64Movsxbl, g.DefineAsRegister(node), g.Use(mleft.left().node()));
      return;
    }
  }
  VisitWord32Shift(this, node, kX64Sar32);
}

void InstructionSelector::VisitWord64Sar(Node* node) {
  if (TryMatchLoadWord64AndShiftRight(this, node, kX64Movsxlq)) return;
  VisitWord64Shift(this, node, kX64Sar);
}

void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitWord32Shift(this, node, kX64Ror32);
}

void InstructionSelector::VisitWord64Ror(Node* node) {
  VisitWord64Shift(this, node, kX64Ror);
}

void InstructionSelector::VisitWord32ReverseBits(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord64ReverseBits(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord64ReverseBytes(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Bswap, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitWord32ReverseBytes(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Bswap32, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitSimd128ReverseBytes(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitInt32Add(Node* node) {
  X64OperandGenerator g(this);

  // Try to match the Add to a leal pattern
  BaseWithIndexAndDisplacement32Matcher m(node);
  if (m.matches() &&
      (m.displacement() == nullptr || g.CanBeImmediate(m.displacement()))) {
    EmitLea(this, kX64Lea32, node, m.index(), m.scale(), m.base(),
            m.displacement(), m.displacement_mode());
    return;
  }

  // No leal pattern match, use addl
  VisitBinop(this, node, kX64Add32);
}

void InstructionSelector::VisitInt64Add(Node* node) {
  X64OperandGenerator g(this);

  // Try to match the Add to a leaq pattern
  BaseWithIndexAndDisplacement64Matcher m(node);
  if (m.matches() &&
      (m.displacement() == nullptr || g.CanBeImmediate(m.displacement()))) {
    EmitLea(this, kX64Lea, node, m.index(), m.scale(), m.base(),
            m.displacement(), m.displacement_mode());
    return;
  }

  // No leal pattern match, use addq
  VisitBinop(this, node, kX64Add);
}

void InstructionSelector::VisitInt64AddWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kX64Add, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kX64Add, &cont);
}

void InstructionSelector::VisitInt32Sub(Node* node) {
  X64OperandGenerator g(this);
  DCHECK_EQ(node->InputCount(), 2);
  Node* input1 = node->InputAt(0);
  Node* input2 = node->InputAt(1);
  if (input1->opcode() == IrOpcode::kTruncateInt64ToInt32 &&
      g.CanBeImmediate(input2)) {
    int32_t imm = g.GetImmediateIntegerValue(input2);
    InstructionOperand int64_input = g.UseRegister(input1->InputAt(0));
    if (imm == 0) {
      // Emit "movl" for subtraction of 0.
      Emit(kX64Movl, g.DefineAsRegister(node), int64_input);
    } else {
      // Omit truncation and turn subtractions of constant values into immediate
      // "leal" instructions by negating the value.
      Emit(kX64Lea32 | AddressingModeField::encode(kMode_MRI),
           g.DefineAsRegister(node), int64_input,
           g.TempImmediate(base::NegateWithWraparound(imm)));
    }
    return;
  }

  Int32BinopMatcher m(node);
  if (m.left().Is(0)) {
    Emit(kX64Neg32, g.DefineSameAsFirst(node), g.UseRegister(m.right().node()));
  } else if (m.right().Is(0)) {
    // {EmitIdentity} reuses the virtual register of the first input
    // for the output. This is exactly what we want here.
    EmitIdentity(node);
  } else if (m.right().HasValue() && g.CanBeImmediate(m.right().node())) {
    // Turn subtractions of constant values into immediate "leal" instructions
    // by negating the value.
    Emit(kX64Lea32 | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(m.left().node()),
         g.TempImmediate(base::NegateWithWraparound(m.right().Value())));
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
    if (m.right().HasValue() && g.CanBeImmediate(m.right().node())) {
      // Turn subtractions of constant values into immediate "leaq" instructions
      // by negating the value.
      Emit(kX64Lea | AddressingModeField::encode(kMode_MRI),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(-static_cast<int32_t>(m.right().Value())));
      return;
    }
    VisitBinop(this, node, kX64Sub);
  }
}

void InstructionSelector::VisitInt64SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kX64Sub, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kX64Sub, &cont);
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
  InstructionOperand temps[] = {g.TempRegister(rax)};
  // TODO(turbofan): We use UseUniqueRegister here to improve register
  // allocation.
  selector->Emit(opcode, g.DefineAsFixed(node, rdx), g.UseFixed(left, rax),
                 g.UseUniqueRegister(right), arraysize(temps), temps);
}

void VisitDiv(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  InstructionOperand temps[] = {g.TempRegister(rdx)};
  selector->Emit(
      opcode, g.DefineAsFixed(node, rax), g.UseFixed(node->InputAt(0), rax),
      g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps);
}

void VisitMod(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  InstructionOperand temps[] = {g.TempRegister(rax)};
  selector->Emit(
      opcode, g.DefineAsFixed(node, rdx), g.UseFixed(node->InputAt(0), rax),
      g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps);
}

}  // namespace

void InstructionSelector::VisitInt32Mul(Node* node) {
  Int32ScaleMatcher m(node, true);
  if (m.matches()) {
    Node* index = node->InputAt(0);
    Node* base = m.power_of_two_plus_one() ? index : nullptr;
    EmitLea(this, kX64Lea32, node, index, m.scale(), base, nullptr,
            kPositiveDisplacement);
    return;
  }
  VisitMul(this, node, kX64Imul32);
}

void InstructionSelector::VisitInt32MulWithOverflow(Node* node) {
  // TODO(mvstanton): Use Int32ScaleMatcher somehow.
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kX64Imul32, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kX64Imul32, &cont);
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

void InstructionSelector::VisitTryTruncateFloat32ToInt64(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kSSEFloat32ToInt64, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitTryTruncateFloat64ToInt64(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kSSEFloat64ToInt64, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitTryTruncateFloat32ToUint64(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kSSEFloat32ToUint64, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitTryTruncateFloat64ToUint64(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kSSEFloat64ToUint64, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitBitcastWord32ToWord64(Node* node) {
  DCHECK(SmiValuesAre31Bits());
  DCHECK(COMPRESS_POINTERS_BOOL);
  EmitIdentity(node);
}

void InstructionSelector::VisitChangeInt32ToInt64(Node* node) {
  X64OperandGenerator g(this);
  Node* const value = node->InputAt(0);
  if (value->opcode() == IrOpcode::kLoad && CanCover(node, value)) {
    LoadRepresentation load_rep = LoadRepresentationOf(value->op());
    MachineRepresentation rep = load_rep.representation();
    InstructionCode opcode = kArchNop;
    switch (rep) {
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = load_rep.IsSigned() ? kX64Movsxbq : kX64Movzxbq;
        break;
      case MachineRepresentation::kWord16:
        opcode = load_rep.IsSigned() ? kX64Movsxwq : kX64Movzxwq;
        break;
      case MachineRepresentation::kWord32:
        opcode = load_rep.IsSigned() ? kX64Movsxlq : kX64Movl;
        break;
      default:
        UNREACHABLE();
        return;
    }
    InstructionOperand outputs[] = {g.DefineAsRegister(node)};
    size_t input_count = 0;
    InstructionOperand inputs[3];
    AddressingMode mode = g.GetEffectiveAddressMemoryOperand(
        node->InputAt(0), inputs, &input_count);
    opcode |= AddressingModeField::encode(mode);
    Emit(opcode, 1, outputs, input_count, inputs);
  } else {
    Emit(kX64Movsxlq, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
  }
}

namespace {

bool ZeroExtendsWord32ToWord64(Node* node) {
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
    case IrOpcode::kUint32MulHigh:
    case IrOpcode::kTruncateInt64ToInt32:
      // These 32-bit operations implicitly zero-extend to 64-bit on x64, so the
      // zero-extension is a no-op.
      return true;
    case IrOpcode::kProjection: {
      Node* const value = node->InputAt(0);
      switch (value->opcode()) {
        case IrOpcode::kInt32AddWithOverflow:
        case IrOpcode::kInt32SubWithOverflow:
        case IrOpcode::kInt32MulWithOverflow:
          return true;
        default:
          return false;
      }
    }
    case IrOpcode::kLoad:
    case IrOpcode::kProtectedLoad:
    case IrOpcode::kPoisonedLoad: {
      // The movzxbl/movsxbl/movzxwl/movsxwl/movl operations implicitly
      // zero-extend to 64-bit on x64, so the zero-extension is a no-op.
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

}  // namespace

void InstructionSelector::VisitChangeUint32ToUint64(Node* node) {
  X64OperandGenerator g(this);
  Node* value = node->InputAt(0);
  if (ZeroExtendsWord32ToWord64(value)) {
    // These 32-bit operations implicitly zero-extend to 64-bit on x64, so the
    // zero-extension is a no-op.
    return EmitIdentity(node);
  }
  Emit(kX64Movl, g.DefineAsRegister(node), g.Use(value));
}

void InstructionSelector::VisitChangeTaggedToCompressed(Node* node) {
  // The top 32 bits in the 64-bit register will be undefined, and
  // must not be used by a dependent node.
  return EmitIdentity(node);
}

namespace {

void VisitRO(InstructionSelector* selector, Node* node,
             InstructionCode opcode) {
  X64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}

void VisitRR(InstructionSelector* selector, Node* node,
             InstructionCode opcode) {
  X64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}

void VisitRRO(InstructionSelector* selector, Node* node,
              InstructionCode opcode) {
  X64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineSameAsFirst(node),
                 g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(1)));
}

void VisitFloatBinop(InstructionSelector* selector, Node* node,
                     ArchOpcode avx_opcode, ArchOpcode sse_opcode) {
  X64OperandGenerator g(selector);
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
  X64OperandGenerator g(selector);
  InstructionOperand temps[] = {g.TempDoubleRegister()};
  if (selector->IsSupported(AVX)) {
    selector->Emit(avx_opcode, g.DefineAsRegister(node), g.UseUnique(input),
                   arraysize(temps), temps);
  } else {
    selector->Emit(sse_opcode, g.DefineSameAsFirst(node), g.UseRegister(input),
                   arraysize(temps), temps);
  }
}

}  // namespace

#define RO_OP_LIST(V)                                                    \
  V(Word64Clz, kX64Lzcnt)                                                \
  V(Word32Clz, kX64Lzcnt32)                                              \
  V(Word64Ctz, kX64Tzcnt)                                                \
  V(Word32Ctz, kX64Tzcnt32)                                              \
  V(Word64Popcnt, kX64Popcnt)                                            \
  V(Word32Popcnt, kX64Popcnt32)                                          \
  V(Float64Sqrt, kSSEFloat64Sqrt)                                        \
  V(Float32Sqrt, kSSEFloat32Sqrt)                                        \
  V(ChangeFloat64ToInt32, kSSEFloat64ToInt32)                            \
  V(ChangeFloat64ToInt64, kSSEFloat64ToInt64)                            \
  V(ChangeFloat64ToUint32, kSSEFloat64ToUint32 | MiscField::encode(1))   \
  V(TruncateFloat64ToInt64, kSSEFloat64ToInt64)                          \
  V(TruncateFloat64ToUint32, kSSEFloat64ToUint32 | MiscField::encode(0)) \
  V(ChangeFloat64ToUint64, kSSEFloat64ToUint64)                          \
  V(TruncateFloat64ToFloat32, kSSEFloat64ToFloat32)                      \
  V(ChangeFloat32ToFloat64, kSSEFloat32ToFloat64)                        \
  V(TruncateFloat32ToInt32, kSSEFloat32ToInt32)                          \
  V(TruncateFloat32ToUint32, kSSEFloat32ToUint32)                        \
  V(ChangeInt32ToFloat64, kSSEInt32ToFloat64)                            \
  V(ChangeInt64ToFloat64, kSSEInt64ToFloat64)                            \
  V(ChangeUint32ToFloat64, kSSEUint32ToFloat64)                          \
  V(RoundFloat64ToInt32, kSSEFloat64ToInt32)                             \
  V(RoundInt32ToFloat32, kSSEInt32ToFloat32)                             \
  V(RoundInt64ToFloat32, kSSEInt64ToFloat32)                             \
  V(RoundUint64ToFloat32, kSSEUint64ToFloat32)                           \
  V(RoundInt64ToFloat64, kSSEInt64ToFloat64)                             \
  V(RoundUint64ToFloat64, kSSEUint64ToFloat64)                           \
  V(RoundUint32ToFloat32, kSSEUint32ToFloat32)                           \
  V(BitcastFloat32ToInt32, kX64BitcastFI)                                \
  V(BitcastFloat64ToInt64, kX64BitcastDL)                                \
  V(BitcastInt32ToFloat32, kX64BitcastIF)                                \
  V(BitcastInt64ToFloat64, kX64BitcastLD)                                \
  V(Float64ExtractLowWord32, kSSEFloat64ExtractLowWord32)                \
  V(Float64ExtractHighWord32, kSSEFloat64ExtractHighWord32)              \
  V(SignExtendWord8ToInt32, kX64Movsxbl)                                 \
  V(SignExtendWord16ToInt32, kX64Movsxwl)                                \
  V(SignExtendWord8ToInt64, kX64Movsxbq)                                 \
  V(SignExtendWord16ToInt64, kX64Movsxwq)                                \
  V(SignExtendWord32ToInt64, kX64Movsxlq)

#define RR_OP_LIST(V)                                                         \
  V(Float32RoundDown, kSSEFloat32Round | MiscField::encode(kRoundDown))       \
  V(Float64RoundDown, kSSEFloat64Round | MiscField::encode(kRoundDown))       \
  V(Float32RoundUp, kSSEFloat32Round | MiscField::encode(kRoundUp))           \
  V(Float64RoundUp, kSSEFloat64Round | MiscField::encode(kRoundUp))           \
  V(Float32RoundTruncate, kSSEFloat32Round | MiscField::encode(kRoundToZero)) \
  V(Float64RoundTruncate, kSSEFloat64Round | MiscField::encode(kRoundToZero)) \
  V(Float32RoundTiesEven,                                                     \
    kSSEFloat32Round | MiscField::encode(kRoundToNearest))                    \
  V(Float64RoundTiesEven, kSSEFloat64Round | MiscField::encode(kRoundToNearest))

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

void InstructionSelector::VisitTruncateFloat64ToWord32(Node* node) {
  VisitRR(this, node, kArchTruncateDoubleToI);
}

void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  // We rely on the fact that TruncateInt64ToInt32 zero extends the
  // value (see ZeroExtendsWord32ToWord64). So all code paths here
  // have to satisfy that condition.
  X64OperandGenerator g(this);
  Node* value = node->InputAt(0);
  if (CanCover(node, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord64Sar:
      case IrOpcode::kWord64Shr: {
        Int64BinopMatcher m(value);
        if (m.right().Is(32)) {
          if (CanCoverTransitively(node, value, value->InputAt(0)) &&
              TryMatchLoadWord64AndShiftRight(this, value, kX64Movl)) {
            return EmitIdentity(node);
          }
          Emit(kX64Shr, g.DefineSameAsFirst(node),
               g.UseRegister(m.left().node()), g.TempImmediate(32));
          return;
        }
        break;
      }
      case IrOpcode::kLoad: {
        if (TryMergeTruncateInt64ToInt32IntoLoad(this, node, value)) {
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

void InstructionSelector::VisitFloat32Add(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat32Add, kSSEFloat32Add);
}

void InstructionSelector::VisitFloat32Sub(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat32Sub, kSSEFloat32Sub);
}

void InstructionSelector::VisitFloat32Mul(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat32Mul, kSSEFloat32Mul);
}

void InstructionSelector::VisitFloat32Div(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat32Div, kSSEFloat32Div);
}

void InstructionSelector::VisitFloat32Abs(Node* node) {
  VisitFloatUnop(this, node, node->InputAt(0), kAVXFloat32Abs, kSSEFloat32Abs);
}

void InstructionSelector::VisitFloat32Max(Node* node) {
  VisitRRO(this, node, kSSEFloat32Max);
}

void InstructionSelector::VisitFloat32Min(Node* node) {
  VisitRRO(this, node, kSSEFloat32Min);
}

void InstructionSelector::VisitFloat64Add(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat64Add, kSSEFloat64Add);
}

void InstructionSelector::VisitFloat64Sub(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat64Sub, kSSEFloat64Sub);
}

void InstructionSelector::VisitFloat64Mul(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat64Mul, kSSEFloat64Mul);
}

void InstructionSelector::VisitFloat64Div(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat64Div, kSSEFloat64Div);
}

void InstructionSelector::VisitFloat64Mod(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister(rax)};
  Emit(kSSEFloat64Mod, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)), 1,
       temps);
}

void InstructionSelector::VisitFloat64Max(Node* node) {
  VisitRRO(this, node, kSSEFloat64Max);
}

void InstructionSelector::VisitFloat64Min(Node* node) {
  VisitRRO(this, node, kSSEFloat64Min);
}

void InstructionSelector::VisitFloat64Abs(Node* node) {
  VisitFloatUnop(this, node, node->InputAt(0), kAVXFloat64Abs, kSSEFloat64Abs);
}

void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitFloat32Neg(Node* node) {
  VisitFloatUnop(this, node, node->InputAt(0), kAVXFloat32Neg, kSSEFloat32Neg);
}

void InstructionSelector::VisitFloat64Neg(Node* node) {
  VisitFloatUnop(this, node, node->InputAt(0), kAVXFloat64Neg, kSSEFloat64Neg);
}

void InstructionSelector::VisitFloat64Ieee754Binop(Node* node,
                                                   InstructionCode opcode) {
  X64OperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, xmm0), g.UseFixed(node->InputAt(0), xmm0),
       g.UseFixed(node->InputAt(1), xmm1))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat64Ieee754Unop(Node* node,
                                                  InstructionCode opcode) {
  X64OperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, xmm0), g.UseFixed(node->InputAt(0), xmm0))
      ->MarkAsCall();
}

void InstructionSelector::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* call_descriptor,
    Node* node) {
  X64OperandGenerator g(this);

  // Prepare for C function call.
  if (call_descriptor->IsCFunctionCall()) {
    Emit(kArchPrepareCallCFunction | MiscField::encode(static_cast<int>(
                                         call_descriptor->ParameterCount())),
         0, nullptr, 0, nullptr);

    // Poke any stack arguments.
    for (size_t n = 0; n < arguments->size(); ++n) {
      PushParameter input = (*arguments)[n];
      if (input.node) {
        int slot = static_cast<int>(n);
        InstructionOperand value = g.CanBeImmediate(input.node)
                                       ? g.UseImmediate(input.node)
                                       : g.UseRegister(input.node);
        Emit(kX64Poke | MiscField::encode(slot), g.NoOutput(), value);
      }
    }
  } else {
    // Push any stack arguments.
    int effect_level = GetEffectLevel(node);
    for (PushParameter input : base::Reversed(*arguments)) {
      // Skip any alignment holes in pushed nodes. We may have one in case of a
      // Simd128 stack argument.
      if (input.node == nullptr) continue;
      if (g.CanBeImmediate(input.node)) {
        Emit(kX64Push, g.NoOutput(), g.UseImmediate(input.node));
      } else if (IsSupported(ATOM) ||
                 sequence()->IsFP(GetVirtualRegister(input.node))) {
        // TODO(titzer): X64Push cannot handle stack->stack double moves
        // because there is no way to encode fixed double slots.
        Emit(kX64Push, g.NoOutput(), g.UseRegister(input.node));
      } else if (g.CanBeMemoryOperand(kX64Push, node, input.node,
                                      effect_level)) {
        InstructionOperand outputs[1];
        InstructionOperand inputs[4];
        size_t input_count = 0;
        InstructionCode opcode = kX64Push;
        AddressingMode mode = g.GetEffectiveAddressMemoryOperand(
            input.node, inputs, &input_count);
        opcode |= AddressingModeField::encode(mode);
        Emit(opcode, 0, outputs, input_count, inputs);
      } else {
        Emit(kX64Push, g.NoOutput(), g.UseAny(input.node));
      }
    }
  }
}

void InstructionSelector::EmitPrepareResults(
    ZoneVector<PushParameter>* results, const CallDescriptor* call_descriptor,
    Node* node) {
  X64OperandGenerator g(this);

  int reverse_slot = 0;
  for (PushParameter output : *results) {
    if (!output.location.IsCallerFrameSlot()) continue;
    reverse_slot += output.location.GetSizeInPointers();
    // Skip any alignment holes in nodes.
    if (output.node == nullptr) continue;
    DCHECK(!call_descriptor->IsCFunctionCall());
    if (output.location.GetType() == MachineType::Float32()) {
      MarkAsFloat32(output.node);
    } else if (output.location.GetType() == MachineType::Float64()) {
      MarkAsFloat64(output.node);
    }
    InstructionOperand result = g.DefineAsRegister(output.node);
    InstructionOperand slot = g.UseImmediate(reverse_slot);
    Emit(kX64Peek, 1, &result, 1, &slot);
  }
}

bool InstructionSelector::IsTailCallAddressImmediate() { return true; }

int InstructionSelector::GetTempsCountForTailCallFromJSFunction() { return 3; }

namespace {

void VisitCompareWithMemoryOperand(InstructionSelector* selector,
                                   InstructionCode opcode, Node* left,
                                   InstructionOperand right,
                                   FlagsContinuation* cont) {
  DCHECK_EQ(IrOpcode::kLoad, left->opcode());
  X64OperandGenerator g(selector);
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
  X64OperandGenerator g(selector);
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
  // TODO(epertoso): we can probably get some size information out phi nodes.
  // If the load representations don't match, both operands will be
  // zero/sign-extended to 32bit.
  MachineType left_type = MachineTypeForNarrow(left, right);
  MachineType right_type = MachineTypeForNarrow(right, left);
  if (left_type == right_type) {
    switch (left_type.representation()) {
      case MachineRepresentation::kBit:
      case MachineRepresentation::kWord8: {
        if (opcode == kX64Test32) return kX64Test8;
        if (opcode == kX64Cmp32) {
          if (left_type.semantic() == MachineSemantic::kUint32) {
            cont->OverwriteUnsignedIfSigned();
          } else {
            CHECK_EQ(MachineSemantic::kInt32, left_type.semantic());
          }
          return kX64Cmp8;
        }
        break;
      }
      case MachineRepresentation::kWord16:
        if (opcode == kX64Test32) return kX64Test16;
        if (opcode == kX64Cmp32) {
          if (left_type.semantic() == MachineSemantic::kUint32) {
            cont->OverwriteUnsignedIfSigned();
          } else {
            CHECK_EQ(MachineSemantic::kInt32, left_type.semantic());
          }
          return kX64Cmp16;
        }
        break;
#ifdef V8_COMPRESS_POINTERS
      case MachineRepresentation::kTaggedSigned:
      case MachineRepresentation::kTaggedPointer:
      case MachineRepresentation::kTagged:
        // When pointer compression is enabled the lower 32-bits uniquely
        // identify tagged value.
        if (opcode == kX64Cmp) return kX64Cmp32;
        break;
#endif
      default:
        break;
    }
  }
  return opcode;
}

// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  // The 32-bit comparisons automatically truncate Word64
  // values to Word32 range, no need to do that explicitly.
  if (opcode == kX64Cmp32 || opcode == kX64Test32) {
    if (left->opcode() == IrOpcode::kTruncateInt64ToInt32 &&
        selector->CanCover(node, left)) {
      left = left->InputAt(0);
    }

    if (right->opcode() == IrOpcode::kTruncateInt64ToInt32 &&
        selector->CanCover(node, right)) {
      right = right->InputAt(0);
    }
  }

  opcode = TryNarrowOpcodeSize(opcode, left, right, cont);

  // If one of the two inputs is an immediate, make sure it's on the right, or
  // if one of the two inputs is a memory operand, make sure it's on the left.
  int effect_level = selector->GetEffectLevel(node);
  if (cont->IsBranch()) {
    effect_level = selector->GetEffectLevel(
        cont->true_block()->PredecessorAt(0)->control_input());
  }

  if ((!g.CanBeImmediate(right) && g.CanBeImmediate(left)) ||
      (g.CanBeMemoryOperand(opcode, node, right, effect_level) &&
       !g.CanBeMemoryOperand(opcode, node, left, effect_level))) {
    if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
    std::swap(left, right);
  }

  // Match immediates on right side of comparison.
  if (g.CanBeImmediate(right)) {
    if (g.CanBeMemoryOperand(opcode, node, left, effect_level)) {
      return VisitCompareWithMemoryOperand(selector, opcode, left,
                                           g.UseImmediate(right), cont);
    }
    return VisitCompare(selector, opcode, g.Use(left), g.UseImmediate(right),
                        cont);
  }

  // Match memory operands on left side of comparison.
  if (g.CanBeMemoryOperand(opcode, node, left, effect_level)) {
    return VisitCompareWithMemoryOperand(selector, opcode, left,
                                         g.UseRegister(right), cont);
  }

  return VisitCompare(selector, opcode, left, right, cont,
                      node->op()->HasProperty(Operator::kCommutative));
}

void VisitWord64EqualImpl(InstructionSelector* selector, Node* node,
                          FlagsContinuation* cont) {
  if (selector->CanUseRootsRegister()) {
    X64OperandGenerator g(selector);
    const RootsTable& roots_table = selector->isolate()->roots_table();
    RootIndex root_index;
    HeapObjectBinopMatcher m(node);
    if (m.right().HasValue() &&
        roots_table.IsRootHandle(m.right().Value(), &root_index)) {
      InstructionCode opcode =
          kX64Cmp | AddressingModeField::encode(kMode_Root);
      return VisitCompare(
          selector, opcode,
          g.TempImmediate(
              TurboAssemblerBase::RootRegisterOffsetForRootIndex(root_index)),
          g.UseRegister(m.left().node()), cont);
    }
  }
  VisitWordCompare(selector, node, kX64Cmp, cont);
}

void VisitWord32EqualImpl(InstructionSelector* selector, Node* node,
                          FlagsContinuation* cont) {
  if (COMPRESS_POINTERS_BOOL && selector->CanUseRootsRegister()) {
    X64OperandGenerator g(selector);
    const RootsTable& roots_table = selector->isolate()->roots_table();
    RootIndex root_index;
    CompressedHeapObjectBinopMatcher m(node);
    if (m.right().HasValue() &&
        roots_table.IsRootHandle(m.right().Value(), &root_index)) {
      InstructionCode opcode =
          kX64Cmp32 | AddressingModeField::encode(kMode_Root);
      return VisitCompare(
          selector, opcode,
          g.TempImmediate(
              TurboAssemblerBase::RootRegisterOffsetForRootIndex(root_index)),
          g.UseRegister(m.left().node()), cont);
    }
  }
  VisitWordCompare(selector, node, kX64Cmp32, cont);
}

// Shared routine for comparison with zero.
void VisitCompareZero(InstructionSelector* selector, Node* user, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  if (cont->IsBranch() &&
      (cont->condition() == kNotEqual || cont->condition() == kEqual)) {
    switch (node->opcode()) {
#define FLAGS_SET_BINOP_LIST(V)        \
  V(kInt32Add, VisitBinop, kX64Add32)  \
  V(kInt32Sub, VisitBinop, kX64Sub32)  \
  V(kWord32And, VisitBinop, kX64And32) \
  V(kWord32Or, VisitBinop, kX64Or32)   \
  V(kInt64Add, VisitBinop, kX64Add)    \
  V(kInt64Sub, VisitBinop, kX64Sub)    \
  V(kWord64And, VisitBinop, kX64And)   \
  V(kWord64Or, VisitBinop, kX64Or)
#define FLAGS_SET_BINOP(opcode, Visit, archOpcode)           \
  case IrOpcode::opcode:                                     \
    if (selector->IsOnlyUserOfNodeInSameBlock(user, node)) { \
      return Visit(selector, node, archOpcode, cont);        \
    }                                                        \
    break;
      FLAGS_SET_BINOP_LIST(FLAGS_SET_BINOP)
#undef FLAGS_SET_BINOP_LIST
#undef FLAGS_SET_BINOP

#define TRY_VISIT_WORD32_SHIFT TryVisitWordShift<Int32BinopMatcher, 32>
#define TRY_VISIT_WORD64_SHIFT TryVisitWordShift<Int64BinopMatcher, 64>
// Skip Word64Sar/Word32Sar since no instruction reduction in most cases.
#define FLAGS_SET_SHIFT_LIST(V)                    \
  V(kWord32Shl, TRY_VISIT_WORD32_SHIFT, kX64Shl32) \
  V(kWord32Shr, TRY_VISIT_WORD32_SHIFT, kX64Shr32) \
  V(kWord64Shl, TRY_VISIT_WORD64_SHIFT, kX64Shl)   \
  V(kWord64Shr, TRY_VISIT_WORD64_SHIFT, kX64Shr)
#define FLAGS_SET_SHIFT(opcode, TryVisit, archOpcode)         \
  case IrOpcode::opcode:                                      \
    if (selector->IsOnlyUserOfNodeInSameBlock(user, node)) {  \
      if (TryVisit(selector, node, archOpcode, cont)) return; \
    }                                                         \
    break;
      FLAGS_SET_SHIFT_LIST(FLAGS_SET_SHIFT)
#undef TRY_VISIT_WORD32_SHIFT
#undef TRY_VISIT_WORD64_SHIFT
#undef FLAGS_SET_SHIFT_LIST
#undef FLAGS_SET_SHIFT
      default:
        break;
    }
  }
  int effect_level = selector->GetEffectLevel(node);
  if (cont->IsBranch()) {
    effect_level = selector->GetEffectLevel(
        cont->true_block()->PredecessorAt(0)->control_input());
  }
  if (node->opcode() == IrOpcode::kLoad) {
    switch (LoadRepresentationOf(node->op()).representation()) {
      case MachineRepresentation::kWord8:
        if (opcode == kX64Cmp32) {
          opcode = kX64Cmp8;
        } else if (opcode == kX64Test32) {
          opcode = kX64Test8;
        }
        break;
      case MachineRepresentation::kWord16:
        if (opcode == kX64Cmp32) {
          opcode = kX64Cmp16;
        } else if (opcode == kX64Test32) {
          opcode = kX64Test16;
        }
        break;
      default:
        break;
    }
  }
  if (g.CanBeMemoryOperand(opcode, user, node, effect_level)) {
    VisitCompareWithMemoryOperand(selector, opcode, node, g.TempImmediate(0),
                                  cont);
  } else {
    VisitCompare(selector, opcode, g.Use(node), g.TempImmediate(0), cont);
  }
}

// Shared routine for multiple float32 compare operations (inputs commuted).
void VisitFloat32Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  Node* const left = node->InputAt(0);
  Node* const right = node->InputAt(1);
  InstructionCode const opcode =
      selector->IsSupported(AVX) ? kAVXFloat32Cmp : kSSEFloat32Cmp;
  VisitCompare(selector, opcode, right, left, cont, false);
}

// Shared routine for multiple float64 compare operations (inputs commuted).
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  Node* const left = node->InputAt(0);
  Node* const right = node->InputAt(1);
  InstructionCode const opcode =
      selector->IsSupported(AVX) ? kAVXFloat64Cmp : kSSEFloat64Cmp;
  VisitCompare(selector, opcode, right, left, cont, false);
}

// Shared routine for Word32/Word64 Atomic Binops
void VisitAtomicBinop(InstructionSelector* selector, Node* node,
                      ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  AddressingMode addressing_mode;
  InstructionOperand inputs[] = {
      g.UseUniqueRegister(value), g.UseUniqueRegister(base),
      g.GetEffectiveIndexOperand(index, &addressing_mode)};
  InstructionOperand outputs[] = {g.DefineAsFixed(node, rax)};
  InstructionOperand temps[] = {g.TempRegister()};
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  selector->Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
                 arraysize(temps), temps);
}

// Shared routine for Word32/Word64 Atomic CmpExchg
void VisitAtomicCompareExchange(InstructionSelector* selector, Node* node,
                                ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* old_value = node->InputAt(2);
  Node* new_value = node->InputAt(3);
  AddressingMode addressing_mode;
  InstructionOperand inputs[] = {
      g.UseFixed(old_value, rax), g.UseUniqueRegister(new_value),
      g.UseUniqueRegister(base),
      g.GetEffectiveIndexOperand(index, &addressing_mode)};
  InstructionOperand outputs[] = {g.DefineAsFixed(node, rax)};
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  selector->Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs);
}

// Shared routine for Word32/Word64 Atomic Exchange
void VisitAtomicExchange(InstructionSelector* selector, Node* node,
                         ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  AddressingMode addressing_mode;
  InstructionOperand inputs[] = {
      g.UseUniqueRegister(value), g.UseUniqueRegister(base),
      g.GetEffectiveIndexOperand(index, &addressing_mode)};
  InstructionOperand outputs[] = {g.DefineSameAsFirst(node)};
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  selector->Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs);
}

}  // namespace

// Shared routine for word comparison against zero.
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
        return VisitWord32EqualImpl(this, value, cont);
      case IrOpcode::kInt32LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWordCompare(this, value, kX64Cmp32, cont);
      case IrOpcode::kInt32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWordCompare(this, value, kX64Cmp32, cont);
      case IrOpcode::kUint32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWordCompare(this, value, kX64Cmp32, cont);
      case IrOpcode::kUint32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWordCompare(this, value, kX64Cmp32, cont);
      case IrOpcode::kWord64Equal: {
        cont->OverwriteAndNegateIfEqual(kEqual);
        Int64BinopMatcher m(value);
        if (m.right().Is(0)) {
          // Try to combine the branch with a comparison.
          Node* const user = m.node();
          Node* const value = m.left().node();
          if (CanCover(user, value)) {
            switch (value->opcode()) {
              case IrOpcode::kInt64Sub:
                return VisitWordCompare(this, value, kX64Cmp, cont);
              case IrOpcode::kWord64And:
                return VisitWordCompare(this, value, kX64Test, cont);
              default:
                break;
            }
          }
          return VisitCompareZero(this, user, value, kX64Cmp, cont);
        }
        return VisitWord64EqualImpl(this, value, cont);
      }
      case IrOpcode::kInt64LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWordCompare(this, value, kX64Cmp, cont);
      case IrOpcode::kInt64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWordCompare(this, value, kX64Cmp, cont);
      case IrOpcode::kUint64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWordCompare(this, value, kX64Cmp, cont);
      case IrOpcode::kUint64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWordCompare(this, value, kX64Cmp, cont);
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
      case IrOpcode::kFloat64LessThan: {
        Float64BinopMatcher m(value);
        if (m.left().Is(0.0) && m.right().IsFloat64Abs()) {
          // This matches the pattern
          //
          //   Float64LessThan(#0.0, Float64Abs(x))
          //
          // which TurboFan generates for NumberToBoolean in the general case,
          // and which evaluates to false if x is 0, -0 or NaN. We can compile
          // this to a simple (v)ucomisd using not_equal flags condition, which
          // avoids the costly Float64Abs.
          cont->OverwriteAndNegateIfEqual(kNotEqual);
          InstructionCode const opcode =
              IsSupported(AVX) ? kAVXFloat64Cmp : kSSEFloat64Cmp;
          return VisitCompare(this, opcode, m.left().node(),
                              m.right().InputAt(0), cont, false);
        }
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThan);
        return VisitFloat64Compare(this, value, cont);
      }
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
                return VisitBinop(this, node, kX64Add32, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kX64Sub32, cont);
              case IrOpcode::kInt32MulWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kX64Imul32, cont);
              case IrOpcode::kInt64AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kX64Add, cont);
              case IrOpcode::kInt64SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kX64Sub, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Sub:
        return VisitWordCompare(this, value, kX64Cmp32, cont);
      case IrOpcode::kWord32And:
        return VisitWordCompare(this, value, kX64Test32, cont);
      case IrOpcode::kStackPointerGreaterThan:
        cont->OverwriteAndNegateIfEqual(kStackPointerGreaterThanCondition);
        return VisitStackPointerGreaterThan(value, cont);
      default:
        break;
    }
  }

  // Branch could not be combined with a compare, emit compare against 0.
  VisitCompareZero(this, user, value, kX64Cmp32, cont);
}

void InstructionSelector::VisitSwitch(Node* node, const SwitchInfo& sw) {
  X64OperandGenerator g(this);
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
      InstructionOperand index_operand = g.TempRegister();
      if (sw.min_value()) {
        // The leal automatically zero extends, so result is a valid 64-bit
        // index.
        Emit(kX64Lea32 | AddressingModeField::encode(kMode_MRI), index_operand,
             value_operand, g.TempImmediate(-sw.min_value()));
      } else {
        // Zero extend, because we use it as 64-bit index into the jump table.
        Emit(kX64Movl, index_operand, value_operand);
      }
      // Generate a table lookup.
      return EmitTableSwitch(sw, index_operand);
    }
  }

  // Generate a tree of conditional jumps.
  return EmitBinarySearchSwitch(sw, value_operand);
}

void InstructionSelector::VisitWord32Equal(Node* const node) {
  Node* user = node;
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int32BinopMatcher m(user);
  if (m.right().Is(0)) {
    return VisitWordCompareZero(m.node(), m.left().node(), &cont);
  }
  VisitWord32EqualImpl(this, node, &cont);
}

void InstructionSelector::VisitInt32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}

void InstructionSelector::VisitInt32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}

void InstructionSelector::VisitUint32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}

void InstructionSelector::VisitUint32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}

void InstructionSelector::VisitWord64Equal(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int64BinopMatcher m(node);
  if (m.right().Is(0)) {
    // Try to combine the equality check with a comparison.
    Node* const user = m.node();
    Node* const value = m.left().node();
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
  }
  VisitWord64EqualImpl(this, node, &cont);
}

void InstructionSelector::VisitInt32AddWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kX64Add32, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kX64Add32, &cont);
}

void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kX64Sub32, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kX64Sub32, &cont);
}

void InstructionSelector::VisitInt64LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWordCompare(this, node, kX64Cmp, &cont);
}

void InstructionSelector::VisitInt64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kX64Cmp, &cont);
}

void InstructionSelector::VisitUint64LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWordCompare(this, node, kX64Cmp, &cont);
}

void InstructionSelector::VisitUint64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kX64Cmp, &cont);
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
  Float64BinopMatcher m(node);
  if (m.left().Is(0.0) && m.right().IsFloat64Abs()) {
    // This matches the pattern
    //
    //   Float64LessThan(#0.0, Float64Abs(x))
    //
    // which TurboFan generates for NumberToBoolean in the general case,
    // and which evaluates to false if x is 0, -0 or NaN. We can compile
    // this to a simple (v)ucomisd using not_equal flags condition, which
    // avoids the costly Float64Abs.
    FlagsContinuation cont = FlagsContinuation::ForSet(kNotEqual, node);
    InstructionCode const opcode =
        IsSupported(AVX) ? kAVXFloat64Cmp : kSSEFloat64Cmp;
    return VisitCompare(this, opcode, m.left().node(), m.right().InputAt(0),
                        &cont, false);
  }
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
  X64OperandGenerator g(this);
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
  X64OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Emit(kSSEFloat64InsertHighWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.Use(right));
}

void InstructionSelector::VisitFloat64SilenceNaN(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64SilenceNaN, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitMemoryBarrier(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64MFence, g.NoOutput());
}

void InstructionSelector::VisitWord32AtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  DCHECK(load_rep.representation() == MachineRepresentation::kWord8 ||
         load_rep.representation() == MachineRepresentation::kWord16 ||
         load_rep.representation() == MachineRepresentation::kWord32);
  USE(load_rep);
  VisitLoad(node);
}

void InstructionSelector::VisitWord64AtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  USE(load_rep);
  VisitLoad(node);
}

void InstructionSelector::VisitWord32AtomicStore(Node* node) {
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
  VisitAtomicExchange(this, node, opcode);
}

void InstructionSelector::VisitWord64AtomicStore(Node* node) {
  MachineRepresentation rep = AtomicStoreRepresentationOf(node->op());
  ArchOpcode opcode = kArchNop;
  switch (rep) {
    case MachineRepresentation::kWord8:
      opcode = kX64Word64AtomicExchangeUint8;
      break;
    case MachineRepresentation::kWord16:
      opcode = kX64Word64AtomicExchangeUint16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kX64Word64AtomicExchangeUint32;
      break;
    case MachineRepresentation::kWord64:
      opcode = kX64Word64AtomicExchangeUint64;
      break;
    default:
      UNREACHABLE();
  }
  VisitAtomicExchange(this, node, opcode);
}

void InstructionSelector::VisitWord32AtomicExchange(Node* node) {
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
  VisitAtomicExchange(this, node, opcode);
}

void InstructionSelector::VisitWord64AtomicExchange(Node* node) {
  MachineType type = AtomicOpType(node->op());
  ArchOpcode opcode = kArchNop;
  if (type == MachineType::Uint8()) {
    opcode = kX64Word64AtomicExchangeUint8;
  } else if (type == MachineType::Uint16()) {
    opcode = kX64Word64AtomicExchangeUint16;
  } else if (type == MachineType::Uint32()) {
    opcode = kX64Word64AtomicExchangeUint32;
  } else if (type == MachineType::Uint64()) {
    opcode = kX64Word64AtomicExchangeUint64;
  } else {
    UNREACHABLE();
    return;
  }
  VisitAtomicExchange(this, node, opcode);
}

void InstructionSelector::VisitWord32AtomicCompareExchange(Node* node) {
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
  VisitAtomicCompareExchange(this, node, opcode);
}

void InstructionSelector::VisitWord64AtomicCompareExchange(Node* node) {
  MachineType type = AtomicOpType(node->op());
  ArchOpcode opcode = kArchNop;
  if (type == MachineType::Uint8()) {
    opcode = kX64Word64AtomicCompareExchangeUint8;
  } else if (type == MachineType::Uint16()) {
    opcode = kX64Word64AtomicCompareExchangeUint16;
  } else if (type == MachineType::Uint32()) {
    opcode = kX64Word64AtomicCompareExchangeUint32;
  } else if (type == MachineType::Uint64()) {
    opcode = kX64Word64AtomicCompareExchangeUint64;
  } else {
    UNREACHABLE();
    return;
  }
  VisitAtomicCompareExchange(this, node, opcode);
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
    ArchOpcode word64_op) {
  MachineType type = AtomicOpType(node->op());
  ArchOpcode opcode = kArchNop;
  if (type == MachineType::Uint8()) {
    opcode = uint8_op;
  } else if (type == MachineType::Uint16()) {
    opcode = uint16_op;
  } else if (type == MachineType::Uint32()) {
    opcode = uint32_op;
  } else if (type == MachineType::Uint64()) {
    opcode = word64_op;
  } else {
    UNREACHABLE();
    return;
  }
  VisitAtomicBinop(this, node, opcode);
}

#define VISIT_ATOMIC_BINOP(op)                                           \
  void InstructionSelector::VisitWord64Atomic##op(Node* node) {          \
    VisitWord64AtomicBinaryOperation(                                    \
        node, kX64Word64Atomic##op##Uint8, kX64Word64Atomic##op##Uint16, \
        kX64Word64Atomic##op##Uint32, kX64Word64Atomic##op##Uint64);     \
  }
VISIT_ATOMIC_BINOP(Add)
VISIT_ATOMIC_BINOP(Sub)
VISIT_ATOMIC_BINOP(And)
VISIT_ATOMIC_BINOP(Or)
VISIT_ATOMIC_BINOP(Xor)
#undef VISIT_ATOMIC_BINOP

#define SIMD_TYPES(V) \
  V(F64x2)            \
  V(F32x4)            \
  V(I64x2)            \
  V(I32x4)            \
  V(I16x8)            \
  V(I8x16)

#define SIMD_BINOP_LIST(V) \
  V(F64x2Add)              \
  V(F64x2Sub)              \
  V(F64x2Mul)              \
  V(F64x2Div)              \
  V(F64x2Min)              \
  V(F64x2Max)              \
  V(F64x2Eq)               \
  V(F64x2Ne)               \
  V(F64x2Lt)               \
  V(F64x2Le)               \
  V(F32x4Add)              \
  V(F32x4AddHoriz)         \
  V(F32x4Sub)              \
  V(F32x4Mul)              \
  V(F32x4Div)              \
  V(F32x4Min)              \
  V(F32x4Max)              \
  V(F32x4Eq)               \
  V(F32x4Ne)               \
  V(F32x4Lt)               \
  V(F32x4Le)               \
  V(I64x2Add)              \
  V(I64x2Sub)              \
  V(I64x2Eq)               \
  V(I64x2GtS)              \
  V(I32x4Add)              \
  V(I32x4AddHoriz)         \
  V(I32x4Sub)              \
  V(I32x4Mul)              \
  V(I32x4MinS)             \
  V(I32x4MaxS)             \
  V(I32x4Eq)               \
  V(I32x4GtS)              \
  V(I32x4GeS)              \
  V(I32x4MinU)             \
  V(I32x4MaxU)             \
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
  V(I16x8GtS)              \
  V(I16x8GeS)              \
  V(I16x8AddSaturateU)     \
  V(I16x8SubSaturateU)     \
  V(I16x8MinU)             \
  V(I16x8MaxU)             \
  V(I16x8GeU)              \
  V(I8x16SConvertI16x8)    \
  V(I8x16Add)              \
  V(I8x16AddSaturateS)     \
  V(I8x16Sub)              \
  V(I8x16SubSaturateS)     \
  V(I8x16MinS)             \
  V(I8x16MaxS)             \
  V(I8x16Eq)               \
  V(I8x16GtS)              \
  V(I8x16GeS)              \
  V(I8x16AddSaturateU)     \
  V(I8x16SubSaturateU)     \
  V(I8x16MinU)             \
  V(I8x16MaxU)             \
  V(I8x16GeU)              \
  V(S128And)               \
  V(S128Or)                \
  V(S128Xor)

#define SIMD_BINOP_ONE_TEMP_LIST(V) \
  V(I64x2Ne)                        \
  V(I64x2GeS)                       \
  V(I64x2GtU)                       \
  V(I64x2GeU)                       \
  V(I32x4Ne)                        \
  V(I32x4GtU)                       \
  V(I16x8Ne)                        \
  V(I16x8GtU)                       \
  V(I8x16Ne)                        \
  V(I8x16GtU)

#define SIMD_UNOP_LIST(V)   \
  V(F64x2Sqrt)              \
  V(F32x4SConvertI32x4)     \
  V(F32x4Abs)               \
  V(F32x4Neg)               \
  V(F32x4Sqrt)              \
  V(F32x4RecipApprox)       \
  V(F32x4RecipSqrtApprox)   \
  V(I64x2Neg)               \
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
  V(I8x16Neg)               \
  V(S128Not)

#define SIMD_SHIFT_OPCODES(V) \
  V(I64x2Shl)                 \
  V(I64x2ShrU)                \
  V(I32x4Shl)                 \
  V(I32x4ShrS)                \
  V(I32x4ShrU)                \
  V(I16x8Shl)                 \
  V(I16x8ShrS)                \
  V(I16x8ShrU)

#define SIMD_NARROW_SHIFT_OPCODES(V) \
  V(I8x16Shl)                        \
  V(I8x16ShrS)                       \
  V(I8x16ShrU)

#define SIMD_ANYTRUE_LIST(V) \
  V(S1x2AnyTrue)             \
  V(S1x4AnyTrue)             \
  V(S1x8AnyTrue)             \
  V(S1x16AnyTrue)

#define SIMD_ALLTRUE_LIST(V) \
  V(S1x2AllTrue)             \
  V(S1x4AllTrue)             \
  V(S1x8AllTrue)             \
  V(S1x16AllTrue)

void InstructionSelector::VisitS128Zero(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64S128Zero, g.DefineAsRegister(node));
}

#define VISIT_SIMD_SPLAT(Type)                               \
  void InstructionSelector::Visit##Type##Splat(Node* node) { \
    X64OperandGenerator g(this);                             \
    Emit(kX64##Type##Splat, g.DefineAsRegister(node),        \
         g.Use(node->InputAt(0)));                           \
  }
SIMD_TYPES(VISIT_SIMD_SPLAT)
#undef VISIT_SIMD_SPLAT

#define SIMD_VISIT_EXTRACT_LANE(Type, Sign)                              \
  void InstructionSelector::Visit##Type##ExtractLane##Sign(Node* node) { \
    X64OperandGenerator g(this);                                         \
    int32_t lane = OpParameter<int32_t>(node->op());                     \
    Emit(kX64##Type##ExtractLane##Sign, g.DefineAsRegister(node),        \
         g.UseRegister(node->InputAt(0)), g.UseImmediate(lane));         \
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

#define VISIT_SIMD_REPLACE_LANE(Type)                              \
  void InstructionSelector::Visit##Type##ReplaceLane(Node* node) { \
    X64OperandGenerator g(this);                                   \
    int32_t lane = OpParameter<int32_t>(node->op());               \
    Emit(kX64##Type##ReplaceLane, g.DefineSameAsFirst(node),       \
         g.UseRegister(node->InputAt(0)), g.UseImmediate(lane),    \
         g.Use(node->InputAt(1)));                                 \
  }
SIMD_TYPES(VISIT_SIMD_REPLACE_LANE)
#undef VISIT_SIMD_REPLACE_LANE

#define VISIT_SIMD_SHIFT(Opcode)                                          \
  void InstructionSelector::Visit##Opcode(Node* node) {                   \
    X64OperandGenerator g(this);                                          \
    InstructionOperand temps[] = {g.TempSimd128Register()};               \
    Emit(kX64##Opcode, g.DefineSameAsFirst(node),                         \
         g.UseUniqueRegister(node->InputAt(0)),                           \
         g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps); \
  }
SIMD_SHIFT_OPCODES(VISIT_SIMD_SHIFT)
#undef VISIT_SIMD_SHIFT
#undef SIMD_SHIFT_OPCODES

#define VISIT_SIMD_NARROW_SHIFT(Opcode)                                       \
  void InstructionSelector::Visit##Opcode(Node* node) {                       \
    X64OperandGenerator g(this);                                              \
    InstructionOperand temps[] = {g.TempRegister(), g.TempSimd128Register()}; \
    Emit(kX64##Opcode, g.DefineSameAsFirst(node),                             \
         g.UseUniqueRegister(node->InputAt(0)),                               \
         g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps);     \
  }
SIMD_NARROW_SHIFT_OPCODES(VISIT_SIMD_NARROW_SHIFT)
#undef VISIT_SIMD_NARROW_SHIFT
#undef SIMD_NARROW_SHIFT_OPCODES

#define VISIT_SIMD_UNOP(Opcode)                         \
  void InstructionSelector::Visit##Opcode(Node* node) { \
    X64OperandGenerator g(this);                        \
    Emit(kX64##Opcode, g.DefineAsRegister(node),        \
         g.UseRegister(node->InputAt(0)));              \
  }
SIMD_UNOP_LIST(VISIT_SIMD_UNOP)
#undef VISIT_SIMD_UNOP
#undef SIMD_UNOP_LIST

#define VISIT_SIMD_BINOP(Opcode)                                            \
  void InstructionSelector::Visit##Opcode(Node* node) {                     \
    X64OperandGenerator g(this);                                            \
    Emit(kX64##Opcode, g.DefineSameAsFirst(node),                           \
         g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1))); \
  }
SIMD_BINOP_LIST(VISIT_SIMD_BINOP)
#undef VISIT_SIMD_BINOP
#undef SIMD_BINOP_LIST

#define VISIT_SIMD_BINOP_ONE_TEMP(Opcode)                                  \
  void InstructionSelector::Visit##Opcode(Node* node) {                    \
    X64OperandGenerator g(this);                                           \
    InstructionOperand temps[] = {g.TempSimd128Register()};                \
    Emit(kX64##Opcode, g.DefineSameAsFirst(node),                          \
         g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)), \
         arraysize(temps), temps);                                         \
  }
SIMD_BINOP_ONE_TEMP_LIST(VISIT_SIMD_BINOP_ONE_TEMP)
#undef VISIT_SIMD_BINOP_ONE_TEMP
#undef SIMD_BINOP_ONE_TEMP_LIST

#define VISIT_SIMD_ANYTRUE(Opcode)                                        \
  void InstructionSelector::Visit##Opcode(Node* node) {                   \
    X64OperandGenerator g(this);                                          \
    InstructionOperand temps[] = {g.TempRegister()};                      \
    Emit(kX64##Opcode, g.DefineAsRegister(node),                          \
         g.UseUniqueRegister(node->InputAt(0)), arraysize(temps), temps); \
  }
SIMD_ANYTRUE_LIST(VISIT_SIMD_ANYTRUE)
#undef VISIT_SIMD_ANYTRUE
#undef SIMD_ANYTRUE_LIST

#define VISIT_SIMD_ALLTRUE(Opcode)                                            \
  void InstructionSelector::Visit##Opcode(Node* node) {                       \
    X64OperandGenerator g(this);                                              \
    InstructionOperand temps[] = {g.TempRegister(), g.TempSimd128Register()}; \
    Emit(kX64##Opcode, g.DefineAsRegister(node),                              \
         g.UseUniqueRegister(node->InputAt(0)), arraysize(temps), temps);     \
  }
SIMD_ALLTRUE_LIST(VISIT_SIMD_ALLTRUE)
#undef VISIT_SIMD_ALLTRUE
#undef SIMD_ALLTRUE_LIST
#undef SIMD_TYPES

void InstructionSelector::VisitS128Select(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64S128Select, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)),
       g.UseRegister(node->InputAt(2)));
}

void InstructionSelector::VisitF64x2Abs(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempDoubleRegister()};
  Emit(kX64F64x2Abs, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
       arraysize(temps), temps);
}

void InstructionSelector::VisitF64x2Neg(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempDoubleRegister()};
  Emit(kX64F64x2Neg, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
       arraysize(temps), temps);
}

void InstructionSelector::VisitF64x2SConvertI64x2(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
  Emit(kX64F64x2SConvertI64x2, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), arraysize(temps), temps);
}

void InstructionSelector::VisitF64x2UConvertI64x2(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister(), g.TempSimd128Register()};
  // Need dst to be unique to temp because Cvtqui2sd will zero temp.
  Emit(kX64F64x2UConvertI64x2, g.DefineSameAsFirst(node),
       g.UseUniqueRegister(node->InputAt(0)), arraysize(temps), temps);
}

void InstructionSelector::VisitF32x4UConvertI32x4(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64F32x4UConvertI32x4, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)));
}

#define VISIT_SIMD_QFMOP(Opcode)                                             \
  void InstructionSelector::Visit##Opcode(Node* node) {                      \
    X64OperandGenerator g(this);                                             \
    if (CpuFeatures::IsSupported(FMA3)) {                                    \
      Emit(kX64##Opcode, g.DefineSameAsFirst(node),                          \
           g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)), \
           g.UseRegister(node->InputAt(2)));                                 \
    } else {                                                                 \
      InstructionOperand temps[] = {g.TempSimd128Register()};                \
      Emit(kX64##Opcode, g.DefineSameAsFirst(node),                          \
           g.UseUniqueRegister(node->InputAt(0)),                            \
           g.UseUniqueRegister(node->InputAt(1)),                            \
           g.UseRegister(node->InputAt(2)), arraysize(temps), temps);        \
    }                                                                        \
  }
VISIT_SIMD_QFMOP(F64x2Qfma)
VISIT_SIMD_QFMOP(F64x2Qfms)
VISIT_SIMD_QFMOP(F32x4Qfma)
VISIT_SIMD_QFMOP(F32x4Qfms)
#undef VISIT_SIMD_QFMOP

void InstructionSelector::VisitI64x2ShrS(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister()};
  // Use fixed to rcx, to use sarq_cl in codegen.
  Emit(kX64I64x2ShrS, g.DefineSameAsFirst(node),
       g.UseUniqueRegister(node->InputAt(0)), g.UseFixed(node->InputAt(1), rcx),
       arraysize(temps), temps);
}

void InstructionSelector::VisitI64x2Mul(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempSimd128Register(),
                                g.TempSimd128Register()};
  Emit(kX64I64x2Mul, g.DefineSameAsFirst(node),
       g.UseUniqueRegister(node->InputAt(0)),
       g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps);
}

void InstructionSelector::VisitI64x2MinS(Node* node) {
  X64OperandGenerator g(this);
  if (this->IsSupported(SSE4_2)) {
    InstructionOperand temps[] = {g.TempFpRegister(xmm0)};
    Emit(kX64I64x2MinS, g.DefineAsRegister(node),
         g.UseUniqueRegister(node->InputAt(0)),
         g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps);
  } else {
    InstructionOperand temps[] = {g.TempSimd128Register(), g.TempRegister(),
                                  g.TempRegister()};
    Emit(kX64I64x2MinS, g.DefineSameAsFirst(node),
         g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)),
         arraysize(temps), temps);
  }
}

void InstructionSelector::VisitI64x2MaxS(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempFpRegister(xmm0)};
  Emit(kX64I64x2MaxS, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseUniqueRegister(node->InputAt(1)),
       arraysize(temps), temps);
}

void InstructionSelector::VisitI64x2MinU(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempSimd128Register(),
                                g.TempFpRegister(xmm0)};
  Emit(kX64I64x2MinU, g.DefineAsRegister(node),
       g.UseUniqueRegister(node->InputAt(0)),
       g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps);
}

void InstructionSelector::VisitI64x2MaxU(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempSimd128Register(),
                                g.TempFpRegister(xmm0)};
  Emit(kX64I64x2MaxU, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseUniqueRegister(node->InputAt(1)),
       arraysize(temps), temps);
}

void InstructionSelector::VisitI32x4SConvertF32x4(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempSimd128Register()};
  Emit(kX64I32x4SConvertF32x4, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), arraysize(temps), temps);
}

void InstructionSelector::VisitI32x4UConvertF32x4(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempSimd128Register(),
                                g.TempSimd128Register()};
  Emit(kX64I32x4UConvertF32x4, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), arraysize(temps), temps);
}

void InstructionSelector::VisitI16x8UConvertI32x4(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64I16x8UConvertI32x4, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitI8x16UConvertI16x8(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64I8x16UConvertI16x8, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitI8x16Mul(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempSimd128Register()};
  Emit(kX64I8x16Mul, g.DefineSameAsFirst(node),
       g.UseUniqueRegister(node->InputAt(0)),
       g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps);
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
     kX64S64x2UnpackLow,
     true,
     true},
    {{8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31},
     kX64S64x2UnpackHigh,
     true,
     true},
    {{0, 1, 2, 3, 16, 17, 18, 19, 4, 5, 6, 7, 20, 21, 22, 23},
     kX64S32x4UnpackLow,
     true,
     true},
    {{8, 9, 10, 11, 24, 25, 26, 27, 12, 13, 14, 15, 28, 29, 30, 31},
     kX64S32x4UnpackHigh,
     true,
     true},
    {{0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21, 6, 7, 22, 23},
     kX64S16x8UnpackLow,
     true,
     true},
    {{8, 9, 24, 25, 10, 11, 26, 27, 12, 13, 28, 29, 14, 15, 30, 31},
     kX64S16x8UnpackHigh,
     true,
     true},
    {{0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23},
     kX64S8x16UnpackLow,
     true,
     true},
    {{8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31},
     kX64S8x16UnpackHigh,
     true,
     true},

    {{0, 1, 4, 5, 8, 9, 12, 13, 16, 17, 20, 21, 24, 25, 28, 29},
     kX64S16x8UnzipLow,
     true,
     true},
    {{2, 3, 6, 7, 10, 11, 14, 15, 18, 19, 22, 23, 26, 27, 30, 31},
     kX64S16x8UnzipHigh,
     true,
     true},
    {{0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30},
     kX64S8x16UnzipLow,
     true,
     true},
    {{1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31},
     kX64S8x16UnzipHigh,
     true,
     true},
    {{0, 16, 2, 18, 4, 20, 6, 22, 8, 24, 10, 26, 12, 28, 14, 30},
     kX64S8x16TransposeLow,
     true,
     true},
    {{1, 17, 3, 19, 5, 21, 7, 23, 9, 25, 11, 27, 13, 29, 15, 31},
     kX64S8x16TransposeHigh,
     true,
     true},
    {{7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8},
     kX64S8x8Reverse,
     true,
     true},
    {{3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12},
     kX64S8x4Reverse,
     true,
     true},
    {{1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14},
     kX64S8x2Reverse,
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

  X64OperandGenerator g(this);
  // Swizzles don't generally need DefineSameAsFirst to avoid a move.
  bool no_same_as_first = is_swizzle;
  // We generally need UseRegister for input0, Use for input1.
  bool src0_needs_reg = true;
  bool src1_needs_reg = false;
  ArchOpcode opcode = kX64S8x16Shuffle;  // general shuffle is the default

  uint8_t offset;
  uint8_t shuffle32x4[4];
  uint8_t shuffle16x8[8];
  int index;
  const ShuffleEntry* arch_shuffle;
  if (TryMatchConcat(shuffle, &offset)) {
    // Swap inputs from the normal order for (v)palignr.
    SwapShuffleInputs(node);
    is_swizzle = false;        // It's simpler to just handle the general case.
    no_same_as_first = false;  // SSE requires same-as-first.
    // TODO(v8:9608): also see v8:9083
    src1_needs_reg = true;
    opcode = kX64S8x16Alignr;
    // palignr takes a single imm8 offset.
    imms[imm_count++] = offset;
  } else if (TryMatchArchShuffle(shuffle, arch_shuffles,
                                 arraysize(arch_shuffles), is_swizzle,
                                 &arch_shuffle)) {
    opcode = arch_shuffle->opcode;
    src0_needs_reg = arch_shuffle->src0_needs_reg;
    // SSE can't take advantage of both operands in registers and needs
    // same-as-first.
    src1_needs_reg = arch_shuffle->src1_needs_reg;
    no_same_as_first = false;
  } else if (TryMatch32x4Shuffle(shuffle, shuffle32x4)) {
    uint8_t shuffle_mask = PackShuffle4(shuffle32x4);
    if (is_swizzle) {
      if (TryMatchIdentity(shuffle)) {
        // Bypass normal shuffle code generation in this case.
        EmitIdentity(node);
        return;
      } else {
        // pshufd takes a single imm8 shuffle mask.
        opcode = kX64S32x4Swizzle;
        no_same_as_first = true;
        // TODO(v8:9083): This doesn't strictly require a register, forcing the
        // swizzles to always use registers until generation of incorrect memory
        // operands can be fixed.
        src0_needs_reg = true;
        imms[imm_count++] = shuffle_mask;
      }
    } else {
      // 2 operand shuffle
      // A blend is more efficient than a general 32x4 shuffle; try it first.
      if (TryMatchBlend(shuffle)) {
        opcode = kX64S16x8Blend;
        uint8_t blend_mask = PackBlend4(shuffle32x4);
        imms[imm_count++] = blend_mask;
      } else {
        opcode = kX64S32x4Shuffle;
        no_same_as_first = true;
        // TODO(v8:9083): src0 and src1 is used by pshufd in codegen, which
        // requires memory to be 16-byte aligned, since we cannot guarantee that
        // yet, force using a register here.
        src0_needs_reg = true;
        src1_needs_reg = true;
        imms[imm_count++] = shuffle_mask;
        uint8_t blend_mask = PackBlend4(shuffle32x4);
        imms[imm_count++] = blend_mask;
      }
    }
  } else if (TryMatch16x8Shuffle(shuffle, shuffle16x8)) {
    uint8_t blend_mask;
    if (TryMatchBlend(shuffle)) {
      opcode = kX64S16x8Blend;
      blend_mask = PackBlend8(shuffle16x8);
      imms[imm_count++] = blend_mask;
    } else if (TryMatchDup<8>(shuffle, &index)) {
      opcode = kX64S16x8Dup;
      src0_needs_reg = false;
      imms[imm_count++] = index;
    } else if (TryMatch16x8HalfShuffle(shuffle16x8, &blend_mask)) {
      opcode = is_swizzle ? kX64S16x8HalfShuffle1 : kX64S16x8HalfShuffle2;
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
    opcode = kX64S8x16Dup;
    no_same_as_first = false;
    src0_needs_reg = true;
    imms[imm_count++] = index;
  }
  if (opcode == kX64S8x16Shuffle) {
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

void InstructionSelector::VisitS8x16Swizzle(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempSimd128Register()};
  Emit(kX64S8x16Swizzle, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseUniqueRegister(node->InputAt(1)),
       arraysize(temps), temps);
}

// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags =
      MachineOperatorBuilder::kWord32ShiftIsSafe |
      MachineOperatorBuilder::kWord32Ctz | MachineOperatorBuilder::kWord64Ctz;
  if (CpuFeatures::IsSupported(POPCNT)) {
    flags |= MachineOperatorBuilder::kWord32Popcnt |
             MachineOperatorBuilder::kWord64Popcnt;
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
