// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/codegen/machine-type.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"

namespace v8 {
namespace internal {
namespace compiler {

using namespace turboshaft;  // NOLINT(build/namespaces)

#define TRACE(...) PrintF(__VA_ARGS__)

// Adds Mips-specific methods for generating InstructionOperands.
class Mips64OperandGeneratorT final : public OperandGeneratorT {
 public:
  explicit Mips64OperandGeneratorT(InstructionSelectorT* selector)
      : OperandGeneratorT(selector) {}

  InstructionOperand UseOperand(OpIndex node, InstructionCode opcode) {
    if (CanBeImmediate(node, opcode)) {
      return UseImmediate(node);
    }
    return UseRegister(node);
  }

  // Use the zero register if the node has the immediate value zero, otherwise
  // assign a register.
  InstructionOperand UseRegisterOrImmediateZero(OpIndex node) {
    if (const ConstantOp* constant =
            selector()->Get(node).TryCast<ConstantOp>()) {
      if ((constant->IsIntegral() && constant->integral() == 0) ||
          (constant->kind == ConstantOp::Kind::kFloat32 &&
           constant->float32().get_bits() == 0) ||
          (constant->kind == ConstantOp::Kind::kFloat64 &&
           constant->float64().get_bits() == 0)) {
        return UseImmediate(node);
      }
    }
    return UseRegister(node);
  }

  bool IsIntegerConstant(OpIndex node) {
    int64_t unused;
    return selector()->MatchSignedIntegralConstant(node, &unused);
  }

  std::optional<int64_t> GetOptionalIntegerConstant(OpIndex operation) {
    if (int64_t constant; MatchSignedIntegralConstant(operation, &constant)) {
      return constant;
    }
    return std::nullopt;
  }

  bool CanBeImmediate(OpIndex node, InstructionCode mode) {
    const ConstantOp* constant = selector()->Get(node).TryCast<ConstantOp>();
    if (!constant) return false;

    int64_t value;
    return selector()->MatchSignedIntegralConstant(node, &value) &&
           CanBeImmediate(value, mode);
  }

  bool CanBeImmediate(int64_t value, InstructionCode opcode) {
    switch (ArchOpcodeField::decode(opcode)) {
      case kMips64Shl:
      case kMips64Sar:
      case kMips64Shr:
        return is_uint5(value);
      case kMips64Dshl:
      case kMips64Dsar:
      case kMips64Dshr:
        return is_uint6(value);
      case kMips64Add:
      case kMips64And32:
      case kMips64And:
      case kMips64Dadd:
      case kMips64Or32:
      case kMips64Or:
      case kMips64Tst:
      case kMips64Xor:
        return is_uint16(value);
      case kMips64Lb:
      case kMips64Lbu:
      case kMips64Sb:
      case kMips64Lh:
      case kMips64Lhu:
      case kMips64Sh:
      case kMips64Lw:
      case kMips64Sw:
      case kMips64Ld:
      case kMips64Sd:
      case kMips64Lwc1:
      case kMips64Swc1:
      case kMips64Ldc1:
      case kMips64Sdc1:
        return is_int32(value);
      default:
        return is_int16(value);
    }
  }

 private:
  bool ImmediateFitsAddrMode1Instruction(int32_t imm) const {
    TRACE("UNIMPLEMENTED instr_sel: %s at line %d\n", __FUNCTION__, __LINE__);
    return false;
  }
};

static void VisitRR(InstructionSelectorT* selector, ArchOpcode opcode,
                    OpIndex node) {
  Mips64OperandGeneratorT g(selector);
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 1);
  selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

#if V8_ENABLE_WEBASSEMBLY
static void VisitRRI(InstructionSelectorT* selector, ArchOpcode opcode,
                     OpIndex node) {
  UNIMPLEMENTED();
}

static void VisitSimdShift(InstructionSelectorT* selector, ArchOpcode opcode,
                           OpIndex node) {
  UNIMPLEMENTED();
}

static void VisitRRIR(InstructionSelectorT* selector, ArchOpcode opcode,
                      OpIndex node) {
  UNIMPLEMENTED();
}

static void VisitUniqueRRR(InstructionSelectorT* selector, ArchOpcode opcode,
                           OpIndex node) {
  UNIMPLEMENTED();
}
#endif  // V8_ENABLE_WEBASSEMBLY

void VisitRRR(InstructionSelectorT* selector, ArchOpcode opcode, OpIndex node) {
  Mips64OperandGeneratorT g(selector);
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 2);
  selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
                 g.UseRegister(op.input(1)));
}

void VisitRRRR(InstructionSelectorT* selector, ArchOpcode opcode,
               OpIndex node) {
  UNIMPLEMENTED();
}

static void VisitRRO(InstructionSelectorT* selector, ArchOpcode opcode,
                     OpIndex node) {
  Mips64OperandGeneratorT g(selector);
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 2);
  selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)),
                 g.UseOperand(op.input(1), opcode));
}

struct ExtendingLoadMatcher {
  ExtendingLoadMatcher(OpIndex node, InstructionSelectorT* selector)
      : matches_(false), selector_(selector), immediate_(0) {
    Initialize(node);
  }

  bool Matches() const { return matches_; }

  OpIndex base() const {
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
  InstructionSelectorT* selector_;
  OpIndex base_{};
  int64_t immediate_;
  ArchOpcode opcode_;

  void Initialize(turboshaft::OpIndex node) {
    const ShiftOp& shift = selector_->Get(node).template Cast<ShiftOp>();
    DCHECK(shift.kind == ShiftOp::Kind::kShiftRightArithmetic ||
           shift.kind == ShiftOp::Kind::kShiftRightArithmeticShiftOutZeros);
    // When loading a 64-bit value and shifting by 32, we should
    // just load and sign-extend the interesting 4 bytes instead.
    // This happens, for example, when we're loading and untagging SMIs.
    const Operation& lhs = selector_->Get(shift.left());
    int64_t constant_rhs;

    if (lhs.Is<LoadOp>() &&
        selector_->MatchIntegralWord64Constant(shift.right(), &constant_rhs) &&
        constant_rhs == 32 && selector_->CanCover(node, shift.left())) {
      Mips64OperandGeneratorT g(selector_);

      const LoadOp& load = lhs.Cast<LoadOp>();
      base_ = load.base();
      opcode_ = kMips64Lw;
      if (load.index().has_value()) {
        int64_t index_constant;
        if (selector_->MatchIntegralWord64Constant(load.index().value(),
                                                   &index_constant)) {
          DCHECK_EQ(load.element_size_log2, 0);
          immediate_ = index_constant + 4;
          matches_ = g.CanBeImmediate(immediate_, kMips64Lw);
        }
      } else {
        immediate_ = load.offset + 4;
        matches_ = g.CanBeImmediate(immediate_, kMips64Lw);
      }
    }
  }
};

bool TryEmitExtendingLoad(InstructionSelectorT* selector, OpIndex node,
                          OpIndex output_node) {
  ExtendingLoadMatcher m(node, selector);
  Mips64OperandGeneratorT g(selector);
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

bool TryMatchImmediate(InstructionSelectorT* selector,
                       InstructionCode* opcode_return, OpIndex node,
                       size_t* input_count_return, InstructionOperand* inputs) {
  Mips64OperandGeneratorT g(selector);
  if (g.CanBeImmediate(node, *opcode_return)) {
    *opcode_return |= AddressingModeField::encode(kMode_MRI);
    inputs[0] = g.UseImmediate(node);
    *input_count_return = 1;
    return true;
  }
  return false;
}

static void VisitBinop(InstructionSelectorT* selector, OpIndex node,
                       InstructionCode opcode, bool has_reverse_opcode,
                       InstructionCode reverse_opcode,
                       FlagsContinuationT* cont) {
  Mips64OperandGeneratorT g(selector);
  InstructionOperand inputs[2];
  size_t input_count = 0;
  InstructionOperand outputs[1];
  size_t output_count = 0;

  const Operation& binop = selector->Get(node);
  OpIndex left_node = binop.input(0);
  OpIndex right_node = binop.input(1);

  if (TryMatchImmediate(selector, &opcode, right_node, &input_count,
                        &inputs[1])) {
    inputs[0] = g.UseRegister(left_node);
    input_count++;
  } else if (has_reverse_opcode &&
             TryMatchImmediate(selector, &reverse_opcode, left_node,
                               &input_count, &inputs[1])) {
    inputs[0] = g.UseRegister(right_node);
    opcode = reverse_opcode;
    input_count++;
  } else {
    inputs[input_count++] = g.UseRegister(left_node);
    inputs[input_count++] = g.UseOperand(right_node, opcode);
  }

  outputs[output_count++] = g.DefineAsRegister(node);

  DCHECK_NE(0u, input_count);
  DCHECK_EQ(1u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}

static void VisitBinop(InstructionSelectorT* selector, OpIndex node,
                       InstructionCode opcode, bool has_reverse_opcode,
                       InstructionCode reverse_opcode) {
  FlagsContinuationT cont;
  VisitBinop(selector, node, opcode, has_reverse_opcode, reverse_opcode, &cont);
}

static void VisitBinop(InstructionSelectorT* selector, OpIndex node,
                       InstructionCode opcode, FlagsContinuationT* cont) {
  VisitBinop(selector, node, opcode, false, kArchNop, cont);
}

static void VisitBinop(InstructionSelectorT* selector, OpIndex node,
                       InstructionCode opcode) {
  VisitBinop(selector, node, opcode, false, kArchNop);
}

void InstructionSelectorT::VisitStackSlot(OpIndex node) {
  const StackSlotOp& stack_slot = Cast<StackSlotOp>(node);
  int slot = frame_->AllocateSpillSlot(stack_slot.size, stack_slot.alignment,
                                       stack_slot.is_tagged);
  OperandGenerator g(this);

  Emit(kArchStackSlot, g.DefineAsRegister(node),
       sequence()->AddImmediate(Constant(slot)), 0, nullptr);
}

void InstructionSelectorT::VisitAbortCSADcheck(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  const AbortCSADcheckOp& op = Cast<AbortCSADcheckOp>(node);
  Emit(kArchAbortCSADcheck, g.NoOutput(), g.UseFixed(op.message(), a0));
}

void EmitLoad(InstructionSelectorT* selector, OpIndex node,
              InstructionCode opcode, OpIndex output = OpIndex{}) {
  Mips64OperandGeneratorT g(selector);
  const Operation& op = selector->Get(node);
  const LoadOp& load = op.Cast<LoadOp>();

  // The LoadStoreSimplificationReducer transforms all loads into
  // *(base + index).
  OpIndex base = load.base();
  OpIndex index = load.index().value();
  CHECK_EQ(load.offset, 0);
  DCHECK_EQ(load.element_size_log2, 0);

  InstructionOperand inputs[3];
  size_t input_count = 0;
  InstructionOperand output_op;

  // If output is valid, use that as the output register. This is used when we
  // merge a conversion into the load.
  output_op = g.DefineAsRegister(output.valid() ? output : node);

  const Operation& base_op = selector->Get(base);
  int64_t index_constant;
  const bool is_index_constant =
      selector->MatchSignedIntegralConstant(index, &index_constant);
  if (base_op.Is<Opmask::kExternalConstant>() && is_index_constant) {
    const ConstantOp& constant_base = base_op.Cast<ConstantOp>();
    if (selector->CanAddressRelativeToRootsRegister(
            constant_base.external_reference())) {
      ptrdiff_t const delta =
          index_constant +
          MacroAssemblerBase::RootRegisterOffsetForExternalReference(
              selector->isolate(), constant_base.external_reference());
      input_count = 1;
      // Check that the delta is a 32-bit integer due to the limitations of
      // immediate operands.
      if (is_int32(delta)) {
        inputs[0] = g.UseImmediate(static_cast<int32_t>(delta));
        opcode |= AddressingModeField::encode(kMode_Root);
        selector->Emit(opcode, 1, &output_op, input_count, inputs);
        return;
      }
    }
  }

  if (base_op.Is<LoadRootRegisterOp>() && is_index_constant) {
    input_count = 1;
    inputs[0] = g.UseImmediate64(index_constant);
    opcode |= AddressingModeField::encode(kMode_Root);
    selector->Emit(opcode, 1, &output_op, input_count, inputs);
    return;
  }

  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(output.valid() ? output : node),
                   g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kMips64Dadd | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(output.valid() ? output : node), addr_reg,
                   g.TempImmediate(0));
  }
}

#if V8_ENABLE_WEBASSEMBLY
void InstructionSelectorT::VisitStoreLane(OpIndex node) { UNIMPLEMENTED(); }

void InstructionSelectorT::VisitLoadLane(OpIndex node) { UNIMPLEMENTED(); }

void InstructionSelectorT::VisitLoadTransform(OpIndex node) { UNIMPLEMENTED(); }
#endif  // V8_ENABLE_WEBASSEMBLY

void InstructionSelectorT::VisitLoad(OpIndex node) {
  auto load = this->load_view(node);
  LoadRepresentation load_rep = load.loaded_rep();

  InstructionCode opcode = kArchNop;
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
      opcode = kMips64Lw;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord64:
      opcode = kMips64Ld;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kMips64MsaLd;
      break;
    case MachineRepresentation::kFloat16:
      UNIMPLEMENTED();
    case MachineRepresentation::kSimd256:            // Fall through.
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kProtectedPointer:   // Fall through.
    case MachineRepresentation::kSandboxedPointer:   // Fall through.
    case MachineRepresentation::kCompressed:         // Fall through.
    case MachineRepresentation::kMapWord:            // Fall through.
    case MachineRepresentation::kIndirectPointer:    // Fall through.
    case MachineRepresentation::kFloat16RawBits:     // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }

  EmitLoad(this, node, opcode);
}

void InstructionSelectorT::VisitProtectedLoad(OpIndex node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitStorePair(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitStore(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  TurboshaftAdapter::StoreView store_view = this->store_view(node);
  DCHECK_EQ(store_view.displacement(), 0);
  OpIndex base = store_view.base();
  OpIndex index = store_view.index().value();
  OpIndex value = store_view.value();

  WriteBarrierKind write_barrier_kind =
      store_view.stored_rep().write_barrier_kind();
  MachineRepresentation rep = store_view.stored_rep().representation();

  if (v8_flags.enable_unconditional_write_barriers && CanBeTaggedPointer(rep)) {
    write_barrier_kind = kFullWriteBarrier;
  }

  // TODO(mips): I guess this could be done in a better way.
  if (write_barrier_kind != kNoWriteBarrier &&
      !v8_flags.disable_write_barriers) {
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
    ArchOpcode opcode;
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
      case MachineRepresentation::kTagged:         // Fall through.
      case MachineRepresentation::kWord64:
        opcode = kMips64Sd;
        break;
      case MachineRepresentation::kSimd128:
        opcode = kMips64MsaSt;
        break;
      case MachineRepresentation::kFloat16:
        UNIMPLEMENTED();
      case MachineRepresentation::kSimd256:            // Fall through.
      case MachineRepresentation::kCompressedPointer:  // Fall through.
      case MachineRepresentation::kCompressed:         // Fall through.
      case MachineRepresentation::kProtectedPointer:   // Fall through.
      case MachineRepresentation::kSandboxedPointer:   // Fall through.
      case MachineRepresentation::kMapWord:            // Fall through.
      case MachineRepresentation::kIndirectPointer:    // Fall through.
      case MachineRepresentation::kFloat16RawBits:     // Fall through.
      case MachineRepresentation::kNone:
        UNREACHABLE();
    }

    if (this->is_load_root_register(base)) {
      // This will only work if {index} is a constant.
      Emit(opcode | AddressingModeField::encode(kMode_Root), g.NoOutput(),
           g.UseImmediate(index), g.UseRegisterOrImmediateZero(value));
      return;
    }

    if (g.CanBeImmediate(index, opcode)) {
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           g.UseRegister(base), g.UseImmediate(index),
           g.UseRegisterOrImmediateZero(value));
    } else {
      InstructionOperand addr_reg = g.TempRegister();
      Emit(kMips64Dadd | AddressingModeField::encode(kMode_None), addr_reg,
           g.UseRegister(index), g.UseRegister(base));
      // Emit desired store opcode, using temp addr_reg.
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           addr_reg, g.TempImmediate(0), g.UseRegisterOrImmediateZero(value));
    }
  }
}

