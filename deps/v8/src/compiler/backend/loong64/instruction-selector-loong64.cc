// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/machine-type.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"

namespace v8 {
namespace internal {
namespace compiler {

using namespace turboshaft;  // NOLINT(build/namespaces)

#define TRACE(...) PrintF(__VA_ARGS__)

// Adds loong64-specific methods for generating InstructionOperands.
class Loong64OperandGeneratorT final : public OperandGeneratorT {
 public:
  explicit Loong64OperandGeneratorT(InstructionSelectorT* selector)
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
           constant->float64().get_bits() == 0))
        return UseImmediate(node);
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
    if (constant->kind == ConstantOp::Kind::kCompressedHeapObject) {
      if (!COMPRESS_POINTERS_BOOL) return false;
      // For builtin code we need static roots
      if (selector()->isolate()->bootstrapper() && !V8_STATIC_ROOTS_BOOL) {
        return false;
      }
      const RootsTable& roots_table = selector()->isolate()->roots_table();
      RootIndex root_index;
      Handle<HeapObject> value = constant->handle();
      if (roots_table.IsRootHandle(value, &root_index)) {
        if (!RootsTable::IsReadOnly(root_index)) return false;
        return CanBeImmediate(MacroAssemblerBase::ReadOnlyRootPtr(
                                  root_index, selector()->isolate()),
                              mode);
      }
      return false;
    }

    int64_t value;
    return selector()->MatchSignedIntegralConstant(node, &value) &&
           CanBeImmediate(value, mode);
  }

  bool CanBeImmediate(int64_t value, InstructionCode opcode) {
    switch (ArchOpcodeField::decode(opcode)) {
      case kLoong64Cmp32:
      case kLoong64Cmp64:
        return true;
      case kLoong64Sll_w:
      case kLoong64Srl_w:
      case kLoong64Sra_w:
        return is_uint5(value);
      case kLoong64Sll_d:
      case kLoong64Srl_d:
      case kLoong64Sra_d:
        return is_uint6(value);
      case kLoong64And:
      case kLoong64And32:
      case kLoong64Or:
      case kLoong64Or32:
      case kLoong64Xor:
      case kLoong64Xor32:
      case kLoong64Tst:
        return is_uint12(value);
      case kLoong64Ld_w:
      case kLoong64St_w:
      case kLoong64Ld_d:
      case kLoong64St_d:
      case kAtomicLoadWord32:
      case kAtomicStoreWord32:
      case kLoong64Word64AtomicLoadUint64:
      case kLoong64Word64AtomicStoreWord64:
      case kLoong64StoreCompressTagged:
        return (is_int12(value) || (is_int16(value) && ((value & 0b11) == 0)));
      default:
        return is_int12(value);
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
  Loong64OperandGeneratorT g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(selector->input_at(node, 0)));
}

static void VisitRRI(InstructionSelectorT* selector, ArchOpcode opcode,
                     OpIndex node) {
  UNIMPLEMENTED();
}

static void VisitSimdShift(InstructionSelectorT* selector, ArchOpcode opcode,
                           OpIndex node) {
  Loong64OperandGeneratorT g(selector);
  OpIndex rhs = selector->input_at(node, 1);
  if (selector->Get(rhs).TryCast<ConstantOp>()) {
    selector->Emit(opcode, g.DefineAsRegister(node),
                   g.UseRegister(selector->input_at(node, 0)),
                   g.UseImmediate(selector->input_at(node, 1)));
  } else {
    selector->Emit(opcode, g.DefineAsRegister(node),
                   g.UseRegister(selector->input_at(node, 0)),
                   g.UseRegister(selector->input_at(node, 1)));
  }
}

static void VisitRRIR(InstructionSelectorT* selector, ArchOpcode opcode,
                      OpIndex node) {
  UNIMPLEMENTED();
}

void VisitRRR(InstructionSelectorT* selector, ArchOpcode opcode, OpIndex node) {
  Loong64OperandGeneratorT g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(selector->input_at(node, 0)),
                 g.UseRegister(selector->input_at(node, 1)));
}

static void VisitUniqueRRR(InstructionSelectorT* selector, ArchOpcode opcode,
                           OpIndex node) {
  Loong64OperandGeneratorT g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseUniqueRegister(selector->input_at(node, 0)),
                 g.UseUniqueRegister(selector->input_at(node, 1)));
}

void VisitRRRR(InstructionSelectorT* selector, ArchOpcode opcode,
               OpIndex node) {
  UNIMPLEMENTED();
}

static void VisitRRO(InstructionSelectorT* selector, ArchOpcode opcode,
                     OpIndex node) {
  Loong64OperandGeneratorT g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(selector->input_at(node, 0)),
                 g.UseOperand(selector->input_at(node, 1), opcode));
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
      Loong64OperandGeneratorT g(selector_);

      const LoadOp& load = lhs.Cast<LoadOp>();
      base_ = load.base();
      opcode_ = kLoong64Ld_w;
      if (load.index().has_value()) {
        int64_t index_constant;
        if (selector_->MatchIntegralWord64Constant(load.index().value(),
                                                   &index_constant)) {
          DCHECK_EQ(load.element_size_log2, 0);
          immediate_ = index_constant + 4;
          matches_ = g.CanBeImmediate(immediate_, kLoong64Ld_w);
        }
      } else {
        immediate_ = load.offset + 4;
        matches_ = g.CanBeImmediate(immediate_, kLoong64Ld_w);
      }
    }
  }
};

bool TryEmitExtendingLoad(InstructionSelectorT* selector, OpIndex node,
                          OpIndex output_node) {
  ExtendingLoadMatcher m(node, selector);
  Loong64OperandGeneratorT g(selector);
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
  Loong64OperandGeneratorT g(selector);
  if (g.CanBeImmediate(node, *opcode_return)) {
    *opcode_return |= AddressingModeField::encode(kMode_MRI);
    inputs[0] = g.UseImmediate(node);
    *input_count_return = 1;
    return true;
  }
  return false;
}

static void VisitBinop(InstructionSelectorT* selector, turboshaft::OpIndex node,
                       InstructionCode opcode, bool has_reverse_opcode,
                       InstructionCode reverse_opcode,
                       FlagsContinuationT* cont) {
  Loong64OperandGeneratorT g(selector);
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

static void VisitBinop(InstructionSelectorT* selector, turboshaft::OpIndex node,
                       InstructionCode opcode, bool has_reverse_opcode,
                       InstructionCode reverse_opcode) {
  FlagsContinuationT cont;
  VisitBinop(selector, node, opcode, has_reverse_opcode, reverse_opcode, &cont);
}

static void VisitBinop(InstructionSelectorT* selector, turboshaft::OpIndex node,
                       InstructionCode opcode, FlagsContinuationT* cont) {
  VisitBinop(selector, node, opcode, false, kArchNop, cont);
}

static void VisitBinop(InstructionSelectorT* selector, turboshaft::OpIndex node,
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
  Loong64OperandGeneratorT g(this);
  Emit(kArchAbortCSADcheck, g.NoOutput(),
       g.UseFixed(this->input_at(node, 0), a0));
}

void EmitLoad(InstructionSelectorT* selector, turboshaft::OpIndex node,
              InstructionCode opcode, turboshaft::OpIndex output = OpIndex{}) {
  Loong64OperandGeneratorT g(selector);
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
  int64_t index_value;
  if (base_op.Is<Opmask::kExternalConstant>() &&
      selector->MatchSignedIntegralConstant(index, &index_value)) {
    const ConstantOp& constant_base = base_op.Cast<ConstantOp>();
    if (selector->CanAddressRelativeToRootsRegister(
            constant_base.external_reference())) {
      ptrdiff_t const delta =
          index_value +
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

  if (base_op.Is<LoadRootRegisterOp>()) {
    int64_t index_value;
    CHECK(selector->MatchSignedIntegralConstant(index, &index_value));
    input_count = 1;
    inputs[0] = g.UseImmediate64(index_value);
    opcode |= AddressingModeField::encode(kMode_Root);
    selector->Emit(opcode, 1, &output_op, input_count, inputs);
    return;
  }

  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(output.valid() ? output : node),
                   g.UseRegister(base), g.UseImmediate(index));
  } else {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRR),
                   g.DefineAsRegister(output.valid() ? output : node),
                   g.UseRegister(base), g.UseRegister(index));
  }
}

