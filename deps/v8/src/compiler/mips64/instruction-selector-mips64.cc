// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/adapters.h"
#include "src/base/bits.h"
#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE_UNIMPL() \
  PrintF("UNIMPLEMENTED instr_sel: %s at line %d\n", __FUNCTION__, __LINE__)

#define TRACE() PrintF("instr_sel: %s at line %d\n", __FUNCTION__, __LINE__)


// Adds Mips-specific methods for generating InstructionOperands.
class Mips64OperandGenerator final : public OperandGenerator {
 public:
  explicit Mips64OperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand UseOperand(Node* node, InstructionCode opcode) {
    if (CanBeImmediate(node, opcode)) {
      return UseImmediate(node);
    }
    return UseRegister(node);
  }

  bool CanBeImmediate(Node* node, InstructionCode opcode) {
    int64_t value;
    if (node->opcode() == IrOpcode::kInt32Constant)
      value = OpParameter<int32_t>(node);
    else if (node->opcode() == IrOpcode::kInt64Constant)
      value = OpParameter<int64_t>(node);
    else
      return false;
    switch (ArchOpcodeField::decode(opcode)) {
      case kMips64Shl:
      case kMips64Sar:
      case kMips64Shr:
        return is_uint5(value);
      case kMips64Dshl:
      case kMips64Dsar:
      case kMips64Dshr:
        return is_uint6(value);
      case kMips64Xor:
        return is_uint16(value);
      case kMips64Ldc1:
      case kMips64Sdc1:
        return is_int16(value + kIntSize);
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


static void VisitRR(InstructionSelector* selector, ArchOpcode opcode,
                    Node* node) {
  Mips64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}


static void VisitRRR(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node) {
  Mips64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
}


static void VisitRRO(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node) {
  Mips64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseOperand(node->InputAt(1), opcode));
}


static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, FlagsContinuation* cont) {
  Mips64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand inputs[4];
  size_t input_count = 0;
  InstructionOperand outputs[2];
  size_t output_count = 0;

  inputs[input_count++] = g.UseRegister(m.left().node());
  inputs[input_count++] = g.UseOperand(m.right().node(), opcode);

  if (cont->IsBranch()) {
    inputs[input_count++] = g.Label(cont->true_block());
    inputs[input_count++] = g.Label(cont->false_block());
  }

  if (cont->IsDeoptimize()) {
    // If we can deoptimize as a result of the binop, we need to make sure that
    // the deopt inputs are not overwritten by the binop result. One way
    // to achieve that is to declare the output register as same-as-first.
    outputs[output_count++] = g.DefineSameAsFirst(node);
  } else {
    outputs[output_count++] = g.DefineAsRegister(node);
  }
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


static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode) {
  FlagsContinuation cont;
  VisitBinop(selector, node, opcode, &cont);
}

void EmitLoad(InstructionSelector* selector, Node* node, InstructionCode opcode,
              Node* output = nullptr) {
  Mips64OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(output == nullptr ? node : output),
                   g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kMips64Dadd | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(output == nullptr ? node : output),
                   addr_reg, g.TempImmediate(0));
  }
}

void InstructionSelector::VisitLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());

  ArchOpcode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kMips64Lwc1;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kMips64Ldc1;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsUnsigned() ? kMips64Lbu : kMips64Lb;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsUnsigned() ? kMips64Lhu : kMips64Lh;
      break;
    case MachineRepresentation::kWord32:
      opcode = load_rep.IsUnsigned() ? kMips64Lwu : kMips64Lw;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:  // Fall through.
    case MachineRepresentation::kWord64:
      opcode = kMips64Ld;
      break;
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }

  EmitLoad(this, node, opcode);
}

void InstructionSelector::VisitProtectedLoad(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelector::VisitStore(Node* node) {
  Mips64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());
  WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();
  MachineRepresentation rep = store_rep.representation();

  // TODO(mips): I guess this could be done in a better way.
  if (write_barrier_kind != kNoWriteBarrier) {
    DCHECK(CanBeTaggedPointer(rep));
    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    inputs[input_count++] = g.UseUniqueRegister(index);
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
    code |= MiscField::encode(static_cast<int>(record_write_mode));
    Emit(code, 0, nullptr, input_count, inputs, temp_count, temps);
  } else {
    ArchOpcode opcode = kArchNop;
    switch (rep) {
      case MachineRepresentation::kFloat32:
        opcode = kMips64Swc1;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kMips64Sdc1;
        break;
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = kMips64Sb;
        break;
      case MachineRepresentation::kWord16:
        opcode = kMips64Sh;
        break;
      case MachineRepresentation::kWord32:
        opcode = kMips64Sw;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:  // Fall through.
      case MachineRepresentation::kWord64:
        opcode = kMips64Sd;
        break;
      case MachineRepresentation::kSimd128:  // Fall through.
      case MachineRepresentation::kNone:
        UNREACHABLE();
        return;
    }

    if (g.CanBeImmediate(index, opcode)) {
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           g.UseRegister(base), g.UseImmediate(index), g.UseRegister(value));
    } else {
      InstructionOperand addr_reg = g.TempRegister();
      Emit(kMips64Dadd | AddressingModeField::encode(kMode_None), addr_reg,
           g.UseRegister(index), g.UseRegister(base));
      // Emit desired store opcode, using temp addr_reg.
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           addr_reg, g.TempImmediate(0), g.UseRegister(value));
    }
  }
}


void InstructionSelector::VisitWord32And(Node* node) {
  Mips64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().IsWord32Shr() && CanCover(node, m.left().node()) &&
      m.right().HasValue()) {
    uint32_t mask = m.right().Value();
    uint32_t mask_width = base::bits::CountPopulation32(mask);
    uint32_t mask_msb = base::bits::CountLeadingZeros32(mask);
    if ((mask_width != 0) && (mask_msb + mask_width == 32)) {
      // The mask must be contiguous, and occupy the least-significant bits.
      DCHECK_EQ(0u, base::bits::CountTrailingZeros32(mask));

      // Select Ext for And(Shr(x, imm), mask) where the mask is in the least
      // significant bits.
      Int32BinopMatcher mleft(m.left().node());
      if (mleft.right().HasValue()) {
        // Any shift value can match; int32 shifts use `value % 32`.
        uint32_t lsb = mleft.right().Value() & 0x1f;

        // Ext cannot extract bits past the register size, however since
        // shifting the original value would have introduced some zeros we can
        // still use Ext with a smaller mask and the remaining bits will be
        // zeros.
        if (lsb + mask_width > 32) mask_width = 32 - lsb;

        Emit(kMips64Ext, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(lsb),
             g.TempImmediate(mask_width));
        return;
      }
      // Other cases fall through to the normal And operation.
    }
  }
  if (m.right().HasValue()) {
    uint32_t mask = m.right().Value();
    uint32_t shift = base::bits::CountPopulation32(~mask);
    uint32_t msb = base::bits::CountLeadingZeros32(~mask);
    if (shift != 0 && shift != 32 && msb + shift == 32) {
      // Insert zeros for (x >> K) << K => x & ~(2^K - 1) expression reduction
      // and remove constant loading of inverted mask.
      Emit(kMips64Ins, g.DefineSameAsFirst(node),
           g.UseRegister(m.left().node()), g.TempImmediate(0),
           g.TempImmediate(shift));
      return;
    }
  }
  VisitBinop(this, node, kMips64And32);
}


