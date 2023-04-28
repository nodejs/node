// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/backend/riscv/instruction-selector-riscv.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

bool RiscvOperandGenerator::IsIntegerConstant(Node* node) {
  if (node->opcode() == IrOpcode::kNumberConstant) {
    const double value = OpParameter<double>(node->op());
    return base::bit_cast<int64_t>(value) == 0;
  }
  return (node->opcode() == IrOpcode::kInt32Constant) ||
         (node->opcode() == IrOpcode::kInt64Constant);
}

int64_t RiscvOperandGenerator::GetIntegerConstantValue(Node* node) {
  if (node->opcode() == IrOpcode::kInt32Constant) {
    return OpParameter<int32_t>(node->op());
  } else if (node->opcode() == IrOpcode::kInt64Constant) {
    return OpParameter<int64_t>(node->op());
  }
  DCHECK_EQ(node->opcode(), IrOpcode::kNumberConstant);
  const double value = OpParameter<double>(node->op());
  DCHECK_EQ(base::bit_cast<int64_t>(value), 0);
  return base::bit_cast<int64_t>(value);
}

bool RiscvOperandGenerator::CanBeImmediate(int64_t value,
                                           InstructionCode opcode) {
  switch (ArchOpcodeField::decode(opcode)) {
    case kRiscvShl32:
    case kRiscvSar32:
    case kRiscvShr32:
      return is_uint5(value);
    case kRiscvShl64:
    case kRiscvSar64:
    case kRiscvShr64:
      return is_uint6(value);
    case kRiscvAdd32:
    case kRiscvAnd32:
    case kRiscvAnd:
    case kRiscvAdd64:
    case kRiscvOr32:
    case kRiscvOr:
    case kRiscvTst:
    case kRiscvXor:
      return is_int12(value);
    case kRiscvLb:
    case kRiscvLbu:
    case kRiscvSb:
    case kRiscvLh:
    case kRiscvLhu:
    case kRiscvSh:
    case kRiscvLw:
    case kRiscvSw:
    case kRiscvLd:
    case kRiscvSd:
    case kRiscvLoadFloat:
    case kRiscvStoreFloat:
    case kRiscvLoadDouble:
    case kRiscvStoreDouble:
      return is_int32(value);
    default:
      return is_int12(value);
  }
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
      DCHECK_EQ(selector_->GetEffectLevel(node),
                selector_->GetEffectLevel(m.left().node()));
      MachineRepresentation rep =
          LoadRepresentationOf(m.left().node()->op()).representation();
      DCHECK_EQ(3, ElementSizeLog2Of(rep));
      if (rep != MachineRepresentation::kTaggedSigned &&
          rep != MachineRepresentation::kTaggedPointer &&
          rep != MachineRepresentation::kTagged &&
          rep != MachineRepresentation::kWord64) {
        return;
      }

      RiscvOperandGenerator g(selector_);
      Node* load = m.left().node();
      Node* offset = load->InputAt(1);
      base_ = load->InputAt(0);
      opcode_ = kRiscvLw;
      if (g.CanBeImmediate(offset, opcode_)) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
        immediate_ = g.GetIntegerConstantValue(offset) + 4;
#elif defined(V8_TARGET_BIG_ENDIAN)
        immediate_ = g.GetIntegerConstantValue(offset);
#endif
        matches_ = g.CanBeImmediate(immediate_, kRiscvLw);
      }
    }
  }
};

bool TryEmitExtendingLoad(InstructionSelector* selector, Node* node,
                          Node* output_node) {
  ExtendingLoadMatcher m(node, selector);
  RiscvOperandGenerator g(selector);
  if (m.Matches()) {
    InstructionOperand inputs[2];
    inputs[0] = g.UseRegister(m.base());
    InstructionCode opcode =
        m.opcode() | AddressingModeField::encode(kMode_MRI);
    DCHECK(is_int32(m.immediate()));
    inputs[1] = g.TempImmediate(static_cast<int32_t>(m.immediate()));
    InstructionOperand outputs[] = {g.DefineAsRegister(output_node)};
    selector->Emit(opcode, arraysize(outputs), outputs, arraysize(inputs),
                   inputs);
    return true;
  }
  return false;
}

void EmitLoad(InstructionSelector* selector, Node* node, InstructionCode opcode,
              Node* output = nullptr) {
  RiscvOperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  ExternalReferenceMatcher m(base);
  if (m.HasResolvedValue() && g.IsIntegerConstant(index) &&
      selector->CanAddressRelativeToRootsRegister(m.ResolvedValue())) {
    ptrdiff_t const delta =
        g.GetIntegerConstantValue(index) +
        MacroAssemblerBase::RootRegisterOffsetForExternalReference(
            selector->isolate(), m.ResolvedValue());
    // Check that the delta is a 32-bit integer due to the limitations of
    // immediate operands.
    if (is_int32(delta)) {
      opcode |= AddressingModeField::encode(kMode_Root);
      selector->Emit(opcode,
                     g.DefineAsRegister(output == nullptr ? node : output),
                     g.UseImmediate(static_cast<int32_t>(delta)));
      return;
    }
  }

  if (base != nullptr && base->opcode() == IrOpcode::kLoadRootRegister) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_Root),
                   g.DefineAsRegister(output == nullptr ? node : output),
                   g.UseImmediate(index));
    return;
  }

  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(output == nullptr ? node : output),
                   g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(output == nullptr ? node : output),
                   addr_reg, g.TempImmediate(0));
  }
}

void EmitS128Load(InstructionSelector* selector, Node* node,
                  InstructionCode opcode, VSew sew, Vlmul lmul) {
  RiscvOperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(node), g.UseRegister(base),
                   g.UseImmediate(index), g.UseImmediate(sew),
                   g.UseImmediate(lmul));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(node), addr_reg, g.TempImmediate(0),
                   g.UseImmediate(sew), g.UseImmediate(lmul));
  }
}

void InstructionSelector::VisitStoreLane(Node* node) {
  StoreLaneParameters params = StoreLaneParametersOf(node->op());
  LoadStoreLaneParams f(params.rep, params.laneidx);
  InstructionCode opcode = kRiscvS128StoreLane;
  opcode |= MiscField::encode(f.sz);

  RiscvOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  InstructionOperand addr_reg = g.TempRegister();
  Emit(kRiscvAdd64, addr_reg, g.UseRegister(base), g.UseRegister(index));
  InstructionOperand inputs[4] = {
      g.UseRegister(node->InputAt(2)),
      g.UseImmediate(f.laneidx),
      addr_reg,
      g.TempImmediate(0),
  };
  opcode |= AddressingModeField::encode(kMode_MRI);
  Emit(opcode, 0, nullptr, 4, inputs);
}
void InstructionSelector::VisitLoadLane(Node* node) {
  LoadLaneParameters params = LoadLaneParametersOf(node->op());
  LoadStoreLaneParams f(params.rep.representation(), params.laneidx);
  InstructionCode opcode = kRiscvS128LoadLane;
  opcode |= MiscField::encode(f.sz);

  RiscvOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  InstructionOperand addr_reg = g.TempRegister();
  Emit(kRiscvAdd64, addr_reg, g.UseRegister(base), g.UseRegister(index));
  opcode |= AddressingModeField::encode(kMode_MRI);
  Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(2)),
       g.UseImmediate(params.laneidx), addr_reg, g.TempImmediate(0));
}