void InstructionSelectorT::VisitProtectedStore(OpIndex node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitWord32And(turboshaft::OpIndex node) {
  // TODO(MIPS_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kMips64And32, true, kMips64And32);
}

void InstructionSelectorT::VisitWord64And(OpIndex node) {
  // TODO(MIPS_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kMips64And, true, kMips64And);
}

void InstructionSelectorT::VisitWord32Or(OpIndex node) {
  VisitBinop(this, node, kMips64Or32, true, kMips64Or32);
}

void InstructionSelectorT::VisitWord64Or(OpIndex node) {
  VisitBinop(this, node, kMips64Or, true, kMips64Or);
}

void InstructionSelectorT::VisitWord32Xor(OpIndex node) {
  // TODO(MIPS_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kMips64Xor32, true, kMips64Xor32);
}

void InstructionSelectorT::VisitWord64Xor(OpIndex node) {
  // TODO(MIPS_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kMips64Xor, true, kMips64Xor);
}

void InstructionSelectorT::VisitWord32Shl(OpIndex node) {
  // TODO(MIPS_dev): May could be optimized like in Turbofan.
  VisitRRO(this, kMips64Shl, node);
}

void InstructionSelectorT::VisitWord32Shr(OpIndex node) {
  // TODO(MIPS_dev): May could be optimized like in Turbofan.
  VisitRRO(this, kMips64Shr, node);
}

void InstructionSelectorT::VisitWord32Sar(turboshaft::OpIndex node) {
  // TODO(MIPS_dev): May could be optimized like in Turbofan.
  VisitRRO(this, kMips64Sar, node);
}

void InstructionSelectorT::VisitWord64Shl(OpIndex node) {
  const ShiftOp& shift_op = this->Get(node).template Cast<ShiftOp>();
  const Operation& lhs = this->Get(shift_op.left());
  const Operation& rhs = this->Get(shift_op.right());
  if ((lhs.Is<Opmask::kChangeInt32ToInt64>() ||
       lhs.Is<Opmask::kChangeUint32ToUint64>()) &&
      rhs.Is<Opmask::kWord32Constant>()) {
    int64_t shift_by = rhs.Cast<ConstantOp>().signed_integral();
    if (base::IsInRange(shift_by, 32, 63) && CanCover(node, shift_op.left())) {
      Mips64OperandGeneratorT g(this);
      // There's no need to sign/zero-extend to 64-bit if we shift out the
      // upper 32 bits anyway.
      Emit(kMips64Dshl, g.DefineAsRegister(node),
           g.UseRegister(lhs.Cast<ChangeOp>().input()),
           g.UseImmediate64(shift_by));
      return;
    }
  }
  VisitRRO(this, kMips64Dshl, node);
}

void InstructionSelectorT::VisitWord64Shr(OpIndex node) {
  // TODO(MIPS_dev): May could be optimized like in Turbofan.
  VisitRRO(this, kMips64Dshr, node);
}

void InstructionSelectorT::VisitWord64Sar(OpIndex node) {
  if (TryEmitExtendingLoad(this, node, node)) return;

  const ShiftOp& shiftop = Get(node).Cast<ShiftOp>();
  const Operation& lhs = Get(shiftop.left());

  int64_t constant_rhs;
  if (lhs.Is<Opmask::kChangeInt32ToInt64>() &&
      MatchIntegralWord64Constant(shiftop.right(), &constant_rhs) &&
      is_uint5(constant_rhs) && CanCover(node, shiftop.left())) {
    OpIndex input = lhs.Cast<ChangeOp>().input();
    if (!Get(input).Is<LoadOp>() || !CanCover(shiftop.left(), input)) {
      Mips64OperandGeneratorT g(this);
      int right = static_cast<int>(constant_rhs);
      Emit(kMips64Sar, g.DefineAsRegister(node), g.UseRegister(input),
           g.UseImmediate(right));
      return;
    }
  }

  VisitRRO(this, kMips64Dsar, node);
}

void InstructionSelectorT::VisitWord32Rol(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitWord64Rol(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitWord32Ror(OpIndex node) {
  VisitRRO(this, kMips64Ror, node);
}

void InstructionSelectorT::VisitWord32Clz(OpIndex node) {
  VisitRR(this, kMips64Clz, node);
}

void InstructionSelectorT::VisitWord32ReverseBits(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitWord64ReverseBits(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitWord64ReverseBytes(OpIndex node) {
  VisitRR(this, kMips64ByteSwap64, node);
}

void InstructionSelectorT::VisitWord32ReverseBytes(OpIndex node) {
  VisitRR(this, kMips64ByteSwap32, node);
}

void InstructionSelectorT::VisitSimd128ReverseBytes(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitWord32Ctz(OpIndex node) {
  VisitRR(this, kMips64Ctz, node);
}

void InstructionSelectorT::VisitWord64Ctz(OpIndex node) {
  VisitRR(this, kMips64Dctz, node);
}

void InstructionSelectorT::VisitWord32Popcnt(OpIndex node) {
  VisitRR(this, kMips64Popcnt, node);
}

void InstructionSelectorT::VisitWord64Popcnt(OpIndex node) {
  VisitRR(this, kMips64Dpopcnt, node);
}

void InstructionSelectorT::VisitWord64Ror(OpIndex node) {
  VisitRRO(this, kMips64Dror, node);
}

void InstructionSelectorT::VisitWord64Clz(OpIndex node) {
  VisitRR(this, kMips64Dclz, node);
}

void InstructionSelectorT::VisitInt32Add(OpIndex node) {
  // TODO(MIPS_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kMips64Add, true, kMips64Add);
}

void InstructionSelectorT::VisitInt64Add(OpIndex node) {
  // TODO(MIPS_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kMips64Dadd, true, kMips64Dadd);
}

void InstructionSelectorT::VisitInt32Sub(OpIndex node) {
  VisitBinop(this, node, kMips64Sub);
}

void InstructionSelectorT::VisitInt64Sub(OpIndex node) {
  VisitBinop(this, node, kMips64Dsub);
}

void InstructionSelectorT::VisitInt32Mul(OpIndex node) {
  // TODO(MIPS_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kMips64Mul, true, kMips64Mul);
}

void InstructionSelectorT::VisitInt32MulHigh(OpIndex node) {
  VisitRRR(this, kMips64MulHigh, node);
}

void InstructionSelectorT::VisitInt64MulHigh(OpIndex node) {
  VisitRRR(this, kMips64DMulHigh, node);
}

void InstructionSelectorT::VisitUint32MulHigh(OpIndex node) {
  VisitRRR(this, kMips64MulHighU, node);
}

void InstructionSelectorT::VisitUint64MulHigh(OpIndex node) {
  VisitRRR(this, kMips64DMulHighU, node);
}

void InstructionSelectorT::VisitInt64Mul(OpIndex node) {
  // TODO(MIPS_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kMips64Dmul, true, kMips64Dmul);
}

void InstructionSelectorT::VisitInt32Div(OpIndex node) {
  Mips64OperandGeneratorT g(this);

  auto [left, right] = Inputs<WordBinopOp>(node);
  Emit(kMips64Div, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}

void InstructionSelectorT::VisitUint32Div(OpIndex node) {
  Mips64OperandGeneratorT g(this);

  auto [left, right] = Inputs<WordBinopOp>(node);
  Emit(kMips64DivU, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}

void InstructionSelectorT::VisitInt32Mod(OpIndex node) {
  Mips64OperandGeneratorT g(this);

  auto [left, right] = Inputs<WordBinopOp>(node);
  Emit(kMips64Mod, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}

void InstructionSelectorT::VisitUint32Mod(OpIndex node) {
  VisitRRR(this, kMips64ModU, node);
}

void InstructionSelectorT::VisitInt64Div(OpIndex node) {
  Mips64OperandGeneratorT g(this);

  auto [left, right] = Inputs<WordBinopOp>(node);
  Emit(kMips64Ddiv, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}

void InstructionSelectorT::VisitUint64Div(OpIndex node) {
  Mips64OperandGeneratorT g(this);

  auto [left, right] = Inputs<WordBinopOp>(node);
  Emit(kMips64DdivU, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}

void InstructionSelectorT::VisitInt64Mod(OpIndex node) {
  VisitRRR(this, kMips64Dmod, node);
}

void InstructionSelectorT::VisitUint64Mod(OpIndex node) {
  VisitRRR(this, kMips64DmodU, node);
}

void InstructionSelectorT::VisitChangeFloat32ToFloat64(OpIndex node) {
  VisitRR(this, kMips64CvtDS, node);
}

void InstructionSelectorT::VisitRoundInt32ToFloat32(OpIndex node) {
  VisitRR(this, kMips64CvtSW, node);
}

void InstructionSelectorT::VisitRoundUint32ToFloat32(OpIndex node) {
  VisitRR(this, kMips64CvtSUw, node);
}

void InstructionSelectorT::VisitChangeInt32ToFloat64(OpIndex node) {
  VisitRR(this, kMips64CvtDW, node);
}

void InstructionSelectorT::VisitChangeInt64ToFloat64(OpIndex node) {
  VisitRR(this, kMips64CvtDL, node);
}

void InstructionSelectorT::VisitChangeUint32ToFloat64(OpIndex node) {
  VisitRR(this, kMips64CvtDUw, node);
}

void InstructionSelectorT::VisitTruncateFloat32ToInt32(OpIndex node) {
  Mips64OperandGeneratorT g(this);

  const ChangeOp& op = Cast<ChangeOp>(node);
  InstructionCode opcode = kMips64TruncWS;
  opcode |=
      MiscField::encode(op.Is<Opmask::kTruncateFloat32ToInt32OverflowToMin>());
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input()));
}

void InstructionSelectorT::VisitTruncateFloat32ToUint32(OpIndex node) {
  Mips64OperandGeneratorT g(this);

  const ChangeOp& op = Cast<ChangeOp>(node);
  InstructionCode opcode = kMips64TruncUwS;
  if (op.Is<Opmask::kTruncateFloat32ToUint32OverflowToMin>()) {
    opcode |= MiscField::encode(true);
  }

  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input()));
}

void InstructionSelectorT::VisitChangeFloat64ToInt32(OpIndex node) {
  VisitRR(this, kMips64TruncWD, node);
}

void InstructionSelectorT::VisitChangeFloat64ToInt64(OpIndex node) {
  VisitRR(this, kMips64TruncLD, node);
}

void InstructionSelectorT::VisitChangeFloat64ToUint32(OpIndex node) {
  VisitRR(this, kMips64TruncUwD, node);
}

void InstructionSelectorT::VisitChangeFloat64ToUint64(OpIndex node) {
  VisitRR(this, kMips64TruncUlD, node);
}

void InstructionSelectorT::VisitTruncateFloat64ToUint32(OpIndex node) {
  VisitRR(this, kMips64TruncUwD, node);
}

void InstructionSelectorT::VisitTruncateFloat64ToInt64(OpIndex node) {
  Mips64OperandGeneratorT g(this);

  InstructionCode opcode = kMips64TruncLD;
  const ChangeOp& op = Cast<ChangeOp>(node);
  if (op.Is<Opmask::kTruncateFloat64ToInt64OverflowToMin>()) {
    opcode |= MiscField::encode(true);
  }

  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input()));
}

void InstructionSelectorT::VisitTruncateFloat64ToFloat16RawBits(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitChangeFloat16RawBitsToFloat64(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitTryTruncateFloat32ToInt64(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  const TryChangeOp& op = Cast<TryChangeOp>(node);

  InstructionOperand inputs[] = {g.UseRegister(op.input())};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kMips64TruncLS, output_count, outputs, 1, inputs);
}

void InstructionSelectorT::VisitTryTruncateFloat64ToInt64(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  const TryChangeOp& op = Cast<TryChangeOp>(node);

  InstructionOperand inputs[] = {g.UseRegister(op.input())};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kMips64TruncLD, output_count, outputs, 1, inputs);
}

void InstructionSelectorT::VisitTryTruncateFloat32ToUint64(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  const TryChangeOp& op = Cast<TryChangeOp>(node);

  InstructionOperand inputs[] = {g.UseRegister(op.input())};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kMips64TruncUlS, output_count, outputs, 1, inputs);
}

void InstructionSelectorT::VisitTryTruncateFloat64ToUint64(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  const TryChangeOp& op = Cast<TryChangeOp>(node);

  InstructionOperand inputs[] = {g.UseRegister(op.input())};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kMips64TruncUlD, output_count, outputs, 1, inputs);
}

void InstructionSelectorT::VisitTryTruncateFloat64ToInt32(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  const TryChangeOp& op = Cast<TryChangeOp>(node);

  InstructionOperand inputs[] = {g.UseRegister(op.input())};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kMips64TruncWD, output_count, outputs, 1, inputs);
}

void InstructionSelectorT::VisitTryTruncateFloat64ToUint32(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  const TryChangeOp& op = Cast<TryChangeOp>(node);

  InstructionOperand inputs[] = {g.UseRegister(op.input())};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kMips64TruncUwD, output_count, outputs, 1, inputs);
}

void InstructionSelectorT::VisitBitcastWord32ToWord64(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitChangeInt32ToInt64(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  const ChangeOp& change_op = this->Get(node).template Cast<ChangeOp>();
  const Operation& input_op = this->Get(change_op.input());
  if (input_op.Is<LoadOp>() && CanCover(node, change_op.input())) {
    // Generate sign-extending load.
    LoadRepresentation load_rep =
        this->load_view(change_op.input()).loaded_rep();
    MachineRepresentation rep = load_rep.representation();
    InstructionCode opcode = kArchNop;
    switch (rep) {
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
    }
    EmitLoad(this, change_op.input(), opcode, node);
    return;
  } else if (input_op.Is<Opmask::kWord32ShiftRightArithmetic>() &&
             CanCover(node, change_op.input())) {
    // TODO(MIPS_dev): May also optimize 'TruncateInt64ToInt32' here.
    EmitIdentity(node);
  }
  Emit(kMips64Shl, g.DefineAsRegister(node), g.UseRegister(change_op.input()),
       g.TempImmediate(0));
}

bool InstructionSelectorT::ZeroExtendsWord32ToWord64NoPhis(OpIndex node) {
  DCHECK(!this->Get(node).Is<PhiOp>());
  const Operation& op = this->Get(node);
  switch (op.opcode) {
    // Comparisons only emit 0/1, so the upper 32 bits must be zero.
    case Opcode::kComparison:
      return op.Cast<ComparisonOp>().rep == RegisterRepresentation::Word32();
    case Opcode::kOverflowCheckedBinop:
      return op.Cast<OverflowCheckedBinopOp>().rep ==
             WordRepresentation::Word32();
    case Opcode::kLoad: {
      auto load = this->load_view(node);
      LoadRepresentation load_rep = load.loaded_rep();
      if (load_rep.IsUnsigned()) {
        switch (load_rep.representation()) {
          case MachineRepresentation::kBit:    // Fall through.
          case MachineRepresentation::kWord8:  // Fall through.
          case MachineRepresentation::kWord16:
            return true;
          default:
            return false;
        }
      }
      return false;
    }
    default:
      return false;
  }
}

void InstructionSelectorT::VisitChangeUint32ToUint64(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  const ChangeOp& change_op = this->Get(node).template Cast<ChangeOp>();
  OpIndex input = change_op.input();
  const Operation& input_op = this->Get(input);

  if (input_op.Is<LoadOp>() && CanCover(node, input)) {
    // Generate zero-extending load.
    LoadRepresentation load_rep = this->load_view(input).loaded_rep();
    if (load_rep.IsUnsigned() &&
        load_rep.representation() == MachineRepresentation::kWord32) {
      EmitLoad(this, input, kMips64Lwu, node);
      return;
    }
  }
  if (ZeroExtendsWord32ToWord64(input)) {
    EmitIdentity(node);
    return;
  }
  Emit(kMips64Dext, g.DefineAsRegister(node), g.UseRegister(input),
       g.TempImmediate(0), g.TempImmediate(32));
}

void InstructionSelectorT::VisitTruncateInt64ToInt32(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  OpIndex value = Cast<ChangeOp>(node).input();
  if (CanCover(node, value)) {
    if (Get(value).Is<Opmask::kWord64ShiftRightArithmetic>()) {
      const ShiftOp& shift = Cast<ShiftOp>(value);
      auto shift_value = shift.right();
      if (CanCover(value, shift.left()) &&
          TryEmitExtendingLoad(this, value, node)) {
        return;
      } else if (int64_t constant;
                 MatchSignedIntegralConstant(shift_value, &constant)) {
        if (constant >= 32 && constant <= 63) {
          // After smi untagging no need for truncate. Combine sequence.
          Emit(kMips64Dsar, g.DefineAsRegister(node),
               g.UseRegister(shift.left()),
               g.UseImmediate(static_cast<int32_t>(constant)));
          return;
        }
      }
    }
  }
  Emit(kMips64Shl, g.DefineAsRegister(node), g.UseRegister(value),
       g.TempImmediate(0));
}

void InstructionSelectorT::VisitTruncateFloat64ToFloat32(OpIndex node) {
  VisitRR(this, kMips64CvtSD, node);
}

void InstructionSelectorT::VisitTruncateFloat64ToWord32(OpIndex node) {
  VisitRR(this, kArchTruncateDoubleToI, node);
}

void InstructionSelectorT::VisitRoundFloat64ToInt32(OpIndex node) {
  VisitRR(this, kMips64TruncWD, node);
}

void InstructionSelectorT::VisitRoundInt64ToFloat32(OpIndex node) {
  VisitRR(this, kMips64CvtSL, node);
}

void InstructionSelectorT::VisitRoundInt64ToFloat64(OpIndex node) {
  VisitRR(this, kMips64CvtDL, node);
}

void InstructionSelectorT::VisitRoundUint64ToFloat32(OpIndex node) {
  VisitRR(this, kMips64CvtSUl, node);
}

void InstructionSelectorT::VisitRoundUint64ToFloat64(OpIndex node) {
  VisitRR(this, kMips64CvtDUl, node);
}

void InstructionSelectorT::VisitBitcastFloat32ToInt32(OpIndex node) {
  VisitRR(this, kMips64Float64ExtractLowWord32, node);
}

void InstructionSelectorT::VisitBitcastFloat64ToInt64(OpIndex node) {
  VisitRR(this, kMips64BitcastDL, node);
}

void InstructionSelectorT::VisitBitcastInt32ToFloat32(OpIndex node) {
  // when move lower 32 bits of general registers to 64-bit fpu registers on
  // mips64, the upper 32 bits of the fpu register is undefined. So we could
  // just move the whole 64 bits to fpu registers.
  VisitRR(this, kMips64BitcastLD, node);
}

void InstructionSelectorT::VisitBitcastInt64ToFloat64(OpIndex node) {
  VisitRR(this, kMips64BitcastLD, node);
}

void InstructionSelectorT::VisitFloat32Add(OpIndex node) {
  // Optimization with Madd.S(z, x, y) is intentionally removed.
  // See explanation for madd_s in assembler-mips64.cc.
  VisitRRR(this, kMips64AddS, node);
}

void InstructionSelectorT::VisitFloat64Add(OpIndex node) {
  // Optimization with Madd.D(z, x, y) is intentionally removed.
  // See explanation for madd_d in assembler-mips64.cc.
  VisitRRR(this, kMips64AddD, node);
}

void InstructionSelectorT::VisitFloat32Sub(OpIndex node) {
  // Optimization with Msub.S(z, x, y) is intentionally removed.
  // See explanation for madd_s in assembler-mips64.cc.
  VisitRRR(this, kMips64SubS, node);
}

void InstructionSelectorT::VisitFloat64Sub(OpIndex node) {
  // Optimization with Msub.D(z, x, y) is intentionally removed.
  // See explanation for madd_d in assembler-mips64.cc.
  VisitRRR(this, kMips64SubD, node);
}

void InstructionSelectorT::VisitFloat32Mul(OpIndex node) {
  VisitRRR(this, kMips64MulS, node);
}

void InstructionSelectorT::VisitFloat64Mul(OpIndex node) {
  VisitRRR(this, kMips64MulD, node);
}

void InstructionSelectorT::VisitFloat32Div(OpIndex node) {
  VisitRRR(this, kMips64DivS, node);
}

void InstructionSelectorT::VisitFloat64Div(OpIndex node) {
  VisitRRR(this, kMips64DivD, node);
}

void InstructionSelectorT::VisitFloat64Mod(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  const FloatBinopOp& op = Cast<FloatBinopOp>(node);

  Emit(kMips64ModD, g.DefineAsFixed(node, f0), g.UseFixed(op.left(), f12),
       g.UseFixed(op.right(), f14))
      ->MarkAsCall();
}

void InstructionSelectorT::VisitFloat32Max(OpIndex node) {
  VisitRRR(this, kMips64Float32Max, node);
}

void InstructionSelectorT::VisitFloat64Max(OpIndex node) {
  VisitRRR(this, kMips64Float64Max, node);
}

void InstructionSelectorT::VisitFloat32Min(OpIndex node) {
  VisitRRR(this, kMips64Float32Min, node);
}

void InstructionSelectorT::VisitFloat64Min(OpIndex node) {
  VisitRRR(this, kMips64Float64Min, node);
}

void InstructionSelectorT::VisitFloat32Abs(OpIndex node) {
  VisitRR(this, kMips64AbsS, node);
}

void InstructionSelectorT::VisitFloat64Abs(OpIndex node) {
  VisitRR(this, kMips64AbsD, node);
}

void InstructionSelectorT::VisitFloat32Sqrt(OpIndex node) {
  VisitRR(this, kMips64SqrtS, node);
}

void InstructionSelectorT::VisitFloat64Sqrt(OpIndex node) {
  VisitRR(this, kMips64SqrtD, node);
}

void InstructionSelectorT::VisitFloat32RoundDown(OpIndex node) {
  VisitRR(this, kMips64Float32RoundDown, node);
}

void InstructionSelectorT::VisitFloat64RoundDown(OpIndex node) {
  VisitRR(this, kMips64Float64RoundDown, node);
}

void InstructionSelectorT::VisitFloat32RoundUp(OpIndex node) {
  VisitRR(this, kMips64Float32RoundUp, node);
}

void InstructionSelectorT::VisitFloat64RoundUp(OpIndex node) {
  VisitRR(this, kMips64Float64RoundUp, node);
}

void InstructionSelectorT::VisitFloat32RoundTruncate(OpIndex node) {
  VisitRR(this, kMips64Float32RoundTruncate, node);
}

void InstructionSelectorT::VisitFloat64RoundTruncate(OpIndex node) {
  VisitRR(this, kMips64Float64RoundTruncate, node);
}

void InstructionSelectorT::VisitFloat64RoundTiesAway(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitFloat32RoundTiesEven(OpIndex node) {
  VisitRR(this, kMips64Float32RoundTiesEven, node);
}

void InstructionSelectorT::VisitFloat64RoundTiesEven(OpIndex node) {
  VisitRR(this, kMips64Float64RoundTiesEven, node);
}

void InstructionSelectorT::VisitFloat32Neg(OpIndex node) {
  VisitRR(this, kMips64NegS, node);
}

void InstructionSelectorT::VisitFloat64Neg(OpIndex node) {
  VisitRR(this, kMips64NegD, node);
}

void InstructionSelectorT::VisitFloat64Ieee754Binop(OpIndex node,
                                                    InstructionCode opcode) {
  Mips64OperandGeneratorT g(this);
  const FloatBinopOp& op = Cast<FloatBinopOp>(node);

  Emit(opcode, g.DefineAsFixed(node, f0), g.UseFixed(op.left(), f2),
       g.UseFixed(op.right(), f4))
      ->MarkAsCall();
}

void InstructionSelectorT::VisitFloat64Ieee754Unop(OpIndex node,
                                                   InstructionCode opcode) {
  Mips64OperandGeneratorT g(this);
  const FloatUnaryOp& op = Cast<FloatUnaryOp>(node);

  Emit(opcode, g.DefineAsFixed(node, f0), g.UseFixed(op.input(), f12))
      ->MarkAsCall();
}

void InstructionSelectorT::EmitMoveParamToFPR(OpIndex node, int index) {}

void InstructionSelectorT::EmitMoveFPRToParam(InstructionOperand* op,
                                              LinkageLocation location) {}

void InstructionSelectorT::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* call_descriptor,
    OpIndex node) {
  Mips64OperandGeneratorT g(this);

  // Prepare for C function call.
  if (call_descriptor->IsCFunctionCall()) {
    Emit(kArchPrepareCallCFunction | MiscField::encode(static_cast<int>(
                                         call_descriptor->ParameterCount())),
         0, nullptr, 0, nullptr);

    // Poke any stack arguments.
    int slot = kCArgSlotCount;
    for (PushParameter input : (*arguments)) {
      Emit(kMips64StoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
           g.TempImmediate(slot << kSystemPointerSizeLog2));
      ++slot;
    }
  } else {
    int push_count = static_cast<int>(call_descriptor->ParameterSlotCount());
    if (push_count > 0) {
      // Calculate needed space
      int stack_size = 0;
      for (PushParameter input : (*arguments)) {
        if (input.node.valid()) {
          stack_size += input.location.GetSizeInPointers();
        }
      }
      Emit(kMips64StackClaim, g.NoOutput(),
           g.TempImmediate(stack_size << kSystemPointerSizeLog2));
    }
    for (size_t n = 0; n < arguments->size(); ++n) {
      PushParameter input = (*arguments)[n];
      if (input.node.valid()) {
        Emit(kMips64StoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
             g.TempImmediate(static_cast<int>(n << kSystemPointerSizeLog2)));
      }
    }
  }
}

void InstructionSelectorT::EmitPrepareResults(
    ZoneVector<PushParameter>* results, const CallDescriptor* call_descriptor,
    OpIndex node) {
  Mips64OperandGeneratorT g(this);

  for (PushParameter output : *results) {
    if (!output.location.IsCallerFrameSlot()) continue;
    // Skip any alignment holes in nodes.
    if (output.node.valid()) {
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
      Emit(kMips64Peek, g.DefineAsRegister(output.node),
           g.UseImmediate(reverse_slot));
    }
  }
}

bool InstructionSelectorT::IsTailCallAddressImmediate() { return false; }

void InstructionSelectorT::VisitUnalignedLoad(OpIndex node) {
  auto load = this->load_view(node);
  LoadRepresentation load_rep = load.loaded_rep();

  InstructionCode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kMips64Ulwc1;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kMips64Uldc1;
      break;
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsUnsigned() ? kMips64Lbu : kMips64Lb;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsUnsigned() ? kMips64Ulhu : kMips64Ulh;
      break;
    case MachineRepresentation::kWord32:
      opcode = kMips64Ulw;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord64:
      opcode = kMips64Uld;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kMips64MsaLd;
      break;
    case MachineRepresentation::kFloat16:
      UNIMPLEMENTED();
    case MachineRepresentation::kSimd256:            // Fall through.
    case MachineRepresentation::kBit:                // Fall through.
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:         // Fall through.
    case MachineRepresentation::kProtectedPointer:   // Fall through.
    case MachineRepresentation::kSandboxedPointer:   // Fall through.
    case MachineRepresentation::kMapWord:            // Fall through.
    case MachineRepresentation::kIndirectPointer:    // Fall through.
    case MachineRepresentation::kFloat16RawBits:     // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }

  EmitLoad(this, node, opcode);
}

void InstructionSelectorT::VisitUnalignedStore(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  TurboshaftAdapter::StoreView store_view = this->store_view(node);
  DCHECK_EQ(store_view.displacement(), 0);
  OpIndex base = store_view.base();
  OpIndex index = store_view.index().value();
  OpIndex value = store_view.value();

  MachineRepresentation rep = store_view.stored_rep().representation();

  ArchOpcode opcode;
  switch (rep) {
    case MachineRepresentation::kFloat32:
      opcode = kMips64Uswc1;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kMips64Usdc1;
      break;
    case MachineRepresentation::kWord8:
      opcode = kMips64Sb;
      break;
    case MachineRepresentation::kWord16:
      opcode = kMips64Ush;
      break;
    case MachineRepresentation::kWord32:
      opcode = kMips64Usw;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord64:
      opcode = kMips64Usd;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kMips64MsaSt;
      break;
    case MachineRepresentation::kFloat16:
      UNIMPLEMENTED();
    case MachineRepresentation::kSimd256:            // Fall through.
    case MachineRepresentation::kBit:                // Fall through.
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:         // Fall through.
    case MachineRepresentation::kProtectedPointer:   // Fall through.
    case MachineRepresentation::kSandboxedPointer:   // Fall through.
    case MachineRepresentation::kMapWord:            // Fall through.
    case MachineRepresentation::kIndirectPointer:    // Fall through.
    case MachineRepresentation::kFloat16RawBits:     // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         g.UseRegister(base), g.UseImmediate(index),
         g.UseRegisterOrImmediateZero(value));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kMips64Dadd | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired store opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         addr_reg, g.TempImmediate(0), g.UseRegisterOrImmediateZero(value));
  }
}