void InstructionSelector::VisitWord64And(Node* node) {
  Mips64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  if (m.left().IsWord64Shr() && CanCover(node, m.left().node()) &&
      m.right().HasValue()) {
    uint64_t mask = m.right().Value();
    uint32_t mask_width = base::bits::CountPopulation64(mask);
    uint32_t mask_msb = base::bits::CountLeadingZeros64(mask);
    if ((mask_width != 0) && (mask_msb + mask_width == 64)) {
      // The mask must be contiguous, and occupy the least-significant bits.
      DCHECK_EQ(0u, base::bits::CountTrailingZeros64(mask));

      // Select Dext for And(Shr(x, imm), mask) where the mask is in the least
      // significant bits.
      Int64BinopMatcher mleft(m.left().node());
      if (mleft.right().HasValue()) {
        // Any shift value can match; int64 shifts use `value % 64`.
        uint32_t lsb = static_cast<uint32_t>(mleft.right().Value() & 0x3f);

        // Dext cannot extract bits past the register size, however since
        // shifting the original value would have introduced some zeros we can
        // still use Dext with a smaller mask and the remaining bits will be
        // zeros.
        if (lsb + mask_width > 64) mask_width = 64 - lsb;

        Emit(kMips64Dext, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(lsb),
             g.TempImmediate(static_cast<int32_t>(mask_width)));
        return;
      }
      // Other cases fall through to the normal And operation.
    }
  }
  if (m.right().HasValue()) {
    uint64_t mask = m.right().Value();
    uint32_t shift = base::bits::CountPopulation64(~mask);
    uint32_t msb = base::bits::CountLeadingZeros64(~mask);
    if (shift != 0 && shift < 32 && msb + shift == 64) {
      // Insert zeros for (x >> K) << K => x & ~(2^K - 1) expression reduction
      // and remove constant loading of inverted mask. Dins cannot insert bits
      // past word size, so shifts smaller than 32 are covered.
      Emit(kMips64Dins, g.DefineSameAsFirst(node),
           g.UseRegister(m.left().node()), g.TempImmediate(0),
           g.TempImmediate(shift));
      return;
    }
  }
  VisitBinop(this, node, kMips64And);
}


void InstructionSelector::VisitWord32Or(Node* node) {
  VisitBinop(this, node, kMips64Or32);
}


void InstructionSelector::VisitWord64Or(Node* node) {
  VisitBinop(this, node, kMips64Or);
}


void InstructionSelector::VisitWord32Xor(Node* node) {
  Int32BinopMatcher m(node);
  if (m.left().IsWord32Or() && CanCover(node, m.left().node()) &&
      m.right().Is(-1)) {
    Int32BinopMatcher mleft(m.left().node());
    if (!mleft.right().HasValue()) {
      Mips64OperandGenerator g(this);
      Emit(kMips64Nor32, g.DefineAsRegister(node),
           g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    }
  }
  if (m.right().Is(-1)) {
    // Use Nor for bit negation and eliminate constant loading for xori.
    Mips64OperandGenerator g(this);
    Emit(kMips64Nor32, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
         g.TempImmediate(0));
    return;
  }
  VisitBinop(this, node, kMips64Xor32);
}


void InstructionSelector::VisitWord64Xor(Node* node) {
  Int64BinopMatcher m(node);
  if (m.left().IsWord64Or() && CanCover(node, m.left().node()) &&
      m.right().Is(-1)) {
    Int64BinopMatcher mleft(m.left().node());
    if (!mleft.right().HasValue()) {
      Mips64OperandGenerator g(this);
      Emit(kMips64Nor, g.DefineAsRegister(node),
           g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    }
  }
  if (m.right().Is(-1)) {
    // Use Nor for bit negation and eliminate constant loading for xori.
    Mips64OperandGenerator g(this);
    Emit(kMips64Nor, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
         g.TempImmediate(0));
    return;
  }
  VisitBinop(this, node, kMips64Xor);
}


void InstructionSelector::VisitWord32Shl(Node* node) {
  Int32BinopMatcher m(node);
  if (m.left().IsWord32And() && CanCover(node, m.left().node()) &&
      m.right().IsInRange(1, 31)) {
    Mips64OperandGenerator g(this);
    Int32BinopMatcher mleft(m.left().node());
    // Match Word32Shl(Word32And(x, mask), imm) to Shl where the mask is
    // contiguous, and the shift immediate non-zero.
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
          Emit(kMips64Shl, g.DefineAsRegister(node),
               g.UseRegister(mleft.left().node()),
               g.UseImmediate(m.right().node()));
          return;
        }
      }
    }
  }
  VisitRRO(this, kMips64Shl, node);
}


void InstructionSelector::VisitWord32Shr(Node* node) {
  Int32BinopMatcher m(node);
  if (m.left().IsWord32And() && m.right().HasValue()) {
    uint32_t lsb = m.right().Value() & 0x1f;
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      // Select Ext for Shr(And(x, mask), imm) where the result of the mask is
      // shifted into the least-significant bits.
      uint32_t mask = (mleft.right().Value() >> lsb) << lsb;
      unsigned mask_width = base::bits::CountPopulation32(mask);
      unsigned mask_msb = base::bits::CountLeadingZeros32(mask);
      if ((mask_msb + mask_width + lsb) == 32) {
        Mips64OperandGenerator g(this);
        DCHECK_EQ(lsb, base::bits::CountTrailingZeros32(mask));
        Emit(kMips64Ext, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(lsb),
             g.TempImmediate(mask_width));
        return;
      }
    }
  }
  VisitRRO(this, kMips64Shr, node);
}


