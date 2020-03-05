// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/iterator.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/execution/ppc/frame-constants-ppc.h"

namespace v8 {
namespace internal {
namespace compiler {

enum ImmediateMode {
  kInt16Imm,
  kInt16Imm_Unsigned,
  kInt16Imm_Negate,
  kInt16Imm_4ByteAligned,
  kShift32Imm,
  kShift64Imm,
  kNoImmediate
};

// Adds PPC-specific methods for generating operands.
class PPCOperandGenerator final : public OperandGenerator {
 public:
  explicit PPCOperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand UseOperand(Node* node, ImmediateMode mode) {
    if (CanBeImmediate(node, mode)) {
      return UseImmediate(node);
    }
    return UseRegister(node);
  }

  bool CanBeImmediate(Node* node, ImmediateMode mode) {
    int64_t value;
    if (node->opcode() == IrOpcode::kInt32Constant)
      value = OpParameter<int32_t>(node->op());
    else if (node->opcode() == IrOpcode::kInt64Constant)
      value = OpParameter<int64_t>(node->op());
    else
      return false;
    return CanBeImmediate(value, mode);
  }

  bool CanBeImmediate(int64_t value, ImmediateMode mode) {
    switch (mode) {
      case kInt16Imm:
        return is_int16(value);
      case kInt16Imm_Unsigned:
        return is_uint16(value);
      case kInt16Imm_Negate:
        return is_int16(-value);
      case kInt16Imm_4ByteAligned:
        return is_int16(value) && !(value & 3);
      case kShift32Imm:
        return 0 <= value && value < 32;
      case kShift64Imm:
        return 0 <= value && value < 64;
      case kNoImmediate:
        return false;
    }
    return false;
  }
};

namespace {

void VisitRR(InstructionSelector* selector, InstructionCode opcode,
             Node* node) {
  PPCOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}

void VisitRRR(InstructionSelector* selector, InstructionCode opcode,
              Node* node) {
  PPCOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
}

void VisitRRO(InstructionSelector* selector, InstructionCode opcode, Node* node,
              ImmediateMode operand_mode) {
  PPCOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseOperand(node->InputAt(1), operand_mode));
}

#if V8_TARGET_ARCH_PPC64
void VisitTryTruncateDouble(InstructionSelector* selector,
                            InstructionCode opcode, Node* node) {
  PPCOperandGenerator g(selector);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  selector->Emit(opcode, output_count, outputs, 1, inputs);
}
#endif

// Shared routine for multiple binary operations.
template <typename Matcher>
void VisitBinop(InstructionSelector* selector, Node* node,
                InstructionCode opcode, ImmediateMode operand_mode,
                FlagsContinuation* cont) {
  PPCOperandGenerator g(selector);
  Matcher m(node);
  InstructionOperand inputs[4];
  size_t input_count = 0;
  InstructionOperand outputs[2];
  size_t output_count = 0;

  inputs[input_count++] = g.UseRegister(m.left().node());
  inputs[input_count++] = g.UseOperand(m.right().node(), operand_mode);

  if (cont->IsDeoptimize()) {
    // If we can deoptimize as a result of the binop, we need to make sure that
    // the deopt inputs are not overwritten by the binop result. One way
    // to achieve that is to declare the output register as same-as-first.
    outputs[output_count++] = g.DefineSameAsFirst(node);
  } else {
    outputs[output_count++] = g.DefineAsRegister(node);
  }

  DCHECK_NE(0u, input_count);
  DCHECK_NE(0u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}

// Shared routine for multiple binary operations.
template <typename Matcher>
void VisitBinop(InstructionSelector* selector, Node* node,
                InstructionCode opcode, ImmediateMode operand_mode) {
  FlagsContinuation cont;
  VisitBinop<Matcher>(selector, node, opcode, operand_mode, &cont);
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
  PPCOperandGenerator g(this);
  Emit(kArchAbortCSAAssert, g.NoOutput(), g.UseFixed(node->InputAt(0), r4));
}

void InstructionSelector::VisitLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  PPCOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* offset = node->InputAt(1);
  InstructionCode opcode = kArchNop;
  ImmediateMode mode = kInt16Imm;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kPPC_LoadFloat32;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kPPC_LoadDouble;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsSigned() ? kPPC_LoadWordS8 : kPPC_LoadWordU8;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsSigned() ? kPPC_LoadWordS16 : kPPC_LoadWordU16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kPPC_LoadWordU32;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord64:
      opcode = kPPC_LoadWord64;
      mode = kInt16Imm_4ByteAligned;
      break;
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:         // Fall through.
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }

  if (node->opcode() == IrOpcode::kPoisonedLoad &&
      poisoning_level_ != PoisoningMitigationLevel::kDontPoison) {
    opcode |= MiscField::encode(kMemoryAccessPoisoned);
  }

  bool is_atomic = (node->opcode() == IrOpcode::kWord32AtomicLoad ||
                    node->opcode() == IrOpcode::kWord64AtomicLoad);