void InstructionSelector::VisitLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());

  InstructionCode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kRiscvLoadFloat;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kRiscvLoadDouble;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsUnsigned() ? kRiscvLbu : kRiscvLb;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsUnsigned() ? kRiscvLhu : kRiscvLh;
      break;
    case MachineRepresentation::kWord32:
      opcode = kRiscvLw;
      break;
#ifdef V8_COMPRESS_POINTERS
    case MachineRepresentation::kTaggedSigned:
      opcode = kRiscvLoadDecompressTaggedSigned;
      break;
    case MachineRepresentation::kTaggedPointer:
      opcode = kRiscvLoadDecompressTagged;
      break;
    case MachineRepresentation::kTagged:
      opcode = kRiscvLoadDecompressTagged;
      break;
#else
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
#endif
    case MachineRepresentation::kWord64:
      opcode = kRiscvLd;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kRiscvRvvLd;
      break;
    case MachineRepresentation::kCompressedPointer:
    case MachineRepresentation::kCompressed:
#ifdef V8_COMPRESS_POINTERS
      opcode = kRiscvLw;
      break;
#else
                                                 // Fall through.
#endif
    case MachineRepresentation::kSimd256:           // Fall through.
    case MachineRepresentation::kSandboxedPointer:  // Fall through.
    case MachineRepresentation::kMapWord:           // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }

  EmitLoad(this, node, opcode);
}

void InstructionSelector::VisitStore(Node* node) {
  RiscvOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());
  WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();
  MachineRepresentation rep = store_rep.representation();

  // TODO(riscv): I guess this could be done in a better way.
  if (write_barrier_kind != kNoWriteBarrier &&
      V8_LIKELY(!v8_flags.disable_write_barriers)) {
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
    code |= RecordWriteModeField::encode(record_write_mode);
    Emit(code, 0, nullptr, input_count, inputs, temp_count, temps);
  } else {
    ArchOpcode opcode;
    switch (rep) {
      case MachineRepresentation::kFloat32:
        opcode = kRiscvStoreFloat;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kRiscvStoreDouble;
        break;
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = kRiscvSb;
        break;
      case MachineRepresentation::kWord16:
        opcode = kRiscvSh;
        break;
      case MachineRepresentation::kWord32:
        opcode = kRiscvSw;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:
#ifdef V8_COMPRESS_POINTERS
        opcode = kRiscvStoreCompressTagged;
        break;
#endif
      case MachineRepresentation::kWord64:
        opcode = kRiscvSd;
        break;
      case MachineRepresentation::kSimd128:
        opcode = kRiscvRvvSt;
        break;
      case MachineRepresentation::kCompressedPointer:  // Fall through.
      case MachineRepresentation::kCompressed:
#ifdef V8_COMPRESS_POINTERS
        opcode = kRiscvStoreCompressTagged;
        break;
#else
        UNREACHABLE();
#endif
      case MachineRepresentation::kSimd256:           // Fall through.
      case MachineRepresentation::kSandboxedPointer:  // Fall through.
      case MachineRepresentation::kMapWord:           // Fall through.
      case MachineRepresentation::kNone:
        UNREACHABLE();
    }

    if (base != nullptr && base->opcode() == IrOpcode::kLoadRootRegister) {
      Emit(opcode | AddressingModeField::encode(kMode_Root), g.NoOutput(),
           g.UseRegisterOrImmediateZero(value), g.UseImmediate(index));
      return;
    }

    if (g.CanBeImmediate(index, opcode)) {
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           g.UseRegisterOrImmediateZero(value), g.UseRegister(base),
           g.UseImmediate(index));
    } else {
      InstructionOperand addr_reg = g.TempRegister();
      Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_None), addr_reg,
           g.UseRegister(index), g.UseRegister(base));
      // Emit desired store opcode, using temp addr_reg.
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           g.UseRegisterOrImmediateZero(value), addr_reg, g.TempImmediate(0));
    }
  }
}

void InstructionSelector::VisitWord32And(Node* node) {
  VisitBinop(this, node, kRiscvAnd32, true, kRiscvAnd32);
}

void InstructionSelector::VisitWord64And(Node* node) {
  RiscvOperandGenerator g(this);
  Int64BinopMatcher m(node);
  if (m.left().IsWord64Shr() && CanCover(node, m.left().node()) &&
      m.right().HasResolvedValue()) {
    uint64_t mask = m.right().ResolvedValue();
    uint32_t mask_width = base::bits::CountPopulation(mask);
    uint32_t mask_msb = base::bits::CountLeadingZeros64(mask);
    if ((mask_width != 0) && (mask_msb + mask_width == 64)) {
      // The mask must be contiguous, and occupy the least-significant bits.
      DCHECK_EQ(0u, base::bits::CountTrailingZeros64(mask));

      // Select Dext for And(Shr(x, imm), mask) where the mask is in the least
      // significant bits.
      Int64BinopMatcher mleft(m.left().node());
      if (mleft.right().HasResolvedValue()) {
        // Any shift value can match; int64 shifts use `value % 64`.
        uint32_t lsb =
            static_cast<uint32_t>(mleft.right().ResolvedValue() & 0x3F);

        // Dext cannot extract bits past the register size, however since
        // shifting the original value would have introduced some zeros we can
        // still use Dext with a smaller mask and the remaining bits will be
        // zeros.
        if (lsb + mask_width > 64) mask_width = 64 - lsb;

        if (lsb == 0 && mask_width == 64) {
          Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(mleft.left().node()));
          return;
        }
      }
      // Other cases fall through to the normal And operation.
    }
  }
  VisitBinop(this, node, kRiscvAnd, true, kRiscvAnd);
}

void InstructionSelector::VisitWord32Or(Node* node) {
  VisitBinop(this, node, kRiscvOr32, true, kRiscvOr32);
}

void InstructionSelector::VisitWord64Or(Node* node) {
  VisitBinop(this, node, kRiscvOr, true, kRiscvOr);
}