void InstructionSelector::VisitWord32Sar(Node* node) {
  Int32BinopMatcher m(node);
  if (m.left().IsWord32Shl() && CanCover(node, m.left().node())) {
    Int32BinopMatcher mleft(m.left().node());
    if (m.right().HasValue() && mleft.right().HasValue()) {
      Mips64OperandGenerator g(this);
      uint32_t sar = m.right().Value();
      uint32_t shl = mleft.right().Value();
      if ((sar == shl) && (sar == 16)) {
        Emit(kMips64Seh, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()));
        return;
      } else if ((sar == shl) && (sar == 24)) {
        Emit(kMips64Seb, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()));
        return;
      } else if ((sar == shl) && (sar == 32)) {
        Emit(kMips64Shl, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(0));
        return;
      }
    }
  }
  VisitRRO(this, kMips64Sar, node);
}


void InstructionSelector::VisitWord64Shl(Node* node) {
  Mips64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  if ((m.left().IsChangeInt32ToInt64() || m.left().IsChangeUint32ToUint64()) &&
      m.right().IsInRange(32, 63) && CanCover(node, m.left().node())) {
    // There's no need to sign/zero-extend to 64-bit if we shift out the upper
    // 32 bits anyway.
    Emit(kMips64Dshl, g.DefineSameAsFirst(node),
         g.UseRegister(m.left().node()->InputAt(0)),
         g.UseImmediate(m.right().node()));
    return;
  }
  if (m.left().IsWord64And() && CanCover(node, m.left().node()) &&
      m.right().IsInRange(1, 63)) {
    // Match Word64Shl(Word64And(x, mask), imm) to Dshl where the mask is
    // contiguous, and the shift immediate non-zero.
    Int64BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      uint64_t mask = mleft.right().Value();
      uint32_t mask_width = base::bits::CountPopulation64(mask);
      uint32_t mask_msb = base::bits::CountLeadingZeros64(mask);
      if ((mask_width != 0) && (mask_msb + mask_width == 64)) {
        uint64_t shift = m.right().Value();
        DCHECK_EQ(0u, base::bits::CountTrailingZeros64(mask));
        DCHECK_NE(0u, shift);

        if ((shift + mask_width) >= 64) {
          // If the mask is contiguous and reaches or extends beyond the top
          // bit, only the shift is needed.
          Emit(kMips64Dshl, g.DefineAsRegister(node),
               g.UseRegister(mleft.left().node()),
               g.UseImmediate(m.right().node()));
          return;
        }
      }
    }
  }
  VisitRRO(this, kMips64Dshl, node);
}


void InstructionSelector::VisitWord64Shr(Node* node) {
  Int64BinopMatcher m(node);
  if (m.left().IsWord64And() && m.right().HasValue()) {
    uint32_t lsb = m.right().Value() & 0x3f;
    Int64BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      // Select Dext for Shr(And(x, mask), imm) where the result of the mask is
      // shifted into the least-significant bits.
      uint64_t mask = (mleft.right().Value() >> lsb) << lsb;
      unsigned mask_width = base::bits::CountPopulation64(mask);
      unsigned mask_msb = base::bits::CountLeadingZeros64(mask);
      if ((mask_msb + mask_width + lsb) == 64) {
        Mips64OperandGenerator g(this);
        DCHECK_EQ(lsb, base::bits::CountTrailingZeros64(mask));
        Emit(kMips64Dext, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(lsb),
             g.TempImmediate(mask_width));
        return;
      }
    }
  }
  VisitRRO(this, kMips64Dshr, node);
}


void InstructionSelector::VisitWord64Sar(Node* node) {
  VisitRRO(this, kMips64Dsar, node);
}


void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitRRO(this, kMips64Ror, node);
}


void InstructionSelector::VisitWord32Clz(Node* node) {
  VisitRR(this, kMips64Clz, node);
}


void InstructionSelector::VisitWord32ReverseBits(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitWord64ReverseBits(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord64ReverseBytes(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64ByteSwap64, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitWord32ReverseBytes(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64ByteSwap32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitWord32Ctz(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64Ctz, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitWord64Ctz(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64Dctz, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitWord32Popcnt(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64Popcnt, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitWord64Popcnt(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64Dpopcnt, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitWord64Ror(Node* node) {
  VisitRRO(this, kMips64Dror, node);
}


void InstructionSelector::VisitWord64Clz(Node* node) {
  VisitRR(this, kMips64Dclz, node);
}


void InstructionSelector::VisitInt32Add(Node* node) {
  Mips64OperandGenerator g(this);
  Int32BinopMatcher m(node);

  // Select Lsa for (left + (left_of_right << imm)).
  if (m.right().opcode() == IrOpcode::kWord32Shl &&
      CanCover(node, m.left().node()) && CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    if (mright.right().HasValue()) {
      int32_t shift_value = static_cast<int32_t>(mright.right().Value());
      Emit(kMips64Lsa, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.UseRegister(mright.left().node()), g.TempImmediate(shift_value));
      return;
    }
  }

  // Select Lsa for ((left_of_left << imm) + right).
  if (m.left().opcode() == IrOpcode::kWord32Shl &&
      CanCover(node, m.right().node()) && CanCover(node, m.left().node())) {
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      int32_t shift_value = static_cast<int32_t>(mleft.right().Value());
      Emit(kMips64Lsa, g.DefineAsRegister(node),
           g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
           g.TempImmediate(shift_value));
      return;
    }
  }
  VisitBinop(this, node, kMips64Add);
}


void InstructionSelector::VisitInt64Add(Node* node) {
  Mips64OperandGenerator g(this);
  Int64BinopMatcher m(node);

  // Select Dlsa for (left + (left_of_right << imm)).
  if (m.right().opcode() == IrOpcode::kWord64Shl &&
      CanCover(node, m.left().node()) && CanCover(node, m.right().node())) {
    Int64BinopMatcher mright(m.right().node());
    if (mright.right().HasValue()) {
      int32_t shift_value = static_cast<int32_t>(mright.right().Value());
      Emit(kMips64Dlsa, g.DefineAsRegister(node),
           g.UseRegister(m.left().node()), g.UseRegister(mright.left().node()),
           g.TempImmediate(shift_value));
      return;
    }
  }

  // Select Dlsa for ((left_of_left << imm) + right).
  if (m.left().opcode() == IrOpcode::kWord64Shl &&
      CanCover(node, m.right().node()) && CanCover(node, m.left().node())) {
    Int64BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      int32_t shift_value = static_cast<int32_t>(mleft.right().Value());
      Emit(kMips64Dlsa, g.DefineAsRegister(node),
           g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
           g.TempImmediate(shift_value));
      return;
    }
  }

  VisitBinop(this, node, kMips64Dadd);
}


void InstructionSelector::VisitInt32Sub(Node* node) {
  VisitBinop(this, node, kMips64Sub);
}


void InstructionSelector::VisitInt64Sub(Node* node) {
  VisitBinop(this, node, kMips64Dsub);
}


void InstructionSelector::VisitInt32Mul(Node* node) {
  Mips64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.right().HasValue() && m.right().Value() > 0) {
    int32_t value = m.right().Value();
    if (base::bits::IsPowerOfTwo32(value)) {
      Emit(kMips64Shl | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value)));
      return;
    }
    if (base::bits::IsPowerOfTwo32(value - 1)) {
      Emit(kMips64Lsa, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value - 1)));
      return;
    }
    if (base::bits::IsPowerOfTwo32(value + 1)) {
      InstructionOperand temp = g.TempRegister();
      Emit(kMips64Shl | AddressingModeField::encode(kMode_None), temp,
           g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value + 1)));
      Emit(kMips64Sub | AddressingModeField::encode(kMode_None),
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
        Emit(kMips64DMulHigh, g.DefineSameAsFirst(node),
             g.UseRegister(leftInput.left().node()),
             g.UseRegister(rightInput.left().node()));
        return;
      }
    }
  }
  VisitRRR(this, kMips64Mul, node);
}


