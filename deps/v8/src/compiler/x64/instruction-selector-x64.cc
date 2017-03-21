// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "src/base/adapters.h"
#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

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
        const int64_t value = OpParameter<int64_t>(node);
        return value == static_cast<int64_t>(static_cast<int32_t>(value));
      }
      case IrOpcode::kNumberConstant: {
        const double value = OpParameter<double>(node);
        return bit_cast<int64_t>(value) == 0;
      }
      default:
        return false;
    }
  }

  int32_t GetImmediateIntegerValue(Node* node) {
    DCHECK(CanBeImmediate(node));
    if (node->opcode() == IrOpcode::kInt32Constant) {
      return OpParameter<int32_t>(node);
    }
    DCHECK_EQ(IrOpcode::kInt64Constant, node->opcode());
    return static_cast<int32_t>(OpParameter<int64_t>(node));
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
      case kX64Cmp:
      case kX64Test:
        return rep == MachineRepresentation::kWord64 || IsAnyTagged(rep);
      case kX64Cmp32:
      case kX64Test32:
        return rep == MachineRepresentation::kWord32;
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
          OpParameter<int32_t>(base) == 0) {
        base = nullptr;
      } else if (base->opcode() == IrOpcode::kInt64Constant &&
                 OpParameter<int64_t>(base) == 0) {
        base = nullptr;
      }
    }
    if (base != nullptr) {
      inputs[(*input_count)++] = UseRegister(base);
      if (index != nullptr) {
        DCHECK(scale_exponent >= 0 && scale_exponent <= 3);
        inputs[(*input_count)++] = UseRegister(index);
        if (displacement != nullptr) {
          inputs[(*input_count)++] = displacement_mode
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
    if (selector()->CanAddressRelativeToRootsRegister()) {
      LoadMatcher<ExternalReferenceMatcher> m(operand);
      if (m.index().HasValue() && m.object().HasValue()) {
        Address const kRootsRegisterValue =
            kRootRegisterBias +
            reinterpret_cast<Address>(
                selector()->isolate()->heap()->roots_array_start());
        ptrdiff_t const delta =
            m.index().Value() +
            (m.object().Value().address() - kRootsRegisterValue);
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
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:  // Fall through.
    case MachineRepresentation::kWord64:
      opcode = kX64Movq;
      break;
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      break;
  }
  return opcode;
}

ArchOpcode GetStoreOpcode(StoreRepresentation store_rep) {
  switch (store_rep.representation()) {
    case MachineRepresentation::kFloat32:
      return kX64Movss;
      break;
    case MachineRepresentation::kFloat64:
      return kX64Movsd;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      return kX64Movb;
      break;
    case MachineRepresentation::kWord16:
      return kX64Movw;
      break;
    case MachineRepresentation::kWord32:
      return kX64Movl;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord64:
      return kX64Movq;
      break;
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return kArchNop;
  }
  UNREACHABLE();
  return kArchNop;
}

}  // namespace

void InstructionSelector::VisitLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  X64OperandGenerator g(this);

  ArchOpcode opcode = GetLoadOpcode(load_rep);
  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(node);
  InstructionOperand inputs[4];
  size_t input_count = 0;
  AddressingMode mode =
      g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
  InstructionCode code = opcode | AddressingModeField::encode(mode);
  if (node->opcode() == IrOpcode::kProtectedLoad) {
    code |= MiscField::encode(X64MemoryProtection::kProtected);
    // Add the source position as an input
    inputs[input_count++] = g.UseImmediate(node->InputAt(2));
  }
  Emit(code, 1, outputs, input_count, inputs);
}

void InstructionSelector::VisitProtectedLoad(Node* node) { VisitLoad(node); }

void InstructionSelector::VisitStore(Node* node) {
  X64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());
  WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();

  if (write_barrier_kind != kNoWriteBarrier) {
    DCHECK(CanBeTaggedPointer(store_rep.representation()));
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
    ArchOpcode opcode = GetStoreOpcode(store_rep);
    InstructionOperand inputs[4];
    size_t input_count = 0;
    AddressingMode addressing_mode =
        g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
    InstructionCode code =
        opcode | AddressingModeField::encode(addressing_mode);
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
  Node* position = node->InputAt(3);

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());

  ArchOpcode opcode = GetStoreOpcode(store_rep);
  InstructionOperand inputs[5];
  size_t input_count = 0;
  AddressingMode addressing_mode =
      g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode) |
                         MiscField::encode(X64MemoryProtection::kProtected);
  InstructionOperand value_operand =
      g.CanBeImmediate(value) ? g.UseImmediate(value) : g.UseRegister(value);
  inputs[input_count++] = value_operand;
  inputs[input_count++] = g.UseImmediate(position);
  Emit(code, 0, static_cast<InstructionOperand*>(nullptr), input_count, inputs);
}