void InstructionSelector::VisitWord32Xor(Node* node) {
  Int32BinopMatcher m(node);
  if (m.left().IsWord32Or() && CanCover(node, m.left().node()) &&
      m.right().Is(-1)) {
    Int32BinopMatcher mleft(m.left().node());
    if (!mleft.right().HasResolvedValue()) {
      RiscvOperandGenerator g(this);
      Emit(kRiscvNor32, g.DefineAsRegister(node),
           g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    }
  }
  if (m.right().Is(-1)) {
    // Use Nor for bit negation and eliminate constant loading for xori.
    RiscvOperandGenerator g(this);
    Emit(kRiscvNor32, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
         g.TempImmediate(0));
    return;
  }
  VisitBinop(this, node, kRiscvXor32, true, kRiscvXor32);
}

void InstructionSelector::VisitWord64Xor(Node* node) {
  Int64BinopMatcher m(node);
  if (m.left().IsWord64Or() && CanCover(node, m.left().node()) &&
      m.right().Is(-1)) {
    Int64BinopMatcher mleft(m.left().node());
    if (!mleft.right().HasResolvedValue()) {
      RiscvOperandGenerator g(this);
      Emit(kRiscvNor, g.DefineAsRegister(node),
           g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    }
  }
  if (m.right().Is(-1)) {
    // Use Nor for bit negation and eliminate constant loading for xori.
    RiscvOperandGenerator g(this);
    Emit(kRiscvNor, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
         g.TempImmediate(0));
    return;
  }
  VisitBinop(this, node, kRiscvXor, true, kRiscvXor);
}

void InstructionSelector::VisitWord64Shl(Node* node) {
  RiscvOperandGenerator g(this);
  Int64BinopMatcher m(node);
  if ((m.left().IsChangeInt32ToInt64() || m.left().IsChangeUint32ToUint64()) &&
      m.right().IsInRange(32, 63) && CanCover(node, m.left().node())) {
    // There's no need to sign/zero-extend to 64-bit if we shift out the upper
    // 32 bits anyway.
    Emit(kRiscvShl64, g.DefineSameAsFirst(node),
         g.UseRegister(m.left().node()->InputAt(0)),
         g.UseImmediate(m.right().node()));
    return;
  }
  if (m.left().IsWord64And() && CanCover(node, m.left().node()) &&
      m.right().IsInRange(1, 63)) {
    // Match Word64Shl(Word64And(x, mask), imm) to Dshl where the mask is
    // contiguous, and the shift immediate non-zero.
    Int64BinopMatcher mleft(m.left().node());
    if (mleft.right().HasResolvedValue()) {
      uint64_t mask = mleft.right().ResolvedValue();
      uint32_t mask_width = base::bits::CountPopulation(mask);
      uint32_t mask_msb = base::bits::CountLeadingZeros64(mask);
      if ((mask_width != 0) && (mask_msb + mask_width == 64)) {
        uint64_t shift = m.right().ResolvedValue();
        DCHECK_EQ(0u, base::bits::CountTrailingZeros64(mask));
        DCHECK_NE(0u, shift);

        if ((shift + mask_width) >= 64) {
          // If the mask is contiguous and reaches or extends beyond the top
          // bit, only the shift is needed.
          Emit(kRiscvShl64, g.DefineAsRegister(node),
               g.UseRegister(mleft.left().node()),
               g.UseImmediate(m.right().node()));
          return;
        }
      }
    }
  }
  VisitRRO(this, kRiscvShl64, node);
}

void InstructionSelector::VisitWord64Shr(Node* node) {
  VisitRRO(this, kRiscvShr64, node);
}

void InstructionSelector::VisitWord64Sar(Node* node) {
  if (TryEmitExtendingLoad(this, node, node)) return;
  VisitRRO(this, kRiscvSar64, node);
}

void InstructionSelector::VisitWord32Rol(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord64Rol(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitRRO(this, kRiscvRor32, node);
}

void InstructionSelector::VisitWord32Clz(Node* node) {
  VisitRR(this, kRiscvClz32, node);
}

void InstructionSelector::VisitWord32ReverseBits(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord64ReverseBits(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord64ReverseBytes(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvByteSwap64, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitWord32ReverseBytes(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvByteSwap32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitSimd128ReverseBytes(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitWord32Ctz(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvCtz32, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitWord64Ctz(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvCtz64, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitWord32Popcnt(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvPopcnt32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitWord64Popcnt(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvPopcnt64, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitWord64Ror(Node* node) {
  VisitRRO(this, kRiscvRor64, node);
}

void InstructionSelector::VisitWord64Clz(Node* node) {
  VisitRR(this, kRiscvClz64, node);
}

void InstructionSelector::VisitInt32Add(Node* node) {
  VisitBinop(this, node, kRiscvAdd32, true, kRiscvAdd32);
}

void InstructionSelector::VisitInt64Add(Node* node) {
  VisitBinop(this, node, kRiscvAdd64, true, kRiscvAdd64);
}

void InstructionSelector::VisitInt32Sub(Node* node) {
  VisitBinop(this, node, kRiscvSub32);
}

void InstructionSelector::VisitInt64Sub(Node* node) {
  VisitBinop(this, node, kRiscvSub64);
}

void InstructionSelector::VisitInt32Mul(Node* node) {
  RiscvOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.right().HasResolvedValue() && m.right().ResolvedValue() > 0) {
    uint32_t value = static_cast<uint32_t>(m.right().ResolvedValue());
    if (base::bits::IsPowerOfTwo(value)) {
      Emit(kRiscvShl32 | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(base::bits::WhichPowerOfTwo(value)));
      return;
    }
    if (base::bits::IsPowerOfTwo(value + 1)) {
      InstructionOperand temp = g.TempRegister();
      Emit(kRiscvShl32 | AddressingModeField::encode(kMode_None), temp,
           g.UseRegister(m.left().node()),
           g.TempImmediate(base::bits::WhichPowerOfTwo(value + 1)));
      Emit(kRiscvSub32 | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), temp, g.UseRegister(m.left().node()));
      return;
    }
  }
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (CanCover(node, left) && CanCover(node, right)) {
    if (left->opcode() == IrOpcode::kWord64Sar &&
        right->opcode() == IrOpcode::kWord64Sar) {
      Int64BinopMatcher leftInput(left), rightInput(right);
      if (leftInput.right().Is(32) && rightInput.right().Is(32)) {
        // Combine untagging shifts with Dmul high.
        Emit(kRiscvMulHigh64, g.DefineSameAsFirst(node),
             g.UseRegister(leftInput.left().node()),
             g.UseRegister(rightInput.left().node()));
        return;
      }
    }
  }
  VisitRRR(this, kRiscvMul32, node);
}

void InstructionSelector::VisitInt32MulHigh(Node* node) {
  VisitRRR(this, kRiscvMulHigh32, node);
}

void InstructionSelector::VisitInt64MulHigh(Node* node) {
  VisitRRR(this, kRiscvMulHigh64, node);
}

void InstructionSelector::VisitUint32MulHigh(Node* node) {
  VisitRRR(this, kRiscvMulHighU32, node);
}

void InstructionSelector::VisitUint64MulHigh(Node* node) {
  VisitRRR(this, kRiscvMulHighU64, node);
}

void InstructionSelector::VisitInt64Mul(Node* node) {
  RiscvOperandGenerator g(this);
  Int64BinopMatcher m(node);
  // TODO(dusmil): Add optimization for shifts larger than 32.
  if (m.right().HasResolvedValue() && m.right().ResolvedValue() > 0) {
    uint64_t value = static_cast<uint64_t>(m.right().ResolvedValue());
    if (base::bits::IsPowerOfTwo(value)) {
      Emit(kRiscvShl64 | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(base::bits::WhichPowerOfTwo(value)));
      return;
    }
    if (base::bits::IsPowerOfTwo(value + 1)) {
      InstructionOperand temp = g.TempRegister();
      Emit(kRiscvShl64 | AddressingModeField::encode(kMode_None), temp,
           g.UseRegister(m.left().node()),
           g.TempImmediate(base::bits::WhichPowerOfTwo(value + 1)));
      Emit(kRiscvSub64 | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), temp, g.UseRegister(m.left().node()));
      return;
    }
  }
  Emit(kRiscvMul64, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}

void InstructionSelector::VisitInt32Div(Node* node) {
  RiscvOperandGenerator g(this);
  Int32BinopMatcher m(node);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (CanCover(node, left) && CanCover(node, right)) {
    if (left->opcode() == IrOpcode::kWord64Sar &&
        right->opcode() == IrOpcode::kWord64Sar) {
      Int64BinopMatcher rightInput(right), leftInput(left);
      if (rightInput.right().Is(32) && leftInput.right().Is(32)) {
        // Combine both shifted operands with Ddiv.
        Emit(kRiscvDiv64, g.DefineSameAsFirst(node),
             g.UseRegister(leftInput.left().node()),
             g.UseRegister(rightInput.left().node()));
        return;
      }
    }
  }
  Emit(kRiscvDiv32, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}

void InstructionSelector::VisitUint32Div(Node* node) {
  RiscvOperandGenerator g(this);
  Int32BinopMatcher m(node);
  Emit(kRiscvDivU32, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}

void InstructionSelector::VisitInt32Mod(Node* node) {
  RiscvOperandGenerator g(this);
  Int32BinopMatcher m(node);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (CanCover(node, left) && CanCover(node, right)) {
    if (left->opcode() == IrOpcode::kWord64Sar &&
        right->opcode() == IrOpcode::kWord64Sar) {
      Int64BinopMatcher rightInput(right), leftInput(left);
      if (rightInput.right().Is(32) && leftInput.right().Is(32)) {
        // Combine both shifted operands with Dmod.
        Emit(kRiscvMod64, g.DefineSameAsFirst(node),
             g.UseRegister(leftInput.left().node()),
             g.UseRegister(rightInput.left().node()));
        return;
      }
    }
  }
  Emit(kRiscvMod32, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}

void InstructionSelector::VisitUint32Mod(Node* node) {
  RiscvOperandGenerator g(this);
  Int32BinopMatcher m(node);
  Emit(kRiscvModU32, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}

void InstructionSelector::VisitInt64Div(Node* node) {
  RiscvOperandGenerator g(this);
  Int64BinopMatcher m(node);
  Emit(kRiscvDiv64, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}

void InstructionSelector::VisitUint64Div(Node* node) {
  RiscvOperandGenerator g(this);
  Int64BinopMatcher m(node);
  Emit(kRiscvDivU64, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}

void InstructionSelector::VisitInt64Mod(Node* node) {
  RiscvOperandGenerator g(this);
  Int64BinopMatcher m(node);
  Emit(kRiscvMod64, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}

void InstructionSelector::VisitUint64Mod(Node* node) {
  RiscvOperandGenerator g(this);
  Int64BinopMatcher m(node);
  Emit(kRiscvModU64, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}

void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  VisitRR(this, kRiscvCvtDS, node);
}

void InstructionSelector::VisitRoundInt32ToFloat32(Node* node) {
  VisitRR(this, kRiscvCvtSW, node);
}

void InstructionSelector::VisitRoundUint32ToFloat32(Node* node) {
  VisitRR(this, kRiscvCvtSUw, node);
}

void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  VisitRR(this, kRiscvCvtDW, node);
}

void InstructionSelector::VisitChangeInt64ToFloat64(Node* node) {
  VisitRR(this, kRiscvCvtDL, node);
}

void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  VisitRR(this, kRiscvCvtDUw, node);
}

void InstructionSelector::VisitTruncateFloat32ToInt32(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionCode opcode = kRiscvTruncWS;
  TruncateKind kind = OpParameter<TruncateKind>(node->op());
  if (kind == TruncateKind::kSetOverflowToMin) {
    opcode |= MiscField::encode(true);
  }
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitTruncateFloat32ToUint32(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionCode opcode = kRiscvTruncUwS;
  TruncateKind kind = OpParameter<TruncateKind>(node->op());
  if (kind == TruncateKind::kSetOverflowToMin) {
    opcode |= MiscField::encode(true);
  }
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  RiscvOperandGenerator g(this);
  Node* value = node->InputAt(0);
  // Match ChangeFloat64ToInt32(Float64Round##OP) to corresponding instruction
  // which does rounding and conversion to integer format.
  if (CanCover(node, value)) {
    switch (value->opcode()) {
      case IrOpcode::kFloat64RoundDown:
        Emit(kRiscvFloorWD, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      case IrOpcode::kFloat64RoundUp:
        Emit(kRiscvCeilWD, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      case IrOpcode::kFloat64RoundTiesEven:
        Emit(kRiscvRoundWD, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      case IrOpcode::kFloat64RoundTruncate:
        Emit(kRiscvTruncWD, g.DefineAsRegister(node),
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
            Emit(kRiscvFloorWS, g.DefineAsRegister(node),
                 g.UseRegister(next->InputAt(0)));
            return;
          case IrOpcode::kFloat32RoundUp:
            Emit(kRiscvCeilWS, g.DefineAsRegister(node),
                 g.UseRegister(next->InputAt(0)));
            return;
          case IrOpcode::kFloat32RoundTiesEven:
            Emit(kRiscvRoundWS, g.DefineAsRegister(node),
                 g.UseRegister(next->InputAt(0)));
            return;
          case IrOpcode::kFloat32RoundTruncate:
            Emit(kRiscvTruncWS, g.DefineAsRegister(node),
                 g.UseRegister(next->InputAt(0)));
            return;
          default:
            Emit(kRiscvTruncWS, g.DefineAsRegister(node),
                 g.UseRegister(value->InputAt(0)));
            return;
        }
      } else {
        // Match float32 -> float64 -> int32 representation change path.
        Emit(kRiscvTruncWS, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      }
    }
  }
  VisitRR(this, kRiscvTruncWD, node);
}

void InstructionSelector::VisitTryTruncateFloat64ToInt32(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  this->Emit(kRiscvTruncWD, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitTryTruncateFloat64ToUint32(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kRiscvTruncUwD, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitChangeFloat64ToInt64(Node* node) {
  VisitRR(this, kRiscvTruncLD, node);
}

void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  VisitRR(this, kRiscvTruncUwD, node);
}

void InstructionSelector::VisitChangeFloat64ToUint64(Node* node) {
  VisitRR(this, kRiscvTruncUlD, node);
}

void InstructionSelector::VisitTruncateFloat64ToUint32(Node* node) {
  VisitRR(this, kRiscvTruncUwD, node);
}

void InstructionSelector::VisitTruncateFloat64ToInt64(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionCode opcode = kRiscvTruncLD;
  TruncateKind kind = OpParameter<TruncateKind>(node->op());
  if (kind == TruncateKind::kSetOverflowToMin) {
    opcode |= MiscField::encode(true);
  }
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitTryTruncateFloat32ToInt64(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  this->Emit(kRiscvTruncLS, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitTryTruncateFloat64ToInt64(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kRiscvTruncLD, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitTryTruncateFloat32ToUint64(Node* node) {
  RiscvOperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kRiscvTruncUlS, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitTryTruncateFloat64ToUint64(Node* node) {
  RiscvOperandGenerator g(this);

  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kRiscvTruncUlD, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitBitcastWord32ToWord64(Node* node) {
  DCHECK(SmiValuesAre31Bits());
  DCHECK(COMPRESS_POINTERS_BOOL);
  RiscvOperandGenerator g(this);
  Emit(kRiscvZeroExtendWord, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void EmitSignExtendWord(InstructionSelector* selector, Node* node) {
  RiscvOperandGenerator g(selector);
  Node* value = node->InputAt(0);
  IrOpcode::Value lastOpCode = value->opcode();
  if (lastOpCode == IrOpcode::kInt32Add || lastOpCode == IrOpcode::kInt32Sub ||
      lastOpCode == IrOpcode::kWord32And || lastOpCode == IrOpcode::kWord32Or ||
      lastOpCode == IrOpcode::kWord32Xor ||
      lastOpCode == IrOpcode::kWord32Shl ||
      lastOpCode == IrOpcode::kWord32Shr ||
      lastOpCode == IrOpcode::kWord32Sar ||
      lastOpCode == IrOpcode::kUint32Mod) {
    selector->Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(value));
    return;
  }
  if (lastOpCode == IrOpcode::kInt32Mul) {
    Node* left = value->InputAt(0);
    Node* right = value->InputAt(1);
    if (selector->CanCover(value, left) && selector->CanCover(value, right)) {
      if (left->opcode() == IrOpcode::kWord64Sar &&
          right->opcode() == IrOpcode::kWord64Sar) {
        Int64BinopMatcher leftInput(left), rightInput(right);
        if (leftInput.right().Is(32) && rightInput.right().Is(32)) {
          selector->Emit(kRiscvSignExtendWord, g.DefineAsRegister(node),
                         g.UseRegister(value));
          return;
        }
      }
    }
    selector->Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(value));
    return;
  }
  if (lastOpCode == IrOpcode::kInt32Mod) {
    Node* left = value->InputAt(0);
    Node* right = value->InputAt(1);
    if (selector->CanCover(value, left) && selector->CanCover(value, right)) {
      if (left->opcode() == IrOpcode::kWord64Sar &&
          right->opcode() == IrOpcode::kWord64Sar) {
        Int64BinopMatcher rightInput(right), leftInput(left);
        if (rightInput.right().Is(32) && leftInput.right().Is(32)) {
          // Combine both shifted operands with Dmod.
          selector->Emit(kRiscvSignExtendWord, g.DefineAsRegister(node),
                         g.UseRegister(value));
          return;
        }
      }
    }
    selector->Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(value));
    return;
  }
  selector->Emit(kRiscvSignExtendWord, g.DefineAsRegister(node),
                 g.UseRegister(value));
}

void InstructionSelector::VisitChangeInt32ToInt64(Node* node) {
  Node* value = node->InputAt(0);
  if ((value->opcode() == IrOpcode::kLoad ||
       value->opcode() == IrOpcode::kLoadImmutable) &&
      CanCover(node, value)) {
    // Generate sign-extending load.
    LoadRepresentation load_rep = LoadRepresentationOf(value->op());
    InstructionCode opcode = kArchNop;
    switch (load_rep.representation()) {
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = load_rep.IsUnsigned() ? kRiscvLbu : kRiscvLb;
        break;
      case MachineRepresentation::kWord16:
        opcode = load_rep.IsUnsigned() ? kRiscvLhu : kRiscvLh;
        break;
      case MachineRepresentation::kWord32:
        opcode = kRiscvLw;
        break;
      default:
        UNREACHABLE();
    }
    EmitLoad(this, value, opcode, node);
  } else {
    EmitSignExtendWord(this, node);
  }
}

bool InstructionSelector::ZeroExtendsWord32ToWord64NoPhis(Node* node) {
  DCHECK_NE(node->opcode(), IrOpcode::kPhi);
  if (node->opcode() == IrOpcode::kLoad ||
      node->opcode() == IrOpcode::kLoadImmutable) {
    LoadRepresentation load_rep = LoadRepresentationOf(node->op());
    if (load_rep.IsUnsigned()) {
      switch (load_rep.representation()) {
        case MachineRepresentation::kWord8:
        case MachineRepresentation::kWord16:
          return true;
        default:
          return false;
      }
    }
  }

  // All other 32-bit operations sign-extend to the upper 32 bits
  return false;
}

void InstructionSelector::VisitChangeUint32ToUint64(Node* node) {
  RiscvOperandGenerator g(this);
  Node* value = node->InputAt(0);
  if (ZeroExtendsWord32ToWord64(value)) {
    Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(value));
    return;
  }
  Emit(kRiscvZeroExtendWord, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  RiscvOperandGenerator g(this);
  Node* value = node->InputAt(0);
  if (CanCover(node, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord64Sar: {
        if (CanCover(value, value->InputAt(0)) &&
            TryEmitExtendingLoad(this, value, node)) {
          return;
        } else {
          Int64BinopMatcher m(value);
          if (m.right().IsInRange(32, 63)) {
            // After smi untagging no need for truncate. Combine sequence.
            Emit(kRiscvSar64, g.DefineSameAsFirst(node),
                 g.UseRegister(m.left().node()),
                 g.UseImmediate(m.right().node()));
            return;
          }
        }
        break;
      }
      default:
        break;
    }
  }

  // Semantics of this machine IR is not clear. For example, x86 zero-extend the
  // truncated value; arm treats it as nop thus the upper 32-bit as undefined;
  // Riscv emits ext instruction which zero-extend the 32-bit value; for riscv,
  // we do sign-extension of the truncated value
  EmitSignExtendWord(this, node);
}

void InstructionSelector::VisitRoundInt64ToFloat32(Node* node) {
  VisitRR(this, kRiscvCvtSL, node);
}

void InstructionSelector::VisitRoundInt64ToFloat64(Node* node) {
  VisitRR(this, kRiscvCvtDL, node);
}

void InstructionSelector::VisitRoundUint64ToFloat32(Node* node) {
  VisitRR(this, kRiscvCvtSUl, node);
}

void InstructionSelector::VisitRoundUint64ToFloat64(Node* node) {
  VisitRR(this, kRiscvCvtDUl, node);
}

void InstructionSelector::VisitBitcastFloat32ToInt32(Node* node) {
  VisitRR(this, kRiscvBitcastFloat32ToInt32, node);
}

void InstructionSelector::VisitBitcastFloat64ToInt64(Node* node) {
  VisitRR(this, kRiscvBitcastDL, node);
}

void InstructionSelector::VisitBitcastInt32ToFloat32(Node* node) {
  VisitRR(this, kRiscvBitcastInt32ToFloat32, node);
}

void InstructionSelector::VisitBitcastInt64ToFloat64(Node* node) {
  VisitRR(this, kRiscvBitcastLD, node);
}

void InstructionSelector::VisitFloat64RoundDown(Node* node) {
  VisitRR(this, kRiscvFloat64RoundDown, node);
}

void InstructionSelector::VisitFloat32RoundUp(Node* node) {
  VisitRR(this, kRiscvFloat32RoundUp, node);
}

void InstructionSelector::VisitFloat64RoundUp(Node* node) {
  VisitRR(this, kRiscvFloat64RoundUp, node);
}

void InstructionSelector::VisitFloat32RoundTruncate(Node* node) {
  VisitRR(this, kRiscvFloat32RoundTruncate, node);
}

void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  VisitRR(this, kRiscvFloat64RoundTruncate, node);
}

void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitFloat32RoundTiesEven(Node* node) {
  VisitRR(this, kRiscvFloat32RoundTiesEven, node);
}

void InstructionSelector::VisitFloat64RoundTiesEven(Node* node) {
  VisitRR(this, kRiscvFloat64RoundTiesEven, node);
}

void InstructionSelector::VisitFloat32Neg(Node* node) {
  VisitRR(this, kRiscvNegS, node);
}

void InstructionSelector::VisitFloat64Neg(Node* node) {
  VisitRR(this, kRiscvNegD, node);
}

void InstructionSelector::VisitFloat64Ieee754Binop(Node* node,
                                                   InstructionCode opcode) {
  RiscvOperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, fa0), g.UseFixed(node->InputAt(0), fa0),
       g.UseFixed(node->InputAt(1), fa1))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat64Ieee754Unop(Node* node,
                                                  InstructionCode opcode) {
  RiscvOperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, fa0), g.UseFixed(node->InputAt(0), fa1))
      ->MarkAsCall();
}

void InstructionSelector::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* call_descriptor,
    Node* node) {
  RiscvOperandGenerator g(this);

  // Prepare for C function call.
  if (call_descriptor->IsCFunctionCall()) {
    Emit(kArchPrepareCallCFunction | MiscField::encode(static_cast<int>(
                                         call_descriptor->ParameterCount())),
         0, nullptr, 0, nullptr);

    // Poke any stack arguments.
    int slot = kCArgSlotCount;
    for (PushParameter input : (*arguments)) {
      Emit(kRiscvStoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
           g.TempImmediate(slot << kSystemPointerSizeLog2));
      ++slot;
    }
  } else {
    int push_count = static_cast<int>(call_descriptor->ParameterSlotCount());
    if (push_count > 0) {
      // Calculate needed space
      int stack_size = 0;
      for (PushParameter input : (*arguments)) {
        if (input.node) {
          stack_size += input.location.GetSizeInPointers();
        }
      }
      Emit(kRiscvStackClaim, g.NoOutput(),
           g.TempImmediate(stack_size << kSystemPointerSizeLog2));
    }
    for (size_t n = 0; n < arguments->size(); ++n) {
      PushParameter input = (*arguments)[n];
      if (input.node) {
        Emit(kRiscvStoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
             g.TempImmediate(static_cast<int>(n << kSystemPointerSizeLog2)));
      }
    }
  }
}

void InstructionSelector::VisitUnalignedLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  RiscvOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  ArchOpcode opcode;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kRiscvULoadFloat;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kRiscvULoadDouble;
      break;
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsUnsigned() ? kRiscvLbu : kRiscvLb;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsUnsigned() ? kRiscvUlhu : kRiscvUlh;
      break;
    case MachineRepresentation::kWord32:
      opcode = kRiscvUlw;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord64:
      opcode = kRiscvUld;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kRiscvRvvLd;
      break;
    case MachineRepresentation::kSimd256:            // Fall through.
    case MachineRepresentation::kBit:                // Fall through.
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:         // Fall through.
    case MachineRepresentation::kSandboxedPointer:   // Fall through.
    case MachineRepresentation::kMapWord:            // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

void InstructionSelector::VisitUnalignedStore(Node* node) {
  RiscvOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  UnalignedStoreRepresentation rep = UnalignedStoreRepresentationOf(node->op());
  ArchOpcode opcode;
  switch (rep) {
    case MachineRepresentation::kFloat32:
      opcode = kRiscvUStoreFloat;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kRiscvUStoreDouble;
      break;
    case MachineRepresentation::kWord8:
      opcode = kRiscvSb;
      break;
    case MachineRepresentation::kWord16:
      opcode = kRiscvUsh;
      break;
    case MachineRepresentation::kWord32:
      opcode = kRiscvUsw;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord64:
      opcode = kRiscvUsd;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kRiscvRvvSt;
      break;
    case MachineRepresentation::kSimd256:            // Fall through.
    case MachineRepresentation::kBit:                // Fall through.
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:         // Fall through.
    case MachineRepresentation::kSandboxedPointer:   // Fall through.
    case MachineRepresentation::kMapWord:            // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         g.UseRegister(base), g.UseImmediate(index),
         g.UseRegisterOrImmediateZero(value));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired store opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         addr_reg, g.TempImmediate(0), g.UseRegisterOrImmediateZero(value));
  }
}

namespace {

#ifndef V8_COMPRESS_POINTERS
bool IsNodeUnsigned(Node* n) {
  NodeMatcher m(n);

  if (m.IsLoad() || m.IsUnalignedLoad() || m.IsProtectedLoad()) {
    LoadRepresentation load_rep = LoadRepresentationOf(n->op());
    return load_rep.IsUnsigned();
  } else if (m.IsWord32AtomicLoad() || m.IsWord64AtomicLoad()) {
    AtomicLoadParameters atomic_load_params = AtomicLoadParametersOf(n->op());
    LoadRepresentation load_rep = atomic_load_params.representation();
    return load_rep.IsUnsigned();
  } else {
    return m.IsUint32Div() || m.IsUint32LessThan() ||
           m.IsUint32LessThanOrEqual() || m.IsUint32Mod() ||
           m.IsUint32MulHigh() || m.IsChangeFloat64ToUint32() ||
           m.IsTruncateFloat64ToUint32() || m.IsTruncateFloat32ToUint32();
  }
}
#endif

// Shared routine for multiple word compare operations.
void VisitFullWord32Compare(InstructionSelector* selector, Node* node,
                            InstructionCode opcode, FlagsContinuation* cont) {
  RiscvOperandGenerator g(selector);
  InstructionOperand leftOp = g.TempRegister();
  InstructionOperand rightOp = g.TempRegister();

  selector->Emit(kRiscvShl64, leftOp, g.UseRegister(node->InputAt(0)),
                 g.TempImmediate(32));
  selector->Emit(kRiscvShl64, rightOp, g.UseRegister(node->InputAt(1)),
                 g.TempImmediate(32));

  VisitCompare(selector, opcode, leftOp, rightOp, cont);
}

#ifndef V8_COMPRESS_POINTERS
void VisitOptimizedWord32Compare(InstructionSelector* selector, Node* node,
                                 InstructionCode opcode,
                                 FlagsContinuation* cont) {
  if (v8_flags.debug_code) {
    RiscvOperandGenerator g(selector);
    InstructionOperand leftOp = g.TempRegister();
    InstructionOperand rightOp = g.TempRegister();
    InstructionOperand optimizedResult = g.TempRegister();
    InstructionOperand fullResult = g.TempRegister();
    FlagsCondition condition = cont->condition();
    InstructionCode testOpcode = opcode |
                                 FlagsConditionField::encode(condition) |
                                 FlagsModeField::encode(kFlags_set);

    selector->Emit(testOpcode, optimizedResult, g.UseRegister(node->InputAt(0)),
                   g.UseRegister(node->InputAt(1)));

    selector->Emit(kRiscvShl64, leftOp, g.UseRegister(node->InputAt(0)),
                   g.TempImmediate(32));
    selector->Emit(kRiscvShl64, rightOp, g.UseRegister(node->InputAt(1)),
                   g.TempImmediate(32));
    selector->Emit(testOpcode, fullResult, leftOp, rightOp);

    selector->Emit(kRiscvAssertEqual, g.NoOutput(), optimizedResult, fullResult,
                   g.TempImmediate(static_cast<int>(
                       AbortReason::kUnsupportedNonPrimitiveCompare)));
  }

  VisitWordCompare(selector, node, opcode, cont, false);
}
#endif
void VisitWord32Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  // RISC-V doesn't support Word32 compare instructions. Instead it relies
  // that the values in registers are correctly sign-extended and uses
  // Word64 comparison instead. This behavior is correct in most cases,
  // but doesn't work when comparing signed with unsigned operands.
  // We could simulate full Word32 compare in all cases but this would
  // create an unnecessary overhead since unsigned integers are rarely
  // used in JavaScript.
  // The solution proposed here tries to match a comparison of signed
  // with unsigned operand, and perform full Word32Compare only
  // in those cases. Unfortunately, the solution is not complete because
  // it might skip cases where Word32 full compare is needed, so
  // basically it is a hack.
  // When call to a host function in simulator, if the function return a
  // int32 value, the simulator do not sign-extended to int64 because in
  // simulator we do not know the function whether return a int32 or int64.
  // so we need do a full word32 compare in this case.
#ifndef V8_COMPRESS_POINTERS
#ifndef USE_SIMULATOR
  if (IsNodeUnsigned(node->InputAt(0)) != IsNodeUnsigned(node->InputAt(1))) {
#else
  if (IsNodeUnsigned(node->InputAt(0)) != IsNodeUnsigned(node->InputAt(1)) ||
      node->InputAt(0)->opcode() == IrOpcode::kCall ||
      node->InputAt(1)->opcode() == IrOpcode::kCall) {
#endif
    VisitFullWord32Compare(selector, node, kRiscvCmp, cont);
  } else {
    VisitOptimizedWord32Compare(selector, node, kRiscvCmp, cont);
  }
#else
  VisitFullWord32Compare(selector, node, kRiscvCmp, cont);
#endif
}

void VisitWord64Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  VisitWordCompare(selector, node, kRiscvCmp, cont, false);
}

void VisitAtomicLoad(InstructionSelector* selector, Node* node,
                     ArchOpcode opcode, AtomicWidth width) {
  RiscvOperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.DefineAsRegister(node), g.UseRegister(base),
                   g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

void VisitAtomicStore(InstructionSelector* selector, Node* node,
                      ArchOpcode opcode, AtomicWidth width) {
  RiscvOperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.NoOutput(), g.UseRegister(base), g.UseImmediate(index),
                   g.UseRegisterOrImmediateZero(value));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired store opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.NoOutput(), addr_reg, g.TempImmediate(0),
                   g.UseRegisterOrImmediateZero(value));
  }
}

void VisitAtomicBinop(InstructionSelector* selector, Node* node,
                      ArchOpcode opcode, AtomicWidth width) {
  RiscvOperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

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
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode) |
                         AtomicWidthField::encode(width);
  selector->Emit(code, 1, outputs, input_count, inputs, 4, temps);
}

}  // namespace

void InstructionSelector::VisitStackPointerGreaterThan(
    Node* node, FlagsContinuation* cont) {
  StackCheckKind kind = StackCheckKindOf(node->op());
  InstructionCode opcode =
      kArchStackPointerGreaterThan | MiscField::encode(static_cast<int>(kind));

  RiscvOperandGenerator g(this);

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

bool CanCoverTrap(Node* user, Node* value) {
  if (user->opcode() != IrOpcode::kTrapUnless &&
      user->opcode() != IrOpcode::kTrapIf)
    return true;
  if (value->opcode() == IrOpcode::kWord32Equal ||
      value->opcode() == IrOpcode::kInt32LessThanOrEqual ||
      value->opcode() == IrOpcode::kInt32LessThanOrEqual ||
      value->opcode() == IrOpcode::kUint32LessThan ||
      value->opcode() == IrOpcode::kUint32LessThanOrEqual)
    return false;
  return true;
}
// Shared routine for word comparisons against zero.
void InstructionSelector::VisitWordCompareZero(Node* user, Node* value,
                                               FlagsContinuation* cont) {
  // Try to combine with comparisons against 0 by simply inverting the branch.
  while (CanCover(user, value)) {
    if (value->opcode() == IrOpcode::kWord32Equal) {
      Int32BinopMatcher m(value);
      if (!m.right().Is(0)) break;
      user = value;
      value = m.left().node();
    } else if (value->opcode() == IrOpcode::kWord64Equal) {
      Int64BinopMatcher m(value);
      if (!m.right().Is(0)) break;
      user = value;
      value = m.left().node();
    } else {
      break;
    }

    cont->Negate();
  }

  if (CanCoverTrap(user, value) && CanCover(user, value)) {
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
      case IrOpcode::kWord64Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitWord64Compare(this, value, cont);
      case IrOpcode::kInt64LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWord64Compare(this, value, cont);
      case IrOpcode::kInt64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWord64Compare(this, value, cont);
      case IrOpcode::kUint64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWord64Compare(this, value, cont);
      case IrOpcode::kUint64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWord64Compare(this, value, cont);
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
          if (result == nullptr || IsDefined(result)) {
            switch (node->opcode()) {
              case IrOpcode::kInt32AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kRiscvAdd64, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kRiscvSub64, cont);
              case IrOpcode::kInt32MulWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kRiscvMulOvf32, cont);
              case IrOpcode::kInt64AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kRiscvAddOvf64, cont);
              case IrOpcode::kInt64SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kRiscvSubOvf64, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kWord32And:
      case IrOpcode::kWord64And:
        return VisitWordCompare(this, value, kRiscvTst, cont, true);
      case IrOpcode::kStackPointerGreaterThan:
        cont->OverwriteAndNegateIfEqual(kStackPointerGreaterThanCondition);
        return VisitStackPointerGreaterThan(value, cont);
      default:
        break;
    }
  }

  // Continuation could not be combined with a compare, emit compare against 0.
  EmitWordCompareZero(this, value, cont);
}

void InstructionSelector::VisitWord32Equal(Node* const node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) {
    return VisitWordCompareZero(m.node(), m.left().node(), &cont);
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

void InstructionSelector::VisitInt32AddWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kRiscvAdd64, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kRiscvAdd64, &cont);
}

void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kRiscvSub64, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kRiscvSub64, &cont);
}

void InstructionSelector::VisitInt32MulWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kRiscvMulOvf32, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kRiscvMulOvf32, &cont);
}

void InstructionSelector::VisitInt64AddWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kRiscvAddOvf64, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kRiscvAddOvf64, &cont);
}

void InstructionSelector::VisitInt64SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kRiscvSubOvf64, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kRiscvSubOvf64, &cont);
}

void InstructionSelector::VisitInt64MulWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    // RISCV64 doesn't set the overflow flag for multiplication, so we need to
    // test on kNotEqual. Here is the code sequence used:
    //   mulh rdh, left, right
    //   mul rdl, left, right
    //   srai temp, rdl, 63
    //   xor overflow, rdl, temp
    FlagsContinuation cont = FlagsContinuation::ForSet(kNotEqual, ovf);
    return VisitBinop(this, node, kRiscvMulOvf64, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kRiscvMulOvf64, &cont);
}

void InstructionSelector::VisitWord64Equal(Node* const node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int64BinopMatcher m(node);
  if (m.right().Is(0)) {
    return VisitWordCompareZero(m.node(), m.left().node(), &cont);
  }

  VisitWord64Compare(this, node, &cont);
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

void InstructionSelector::VisitWord32AtomicLoad(Node* node) {
  AtomicLoadParameters atomic_load_params = AtomicLoadParametersOf(node->op());
  LoadRepresentation load_rep = atomic_load_params.representation();
  ArchOpcode opcode;
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
  }
  VisitAtomicLoad(this, node, opcode, AtomicWidth::kWord32);
}

void InstructionSelector::VisitWord32AtomicStore(Node* node) {
  AtomicStoreParameters store_params = AtomicStoreParametersOf(node->op());
  MachineRepresentation rep = store_params.representation();
  ArchOpcode opcode;
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
  }

  VisitAtomicStore(this, node, opcode, AtomicWidth::kWord32);
}

void InstructionSelector::VisitWord64AtomicLoad(Node* node) {
  AtomicLoadParameters atomic_load_params = AtomicLoadParametersOf(node->op());
  LoadRepresentation load_rep = atomic_load_params.representation();
  ArchOpcode opcode;
  switch (load_rep.representation()) {
    case MachineRepresentation::kWord8:
      opcode = kAtomicLoadUint8;
      break;
    case MachineRepresentation::kWord16:
      opcode = kAtomicLoadUint16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kAtomicLoadWord32;
      break;
    case MachineRepresentation::kWord64:
      opcode = kRiscvWord64AtomicLoadUint64;
      break;
#ifdef V8_COMPRESS_POINTERS
    case MachineRepresentation::kTaggedSigned:
      opcode = kRiscv64LdDecompressTaggedSigned;
      break;
    case MachineRepresentation::kTaggedPointer:
      opcode = kRiscv64LdDecompressTagged;
      break;
    case MachineRepresentation::kTagged:
      opcode = kRiscv64LdDecompressTagged;
      break;
#else
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:
      if (kTaggedSize == 8) {
        opcode = kRiscvWord64AtomicLoadUint64;
      } else {
        opcode = kAtomicLoadWord32;
      }
      break;
#endif
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:
      DCHECK(COMPRESS_POINTERS_BOOL);
      opcode = kAtomicLoadWord32;
      break;
    default:
      UNREACHABLE();
  }
  VisitAtomicLoad(this, node, opcode, AtomicWidth::kWord64);
}

void InstructionSelector::VisitWord64AtomicStore(Node* node) {
  AtomicStoreParameters store_params = AtomicStoreParametersOf(node->op());
  MachineRepresentation rep = store_params.representation();
  ArchOpcode opcode;
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
    case MachineRepresentation::kWord64:
      opcode = kRiscvWord64AtomicStoreWord64;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:
      opcode = kRiscvWord64AtomicStoreWord64;
      break;
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:
      CHECK(COMPRESS_POINTERS_BOOL);
      opcode = kAtomicStoreWord32;
      break;
    default:
      UNREACHABLE();
  }

  VisitAtomicStore(this, node, opcode, AtomicWidth::kWord64);
}

void InstructionSelector::VisitWord32AtomicExchange(Node* node) {
  ArchOpcode opcode;
  MachineType type = AtomicOpType(node->op());
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
  }

  VisitAtomicExchange(this, node, opcode, AtomicWidth::kWord32);
}

void InstructionSelector::VisitWord64AtomicExchange(Node* node) {
  ArchOpcode opcode;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Uint8()) {
    opcode = kAtomicExchangeUint8;
  } else if (type == MachineType::Uint16()) {
    opcode = kAtomicExchangeUint16;
  } else if (type == MachineType::Uint32()) {
    opcode = kAtomicExchangeWord32;
  } else if (type == MachineType::Uint64()) {
    opcode = kRiscvWord64AtomicExchangeUint64;
  } else {
    UNREACHABLE();
  }
  VisitAtomicExchange(this, node, opcode, AtomicWidth::kWord64);
}