void InstructionSelector::VisitInt32MulHigh(Node* node) {
  VisitRRR(this, kMips64MulHigh, node);
}


void InstructionSelector::VisitUint32MulHigh(Node* node) {
  VisitRRR(this, kMips64MulHighU, node);
}


void InstructionSelector::VisitInt64Mul(Node* node) {
  Mips64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  // TODO(dusmil): Add optimization for shifts larger than 32.
  if (m.right().HasValue() && m.right().Value() > 0) {
    int32_t value = static_cast<int32_t>(m.right().Value());
    if (base::bits::IsPowerOfTwo32(value)) {
      Emit(kMips64Dshl | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value)));
      return;
    }
    if (base::bits::IsPowerOfTwo32(value - 1)) {
      // Dlsa macro will handle the shifting value out of bound cases.
      Emit(kMips64Dlsa, g.DefineAsRegister(node),
           g.UseRegister(m.left().node()), g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value - 1)));
      return;
    }
    if (base::bits::IsPowerOfTwo32(value + 1)) {
      InstructionOperand temp = g.TempRegister();
      Emit(kMips64Dshl | AddressingModeField::encode(kMode_None), temp,
           g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value + 1)));
      Emit(kMips64Dsub | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), temp, g.UseRegister(m.left().node()));
      return;
    }
  }
  Emit(kMips64Dmul, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitInt32Div(Node* node) {
  Mips64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (CanCover(node, left) && CanCover(node, right)) {
    if (left->opcode() == IrOpcode::kWord64Sar &&
        right->opcode() == IrOpcode::kWord64Sar) {
      Int64BinopMatcher rightInput(right), leftInput(left);
      if (rightInput.right().Is(32) && leftInput.right().Is(32)) {
        // Combine both shifted operands with Ddiv.
        Emit(kMips64Ddiv, g.DefineSameAsFirst(node),
             g.UseRegister(leftInput.left().node()),
             g.UseRegister(rightInput.left().node()));
        return;
      }
    }
  }
  Emit(kMips64Div, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitUint32Div(Node* node) {
  Mips64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  Emit(kMips64DivU, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitInt32Mod(Node* node) {
  Mips64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (CanCover(node, left) && CanCover(node, right)) {
    if (left->opcode() == IrOpcode::kWord64Sar &&
        right->opcode() == IrOpcode::kWord64Sar) {
      Int64BinopMatcher rightInput(right), leftInput(left);
      if (rightInput.right().Is(32) && leftInput.right().Is(32)) {
        // Combine both shifted operands with Dmod.
        Emit(kMips64Dmod, g.DefineSameAsFirst(node),
             g.UseRegister(leftInput.left().node()),
             g.UseRegister(rightInput.left().node()));
        return;
      }
    }
  }
  Emit(kMips64Mod, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitUint32Mod(Node* node) {
  Mips64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  Emit(kMips64ModU, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitInt64Div(Node* node) {
  Mips64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  Emit(kMips64Ddiv, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitUint64Div(Node* node) {
  Mips64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  Emit(kMips64DdivU, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitInt64Mod(Node* node) {
  Mips64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  Emit(kMips64Dmod, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitUint64Mod(Node* node) {
  Mips64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  Emit(kMips64DmodU, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
}


void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  VisitRR(this, kMips64CvtDS, node);
}


void InstructionSelector::VisitRoundInt32ToFloat32(Node* node) {
  VisitRR(this, kMips64CvtSW, node);
}


void InstructionSelector::VisitRoundUint32ToFloat32(Node* node) {
  VisitRR(this, kMips64CvtSUw, node);
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  VisitRR(this, kMips64CvtDW, node);
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  VisitRR(this, kMips64CvtDUw, node);
}


void InstructionSelector::VisitTruncateFloat32ToInt32(Node* node) {
  VisitRR(this, kMips64TruncWS, node);
}


void InstructionSelector::VisitTruncateFloat32ToUint32(Node* node) {
  VisitRR(this, kMips64TruncUwS, node);
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  Mips64OperandGenerator g(this);
  Node* value = node->InputAt(0);
  // Match ChangeFloat64ToInt32(Float64Round##OP) to corresponding instruction
  // which does rounding and conversion to integer format.
  if (CanCover(node, value)) {
    switch (value->opcode()) {
      case IrOpcode::kFloat64RoundDown:
        Emit(kMips64FloorWD, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      case IrOpcode::kFloat64RoundUp:
        Emit(kMips64CeilWD, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      case IrOpcode::kFloat64RoundTiesEven:
        Emit(kMips64RoundWD, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      case IrOpcode::kFloat64RoundTruncate:
        Emit(kMips64TruncWD, g.DefineAsRegister(node),
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
            Emit(kMips64FloorWS, g.DefineAsRegister(node),
                 g.UseRegister(next->InputAt(0)));
            return;
          case IrOpcode::kFloat32RoundUp:
            Emit(kMips64CeilWS, g.DefineAsRegister(node),
                 g.UseRegister(next->InputAt(0)));
            return;
          case IrOpcode::kFloat32RoundTiesEven:
            Emit(kMips64RoundWS, g.DefineAsRegister(node),
                 g.UseRegister(next->InputAt(0)));
            return;
          case IrOpcode::kFloat32RoundTruncate:
            Emit(kMips64TruncWS, g.DefineAsRegister(node),
                 g.UseRegister(next->InputAt(0)));
            return;
          default:
            Emit(kMips64TruncWS, g.DefineAsRegister(node),
                 g.UseRegister(value->InputAt(0)));
            return;
        }
      } else {
        // Match float32 -> float64 -> int32 representation change path.
        Emit(kMips64TruncWS, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      }
    }
  }
  VisitRR(this, kMips64TruncWD, node);
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  VisitRR(this, kMips64TruncUwD, node);
}

void InstructionSelector::VisitTruncateFloat64ToUint32(Node* node) {
  VisitRR(this, kMips64TruncUwD, node);
}

void InstructionSelector::VisitTryTruncateFloat32ToInt64(Node* node) {
  Mips64OperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  this->Emit(kMips64TruncLS, output_count, outputs, 1, inputs);
}


void InstructionSelector::VisitTryTruncateFloat64ToInt64(Node* node) {
  Mips64OperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kMips64TruncLD, output_count, outputs, 1, inputs);
}


void InstructionSelector::VisitTryTruncateFloat32ToUint64(Node* node) {
  Mips64OperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kMips64TruncUlS, output_count, outputs, 1, inputs);
}


void InstructionSelector::VisitTryTruncateFloat64ToUint64(Node* node) {
  Mips64OperandGenerator g(this);

  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kMips64TruncUlD, output_count, outputs, 1, inputs);
}


void InstructionSelector::VisitChangeInt32ToInt64(Node* node) {
  Node* value = node->InputAt(0);
  if (value->opcode() == IrOpcode::kLoad && CanCover(node, value)) {
    // Generate sign-extending load.
    LoadRepresentation load_rep = LoadRepresentationOf(value->op());
    InstructionCode opcode = kArchNop;
    switch (load_rep.representation()) {
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = load_rep.IsUnsigned() ? kMips64Lbu : kMips64Lb;
        break;
      case MachineRepresentation::kWord16:
        opcode = load_rep.IsUnsigned() ? kMips64Lhu : kMips64Lh;
        break;
      case MachineRepresentation::kWord32:
        opcode = kMips64Lw;
        break;
      default:
        UNREACHABLE();
        return;
    }
    EmitLoad(this, value, opcode, node);
  } else {
    Mips64OperandGenerator g(this);
    Emit(kMips64Shl, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)),
         g.TempImmediate(0));
  }
}


void InstructionSelector::VisitChangeUint32ToUint64(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64Dext, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)),
       g.TempImmediate(0), g.TempImmediate(32));
}


void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  Mips64OperandGenerator g(this);
  Node* value = node->InputAt(0);
  if (CanCover(node, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord64Sar: {
        Int64BinopMatcher m(value);
        if (m.right().IsInRange(32, 63)) {
          // After smi untagging no need for truncate. Combine sequence.
          Emit(kMips64Dsar, g.DefineSameAsFirst(node),
               g.UseRegister(m.left().node()),
               g.UseImmediate(m.right().node()));
          return;
        }
        break;
      }
      default:
        break;
    }
  }
  Emit(kMips64Ext, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)),
       g.TempImmediate(0), g.TempImmediate(32));
}


void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  Mips64OperandGenerator g(this);
  Node* value = node->InputAt(0);
  // Match TruncateFloat64ToFloat32(ChangeInt32ToFloat64) to corresponding
  // instruction.
  if (CanCover(node, value) &&
      value->opcode() == IrOpcode::kChangeInt32ToFloat64) {
    Emit(kMips64CvtSW, g.DefineAsRegister(node),
         g.UseRegister(value->InputAt(0)));
    return;
  }
  VisitRR(this, kMips64CvtSD, node);
}

void InstructionSelector::VisitTruncateFloat64ToWord32(Node* node) {
  VisitRR(this, kArchTruncateDoubleToI, node);
}

void InstructionSelector::VisitRoundFloat64ToInt32(Node* node) {
  VisitRR(this, kMips64TruncWD, node);
}

void InstructionSelector::VisitRoundInt64ToFloat32(Node* node) {
  VisitRR(this, kMips64CvtSL, node);
}


void InstructionSelector::VisitRoundInt64ToFloat64(Node* node) {
  VisitRR(this, kMips64CvtDL, node);
}


void InstructionSelector::VisitRoundUint64ToFloat32(Node* node) {
  VisitRR(this, kMips64CvtSUl, node);
}


void InstructionSelector::VisitRoundUint64ToFloat64(Node* node) {
  VisitRR(this, kMips64CvtDUl, node);
}


void InstructionSelector::VisitBitcastFloat32ToInt32(Node* node) {
  VisitRR(this, kMips64Float64ExtractLowWord32, node);
}


void InstructionSelector::VisitBitcastFloat64ToInt64(Node* node) {
  VisitRR(this, kMips64BitcastDL, node);
}


void InstructionSelector::VisitBitcastInt32ToFloat32(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64Float64InsertLowWord32, g.DefineAsRegister(node),
       ImmediateOperand(ImmediateOperand::INLINE, 0),
       g.UseRegister(node->InputAt(0)));
}


void InstructionSelector::VisitBitcastInt64ToFloat64(Node* node) {
  VisitRR(this, kMips64BitcastLD, node);
}


void InstructionSelector::VisitFloat32Add(Node* node) {
  Mips64OperandGenerator g(this);
  Float32BinopMatcher m(node);
  if (m.left().IsFloat32Mul() && CanCover(node, m.left().node())) {
    // For Add.S(Mul.S(x, y), z):
    Float32BinopMatcher mleft(m.left().node());
    if (kArchVariant == kMips64r2) {  // Select Madd.S(z, x, y).
      Emit(kMips64MaddS, g.DefineAsRegister(node),
           g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    } else if (kArchVariant == kMips64r6) {  // Select Maddf.S(z, x, y).
      Emit(kMips64MaddfS, g.DefineSameAsFirst(node),
           g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    }
  }
  if (m.right().IsFloat32Mul() && CanCover(node, m.right().node())) {
    // For Add.S(x, Mul.S(y, z)):
    Float32BinopMatcher mright(m.right().node());
    if (kArchVariant == kMips64r2) {  // Select Madd.S(x, y, z).
      Emit(kMips64MaddS, g.DefineAsRegister(node),
           g.UseRegister(m.left().node()), g.UseRegister(mright.left().node()),
           g.UseRegister(mright.right().node()));
      return;
    } else if (kArchVariant == kMips64r6) {  // Select Maddf.S(x, y, z).
      Emit(kMips64MaddfS, g.DefineSameAsFirst(node),
           g.UseRegister(m.left().node()), g.UseRegister(mright.left().node()),
           g.UseRegister(mright.right().node()));
      return;
    }
  }
  VisitRRR(this, kMips64AddS, node);
}


void InstructionSelector::VisitFloat64Add(Node* node) {
  Mips64OperandGenerator g(this);
  Float64BinopMatcher m(node);
  if (m.left().IsFloat64Mul() && CanCover(node, m.left().node())) {
    // For Add.D(Mul.D(x, y), z):
    Float64BinopMatcher mleft(m.left().node());
    if (kArchVariant == kMips64r2) {  // Select Madd.D(z, x, y).
      Emit(kMips64MaddD, g.DefineAsRegister(node),
           g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    } else if (kArchVariant == kMips64r6) {  // Select Maddf.D(z, x, y).
      Emit(kMips64MaddfD, g.DefineSameAsFirst(node),
           g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    }
  }
  if (m.right().IsFloat64Mul() && CanCover(node, m.right().node())) {
    // For Add.D(x, Mul.D(y, z)):
    Float64BinopMatcher mright(m.right().node());
    if (kArchVariant == kMips64r2) {  // Select Madd.D(x, y, z).
      Emit(kMips64MaddD, g.DefineAsRegister(node),
           g.UseRegister(m.left().node()), g.UseRegister(mright.left().node()),
           g.UseRegister(mright.right().node()));
      return;
    } else if (kArchVariant == kMips64r6) {  // Select Maddf.D(x, y, z).
      Emit(kMips64MaddfD, g.DefineSameAsFirst(node),
           g.UseRegister(m.left().node()), g.UseRegister(mright.left().node()),
           g.UseRegister(mright.right().node()));
      return;
    }
  }
  VisitRRR(this, kMips64AddD, node);
}


void InstructionSelector::VisitFloat32Sub(Node* node) {
  Mips64OperandGenerator g(this);
  Float32BinopMatcher m(node);
  if (m.left().IsFloat32Mul() && CanCover(node, m.left().node())) {
    if (kArchVariant == kMips64r2) {
      // For Sub.S(Mul.S(x,y), z) select Msub.S(z, x, y).
      Float32BinopMatcher mleft(m.left().node());
      Emit(kMips64MsubS, g.DefineAsRegister(node),
           g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    }
  } else if (m.right().IsFloat32Mul() && CanCover(node, m.right().node())) {
    if (kArchVariant == kMips64r6) {
      // For Sub.S(x,Mul.S(y,z)) select Msubf.S(x, y, z).
      Float32BinopMatcher mright(m.right().node());
      Emit(kMips64MsubfS, g.DefineSameAsFirst(node),
           g.UseRegister(m.left().node()), g.UseRegister(mright.left().node()),
           g.UseRegister(mright.right().node()));
      return;
    }
  }
  VisitRRR(this, kMips64SubS, node);
}

void InstructionSelector::VisitFloat64Sub(Node* node) {
  Mips64OperandGenerator g(this);
  Float64BinopMatcher m(node);
  if (m.left().IsFloat64Mul() && CanCover(node, m.left().node())) {
    if (kArchVariant == kMips64r2) {
      // For Sub.D(Mul.S(x,y), z) select Msub.D(z, x, y).
      Float64BinopMatcher mleft(m.left().node());
      Emit(kMips64MsubD, g.DefineAsRegister(node),
           g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
           g.UseRegister(mleft.right().node()));
      return;
    }
  } else if (m.right().IsFloat64Mul() && CanCover(node, m.right().node())) {
    if (kArchVariant == kMips64r6) {
      // For Sub.D(x,Mul.S(y,z)) select Msubf.D(x, y, z).
      Float64BinopMatcher mright(m.right().node());
      Emit(kMips64MsubfD, g.DefineSameAsFirst(node),
           g.UseRegister(m.left().node()), g.UseRegister(mright.left().node()),
           g.UseRegister(mright.right().node()));
      return;
    }
  }
  VisitRRR(this, kMips64SubD, node);
}

void InstructionSelector::VisitFloat32Mul(Node* node) {
  VisitRRR(this, kMips64MulS, node);
}


void InstructionSelector::VisitFloat64Mul(Node* node) {
  VisitRRR(this, kMips64MulD, node);
}


void InstructionSelector::VisitFloat32Div(Node* node) {
  VisitRRR(this, kMips64DivS, node);
}


void InstructionSelector::VisitFloat64Div(Node* node) {
  VisitRRR(this, kMips64DivD, node);
}


void InstructionSelector::VisitFloat64Mod(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64ModD, g.DefineAsFixed(node, f0),
       g.UseFixed(node->InputAt(0), f12),
       g.UseFixed(node->InputAt(1), f14))->MarkAsCall();
}

void InstructionSelector::VisitFloat32Max(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64Float32Max, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitFloat64Max(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64Float64Max, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitFloat32Min(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64Float32Min, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitFloat64Min(Node* node) {
  Mips64OperandGenerator g(this);
  Emit(kMips64Float64Min, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}


void InstructionSelector::VisitFloat32Abs(Node* node) {
  VisitRR(this, kMips64AbsS, node);
}


void InstructionSelector::VisitFloat64Abs(Node* node) {
  VisitRR(this, kMips64AbsD, node);
}

void InstructionSelector::VisitFloat32Sqrt(Node* node) {
  VisitRR(this, kMips64SqrtS, node);
}


void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  VisitRR(this, kMips64SqrtD, node);
}


void InstructionSelector::VisitFloat32RoundDown(Node* node) {
  VisitRR(this, kMips64Float32RoundDown, node);
}


void InstructionSelector::VisitFloat64RoundDown(Node* node) {
  VisitRR(this, kMips64Float64RoundDown, node);
}


void InstructionSelector::VisitFloat32RoundUp(Node* node) {
  VisitRR(this, kMips64Float32RoundUp, node);
}


void InstructionSelector::VisitFloat64RoundUp(Node* node) {
  VisitRR(this, kMips64Float64RoundUp, node);
}


void InstructionSelector::VisitFloat32RoundTruncate(Node* node) {
  VisitRR(this, kMips64Float32RoundTruncate, node);
}


void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  VisitRR(this, kMips64Float64RoundTruncate, node);
}


void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  UNREACHABLE();
}


void InstructionSelector::VisitFloat32RoundTiesEven(Node* node) {
  VisitRR(this, kMips64Float32RoundTiesEven, node);
}


void InstructionSelector::VisitFloat64RoundTiesEven(Node* node) {
  VisitRR(this, kMips64Float64RoundTiesEven, node);
}

void InstructionSelector::VisitFloat32Neg(Node* node) {
  VisitRR(this, kMips64NegS, node);
}

void InstructionSelector::VisitFloat64Neg(Node* node) {
  VisitRR(this, kMips64NegD, node);
}

void InstructionSelector::VisitFloat64Ieee754Binop(Node* node,
                                                   InstructionCode opcode) {
  Mips64OperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, f0), g.UseFixed(node->InputAt(0), f2),
       g.UseFixed(node->InputAt(1), f4))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat64Ieee754Unop(Node* node,
                                                  InstructionCode opcode) {
  Mips64OperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, f0), g.UseFixed(node->InputAt(0), f12))
      ->MarkAsCall();
}

void InstructionSelector::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* descriptor,
    Node* node) {
  Mips64OperandGenerator g(this);

  // Prepare for C function call.
  if (descriptor->IsCFunctionCall()) {
    Emit(kArchPrepareCallCFunction |
             MiscField::encode(static_cast<int>(descriptor->ParameterCount())),
         0, nullptr, 0, nullptr);

    // Poke any stack arguments.
    int slot = kCArgSlotCount;
    for (PushParameter input : (*arguments)) {
      Emit(kMips64StoreToStackSlot, g.NoOutput(), g.UseRegister(input.node()),
           g.TempImmediate(slot << kPointerSizeLog2));
      ++slot;
    }
  } else {
    int push_count = static_cast<int>(descriptor->StackParameterCount());
    if (push_count > 0) {
      Emit(kMips64StackClaim, g.NoOutput(),
           g.TempImmediate(push_count << kPointerSizeLog2));
    }
    for (size_t n = 0; n < arguments->size(); ++n) {
      PushParameter input = (*arguments)[n];
      if (input.node()) {
        Emit(kMips64StoreToStackSlot, g.NoOutput(), g.UseRegister(input.node()),
             g.TempImmediate(static_cast<int>(n << kPointerSizeLog2)));
      }
    }
  }
}


bool InstructionSelector::IsTailCallAddressImmediate() { return false; }

int InstructionSelector::GetTempsCountForTailCallFromJSFunction() { return 3; }

void InstructionSelector::VisitUnalignedLoad(Node* node) {
  UnalignedLoadRepresentation load_rep =
      UnalignedLoadRepresentationOf(node->op());
  Mips64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  ArchOpcode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kMips64Ulwc1;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kMips64Uldc1;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      UNREACHABLE();
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsUnsigned() ? kMips64Ulhu : kMips64Ulh;
      break;
    case MachineRepresentation::kWord32:
      opcode = load_rep.IsUnsigned() ? kMips64Ulwu : kMips64Ulw;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:  // Fall through.
    case MachineRepresentation::kWord64:
      opcode = kMips64Uld;
      break;
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kMips64Dadd | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

void InstructionSelector::VisitUnalignedStore(Node* node) {
  Mips64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  UnalignedStoreRepresentation rep = UnalignedStoreRepresentationOf(node->op());
  ArchOpcode opcode = kArchNop;
  switch (rep) {
    case MachineRepresentation::kFloat32:
      opcode = kMips64Uswc1;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kMips64Usdc1;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      UNREACHABLE();
      break;
    case MachineRepresentation::kWord16:
      opcode = kMips64Ush;
      break;
    case MachineRepresentation::kWord32:
      opcode = kMips64Usw;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:  // Fall through.
    case MachineRepresentation::kWord64:
      opcode = kMips64Usd;
      break;
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         g.UseRegister(base), g.UseImmediate(index), g.UseRegister(value));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kMips64Dadd | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired store opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         addr_reg, g.TempImmediate(0), g.UseRegister(value));
  }
}

void InstructionSelector::VisitCheckedLoad(Node* node) {
  CheckedLoadRepresentation load_rep = CheckedLoadRepresentationOf(node->op());
  Mips64OperandGenerator g(this);
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
    case MachineRepresentation::kBit:
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kSimd128:
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }
  InstructionOperand offset_operand = g.CanBeImmediate(offset, opcode)
                                          ? g.UseImmediate(offset)
                                          : g.UseRegister(offset);

  InstructionOperand length_operand = (!g.CanBeImmediate(offset, opcode))
                                          ? g.CanBeImmediate(length, opcode)
                                                ? g.UseImmediate(length)
                                                : g.UseRegister(length)
                                          : g.UseRegister(length);

  Emit(opcode | AddressingModeField::encode(kMode_MRI),
       g.DefineAsRegister(node), offset_operand, length_operand,
       g.UseRegister(buffer));
}


void InstructionSelector::VisitCheckedStore(Node* node) {
  MachineRepresentation rep = CheckedStoreRepresentationOf(node->op());
  Mips64OperandGenerator g(this);
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
    case MachineRepresentation::kBit:
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kSimd128:
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }
  InstructionOperand offset_operand = g.CanBeImmediate(offset, opcode)
                                          ? g.UseImmediate(offset)
                                          : g.UseRegister(offset);

  InstructionOperand length_operand = (!g.CanBeImmediate(offset, opcode))
                                          ? g.CanBeImmediate(length, opcode)
                                                ? g.UseImmediate(length)
                                                : g.UseRegister(length)
                                          : g.UseRegister(length);

  Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
       offset_operand, length_operand, g.UseRegister(value),
       g.UseRegister(buffer));
}


namespace {

// Shared routine for multiple compare operations.
static void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                         InstructionOperand left, InstructionOperand right,
                         FlagsContinuation* cont) {
  Mips64OperandGenerator g(selector);
  opcode = cont->Encode(opcode);
  if (cont->IsBranch()) {
    selector->Emit(opcode, g.NoOutput(), left, right,
                   g.Label(cont->true_block()), g.Label(cont->false_block()));
  } else if (cont->IsDeoptimize()) {
    selector->EmitDeoptimize(opcode, g.NoOutput(), left, right, cont->reason(),
                             cont->frame_state());
  } else {
    DCHECK(cont->IsSet());
    selector->Emit(opcode, g.DefineAsRegister(cont->result()), left, right);
  }
}


// Shared routine for multiple float32 compare operations.
void VisitFloat32Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  Mips64OperandGenerator g(selector);
  Float32BinopMatcher m(node);
  InstructionOperand lhs, rhs;

  lhs = m.left().IsZero() ? g.UseImmediate(m.left().node())
                          : g.UseRegister(m.left().node());
  rhs = m.right().IsZero() ? g.UseImmediate(m.right().node())
                           : g.UseRegister(m.right().node());
  VisitCompare(selector, kMips64CmpS, lhs, rhs, cont);
}


// Shared routine for multiple float64 compare operations.
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  Mips64OperandGenerator g(selector);
  Float64BinopMatcher m(node);
  InstructionOperand lhs, rhs;

  lhs = m.left().IsZero() ? g.UseImmediate(m.left().node())
                          : g.UseRegister(m.left().node());
  rhs = m.right().IsZero() ? g.UseImmediate(m.right().node())
                           : g.UseRegister(m.right().node());
  VisitCompare(selector, kMips64CmpD, lhs, rhs, cont);
}


// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont,
                      bool commutative) {
  Mips64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  // Match immediates on left or right side of comparison.
  if (g.CanBeImmediate(right, opcode)) {
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
  } else if (g.CanBeImmediate(left, opcode)) {
    if (!commutative) cont->Commute();
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
  } else {
    VisitCompare(selector, opcode, g.UseRegister(left), g.UseRegister(right),
                 cont);
  }
}


void VisitWord32Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  VisitWordCompare(selector, node, kMips64Cmp, cont, false);
}


void VisitWord64Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  VisitWordCompare(selector, node, kMips64Cmp, cont, false);
}



void EmitWordCompareZero(InstructionSelector* selector, Node* value,
                         FlagsContinuation* cont) {
  Mips64OperandGenerator g(selector);
  InstructionCode opcode = cont->Encode(kMips64Cmp);
  InstructionOperand const value_operand = g.UseRegister(value);
  if (cont->IsBranch()) {
    selector->Emit(opcode, g.NoOutput(), value_operand, g.TempImmediate(0),
                   g.Label(cont->true_block()), g.Label(cont->false_block()));
  } else if (cont->IsDeoptimize()) {
    selector->EmitDeoptimize(opcode, g.NoOutput(), value_operand,
                             g.TempImmediate(0), cont->reason(),
                             cont->frame_state());
  } else {
    selector->Emit(opcode, g.DefineAsRegister(cont->result()), value_operand,
                   g.TempImmediate(0));
  }
}