// Architecture supports unaligned access, therefore VisitLoad is used instead
void InstructionSelector::VisitUnalignedLoad(Node* node) { UNREACHABLE(); }

// Architecture supports unaligned access, therefore VisitStore is used instead
void InstructionSelector::VisitUnalignedStore(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitCheckedLoad(Node* node) {
  CheckedLoadRepresentation load_rep = CheckedLoadRepresentationOf(node->op());
  X64OperandGenerator g(this);
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
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:   // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }
  if (offset->opcode() == IrOpcode::kInt32Add && CanCover(node, offset)) {
    Int32Matcher mlength(length);
    Int32BinopMatcher moffset(offset);
    if (mlength.HasValue() && moffset.right().HasValue() &&
        moffset.right().Value() >= 0 &&
        mlength.Value() >= moffset.right().Value()) {
      Emit(opcode, g.DefineAsRegister(node), g.UseRegister(buffer),
           g.UseRegister(moffset.left().node()),
           g.UseImmediate(moffset.right().node()), g.UseImmediate(length));
      return;
    }
  }
  InstructionOperand length_operand =
      g.CanBeImmediate(length) ? g.UseImmediate(length) : g.UseRegister(length);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(buffer),
       g.UseRegister(offset), g.TempImmediate(0), length_operand);
}


void InstructionSelector::VisitCheckedStore(Node* node) {
  MachineRepresentation rep = CheckedStoreRepresentationOf(node->op());
  X64OperandGenerator g(this);
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
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:   // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }
  InstructionOperand value_operand =
      g.CanBeImmediate(value) ? g.UseImmediate(value) : g.UseRegister(value);
  if (offset->opcode() == IrOpcode::kInt32Add && CanCover(node, offset)) {
    Int32Matcher mlength(length);
    Int32BinopMatcher moffset(offset);
    if (mlength.HasValue() && moffset.right().HasValue() &&
        moffset.right().Value() >= 0 &&
        mlength.Value() >= moffset.right().Value()) {
      Emit(opcode, g.NoOutput(), g.UseRegister(buffer),
           g.UseRegister(moffset.left().node()),
           g.UseImmediate(moffset.right().node()), g.UseImmediate(length),
           value_operand);
      return;
    }
  }
  InstructionOperand length_operand =
      g.CanBeImmediate(length) ? g.UseImmediate(length) : g.UseRegister(length);
  Emit(opcode, g.NoOutput(), g.UseRegister(buffer), g.UseRegister(offset),
       g.TempImmediate(0), length_operand, value_operand);
}