void InstructionSelectorT::VisitStoreLane(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitLoadLane(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitLoadTransform(OpIndex node) { UNIMPLEMENTED(); }

namespace {

ArchOpcode GetLoadOpcode(turboshaft::MemoryRepresentation loaded_rep,
                         turboshaft::RegisterRepresentation result_rep) {
  // NOTE: The meaning of `loaded_rep` = `MemoryRepresentation::AnyTagged()` is
  // we are loading a compressed tagged field, while `result_rep` =
  // `RegisterRepresentation::Tagged()` refers to an uncompressed tagged value.
  switch (loaded_rep) {
    case MemoryRepresentation::Int8():
      DCHECK_EQ(result_rep, RegisterRepresentation::Word32());
      return kLoong64Ld_b;
    case MemoryRepresentation::Uint8():
      DCHECK_EQ(result_rep, RegisterRepresentation::Word32());
      return kLoong64Ld_bu;
    case MemoryRepresentation::Int16():
      DCHECK_EQ(result_rep, RegisterRepresentation::Word32());
      return kLoong64Ld_h;
    case MemoryRepresentation::Uint16():
      DCHECK_EQ(result_rep, RegisterRepresentation::Word32());
      return kLoong64Ld_hu;
    case MemoryRepresentation::Int32():
    case MemoryRepresentation::Uint32():
      DCHECK_EQ(result_rep, RegisterRepresentation::Word32());
      return kLoong64Ld_w;
    case MemoryRepresentation::Int64():
    case MemoryRepresentation::Uint64():
      DCHECK_EQ(result_rep, RegisterRepresentation::Word64());
      return kLoong64Ld_d;
    case MemoryRepresentation::Float16():
      UNIMPLEMENTED();
    case MemoryRepresentation::Float32():
      DCHECK_EQ(result_rep, RegisterRepresentation::Float32());
      return kLoong64Fld_s;
    case MemoryRepresentation::Float64():
      DCHECK_EQ(result_rep, RegisterRepresentation::Float64());
      return kLoong64Fld_d;
#ifdef V8_COMPRESS_POINTERS
    case MemoryRepresentation::AnyTagged():
    case MemoryRepresentation::TaggedPointer():
      if (result_rep == RegisterRepresentation::Compressed()) {
        return kLoong64Ld_wu;
      }
      DCHECK_EQ(result_rep, RegisterRepresentation::Tagged());
      return kLoong64LoadDecompressTagged;
    case MemoryRepresentation::TaggedSigned():
      if (result_rep == RegisterRepresentation::Compressed()) {
        return kLoong64Ld_wu;
      }
      DCHECK_EQ(result_rep, RegisterRepresentation::Tagged());
      return kLoong64LoadDecompressTaggedSigned;
#else
    case MemoryRepresentation::AnyTagged():
    case MemoryRepresentation::TaggedPointer():
    case MemoryRepresentation::TaggedSigned():
      DCHECK_EQ(result_rep, RegisterRepresentation::Tagged());
      return kLoong64Ld_d;
#endif
    case MemoryRepresentation::AnyUncompressedTagged():
    case MemoryRepresentation::UncompressedTaggedPointer():
    case MemoryRepresentation::UncompressedTaggedSigned():
      DCHECK_EQ(result_rep, RegisterRepresentation::Tagged());
      return kLoong64Ld_d;
    case MemoryRepresentation::ProtectedPointer():
      CHECK(V8_ENABLE_SANDBOX_BOOL);
      return kLoong64LoadDecompressProtected;
    case MemoryRepresentation::IndirectPointer():
      UNREACHABLE();
    case MemoryRepresentation::SandboxedPointer():
      return kLoong64LoadDecodeSandboxedPointer;
    case MemoryRepresentation::Simd128():  // Fall through.
    case MemoryRepresentation::Simd256():
      UNREACHABLE();
  }
}

ArchOpcode GetStoreOpcode(turboshaft::MemoryRepresentation stored_rep) {
  switch (stored_rep) {
    case MemoryRepresentation::Int8():
    case MemoryRepresentation::Uint8():
      return kLoong64St_b;
    case MemoryRepresentation::Int16():
    case MemoryRepresentation::Uint16():
      return kLoong64St_h;
    case MemoryRepresentation::Int32():
    case MemoryRepresentation::Uint32():
      return kLoong64St_w;
    case MemoryRepresentation::Int64():
    case MemoryRepresentation::Uint64():
      return kLoong64St_d;
    case MemoryRepresentation::Float16():
      UNIMPLEMENTED();
    case MemoryRepresentation::Float32():
      return kLoong64Fst_s;
    case MemoryRepresentation::Float64():
      return kLoong64Fst_d;
    case MemoryRepresentation::AnyTagged():
    case MemoryRepresentation::TaggedPointer():
    case MemoryRepresentation::TaggedSigned():
      return kLoong64StoreCompressTagged;
    case MemoryRepresentation::AnyUncompressedTagged():
    case MemoryRepresentation::UncompressedTaggedPointer():
    case MemoryRepresentation::UncompressedTaggedSigned():
      return kLoong64St_d;
    case MemoryRepresentation::ProtectedPointer():
      // We never store directly to protected pointers from generated code.
      UNREACHABLE();
    case MemoryRepresentation::IndirectPointer():
      return kLoong64StoreIndirectPointer;
    case MemoryRepresentation::SandboxedPointer():
      return kLoong64StoreEncodeSandboxedPointer;
    case MemoryRepresentation::Simd128():
    case MemoryRepresentation::Simd256():
      UNREACHABLE();
  }
}

}  // namespace

void InstructionSelectorT::VisitLoad(OpIndex node) {
  auto load = this->load_view(node);
  InstructionCode opcode = kArchNop;

  opcode = GetLoadOpcode(load.ts_loaded_rep(), load.ts_result_rep());

  bool traps_on_null;
  if (load.is_protected(&traps_on_null)) {
    if (traps_on_null) {
      opcode |= AccessModeField::encode(kMemoryAccessProtectedNullDereference);
    } else {
      opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
    }
  }

  EmitLoad(this, node, opcode);
}

void InstructionSelectorT::VisitProtectedLoad(OpIndex node) { VisitLoad(node); }

void InstructionSelectorT::VisitStorePair(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitStore(OpIndex node) {
  Loong64OperandGeneratorT g(this);
  TurboshaftAdapter::StoreView store_view = this->store_view(node);
  DCHECK_EQ(store_view.displacement(), 0);
  OpIndex base = store_view.base();
  OpIndex index = this->value(store_view.index());
  OpIndex value = store_view.value();

  WriteBarrierKind write_barrier_kind =
      store_view.stored_rep().write_barrier_kind();
  const MachineRepresentation rep = store_view.stored_rep().representation();

  if (v8_flags.enable_unconditional_write_barriers &&
      CanBeTaggedOrCompressedPointer(rep)) {
    write_barrier_kind = kFullWriteBarrier;
  }

  // TODO(loong64): I guess this could be done in a better way.
  if (write_barrier_kind != kNoWriteBarrier &&
      !v8_flags.disable_write_barriers) {
    DCHECK(CanBeTaggedOrCompressedOrIndirectPointer(rep));
    AddressingMode addressing_mode;
    InstructionOperand inputs[4];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    // OutOfLineRecordWrite uses the index in an arithmetic instruction, so we
    // must check kArithmeticImm as well as kLoadStoreImm64.
    if (g.CanBeImmediate(index, kLoong64Add_d)) {
      inputs[input_count++] = g.UseImmediate(index);
      addressing_mode = kMode_MRI;
    } else {
      inputs[input_count++] = g.UseUniqueRegister(index);
      addressing_mode = kMode_MRR;
    }
    inputs[input_count++] = g.UseUniqueRegister(value);
    RecordWriteMode record_write_mode =
        WriteBarrierKindToRecordWriteMode(write_barrier_kind);
    InstructionCode code;
    if (rep == MachineRepresentation::kIndirectPointer) {
      DCHECK_EQ(write_barrier_kind, kIndirectPointerWriteBarrier);
      // In this case we need to add the IndirectPointerTag as additional input.
      code = kArchStoreIndirectWithWriteBarrier;
      IndirectPointerTag tag = store_view.indirect_pointer_tag();
      inputs[input_count++] = g.UseImmediate64(static_cast<int64_t>(tag));
    } else {
      code = kArchStoreWithWriteBarrier;
    }
    code |= AddressingModeField::encode(addressing_mode);
    code |= RecordWriteModeField::encode(record_write_mode);
    if (store_view.is_store_trap_on_null()) {
      code |= AccessModeField::encode(kMemoryAccessProtectedNullDereference);
    }
    Emit(code, 0, nullptr, input_count, inputs);
    return;
  }

  InstructionCode code;
  code = GetStoreOpcode(store_view.ts_stored_rep());

  std::optional<ExternalReference> external_base;
  {
    ExternalReference value;
    if (this->MatchExternalConstant(base, &value)) {
      external_base = value;
    }
  }

  std::optional<int64_t> constant_index;
  if (store_view.index().valid()) {
    OpIndex index = this->value(store_view.index());
    constant_index = g.GetOptionalIntegerConstant(index);
  }
  if (external_base.has_value() && constant_index.has_value() &&
      CanAddressRelativeToRootsRegister(*external_base)) {
    ptrdiff_t const delta =
        *constant_index +
        MacroAssemblerBase::RootRegisterOffsetForExternalReference(
            isolate(), *external_base);
    // Check that the delta is a 32-bit integer due to the limitations of
    // immediate operands.
    if (is_int32(delta)) {
      Emit(code | AddressingModeField::encode(kMode_Root), g.NoOutput(),
           g.UseImmediate(static_cast<int32_t>(delta)),
           g.UseRegisterOrImmediateZero(value));
      return;
    }
  }

  if (this->is_load_root_register(base)) {
    // This will only work if {index} is a constant.
    Emit(code | AddressingModeField::encode(kMode_Root), g.NoOutput(),
         g.UseImmediate(index), g.UseRegisterOrImmediateZero(value));
    return;
  }

  if (store_view.is_store_trap_on_null()) {
    code |= AccessModeField::encode(kMemoryAccessProtectedNullDereference);
  } else if (store_view.access_kind() ==
             MemoryAccessKind::kProtectedByTrapHandler) {
    code |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
  }

  if (g.CanBeImmediate(index, code)) {
    Emit(code | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         g.UseRegister(base), g.UseImmediate(index),
         g.UseRegisterOrImmediateZero(value));
  } else {
    Emit(code | AddressingModeField::encode(kMode_MRR), g.NoOutput(),
         g.UseRegister(base), g.UseRegister(index),
         g.UseRegisterOrImmediateZero(value));
  }
}

void InstructionSelectorT::VisitProtectedStore(OpIndex node) {
  VisitStore(node);
}

void InstructionSelectorT::VisitWord32And(turboshaft::OpIndex node) {
  // TODO(LOONG_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kLoong64And32, true, kLoong64And32);
}

void InstructionSelectorT::VisitWord64And(OpIndex node) {
  // TODO(LOONG_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kLoong64And, true, kLoong64And);
}

void InstructionSelectorT::VisitWord32Or(OpIndex node) {
  VisitBinop(this, node, kLoong64Or32, true, kLoong64Or32);
}

void InstructionSelectorT::VisitWord64Or(OpIndex node) {
  VisitBinop(this, node, kLoong64Or, true, kLoong64Or);
}

void InstructionSelectorT::VisitWord32Xor(OpIndex node) {
  // TODO(LOONG_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kLoong64Xor32, true, kLoong64Xor32);
}

void InstructionSelectorT::VisitWord64Xor(OpIndex node) {
  // TODO(LOONG_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kLoong64Xor, true, kLoong64Xor);
}

void InstructionSelectorT::VisitWord32Shl(OpIndex node) {
  // TODO(LOONG_dev): May could be optimized like in Turbofan.
  VisitRRO(this, kLoong64Sll_w, node);
}

void InstructionSelectorT::VisitWord32Shr(OpIndex node) {
  VisitRRO(this, kLoong64Srl_w, node);
}

void InstructionSelectorT::VisitWord32Sar(turboshaft::OpIndex node) {
  // TODO(LOONG_dev): May could be optimized like in Turbofan.
  VisitRRO(this, kLoong64Sra_w, node);
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
      Loong64OperandGeneratorT g(this);
      // There's no need to sign/zero-extend to 64-bit if we shift out the
      // upper 32 bits anyway.
      Emit(kLoong64Sll_d, g.DefineAsRegister(node),
           g.UseRegister(lhs.Cast<ChangeOp>().input()),
           g.UseImmediate(shift_by));
      return;
    }
  }
  VisitRRO(this, kLoong64Sll_d, node);
}

void InstructionSelectorT::VisitWord64Shr(OpIndex node) {
  // TODO(LOONG_dev): May could be optimized like in Turbofan.
  VisitRRO(this, kLoong64Srl_d, node);
}

void InstructionSelectorT::VisitWord64Sar(OpIndex node) {
  if (TryEmitExtendingLoad(this, node, node)) return;

  // Select Sbfx(x, imm, 32-imm) for Word64Sar(ChangeInt32ToInt64(x), imm)
  // where possible
  const ShiftOp& shiftop = Get(node).Cast<ShiftOp>();
  const Operation& lhs = Get(shiftop.left());

  int64_t constant_rhs;
  if (lhs.Is<Opmask::kChangeInt32ToInt64>() &&
      MatchIntegralWord64Constant(shiftop.right(), &constant_rhs) &&
      is_uint5(constant_rhs) && CanCover(node, shiftop.left())) {
    OpIndex input = lhs.Cast<ChangeOp>().input();
    if (!Get(input).Is<LoadOp>() || !CanCover(shiftop.left(), input)) {
      Loong64OperandGeneratorT g(this);
      int right = static_cast<int>(constant_rhs);
      Emit(kLoong64Sra_w, g.DefineAsRegister(node), g.UseRegister(input),
           g.UseImmediate(right));
      return;
    }
  }

  VisitRRO(this, kLoong64Sra_d, node);
}

void InstructionSelectorT::VisitWord32Rol(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitWord64Rol(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitWord32Ror(OpIndex node) {
  VisitRRO(this, kLoong64Rotr_w, node);
}

void InstructionSelectorT::VisitWord64Ror(OpIndex node) {
  VisitRRO(this, kLoong64Rotr_d, node);
}

void InstructionSelectorT::VisitWord32ReverseBits(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitWord64ReverseBits(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitWord32ReverseBytes(OpIndex node) {
  VisitRR(this, kLoong64ByteSwap32, node);
}

void InstructionSelectorT::VisitWord64ReverseBytes(OpIndex node) {
  VisitRR(this, kLoong64ByteSwap64, node);
}

void InstructionSelectorT::VisitSimd128ReverseBytes(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitWord32Clz(OpIndex node) {
  VisitRR(this, kLoong64Clz_w, node);
}

void InstructionSelectorT::VisitWord64Clz(OpIndex node) {
  VisitRR(this, kLoong64Clz_d, node);
}

void InstructionSelectorT::VisitWord32Ctz(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitWord64Ctz(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitWord32Popcnt(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitWord64Popcnt(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitInt32Add(OpIndex node) {
  // TODO(LOONG_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kLoong64Add_w, true, kLoong64Add_w);
}

void InstructionSelectorT::VisitInt64Add(OpIndex node) {
  // TODO(LOONG_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kLoong64Add_d, true, kLoong64Add_d);
}

void InstructionSelectorT::VisitInt32Sub(OpIndex node) {
  VisitBinop(this, node, kLoong64Sub_w);
}

void InstructionSelectorT::VisitInt64Sub(OpIndex node) {
  VisitBinop(this, node, kLoong64Sub_d);
}

void InstructionSelectorT::VisitInt32Mul(OpIndex node) {
  // TODO(LOONG_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kLoong64Mul_w, true, kLoong64Mul_w);
}

void InstructionSelectorT::VisitInt32MulHigh(OpIndex node) {
  VisitRRR(this, kLoong64Mulh_w, node);
}

void InstructionSelectorT::VisitInt64MulHigh(OpIndex node) {
  VisitRRR(this, kLoong64Mulh_d, node);
}

void InstructionSelectorT::VisitUint32MulHigh(OpIndex node) {
  VisitRRR(this, kLoong64Mulh_wu, node);
}

void InstructionSelectorT::VisitUint64MulHigh(OpIndex node) {
  VisitRRR(this, kLoong64Mulh_du, node);
}

void InstructionSelectorT::VisitInt64Mul(OpIndex node) {
  // TODO(LOONG_dev): May could be optimized like in Turbofan.
  VisitBinop(this, node, kLoong64Mul_d, true, kLoong64Mul_d);
}

void InstructionSelectorT::VisitInt32Div(OpIndex node) {
  Loong64OperandGeneratorT g(this);

  auto [left, right] = Inputs<WordBinopOp>(node);
  Emit(kLoong64Div_w, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}

void InstructionSelectorT::VisitUint32Div(OpIndex node) {
  Loong64OperandGeneratorT g(this);

  auto [left, right] = Inputs<WordBinopOp>(node);
  Emit(kLoong64Div_wu, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}

void InstructionSelectorT::VisitInt32Mod(OpIndex node) {
  Loong64OperandGeneratorT g(this);

  auto [left, right] = Inputs<WordBinopOp>(node);
  Emit(kLoong64Mod_w, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}

void InstructionSelectorT::VisitUint32Mod(OpIndex node) {
  VisitRRR(this, kLoong64Mod_wu, node);
}

void InstructionSelectorT::VisitInt64Div(OpIndex node) {
  Loong64OperandGeneratorT g(this);

  auto [left, right] = Inputs<WordBinopOp>(node);
  Emit(kLoong64Div_d, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}

void InstructionSelectorT::VisitUint64Div(OpIndex node) {
  Loong64OperandGeneratorT g(this);

  auto [left, right] = Inputs<WordBinopOp>(node);
  Emit(kLoong64Div_du, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}

void InstructionSelectorT::VisitInt64Mod(OpIndex node) {
  VisitRRR(this, kLoong64Mod_d, node);
}

void InstructionSelectorT::VisitUint64Mod(OpIndex node) {
  VisitRRR(this, kLoong64Mod_du, node);
}

void InstructionSelectorT::VisitChangeFloat32ToFloat64(OpIndex node) {
  VisitRR(this, kLoong64Float32ToFloat64, node);
}

void InstructionSelectorT::VisitRoundInt32ToFloat32(OpIndex node) {
  VisitRR(this, kLoong64Int32ToFloat32, node);
}

void InstructionSelectorT::VisitRoundUint32ToFloat32(OpIndex node) {
  VisitRR(this, kLoong64Uint32ToFloat32, node);
}

void InstructionSelectorT::VisitChangeInt32ToFloat64(OpIndex node) {
  VisitRR(this, kLoong64Int32ToFloat64, node);
}

void InstructionSelectorT::VisitChangeInt64ToFloat64(OpIndex node) {
  VisitRR(this, kLoong64Int64ToFloat64, node);
}

void InstructionSelectorT::VisitChangeUint32ToFloat64(OpIndex node) {
  VisitRR(this, kLoong64Uint32ToFloat64, node);
}

void InstructionSelectorT::VisitTruncateFloat32ToInt32(OpIndex node) {
  Loong64OperandGeneratorT g(this);

  const Operation& op = this->Get(node);
  InstructionCode opcode = kLoong64Float32ToInt32;
  opcode |=
      MiscField::encode(op.Is<Opmask::kTruncateFloat32ToInt32OverflowToMin>());
  Emit(opcode, g.DefineAsRegister(node),
       g.UseRegister(this->input_at(node, 0)));
}

void InstructionSelectorT::VisitTruncateFloat32ToUint32(OpIndex node) {
  Loong64OperandGeneratorT g(this);

  const Operation& op = this->Get(node);
  InstructionCode opcode = kLoong64Float32ToUint32;
  if (op.Is<Opmask::kTruncateFloat32ToUint32OverflowToMin>()) {
    opcode |= MiscField::encode(true);
  }

  Emit(opcode, g.DefineAsRegister(node),
       g.UseRegister(this->input_at(node, 0)));
}

void InstructionSelectorT::VisitChangeFloat64ToInt32(OpIndex node) {
  VisitRR(this, kLoong64Float64ToInt32, node);
}

void InstructionSelectorT::VisitChangeFloat64ToInt64(OpIndex node) {
  VisitRR(this, kLoong64Float64ToInt64, node);
}

void InstructionSelectorT::VisitChangeFloat64ToUint32(OpIndex node) {
  VisitRR(this, kLoong64Float64ToUint32, node);
}

void InstructionSelectorT::VisitChangeFloat64ToUint64(OpIndex node) {
  VisitRR(this, kLoong64Float64ToUint64, node);
}

void InstructionSelectorT::VisitTruncateFloat64ToUint32(OpIndex node) {
  VisitRR(this, kLoong64Float64ToUint32, node);
}

void InstructionSelectorT::VisitTruncateFloat64ToInt64(OpIndex node) {
  Loong64OperandGeneratorT g(this);
  InstructionCode opcode = kLoong64Float64ToInt64;
  const Operation& op = this->Get(node);
  if (op.Is<Opmask::kTruncateFloat64ToInt64OverflowToMin>()) {
    opcode |= MiscField::encode(true);
  }

  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

void InstructionSelectorT::VisitTruncateFloat64ToFloat16RawBits(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitChangeFloat16RawBitsToFloat64(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitTryTruncateFloat32ToInt64(OpIndex node) {
  Loong64OperandGeneratorT g(this);

  InstructionOperand inputs[] = {g.UseRegister(this->input_at(node, 0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kLoong64Float32ToInt64, output_count, outputs, 1, inputs);
}

void InstructionSelectorT::VisitTryTruncateFloat64ToInt64(OpIndex node) {
  Loong64OperandGeneratorT g(this);

  InstructionOperand inputs[] = {g.UseRegister(this->input_at(node, 0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kLoong64Float64ToInt64, output_count, outputs, 1, inputs);
}

void InstructionSelectorT::VisitTryTruncateFloat32ToUint64(OpIndex node) {
  Loong64OperandGeneratorT g(this);

  InstructionOperand inputs[] = {g.UseRegister(this->input_at(node, 0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kLoong64Float32ToUint64, output_count, outputs, 1, inputs);
}

void InstructionSelectorT::VisitTryTruncateFloat64ToUint64(OpIndex node) {
  Loong64OperandGeneratorT g(this);

  InstructionOperand inputs[] = {g.UseRegister(this->input_at(node, 0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kLoong64Float64ToUint64, output_count, outputs, 1, inputs);
}

void InstructionSelectorT::VisitTryTruncateFloat64ToInt32(OpIndex node) {
  Loong64OperandGeneratorT g(this);

  InstructionOperand inputs[] = {g.UseRegister(this->input_at(node, 0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kLoong64Float64ToInt32, output_count, outputs, 1, inputs);
}

void InstructionSelectorT::VisitTryTruncateFloat64ToUint32(OpIndex node) {
  Loong64OperandGeneratorT g(this);

  InstructionOperand inputs[] = {g.UseRegister(this->input_at(node, 0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kLoong64Float64ToUint32, output_count, outputs, 1, inputs);
}

void InstructionSelectorT::VisitBitcastWord32ToWord64(OpIndex node) {
  DCHECK(SmiValuesAre31Bits());
  DCHECK(COMPRESS_POINTERS_BOOL);
  EmitIdentity(node);
}

void InstructionSelectorT::VisitChangeInt32ToInt64(OpIndex node) {
  Loong64OperandGeneratorT g(this);
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
        opcode = load_rep.IsUnsigned() ? kLoong64Ld_bu : kLoong64Ld_b;
        break;
      case MachineRepresentation::kWord16:
        opcode = load_rep.IsUnsigned() ? kLoong64Ld_hu : kLoong64Ld_h;
        break;
      case MachineRepresentation::kWord32:
      case MachineRepresentation::kTaggedSigned:
      case MachineRepresentation::kTagged:
      case MachineRepresentation::kTaggedPointer:
        opcode = kLoong64Ld_w;
        break;
      default:
        UNREACHABLE();
    }
    EmitLoad(this, change_op.input(), opcode, node);
    return;
  } else if (input_op.Is<Opmask::kWord32ShiftRightArithmetic>() &&
             CanCover(node, change_op.input())) {
    // TODO(LOONG_dev): May also optimize 'TruncateInt64ToInt32' here.
    EmitIdentity(node);
  }
  Emit(kLoong64Sll_w, g.DefineAsRegister(node),
       g.UseRegister(change_op.input()), g.TempImmediate(0));
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
  Loong64OperandGeneratorT g(this);
  const ChangeOp& change_op = this->Get(node).template Cast<ChangeOp>();
  OpIndex input = change_op.input();
  const Operation& input_op = this->Get(input);

  if (input_op.Is<LoadOp>() && CanCover(node, input)) {
    // Generate zero-extending load.
    LoadRepresentation load_rep = this->load_view(input).loaded_rep();
    if (load_rep.IsUnsigned() &&
        load_rep.representation() == MachineRepresentation::kWord32) {
      EmitLoad(this, input, kLoong64Ld_wu, node);
      return;
    }
  }
  if (ZeroExtendsWord32ToWord64(input)) {
    EmitIdentity(node);
    return;
  }
  Emit(kLoong64Bstrpick_d, g.DefineAsRegister(node), g.UseRegister(input),
       g.TempImmediate(0), g.TempImmediate(32));
}

void InstructionSelectorT::VisitTruncateInt64ToInt32(OpIndex node) {
  Loong64OperandGeneratorT g(this);
  auto value = input_at(node, 0);
  if (CanCover(node, value)) {
    if (Get(value).Is<Opmask::kWord64ShiftRightArithmetic>()) {
      auto shift_value = input_at(value, 1);
      if (CanCover(value, input_at(value, 0)) &&
          TryEmitExtendingLoad(this, value, node)) {
        return;
      } else if (int64_t constant;
                 MatchSignedIntegralConstant(shift_value, &constant)) {
        if (constant >= 32 && constant <= 63) {
          // After smi untagging no need for truncate. Combine sequence.
          Emit(kLoong64Sra_d, g.DefineAsRegister(node),
               g.UseRegister(input_at(value, 0)), g.UseImmediate(constant));
          return;
        }
      }
    }
  }
  Emit(kLoong64Sll_w, g.DefineAsRegister(node), g.UseRegister(value),
       g.TempImmediate(0));
}

void InstructionSelectorT::VisitTruncateFloat64ToFloat32(OpIndex node) {
  VisitRR(this, kLoong64Float64ToFloat32, node);
}

void InstructionSelectorT::VisitTruncateFloat64ToWord32(OpIndex node) {
  VisitRR(this, kArchTruncateDoubleToI, node);
}

void InstructionSelectorT::VisitRoundFloat64ToInt32(OpIndex node) {
  VisitRR(this, kLoong64Float64ToInt32, node);
}

void InstructionSelectorT::VisitRoundInt64ToFloat32(OpIndex node) {
  VisitRR(this, kLoong64Int64ToFloat32, node);
}

void InstructionSelectorT::VisitRoundInt64ToFloat64(OpIndex node) {
  VisitRR(this, kLoong64Int64ToFloat64, node);
}

void InstructionSelectorT::VisitRoundUint64ToFloat32(OpIndex node) {
  VisitRR(this, kLoong64Uint64ToFloat32, node);
}

void InstructionSelectorT::VisitRoundUint64ToFloat64(OpIndex node) {
  VisitRR(this, kLoong64Uint64ToFloat64, node);
}

void InstructionSelectorT::VisitBitcastFloat32ToInt32(OpIndex node) {
  VisitRR(this, kLoong64Float64ExtractLowWord32, node);
}

void InstructionSelectorT::VisitBitcastFloat64ToInt64(OpIndex node) {
  VisitRR(this, kLoong64BitcastDL, node);
}

void InstructionSelectorT::VisitBitcastInt32ToFloat32(OpIndex node) {
  // when move lower 32 bits of general registers to 64-bit fpu registers on
  // LoongArch64, the upper 32 bits of the fpu register is undefined. So we
  // could just move the whole 64 bits to fpu registers.
  VisitRR(this, kLoong64BitcastLD, node);
}

void InstructionSelectorT::VisitBitcastInt64ToFloat64(OpIndex node) {
  VisitRR(this, kLoong64BitcastLD, node);
}

void InstructionSelectorT::VisitFloat32Add(OpIndex node) {
  VisitRRR(this, kLoong64Float32Add, node);
}

void InstructionSelectorT::VisitFloat64Add(OpIndex node) {
  VisitRRR(this, kLoong64Float64Add, node);
}

void InstructionSelectorT::VisitFloat32Sub(OpIndex node) {
  VisitRRR(this, kLoong64Float32Sub, node);
}

void InstructionSelectorT::VisitFloat64Sub(OpIndex node) {
  VisitRRR(this, kLoong64Float64Sub, node);
}

void InstructionSelectorT::VisitFloat32Mul(OpIndex node) {
  VisitRRR(this, kLoong64Float32Mul, node);
}

void InstructionSelectorT::VisitFloat64Mul(OpIndex node) {
  VisitRRR(this, kLoong64Float64Mul, node);
}

void InstructionSelectorT::VisitFloat32Div(OpIndex node) {
  VisitRRR(this, kLoong64Float32Div, node);
}

void InstructionSelectorT::VisitFloat64Div(OpIndex node) {
  VisitRRR(this, kLoong64Float64Div, node);
}

void InstructionSelectorT::VisitFloat64Mod(OpIndex node) {
  Loong64OperandGeneratorT g(this);
  Emit(kLoong64Float64Mod, g.DefineAsFixed(node, f0),
       g.UseFixed(this->input_at(node, 0), f0),
       g.UseFixed(this->input_at(node, 1), f1))
      ->MarkAsCall();
}

void InstructionSelectorT::VisitFloat32Max(OpIndex node) {
  VisitRRR(this, kLoong64Float32Max, node);
}

void InstructionSelectorT::VisitFloat64Max(OpIndex node) {
  VisitRRR(this, kLoong64Float64Max, node);
}

void InstructionSelectorT::VisitFloat32Min(OpIndex node) {
  VisitRRR(this, kLoong64Float32Min, node);
}

void InstructionSelectorT::VisitFloat64Min(OpIndex node) {
  VisitRRR(this, kLoong64Float64Min, node);
}

void InstructionSelectorT::VisitFloat32Abs(OpIndex node) {
  VisitRR(this, kLoong64Float32Abs, node);
}

void InstructionSelectorT::VisitFloat64Abs(OpIndex node) {
  VisitRR(this, kLoong64Float64Abs, node);
}

void InstructionSelectorT::VisitFloat32Sqrt(OpIndex node) {
  VisitRR(this, kLoong64Float32Sqrt, node);
}

void InstructionSelectorT::VisitFloat64Sqrt(OpIndex node) {
  VisitRR(this, kLoong64Float64Sqrt, node);
}

void InstructionSelectorT::VisitFloat32RoundDown(OpIndex node) {
  VisitRR(this, kLoong64Float32RoundDown, node);
}

void InstructionSelectorT::VisitFloat64RoundDown(OpIndex node) {
  VisitRR(this, kLoong64Float64RoundDown, node);
}

void InstructionSelectorT::VisitFloat32RoundUp(OpIndex node) {
  VisitRR(this, kLoong64Float32RoundUp, node);
}

void InstructionSelectorT::VisitFloat64RoundUp(OpIndex node) {
  VisitRR(this, kLoong64Float64RoundUp, node);
}

void InstructionSelectorT::VisitFloat32RoundTruncate(OpIndex node) {
  VisitRR(this, kLoong64Float32RoundTruncate, node);
}

void InstructionSelectorT::VisitFloat64RoundTruncate(OpIndex node) {
  VisitRR(this, kLoong64Float64RoundTruncate, node);
}

void InstructionSelectorT::VisitFloat64RoundTiesAway(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitFloat32RoundTiesEven(OpIndex node) {
  VisitRR(this, kLoong64Float32RoundTiesEven, node);
}

void InstructionSelectorT::VisitFloat64RoundTiesEven(OpIndex node) {
  VisitRR(this, kLoong64Float64RoundTiesEven, node);
}

void InstructionSelectorT::VisitFloat32Neg(OpIndex node) {
  VisitRR(this, kLoong64Float32Neg, node);
}

void InstructionSelectorT::VisitFloat64Neg(OpIndex node) {
  VisitRR(this, kLoong64Float64Neg, node);
}

void InstructionSelectorT::VisitFloat64Ieee754Binop(OpIndex node,
                                                    InstructionCode opcode) {
  Loong64OperandGeneratorT g(this);
  Emit(opcode, g.DefineAsFixed(node, f0),
       g.UseFixed(this->input_at(node, 0), f0),
       g.UseFixed(this->input_at(node, 1), f1))
      ->MarkAsCall();
}

void InstructionSelectorT::VisitFloat64Ieee754Unop(OpIndex node,
                                                   InstructionCode opcode) {
  Loong64OperandGeneratorT g(this);
  Emit(opcode, g.DefineAsFixed(node, f0),
       g.UseFixed(this->input_at(node, 0), f0))
      ->MarkAsCall();
}

void InstructionSelectorT::EmitMoveParamToFPR(OpIndex node, int32_t index) {
  OperandGenerator g(this);
  int count = linkage()->GetParameterLocation(index).GetLocation();
  InstructionOperand out_op = g.TempRegister(-count);
  Emit(kArchNop, out_op);
  Emit(kLoong64BitcastLD, g.DefineAsRegister(node), out_op);
}

void InstructionSelectorT::EmitMoveFPRToParam(InstructionOperand* op,
                                              LinkageLocation location) {
  OperandGenerator g(this);
  int count = location.GetLocation();
  InstructionOperand new_op = g.TempRegister(-count);
  Emit(kLoong64BitcastDL, new_op, *op);
  *op = new_op;
}

void InstructionSelectorT::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* call_descriptor,
    OpIndex node) {
  Loong64OperandGeneratorT g(this);

  // Prepare for C function call.
  if (call_descriptor->IsCFunctionCall()) {
    int gp_param_count = static_cast<int>(call_descriptor->GPParameterCount());
    int fp_param_count = static_cast<int>(call_descriptor->FPParameterCount());
    Emit(kArchPrepareCallCFunction | ParamField::encode(gp_param_count) |
             FPParamField::encode(fp_param_count),
         0, nullptr, 0, nullptr);

    // Poke any stack arguments.
    int slot = 0;
    for (PushParameter input : (*arguments)) {
      Emit(kLoong64Poke, g.NoOutput(), g.UseRegister(input.node),
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
      Emit(kLoong64StackClaim, g.NoOutput(),
           g.TempImmediate(stack_size << kSystemPointerSizeLog2));
    }
    for (size_t n = 0; n < arguments->size(); ++n) {
      PushParameter input = (*arguments)[n];
      if (input.node.valid()) {
        Emit(kLoong64Poke, g.NoOutput(), g.UseRegister(input.node),
             g.TempImmediate(static_cast<int>(n << kSystemPointerSizeLog2)));
      }
    }
  }
}

void InstructionSelectorT::EmitPrepareResults(
    ZoneVector<PushParameter>* results, const CallDescriptor* call_descriptor,
    OpIndex node) {
  Loong64OperandGeneratorT g(this);

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
        abort();
      }
      int offset = call_descriptor->GetOffsetToReturns();
      int reverse_slot = -output.location.GetLocation() - offset;
      Emit(kLoong64Peek, g.DefineAsRegister(output.node),
           g.UseImmediate(reverse_slot));
    }
  }
}

bool InstructionSelectorT::IsTailCallAddressImmediate() { return false; }

void InstructionSelectorT::VisitUnalignedLoad(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitUnalignedStore(OpIndex node) { UNREACHABLE(); }

namespace {

// Shared routine for multiple compare operations.
static Instruction* VisitCompare(InstructionSelectorT* selector,
                                 InstructionCode opcode,
                                 InstructionOperand left,
                                 InstructionOperand right,
                                 FlagsContinuationT* cont) {
#ifdef V8_COMPRESS_POINTERS
  if (opcode == kLoong64Cmp32) {
    Loong64OperandGeneratorT g(selector);
    InstructionOperand inputs[] = {left, right};
    if (right.IsImmediate()) {
      InstructionOperand temps[1] = {g.TempRegister()};
      return selector->EmitWithContinuation(opcode, 0, nullptr,
                                            arraysize(inputs), inputs,
                                            arraysize(temps), temps, cont);
    } else {
      InstructionOperand temps[2] = {g.TempRegister(), g.TempRegister()};
      return selector->EmitWithContinuation(opcode, 0, nullptr,
                                            arraysize(inputs), inputs,
                                            arraysize(temps), temps, cont);
    }
  }
#endif
  return selector->EmitWithContinuation(opcode, left, right, cont);
}

// Shared routine for multiple float32 compare operations.
void VisitFloat32Compare(InstructionSelectorT* selector, OpIndex node,
                         FlagsContinuationT* cont) {
  Loong64OperandGeneratorT g(selector);
  const ComparisonOp& op = selector->Get(node).template Cast<ComparisonOp>();
  OpIndex left = op.left();
  OpIndex right = op.right();
  InstructionOperand lhs, rhs;

  lhs = selector->MatchZero(left) ? g.UseImmediate(left) : g.UseRegister(left);
  rhs =
      selector->MatchZero(right) ? g.UseImmediate(right) : g.UseRegister(right);
  VisitCompare(selector, kLoong64Float32Cmp, lhs, rhs, cont);
}

// Shared routine for multiple float64 compare operations.
void VisitFloat64Compare(InstructionSelectorT* selector, OpIndex node,
                         FlagsContinuationT* cont) {
  Loong64OperandGeneratorT g(selector);
  const Operation& compare = selector->Get(node);
  DCHECK(compare.Is<ComparisonOp>());
  OpIndex lhs = compare.input(0);
  OpIndex rhs = compare.input(1);
  if (selector->MatchZero(rhs)) {
    VisitCompare(selector, kLoong64Float64Cmp, g.UseRegister(lhs),
                 g.UseImmediate(rhs), cont);
  } else if (selector->MatchZero(lhs)) {
    VisitCompare(selector, kLoong64Float64Cmp, g.UseImmediate(lhs),
                 g.UseRegister(rhs), cont);
  } else {
    VisitCompare(selector, kLoong64Float64Cmp, g.UseRegister(lhs),
                 g.UseRegister(rhs), cont);
  }
}

// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelectorT* selector, OpIndex node,
                      InstructionCode opcode, FlagsContinuationT* cont,
                      bool commutative) {
  Loong64OperandGeneratorT g(selector);
  DCHECK_EQ(selector->value_input_count(node), 2);
  auto left = selector->input_at(node, 0);
  auto right = selector->input_at(node, 1);

  // Match immediates on left or right side of comparison.
  if (g.CanBeImmediate(right, opcode)) {
    if (opcode == kLoong64Tst) {
      VisitCompare(selector, opcode, g.UseRegister(left), g.UseImmediate(right),
                   cont);
    } else {
      switch (cont->condition()) {
        case kEqual:
        case kNotEqual:
          if (cont->IsSet()) {
            VisitCompare(selector, opcode, g.UseUniqueRegister(left),
                         g.UseImmediate(right), cont);
          } else {
            VisitCompare(selector, opcode, g.UseUniqueRegister(left),
                         g.UseImmediate(right), cont);
          }
          break;
        case kSignedLessThan:
        case kSignedGreaterThanOrEqual:
        case kSignedLessThanOrEqual:
        case kSignedGreaterThan:
        case kUnsignedLessThan:
        case kUnsignedGreaterThanOrEqual:
        case kUnsignedLessThanOrEqual:
        case kUnsignedGreaterThan:
          VisitCompare(selector, opcode, g.UseUniqueRegister(left),
                       g.UseImmediate(right), cont);
          break;
        default:
          UNREACHABLE();
      }
    }
  } else if (g.CanBeImmediate(left, opcode)) {
    if (!commutative) cont->Commute();
    if (opcode == kLoong64Tst) {
      VisitCompare(selector, opcode, g.UseRegister(right), g.UseImmediate(left),
                   cont);
    } else {
      switch (cont->condition()) {
        case kEqual:
        case kNotEqual:
          if (cont->IsSet()) {
            VisitCompare(selector, opcode, g.UseUniqueRegister(right),
                         g.UseImmediate(left), cont);
          } else {
            VisitCompare(selector, opcode, g.UseUniqueRegister(right),
                         g.UseImmediate(left), cont);
          }
          break;
        case kSignedLessThan:
        case kSignedGreaterThanOrEqual:
        case kSignedLessThanOrEqual:
        case kSignedGreaterThan:
        case kUnsignedLessThan:
        case kUnsignedGreaterThanOrEqual:
        case kUnsignedLessThanOrEqual:
        case kUnsignedGreaterThan:
          VisitCompare(selector, opcode, g.UseUniqueRegister(right),
                       g.UseImmediate(left), cont);
          break;
        default:
          UNREACHABLE();
      }
    }
  } else {
    VisitCompare(selector, opcode, g.UseUniqueRegister(left),
                 g.UseUniqueRegister(right), cont);
  }
}

// Shared routine for multiple word compare operations.
void VisitFullWord32Compare(InstructionSelectorT* selector, OpIndex node,
                            InstructionCode opcode, FlagsContinuationT* cont) {
  Loong64OperandGeneratorT g(selector);
  InstructionOperand leftOp = g.TempRegister();
  InstructionOperand rightOp = g.TempRegister();

  selector->Emit(kLoong64Sll_d, leftOp,
                 g.UseRegister(selector->input_at(node, 0)),
                 g.TempImmediate(32));
  selector->Emit(kLoong64Sll_d, rightOp,
                 g.UseRegister(selector->input_at(node, 1)),
                 g.TempImmediate(32));

  Instruction* instr = VisitCompare(selector, opcode, leftOp, rightOp, cont);
  selector->UpdateSourcePosition(instr, node);
}

void VisitWord32Compare(InstructionSelectorT* selector, OpIndex node,
                        FlagsContinuationT* cont) {
  VisitFullWord32Compare(selector, node, kLoong64Cmp64, cont);
}

void VisitWord64Compare(InstructionSelectorT* selector, OpIndex node,
                        FlagsContinuationT* cont) {
  VisitWordCompare(selector, node, kLoong64Cmp64, cont, false);
}

void VisitAtomicLoad(InstructionSelectorT* selector, OpIndex node,
                     AtomicWidth width) {
  using OpIndex = OpIndex;
  Loong64OperandGeneratorT g(selector);
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
      code = (width == AtomicWidth::kWord32) ? kAtomicLoadWord32
                                             : kLoong64Word64AtomicLoadUint32;
      break;
    case MachineRepresentation::kWord64:
      code = kLoong64Word64AtomicLoadUint64;
      break;
#ifdef V8_COMPRESS_POINTERS
    case MachineRepresentation::kTaggedSigned:
      code = kLoong64AtomicLoadDecompressTaggedSigned;
      break;
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTagged:
      code = kLoong64AtomicLoadDecompressTagged;
      break;
#else
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:
      code = kLoong64Word64AtomicLoadUint64;
      break;
#endif
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:
      DCHECK(COMPRESS_POINTERS_BOOL);
      code = kLoong64Word64AtomicLoadUint32;
      break;
    default:
      UNREACHABLE();
  }

  bool traps_on_null;
  if (load.is_protected(&traps_on_null)) {
    // Atomic loads and null dereference are mutually exclusive. This might
    // change with multi-threaded wasm-gc in which case the access mode should
    // probably be kMemoryAccessProtectedNullDereference.
    DCHECK(!traps_on_null);
    code |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
  }

  if (g.CanBeImmediate(index, code)) {
    selector->Emit(code | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.DefineAsRegister(node), g.UseRegister(base),
                   g.UseImmediate(index));
  } else {
    selector->Emit(code | AddressingModeField::encode(kMode_MRR) |
                       AtomicWidthField::encode(width),
                   g.DefineAsRegister(node), g.UseRegister(base),
                   g.UseRegister(index));
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
  Loong64OperandGeneratorT g(selector);
  auto store = selector->store_view(node);
  OpIndex base = store.base();
  OpIndex index = selector->value(store.index());
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
    DCHECK_EQ(kTaggedSize, 8);

    RecordWriteMode record_write_mode =
        WriteBarrierKindToRecordWriteMode(write_barrier_kind);
    code = kArchAtomicStoreWithWriteBarrier;
    code |= RecordWriteModeField::encode(record_write_mode);
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
        code = kLoong64Word64AtomicStoreWord64;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:
        DCHECK_EQ(AtomicWidthSize(width), kTaggedSize);
        code = kLoong64AtomicStoreCompressTagged;
        break;
      case MachineRepresentation::kCompressedPointer:  // Fall through.
      case MachineRepresentation::kCompressed:
        DCHECK(COMPRESS_POINTERS_BOOL);
        DCHECK_EQ(width, AtomicWidth::kWord32);
        code = kLoong64AtomicStoreCompressTagged;
        break;
      default:
        UNREACHABLE();
    }
  }

  if (store_params.kind() == MemoryAccessKind::kProtectedByTrapHandler) {
    code |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
  }

  if (g.CanBeImmediate(index, code)) {
    selector->Emit(code | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.NoOutput(), g.UseRegister(base), g.UseImmediate(index),
                   g.UseRegisterOrImmediateZero(value));
  } else {
    selector->Emit(code | AddressingModeField::encode(kMode_MRR) |
                       AtomicWidthField::encode(width),
                   g.NoOutput(), g.UseRegister(base), g.UseRegister(index),
                   g.UseRegisterOrImmediateZero(value));
  }
}

void VisitAtomicExchange(InstructionSelectorT* selector, OpIndex node,
                         ArchOpcode opcode, AtomicWidth width,
                         MemoryAccessKind access_kind) {
  using OpIndex = OpIndex;
  Loong64OperandGeneratorT g(selector);
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
  if (access_kind == MemoryAccessKind::kProtectedByTrapHandler) {
    code |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
  }
  selector->Emit(code, 1, outputs, input_count, inputs, 3, temp);
}

void VisitAtomicCompareExchange(InstructionSelectorT* selector, OpIndex node,
                                ArchOpcode opcode, AtomicWidth width,
                                MemoryAccessKind access_kind) {
  using OpIndex = OpIndex;
  Loong64OperandGeneratorT g(selector);
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
  if (access_kind == MemoryAccessKind::kProtectedByTrapHandler) {
    code |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
  }
  selector->Emit(code, 1, outputs, input_count, inputs, 3, temp);
}

void VisitAtomicBinop(InstructionSelectorT* selector, OpIndex node,
                      ArchOpcode opcode, AtomicWidth width,
                      MemoryAccessKind access_kind) {
  using OpIndex = OpIndex;
  Loong64OperandGeneratorT g(selector);
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
  if (access_kind == MemoryAccessKind::kProtectedByTrapHandler) {
    code |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
  }
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
      kArchStackPointerGreaterThan | MiscField::encode(static_cast<int>(kind));

  Loong64OperandGeneratorT g(this);

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
    Loong64OperandGeneratorT g(this);
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
                                    is64 ? kLoong64AddOvf_d : kLoong64Add_d,
                                    cont);
                case OverflowCheckedBinopOp::Kind::kSignedSub:
                  cont->OverwriteAndNegateIfEqual(kOverflow);
                  return VisitBinop(this, node,
                                    is64 ? kLoong64SubOvf_d : kLoong64Sub_d,
                                    cont);
                case OverflowCheckedBinopOp::Kind::kSignedMul:
                  cont->OverwriteAndNegateIfEqual(kOverflow);
                  return VisitBinop(this, node,
                                    is64 ? kLoong64MulOvf_d : kLoong64MulOvf_w,
                                    cont);
              }
            }
          }
        }
      } else if (value_op.Is<Opmask::kWord32BitwiseAnd>() ||
                 value_op.Is<Opmask::kWord64BitwiseAnd>()) {
        return VisitWordCompare(this, value, kLoong64Tst, cont, true);
      } else if (value_op.Is<StackPointerGreaterThanOp>()) {
        cont->OverwriteAndNegateIfEqual(kStackPointerGreaterThanCondition);
        return VisitStackPointerGreaterThan(value, cont);
      }
    }

    // Continuation could not be combined with a compare, emit compare against
    // 0.
    VisitCompare(this, kLoong64Cmp32, g.UseRegister(value), g.TempImmediate(0),
                 cont);
  }
}

void InstructionSelectorT::VisitSwitch(OpIndex node, const SwitchInfo& sw) {
  Loong64OperandGeneratorT g(this);
  InstructionOperand value_operand = g.UseRegister(this->input_at(node, 0));

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
        Emit(kLoong64Sub_w, index_operand, value_operand,
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

  if (isolate() && (V8_STATIC_ROOTS_BOOL ||
                    (COMPRESS_POINTERS_BOOL && !isolate()->bootstrapper()))) {
    Loong64OperandGeneratorT g(this);
    const RootsTable& roots_table = isolate()->roots_table();
    RootIndex root_index;
    Handle<HeapObject> right;
    // HeapConstants and CompressedHeapConstants can be treated the same when
    // using them as an input to a 32-bit comparison. Check whether either is
    // present.
    if (MatchHeapConstant(node, &right) && !right.is_null() &&
        roots_table.IsRootHandle(right, &root_index)) {
      if (RootsTable::IsReadOnly(root_index)) {
        Tagged_t ptr =
            MacroAssemblerBase::ReadOnlyRootPtr(root_index, isolate());
        if (g.CanBeImmediate(ptr, kLoong64Cmp32)) {
          VisitCompare(this, kLoong64Cmp32, g.UseRegister(left),
                       g.TempImmediate(int32_t(ptr)), &cont);
          return;
        }
      }
    }
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
    return VisitBinop(this, node, kLoong64Add_d, &cont);
  }

  FlagsContinuation cont;
  VisitBinop(this, node, kLoong64Add_d, &cont);
}

void InstructionSelectorT::VisitInt32SubWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop(this, node, kLoong64Sub_d, &cont);
  }

  FlagsContinuation cont;
  VisitBinop(this, node, kLoong64Sub_d, &cont);
}

void InstructionSelectorT::VisitInt32MulWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop(this, node, kLoong64MulOvf_w, &cont);
  }

  FlagsContinuation cont;
  VisitBinop(this, node, kLoong64MulOvf_w, &cont);
}

void InstructionSelectorT::VisitInt64MulWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop(this, node, kLoong64MulOvf_d, &cont);
  }

  FlagsContinuation cont;
  VisitBinop(this, node, kLoong64MulOvf_d, &cont);
}

void InstructionSelectorT::VisitInt64AddWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop(this, node, kLoong64AddOvf_d, &cont);
  }

  FlagsContinuation cont;
  VisitBinop(this, node, kLoong64AddOvf_d, &cont);
}

void InstructionSelectorT::VisitInt64SubWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop(this, node, kLoong64SubOvf_d, &cont);
  }

  FlagsContinuation cont;
  VisitBinop(this, node, kLoong64SubOvf_d, &cont);
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
  VisitRR(this, kLoong64Float64ExtractLowWord32, node);
}

void InstructionSelectorT::VisitFloat64ExtractHighWord32(OpIndex node) {
  VisitRR(this, kLoong64Float64ExtractHighWord32, node);
}

void InstructionSelectorT::VisitBitcastWord32PairToFloat64(OpIndex node) {
  Loong64OperandGeneratorT g(this);
  const auto& bitcast = this->Cast<BitcastWord32PairToFloat64Op>(node);
  OpIndex hi = bitcast.high_word32();
  OpIndex lo = bitcast.low_word32();

  InstructionOperand temps[] = {g.TempRegister()};
  Emit(kLoong64Float64FromWord32Pair, g.DefineAsRegister(node), g.Use(hi),
       g.Use(lo), arraysize(temps), temps);
}

void InstructionSelectorT::VisitFloat64SilenceNaN(OpIndex node) {
  VisitRR(this, kLoong64Float64SilenceNaN, node);
}

void InstructionSelectorT::VisitFloat64InsertLowWord32(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitFloat64InsertHighWord32(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitMemoryBarrier(OpIndex node) {
  Loong64OperandGeneratorT g(this);
  Emit(kLoong64Dbar, g.NoOutput());
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
  VisitAtomicExchange(this, node, opcode, AtomicWidth::kWord32,
                      atomic_op.memory_access_kind);
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
    opcode = kLoong64Word64AtomicExchangeUint64;
  } else {
    UNREACHABLE();
  }
  VisitAtomicExchange(this, node, opcode, AtomicWidth::kWord64,
                      atomic_op.memory_access_kind);
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
  VisitAtomicCompareExchange(this, node, opcode, AtomicWidth::kWord32,
                             atomic_op.memory_access_kind);
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
    opcode = kLoong64Word64AtomicCompareExchangeUint64;
  } else {
    UNREACHABLE();
  }
  VisitAtomicCompareExchange(this, node, opcode, AtomicWidth::kWord64,
                             atomic_op.memory_access_kind);
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
  VisitAtomicBinop(this, node, opcode, AtomicWidth::kWord32,
                   atomic_op.memory_access_kind);
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
  VisitAtomicBinop(this, node, opcode, AtomicWidth::kWord64,
                   atomic_op.memory_access_kind);
}

#define VISIT_ATOMIC_BINOP(op)                                                 \
  void InstructionSelectorT::VisitWord64Atomic##op(OpIndex node) {             \
    VisitWord64AtomicBinaryOperation(node, kAtomic##op##Uint8,                 \
                                     kAtomic##op##Uint16, kAtomic##op##Word32, \
                                     kLoong64Word64Atomic##op##Uint64);        \
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

#define SIMD_TYPE_LIST(V) \
  V(F64x2)                \
  V(F32x4)                \
  V(I64x2)                \
  V(I32x4)                \
  V(I16x8)                \
  V(I8x16)

#define SIMD_UNOP_LIST(V)                                             \
  V(F64x2Abs, kLoong64F64x2Abs)                                       \
  V(F64x2Neg, kLoong64F64x2Neg)                                       \
  V(F64x2Sqrt, kLoong64F64x2Sqrt)                                     \
  V(F64x2Ceil, kLoong64F64x2Ceil)                                     \
  V(F64x2Floor, kLoong64F64x2Floor)                                   \
  V(F64x2Trunc, kLoong64F64x2Trunc)                                   \
  V(F64x2NearestInt, kLoong64F64x2NearestInt)                         \
  V(I64x2Neg, kLoong64I64x2Neg)                                       \
  V(I64x2BitMask, kLoong64I64x2BitMask)                               \
  V(F64x2ConvertLowI32x4S, kLoong64F64x2ConvertLowI32x4S)             \
  V(F64x2ConvertLowI32x4U, kLoong64F64x2ConvertLowI32x4U)             \
  V(F64x2PromoteLowF32x4, kLoong64F64x2PromoteLowF32x4)               \
  V(F32x4SConvertI32x4, kLoong64F32x4SConvertI32x4)                   \
  V(F32x4UConvertI32x4, kLoong64F32x4UConvertI32x4)                   \
  V(F32x4Abs, kLoong64F32x4Abs)                                       \
  V(F32x4Neg, kLoong64F32x4Neg)                                       \
  V(F32x4Sqrt, kLoong64F32x4Sqrt)                                     \
  V(F32x4Ceil, kLoong64F32x4Ceil)                                     \
  V(F32x4Floor, kLoong64F32x4Floor)                                   \
  V(F32x4Trunc, kLoong64F32x4Trunc)                                   \
  V(F32x4NearestInt, kLoong64F32x4NearestInt)                         \
  V(F32x4DemoteF64x2Zero, kLoong64F32x4DemoteF64x2Zero)               \
  V(I64x2Abs, kLoong64I64x2Abs)                                       \
  V(I64x2SConvertI32x4Low, kLoong64I64x2SConvertI32x4Low)             \
  V(I64x2SConvertI32x4High, kLoong64I64x2SConvertI32x4High)           \
  V(I64x2UConvertI32x4Low, kLoong64I64x2UConvertI32x4Low)             \
  V(I64x2UConvertI32x4High, kLoong64I64x2UConvertI32x4High)           \
  V(I32x4SConvertF32x4, kLoong64I32x4SConvertF32x4)                   \
  V(I32x4UConvertF32x4, kLoong64I32x4UConvertF32x4)                   \
  V(I32x4Neg, kLoong64I32x4Neg)                                       \
  V(I32x4SConvertI16x8Low, kLoong64I32x4SConvertI16x8Low)             \
  V(I32x4SConvertI16x8High, kLoong64I32x4SConvertI16x8High)           \
  V(I32x4UConvertI16x8Low, kLoong64I32x4UConvertI16x8Low)             \
  V(I32x4UConvertI16x8High, kLoong64I32x4UConvertI16x8High)           \
  V(I32x4Abs, kLoong64I32x4Abs)                                       \
  V(I32x4BitMask, kLoong64I32x4BitMask)                               \
  V(I32x4TruncSatF64x2SZero, kLoong64I32x4TruncSatF64x2SZero)         \
  V(I32x4TruncSatF64x2UZero, kLoong64I32x4TruncSatF64x2UZero)         \
  V(I32x4RelaxedTruncF32x4S, kLoong64I32x4RelaxedTruncF32x4S)         \
  V(I32x4RelaxedTruncF32x4U, kLoong64I32x4RelaxedTruncF32x4U)         \
  V(I32x4RelaxedTruncF64x2SZero, kLoong64I32x4RelaxedTruncF64x2SZero) \
  V(I32x4RelaxedTruncF64x2UZero, kLoong64I32x4RelaxedTruncF64x2UZero) \
  V(I16x8Neg, kLoong64I16x8Neg)                                       \
  V(I16x8SConvertI8x16Low, kLoong64I16x8SConvertI8x16Low)             \
  V(I16x8SConvertI8x16High, kLoong64I16x8SConvertI8x16High)           \
  V(I16x8UConvertI8x16Low, kLoong64I16x8UConvertI8x16Low)             \
  V(I16x8UConvertI8x16High, kLoong64I16x8UConvertI8x16High)           \
  V(I16x8Abs, kLoong64I16x8Abs)                                       \
  V(I16x8BitMask, kLoong64I16x8BitMask)                               \
  V(I8x16Neg, kLoong64I8x16Neg)                                       \
  V(I8x16Abs, kLoong64I8x16Abs)                                       \
  V(I8x16Popcnt, kLoong64I8x16Popcnt)                                 \
  V(I8x16BitMask, kLoong64I8x16BitMask)                               \
  V(S128Not, kLoong64S128Not)                                         \
  V(I64x2AllTrue, kLoong64I64x2AllTrue)                               \
  V(I32x4AllTrue, kLoong64I32x4AllTrue)                               \
  V(I16x8AllTrue, kLoong64I16x8AllTrue)                               \
  V(I8x16AllTrue, kLoong64I8x16AllTrue)                               \
  V(V128AnyTrue, kLoong64V128AnyTrue)

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

#define SIMD_BINOP_LIST(V)                                \
  V(F64x2Add, kLoong64F64x2Add)                           \
  V(F64x2Sub, kLoong64F64x2Sub)                           \
  V(F64x2Mul, kLoong64F64x2Mul)                           \
  V(F64x2Div, kLoong64F64x2Div)                           \
  V(F64x2Min, kLoong64F64x2Min)                           \
  V(F64x2Max, kLoong64F64x2Max)                           \
  V(F64x2Eq, kLoong64F64x2Eq)                             \
  V(F64x2Ne, kLoong64F64x2Ne)                             \
  V(F64x2Lt, kLoong64F64x2Lt)                             \
  V(F64x2Le, kLoong64F64x2Le)                             \
  V(F64x2RelaxedMin, kLoong64F64x2RelaxedMin)             \
  V(F64x2RelaxedMax, kLoong64F64x2RelaxedMax)             \
  V(I64x2Eq, kLoong64I64x2Eq)                             \
  V(I64x2Ne, kLoong64I64x2Ne)                             \
  V(I64x2Add, kLoong64I64x2Add)                           \
  V(I64x2Sub, kLoong64I64x2Sub)                           \
  V(I64x2Mul, kLoong64I64x2Mul)                           \
  V(I64x2GtS, kLoong64I64x2GtS)                           \
  V(I64x2GeS, kLoong64I64x2GeS)                           \
  V(F32x4Add, kLoong64F32x4Add)                           \
  V(F32x4Sub, kLoong64F32x4Sub)                           \
  V(F32x4Mul, kLoong64F32x4Mul)                           \
  V(F32x4Div, kLoong64F32x4Div)                           \
  V(F32x4Max, kLoong64F32x4Max)                           \
  V(F32x4Min, kLoong64F32x4Min)                           \
  V(F32x4Eq, kLoong64F32x4Eq)                             \
  V(F32x4Ne, kLoong64F32x4Ne)                             \
  V(F32x4Lt, kLoong64F32x4Lt)                             \
  V(F32x4Le, kLoong64F32x4Le)                             \
  V(F32x4RelaxedMin, kLoong64F32x4RelaxedMin)             \
  V(F32x4RelaxedMax, kLoong64F32x4RelaxedMax)             \
  V(I32x4Add, kLoong64I32x4Add)                           \
  V(I32x4Sub, kLoong64I32x4Sub)                           \
  V(I32x4Mul, kLoong64I32x4Mul)                           \
  V(I32x4MaxS, kLoong64I32x4MaxS)                         \
  V(I32x4MinS, kLoong64I32x4MinS)                         \
  V(I32x4MaxU, kLoong64I32x4MaxU)                         \
  V(I32x4MinU, kLoong64I32x4MinU)                         \
  V(I32x4Eq, kLoong64I32x4Eq)                             \
  V(I32x4Ne, kLoong64I32x4Ne)                             \
  V(I32x4GtS, kLoong64I32x4GtS)                           \
  V(I32x4GeS, kLoong64I32x4GeS)                           \
  V(I32x4GtU, kLoong64I32x4GtU)                           \
  V(I32x4GeU, kLoong64I32x4GeU)                           \
  V(I32x4DotI16x8S, kLoong64I32x4DotI16x8S)               \
  V(I16x8Add, kLoong64I16x8Add)                           \
  V(I16x8AddSatS, kLoong64I16x8AddSatS)                   \
  V(I16x8AddSatU, kLoong64I16x8AddSatU)                   \
  V(I16x8Sub, kLoong64I16x8Sub)                           \
  V(I16x8SubSatS, kLoong64I16x8SubSatS)                   \
  V(I16x8SubSatU, kLoong64I16x8SubSatU)                   \
  V(I16x8Mul, kLoong64I16x8Mul)                           \
  V(I16x8MaxS, kLoong64I16x8MaxS)                         \
  V(I16x8MinS, kLoong64I16x8MinS)                         \
  V(I16x8MaxU, kLoong64I16x8MaxU)                         \
  V(I16x8MinU, kLoong64I16x8MinU)                         \
  V(I16x8Eq, kLoong64I16x8Eq)                             \
  V(I16x8Ne, kLoong64I16x8Ne)                             \
  V(I16x8GtS, kLoong64I16x8GtS)                           \
  V(I16x8GeS, kLoong64I16x8GeS)                           \
  V(I16x8GtU, kLoong64I16x8GtU)                           \
  V(I16x8GeU, kLoong64I16x8GeU)                           \
  V(I16x8RoundingAverageU, kLoong64I16x8RoundingAverageU) \
  V(I16x8SConvertI32x4, kLoong64I16x8SConvertI32x4)       \
  V(I16x8UConvertI32x4, kLoong64I16x8UConvertI32x4)       \
  V(I16x8Q15MulRSatS, kLoong64I16x8Q15MulRSatS)           \
  V(I16x8RelaxedQ15MulRS, kLoong64I16x8RelaxedQ15MulRS)   \
  V(I8x16Add, kLoong64I8x16Add)                           \
  V(I8x16AddSatS, kLoong64I8x16AddSatS)                   \
  V(I8x16AddSatU, kLoong64I8x16AddSatU)                   \
  V(I8x16Sub, kLoong64I8x16Sub)                           \
  V(I8x16SubSatS, kLoong64I8x16SubSatS)                   \
  V(I8x16SubSatU, kLoong64I8x16SubSatU)                   \
  V(I8x16MaxS, kLoong64I8x16MaxS)                         \
  V(I8x16MinS, kLoong64I8x16MinS)                         \
  V(I8x16MaxU, kLoong64I8x16MaxU)                         \
  V(I8x16MinU, kLoong64I8x16MinU)                         \
  V(I8x16Eq, kLoong64I8x16Eq)                             \
  V(I8x16Ne, kLoong64I8x16Ne)                             \
  V(I8x16GtS, kLoong64I8x16GtS)                           \
  V(I8x16GeS, kLoong64I8x16GeS)                           \
  V(I8x16GtU, kLoong64I8x16GtU)                           \
  V(I8x16GeU, kLoong64I8x16GeU)                           \
  V(I8x16RoundingAverageU, kLoong64I8x16RoundingAverageU) \
  V(I8x16SConvertI16x8, kLoong64I8x16SConvertI16x8)       \
  V(I8x16UConvertI16x8, kLoong64I8x16UConvertI16x8)       \
  V(S128And, kLoong64S128And)                             \
  V(S128Or, kLoong64S128Or)                               \
  V(S128Xor, kLoong64S128Xor)                             \
  V(S128AndNot, kLoong64S128AndNot)

void InstructionSelectorT::VisitS128Const(OpIndex node) { UNIMPLEMENTED(); }

void InstructionSelectorT::VisitS128Zero(OpIndex node) { UNIMPLEMENTED(); }

#define SIMD_VISIT_SPLAT(Type)                                  \
  void InstructionSelectorT::Visit##Type##Splat(OpIndex node) { \
    VisitRR(this, kLoong64##Type##Splat, node);                 \
  }
SIMD_TYPE_LIST(SIMD_VISIT_SPLAT)
#undef SIMD_VISIT_SPLAT

#define SIMD_VISIT_EXTRACT_LANE(Type, Sign)                                 \
  void InstructionSelectorT::Visit##Type##ExtractLane##Sign(OpIndex node) { \
    VisitRRI(this, kLoong64##Type##ExtractLane##Sign, node);                \
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
    VisitRRIR(this, kLoong64##Type##ReplaceLane, node);               \
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
    VisitSimdShift(this, kLoong64##Name, node);          \
  }
SIMD_SHIFT_OP_LIST(SIMD_VISIT_SHIFT_OP)
#undef SIMD_VISIT_SHIFT_OP

#define SIMD_VISIT_BINOP(Name, instruction)              \
  void InstructionSelectorT::Visit##Name(OpIndex node) { \
    VisitRRR(this, instruction, node);                   \
  }
SIMD_BINOP_LIST(SIMD_VISIT_BINOP)
#undef SIMD_VISIT_BINOP

void InstructionSelectorT::VisitS128Select(OpIndex node) {
  VisitRRRR(this, kLoong64S128Select, node);
}

void InstructionSelectorT::VisitI8x16RelaxedLaneSelect(OpIndex node) {
  VisitS128Select(node);
}

void InstructionSelectorT::VisitI16x8RelaxedLaneSelect(OpIndex node) {
  VisitS128Select(node);
}

void InstructionSelectorT::VisitI32x4RelaxedLaneSelect(OpIndex node) {
  VisitS128Select(node);
}

void InstructionSelectorT::VisitI64x2RelaxedLaneSelect(OpIndex node) {
  VisitS128Select(node);
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

#if V8_ENABLE_WEBASSEMBLY

void InstructionSelectorT::VisitI8x16Shuffle(OpIndex node) { UNIMPLEMENTED(); }

#else
void InstructionSelectorT::VisitI8x16Shuffle(OpIndex node) { UNREACHABLE(); }
#endif  // V8_ENABLE_WEBASSEMBLY

void InstructionSelectorT::VisitI8x16Swizzle(OpIndex node) { UNIMPLEMENTED(); }

void InstructionSelectorT::VisitSetStackPointer(OpIndex node) {
  OperandGenerator g(this);
  auto input = g.UseRegister(this->input_at(node, 0));
  Emit(kArchSetStackPointer, 0, nullptr, 1, &input);
}

void InstructionSelectorT::VisitSignExtendWord8ToInt32(OpIndex node) {
  VisitRR(this, kLoong64Ext_w_b, node);
}

void InstructionSelectorT::VisitSignExtendWord16ToInt32(OpIndex node) {
  VisitRR(this, kLoong64Ext_w_h, node);
}

void InstructionSelectorT::VisitSignExtendWord8ToInt64(OpIndex node) {
  VisitRR(this, kLoong64Ext_w_b, node);
}

void InstructionSelectorT::VisitSignExtendWord16ToInt64(OpIndex node) {
  VisitRR(this, kLoong64Ext_w_h, node);
}

void InstructionSelectorT::VisitSignExtendWord32ToInt64(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitF32x4Pmin(OpIndex node) {
  VisitUniqueRRR(this, kLoong64F32x4Pmin, node);
}

void InstructionSelectorT::VisitF32x4Pmax(OpIndex node) {
  VisitUniqueRRR(this, kLoong64F32x4Pmax, node);
}

void InstructionSelectorT::VisitF64x2Pmin(OpIndex node) {
  VisitUniqueRRR(this, kLoong64F64x2Pmin, node);
}

void InstructionSelectorT::VisitF64x2Pmax(OpIndex node) {
  VisitUniqueRRR(this, kLoong64F64x2Pmax, node);
}

#define VISIT_EXT_MUL(OPCODE1, OPCODE2)                           \
  void InstructionSelectorT::Visit##OPCODE1##ExtMulLow##OPCODE2(  \
      OpIndex node) {}                                            \
  void InstructionSelectorT::Visit##OPCODE1##ExtMulHigh##OPCODE2( \
      OpIndex node) {}

VISIT_EXT_MUL(I64x2, I32x4S)
VISIT_EXT_MUL(I64x2, I32x4U)
VISIT_EXT_MUL(I32x4, I16x8S)
VISIT_EXT_MUL(I32x4, I16x8U)
VISIT_EXT_MUL(I16x8, I8x16S)
VISIT_EXT_MUL(I16x8, I8x16U)
#undef VISIT_EXT_MUL

#define VISIT_EXTADD_PAIRWISE(OPCODE) \
  void InstructionSelectorT::Visit##OPCODE(OpIndex node) { UNIMPLEMENTED(); }
VISIT_EXTADD_PAIRWISE(I16x8ExtAddPairwiseI8x16S)
VISIT_EXTADD_PAIRWISE(I16x8ExtAddPairwiseI8x16U)
VISIT_EXTADD_PAIRWISE(I32x4ExtAddPairwiseI16x8S)
VISIT_EXTADD_PAIRWISE(I32x4ExtAddPairwiseI16x8U)
#undef VISIT_EXTADD_PAIRWISE

void InstructionSelectorT::AddOutputToSelectContinuation(OperandGenerator* g,
                                                         int first_input_index,
                                                         OpIndex node) {
  UNREACHABLE();
}

// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags = MachineOperatorBuilder::kNoFlags;
  return flags | MachineOperatorBuilder::kWord32ShiftIsSafe |
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
  return MachineOperatorBuilder::AlignmentRequirements::
      FullUnalignedAccessSupport();
}

#undef SIMD_BINOP_LIST
#undef SIMD_SHIFT_OP_LIST
#undef SIMD_UNOP_LIST
#undef SIMD_TYPE_LIST
#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