// Shared routine for word comparisons against zero.
void VisitWordCompareZero(InstructionSelector* selector, Node* user,
                          Node* value, FlagsContinuation* cont) {
  while (selector->CanCover(user, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord32Equal: {
        // Combine with comparisons against 0 by simply inverting the
        // continuation.
        Int32BinopMatcher m(value);
        if (m.right().Is(0)) {
          user = value;
          value = m.left().node();
          cont->Negate();
          continue;
        }
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitWord32Compare(selector, value, cont);
      }
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
        // Combine with comparisons against 0 by simply inverting the
        // continuation.
        Int64BinopMatcher m(value);
        if (m.right().Is(0)) {
          user = value;
          value = m.left().node();
          cont->Negate();
          continue;
        }
        cont->OverwriteAndNegateIfEqual(kEqual);
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
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitFloat32Compare(selector, value, cont);
      case IrOpcode::kFloat32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitFloat32Compare(selector, value, cont);
      case IrOpcode::kFloat32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitFloat32Compare(selector, value, cont);
      case IrOpcode::kFloat64Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitFloat64Compare(selector, value, cont);
      case IrOpcode::kFloat64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitFloat64Compare(selector, value, cont);
      case IrOpcode::kFloat64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
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
                return VisitBinop(selector, node, kMips64Dadd, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kMips64Dsub, cont);
              case IrOpcode::kInt32MulWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kMips64MulOvf, cont);
              case IrOpcode::kInt64AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kMips64DaddOvf, cont);
              case IrOpcode::kInt64SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(selector, node, kMips64DsubOvf, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kWord32And:
      case IrOpcode::kWord64And:
        return VisitWordCompare(selector, value, kMips64Tst, cont, true);
      default:
        break;
    }
    break;
  }

  // Continuation could not be combined with a compare, emit compare against 0.
  EmitWordCompareZero(selector, value, cont);
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