namespace {

// Shared routine for multiple compare operations.
static Instruction* VisitCompare(InstructionSelectorT* selector,
                                 InstructionCode opcode,
                                 InstructionOperand left,
                                 InstructionOperand right,
                                 FlagsContinuationT* cont) {
  return selector->EmitWithContinuation(opcode, left, right, cont);
}

// Shared routine for multiple float32 compare operations.
void VisitFloat32Compare(InstructionSelectorT* selector, OpIndex node,
                         FlagsContinuationT* cont) {
  Mips64OperandGeneratorT g(selector);
  const ComparisonOp& op = selector->Get(node).template Cast<ComparisonOp>();
  OpIndex left = op.left();
  OpIndex right = op.right();
  InstructionOperand lhs, rhs;

  lhs = selector->MatchZero(left) ? g.UseImmediate(left) : g.UseRegister(left);
  rhs =
      selector->MatchZero(right) ? g.UseImmediate(right) : g.UseRegister(right);
  VisitCompare(selector, kMips64CmpS, lhs, rhs, cont);
}

// Shared routine for multiple float64 compare operations.
void VisitFloat64Compare(InstructionSelectorT* selector, OpIndex node,
                         FlagsContinuationT* cont) {
  Mips64OperandGeneratorT g(selector);
  const Operation& compare = selector->Get(node);
  DCHECK(compare.Is<ComparisonOp>());
  OpIndex lhs = compare.input(0);
  OpIndex rhs = compare.input(1);
  if (selector->MatchZero(rhs)) {
    VisitCompare(selector, kMips64CmpD, g.UseRegister(lhs), g.UseImmediate(rhs),
                 cont);
  } else if (selector->MatchZero(lhs)) {
    VisitCompare(selector, kMips64CmpD, g.UseImmediate(lhs), g.UseRegister(rhs),
                 cont);
  } else {
    VisitCompare(selector, kMips64CmpD, g.UseRegister(lhs), g.UseRegister(rhs),
                 cont);
  }
}

// Shared routine for multiple word compare operations.
Instruction* VisitWordCompare(InstructionSelectorT* selector, OpIndex node,
                              InstructionCode opcode, FlagsContinuationT* cont,
                              bool commutative) {
  Mips64OperandGeneratorT g(selector);
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 2);
  auto left = op.input(0);
  auto right = op.input(1);