void InstructionSelector::VisitWord32AtomicCompareExchange(Node* node) {
  ArchOpcode opcode;
  MachineType type = AtomicOpType(node->op());
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
  }

  VisitAtomicCompareExchange(this, node, opcode, AtomicWidth::kWord32);
}

void InstructionSelector::VisitWord64AtomicCompareExchange(Node* node) {
  ArchOpcode opcode;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Uint8()) {
    opcode = kAtomicCompareExchangeUint8;
  } else if (type == MachineType::Uint16()) {
    opcode = kAtomicCompareExchangeUint16;
  } else if (type == MachineType::Uint32()) {
    opcode = kAtomicCompareExchangeWord32;
  } else if (type == MachineType::Uint64()) {
    opcode = kRiscvWord64AtomicCompareExchangeUint64;
  } else {
    UNREACHABLE();
  }
  VisitAtomicCompareExchange(this, node, opcode, AtomicWidth::kWord64);
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

  VisitAtomicBinop(this, node, opcode, AtomicWidth::kWord32);
}

#define VISIT_ATOMIC_BINOP(op)                                           \
  void InstructionSelector::VisitWord32Atomic##op(Node* node) {          \
    VisitWord32AtomicBinaryOperation(                                    \
        node, kAtomic##op##Int8, kAtomic##op##Uint8, kAtomic##op##Int16, \
        kAtomic##op##Uint16, kAtomic##op##Word32);                       \
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
  VisitAtomicBinop(this, node, opcode, AtomicWidth::kWord64);
}