void InstructionSelector::VisitSwitch(Node* node, const SwitchInfo& sw) {
  Mips64OperandGenerator g(this);
  InstructionOperand value_operand = g.UseRegister(node->InputAt(0));

  // Emit either ArchTableSwitch or ArchLookupSwitch.
  size_t table_space_cost = 10 + 2 * sw.value_range;
  size_t table_time_cost = 3;
  size_t lookup_space_cost = 2 + 2 * sw.case_count;
  size_t lookup_time_cost = sw.case_count;
  if (sw.case_count > 0 &&
      table_space_cost + 3 * table_time_cost <=
          lookup_space_cost + 3 * lookup_time_cost &&
      sw.min_value > std::numeric_limits<int32_t>::min()) {
    InstructionOperand index_operand = value_operand;
    if (sw.min_value) {
      index_operand = g.TempRegister();
      Emit(kMips64Sub, index_operand, value_operand,
           g.TempImmediate(sw.min_value));
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
    return VisitBinop(this, node, kMips64Dadd, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kMips64Dadd, &cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kMips64Dsub, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kMips64Dsub, &cont);
}

void InstructionSelector::VisitInt32MulWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kMips64MulOvf, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kMips64MulOvf, &cont);
}

void InstructionSelector::VisitInt64AddWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kMips64DaddOvf, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kMips64DaddOvf, &cont);
}