  // Match immediates on left or right side of comparison.
  if (g.CanBeImmediate(right, opcode)) {
    if (opcode == kMips64Tst) {
      return VisitCompare(selector, opcode, g.UseRegister(left),
                          g.UseImmediate(right), cont);
    } else {
      switch (cont->condition()) {
        case kEqual:
        case kNotEqual:
          if (cont->IsSet()) {
            return VisitCompare(selector, opcode, g.UseRegister(left),
                                g.UseImmediate(right), cont);
          } else {
            return VisitCompare(selector, opcode, g.UseRegister(left),
                                g.UseRegister(right), cont);
          }
        case kSignedLessThan:
        case kSignedGreaterThanOrEqual:
        case kUnsignedLessThan:
        case kUnsignedGreaterThanOrEqual:
          return VisitCompare(selector, opcode, g.UseRegister(left),
                              g.UseImmediate(right), cont);
        default:
          return VisitCompare(selector, opcode, g.UseRegister(left),
                              g.UseRegister(right), cont);
      }
    }
  } else if (g.CanBeImmediate(left, opcode)) {
    if (!commutative) cont->Commute();
    if (opcode == kMips64Tst) {
      return VisitCompare(selector, opcode, g.UseRegister(right),
                          g.UseImmediate(left), cont);
    } else {
      switch (cont->condition()) {
        case kEqual:
        case kNotEqual:
          if (cont->IsSet()) {
            return VisitCompare(selector, opcode, g.UseRegister(right),
                                g.UseImmediate(left), cont);
          } else {
            return VisitCompare(selector, opcode, g.UseRegister(right),
                                g.UseRegister(left), cont);
          }
        case kSignedLessThan:
        case kSignedGreaterThanOrEqual:
        case kUnsignedLessThan:
        case kUnsignedGreaterThanOrEqual:
          return VisitCompare(selector, opcode, g.UseRegister(right),
                              g.UseImmediate(left), cont);
        default:
          return VisitCompare(selector, opcode, g.UseRegister(right),
                              g.UseRegister(left), cont);
      }
    }
  } else {
    return VisitCompare(selector, opcode, g.UseRegister(left),
                        g.UseRegister(right), cont);
  }
}