// Shared routine for multiple binary operations.
static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();
  InstructionOperand inputs[4];
  size_t input_count = 0;
  InstructionOperand outputs[2];
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

  DCHECK_NE(0u, input_count);
  DCHECK_NE(0u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  opcode = cont->Encode(opcode);
  if (cont->IsDeoptimize()) {
    selector->EmitDeoptimize(opcode, output_count, outputs, input_count, inputs,
                             cont->reason(), cont->frame_state());
  } else {
    selector->Emit(opcode, output_count, outputs, input_count, inputs);
  }
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
  if (m.right().Is(0xff)) {
    Emit(kX64Movzxbl, g.DefineAsRegister(node), g.Use(m.left().node()));
  } else if (m.right().Is(0xffff)) {
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


namespace {

// Shared routine for multiple 32-bit shift operations.
// TODO(bmeurer): Merge this with VisitWord64Shift using template magic?
void VisitWord32Shift(InstructionSelector* selector, Node* node,
                      ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();

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
bool TryMatchLoadWord64AndShiftRight(InstructionSelector* selector, Node* node,
                                     InstructionCode opcode) {
  DCHECK(IrOpcode::kWord64Sar == node->opcode() ||
         IrOpcode::kWord64Shr == node->opcode());
  X64OperandGenerator g(selector);
  Int64BinopMatcher m(node);
  if (selector->CanCover(m.node(), m.left().node()) && m.left().IsLoad() &&
      m.right().Is(32)) {
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
        switch (mode) {
          case kMode_MR:
            mode = kMode_MRI;
            break;
          case kMode_MR1:
            mode = kMode_MR1I;
            break;
          case kMode_MR2:
            mode = kMode_MR2I;
            break;
          case kMode_MR4:
            mode = kMode_MR4I;
            break;
          case kMode_MR8:
            mode = kMode_MR8I;
            break;
          case kMode_M1:
            mode = kMode_M1I;
            break;
          case kMode_M2:
            mode = kMode_M2I;
            break;
          case kMode_M4:
            mode = kMode_M4I;
            break;
          case kMode_M8:
            mode = kMode_M8I;
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
        inputs[input_count++] = ImmediateOperand(ImmediateOperand::INLINE, 4);
      } else {
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


void InstructionSelector::VisitWord64Clz(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Lzcnt, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitWord32Clz(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Lzcnt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitWord64Ctz(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Tzcnt, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitWord32Ctz(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Tzcnt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitWord32ReverseBits(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitWord64ReverseBits(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord64ReverseBytes(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord32ReverseBytes(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord32Popcnt(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Popcnt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitWord64Popcnt(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Popcnt, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
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
  Int32BinopMatcher m(node);
  if (m.left().Is(0)) {
    Emit(kX64Neg32, g.DefineSameAsFirst(node), g.UseRegister(m.right().node()));
  } else {
    if (m.right().HasValue() && g.CanBeImmediate(m.right().node())) {
      // Turn subtractions of constant values into immediate "leal" instructions
      // by negating the value.
      Emit(kX64Lea32 | AddressingModeField::encode(kMode_MRI),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(-m.right().Value()));
      return;
    }
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


void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat32ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEInt32ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEUint32ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64ToInt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64ToUint32 | MiscField::encode(1), g.DefineAsRegister(node),
       g.Use(node->InputAt(0)));
}

void InstructionSelector::VisitTruncateFloat64ToUint32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64ToUint32 | MiscField::encode(0), g.DefineAsRegister(node),
       g.Use(node->InputAt(0)));
}

void InstructionSelector::VisitTruncateFloat32ToInt32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat32ToInt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitTruncateFloat32ToUint32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat32ToUint32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
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
    case IrOpcode::kLoad: {
      // The movzxbl/movsxbl/movzxwl/movsxwl operations implicitly zero-extend
      // to 64-bit on x64,
      // so the zero-extension is a no-op.
      LoadRepresentation load_rep = LoadRepresentationOf(node->op());
      switch (load_rep.representation()) {
        case MachineRepresentation::kWord8:
        case MachineRepresentation::kWord16:
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
  if (selector->IsSupported(AVX)) {
    selector->Emit(avx_opcode, g.DefineAsRegister(node), g.Use(input));
  } else {
    selector->Emit(sse_opcode, g.DefineSameAsFirst(node), g.UseRegister(input));
  }
}

}  // namespace


void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  VisitRO(this, node, kSSEFloat64ToFloat32);
}

void InstructionSelector::VisitTruncateFloat64ToWord32(Node* node) {
  VisitRR(this, node, kArchTruncateDoubleToI);
}


void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  X64OperandGenerator g(this);
  Node* value = node->InputAt(0);
  if (CanCover(node, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord64Sar:
      case IrOpcode::kWord64Shr: {
        Int64BinopMatcher m(value);
        if (m.right().Is(32)) {
          if (TryMatchLoadWord64AndShiftRight(this, value, kX64Movl)) {
            return EmitIdentity(node);
          }
          Emit(kX64Shr, g.DefineSameAsFirst(node),
               g.UseRegister(m.left().node()), g.TempImmediate(32));
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

void InstructionSelector::VisitRoundFloat64ToInt32(Node* node) {
  VisitRO(this, node, kSSEFloat64ToInt32);
}

void InstructionSelector::VisitRoundInt32ToFloat32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEInt32ToFloat32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitRoundInt64ToFloat32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEInt64ToFloat32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitRoundInt64ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEInt64ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitRoundUint32ToFloat32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEUint32ToFloat32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitRoundUint64ToFloat32(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister()};
  Emit(kSSEUint64ToFloat32, g.DefineAsRegister(node), g.Use(node->InputAt(0)),
       arraysize(temps), temps);
}


void InstructionSelector::VisitRoundUint64ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister()};
  Emit(kSSEUint64ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)),
       arraysize(temps), temps);
}


void InstructionSelector::VisitBitcastFloat32ToInt32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64BitcastFI, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitBitcastFloat64ToInt64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64BitcastDL, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitBitcastInt32ToFloat32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64BitcastIF, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitBitcastInt64ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64BitcastLD, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
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


void InstructionSelector::VisitFloat32Sqrt(Node* node) {
  VisitRO(this, node, kSSEFloat32Sqrt);
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

void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  VisitRO(this, node, kSSEFloat64Sqrt);
}


void InstructionSelector::VisitFloat32RoundDown(Node* node) {
  VisitRR(this, node, kSSEFloat32Round | MiscField::encode(kRoundDown));
}


void InstructionSelector::VisitFloat64RoundDown(Node* node) {
  VisitRR(this, node, kSSEFloat64Round | MiscField::encode(kRoundDown));
}


void InstructionSelector::VisitFloat32RoundUp(Node* node) {
  VisitRR(this, node, kSSEFloat32Round | MiscField::encode(kRoundUp));
}


void InstructionSelector::VisitFloat64RoundUp(Node* node) {
  VisitRR(this, node, kSSEFloat64Round | MiscField::encode(kRoundUp));
}


void InstructionSelector::VisitFloat32RoundTruncate(Node* node) {
  VisitRR(this, node, kSSEFloat32Round | MiscField::encode(kRoundToZero));
}


void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  VisitRR(this, node, kSSEFloat64Round | MiscField::encode(kRoundToZero));
}


void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  UNREACHABLE();
}


void InstructionSelector::VisitFloat32RoundTiesEven(Node* node) {
  VisitRR(this, node, kSSEFloat32Round | MiscField::encode(kRoundToNearest));
}


void InstructionSelector::VisitFloat64RoundTiesEven(Node* node) {
  VisitRR(this, node, kSSEFloat64Round | MiscField::encode(kRoundToNearest));
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
    ZoneVector<PushParameter>* arguments, const CallDescriptor* descriptor,
    Node* node) {
  X64OperandGenerator g(this);

  // Prepare for C function call.
  if (descriptor->IsCFunctionCall()) {
    Emit(kArchPrepareCallCFunction |
             MiscField::encode(static_cast<int>(descriptor->ParameterCount())),
         0, nullptr, 0, nullptr);

    // Poke any stack arguments.
    for (size_t n = 0; n < arguments->size(); ++n) {
      PushParameter input = (*arguments)[n];
      if (input.node()) {
        int slot = static_cast<int>(n);
        InstructionOperand value = g.CanBeImmediate(input.node())
                                       ? g.UseImmediate(input.node())
                                       : g.UseRegister(input.node());
        Emit(kX64Poke | MiscField::encode(slot), g.NoOutput(), value);
      }
    }
  } else {
    // Push any stack arguments.
    for (PushParameter input : base::Reversed(*arguments)) {
      // TODO(titzer): X64Push cannot handle stack->stack double moves
      // because there is no way to encode fixed double slots.
      InstructionOperand value =
          g.CanBeImmediate(input.node())
              ? g.UseImmediate(input.node())
              : IsSupported(ATOM) ||
                        sequence()->IsFP(GetVirtualRegister(input.node()))
                    ? g.UseRegister(input.node())
                    : g.Use(input.node());
      Emit(kX64Push, g.NoOutput(), value);
    }
  }
}


bool InstructionSelector::IsTailCallAddressImmediate() { return true; }

int InstructionSelector::GetTempsCountForTailCallFromJSFunction() { return 3; }

namespace {

void VisitCompareWithMemoryOperand(InstructionSelector* selector,
                                   InstructionCode opcode, Node* left,
                                   InstructionOperand right,
                                   FlagsContinuation* cont) {
  DCHECK(left->opcode() == IrOpcode::kLoad);
  X64OperandGenerator g(selector);
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
                             cont->reason(), cont->frame_state());
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
  X64OperandGenerator g(selector);
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

// Shared routine for 64-bit word comparison operations.
void VisitWord64Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  if (selector->CanUseRootsRegister()) {
    Heap* const heap = selector->isolate()->heap();
    Heap::RootListIndex root_index;
    HeapObjectBinopMatcher m(node);
    if (m.right().HasValue() &&
        heap->IsRootHandle(m.right().Value(), &root_index)) {
      if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
      InstructionCode opcode =
          kX64Cmp | AddressingModeField::encode(kMode_Root);
      return VisitCompare(
          selector, opcode,
          g.TempImmediate((root_index * kPointerSize) - kRootRegisterBias),
          g.UseRegister(m.left().node()), cont);
    } else if (m.left().HasValue() &&
               heap->IsRootHandle(m.left().Value(), &root_index)) {
      InstructionCode opcode =
          kX64Cmp | AddressingModeField::encode(kMode_Root);
      return VisitCompare(
          selector, opcode,
          g.TempImmediate((root_index * kPointerSize) - kRootRegisterBias),
          g.UseRegister(m.right().node()), cont);
    }
  }
  Int64BinopMatcher m(node);
  if (m.left().IsLoad() && m.right().IsLoadStackPointer()) {
    LoadMatcher<ExternalReferenceMatcher> mleft(m.left().node());
    ExternalReference js_stack_limit =
        ExternalReference::address_of_stack_limit(selector->isolate());
    if (mleft.object().Is(js_stack_limit) && mleft.index().Is(0)) {
      // Compare(Load(js_stack_limit), LoadStackPointer)
      if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
      InstructionCode opcode = cont->Encode(kX64StackCheck);
      if (cont->IsBranch()) {
        selector->Emit(opcode, g.NoOutput(), g.Label(cont->true_block()),
                       g.Label(cont->false_block()));
      } else if (cont->IsDeoptimize()) {
        selector->EmitDeoptimize(opcode, 0, nullptr, 0, nullptr, cont->reason(),
                                 cont->frame_state());
      } else if (cont->IsSet()) {
        selector->Emit(opcode, g.DefineAsRegister(cont->result()));
      } else {
        DCHECK(cont->IsTrap());
        selector->Emit(opcode, g.NoOutput(), g.UseImmediate(cont->trap_id()));
      }
      return;
    }
  }
  VisitWordCompare(selector, node, kX64Cmp, cont);
}


// Shared routine for comparison with zero.
void VisitCompareZero(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  VisitCompare(selector, opcode, g.Use(node), g.TempImmediate(0), cont);
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

// Shared routine for word comparison against zero.
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
        return VisitWordCompare(selector, value, kX64Cmp32, cont);
      case IrOpcode::kInt32LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWordCompare(selector, value, kX64Cmp32, cont);
      case IrOpcode::kInt32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWordCompare(selector, value, kX64Cmp32, cont);
      case IrOpcode::kUint32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWordCompare(selector, value, kX64Cmp32, cont);
      case IrOpcode::kUint32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWordCompare(selector, value, kX64Cmp32, cont);
      case IrOpcode::kWord64Equal: {
        cont->OverwriteAndNegateIfEqual(kEqual);
        Int64BinopMatcher m(value);
        if (m.right().Is(0)) {
          // Try to combine the branch with a comparison.
          Node* const user = m.node();
          Node* const value = m.left().node();
          if (selector->CanCover(user, value)) {
            switch (value->opcode()) {
              case IrOpcode::kInt64Sub:
                return VisitWord64Compare(selector, value, cont);
              case IrOpcode::kWord64And:
                return VisitWordCompare(selector, value, kX64Test, cont);
              default:
                break;
            }
          }
          return VisitCompareZero(selector, value, kX64Cmp, cont);
        }
        return VisitWord64Compare(selector, value, cont);
      }
      case IrOpcode::kInt64LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWord64Compare(selector, value, cont);
      case IrOpcode::kInt64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWord64Compare(selector, value, cont);
      case IrOpcode::kUint64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWord64Compare(selector, value, cont);
      case IrOpcode::kUint64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWord64Compare(selector, value, cont);
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
              selector->IsSupported(AVX) ? kAVXFloat64Cmp : kSSEFloat64Cmp;
          return VisitCompare(selector, opcode, m.left().node(),
                              m.right().InputAt(0), cont, false);
        }
        cont->OverwriteAndNegateIfEqual(kUnsignedGreaterThan);
        return VisitFloat64Compare(selector, value, cont);
      }
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
                return VisitBinop(selector, node, kX64Add32, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kX64Sub32, cont);
              case IrOpcode::kInt32MulWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kX64Imul32, cont);
              case IrOpcode::kInt64AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kX64Add, cont);
              case IrOpcode::kInt64SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kX64Sub, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Sub:
        return VisitWordCompare(selector, value, kX64Cmp32, cont);
      case IrOpcode::kInt64Sub:
        return VisitWord64Compare(selector, value, cont);
      case IrOpcode::kWord32And:
        return VisitWordCompare(selector, value, kX64Test32, cont);
      case IrOpcode::kWord64And:
        return VisitWordCompare(selector, value, kX64Test, cont);
      default:
        break;
    }
  }

  // Branch could not be combined with a compare, emit compare against 0.
  VisitCompareZero(selector, value, kX64Cmp32, cont);
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
  X64OperandGenerator g(this);
  InstructionOperand value_operand = g.UseRegister(node->InputAt(0));

  // Emit either ArchTableSwitch or ArchLookupSwitch.
  size_t table_space_cost = 4 + sw.value_range;
  size_t table_time_cost = 3;
  size_t lookup_space_cost = 3 + 2 * sw.case_count;
  size_t lookup_time_cost = sw.case_count;
  if (sw.case_count > 4 &&
      table_space_cost + 3 * table_time_cost <=
          lookup_space_cost + 3 * lookup_time_cost &&
      sw.min_value > std::numeric_limits<int32_t>::min()) {
    InstructionOperand index_operand = g.TempRegister();
    if (sw.min_value) {
      // The leal automatically zero extends, so result is a valid 64-bit index.
      Emit(kX64Lea32 | AddressingModeField::encode(kMode_MRI), index_operand,
           value_operand, g.TempImmediate(-sw.min_value));
    } else {
      // Zero extend, because we use it as 64-bit index into the jump table.
      Emit(kX64Movl, index_operand, value_operand);
    }
    // Generate a table lookup.
    return EmitTableSwitch(sw, index_operand);
  }

  // Generate a sequence of conditional jumps.
  return EmitLookupSwitch(sw, value_operand);
}


void InstructionSelector::VisitWord32Equal(Node* const node) {
  Node* user = node;
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int32BinopMatcher m(user);
  if (m.right().Is(0)) {
    Node* value = m.left().node();

    // Try to combine with comparisons against 0 by simply inverting the branch.
    while (CanCover(user, value) && value->opcode() == IrOpcode::kWord32Equal) {
      Int32BinopMatcher m(value);
      if (m.right().Is(0)) {
        user = value;
        value = m.left().node();
        cont.Negate();
      } else {
        break;
      }
    }

    // Try to combine the branch with a comparison.
    if (CanCover(user, value)) {
      switch (value->opcode()) {
        case IrOpcode::kInt32Sub:
          return VisitWordCompare(this, value, kX64Cmp32, &cont);
        case IrOpcode::kWord32And:
          return VisitWordCompare(this, value, kX64Test32, &cont);
        default:
          break;
      }
    }
    return VisitCompareZero(this, value, kX64Cmp32, &cont);
  }
  VisitWordCompare(this, node, kX64Cmp32, &cont);
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


void InstructionSelector::VisitWord64Equal(Node* const node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int64BinopMatcher m(node);
  if (m.right().Is(0)) {
    // Try to combine the equality check with a comparison.
    Node* const user = m.node();
    Node* const value = m.left().node();
    if (CanCover(user, value)) {
      switch (value->opcode()) {
        case IrOpcode::kInt64Sub:
          return VisitWord64Compare(this, value, &cont);
        case IrOpcode::kWord64And:
          return VisitWordCompare(this, value, kX64Test, &cont);
        default:
          break;
      }
    }
  }
  VisitWord64Compare(this, node, &cont);
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
  VisitWord64Compare(this, node, &cont);
}


void InstructionSelector::VisitInt64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWord64Compare(this, node, &cont);
}


void InstructionSelector::VisitUint64LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWord64Compare(this, node, &cont);
}


void InstructionSelector::VisitUint64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWord64Compare(this, node, &cont);
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


void InstructionSelector::VisitFloat64ExtractLowWord32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64ExtractLowWord32, g.DefineAsRegister(node),
       g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64ExtractHighWord32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64ExtractHighWord32, g.DefineAsRegister(node),
       g.Use(node->InputAt(0)));
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

void InstructionSelector::VisitAtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  DCHECK(load_rep.representation() == MachineRepresentation::kWord8 ||
         load_rep.representation() == MachineRepresentation::kWord16 ||
         load_rep.representation() == MachineRepresentation::kWord32);
  USE(load_rep);
  VisitLoad(node);
}

void InstructionSelector::VisitAtomicStore(Node* node) {
  X64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  MachineRepresentation rep = AtomicStoreRepresentationOf(node->op());
  ArchOpcode opcode = kArchNop;
  switch (rep) {
    case MachineRepresentation::kWord8:
      opcode = kX64Xchgb;
      break;
    case MachineRepresentation::kWord16:
      opcode = kX64Xchgw;
      break;
    case MachineRepresentation::kWord32:
      opcode = kX64Xchgl;
      break;
    default:
      UNREACHABLE();
      return;
  }
  AddressingMode addressing_mode;
  InstructionOperand inputs[4];
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
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 0, static_cast<InstructionOperand*>(nullptr), input_count, inputs);
}

void InstructionSelector::VisitCreateInt32x4(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Int32x4Create, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}

void InstructionSelector::VisitInt32x4ExtractLane(Node* node) {
  X64OperandGenerator g(this);
  int32_t lane = OpParameter<int32_t>(node);
  Emit(kX64Int32x4ExtractLane, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseImmediate(lane));
}

void InstructionSelector::VisitInt32x4ReplaceLane(Node* node) {
  X64OperandGenerator g(this);
  int32_t lane = OpParameter<int32_t>(node);
  Emit(kX64Int32x4ReplaceLane, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseImmediate(lane),
       g.Use(node->InputAt(1)));
}

void InstructionSelector::VisitInt32x4Add(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Int32x4Add, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitInt32x4Sub(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Int32x4Sub, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
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