void InstructionSelector::VisitInt64SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kMips64DsubOvf, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kMips64DsubOvf, &cont);
}


void InstructionSelector::VisitWord64Equal(Node* const node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int64BinopMatcher m(node);
  if (m.right().Is(0)) {
    return VisitWordCompareZero(this, m.node(), m.left().node(), &cont);
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
  VisitRR(this, kMips64Float64ExtractLowWord32, node);
}


void InstructionSelector::VisitFloat64ExtractHighWord32(Node* node) {
  VisitRR(this, kMips64Float64ExtractHighWord32, node);
}

void InstructionSelector::VisitFloat64SilenceNaN(Node* node) {
  VisitRR(this, kMips64Float64SilenceNaN, node);
}

void InstructionSelector::VisitFloat64InsertLowWord32(Node* node) {
  Mips64OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Emit(kMips64Float64InsertLowWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.UseRegister(right));
}


void InstructionSelector::VisitFloat64InsertHighWord32(Node* node) {
  Mips64OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Emit(kMips64Float64InsertHighWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.UseRegister(right));
}

void InstructionSelector::VisitAtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  Mips64OperandGenerator g(this);
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
  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kMips64Dadd | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

void InstructionSelector::VisitAtomicStore(Node* node) {
  MachineRepresentation rep = AtomicStoreRepresentationOf(node->op());
  Mips64OperandGenerator g(this);
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

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         g.UseRegister(base), g.UseImmediate(index), g.UseRegister(value));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kMips64Dadd | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired store opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         addr_reg, g.TempImmediate(0), g.UseRegister(value));
  }
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
         MachineOperatorBuilder::kFloat32RoundTiesEven |
         MachineOperatorBuilder::kWord32ReverseBytes |
         MachineOperatorBuilder::kWord64ReverseBytes;
}

// static
MachineOperatorBuilder::AlignmentRequirements
InstructionSelector::AlignmentRequirements() {
  if (kArchVariant == kMips64r6) {
    return MachineOperatorBuilder::AlignmentRequirements::
        FullUnalignedAccessSupport();
  } else {
    DCHECK(kArchVariant == kMips64r2);
    return MachineOperatorBuilder::AlignmentRequirements::
        NoUnalignedAccessSupport();
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