// Shared routine for multiple word compare operations.
void VisitFullWord32Compare(InstructionSelectorT* selector, OpIndex node,
                            InstructionCode opcode, FlagsContinuationT* cont) {
  Mips64OperandGeneratorT g(selector);
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 2);
  InstructionOperand leftOp = g.TempRegister();
  InstructionOperand rightOp = g.TempRegister();

  selector->Emit(kMips64Dshl, leftOp, g.UseRegister(op.input(0)),
                 g.TempImmediate(32));
  selector->Emit(kMips64Dshl, rightOp, g.UseRegister(op.input(1)),
                 g.TempImmediate(32));

  Instruction* instr = VisitCompare(selector, opcode, leftOp, rightOp, cont);
  selector->UpdateSourcePosition(instr, node);
}

void VisitWord32Compare(InstructionSelectorT* selector, OpIndex node,
                        FlagsContinuationT* cont) {
  VisitFullWord32Compare(selector, node, kMips64Cmp, cont);
}

void VisitWord64Compare(InstructionSelectorT* selector, OpIndex node,
                        FlagsContinuationT* cont) {
  VisitWordCompare(selector, node, kMips64Cmp, cont, false);
}

void EmitWordCompareZero(InstructionSelectorT* selector, OpIndex value,
                         FlagsContinuationT* cont) {
  Mips64OperandGeneratorT g(selector);
  selector->EmitWithContinuation(kMips64Cmp, g.UseRegister(value),
                                 g.TempImmediate(0), cont);
}

void VisitAtomicLoad(InstructionSelectorT* selector, OpIndex node,
                     AtomicWidth width) {
  using OpIndex = OpIndex;
  Mips64OperandGeneratorT g(selector);
  auto load = selector->load_view(node);
  OpIndex base = load.base();
  OpIndex index = load.index();

  // The memory order is ignored.
  LoadRepresentation load_rep = load.loaded_rep();
  InstructionCode code;
  switch (load_rep.representation()) {
    case MachineRepresentation::kWord8:
      DCHECK_IMPLIES(load_rep.IsSigned(), width == AtomicWidth::kWord32);
      code = load_rep.IsSigned() ? kAtomicLoadInt8 : kAtomicLoadUint8;
      break;
    case MachineRepresentation::kWord16:
      DCHECK_IMPLIES(load_rep.IsSigned(), width == AtomicWidth::kWord32);
      code = load_rep.IsSigned() ? kAtomicLoadInt16 : kAtomicLoadUint16;
      break;
    case MachineRepresentation::kWord32:
      code = kAtomicLoadWord32;
      break;
    case MachineRepresentation::kWord64:
      code = kMips64Word64AtomicLoadUint64;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:
      DCHECK_EQ(kTaggedSize, 8);
      code = kMips64Word64AtomicLoadUint64;
      break;
    default:
      UNREACHABLE();
  }

  if (g.CanBeImmediate(index, code)) {
    selector->Emit(code | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.DefineAsRegister(node), g.UseRegister(base),
                   g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kMips64Dadd | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(code | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

AtomicStoreParameters AtomicStoreParametersOf(InstructionSelectorT* selector,
                                              OpIndex node) {
  auto store = selector->store_view(node);
  return AtomicStoreParameters(store.stored_rep().representation(),
                               store.stored_rep().write_barrier_kind(),
                               store.memory_order().value(),
                               store.access_kind());
}

void VisitAtomicStore(InstructionSelectorT* selector, OpIndex node,
                      AtomicWidth width) {
  using OpIndex = OpIndex;
  Mips64OperandGeneratorT g(selector);
  auto store = selector->store_view(node);
  OpIndex base = store.base();
  OpIndex index = store.index().value();
  OpIndex value = store.value();
  DCHECK_EQ(store.displacement(), 0);

  // The memory order is ignored.
  AtomicStoreParameters store_params = AtomicStoreParametersOf(selector, node);
  WriteBarrierKind write_barrier_kind = store_params.write_barrier_kind();
  MachineRepresentation rep = store_params.representation();

  if (v8_flags.enable_unconditional_write_barriers &&
      CanBeTaggedOrCompressedPointer(rep)) {
    write_barrier_kind = kFullWriteBarrier;
  }

  InstructionCode code;

  if (write_barrier_kind != kNoWriteBarrier &&
      !v8_flags.disable_write_barriers) {
    DCHECK(CanBeTaggedPointer(rep));
    DCHECK_EQ(AtomicWidthSize(width), kTaggedSize);

    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    inputs[input_count++] = g.UseUniqueRegister(index);
    inputs[input_count++] = g.UseUniqueRegister(value);
    RecordWriteMode record_write_mode =
        WriteBarrierKindToRecordWriteMode(write_barrier_kind);
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    size_t const temp_count = arraysize(temps);
    code = kArchAtomicStoreWithWriteBarrier;
    code |= RecordWriteModeField::encode(record_write_mode);
    selector->Emit(code, 0, nullptr, input_count, inputs, temp_count, temps);
  } else {
    switch (rep) {
      case MachineRepresentation::kWord8:
        code = kAtomicStoreWord8;
        break;
      case MachineRepresentation::kWord16:
        code = kAtomicStoreWord16;
        break;
      case MachineRepresentation::kWord32:
        code = kAtomicStoreWord32;
        break;
      case MachineRepresentation::kWord64:
        DCHECK_EQ(width, AtomicWidth::kWord64);
        code = kMips64Word64AtomicStoreWord64;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:
        DCHECK_EQ(AtomicWidthSize(width), kTaggedSize);
        code = kMips64StoreCompressTagged;
        break;
      default:
        UNREACHABLE();
    }
    code |= AtomicWidthField::encode(width);

    if (g.CanBeImmediate(index, code)) {
      selector->Emit(code | AddressingModeField::encode(kMode_MRI) |
                         AtomicWidthField::encode(width),
                     g.NoOutput(), g.UseRegister(base), g.UseImmediate(index),
                     g.UseRegisterOrImmediateZero(value));
    } else {
      InstructionOperand addr_reg = g.TempRegister();
      selector->Emit(kMips64Dadd | AddressingModeField::encode(kMode_None),
                     addr_reg, g.UseRegister(index), g.UseRegister(base));
      // Emit desired store opcode, using temp addr_reg.
      selector->Emit(code | AddressingModeField::encode(kMode_MRI) |
                         AtomicWidthField::encode(width),
                     g.NoOutput(), addr_reg, g.TempImmediate(0),
                     g.UseRegisterOrImmediateZero(value));
    }
  }
}

void VisitAtomicExchange(InstructionSelectorT* selector, OpIndex node,
                         ArchOpcode opcode, AtomicWidth width) {
  using OpIndex = OpIndex;
  Mips64OperandGeneratorT g(selector);
  const AtomicRMWOp& atomic_op = selector->Cast<AtomicRMWOp>(node);
  OpIndex base = atomic_op.base();
  OpIndex index = atomic_op.index();
  OpIndex value = atomic_op.value();

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
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode) |
                         AtomicWidthField::encode(width);
  selector->Emit(code, 1, outputs, input_count, inputs, 3, temp);
}

void VisitAtomicCompareExchange(InstructionSelectorT* selector, OpIndex node,
                                ArchOpcode opcode, AtomicWidth width) {
  using OpIndex = OpIndex;
  Mips64OperandGeneratorT g(selector);
  const AtomicRMWOp& atomic_op = selector->Cast<AtomicRMWOp>(node);
  OpIndex base = atomic_op.base();
  OpIndex index = atomic_op.index();
  OpIndex old_value = atomic_op.expected().value();
  OpIndex new_value = atomic_op.value();

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
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode) |
                         AtomicWidthField::encode(width);
  selector->Emit(code, 1, outputs, input_count, inputs, 3, temp);
}

void VisitAtomicBinop(InstructionSelectorT* selector, OpIndex node,
                      ArchOpcode opcode, AtomicWidth width) {
  using OpIndex = OpIndex;
  Mips64OperandGeneratorT g(selector);
  const AtomicRMWOp& atomic_op = selector->Cast<AtomicRMWOp>(node);
  OpIndex base = atomic_op.base();
  OpIndex index = atomic_op.index();
  OpIndex value = atomic_op.value();

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

void InstructionSelectorT::VisitStackPointerGreaterThan(
    OpIndex node, FlagsContinuationT* cont) {
  StackCheckKind kind;
  OpIndex value;
  const auto& op = this->turboshaft_graph()
                       ->Get(node)
                       .template Cast<turboshaft::StackPointerGreaterThanOp>();
  kind = op.kind;
  value = op.stack_limit();
  InstructionCode opcode =
      kArchStackPointerGreaterThan |
      StackCheckField::encode(static_cast<StackCheckKind>(kind));

  Mips64OperandGeneratorT g(this);

  // No outputs.
  InstructionOperand* const outputs = nullptr;
  const int output_count = 0;

  // TempRegister(0) is used to store the comparison result.
  // Applying an offset to this stack check requires a temp register. Offsets
  // are only applied to the first stack check. If applying an offset, we must
  // ensure the input and temp registers do not alias, thus kUniqueRegister.
  InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
  const int temp_count = (kind == StackCheckKind::kJSFunctionEntry ? 2 : 1);
  const auto register_mode = (kind == StackCheckKind::kJSFunctionEntry)
                                 ? OperandGenerator::kUniqueRegister
                                 : OperandGenerator::kRegister;

  InstructionOperand inputs[] = {g.UseRegisterWithMode(value, register_mode)};
  static constexpr int input_count = arraysize(inputs);

  EmitWithContinuation(opcode, output_count, outputs, input_count, inputs,
                       temp_count, temps, cont);
}

// Shared routine for word comparisons against zero.
void InstructionSelectorT::VisitWordCompareZero(OpIndex user, OpIndex value,
                                                FlagsContinuation* cont) {
  {
    Mips64OperandGeneratorT g(this);
    // Try to combine with comparisons against 0 by simply inverting the branch.
    while (const ComparisonOp* equal =
               this->TryCast<Opmask::kWord32Equal>(value)) {
      if (!CanCover(user, value)) break;
      if (!MatchIntegralZero(equal->right())) break;

      user = value;
      value = equal->left();
      cont->Negate();
    }
    const Operation& value_op = Get(value);
    if (CanCover(user, value)) {
      if (const ComparisonOp* comparison = value_op.TryCast<ComparisonOp>()) {
        switch (comparison->rep.value()) {
          case RegisterRepresentation::Word32():
            cont->OverwriteAndNegateIfEqual(
                GetComparisonFlagCondition(*comparison));
            return VisitWord32Compare(this, value, cont);

          case RegisterRepresentation::Word64():
            cont->OverwriteAndNegateIfEqual(
                GetComparisonFlagCondition(*comparison));
            return VisitWord64Compare(this, value, cont);

          case RegisterRepresentation::Float32():
            switch (comparison->kind) {
              case ComparisonOp::Kind::kEqual:
                cont->OverwriteAndNegateIfEqual(kEqual);
                return VisitFloat32Compare(this, value, cont);
              case ComparisonOp::Kind::kSignedLessThan:
                cont->OverwriteAndNegateIfEqual(kFloatLessThan);
                return VisitFloat32Compare(this, value, cont);
              case ComparisonOp::Kind::kSignedLessThanOrEqual:
                cont->OverwriteAndNegateIfEqual(kFloatLessThanOrEqual);
                return VisitFloat32Compare(this, value, cont);
              default:
                UNREACHABLE();
            }

          case RegisterRepresentation::Float64():
            switch (comparison->kind) {
              case ComparisonOp::Kind::kEqual:
                cont->OverwriteAndNegateIfEqual(kEqual);
                return VisitFloat64Compare(this, value, cont);
              case ComparisonOp::Kind::kSignedLessThan:
                cont->OverwriteAndNegateIfEqual(kFloatLessThan);
                return VisitFloat64Compare(this, value, cont);
              case ComparisonOp::Kind::kSignedLessThanOrEqual:
                cont->OverwriteAndNegateIfEqual(kFloatLessThanOrEqual);
                return VisitFloat64Compare(this, value, cont);
              default:
                UNREACHABLE();
            }

          default:
            break;
        }
      } else if (const ProjectionOp* projection =
                     value_op.TryCast<ProjectionOp>()) {
        // Check if this is the overflow output projection of an
        // <Operation>WithOverflow node.
        if (projection->index == 1u) {
          // We cannot combine the <Operation>WithOverflow with this branch
          // unless the 0th projection (the use of the actual value of the
          // <Operation> is either nullptr, which means there's no use of the
          // actual value, or was already defined, which means it is scheduled
          // *AFTER* this branch).
          OpIndex node = projection->input();
          OptionalOpIndex result = FindProjection(node, 0);
          if (!result.valid() || IsDefined(result.value())) {
            if (const OverflowCheckedBinopOp* binop =
                    TryCast<OverflowCheckedBinopOp>(node)) {
              const bool is64 = binop->rep == WordRepresentation::Word64();
              switch (binop->kind) {
                case OverflowCheckedBinopOp::Kind::kSignedAdd:
                  cont->OverwriteAndNegateIfEqual(kOverflow);
                  return VisitBinop(this, node,
                                    is64 ? kMips64DaddOvf : kMips64Dadd, cont);
                case OverflowCheckedBinopOp::Kind::kSignedSub:
                  cont->OverwriteAndNegateIfEqual(kOverflow);
                  return VisitBinop(this, node,
                                    is64 ? kMips64DsubOvf : kMips64Dsub, cont);
                case OverflowCheckedBinopOp::Kind::kSignedMul:
                  cont->OverwriteAndNegateIfEqual(kOverflow);
                  return VisitBinop(
                      this, node, is64 ? kMips64DMulOvf : kMips64MulOvf, cont);
              }
            }
          }
        }
      } else if (value_op.Is<Opmask::kWord32BitwiseAnd>() ||
                 value_op.Is<Opmask::kWord64BitwiseAnd>()) {
        VisitWordCompare(this, value, kMips64Tst, cont, true);
        return;
      } else if (value_op.Is<StackPointerGreaterThanOp>()) {
        cont->OverwriteAndNegateIfEqual(kStackPointerGreaterThanCondition);
        return VisitStackPointerGreaterThan(value, cont);
      }
    }
    // Continuation could not be combined with a compare, emit compare against
    // 0.
    EmitWordCompareZero(this, value, cont);
  }
}

void InstructionSelectorT::VisitSwitch(OpIndex node, const SwitchInfo& sw) {
  Mips64OperandGeneratorT g(this);
  const SwitchOp& op = Cast<SwitchOp>(node);
  InstructionOperand value_operand = g.UseRegister(op.input());

  // Emit either ArchTableSwitch or ArchBinarySearchSwitch.
  if (enable_switch_jump_table_ ==
      InstructionSelector::kEnableSwitchJumpTable) {
    static const size_t kMaxTableSwitchValueRange = 2 << 16;
    size_t table_space_cost = 10 + 2 * sw.value_range();
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
        Emit(kMips64Sub, index_operand, value_operand,
             g.TempImmediate(sw.min_value()));
      }
      // Generate a table lookup.
      return EmitTableSwitch(sw, index_operand);
    }
  }

  // Generate a tree of conditional jumps.
  return EmitBinarySearchSwitch(sw, value_operand);
}

void InstructionSelectorT::VisitWord32Equal(OpIndex node) {
  const Operation& equal = Get(node);
  DCHECK(equal.Is<ComparisonOp>());
  OpIndex left = equal.input(0);
  OpIndex right = equal.input(1);
  OpIndex user = node;
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);

  if (MatchZero(right)) {
    return VisitWordCompareZero(user, left, &cont);
  }

  VisitWord32Compare(this, node, &cont);
}