#define VISIT_ATOMIC_BINOP(op)                                                 \
  void InstructionSelector::VisitWord64Atomic##op(Node* node) {                \
    VisitWord64AtomicBinaryOperation(node, kAtomic##op##Uint8,                 \
                                     kAtomic##op##Uint16, kAtomic##op##Word32, \
                                     kRiscvWord64Atomic##op##Uint64);          \
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

void InstructionSelector::VisitSignExtendWord8ToInt64(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvSignExtendByte, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitSignExtendWord16ToInt64(Node* node) {
  RiscvOperandGenerator g(this);
  Emit(kRiscvSignExtendShort, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitSignExtendWord32ToInt64(Node* node) {
  EmitSignExtendWord(this, node);
}

// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags = MachineOperatorBuilder::kNoFlags;
  return flags | MachineOperatorBuilder::kWord32Ctz |
         MachineOperatorBuilder::kWord64Ctz |
         MachineOperatorBuilder::kWord32Popcnt |
         MachineOperatorBuilder::kWord64Popcnt |
         MachineOperatorBuilder::kWord32ShiftIsSafe |
         MachineOperatorBuilder::kInt32DivIsSafe |
         MachineOperatorBuilder::kUint32DivIsSafe |
         MachineOperatorBuilder::kFloat64RoundDown |
         MachineOperatorBuilder::kFloat32RoundDown |
         MachineOperatorBuilder::kFloat64RoundUp |
         MachineOperatorBuilder::kFloat32RoundUp |
         MachineOperatorBuilder::kFloat64RoundTruncate |
         MachineOperatorBuilder::kFloat32RoundTruncate |
         MachineOperatorBuilder::kFloat64RoundTiesEven |
         MachineOperatorBuilder::kFloat32RoundTiesEven;
}
#undef TRACE_UNIMPL
#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