  if (g.CanBeImmediate(offset, mode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseImmediate(offset),
         g.UseImmediate(is_atomic));
  } else if (g.CanBeImmediate(base, mode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(offset), g.UseImmediate(base),
         g.UseImmediate(is_atomic));
  } else {
    Emit(opcode | AddressingModeField::encode(kMode_MRR),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseRegister(offset),
         g.UseImmediate(is_atomic));
  }
}

void InstructionSelector::VisitPoisonedLoad(Node* node) { VisitLoad(node); }

void InstructionSelector::VisitProtectedLoad(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelector::VisitStore(Node* node) {
  PPCOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* offset = node->InputAt(1);
  Node* value = node->InputAt(2);

  bool is_atomic = (node->opcode() == IrOpcode::kWord32AtomicStore ||
                    node->opcode() == IrOpcode::kWord64AtomicStore);

  MachineRepresentation rep;
  WriteBarrierKind write_barrier_kind = kNoWriteBarrier;

  if (is_atomic) {
    rep = AtomicStoreRepresentationOf(node->op());
  } else {
    StoreRepresentation store_rep = StoreRepresentationOf(node->op());
    write_barrier_kind = store_rep.write_barrier_kind();
    rep = store_rep.representation();
  }

  if (write_barrier_kind != kNoWriteBarrier &&
      V8_LIKELY(!FLAG_disable_write_barriers)) {
    DCHECK(CanBeTaggedPointer(rep));
    AddressingMode addressing_mode;
    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    // OutOfLineRecordWrite uses the offset in an 'add' instruction as well as
    // for the store itself, so we must check compatibility with both.
    if (g.CanBeImmediate(offset, kInt16Imm)
#if V8_TARGET_ARCH_PPC64
        && g.CanBeImmediate(offset, kInt16Imm_4ByteAligned)
#endif
            ) {
      inputs[input_count++] = g.UseImmediate(offset);
      addressing_mode = kMode_MRI;
    } else {
      inputs[input_count++] = g.UseUniqueRegister(offset);
      addressing_mode = kMode_MRR;
    }
    inputs[input_count++] = g.UseUniqueRegister(value);
    RecordWriteMode record_write_mode =
        WriteBarrierKindToRecordWriteMode(write_barrier_kind);
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    size_t const temp_count = arraysize(temps);
    InstructionCode code = kArchStoreWithWriteBarrier;
    code |= AddressingModeField::encode(addressing_mode);
    code |= MiscField::encode(static_cast<int>(record_write_mode));
    CHECK_EQ(is_atomic, false);
    Emit(code, 0, nullptr, input_count, inputs, temp_count, temps);
  } else {
    ArchOpcode opcode = kArchNop;
    ImmediateMode mode = kInt16Imm;
    switch (rep) {
      case MachineRepresentation::kFloat32:
        opcode = kPPC_StoreFloat32;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kPPC_StoreDouble;
        break;
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = kPPC_StoreWord8;
        break;
      case MachineRepresentation::kWord16:
        opcode = kPPC_StoreWord16;
        break;
#if !V8_TARGET_ARCH_PPC64
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:         // Fall through.
#endif
      case MachineRepresentation::kWord32:
        opcode = kPPC_StoreWord32;
        break;
#if V8_TARGET_ARCH_PPC64
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:         // Fall through.
      case MachineRepresentation::kWord64:
        opcode = kPPC_StoreWord64;
        mode = kInt16Imm_4ByteAligned;
        break;
#else
      case MachineRepresentation::kWord64:  // Fall through.
#endif
      case MachineRepresentation::kCompressedPointer:  // Fall through.
      case MachineRepresentation::kCompressed:         // Fall through.
      case MachineRepresentation::kSimd128:  // Fall through.
      case MachineRepresentation::kNone:
        UNREACHABLE();
        return;
    }

    if (g.CanBeImmediate(offset, mode)) {
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           g.UseRegister(base), g.UseImmediate(offset), g.UseRegister(value),
           g.UseImmediate(is_atomic));
    } else if (g.CanBeImmediate(base, mode)) {
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           g.UseRegister(offset), g.UseImmediate(base), g.UseRegister(value),
           g.UseImmediate(is_atomic));
    } else {
      Emit(opcode | AddressingModeField::encode(kMode_MRR), g.NoOutput(),
           g.UseRegister(base), g.UseRegister(offset), g.UseRegister(value),
           g.UseImmediate(is_atomic));
    }
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

template <typename Matcher>
static void VisitLogical(InstructionSelector* selector, Node* node, Matcher* m,
                         ArchOpcode opcode, bool left_can_cover,
                         bool right_can_cover, ImmediateMode imm_mode) {
  PPCOperandGenerator g(selector);

  // Map instruction to equivalent operation with inverted right input.
  ArchOpcode inv_opcode = opcode;
  switch (opcode) {
    case kPPC_And:
      inv_opcode = kPPC_AndComplement;
      break;
    case kPPC_Or:
      inv_opcode = kPPC_OrComplement;
      break;
    default:
      UNREACHABLE();
  }

  // Select Logical(y, ~x) for Logical(Xor(x, -1), y).
  if ((m->left().IsWord32Xor() || m->left().IsWord64Xor()) && left_can_cover) {
    Matcher mleft(m->left().node());
    if (mleft.right().Is(-1)) {
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

  VisitBinop<Matcher>(selector, node, opcode, imm_mode);
}

static inline bool IsContiguousMask32(uint32_t value, int* mb, int* me) {
  int mask_width = base::bits::CountPopulation(value);
  int mask_msb = base::bits::CountLeadingZeros32(value);
  int mask_lsb = base::bits::CountTrailingZeros32(value);
  if ((mask_width == 0) || (mask_msb + mask_width + mask_lsb != 32))
    return false;
  *mb = mask_lsb + mask_width - 1;
  *me = mask_lsb;
  return true;
}

#if V8_TARGET_ARCH_PPC64
static inline bool IsContiguousMask64(uint64_t value, int* mb, int* me) {
  int mask_width = base::bits::CountPopulation(value);
  int mask_msb = base::bits::CountLeadingZeros64(value);
  int mask_lsb = base::bits::CountTrailingZeros64(value);
  if ((mask_width == 0) || (mask_msb + mask_width + mask_lsb != 64))
    return false;
  *mb = mask_lsb + mask_width - 1;
  *me = mask_lsb;
  return true;
}
#endif

// TODO(mbrandy): Absorb rotate-right into rlwinm?
void InstructionSelector::VisitWord32And(Node* node) {
  PPCOperandGenerator g(this);
  Int32BinopMatcher m(node);
  int mb = 0;
  int me = 0;
  if (m.right().HasValue() && IsContiguousMask32(m.right().Value(), &mb, &me)) {
    int sh = 0;
    Node* left = m.left().node();
    if ((m.left().IsWord32Shr() || m.left().IsWord32Shl()) &&
        CanCover(node, left)) {
      // Try to absorb left/right shift into rlwinm
      Int32BinopMatcher mleft(m.left().node());
      if (mleft.right().IsInRange(0, 31)) {
        left = mleft.left().node();
        sh = mleft.right().Value();
        if (m.left().IsWord32Shr()) {
          // Adjust the mask such that it doesn't include any rotated bits.
          if (mb > 31 - sh) mb = 31 - sh;
          sh = (32 - sh) & 0x1F;
        } else {
          // Adjust the mask such that it doesn't include any rotated bits.
          if (me < sh) me = sh;
        }
      }
    }
    if (mb >= me) {
      Emit(kPPC_RotLeftAndMask32, g.DefineAsRegister(node), g.UseRegister(left),
           g.TempImmediate(sh), g.TempImmediate(mb), g.TempImmediate(me));
      return;
    }
  }
  VisitLogical<Int32BinopMatcher>(
      this, node, &m, kPPC_And, CanCover(node, m.left().node()),
      CanCover(node, m.right().node()), kInt16Imm_Unsigned);
}

#if V8_TARGET_ARCH_PPC64
// TODO(mbrandy): Absorb rotate-right into rldic?
void InstructionSelector::VisitWord64And(Node* node) {
  PPCOperandGenerator g(this);
  Int64BinopMatcher m(node);
  int mb = 0;
  int me = 0;
  if (m.right().HasValue() && IsContiguousMask64(m.right().Value(), &mb, &me)) {
    int sh = 0;
    Node* left = m.left().node();
    if ((m.left().IsWord64Shr() || m.left().IsWord64Shl()) &&
        CanCover(node, left)) {
      // Try to absorb left/right shift into rldic
      Int64BinopMatcher mleft(m.left().node());
      if (mleft.right().IsInRange(0, 63)) {
        left = mleft.left().node();
        sh = mleft.right().Value();
        if (m.left().IsWord64Shr()) {
          // Adjust the mask such that it doesn't include any rotated bits.
          if (mb > 63 - sh) mb = 63 - sh;
          sh = (64 - sh) & 0x3F;
        } else {
          // Adjust the mask such that it doesn't include any rotated bits.
          if (me < sh) me = sh;
        }
      }
    }
    if (mb >= me) {
      bool match = false;
      ArchOpcode opcode;
      int mask;
      if (me == 0) {
        match = true;
        opcode = kPPC_RotLeftAndClearLeft64;
        mask = mb;
      } else if (mb == 63) {
        match = true;
        opcode = kPPC_RotLeftAndClearRight64;
        mask = me;
      } else if (sh && me <= sh && m.left().IsWord64Shl()) {
        match = true;
        opcode = kPPC_RotLeftAndClear64;
        mask = mb;
      }
      if (match) {
        Emit(opcode, g.DefineAsRegister(node), g.UseRegister(left),
             g.TempImmediate(sh), g.TempImmediate(mask));
        return;
      }
    }
  }
  VisitLogical<Int64BinopMatcher>(
      this, node, &m, kPPC_And, CanCover(node, m.left().node()),
      CanCover(node, m.right().node()), kInt16Imm_Unsigned);
}
#endif

void InstructionSelector::VisitWord32Or(Node* node) {
  Int32BinopMatcher m(node);
  VisitLogical<Int32BinopMatcher>(
      this, node, &m, kPPC_Or, CanCover(node, m.left().node()),
      CanCover(node, m.right().node()), kInt16Imm_Unsigned);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitWord64Or(Node* node) {
  Int64BinopMatcher m(node);
  VisitLogical<Int64BinopMatcher>(
      this, node, &m, kPPC_Or, CanCover(node, m.left().node()),
      CanCover(node, m.right().node()), kInt16Imm_Unsigned);
}
#endif

void InstructionSelector::VisitWord32Xor(Node* node) {
  PPCOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.right().Is(-1)) {
    Emit(kPPC_Not, g.DefineAsRegister(node), g.UseRegister(m.left().node()));
  } else {
    VisitBinop<Int32BinopMatcher>(this, node, kPPC_Xor, kInt16Imm_Unsigned);
  }
}

void InstructionSelector::VisitStackPointerGreaterThan(
    Node* node, FlagsContinuation* cont) {
  StackCheckKind kind = StackCheckKindOf(node->op());
  InstructionCode opcode =
      kArchStackPointerGreaterThan | MiscField::encode(static_cast<int>(kind));

  PPCOperandGenerator g(this);

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

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitWord64Xor(Node* node) {
  PPCOperandGenerator g(this);
  Int64BinopMatcher m(node);
  if (m.right().Is(-1)) {
    Emit(kPPC_Not, g.DefineAsRegister(node), g.UseRegister(m.left().node()));
  } else {
    VisitBinop<Int64BinopMatcher>(this, node, kPPC_Xor, kInt16Imm_Unsigned);
  }
}
#endif

void InstructionSelector::VisitWord32Shl(Node* node) {
  PPCOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().IsWord32And() && m.right().IsInRange(0, 31)) {
    // Try to absorb logical-and into rlwinm
    Int32BinopMatcher mleft(m.left().node());
    int sh = m.right().Value();
    int mb;
    int me;
    if (mleft.right().HasValue() &&
        IsContiguousMask32(mleft.right().Value() << sh, &mb, &me)) {
      // Adjust the mask such that it doesn't include any rotated bits.
      if (me < sh) me = sh;
      if (mb >= me) {
        Emit(kPPC_RotLeftAndMask32, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(sh),
             g.TempImmediate(mb), g.TempImmediate(me));
        return;
      }
    }
  }
  VisitRRO(this, kPPC_ShiftLeft32, node, kShift32Imm);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitWord64Shl(Node* node) {
  PPCOperandGenerator g(this);
  Int64BinopMatcher m(node);
  // TODO(mbrandy): eliminate left sign extension if right >= 32
  if (m.left().IsWord64And() && m.right().IsInRange(0, 63)) {
    // Try to absorb logical-and into rldic
    Int64BinopMatcher mleft(m.left().node());
    int sh = m.right().Value();
    int mb;
    int me;
    if (mleft.right().HasValue() &&
        IsContiguousMask64(mleft.right().Value() << sh, &mb, &me)) {
      // Adjust the mask such that it doesn't include any rotated bits.
      if (me < sh) me = sh;
      if (mb >= me) {
        bool match = false;
        ArchOpcode opcode;
        int mask;
        if (me == 0) {
          match = true;
          opcode = kPPC_RotLeftAndClearLeft64;
          mask = mb;
        } else if (mb == 63) {
          match = true;
          opcode = kPPC_RotLeftAndClearRight64;
          mask = me;
        } else if (sh && me <= sh) {
          match = true;
          opcode = kPPC_RotLeftAndClear64;
          mask = mb;
        }
        if (match) {
          Emit(opcode, g.DefineAsRegister(node),
               g.UseRegister(mleft.left().node()), g.TempImmediate(sh),
               g.TempImmediate(mask));
          return;
        }
      }
    }
  }
  VisitRRO(this, kPPC_ShiftLeft64, node, kShift64Imm);
}
#endif

void InstructionSelector::VisitWord32Shr(Node* node) {
  PPCOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().IsWord32And() && m.right().IsInRange(0, 31)) {
    // Try to absorb logical-and into rlwinm
    Int32BinopMatcher mleft(m.left().node());
    int sh = m.right().Value();
    int mb;
    int me;
    if (mleft.right().HasValue() &&
        IsContiguousMask32((uint32_t)(mleft.right().Value()) >> sh, &mb, &me)) {
      // Adjust the mask such that it doesn't include any rotated bits.
      if (mb > 31 - sh) mb = 31 - sh;
      sh = (32 - sh) & 0x1F;
      if (mb >= me) {
        Emit(kPPC_RotLeftAndMask32, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(sh),
             g.TempImmediate(mb), g.TempImmediate(me));
        return;
      }
    }
  }
  VisitRRO(this, kPPC_ShiftRight32, node, kShift32Imm);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitWord64Shr(Node* node) {
  PPCOperandGenerator g(this);
  Int64BinopMatcher m(node);
  if (m.left().IsWord64And() && m.right().IsInRange(0, 63)) {
    // Try to absorb logical-and into rldic
    Int64BinopMatcher mleft(m.left().node());
    int sh = m.right().Value();
    int mb;
    int me;
    if (mleft.right().HasValue() &&
        IsContiguousMask64((uint64_t)(mleft.right().Value()) >> sh, &mb, &me)) {
      // Adjust the mask such that it doesn't include any rotated bits.
      if (mb > 63 - sh) mb = 63 - sh;
      sh = (64 - sh) & 0x3F;
      if (mb >= me) {
        bool match = false;
        ArchOpcode opcode;
        int mask;
        if (me == 0) {
          match = true;
          opcode = kPPC_RotLeftAndClearLeft64;
          mask = mb;
        } else if (mb == 63) {
          match = true;
          opcode = kPPC_RotLeftAndClearRight64;
          mask = me;
        }
        if (match) {
          Emit(opcode, g.DefineAsRegister(node),
               g.UseRegister(mleft.left().node()), g.TempImmediate(sh),
               g.TempImmediate(mask));
          return;
        }
      }
    }
  }
  VisitRRO(this, kPPC_ShiftRight64, node, kShift64Imm);
}
#endif

void InstructionSelector::VisitWord32Sar(Node* node) {
  PPCOperandGenerator g(this);
  Int32BinopMatcher m(node);
  // Replace with sign extension for (x << K) >> K where K is 16 or 24.
  if (CanCover(node, m.left().node()) && m.left().IsWord32Shl()) {
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().Is(16) && m.right().Is(16)) {
      Emit(kPPC_ExtendSignWord16, g.DefineAsRegister(node),
           g.UseRegister(mleft.left().node()));
      return;
    } else if (mleft.right().Is(24) && m.right().Is(24)) {
      Emit(kPPC_ExtendSignWord8, g.DefineAsRegister(node),
           g.UseRegister(mleft.left().node()));
      return;
    }
  }
  VisitRRO(this, kPPC_ShiftRightAlg32, node, kShift32Imm);
}

#if !V8_TARGET_ARCH_PPC64
void VisitPairBinop(InstructionSelector* selector, InstructionCode opcode,
                    InstructionCode opcode2, Node* node) {
  PPCOperandGenerator g(selector);

  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    // We use UseUniqueRegister here to avoid register sharing with the output
    // registers.
    InstructionOperand inputs[] = {
        g.UseRegister(node->InputAt(0)), g.UseUniqueRegister(node->InputAt(1)),
        g.UseRegister(node->InputAt(2)), g.UseUniqueRegister(node->InputAt(3))};

    InstructionOperand outputs[] = {
        g.DefineAsRegister(node),
        g.DefineAsRegister(NodeProperties::FindProjection(node, 1))};

    selector->Emit(opcode, 2, outputs, 4, inputs);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    selector->Emit(opcode2, g.DefineSameAsFirst(node),
                   g.UseRegister(node->InputAt(0)),
                   g.UseRegister(node->InputAt(2)));
  }
}

void InstructionSelector::VisitInt32PairAdd(Node* node) {
  VisitPairBinop(this, kPPC_AddPair, kPPC_Add32, node);
}

void InstructionSelector::VisitInt32PairSub(Node* node) {
  VisitPairBinop(this, kPPC_SubPair, kPPC_Sub, node);
}

void InstructionSelector::VisitInt32PairMul(Node* node) {
  PPCOperandGenerator g(this);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    InstructionOperand inputs[] = {g.UseUniqueRegister(node->InputAt(0)),
                                   g.UseUniqueRegister(node->InputAt(1)),
                                   g.UseUniqueRegister(node->InputAt(2)),
                                   g.UseUniqueRegister(node->InputAt(3))};

    InstructionOperand outputs[] = {
        g.DefineAsRegister(node),
        g.DefineAsRegister(NodeProperties::FindProjection(node, 1))};

    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};

    Emit(kPPC_MulPair, 2, outputs, 4, inputs, 2, temps);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    Emit(kPPC_Mul32, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
         g.UseRegister(node->InputAt(2)));
  }
}

namespace {
// Shared routine for multiple shift operations.
void VisitPairShift(InstructionSelector* selector, InstructionCode opcode,
                    Node* node) {
  PPCOperandGenerator g(selector);
  // We use g.UseUniqueRegister here to guarantee that there is
  // no register aliasing of input registers with output registers.
  Int32Matcher m(node->InputAt(2));
  InstructionOperand shift_operand;
  if (m.HasValue()) {
    shift_operand = g.UseImmediate(m.node());
  } else {
    shift_operand = g.UseUniqueRegister(m.node());
  }

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
}  // namespace

void InstructionSelector::VisitWord32PairShl(Node* node) {
  VisitPairShift(this, kPPC_ShiftLeftPair, node);
}

void InstructionSelector::VisitWord32PairShr(Node* node) {
  VisitPairShift(this, kPPC_ShiftRightPair, node);
}

void InstructionSelector::VisitWord32PairSar(Node* node) {
  VisitPairShift(this, kPPC_ShiftRightAlgPair, node);
}
#endif

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitWord64Sar(Node* node) {
  PPCOperandGenerator g(this);
  Int64BinopMatcher m(node);
  if (CanCover(m.node(), m.left().node()) && m.left().IsLoad() &&
      m.right().Is(32)) {
    // Just load and sign-extend the interesting 4 bytes instead. This happens,
    // for example, when we're loading and untagging SMIs.
    BaseWithIndexAndDisplacement64Matcher mleft(m.left().node(),
                                                AddressOption::kAllowAll);
    if (mleft.matches() && mleft.index() == nullptr) {
      int64_t offset = 0;
      Node* displacement = mleft.displacement();
      if (displacement != nullptr) {
        Int64Matcher mdisplacement(displacement);
        DCHECK(mdisplacement.HasValue());
        offset = mdisplacement.Value();
      }
      offset = SmiWordOffset(offset);
      if (g.CanBeImmediate(offset, kInt16Imm_4ByteAligned)) {
        Emit(kPPC_LoadWordS32 | AddressingModeField::encode(kMode_MRI),
             g.DefineAsRegister(node), g.UseRegister(mleft.base()),
             g.TempImmediate(offset), g.UseImmediate(0));
        return;
      }
    }
  }
  VisitRRO(this, kPPC_ShiftRightAlg64, node, kShift64Imm);
}
#endif

// TODO(mbrandy): Absorb logical-and into rlwinm?
void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitRRO(this, kPPC_RotRight32, node, kShift32Imm);
}

#if V8_TARGET_ARCH_PPC64
// TODO(mbrandy): Absorb logical-and into rldic?
void InstructionSelector::VisitWord64Ror(Node* node) {
  VisitRRO(this, kPPC_RotRight64, node, kShift64Imm);
}
#endif

void InstructionSelector::VisitWord32Clz(Node* node) {
  PPCOperandGenerator g(this);
  Emit(kPPC_Cntlz32, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitWord64Clz(Node* node) {
  PPCOperandGenerator g(this);
  Emit(kPPC_Cntlz64, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
}
#endif

void InstructionSelector::VisitWord32Popcnt(Node* node) {
  PPCOperandGenerator g(this);
  Emit(kPPC_Popcnt32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitWord64Popcnt(Node* node) {
  PPCOperandGenerator g(this);
  Emit(kPPC_Popcnt64, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}
#endif

void InstructionSelector::VisitWord32Ctz(Node* node) { UNREACHABLE(); }

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitWord64Ctz(Node* node) { UNREACHABLE(); }
#endif

void InstructionSelector::VisitWord32ReverseBits(Node* node) { UNREACHABLE(); }

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitWord64ReverseBits(Node* node) { UNREACHABLE(); }
#endif

void InstructionSelector::VisitWord64ReverseBytes(Node* node) {
  PPCOperandGenerator g(this);
  InstructionOperand temp[] = {g.TempRegister()};
  Emit(kPPC_ByteRev64, g.DefineAsRegister(node),
       g.UseUniqueRegister(node->InputAt(0)), 1, temp);
}

void InstructionSelector::VisitWord32ReverseBytes(Node* node) {
  PPCOperandGenerator g(this);
  Emit(kPPC_ByteRev32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitSimd128ReverseBytes(Node* node) {
  // TODO(miladfar): Implement the ppc selector for reversing SIMD bytes.
  // Check if the input node is a Load and do a Load Reverse at once.
  UNIMPLEMENTED();
}

void InstructionSelector::VisitInt32Add(Node* node) {
  VisitBinop<Int32BinopMatcher>(this, node, kPPC_Add32, kInt16Imm);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitInt64Add(Node* node) {
  VisitBinop<Int64BinopMatcher>(this, node, kPPC_Add64, kInt16Imm);
}
#endif

void InstructionSelector::VisitInt32Sub(Node* node) {
  PPCOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().Is(0)) {
    Emit(kPPC_Neg, g.DefineAsRegister(node), g.UseRegister(m.right().node()));
  } else {
    VisitBinop<Int32BinopMatcher>(this, node, kPPC_Sub, kInt16Imm_Negate);
  }
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitInt64Sub(Node* node) {
  PPCOperandGenerator g(this);
  Int64BinopMatcher m(node);
  if (m.left().Is(0)) {
    Emit(kPPC_Neg, g.DefineAsRegister(node), g.UseRegister(m.right().node()));
  } else {
    VisitBinop<Int64BinopMatcher>(this, node, kPPC_Sub, kInt16Imm_Negate);
  }
}
#endif

namespace {

void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  InstructionOperand left, InstructionOperand right,
                  FlagsContinuation* cont);
void EmitInt32MulWithOverflow(InstructionSelector* selector, Node* node,
                              FlagsContinuation* cont) {
  PPCOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand result_operand = g.DefineAsRegister(node);
  InstructionOperand high32_operand = g.TempRegister();
  InstructionOperand temp_operand = g.TempRegister();
  {
    InstructionOperand outputs[] = {result_operand, high32_operand};
    InstructionOperand inputs[] = {g.UseRegister(m.left().node()),
                                   g.UseRegister(m.right().node())};
    selector->Emit(kPPC_Mul32WithHigh32, 2, outputs, 2, inputs);
  }
  {
    InstructionOperand shift_31 = g.UseImmediate(31);
    InstructionOperand outputs[] = {temp_operand};
    InstructionOperand inputs[] = {result_operand, shift_31};
    selector->Emit(kPPC_ShiftRightAlg32, 1, outputs, 2, inputs);
  }

  VisitCompare(selector, kPPC_Cmp32, high32_operand, temp_operand, cont);
}

}  // namespace

void InstructionSelector::VisitInt32Mul(Node* node) {
  VisitRRR(this, kPPC_Mul32, node);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitInt64Mul(Node* node) {
  VisitRRR(this, kPPC_Mul64, node);
}
#endif

void InstructionSelector::VisitInt32MulHigh(Node* node) {
  PPCOperandGenerator g(this);
  Emit(kPPC_MulHigh32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitUint32MulHigh(Node* node) {
  PPCOperandGenerator g(this);
  Emit(kPPC_MulHighU32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)));
}

void InstructionSelector::VisitInt32Div(Node* node) {
  VisitRRR(this, kPPC_Div32, node);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitInt64Div(Node* node) {
  VisitRRR(this, kPPC_Div64, node);
}
#endif

void InstructionSelector::VisitUint32Div(Node* node) {
  VisitRRR(this, kPPC_DivU32, node);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitUint64Div(Node* node) {
  VisitRRR(this, kPPC_DivU64, node);
}
#endif

void InstructionSelector::VisitInt32Mod(Node* node) {
  VisitRRR(this, kPPC_Mod32, node);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitInt64Mod(Node* node) {
  VisitRRR(this, kPPC_Mod64, node);
}
#endif

void InstructionSelector::VisitUint32Mod(Node* node) {
  VisitRRR(this, kPPC_ModU32, node);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitUint64Mod(Node* node) {
  VisitRRR(this, kPPC_ModU64, node);
}
#endif

void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  VisitRR(this, kPPC_Float32ToDouble, node);
}

void InstructionSelector::VisitRoundInt32ToFloat32(Node* node) {
  VisitRR(this, kPPC_Int32ToFloat32, node);
}

void InstructionSelector::VisitRoundUint32ToFloat32(Node* node) {
  VisitRR(this, kPPC_Uint32ToFloat32, node);
}

void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  VisitRR(this, kPPC_Int32ToDouble, node);
}

void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  VisitRR(this, kPPC_Uint32ToDouble, node);
}

void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  VisitRR(this, kPPC_DoubleToInt32, node);
}

void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  VisitRR(this, kPPC_DoubleToUint32, node);
}

void InstructionSelector::VisitTruncateFloat64ToUint32(Node* node) {
  VisitRR(this, kPPC_DoubleToUint32, node);
}

void InstructionSelector::VisitSignExtendWord8ToInt32(Node* node) {
  // TODO(mbrandy): inspect input to see if nop is appropriate.
  VisitRR(this, kPPC_ExtendSignWord8, node);
}

void InstructionSelector::VisitSignExtendWord16ToInt32(Node* node) {
  // TODO(mbrandy): inspect input to see if nop is appropriate.
  VisitRR(this, kPPC_ExtendSignWord16, node);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitTryTruncateFloat32ToInt64(Node* node) {
  VisitTryTruncateDouble(this, kPPC_DoubleToInt64, node);
}

void InstructionSelector::VisitTryTruncateFloat64ToInt64(Node* node) {
  VisitTryTruncateDouble(this, kPPC_DoubleToInt64, node);
}

void InstructionSelector::VisitTruncateFloat64ToInt64(Node* node) {
  VisitRR(this, kPPC_DoubleToInt64, node);
}

void InstructionSelector::VisitTryTruncateFloat32ToUint64(Node* node) {
  VisitTryTruncateDouble(this, kPPC_DoubleToUint64, node);
}

void InstructionSelector::VisitTryTruncateFloat64ToUint64(Node* node) {
  VisitTryTruncateDouble(this, kPPC_DoubleToUint64, node);
}

void InstructionSelector::VisitBitcastWord32ToWord64(Node* node) {
  DCHECK(SmiValuesAre31Bits());
  DCHECK(COMPRESS_POINTERS_BOOL);
  EmitIdentity(node);
}

void InstructionSelector::VisitChangeInt32ToInt64(Node* node) {
  // TODO(mbrandy): inspect input to see if nop is appropriate.
  VisitRR(this, kPPC_ExtendSignWord32, node);
}

void InstructionSelector::VisitSignExtendWord8ToInt64(Node* node) {
  // TODO(mbrandy): inspect input to see if nop is appropriate.
  VisitRR(this, kPPC_ExtendSignWord8, node);
}

void InstructionSelector::VisitSignExtendWord16ToInt64(Node* node) {
  // TODO(mbrandy): inspect input to see if nop is appropriate.
  VisitRR(this, kPPC_ExtendSignWord16, node);
}

void InstructionSelector::VisitSignExtendWord32ToInt64(Node* node) {
  // TODO(mbrandy): inspect input to see if nop is appropriate.
  VisitRR(this, kPPC_ExtendSignWord32, node);
}

void InstructionSelector::VisitChangeUint32ToUint64(Node* node) {
  // TODO(mbrandy): inspect input to see if nop is appropriate.
  VisitRR(this, kPPC_Uint32ToUint64, node);
}

void InstructionSelector::VisitChangeFloat64ToUint64(Node* node) {
  VisitRR(this, kPPC_DoubleToUint64, node);
}

void InstructionSelector::VisitChangeFloat64ToInt64(Node* node) {
  VisitRR(this, kPPC_DoubleToInt64, node);
}
#endif

void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  VisitRR(this, kPPC_DoubleToFloat32, node);
}

void InstructionSelector::VisitTruncateFloat64ToWord32(Node* node) {
  VisitRR(this, kArchTruncateDoubleToI, node);
}

void InstructionSelector::VisitRoundFloat64ToInt32(Node* node) {
  VisitRR(this, kPPC_DoubleToInt32, node);
}

void InstructionSelector::VisitTruncateFloat32ToInt32(Node* node) {
  VisitRR(this, kPPC_DoubleToInt32, node);
}

void InstructionSelector::VisitTruncateFloat32ToUint32(Node* node) {
  VisitRR(this, kPPC_DoubleToUint32, node);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  // TODO(mbrandy): inspect input to see if nop is appropriate.
  VisitRR(this, kPPC_Int64ToInt32, node);
}

void InstructionSelector::VisitRoundInt64ToFloat32(Node* node) {
  VisitRR(this, kPPC_Int64ToFloat32, node);
}

void InstructionSelector::VisitRoundInt64ToFloat64(Node* node) {
  VisitRR(this, kPPC_Int64ToDouble, node);
}

void InstructionSelector::VisitChangeInt64ToFloat64(Node* node) {
  VisitRR(this, kPPC_Int64ToDouble, node);
}

void InstructionSelector::VisitRoundUint64ToFloat32(Node* node) {
  VisitRR(this, kPPC_Uint64ToFloat32, node);
}

void InstructionSelector::VisitRoundUint64ToFloat64(Node* node) {
  VisitRR(this, kPPC_Uint64ToDouble, node);
}
#endif

void InstructionSelector::VisitBitcastFloat32ToInt32(Node* node) {
  VisitRR(this, kPPC_BitcastFloat32ToInt32, node);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitBitcastFloat64ToInt64(Node* node) {
  VisitRR(this, kPPC_BitcastDoubleToInt64, node);
}
#endif

void InstructionSelector::VisitBitcastInt32ToFloat32(Node* node) {
  VisitRR(this, kPPC_BitcastInt32ToFloat32, node);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitBitcastInt64ToFloat64(Node* node) {
  VisitRR(this, kPPC_BitcastInt64ToDouble, node);
}
#endif

void InstructionSelector::VisitFloat32Add(Node* node) {
  VisitRRR(this, kPPC_AddDouble | MiscField::encode(1), node);
}

void InstructionSelector::VisitFloat64Add(Node* node) {
  // TODO(mbrandy): detect multiply-add
  VisitRRR(this, kPPC_AddDouble, node);
}

void InstructionSelector::VisitFloat32Sub(Node* node) {
  VisitRRR(this, kPPC_SubDouble | MiscField::encode(1), node);
}

void InstructionSelector::VisitFloat64Sub(Node* node) {
  // TODO(mbrandy): detect multiply-subtract
  VisitRRR(this, kPPC_SubDouble, node);
}

void InstructionSelector::VisitFloat32Mul(Node* node) {
  VisitRRR(this, kPPC_MulDouble | MiscField::encode(1), node);
}

void InstructionSelector::VisitFloat64Mul(Node* node) {
  // TODO(mbrandy): detect negate
  VisitRRR(this, kPPC_MulDouble, node);
}

void InstructionSelector::VisitFloat32Div(Node* node) {
  VisitRRR(this, kPPC_DivDouble | MiscField::encode(1), node);
}

void InstructionSelector::VisitFloat64Div(Node* node) {
  VisitRRR(this, kPPC_DivDouble, node);
}

void InstructionSelector::VisitFloat64Mod(Node* node) {
  PPCOperandGenerator g(this);
  Emit(kPPC_ModDouble, g.DefineAsFixed(node, d1),
       g.UseFixed(node->InputAt(0), d1), g.UseFixed(node->InputAt(1), d2))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat32Max(Node* node) {
  VisitRRR(this, kPPC_MaxDouble | MiscField::encode(1), node);
}

void InstructionSelector::VisitFloat64Max(Node* node) {
  VisitRRR(this, kPPC_MaxDouble, node);
}

void InstructionSelector::VisitFloat64SilenceNaN(Node* node) {
  VisitRR(this, kPPC_Float64SilenceNaN, node);
}

void InstructionSelector::VisitFloat32Min(Node* node) {
  VisitRRR(this, kPPC_MinDouble | MiscField::encode(1), node);
}

void InstructionSelector::VisitFloat64Min(Node* node) {
  VisitRRR(this, kPPC_MinDouble, node);
}

void InstructionSelector::VisitFloat32Abs(Node* node) {
  VisitRR(this, kPPC_AbsDouble | MiscField::encode(1), node);
}

void InstructionSelector::VisitFloat64Abs(Node* node) {
  VisitRR(this, kPPC_AbsDouble, node);
}

void InstructionSelector::VisitFloat32Sqrt(Node* node) {
  VisitRR(this, kPPC_SqrtDouble | MiscField::encode(1), node);
}

void InstructionSelector::VisitFloat64Ieee754Unop(Node* node,
                                                  InstructionCode opcode) {
  PPCOperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, d1), g.UseFixed(node->InputAt(0), d1))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat64Ieee754Binop(Node* node,
                                                   InstructionCode opcode) {
  PPCOperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, d1), g.UseFixed(node->InputAt(0), d1),
       g.UseFixed(node->InputAt(1), d2))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  VisitRR(this, kPPC_SqrtDouble, node);
}

void InstructionSelector::VisitFloat32RoundDown(Node* node) {
  VisitRR(this, kPPC_FloorDouble | MiscField::encode(1), node);
}

void InstructionSelector::VisitFloat64RoundDown(Node* node) {
  VisitRR(this, kPPC_FloorDouble, node);
}

void InstructionSelector::VisitFloat32RoundUp(Node* node) {
  VisitRR(this, kPPC_CeilDouble | MiscField::encode(1), node);
}

void InstructionSelector::VisitFloat64RoundUp(Node* node) {
  VisitRR(this, kPPC_CeilDouble, node);
}

void InstructionSelector::VisitFloat32RoundTruncate(Node* node) {
  VisitRR(this, kPPC_TruncateDouble | MiscField::encode(1), node);
}

void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  VisitRR(this, kPPC_TruncateDouble, node);
}

void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  VisitRR(this, kPPC_RoundDouble, node);
}

void InstructionSelector::VisitFloat32RoundTiesEven(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitFloat64RoundTiesEven(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitFloat32Neg(Node* node) {
  VisitRR(this, kPPC_NegDouble, node);
}

void InstructionSelector::VisitFloat64Neg(Node* node) {
  VisitRR(this, kPPC_NegDouble, node);
}

void InstructionSelector::VisitInt32AddWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop<Int32BinopMatcher>(this, node, kPPC_AddWithOverflow32,
                                         kInt16Imm, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int32BinopMatcher>(this, node, kPPC_AddWithOverflow32, kInt16Imm,
                                &cont);
}

void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop<Int32BinopMatcher>(this, node, kPPC_SubWithOverflow32,
                                         kInt16Imm_Negate, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int32BinopMatcher>(this, node, kPPC_SubWithOverflow32,
                                kInt16Imm_Negate, &cont);
}

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitInt64AddWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop<Int64BinopMatcher>(this, node, kPPC_Add64, kInt16Imm,
                                         &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int64BinopMatcher>(this, node, kPPC_Add64, kInt16Imm, &cont);
}

void InstructionSelector::VisitInt64SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop<Int64BinopMatcher>(this, node, kPPC_Sub, kInt16Imm_Negate,
                                         &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int64BinopMatcher>(this, node, kPPC_Sub, kInt16Imm_Negate, &cont);
}
#endif

static bool CompareLogical(FlagsContinuation* cont) {
  switch (cont->condition()) {
    case kUnsignedLessThan:
    case kUnsignedGreaterThanOrEqual:
    case kUnsignedLessThanOrEqual:
    case kUnsignedGreaterThan:
      return true;
    default:
      return false;
  }
  UNREACHABLE();
}

namespace {

// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  InstructionOperand left, InstructionOperand right,
                  FlagsContinuation* cont) {
  selector->EmitWithContinuation(opcode, left, right, cont);
}

// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont,
                      bool commutative, ImmediateMode immediate_mode) {
  PPCOperandGenerator g(selector);
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

void VisitWord32Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  ImmediateMode mode = (CompareLogical(cont) ? kInt16Imm_Unsigned : kInt16Imm);
  VisitWordCompare(selector, node, kPPC_Cmp32, cont, false, mode);
}

#if V8_TARGET_ARCH_PPC64
void VisitWord64Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  ImmediateMode mode = (CompareLogical(cont) ? kInt16Imm_Unsigned : kInt16Imm);
  VisitWordCompare(selector, node, kPPC_Cmp64, cont, false, mode);
}
#endif

// Shared routine for multiple float32 compare operations.
void VisitFloat32Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  PPCOperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  VisitCompare(selector, kPPC_CmpDouble, g.UseRegister(left),
               g.UseRegister(right), cont);
}

// Shared routine for multiple float64 compare operations.
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  PPCOperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  VisitCompare(selector, kPPC_CmpDouble, g.UseRegister(left),
               g.UseRegister(right), cont);
}

}  // namespace

// Shared routine for word comparisons against zero.
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
#if V8_TARGET_ARCH_PPC64
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
#endif
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
                return VisitBinop<Int32BinopMatcher>(
                    this, node, kPPC_AddWithOverflow32, kInt16Imm, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int32BinopMatcher>(
                    this, node, kPPC_SubWithOverflow32, kInt16Imm_Negate, cont);
              case IrOpcode::kInt32MulWithOverflow:
                cont->OverwriteAndNegateIfEqual(kNotEqual);
                return EmitInt32MulWithOverflow(this, node, cont);
#if V8_TARGET_ARCH_PPC64
              case IrOpcode::kInt64AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int64BinopMatcher>(this, node, kPPC_Add64,
                                                     kInt16Imm, cont);
              case IrOpcode::kInt64SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int64BinopMatcher>(this, node, kPPC_Sub,
                                                     kInt16Imm_Negate, cont);
#endif
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Sub:
        return VisitWord32Compare(this, value, cont);
      case IrOpcode::kWord32And:
        // TODO(mbandy): opportunity for rlwinm?
        return VisitWordCompare(this, value, kPPC_Tst32, cont, true,
                                kInt16Imm_Unsigned);
// TODO(mbrandy): Handle?
// case IrOpcode::kInt32Add:
// case IrOpcode::kWord32Or:
// case IrOpcode::kWord32Xor:
// case IrOpcode::kWord32Sar:
// case IrOpcode::kWord32Shl:
// case IrOpcode::kWord32Shr:
// case IrOpcode::kWord32Ror:
#if V8_TARGET_ARCH_PPC64
      case IrOpcode::kInt64Sub:
        return VisitWord64Compare(this, value, cont);
      case IrOpcode::kWord64And:
        // TODO(mbandy): opportunity for rldic?
        return VisitWordCompare(this, value, kPPC_Tst64, cont, true,
                                kInt16Imm_Unsigned);
// TODO(mbrandy): Handle?
// case IrOpcode::kInt64Add:
// case IrOpcode::kWord64Or:
// case IrOpcode::kWord64Xor:
// case IrOpcode::kWord64Sar:
// case IrOpcode::kWord64Shl:
// case IrOpcode::kWord64Shr:
// case IrOpcode::kWord64Ror:
#endif
      case IrOpcode::kStackPointerGreaterThan:
        cont->OverwriteAndNegateIfEqual(kStackPointerGreaterThanCondition);
        return VisitStackPointerGreaterThan(value, cont);
      default:
        break;
    }
  }

  // Branch could not be combined with a compare, emit compare against 0.
  PPCOperandGenerator g(this);
  VisitCompare(this, kPPC_Cmp32, g.UseRegister(value), g.TempImmediate(0),
               cont);
}

void InstructionSelector::VisitSwitch(Node* node, const SwitchInfo& sw) {
  PPCOperandGenerator g(this);
  InstructionOperand value_operand = g.UseRegister(node->InputAt(0));

  // Emit either ArchTableSwitch or ArchLookupSwitch.
  if (enable_switch_jump_table_ == kEnableSwitchJumpTable) {
    static const size_t kMaxTableSwitchValueRange = 2 << 16;
    size_t table_space_cost = 4 + sw.value_range();
    size_t table_time_cost = 3;
    size_t lookup_space_cost = 3 + 2 * sw.case_count();
    size_t lookup_time_cost = sw.case_count();
    if (sw.case_count() > 0 &&
        table_space_cost + 3 * table_time_cost <=
            lookup_space_cost + 3 * lookup_time_cost &&
        sw.min_value() > std::numeric_limits<int32_t>::min() &&
        sw.value_range() <= kMaxTableSwitchValueRange) {
      InstructionOperand index_operand = value_operand;
      if (sw.min_value()) {
        index_operand = g.TempRegister();
        Emit(kPPC_Sub, index_operand, value_operand,
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
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
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

#if V8_TARGET_ARCH_PPC64
void InstructionSelector::VisitWord64Equal(Node* const node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
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
#endif

void InstructionSelector::VisitInt32MulWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kNotEqual, ovf);
    return EmitInt32MulWithOverflow(this, node, &cont);
  }
  FlagsContinuation cont;
  EmitInt32MulWithOverflow(this, node, &cont);
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

void InstructionSelector::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* call_descriptor,
    Node* node) {
  PPCOperandGenerator g(this);

  // Prepare for C function call.
  if (call_descriptor->IsCFunctionCall()) {
    Emit(kArchPrepareCallCFunction | MiscField::encode(static_cast<int>(
                                         call_descriptor->ParameterCount())),
         0, nullptr, 0, nullptr);

    // Poke any stack arguments.
    int slot = kStackFrameExtraParamSlot;
    for (PushParameter input : (*arguments)) {
      if (input.node == nullptr) continue;
      Emit(kPPC_StoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
           g.TempImmediate(slot));
      ++slot;
    }
  } else {
    // Push any stack arguments.
    for (PushParameter input : base::Reversed(*arguments)) {
      // Skip any alignment holes in pushed nodes.
      if (input.node == nullptr) continue;
      Emit(kPPC_Push, g.NoOutput(), g.UseRegister(input.node));
    }
  }
}

bool InstructionSelector::IsTailCallAddressImmediate() { return false; }

int InstructionSelector::GetTempsCountForTailCallFromJSFunction() { return 3; }

void InstructionSelector::VisitFloat64ExtractLowWord32(Node* node) {
  PPCOperandGenerator g(this);
  Emit(kPPC_DoubleExtractLowWord32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitFloat64ExtractHighWord32(Node* node) {
  PPCOperandGenerator g(this);
  Emit(kPPC_DoubleExtractHighWord32, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitFloat64InsertLowWord32(Node* node) {
  PPCOperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (left->opcode() == IrOpcode::kFloat64InsertHighWord32 &&
      CanCover(node, left)) {
    left = left->InputAt(1);
    Emit(kPPC_DoubleConstruct, g.DefineAsRegister(node), g.UseRegister(left),
         g.UseRegister(right));
    return;
  }
  Emit(kPPC_DoubleInsertLowWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.UseRegister(right));
}

void InstructionSelector::VisitFloat64InsertHighWord32(Node* node) {
  PPCOperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (left->opcode() == IrOpcode::kFloat64InsertLowWord32 &&
      CanCover(node, left)) {
    left = left->InputAt(1);
    Emit(kPPC_DoubleConstruct, g.DefineAsRegister(node), g.UseRegister(right),
         g.UseRegister(left));
    return;
  }
  Emit(kPPC_DoubleInsertHighWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.UseRegister(right));
}

void InstructionSelector::VisitMemoryBarrier(Node* node) {
  PPCOperandGenerator g(this);
  Emit(kPPC_Sync, g.NoOutput());
}

void InstructionSelector::VisitWord32AtomicLoad(Node* node) { VisitLoad(node); }

void InstructionSelector::VisitWord64AtomicLoad(Node* node) { VisitLoad(node); }

void InstructionSelector::VisitWord32AtomicStore(Node* node) {
  VisitStore(node);
}

void InstructionSelector::VisitWord64AtomicStore(Node* node) {
  VisitStore(node);
}

void VisitAtomicExchange(InstructionSelector* selector, Node* node,
                         ArchOpcode opcode) {
  PPCOperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  AddressingMode addressing_mode = kMode_MRR;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);
  InstructionOperand outputs[1];
  outputs[0] = g.UseUniqueRegister(node);
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  selector->Emit(code, 1, outputs, input_count, inputs);
}

void InstructionSelector::VisitWord32AtomicExchange(Node* node) {
  ArchOpcode opcode = kArchNop;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Int8()) {
    opcode = kWord32AtomicExchangeInt8;
  } else if (type == MachineType::Uint8()) {
    opcode = kPPC_AtomicExchangeUint8;
  } else if (type == MachineType::Int16()) {
    opcode = kWord32AtomicExchangeInt16;
  } else if (type == MachineType::Uint16()) {
    opcode = kPPC_AtomicExchangeUint16;
  } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
    opcode = kPPC_AtomicExchangeWord32;
  } else {
    UNREACHABLE();
    return;
  }
  VisitAtomicExchange(this, node, opcode);
}

void InstructionSelector::VisitWord64AtomicExchange(Node* node) {
  ArchOpcode opcode = kArchNop;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Uint8()) {
    opcode = kPPC_AtomicExchangeUint8;
  } else if (type == MachineType::Uint16()) {
    opcode = kPPC_AtomicExchangeUint16;
  } else if (type == MachineType::Uint32()) {
    opcode = kPPC_AtomicExchangeWord32;
  } else if (type == MachineType::Uint64()) {
    opcode = kPPC_AtomicExchangeWord64;
  } else {
    UNREACHABLE();
    return;
  }
  VisitAtomicExchange(this, node, opcode);
}

void VisitAtomicCompareExchange(InstructionSelector* selector, Node* node,
                                ArchOpcode opcode) {
  PPCOperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* old_value = node->InputAt(2);
  Node* new_value = node->InputAt(3);

  AddressingMode addressing_mode = kMode_MRR;
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);

  InstructionOperand inputs[4];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(old_value);
  inputs[input_count++] = g.UseUniqueRegister(new_value);

  InstructionOperand outputs[1];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  selector->Emit(code, output_count, outputs, input_count, inputs);
}

void InstructionSelector::VisitWord32AtomicCompareExchange(Node* node) {
  MachineType type = AtomicOpType(node->op());
  ArchOpcode opcode = kArchNop;
  if (type == MachineType::Int8()) {
    opcode = kWord32AtomicCompareExchangeInt8;
  } else if (type == MachineType::Uint8()) {
    opcode = kPPC_AtomicCompareExchangeUint8;
  } else if (type == MachineType::Int16()) {
    opcode = kWord32AtomicCompareExchangeInt16;
  } else if (type == MachineType::Uint16()) {
    opcode = kPPC_AtomicCompareExchangeUint16;
  } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
    opcode = kPPC_AtomicCompareExchangeWord32;
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
    opcode = kPPC_AtomicCompareExchangeUint8;
  } else if (type == MachineType::Uint16()) {
    opcode = kPPC_AtomicCompareExchangeUint16;
  } else if (type == MachineType::Uint32()) {
    opcode = kPPC_AtomicCompareExchangeWord32;
  } else if (type == MachineType::Uint64()) {
    opcode = kPPC_AtomicCompareExchangeWord64;
  } else {
    UNREACHABLE();
    return;
  }
  VisitAtomicCompareExchange(this, node, opcode);
}

void VisitAtomicBinaryOperation(InstructionSelector* selector, Node* node,
                                ArchOpcode int8_op, ArchOpcode uint8_op,
                                ArchOpcode int16_op, ArchOpcode uint16_op,
                                ArchOpcode int32_op, ArchOpcode uint32_op,
                                ArchOpcode int64_op, ArchOpcode uint64_op) {
  PPCOperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
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
  } else if (type == MachineType::Int32()) {
    opcode = int32_op;
  } else if (type == MachineType::Uint32()) {
    opcode = uint32_op;
  } else if (type == MachineType::Int64()) {
    opcode = int64_op;
  } else if (type == MachineType::Uint64()) {
    opcode = uint64_op;
  } else {
    UNREACHABLE();
    return;
  }

  AddressingMode addressing_mode = kMode_MRR;
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  InstructionOperand inputs[3];

  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);

  InstructionOperand outputs[1];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  selector->Emit(code, output_count, outputs, input_count, inputs);
}

void InstructionSelector::VisitWord32AtomicBinaryOperation(
    Node* node, ArchOpcode int8_op, ArchOpcode uint8_op, ArchOpcode int16_op,
    ArchOpcode uint16_op, ArchOpcode word32_op) {
  // Unused
  UNREACHABLE();
}

void InstructionSelector::VisitWord64AtomicBinaryOperation(
    Node* node, ArchOpcode uint8_op, ArchOpcode uint16_op, ArchOpcode uint32_op,
    ArchOpcode uint64_op) {
  // Unused
  UNREACHABLE();
}

#define VISIT_ATOMIC_BINOP(op)                                     \
  void InstructionSelector::VisitWord32Atomic##op(Node* node) {    \
    VisitAtomicBinaryOperation(                                    \
        this, node, kPPC_Atomic##op##Int8, kPPC_Atomic##op##Uint8, \
        kPPC_Atomic##op##Int16, kPPC_Atomic##op##Uint16,           \
        kPPC_Atomic##op##Int32, kPPC_Atomic##op##Uint32,           \
        kPPC_Atomic##op##Int64, kPPC_Atomic##op##Uint64);          \
  }                                                                \
  void InstructionSelector::VisitWord64Atomic##op(Node* node) {    \
    VisitAtomicBinaryOperation(                                    \
        this, node, kPPC_Atomic##op##Int8, kPPC_Atomic##op##Uint8, \
        kPPC_Atomic##op##Int16, kPPC_Atomic##op##Uint16,           \
        kPPC_Atomic##op##Int32, kPPC_Atomic##op##Uint32,           \
        kPPC_Atomic##op##Int64, kPPC_Atomic##op##Uint64);          \
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

#define SIMD_VISIT_EXTRACT_LANE(Type, Sign)                              \
  void InstructionSelector::Visit##Type##ExtractLane##Sign(Node* node) { \
    UNIMPLEMENTED();                                                     \
  }
SIMD_VISIT_EXTRACT_LANE(F64x2, )
SIMD_VISIT_EXTRACT_LANE(F32x4, )
SIMD_VISIT_EXTRACT_LANE(I32x4, )
SIMD_VISIT_EXTRACT_LANE(I16x8, U)
SIMD_VISIT_EXTRACT_LANE(I16x8, S)
SIMD_VISIT_EXTRACT_LANE(I8x16, U)
SIMD_VISIT_EXTRACT_LANE(I8x16, S)
#undef SIMD_VISIT_EXTRACT_LANE

void InstructionSelector::VisitI32x4Splat(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4ReplaceLane(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Add(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Sub(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Shl(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4ShrS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Mul(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4MaxS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4MinS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Eq(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Ne(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4MinU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4MaxU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4ShrU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Neg(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4GtS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4GeS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4GtU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4GeU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8Splat(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8ReplaceLane(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8Shl(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8ShrS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8ShrU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8Add(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8AddSaturateS(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8Sub(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8SubSaturateS(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8Mul(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8MinS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8MaxS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8Eq(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8Ne(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8AddSaturateU(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8SubSaturateU(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8MinU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8MaxU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8Neg(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8GtS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8GeS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8GtU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8GeU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8RoundingAverageU(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16RoundingAverageU(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16Neg(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16Splat(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16ReplaceLane(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16Add(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16AddSaturateS(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16Sub(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16SubSaturateS(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16MinS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16MaxS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16Eq(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16Ne(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16GtS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16GeS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16AddSaturateU(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16SubSaturateU(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16MinU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16MaxU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16GtU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16GeU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS128And(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS128Or(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS128Xor(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS128Not(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS128AndNot(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS128Zero(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Eq(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Ne(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Lt(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Le(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Splat(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4ReplaceLane(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::EmitPrepareResults(
    ZoneVector<PushParameter>* results, const CallDescriptor* call_descriptor,
    Node* node) {
  PPCOperandGenerator g(this);

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
      Emit(kPPC_Peek, g.DefineAsRegister(output.node),
           g.UseImmediate(reverse_slot));
    }
    reverse_slot += output.location.GetSizeInPointers();
  }
}

void InstructionSelector::VisitF32x4Add(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Sub(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Mul(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Sqrt(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Div(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Min(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Max(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS128Select(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Neg(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Abs(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4RecipSqrtApprox(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitF32x4RecipApprox(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4AddHoriz(Node* node) { UNIMPLEMENTED(); }
void InstructionSelector::VisitI32x4AddHoriz(Node* node) { UNIMPLEMENTED(); }
void InstructionSelector::VisitI16x8AddHoriz(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4SConvertI32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitF32x4UConvertI32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI32x4SConvertF32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI32x4UConvertF32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI32x4SConvertI16x8Low(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI32x4SConvertI16x8High(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI32x4UConvertI16x8Low(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI32x4UConvertI16x8High(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8SConvertI8x16Low(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8SConvertI8x16High(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8UConvertI8x16Low(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8UConvertI8x16High(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8SConvertI32x4(Node* node) {
  UNIMPLEMENTED();
}
void InstructionSelector::VisitI16x8UConvertI32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16SConvertI16x8(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16UConvertI16x8(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitS1x4AnyTrue(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS1x4AllTrue(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS1x8AnyTrue(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS1x8AllTrue(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS1x16AnyTrue(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS1x16AllTrue(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16Shl(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16ShrS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16ShrU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16Mul(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS8x16Shuffle(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS8x16Swizzle(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Splat(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2ReplaceLane(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Abs(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Neg(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Sqrt(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Add(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Sub(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Mul(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Div(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Eq(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Ne(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Lt(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Le(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI64x2Neg(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI64x2Add(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI64x2Sub(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI64x2Shl(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI64x2ShrS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI64x2ShrU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI64x2Mul(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Min(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF64x2Max(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitLoadTransform(Node* node) { UNIMPLEMENTED(); }

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
         MachineOperatorBuilder::kWord32Popcnt |
         MachineOperatorBuilder::kWord64Popcnt;
  // We omit kWord32ShiftIsSafe as s[rl]w use 0x3F as a mask rather than 0x1F.
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