void InstructionSelectorT::VisitInt32LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWord32Compare(this, node, &cont);
}

void InstructionSelectorT::VisitInt32LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWord32Compare(this, node, &cont);
}

void InstructionSelectorT::VisitUint32LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWord32Compare(this, node, &cont);
}

void InstructionSelectorT::VisitUint32LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWord32Compare(this, node, &cont);
}

void InstructionSelectorT::VisitInt32AddWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid() && IsUsed(ovf.value())) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop(this, node, kMips64Dadd, &cont);
  }

  FlagsContinuation cont;
  VisitBinop(this, node, kMips64Dadd, &cont);
}

void InstructionSelectorT::VisitInt32SubWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop(this, node, kMips64Dsub, &cont);
  }

  FlagsContinuation cont;
  VisitBinop(this, node, kMips64Dsub, &cont);
}

void InstructionSelectorT::VisitInt32MulWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop(this, node, kMips64MulOvf, &cont);
  }

  FlagsContinuation cont;
  VisitBinop(this, node, kMips64MulOvf, &cont);
}

void InstructionSelectorT::VisitInt64MulWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop(this, node, kMips64DMulOvf, &cont);
  }

  FlagsContinuation cont;
  VisitBinop(this, node, kMips64DMulOvf, &cont);
}

void InstructionSelectorT::VisitInt64AddWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop(this, node, kMips64DaddOvf, &cont);
  }

  FlagsContinuation cont;
  VisitBinop(this, node, kMips64DaddOvf, &cont);
}

void InstructionSelectorT::VisitInt64SubWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop(this, node, kMips64DsubOvf, &cont);
  }

  FlagsContinuation cont;
  VisitBinop(this, node, kMips64DsubOvf, &cont);
}

void InstructionSelectorT::VisitWord64Equal(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelectorT::VisitInt64LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelectorT::VisitInt64LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelectorT::VisitUint64LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelectorT::VisitUint64LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelectorT::VisitFloat32Equal(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelectorT::VisitFloat32LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelectorT::VisitFloat32LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelectorT::VisitFloat64Equal(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelectorT::VisitFloat64LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelectorT::VisitFloat64LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelectorT::VisitFloat64ExtractLowWord32(OpIndex node) {
  VisitRR(this, kMips64Float64ExtractLowWord32, node);
}

void InstructionSelectorT::VisitFloat64ExtractHighWord32(OpIndex node) {
  VisitRR(this, kMips64Float64ExtractHighWord32, node);
}

void InstructionSelectorT::VisitFloat64SilenceNaN(OpIndex node) {
  VisitRR(this, kMips64Float64SilenceNaN, node);
}

void InstructionSelectorT::VisitBitcastWord32PairToFloat64(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  const auto& bitcast = this->Cast<BitcastWord32PairToFloat64Op>(node);
  OpIndex hi = bitcast.high_word32();
  OpIndex lo = bitcast.low_word32();

  InstructionOperand temps[] = {g.TempRegister()};
  Emit(kMips64Float64FromWord32Pair, g.DefineAsRegister(node), g.Use(hi),
       g.Use(lo), arraysize(temps), temps);
}

void InstructionSelectorT::VisitMemoryBarrier(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  Emit(kMips64Sync, g.NoOutput());
}

void InstructionSelectorT::VisitWord32AtomicLoad(OpIndex node) {
  VisitAtomicLoad(this, node, AtomicWidth::kWord32);
}

void InstructionSelectorT::VisitWord32AtomicStore(OpIndex node) {
  VisitAtomicStore(this, node, AtomicWidth::kWord32);
}

void InstructionSelectorT::VisitWord64AtomicLoad(OpIndex node) {
  VisitAtomicLoad(this, node, AtomicWidth::kWord64);
}

void InstructionSelectorT::VisitWord64AtomicStore(OpIndex node) {
  VisitAtomicStore(this, node, AtomicWidth::kWord64);
}

void InstructionSelectorT::VisitWord32AtomicExchange(OpIndex node) {
  const AtomicRMWOp& atomic_op = this->Get(node).template Cast<AtomicRMWOp>();
  ArchOpcode opcode;
  if (atomic_op.memory_rep == MemoryRepresentation::Int8()) {
    opcode = kAtomicExchangeInt8;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint8()) {
    opcode = kAtomicExchangeUint8;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Int16()) {
    opcode = kAtomicExchangeInt16;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint16()) {
    opcode = kAtomicExchangeUint16;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Int32() ||
             atomic_op.memory_rep == MemoryRepresentation::Uint32()) {
    opcode = kAtomicExchangeWord32;
  } else {
    UNREACHABLE();
  }
  VisitAtomicExchange(this, node, opcode, AtomicWidth::kWord32);
}

void InstructionSelectorT::VisitWord64AtomicExchange(OpIndex node) {
  const AtomicRMWOp& atomic_op = this->Get(node).template Cast<AtomicRMWOp>();
  ArchOpcode opcode;
  if (atomic_op.memory_rep == MemoryRepresentation::Uint8()) {
    opcode = kAtomicExchangeUint8;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint16()) {
    opcode = kAtomicExchangeUint16;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint32()) {
    opcode = kAtomicExchangeWord32;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint64()) {
    opcode = kMips64Word64AtomicExchangeUint64;
  } else {
    UNREACHABLE();
  }
  VisitAtomicExchange(this, node, opcode, AtomicWidth::kWord64);
}

void InstructionSelectorT::VisitWord32AtomicCompareExchange(OpIndex node) {
  const AtomicRMWOp& atomic_op = this->Get(node).template Cast<AtomicRMWOp>();
  ArchOpcode opcode;
  if (atomic_op.memory_rep == MemoryRepresentation::Int8()) {
    opcode = kAtomicCompareExchangeInt8;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint8()) {
    opcode = kAtomicCompareExchangeUint8;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Int16()) {
    opcode = kAtomicCompareExchangeInt16;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint16()) {
    opcode = kAtomicCompareExchangeUint16;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Int32() ||
             atomic_op.memory_rep == MemoryRepresentation::Uint32()) {
    opcode = kAtomicCompareExchangeWord32;
  } else {
    UNREACHABLE();
  }
  VisitAtomicCompareExchange(this, node, opcode, AtomicWidth::kWord32);
}

void InstructionSelectorT::VisitWord64AtomicCompareExchange(OpIndex node) {
  const AtomicRMWOp& atomic_op = this->Get(node).template Cast<AtomicRMWOp>();
  ArchOpcode opcode;
  if (atomic_op.memory_rep == MemoryRepresentation::Uint8()) {
    opcode = kAtomicCompareExchangeUint8;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint16()) {
    opcode = kAtomicCompareExchangeUint16;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint32()) {
    opcode = kAtomicCompareExchangeWord32;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint64()) {
    opcode = kMips64Word64AtomicCompareExchangeUint64;
  } else {
    UNREACHABLE();
  }
  VisitAtomicCompareExchange(this, node, opcode, AtomicWidth::kWord64);
}

void InstructionSelectorT::VisitWord32AtomicBinaryOperation(
    OpIndex node, ArchOpcode int8_op, ArchOpcode uint8_op, ArchOpcode int16_op,
    ArchOpcode uint16_op, ArchOpcode word32_op) {
  const AtomicRMWOp& atomic_op = this->Get(node).template Cast<AtomicRMWOp>();
  ArchOpcode opcode;
  if (atomic_op.memory_rep == MemoryRepresentation::Int8()) {
    opcode = int8_op;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint8()) {
    opcode = uint8_op;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Int16()) {
    opcode = int16_op;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint16()) {
    opcode = uint16_op;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Int32() ||
             atomic_op.memory_rep == MemoryRepresentation::Uint32()) {
    opcode = word32_op;
  } else {
    UNREACHABLE();
  }
  VisitAtomicBinop(this, node, opcode, AtomicWidth::kWord32);
}

#define VISIT_ATOMIC_BINOP(op)                                           \
  void InstructionSelectorT::VisitWord32Atomic##op(OpIndex node) {       \
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

void InstructionSelectorT::VisitWord64AtomicBinaryOperation(
    OpIndex node, ArchOpcode uint8_op, ArchOpcode uint16_op,
    ArchOpcode uint32_op, ArchOpcode uint64_op) {
  const AtomicRMWOp& atomic_op = this->Get(node).template Cast<AtomicRMWOp>();
  ArchOpcode opcode;
  if (atomic_op.memory_rep == MemoryRepresentation::Uint8()) {
    opcode = uint8_op;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint16()) {
    opcode = uint16_op;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint32()) {
    opcode = uint32_op;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint64()) {
    opcode = uint64_op;
  } else {
    UNREACHABLE();
  }
  VisitAtomicBinop(this, node, opcode, AtomicWidth::kWord64);
}

#define VISIT_ATOMIC_BINOP(op)                                                 \
  void InstructionSelectorT::VisitWord64Atomic##op(OpIndex node) {             \
    VisitWord64AtomicBinaryOperation(node, kAtomic##op##Uint8,                 \
                                     kAtomic##op##Uint16, kAtomic##op##Word32, \
                                     kMips64Word64Atomic##op##Uint64);         \
  }
VISIT_ATOMIC_BINOP(Add)
VISIT_ATOMIC_BINOP(Sub)
VISIT_ATOMIC_BINOP(And)
VISIT_ATOMIC_BINOP(Or)
VISIT_ATOMIC_BINOP(Xor)
#undef VISIT_ATOMIC_BINOP

void InstructionSelectorT::VisitInt32AbsWithOverflow(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitInt64AbsWithOverflow(OpIndex node) {
  UNREACHABLE();
}

#if V8_ENABLE_WEBASSEMBLY

#define SIMD_TYPE_LIST(V) \
  V(F64x2)                \
  V(F32x4)                \
  V(I64x2)                \
  V(I32x4)                \
  V(I16x8)                \
  V(I8x16)

#define SIMD_UNOP_LIST(V)                                    \
  V(F64x2Abs, kMips64F64x2Abs)                               \
  V(F64x2Neg, kMips64F64x2Neg)                               \
  V(F64x2Sqrt, kMips64F64x2Sqrt)                             \
  V(F64x2Ceil, kMips64F64x2Ceil)                             \
  V(F64x2Floor, kMips64F64x2Floor)                           \
  V(F64x2Trunc, kMips64F64x2Trunc)                           \
  V(F64x2NearestInt, kMips64F64x2NearestInt)                 \
  V(I64x2Neg, kMips64I64x2Neg)                               \
  V(I64x2BitMask, kMips64I64x2BitMask)                       \
  V(F64x2ConvertLowI32x4S, kMips64F64x2ConvertLowI32x4S)     \
  V(F64x2ConvertLowI32x4U, kMips64F64x2ConvertLowI32x4U)     \
  V(F64x2PromoteLowF32x4, kMips64F64x2PromoteLowF32x4)       \
  V(F32x4SConvertI32x4, kMips64F32x4SConvertI32x4)           \
  V(F32x4UConvertI32x4, kMips64F32x4UConvertI32x4)           \
  V(F32x4Abs, kMips64F32x4Abs)                               \
  V(F32x4Neg, kMips64F32x4Neg)                               \
  V(F32x4Sqrt, kMips64F32x4Sqrt)                             \
  V(F32x4Ceil, kMips64F32x4Ceil)                             \
  V(F32x4Floor, kMips64F32x4Floor)                           \
  V(F32x4Trunc, kMips64F32x4Trunc)                           \
  V(F32x4NearestInt, kMips64F32x4NearestInt)                 \
  V(F32x4DemoteF64x2Zero, kMips64F32x4DemoteF64x2Zero)       \
  V(I64x2Abs, kMips64I64x2Abs)                               \
  V(I64x2SConvertI32x4Low, kMips64I64x2SConvertI32x4Low)     \
  V(I64x2SConvertI32x4High, kMips64I64x2SConvertI32x4High)   \
  V(I64x2UConvertI32x4Low, kMips64I64x2UConvertI32x4Low)     \
  V(I64x2UConvertI32x4High, kMips64I64x2UConvertI32x4High)   \
  V(I32x4SConvertF32x4, kMips64I32x4SConvertF32x4)           \
  V(I32x4UConvertF32x4, kMips64I32x4UConvertF32x4)           \
  V(I32x4Neg, kMips64I32x4Neg)                               \
  V(I32x4SConvertI16x8Low, kMips64I32x4SConvertI16x8Low)     \
  V(I32x4SConvertI16x8High, kMips64I32x4SConvertI16x8High)   \
  V(I32x4UConvertI16x8Low, kMips64I32x4UConvertI16x8Low)     \
  V(I32x4UConvertI16x8High, kMips64I32x4UConvertI16x8High)   \
  V(I32x4Abs, kMips64I32x4Abs)                               \
  V(I32x4BitMask, kMips64I32x4BitMask)                       \
  V(I32x4TruncSatF64x2SZero, kMips64I32x4TruncSatF64x2SZero) \
  V(I32x4TruncSatF64x2UZero, kMips64I32x4TruncSatF64x2UZero) \
  V(I16x8Neg, kMips64I16x8Neg)                               \
  V(I16x8SConvertI8x16Low, kMips64I16x8SConvertI8x16Low)     \
  V(I16x8SConvertI8x16High, kMips64I16x8SConvertI8x16High)   \
  V(I16x8UConvertI8x16Low, kMips64I16x8UConvertI8x16Low)     \
  V(I16x8UConvertI8x16High, kMips64I16x8UConvertI8x16High)   \
  V(I16x8Abs, kMips64I16x8Abs)                               \
  V(I16x8BitMask, kMips64I16x8BitMask)                       \
  V(I8x16Neg, kMips64I8x16Neg)                               \
  V(I8x16Abs, kMips64I8x16Abs)                               \
  V(I8x16Popcnt, kMips64I8x16Popcnt)                         \
  V(I8x16BitMask, kMips64I8x16BitMask)                       \
  V(S128Not, kMips64S128Not)                                 \
  V(I64x2AllTrue, kMips64I64x2AllTrue)                       \
  V(I32x4AllTrue, kMips64I32x4AllTrue)                       \
  V(I16x8AllTrue, kMips64I16x8AllTrue)                       \
  V(I8x16AllTrue, kMips64I8x16AllTrue)                       \
  V(V128AnyTrue, kMips64V128AnyTrue)

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

#define SIMD_BINOP_LIST(V)                               \
  V(F64x2Add, kMips64F64x2Add)                           \
  V(F64x2Sub, kMips64F64x2Sub)                           \
  V(F64x2Mul, kMips64F64x2Mul)                           \
  V(F64x2Div, kMips64F64x2Div)                           \
  V(F64x2Min, kMips64F64x2Min)                           \
  V(F64x2Max, kMips64F64x2Max)                           \
  V(F64x2Eq, kMips64F64x2Eq)                             \
  V(F64x2Ne, kMips64F64x2Ne)                             \
  V(F64x2Lt, kMips64F64x2Lt)                             \
  V(F64x2Le, kMips64F64x2Le)                             \
  V(I64x2Eq, kMips64I64x2Eq)                             \
  V(I64x2Ne, kMips64I64x2Ne)                             \
  V(I64x2Add, kMips64I64x2Add)                           \
  V(I64x2Sub, kMips64I64x2Sub)                           \
  V(I64x2Mul, kMips64I64x2Mul)                           \
  V(I64x2GtS, kMips64I64x2GtS)                           \
  V(I64x2GeS, kMips64I64x2GeS)                           \
  V(F32x4Add, kMips64F32x4Add)                           \
  V(F32x4Sub, kMips64F32x4Sub)                           \
  V(F32x4Mul, kMips64F32x4Mul)                           \
  V(F32x4Div, kMips64F32x4Div)                           \
  V(F32x4Max, kMips64F32x4Max)                           \
  V(F32x4Min, kMips64F32x4Min)                           \
  V(F32x4Eq, kMips64F32x4Eq)                             \
  V(F32x4Ne, kMips64F32x4Ne)                             \
  V(F32x4Lt, kMips64F32x4Lt)                             \
  V(F32x4Le, kMips64F32x4Le)                             \
  V(I32x4Add, kMips64I32x4Add)                           \
  V(I32x4Sub, kMips64I32x4Sub)                           \
  V(I32x4Mul, kMips64I32x4Mul)                           \
  V(I32x4MaxS, kMips64I32x4MaxS)                         \
  V(I32x4MinS, kMips64I32x4MinS)                         \
  V(I32x4MaxU, kMips64I32x4MaxU)                         \
  V(I32x4MinU, kMips64I32x4MinU)                         \
  V(I32x4Eq, kMips64I32x4Eq)                             \
  V(I32x4Ne, kMips64I32x4Ne)                             \
  V(I32x4GtS, kMips64I32x4GtS)                           \
  V(I32x4GeS, kMips64I32x4GeS)                           \
  V(I32x4GtU, kMips64I32x4GtU)                           \
  V(I32x4GeU, kMips64I32x4GeU)                           \
  V(I32x4DotI16x8S, kMips64I32x4DotI16x8S)               \
  V(I16x8Add, kMips64I16x8Add)                           \
  V(I16x8AddSatS, kMips64I16x8AddSatS)                   \
  V(I16x8AddSatU, kMips64I16x8AddSatU)                   \
  V(I16x8Sub, kMips64I16x8Sub)                           \
  V(I16x8SubSatS, kMips64I16x8SubSatS)                   \
  V(I16x8SubSatU, kMips64I16x8SubSatU)                   \
  V(I16x8Mul, kMips64I16x8Mul)                           \
  V(I16x8MaxS, kMips64I16x8MaxS)                         \
  V(I16x8MinS, kMips64I16x8MinS)                         \
  V(I16x8MaxU, kMips64I16x8MaxU)                         \
  V(I16x8MinU, kMips64I16x8MinU)                         \
  V(I16x8Eq, kMips64I16x8Eq)                             \
  V(I16x8Ne, kMips64I16x8Ne)                             \
  V(I16x8GtS, kMips64I16x8GtS)                           \
  V(I16x8GeS, kMips64I16x8GeS)                           \
  V(I16x8GtU, kMips64I16x8GtU)                           \
  V(I16x8GeU, kMips64I16x8GeU)                           \
  V(I16x8RoundingAverageU, kMips64I16x8RoundingAverageU) \
  V(I16x8SConvertI32x4, kMips64I16x8SConvertI32x4)       \
  V(I16x8UConvertI32x4, kMips64I16x8UConvertI32x4)       \
  V(I16x8Q15MulRSatS, kMips64I16x8Q15MulRSatS)           \
  V(I8x16Add, kMips64I8x16Add)                           \
  V(I8x16AddSatS, kMips64I8x16AddSatS)                   \
  V(I8x16AddSatU, kMips64I8x16AddSatU)                   \
  V(I8x16Sub, kMips64I8x16Sub)                           \
  V(I8x16SubSatS, kMips64I8x16SubSatS)                   \
  V(I8x16SubSatU, kMips64I8x16SubSatU)                   \
  V(I8x16MaxS, kMips64I8x16MaxS)                         \
  V(I8x16MinS, kMips64I8x16MinS)                         \
  V(I8x16MaxU, kMips64I8x16MaxU)                         \
  V(I8x16MinU, kMips64I8x16MinU)                         \
  V(I8x16Eq, kMips64I8x16Eq)                             \
  V(I8x16Ne, kMips64I8x16Ne)                             \
  V(I8x16GtS, kMips64I8x16GtS)                           \
  V(I8x16GeS, kMips64I8x16GeS)                           \
  V(I8x16GtU, kMips64I8x16GtU)                           \
  V(I8x16GeU, kMips64I8x16GeU)                           \
  V(I8x16RoundingAverageU, kMips64I8x16RoundingAverageU) \
  V(I8x16SConvertI16x8, kMips64I8x16SConvertI16x8)       \
  V(I8x16UConvertI16x8, kMips64I8x16UConvertI16x8)       \
  V(S128And, kMips64S128And)                             \
  V(S128Or, kMips64S128Or)                               \
  V(S128Xor, kMips64S128Xor)                             \
  V(S128AndNot, kMips64S128AndNot)

void InstructionSelectorT::VisitS128Const(OpIndex node) { UNIMPLEMENTED(); }

void InstructionSelectorT::VisitS128Zero(OpIndex node) {
  Mips64OperandGeneratorT g(this);
  Emit(kMips64S128Zero, g.DefineAsRegister(node));
}
#define SIMD_VISIT_SPLAT(Type)                                  \
  void InstructionSelectorT::Visit##Type##Splat(OpIndex node) { \
    VisitRR(this, kMips64##Type##Splat, node);                  \
  }
SIMD_TYPE_LIST(SIMD_VISIT_SPLAT)
#undef SIMD_VISIT_SPLAT

#define SIMD_VISIT_EXTRACT_LANE(Type, Sign)                                 \
  void InstructionSelectorT::Visit##Type##ExtractLane##Sign(OpIndex node) { \
    VisitRRI(this, kMips64##Type##ExtractLane##Sign, node);                 \
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

#define SIMD_VISIT_REPLACE_LANE(Type)                                 \
  void InstructionSelectorT::Visit##Type##ReplaceLane(OpIndex node) { \
    VisitRRIR(this, kMips64##Type##ReplaceLane, node);                \
  }
SIMD_TYPE_LIST(SIMD_VISIT_REPLACE_LANE)
#undef SIMD_VISIT_REPLACE_LANE

#define SIMD_VISIT_UNOP(Name, instruction)               \
  void InstructionSelectorT::Visit##Name(OpIndex node) { \
    VisitRR(this, instruction, node);                    \
  }
SIMD_UNOP_LIST(SIMD_VISIT_UNOP)
#undef SIMD_VISIT_UNOP

#define SIMD_VISIT_SHIFT_OP(Name)                        \
  void InstructionSelectorT::Visit##Name(OpIndex node) { \
    VisitSimdShift(this, kMips64##Name, node);           \
  }
SIMD_SHIFT_OP_LIST(SIMD_VISIT_SHIFT_OP)
#undef SIMD_VISIT_SHIFT_OP

#define SIMD_VISIT_BINOP(Name, instruction)              \
  void InstructionSelectorT::Visit##Name(OpIndex node) { \
    VisitRRR(this, instruction, node);                   \
  }
SIMD_BINOP_LIST(SIMD_VISIT_BINOP)
#undef SIMD_VISIT_BINOP

#define SIMD_RELAXED_OP_LIST(V)  \
  V(F64x2RelaxedMin)             \
  V(F64x2RelaxedMax)             \
  V(F32x4RelaxedMin)             \
  V(F32x4RelaxedMax)             \
  V(I32x4RelaxedTruncF32x4S)     \
  V(I32x4RelaxedTruncF32x4U)     \
  V(I32x4RelaxedTruncF64x2SZero) \
  V(I32x4RelaxedTruncF64x2UZero) \
  V(I16x8RelaxedQ15MulRS)        \
  V(I8x16RelaxedLaneSelect)      \
  V(I16x8RelaxedLaneSelect)      \
  V(I32x4RelaxedLaneSelect)      \
  V(I64x2RelaxedLaneSelect)

#define SIMD_VISIT_RELAXED_OP(Name) \
  void InstructionSelectorT::Visit##Name(OpIndex node) { UNREACHABLE(); }
SIMD_RELAXED_OP_LIST(SIMD_VISIT_RELAXED_OP)
#undef SIMD_VISIT_SHIFT_OP

void InstructionSelectorT::VisitS128Select(OpIndex node) {
  VisitRRRR(this, kMips64S128Select, node);
}

#define SIMD_UNIMP_OP_LIST(V) \
  V(F64x2Qfma)                \
  V(F64x2Qfms)                \
  V(F32x4Qfma)                \
  V(F32x4Qfms)                \
  V(I16x8DotI8x16I7x16S)      \
  V(I32x4DotI8x16I7x16AddS)

#define SIMD_VISIT_UNIMP_OP(Name) \
  void InstructionSelectorT::Visit##Name(OpIndex node) { UNIMPLEMENTED(); }
SIMD_UNIMP_OP_LIST(SIMD_VISIT_UNIMP_OP)

#undef SIMD_VISIT_UNIMP_OP
#undef SIMD_UNIMP_OP_LIST

#define UNIMPLEMENTED_SIMD_FP16_OP_LIST(V) \
  V(F16x8Splat)                            \
  V(F16x8ExtractLane)                      \
  V(F16x8ReplaceLane)                      \
  V(F16x8Abs)                              \
  V(F16x8Neg)                              \
  V(F16x8Sqrt)                             \
  V(F16x8Floor)                            \
  V(F16x8Ceil)                             \
  V(F16x8Trunc)                            \
  V(F16x8NearestInt)                       \
  V(F16x8Add)                              \
  V(F16x8Sub)                              \
  V(F16x8Mul)                              \
  V(F16x8Div)                              \
  V(F16x8Min)                              \
  V(F16x8Max)                              \
  V(F16x8Pmin)                             \
  V(F16x8Pmax)                             \
  V(F16x8Eq)                               \
  V(F16x8Ne)                               \
  V(F16x8Lt)                               \
  V(F16x8Le)                               \
  V(F16x8SConvertI16x8)                    \
  V(F16x8UConvertI16x8)                    \
  V(I16x8SConvertF16x8)                    \
  V(I16x8UConvertF16x8)                    \
  V(F32x4PromoteLowF16x8)                  \
  V(F16x8DemoteF32x4Zero)                  \
  V(F16x8DemoteF64x2Zero)                  \
  V(F16x8Qfma)                             \
  V(F16x8Qfms)

#define SIMD_VISIT_UNIMPL_FP16_OP(Name) \
  void InstructionSelectorT::Visit##Name(OpIndex node) { UNIMPLEMENTED(); }

UNIMPLEMENTED_SIMD_FP16_OP_LIST(SIMD_VISIT_UNIMPL_FP16_OP)
#undef SIMD_VISIT_UNIMPL_FP16_OP
#undef UNIMPLEMENTED_SIMD_FP16_OP_LIST

void InstructionSelectorT::VisitI8x16Shuffle(OpIndex node) { UNIMPLEMENTED(); }

void InstructionSelectorT::VisitI8x16Swizzle(OpIndex node) { UNIMPLEMENTED(); }

void InstructionSelectorT::VisitSetStackPointer(OpIndex node) {
  OperandGenerator g(this);
  const SetStackPointerOp& op = Cast<SetStackPointerOp>(node);
  auto input = g.UseRegister(op.value());
  Emit(kArchSetStackPointer, 0, nullptr, 1, &input);
}

void InstructionSelectorT::VisitF32x4Pmin(OpIndex node) {
  VisitUniqueRRR(this, kMips64F32x4Pmin, node);
}

void InstructionSelectorT::VisitF32x4Pmax(OpIndex node) {
  VisitUniqueRRR(this, kMips64F32x4Pmax, node);
}

void InstructionSelectorT::VisitF64x2Pmin(OpIndex node) {
  VisitUniqueRRR(this, kMips64F64x2Pmin, node);
}

void InstructionSelectorT::VisitF64x2Pmax(OpIndex node) {
  VisitUniqueRRR(this, kMips64F64x2Pmax, node);
}

#define VISIT_EXT_MUL(OPCODE1, OPCODE2, TYPE)                     \
  void InstructionSelectorT::Visit##OPCODE1##ExtMulLow##OPCODE2(  \
      OpIndex node) {                                             \
    UNIMPLEMENTED();                                              \
  }                                                               \
  void InstructionSelectorT::Visit##OPCODE1##ExtMulHigh##OPCODE2( \
      OpIndex node) {                                             \
    UNIMPLEMENTED();                                              \
  }

VISIT_EXT_MUL(I64x2, I32x4S, MSAS32)
VISIT_EXT_MUL(I64x2, I32x4U, MSAU32)
VISIT_EXT_MUL(I32x4, I16x8S, MSAS16)
VISIT_EXT_MUL(I32x4, I16x8U, MSAU16)
VISIT_EXT_MUL(I16x8, I8x16S, MSAS8)
VISIT_EXT_MUL(I16x8, I8x16U, MSAU8)
#undef VISIT_EXT_MUL

#define VISIT_EXTADD_PAIRWISE(OPCODE, TYPE) \
  void InstructionSelectorT::Visit##OPCODE(OpIndex node) { UNIMPLEMENTED(); }
VISIT_EXTADD_PAIRWISE(I16x8ExtAddPairwiseI8x16S, MSAS8)
VISIT_EXTADD_PAIRWISE(I16x8ExtAddPairwiseI8x16U, MSAU8)
VISIT_EXTADD_PAIRWISE(I32x4ExtAddPairwiseI16x8S, MSAS16)
VISIT_EXTADD_PAIRWISE(I32x4ExtAddPairwiseI16x8U, MSAU16)
#undef VISIT_EXTADD_PAIRWISE

#endif  // V8_ENABLE_WEBASSEMBLY

void InstructionSelectorT::VisitSignExtendWord8ToInt32(OpIndex node) {
  VisitRR(this, kMips64Seb, node);
}

void InstructionSelectorT::VisitSignExtendWord16ToInt32(OpIndex node) {
  VisitRR(this, kMips64Seh, node);
}

void InstructionSelectorT::VisitSignExtendWord8ToInt64(OpIndex node) {
  VisitRR(this, kMips64Seb, node);
}

void InstructionSelectorT::VisitSignExtendWord16ToInt64(OpIndex node) {
  VisitRR(this, kMips64Seh, node);
}

void InstructionSelectorT::VisitSignExtendWord32ToInt64(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::AddOutputToSelectContinuation(OperandGenerator* g,
                                                         int first_input_index,
                                                         OpIndex node) {
  UNREACHABLE();
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

// static
MachineOperatorBuilder::AlignmentRequirements
InstructionSelector::AlignmentRequirements() {
  if (kArchVariant == kMips64r6) {
    return MachineOperatorBuilder::AlignmentRequirements::
        FullUnalignedAccessSupport();
  } else {
    DCHECK_EQ(kMips64r2, kArchVariant);
    return MachineOperatorBuilder::AlignmentRequirements::
        NoUnalignedAccessSupport();
  }
}

#undef SIMD_BINOP_LIST
#undef SIMD_SHIFT_OP_LIST
#undef SIMD_RELAXED_OP_LIST
#undef SIMD_UNOP_LIST
#undef SIMD_TYPE_LIST
#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
