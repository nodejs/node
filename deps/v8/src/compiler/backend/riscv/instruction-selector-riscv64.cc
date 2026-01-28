// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/machine-type.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/backend/riscv/instruction-selector-riscv.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"

namespace v8 {
namespace internal {
namespace compiler {

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
    case kRiscvTst64:
    case kRiscvTst32:
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
  ExtendingLoadMatcher(OpIndex node, InstructionSelector* selector)
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
  InstructionSelector* selector_;
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
      RiscvOperandGenerator g(selector_);
      const LoadOp& load = lhs.Cast<LoadOp>();
      base_ = load.base();
      opcode_ = kRiscvLw;
      if (load.index().has_value()) {
        int64_t index_constant;
        if (selector_->MatchIntegralWord64Constant(load.index().value(),
                                                   &index_constant)) {
          DCHECK_EQ(load.element_size_log2, 0);
          immediate_ = index_constant + 4;
          matches_ = g.CanBeImmediate(immediate_, kRiscvLw);
        }
      } else {
        immediate_ = load.offset + 4;
        matches_ = g.CanBeImmediate(immediate_, kRiscvLw);
      }
    }
  }
};

bool TryEmitExtendingLoad(InstructionSelector* selector, OpIndex node,
                          OpIndex output_node) {
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

static InstructionCode GetShxaddCode(int64_t shift) {
  DCHECK(base::IsInRange(shift, 1, 3));

  switch (shift) {
    case 1:
      return kRiscvSh1add;
    case 2:
      return kRiscvSh2add;
    case 3:
      return kRiscvSh3add;
  }
  UNREACHABLE();
}

static void EmitInstrToFoldImpl(InstructionSelector* selector,
                                InstructionCode addr_opcode,
                                InstructionCode opcode, OpIndex addr_left_node,
                                OpIndex base, InstructionOperand output,
                                OpIndex value, InstructionOperand imm,
                                bool read) {
  RiscvOperandGenerator g(selector);

  InstructionOperand addr_reg = g.TempRegister();

  selector->Emit(addr_opcode | AddressingModeField::encode(kMode_None),
                 addr_reg, g.UseRegister(addr_left_node), g.UseRegister(base));
  if (read) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI), output,
                   addr_reg, imm);
  } else {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI), output,
                   g.UseRegisterOrImmediateZero(value), addr_reg, imm);
  }
}

static void EmitInstrToFold(InstructionSelector* selector,
                            InstructionCode addr_opcode, InstructionCode opcode,
                            OpIndex addr_left_node, OpIndex base,
                            InstructionOperand output, OpIndex value,
                            int32_t imm, bool read) {
  RiscvOperandGenerator g(selector);
  EmitInstrToFoldImpl(selector, addr_opcode, opcode, addr_left_node, base,
                      output, value, g.TempImmediate(imm), read);
}

static void EmitInstrToFold(InstructionSelector* selector,
                            InstructionCode addr_opcode, InstructionCode opcode,
                            OpIndex addr_left_node, OpIndex base,
                            InstructionOperand output, OpIndex value,
                            OpIndex index, bool read) {
  RiscvOperandGenerator g(selector);
  EmitInstrToFoldImpl(selector, addr_opcode, opcode, addr_left_node, base,
                      output, value, g.UseImmediate(index), read);
}

// TODO(zhijin): Refactoring is needed to make the function's implementation
// match its name. The bailout path in case 2 does not use the shxadd
// instruction.
static bool TryFoldLoadStoreImpl(InstructionSelector* selector, OpIndex node,
                                 InstructionCode opcode, OpIndex base,
                                 OpIndex index, InstructionOperand output,
                                 OpIndex value, bool read) {
  RiscvOperandGenerator g(selector);

  const Operation& base_op = selector->Get(base);
  const Operation& index_op = selector->Get(index);

  bool index_is_imm = g.CanBeImmediate(index, opcode);

  const WordBinopOp* binop = index_is_imm ? base_op.TryCast<WordBinopOp>()
                                          : index_op.TryCast<WordBinopOp>();

  if (binop && binop->kind == WordBinopOp::Kind::kAdd) {
    OpIndex bin_left_idx = binop->left();
    OpIndex bin_right_idx = binop->right();

    const Operation& bin_rhs_op = selector->Get(bin_right_idx);

    if (index_is_imm && CpuFeatures::IsSupported(ZBA) &&
        bin_rhs_op.Is<Opmask::kWord64ShiftLeft>()) {
      // Easiest case: use Shxadd without needing to reshuffle anything.
      // *((x << shift) + y + index) -> *((x << shift + y) + index).
      const ShiftOp& shift_op = bin_rhs_op.Cast<ShiftOp>();

      const Operation& shift_rhs = selector->Get(shift_op.right());

      if (!shift_rhs.Is<Opmask::kWord32Constant>()) {
        return false;
      }

      // The optimization is less beneficial if 'base' has additional uses.
      if (!selector->CanCover(base, bin_right_idx) ||
          !selector->CanCover(node, base)) {
        return false;
      }

      int64_t shift_by = shift_rhs.Cast<ConstantOp>().signed_integral();
      if (!base::IsInRange(shift_by, 1, 3)) {
        return false;
      }

      //
      // Case 1, fold the following instructions,
      //   slli t7, t7, 3
      //       bin_right = ShiftLeft(shift_op.left(), shift_by)
      //              t7 = ShiftLeft(t7, 3)
      //   add  t1, t1, t7
      //        bin_left = t1
      //            base = Add(bin_left, bin_right)
      //              t1 = Add(t1, t7)
      //   ld   t3, 8(t1)
      //           index = 8
      //          output = Load(base, index)
      //              t3 = Load(t1, 8)
      // into
      //   sh3add t1, t7, t1
      //        bin_left = t1
      //           base' = ShiftLeftAdd(shift_op.left(), bin_left)
      //              t1 = ShiftLeftAdd(t7, t1)
      //   ld   t3, 8(t1)
      //           index = 8
      //          output = Load(base', index)
      //              t3 = Load(t1, 8)
      //
      EmitInstrToFold(selector, GetShxaddCode(shift_by), opcode,
                      shift_op.left(), bin_left_idx, output, value, index,
                      read);
      return true;
    } else if (!index_is_imm && (bin_rhs_op.Is<Opmask::kWord32Constant>() ||
                                 bin_rhs_op.Is<Opmask::kWord64Constant>())) {
      // *(base + (x + imm)) -> *((x + base) + imm), or
      // *(base + ((y << shift) + imm)) -> *((y << shift + base) + imm).
      int64_t constant = bin_rhs_op.Cast<ConstantOp>().signed_integral();
      // The range of offset in load instruction is -2048~2047.
      if (!base::IsInRange(constant, -2048, 2047) ||
          !selector->CanCover(node, index)) {
        return false;
      }

      int32_t imm = static_cast<int32_t>(constant);

      const Operation& bin_lhs_op = selector->Get(bin_left_idx);

      if (!CpuFeatures::IsSupported(ZBA) ||
          !bin_lhs_op.Is<Opmask::kWord64ShiftLeft>()) {
        //
        // Case 2, fold the following instructions,
        //   addi t1, t7, 15
        //       bin_right = imm = 15
        //        bin_left = t7
        //           index = Addi(bin_left, bin_right)
        //              t1 = Addi(t7, 15)
        //   add  t1, t2, t1
        //            base = t2
        //            addr = Add(base, index)
        //              t1 = Add(t2, t1)
        //   ld   t3, 0(t1)
        //          output = Load(addr, 0)
        //              t3 = Load(t1, 0)
        // into
        //   add  t1, t7, t2
        //       bin_right = imm = 15
        //        bin_left = t7
        //            base = t2
        //           base' = Add(bin_left, base)
        //              t1 = Add(t7, t2)
        //   ld   t3, 15(t1)
        //          index' = bin_right = imm = 15
        //          output = Load(base', index')
        //             t3  = Load(t1, 15)
        //
        EmitInstrToFold(selector, kRiscvAdd64, opcode, bin_left_idx, base,
                        output, value, imm, read);
        return true;
      }

      const Operation& shift_rhs = selector->Get(bin_lhs_op.input(1));

      if (shift_rhs.Is<Opmask::kWord32Constant>() &&
          selector->CanCover(index, bin_left_idx)) {
        int64_t shift_by = shift_rhs.Cast<ConstantOp>().signed_integral();
        if (base::IsInRange(shift_by, 1, 3)) {
          //
          // Case 3, fold the following instructions,
          //   slli t7, t7, 3
          //       bin_left = ShiftLeft(bin_lhs_op.input(0), shift_by)
          //             t7 = ShiftLeft(t7, 3)
          //   addi t1, t7, 15
          //      bin_right = imm = 15
          //          index = Addi(bin_left, bin_right)
          //             t1 = Addi(t7, 15)
          //   add  t1, t2, t1
          //           base = t2
          //           addr = Add(base, index)
          //             t1 = Add(t2, t1)
          //   ld   t3, 0(t1)
          //         output = Load(addr, 0)
          //             t3 = Load(t1, 0)
          // into
          //   sh3add t1, t7, t2
          //           base = t2
          //          base' = ShiftLeftAdd(bin_lhs_op.input(0), base)
          //             t1 = ShiftLeftAdd(t7, t2)
          //   ld   t3, 15(t1)
          //         index' = bin_right = imm = 15
          //         output = Load(base', index')
          //             t3 = Load(t1, 15)
          //
          EmitInstrToFold(selector, GetShxaddCode(shift_by), opcode,
                          bin_lhs_op.input(0), base, output, value, imm, read);

          return true;
        }
      }
      // Like case 2.
      EmitInstrToFold(selector, kRiscvAdd64, opcode, bin_left_idx, base, output,
                      value, imm, read);
      return true;
    }
  } else if (!index_is_imm && index_op.Is<Opmask::kWord64ShiftLeft>() &&
             CpuFeatures::IsSupported(ZBA)) {
    // *(base + (x << shift)) -> *((x << shift + base) + 0).
    const ShiftOp& shift_op = index_op.Cast<ShiftOp>();

    const Operation& shift_rhs = selector->Get(shift_op.right());

    if (!shift_rhs.Is<Opmask::kWord32Constant>()) {
      return false;
    }

    if (!selector->CanCover(node, index)) {
      return false;
    }

    int64_t shift_by = shift_rhs.Cast<ConstantOp>().signed_integral();
    if (!base::IsInRange(shift_by, 1, 3)) {
      return false;
    }

    //
    // Case 4, fold the following instructions,
    //   slli t7, t7, 3
    //        index = ShiftLeft(shift_lhs, shift_by)
    //           t7 = ShiftLeft(t7, 3)
    //   add  t1, t1, t7
    //         base = t1
    //         addr = Add(base, index)
    //           t1 = Add(t1, t7)
    //   ld   t3, 0(t1)
    //       output = Load(addr, 0)
    //           t3 = Load(t1, 0)
    // into
    //   sh3add t1, t7, t1
    //         base = t1
    //         addr = ShiftLeftAdd(shift_lhs, base)
    //           t1 = ShiftLeftAdd(t7, t1)
    //   ld   t3, 0(t1)
    //       output = Load(addr, 0)
    //           t3 = Load(t1, 0)
    //
    EmitInstrToFold(selector, GetShxaddCode(shift_by), opcode, shift_op.left(),
                    base, output, value, 0, read);
    return true;
  }
  return false;
}

static bool TryFoldLoad(InstructionSelector* selector, OpIndex node,
                        InstructionCode opcode, OpIndex base, OpIndex index,
                        OpIndex output) {
  RiscvOperandGenerator g(selector);
  return TryFoldLoadStoreImpl(
      selector, node, opcode, base, index,
      g.DefineAsRegister(output.valid() ? output : node), OpIndex(), true);
}

static bool TryFoldStore(InstructionSelector* selector, OpIndex node,
                         InstructionCode opcode, OpIndex base, OpIndex index,
                         InstructionOperand output, OpIndex value) {
  return TryFoldLoadStoreImpl(selector, node, opcode, base, index, output,
                              value, false);
}

void EmitLoad(InstructionSelector* selector, OpIndex node,
              InstructionCode opcode, OpIndex output = OpIndex()) {
  RiscvOperandGenerator g(selector);

  const Operation& op = selector->Get(node);
  const LoadOp& load = op.Cast<LoadOp>();

  // The LoadStoreSimplificationReducer transforms all loads into
  // *(base + index).
  OpIndex base = load.base();
  OpIndex index = load.index().value();
  DCHECK_EQ(load.offset, 0);
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
    DCHECK(is_index_constant);
    input_count = 1;
    inputs[0] = g.UseImmediate64(index_constant);
    opcode |= AddressingModeField::encode(kMode_Root);
    selector->Emit(opcode, 1, &output_op, input_count, inputs);
    return;
  }

  if (TryFoldLoad(selector, node, opcode, base, index, output)) {
    return;
  }

  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(output.valid() ? output : node),
                   g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(output.valid() ? output : node), addr_reg,
                   g.TempImmediate(0));
  }
}

void EmitS128Load(InstructionSelector* selector, OpIndex node,
                  InstructionCode opcode) {
  RiscvOperandGenerator g(selector);
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 2);
  OpIndex base = op.input(0);
  OpIndex index = op.input(1);
  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(node), g.UseRegister(base),
                   g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

void InstructionSelector::VisitStoreLane(OpIndex node) {
  const Simd128LaneMemoryOp& store = Get(node).Cast<Simd128LaneMemoryOp>();
  InstructionCode opcode = kRiscvS128StoreLane;
  opcode |= EncodeElementWidth(ByteSizeToSew(store.lane_size()));
  if (store.kind.with_trap_handler) {
    opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
  }

  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 3);
  OpIndex base = op.input(0);
  OpIndex index = op.input(1);
  InstructionOperand addr_reg = g.TempRegister();
  if (g.CanBeImmediate(index, kRiscvAdd64)) {
    Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_MRR), addr_reg,
         g.UseRegister(base), g.UseImmediate(index));

  } else {
    Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_MRR), addr_reg,
         g.UseRegister(base), g.UseRegister(index));
  }
  opcode |= AddressingModeField::encode(kMode_MRI);
  InstructionOperand inputs[4] = {
      g.UseRegister(op.input(2)),
      g.UseImmediate(store.lane),
      addr_reg,
      g.TempImmediate(0),
  };
  Emit(opcode, 0, nullptr, 4, inputs);
}

void InstructionSelector::VisitLoadLane(OpIndex node) {
  const Simd128LaneMemoryOp& load = this->Get(node).Cast<Simd128LaneMemoryOp>();
  InstructionCode opcode = kRiscvS128LoadLane;
  opcode |= EncodeElementWidth(ByteSizeToSew(load.lane_size()));
  if (load.kind.with_trap_handler) {
    opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
  }

  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 3);
  OpIndex base = op.input(0);
  OpIndex index = op.input(1);
  InstructionOperand addr_reg = g.TempRegister();
  if (g.CanBeImmediate(index, kRiscvAdd64)) {
    Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_MRI), addr_reg,
         g.UseRegister(base), g.UseImmediate(index));
  } else {
    Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_MRR), addr_reg,
         g.UseRegister(base), g.UseRegister(index));
  }
  opcode |= AddressingModeField::encode(kMode_MRI);
  Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(op.input(2)),
       g.UseImmediate(load.lane), addr_reg, g.TempImmediate(0));
}

namespace {
ArchOpcode GetLoadOpcode(MemoryRepresentation loaded_rep,
                         RegisterRepresentation result_rep) {
  // NOTE: The meaning of `loaded_rep` = `MemoryRepresentation::AnyTagged()` is
  // we are loading a compressed tagged field, while `result_rep` =
  // `RegisterRepresentation::Tagged()` refers to an uncompressed tagged value.

  switch (loaded_rep) {
    case MemoryRepresentation::Int8():
      DCHECK_EQ(result_rep, RegisterRepresentation::Word32());
      return kRiscvLb;
    case MemoryRepresentation::Uint8():
      DCHECK_EQ(result_rep, RegisterRepresentation::Word32());
      return kRiscvLbu;
    case MemoryRepresentation::Int16():
      DCHECK_EQ(result_rep, RegisterRepresentation::Word32());
      return kRiscvLh;
    case MemoryRepresentation::Uint16():
      DCHECK_EQ(result_rep, RegisterRepresentation::Word32());
      return kRiscvLhu;
    case MemoryRepresentation::Int32():
      DCHECK_EQ(result_rep, RegisterRepresentation::Word32());
      return kRiscvLw;
    case MemoryRepresentation::Uint32():
      DCHECK_EQ(result_rep, RegisterRepresentation::Word32());
      return kRiscvLwu;
    case MemoryRepresentation::Int64():
    case MemoryRepresentation::Uint64():
      DCHECK_EQ(result_rep, RegisterRepresentation::Word64());
      return kRiscvLd;
    case MemoryRepresentation::Float16():
      UNIMPLEMENTED();
    case MemoryRepresentation::Float32():
      DCHECK_EQ(result_rep, RegisterRepresentation::Float32());
      return kRiscvLoadFloat;
    case MemoryRepresentation::Float64():
      DCHECK_EQ(result_rep, RegisterRepresentation::Float64());
      return kRiscvLoadDouble;
#ifdef V8_COMPRESS_POINTERS
    case MemoryRepresentation::AnyTagged():
    case MemoryRepresentation::TaggedPointer():
      if (result_rep == RegisterRepresentation::Compressed()) {
        return kRiscvLwu;
      }
      DCHECK_EQ(result_rep, RegisterRepresentation::Tagged());
      return kRiscvLoadDecompressTagged;
    case MemoryRepresentation::TaggedSigned():
      if (result_rep == RegisterRepresentation::Compressed()) {
        return kRiscvLwu;
      }
      DCHECK_EQ(result_rep, RegisterRepresentation::Tagged());
      return kRiscvLoadDecompressTaggedSigned;
#else
    case MemoryRepresentation::AnyTagged():
    case MemoryRepresentation::TaggedPointer():
    case MemoryRepresentation::TaggedSigned():
      DCHECK_EQ(result_rep, RegisterRepresentation::Tagged());
      return kRiscvLd;
#endif
    case MemoryRepresentation::AnyUncompressedTagged():
    case MemoryRepresentation::UncompressedTaggedPointer():
    case MemoryRepresentation::UncompressedTaggedSigned():
      DCHECK_EQ(result_rep, RegisterRepresentation::Tagged());
      return kRiscvLd;
    case MemoryRepresentation::ProtectedPointer():
      CHECK(V8_ENABLE_SANDBOX_BOOL);
      return kRiscvLoadDecompressProtected;
    case MemoryRepresentation::IndirectPointer():
      UNREACHABLE();
    case MemoryRepresentation::SandboxedPointer():
      return kRiscvLoadDecodeSandboxedPointer;
    case MemoryRepresentation::Simd128():
      return kRiscvRvvLd;
    case MemoryRepresentation::Simd256():
      UNREACHABLE();
  }
}

ArchOpcode GetStoreOpcode(MemoryRepresentation stored_rep) {
  switch (stored_rep) {
    case MemoryRepresentation::Int8():
    case MemoryRepresentation::Uint8():
      return kRiscvSb;
    case MemoryRepresentation::Int16():
    case MemoryRepresentation::Uint16():
      return kRiscvSh;
    case MemoryRepresentation::Int32():
    case MemoryRepresentation::Uint32():
      return kRiscvSw;
    case MemoryRepresentation::Int64():
    case MemoryRepresentation::Uint64():
      return kRiscvSd;
    case MemoryRepresentation::Float16():
      UNIMPLEMENTED();
    case MemoryRepresentation::Float32():
      return kRiscvStoreFloat;
    case MemoryRepresentation::Float64():
      return kRiscvStoreDouble;
    case MemoryRepresentation::AnyTagged():
    case MemoryRepresentation::TaggedPointer():
    case MemoryRepresentation::TaggedSigned():
      return kRiscvStoreCompressTagged;
    case MemoryRepresentation::AnyUncompressedTagged():
    case MemoryRepresentation::UncompressedTaggedPointer():
    case MemoryRepresentation::UncompressedTaggedSigned():
      return kRiscvSd;
    case MemoryRepresentation::ProtectedPointer():
      // We never store directly to protected pointers from generated code.
      UNREACHABLE();
    case MemoryRepresentation::IndirectPointer():
      return kRiscvStoreIndirectPointer;
    case MemoryRepresentation::SandboxedPointer():
      return kRiscvStoreEncodeSandboxedPointer;
    case MemoryRepresentation::Simd128():
      return kRiscvRvvSt;
    case MemoryRepresentation::Simd256():
      UNREACHABLE();
  }
}
}  // namespace

void InstructionSelector::VisitLoad(OpIndex node) {
  auto load = load_view(node);
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

void InstructionSelector::VisitStorePair(OpIndex node) { UNREACHABLE(); }

void InstructionSelector::VisitProtectedLoad(OpIndex node) { VisitLoad(node); }

void InstructionSelector::VisitStore(OpIndex node) {
  RiscvOperandGenerator g(this);
  StoreView store_view = this->store_view(node);
  DCHECK_EQ(store_view.displacement(), 0);
  OpIndex base = store_view.base();
  OpIndex index = store_view.index().value();
  OpIndex value = store_view.value();

  WriteBarrierKind write_barrier_kind =
      store_view.stored_rep().write_barrier_kind();
  const MachineRepresentation rep = store_view.stored_rep().representation();

  DCHECK_IMPLIES(write_barrier_kind == kSkippedWriteBarrier,
                 v8_flags.verify_write_barriers);

  // TODO(riscv): I guess this could be done in a better way.
  if (write_barrier_kind != kNoWriteBarrier &&
      V8_LIKELY(!v8_flags.disable_write_barriers)) {
    DCHECK(CanBeTaggedOrCompressedOrIndirectPointer(rep));
    InstructionOperand inputs[4];
    InstructionCode code;
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    inputs[input_count++] = g.CanBeImmediate(index, kRiscvAdd64)
                                ? g.UseImmediate(index)
                                : g.UseUniqueRegister(index);
    inputs[input_count++] = g.UseUniqueRegister(value);
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    size_t const temp_count = arraysize(temps);

    if (rep == MachineRepresentation::kIndirectPointer) {
      DCHECK(write_barrier_kind == kIndirectPointerWriteBarrier ||
             write_barrier_kind == kSkippedWriteBarrier);
      // In this case we need to add the IndirectPointerTag as additional input.
      code = write_barrier_kind == kSkippedWriteBarrier
                 ? kArchStoreIndirectSkippedWriteBarrier
                 : kArchStoreIndirectWithWriteBarrier;
      code |= RecordWriteModeField::encode(
          RecordWriteMode::kValueIsIndirectPointer);
      IndirectPointerTag tag = store_view.indirect_pointer_tag();
      inputs[input_count++] = g.UseImmediate64(static_cast<int64_t>(tag));
    } else if (write_barrier_kind == kSkippedWriteBarrier) {
      code = kArchStoreSkippedWriteBarrier;
    } else {
      RecordWriteMode record_write_mode =
          WriteBarrierKindToRecordWriteMode(write_barrier_kind);
      code = kArchStoreWithWriteBarrier;
      code |= RecordWriteModeField::encode(record_write_mode);
    }
    if (store_view.is_store_trap_on_null()) {
      code |= AccessModeField::encode(kMemoryAccessProtectedNullDereference);
    }
    Emit(code, 0, nullptr, input_count, inputs, temp_count, temps);
    return;
  }

  InstructionCode code = GetStoreOpcode(store_view.ts_stored_rep());

  if (code == kRiscvSw) {
    const Operation& value_op = this->Get(value);
    if (value_op.Is<Opmask::kTruncateWord64ToWord32>() &&
        CanCover(node, value_op.input(0))) {
      // If the value is a TruncateWord64ToWord32, we can use the input directly
      //  to store.
      value = value_op.input(0);
    }
  }
  if (Is<LoadRootRegisterOp>(base)) {
    Emit(code | AddressingModeField::encode(kMode_Root), g.NoOutput(),
         g.UseRegisterOrImmediateZero(value), g.UseImmediate(index));
    return;
  }

  if (store_view.is_store_trap_on_null()) {
    code |= AccessModeField::encode(kMemoryAccessProtectedNullDereference);
  } else if (store_view.access_kind() ==
             MemoryAccessKind::kProtectedByTrapHandler) {
    code |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
  }

  if (TryFoldStore(this, node, code, base, index, g.NoOutput(), value)) {
    return;
  }

  if (g.CanBeImmediate(index, code)) {
    Emit(code | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         g.UseRegisterOrImmediateZero(value), g.UseRegister(base),
         g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired store opcode, using temp addr_reg.
    Emit(code | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         g.UseRegisterOrImmediateZero(value), addr_reg, g.TempImmediate(0));
  }
}

void InstructionSelector::VisitProtectedStore(OpIndex node) {
  VisitStore(node);
}

void InstructionSelector::VisitWord32And(OpIndex node) {
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvAnd32, true, kRiscvAnd32);
}

void InstructionSelector::VisitWord64And(OpIndex node) {
  VisitBinop<Int64BinopMatcher>(this, node, kRiscvAnd, true, kRiscvAnd);
}

void InstructionSelector::VisitWord32Or(OpIndex node) {
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvOr32, true, kRiscvOr32);
}

void InstructionSelector::VisitWord64Or(OpIndex node) {
  VisitBinop<Int64BinopMatcher>(this, node, kRiscvOr, true, kRiscvOr);
}

void InstructionSelector::VisitWord32Xor(OpIndex node) {
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvXor32, true, kRiscvXor32);
}

void InstructionSelector::VisitWord64Xor(OpIndex node) {
  VisitBinop<Int64BinopMatcher>(this, node, kRiscvXor, true, kRiscvXor);
}

void InstructionSelector::VisitWord64Shl(OpIndex node) {
  const ShiftOp& shift_op = this->Get(node).template Cast<ShiftOp>();
  const Operation& lhs = this->Get(shift_op.left());
  const Operation& rhs = this->Get(shift_op.right());
  if ((lhs.Is<Opmask::kChangeInt32ToInt64>() ||
       lhs.Is<Opmask::kChangeUint32ToUint64>()) &&
      rhs.Is<Opmask::kWord32Constant>()) {
    int64_t shift_by = rhs.Cast<ConstantOp>().signed_integral();
    if (base::IsInRange(shift_by, 32, 63) && CanCover(node, shift_op.left())) {
      RiscvOperandGenerator g(this);
      // There's no need to sign/zero-extend to 64-bit if we shift out the
      // upper 32 bits anyway.
      Emit(kRiscvShl64, g.DefineSameAsFirst(node),
           g.UseRegister(lhs.Cast<ChangeOp>().input()),
           g.UseImmediate64(shift_by));
      return;
    }
  }
  VisitRRO(this, kRiscvShl64, node);
}

void InstructionSelector::VisitWord64Shr(OpIndex node) {
  VisitRRO(this, kRiscvShr64, node);
}

void InstructionSelector::VisitWord64Sar(OpIndex node) {
  if (TryEmitExtendingLoad(this, node, node)) return;
  // Select Sbfx(x, imm, 32-imm) for Word64Sar(ChangeInt32ToInt64(x), imm)
  // where possible
  const ShiftOp& shiftop = Get(node).Cast<ShiftOp>();
  const Operation& lhs = Get(shiftop.left());

  int64_t constant_rhs;
  if (lhs.Is<Opmask::kChangeInt32ToInt64>() &&
      MatchIntegralWord64Constant(shiftop.right(), &constant_rhs) &&
      is_uint5(constant_rhs) && CanCover(node, shiftop.left())) {
    // Don't select Sbfx here if Asr(Ldrsw(x), imm) can be selected for
    // Word64Sar(ChangeInt32ToInt64(Load(x)), imm)
    OpIndex input = lhs.Cast<ChangeOp>().input();
    if (!Get(input).Is<LoadOp>() || !CanCover(shiftop.left(), input)) {
      RiscvOperandGenerator g(this);
      int right = static_cast<int>(constant_rhs);
      Emit(kRiscvSar32, g.DefineAsRegister(node), g.UseRegister(input),
           g.UseImmediate(right));
      return;
    }
  }
  VisitRRO(this, kRiscvSar64, node);
}

void InstructionSelector::VisitWord32Rol(OpIndex node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitWord64Rol(OpIndex node) { UNREACHABLE(); }

void InstructionSelector::VisitWord32Ror(OpIndex node) {
  VisitRRO(this, kRiscvRor32, node);
}

void InstructionSelector::VisitWord32ReverseBits(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelector::VisitWord64ReverseBits(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelector::VisitWord64ReverseBytes(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  if (CpuFeatures::IsSupported(ZBB)) {
    Emit(kRiscvRev8, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
  } else {
    Emit(kRiscvByteSwap64, g.DefineAsRegister(node),
         g.UseRegister(op.input(0)));
  }
}

void InstructionSelector::VisitWord32ReverseBytes(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  if (CpuFeatures::IsSupported(ZBB)) {
    InstructionOperand temp = g.TempRegister();
    Emit(kRiscvRev8, temp, g.UseRegister(op.input(0)));
    Emit(kRiscvShr64, g.DefineAsRegister(node), temp, g.TempImmediate(32));
  } else {
    Emit(kRiscvByteSwap32, g.DefineAsRegister(node),
         g.UseRegister(op.input(0)));
  }
}

void InstructionSelector::VisitSimd128ReverseBytes(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelector::VisitWord32Ctz(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvCtzw, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitWord64Ctz(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvCtz, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitWord32Popcnt(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvCpopw, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitWord64Popcnt(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvCpop, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitWord64Ror(OpIndex node) {
  VisitRRO(this, kRiscvRor64, node);
}

void InstructionSelector::VisitWord64Clz(OpIndex node) {
  VisitRR(this, kRiscvClz64, node);
}

void InstructionSelector::VisitInt32Add(OpIndex node) {
  const WordBinopOp& add = Cast<WordBinopOp>(node);
  OpIndex left = add.left();
  OpIndex right = add.right();
  const Operation& left_op = this->Get(left);
  const Operation& right_op = this->Get(right);
  InstructionOperand inputs[2];
  RiscvOperandGenerator g(this);
  if (left_op.Is<Opmask::kTruncateWord64ToWord32>() &&
      CanCover(node, left_op.input(0))) {
    inputs[0] = g.UseRegister(left_op.input(0));
  } else {
    inputs[0] = g.UseRegister(left);
  }
  if (right_op.Is<Opmask::kTruncateWord64ToWord32>() &&
      CanCover(node, right_op.input(0))) {
    inputs[1] = g.UseRegister(right_op.input(0));
  } else {
    inputs[1] = g.UseOperand(right, kRiscvAdd32);
  }
  Emit(kRiscvAdd32, g.DefineAsRegister(node), inputs[0], inputs[1]);
}

// Try to match Add(z, a, slli(x, y)) and emit shxadd(z, x, a) for it.
bool TryEmitShxadd(InstructionSelector* selector, OpIndex add, OpIndex lhs,
                   OpIndex rhs) {
  if (CpuFeatures::IsSupported(ZBA)) return false;
  const Operation& left_op = selector->Get(lhs);
  if ((left_op.Is<Opmask::kWord64ShiftLeft>() &&
       selector->CanCover(add, lhs))) {
    const ShiftOp& shift_op = left_op.Cast<ShiftOp>();
    const Operation& shift_rhs = selector->Get(shift_op.right());
    if (shift_rhs.Is<Opmask::kWord32Constant>()) {
      int64_t shift_by = shift_rhs.Cast<ConstantOp>().signed_integral();
      if (base::IsInRange(shift_by, 1, 3)) {
        RiscvOperandGenerator g(selector);
        selector->Emit(
            GetShxaddCode(shift_by) | AddressingModeField::encode(kMode_None),
            g.UseRegister(add), g.UseRegister(shift_op.left()),
            g.UseRegister(rhs));
        return true;
      }
    }
  }
  return false;
}

void InstructionSelector::VisitInt64Add(OpIndex node) {
  const WordBinopOp& add = this->Get(node).Cast<WordBinopOp>();
  DCHECK(add.Is<Opmask::kWord64Add>());
  OpIndex right = add.right();
  OpIndex left = add.left();
  if (TryEmitShxadd(this, node, left, right) ||
      TryEmitShxadd(this, node, right, left)) {
    return;
  }
  VisitBinop<Int64BinopMatcher>(this, node, kRiscvAdd64, true, kRiscvAdd64);
}

void InstructionSelector::VisitInt32Sub(OpIndex node) {
  const WordBinopOp& add = Cast<WordBinopOp>(node);
  OpIndex left = add.left();
  OpIndex right = add.right();
  const Operation& left_op = this->Get(left);
  const Operation& right_op = this->Get(right);
  InstructionOperand inputs[2];
  RiscvOperandGenerator g(this);
  if (left_op.Is<Opmask::kTruncateWord64ToWord32>() &&
      CanCover(node, left_op.input(0))) {
    inputs[0] = g.UseRegister(left_op.input(0));
  } else {
    inputs[0] = g.UseRegister(left);
  }
  if (right_op.Is<Opmask::kTruncateWord64ToWord32>() &&
      CanCover(node, right_op.input(0))) {
    inputs[1] = g.UseRegister(right_op.input(0));
  } else {
    inputs[1] = g.UseOperand(right, kRiscvSub32);
  }
  Emit(kRiscvSub32, g.DefineAsRegister(node), inputs[0], inputs[1]);
}

void InstructionSelector::VisitInt64Sub(OpIndex node) {
  VisitBinop<Int64BinopMatcher>(this, node, kRiscvSub64);
}

void InstructionSelector::VisitInt32Mul(OpIndex node) {
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  OpIndex left = op.input(0);
  OpIndex right = op.input(1);
  if (CanCover(node, left) && CanCover(node, right)) {
    const Operation& left_op = this->Get(left);
    const Operation& right_op = this->Get(right);
    if (left_op.Is<Opmask::kWord64ShiftRightLogical>() &&
        right_op.Is<Opmask::kWord64ShiftRightLogical>()) {
      RiscvOperandGenerator g(this);
      int64_t constant_left;
      MatchSignedIntegralConstant(left_op.input(1), &constant_left);
      int64_t constant_right;
      MatchSignedIntegralConstant(right_op.input(1), &constant_right);
      if (constant_right == 32 && constant_right == 32) {
        // Combine untagging shifts with Dmul high.
        Emit(kRiscvMulHigh64, g.DefineSameAsFirst(node),
             g.UseRegister(left_op.input(0)), g.UseRegister(right_op.input(0)));
        return;
      }
    }
  }
  VisitRRR(this, kRiscvMul32, node);
}

void InstructionSelector::VisitInt32MulHigh(OpIndex node) {
  VisitRRR(this, kRiscvMulHigh32, node);
}

void InstructionSelector::VisitInt64MulHigh(OpIndex node) {
  return VisitRRR(this, kRiscvMulHigh64, node);
}

void InstructionSelector::VisitUint32MulHigh(OpIndex node) {
  VisitRRR(this, kRiscvMulHighU32, node);
}

void InstructionSelector::VisitUint64MulHigh(OpIndex node) {
  VisitRRR(this, kRiscvMulHighU64, node);
}

void InstructionSelector::VisitInt64Mul(OpIndex node) {
  VisitRRR(this, kRiscvMul64, node);
}

void InstructionSelector::VisitInt32Div(OpIndex node) {
  VisitRRR(this, kRiscvDiv32, node,
           OperandGenerator::RegisterUseKind::kUseUniqueRegister);
}

void InstructionSelector::VisitUint32Div(OpIndex node) {
  VisitRRR(this, kRiscvDivU32, node,
           OperandGenerator::RegisterUseKind::kUseUniqueRegister);
}

void InstructionSelector::VisitInt32Mod(OpIndex node) {
  VisitRRR(this, kRiscvMod32, node);
}

void InstructionSelector::VisitUint32Mod(OpIndex node) {
  VisitRRR(this, kRiscvModU32, node);
}

void InstructionSelector::VisitInt64Div(OpIndex node) {
  VisitRRR(this, kRiscvDiv64, node,
           OperandGenerator::RegisterUseKind::kUseUniqueRegister);
}

void InstructionSelector::VisitUint64Div(OpIndex node) {
  VisitRRR(this, kRiscvDivU64, node,
           OperandGenerator::RegisterUseKind::kUseUniqueRegister);
}

void InstructionSelector::VisitInt64Mod(OpIndex node) {
  VisitRRR(this, kRiscvMod64, node);
}

void InstructionSelector::VisitUint64Mod(OpIndex node) {
  VisitRRR(this, kRiscvModU64, node);
}

void InstructionSelector::VisitChangeFloat32ToFloat64(OpIndex node) {
  VisitRR(this, kRiscvCvtDS, node);
}

void InstructionSelector::VisitRoundInt32ToFloat32(OpIndex node) {
  VisitRR(this, kRiscvCvtSW, node);
}

void InstructionSelector::VisitRoundUint32ToFloat32(OpIndex node) {
  VisitRR(this, kRiscvCvtSUw, node);
}

void InstructionSelector::VisitChangeInt32ToFloat64(OpIndex node) {
  VisitRR(this, kRiscvCvtDW, node);
}

void InstructionSelector::VisitChangeInt64ToFloat64(OpIndex node) {
  VisitRR(this, kRiscvCvtDL, node);
}

void InstructionSelector::VisitChangeUint32ToFloat64(OpIndex node) {
  VisitRR(this, kRiscvCvtDUw, node);
}

void InstructionSelector::VisitTruncateFloat32ToInt32(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  InstructionCode opcode = kRiscvTruncWS;
  opcode |=
      MiscField::encode(op.Is<Opmask::kTruncateFloat32ToInt32OverflowToMin>());
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitTruncateFloat32ToUint32(OpIndex node) {
  RiscvOperandGenerator g(this);

  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  InstructionCode opcode = kRiscvTruncUwS;
  if (op.Is<Opmask::kTruncateFloat32ToUint32OverflowToMin>()) {
    opcode |= MiscField::encode(true);
  }

  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitChangeFloat64ToInt32(OpIndex node) {
  RiscvOperandGenerator g(this);
  auto value = this->Get(node).input(0);
  if (CanCover(node, value)) {
    const Operation& op = this->Get(value);
    if (const FloatUnaryOp* load = op.TryCast<FloatUnaryOp>()) {
      DCHECK(load->rep == FloatRepresentation::Float64());
      switch (load->kind) {
        case FloatUnaryOp::Kind::kRoundDown:
          Emit(kRiscvFloorWD, g.DefineAsRegister(node),
               g.UseRegister(op.input(0)));
          return;
        case FloatUnaryOp::Kind::kRoundUp:
          Emit(kRiscvCeilWD, g.DefineAsRegister(node),
               g.UseRegister(op.input(0)));
          return;
        case FloatUnaryOp::Kind::kRoundToZero:
          Emit(kRiscvTruncWD, g.DefineAsRegister(node),
               g.UseRegister(op.input(0)));
          return;
        case FloatUnaryOp::Kind::kRoundTiesEven:
          Emit(kRiscvRoundWD, g.DefineAsRegister(node),
               g.UseRegister(op.input(0)));
          return;
        default:
          break;
      }
    }
    if (op.Is<ChangeOp>()) {
      const ChangeOp& change = op.Cast<ChangeOp>();
      using Rep = turboshaft::RegisterRepresentation;
      if (change.from == Rep::Float32() && change.to == Rep::Float64()) {
        auto next = this->Get(value).input(0);
        if (CanCover(value, next)) {
          const Operation& next_op = this->Get(next);
          if (const FloatUnaryOp* round = next_op.TryCast<FloatUnaryOp>()) {
            DCHECK(round->rep == FloatRepresentation::Float32());
            switch (round->kind) {
              case FloatUnaryOp::Kind::kRoundDown:
                Emit(kRiscvFloorWS, g.DefineAsRegister(node),
                     g.UseRegister(next_op.input(0)));
                return;
              case FloatUnaryOp::Kind::kRoundUp:
                Emit(kRiscvCeilWS, g.DefineAsRegister(node),
                     g.UseRegister(next_op.input(0)));
                return;
              case FloatUnaryOp::Kind::kRoundToZero:
                Emit(kRiscvTruncWS, g.DefineAsRegister(node),
                     g.UseRegister(next_op.input(0)));
                return;
              case FloatUnaryOp::Kind::kRoundTiesEven:
                Emit(kRiscvRoundWS, g.DefineAsRegister(node),
                     g.UseRegister(next_op.input(0)));
                return;
              default:
                break;
            }
          }
        }
        // Match float32 -> float64 -> int32 representation change path.
        Emit(kRiscvTruncWS, g.DefineAsRegister(node),
             g.UseRegister(this->Get(value).input(0)));
        return;
      }
    }
  }
  VisitRR(this, kRiscvTruncWD, node);
}

void InstructionSelector::VisitTryTruncateFloat64ToInt32(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  InstructionOperand inputs[] = {g.UseRegister(op.input(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kRiscvTruncWD, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitTryTruncateFloat64ToUint32(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  InstructionOperand inputs[] = {g.UseRegister(op.input(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kRiscvTruncUwD, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitChangeFloat64ToInt64(OpIndex node) {
  VisitRR(this, kRiscvTruncLD, node);
}

void InstructionSelector::VisitChangeFloat64ToUint32(OpIndex node) {
  VisitRR(this, kRiscvTruncUwD, node);
}

void InstructionSelector::VisitChangeFloat64ToUint64(OpIndex node) {
  VisitRR(this, kRiscvTruncUlD, node);
}

void InstructionSelector::VisitTruncateFloat64ToUint32(OpIndex node) {
  VisitRR(this, kRiscvTruncUwD, node);
}

void InstructionSelector::VisitTruncateFloat64ToInt64(OpIndex node) {
  RiscvOperandGenerator g(this);

  InstructionCode opcode = kRiscvTruncLD;
  const Operation& op = this->Get(node);
  if (op.Is<Opmask::kTruncateFloat64ToInt64OverflowToMin>()) {
    opcode |= MiscField::encode(true);
  }

  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitTryTruncateFloat32ToInt64(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  InstructionOperand inputs[] = {g.UseRegister(op.input(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kRiscvTruncLS, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitTryTruncateFloat64ToInt64(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  InstructionOperand inputs[] = {g.UseRegister(op.input(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kRiscvTruncLD, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitTryTruncateFloat32ToUint64(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  InstructionOperand inputs[] = {g.UseRegister(op.input(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kRiscvTruncUlS, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitTryTruncateFloat64ToUint64(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  InstructionOperand inputs[] = {g.UseRegister(op.input(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  OptionalOpIndex success_output = FindProjection(node, 1);
  if (success_output.valid()) {
    outputs[output_count++] = g.DefineAsRegister(success_output.value());
  }

  Emit(kRiscvTruncUlD, output_count, outputs, 1, inputs);
}

void InstructionSelector::VisitBitcastWord32ToWord64(OpIndex node) {
  DCHECK(SmiValuesAre31Bits());
  DCHECK(COMPRESS_POINTERS_BOOL);
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvZeroExtendWord, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)));
}

void EmitSignExtendWord(InstructionSelector* selector, OpIndex node) {
  RiscvOperandGenerator g(selector);
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 1);
  OpIndex value = op.input(0);
  const Operation& input_op = selector->Get(value);
  if (input_op.Is<Opmask::kTruncateWord64ToWord32>() &&
      selector->CanCover(node, value)) {
    // Match these patterns:
    // Change(#395)[Truncate, NoAssumption, Word64, Word32]
    // Change(#396)[SignExtend, NoAssumption, Word32, Word64]
    RiscvOperandGenerator g(selector);
    selector->Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(value));
    return;
  }
  if ((input_op.Is<Opmask::kWord64Add>() ||
       input_op.Is<Opmask::kWord64Sub>())) {
    if (selector->CanCover(node, value)) {
      const WordBinopOp& binop = input_op.Cast<WordBinopOp>();
      OpIndex left = binop.left();
      OpIndex right = binop.right();
      InstructionCode opcode =
          input_op.Is<Opmask::kWord64Add>() ? kRiscvAdd32 : kRiscvSub32;
      selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(left),
                     g.UseOperand(right, opcode));
      return;
    }
  }
  selector->Emit(kRiscvSignExtendWord, g.DefineAsRegister(node),
                 g.UseRegister(value));
}

bool IsSignExtendWord32ToWord64(const Operation& op) {
  if (op.Is<Opmask::kWord32Add>() || op.Is<Opmask::kWord32Sub>() ||
      op.Is<Opmask::kWord32ShiftLeft>() ||
      op.Is<Opmask::kWord32ShiftRightArithmetic>() ||
      op.Is<Opmask::kWord32ShiftRightArithmeticShiftOutZeros>() ||
      op.Is<Opmask::kWord32ShiftRightLogical>() ||
      op.Is<Opmask::kChangeInt32ToInt64>()) {
    return true;
  }
  return false;
}

void InstructionSelector::VisitChangeInt32ToInt64(OpIndex node) {
  const ChangeOp& change_op = this->Get(node).template Cast<ChangeOp>();
  const Operation& input_op = this->Get(change_op.input());
  if (input_op.Is<LoadOp>() && CanCover(node, change_op.input())) {
    // Generate sign-extending load.
    LoadRepresentation load_rep = load_view(change_op.input()).loaded_rep();
    MachineRepresentation rep = load_rep.representation();
    InstructionCode opcode = kArchNop;
    switch (rep) {
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = load_rep.IsUnsigned() ? kRiscvLbu : kRiscvLb;
        break;
      case MachineRepresentation::kWord16:
        opcode = load_rep.IsUnsigned() ? kRiscvLhu : kRiscvLh;
        break;
      case MachineRepresentation::kWord32:
      case MachineRepresentation::kWord64:
        // Since BitcastElider may remove nodes of
        // IrOpcode::kTruncateInt64ToInt32 and directly use the inputs, values
        // with kWord64 can also reach this line.
      case MachineRepresentation::kTaggedSigned:
      case MachineRepresentation::kTaggedPointer:
      case MachineRepresentation::kTagged:
        opcode = kRiscvLw;
        break;
      default:
        UNREACHABLE();
    }
    EmitLoad(this, change_op.input(), opcode, node);
    return;
  }
  if (IsSignExtendWord32ToWord64(input_op) &&
      CanCover(node, change_op.input())) {
    RiscvOperandGenerator g(this);
    Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(change_op.input()));
    return;
  }
  EmitSignExtendWord(this, node);
}

void InstructionSelector::VisitChangeUint32ToUint64(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  OpIndex value = op.input(0);
  if (ZeroExtendsWord32ToWord64(value)) {
    Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(value));
    return;
  }
  Emit(kRiscvZeroExtendWord, g.DefineAsRegister(node), g.UseRegister(value));
}

bool InstructionSelector::ZeroExtendsWord32ToWord64NoPhis(OpIndex node) {
  DCHECK(!this->Get(node).Is<PhiOp>());
  const Operation& op = this->Get(node);
  if (op.opcode == Opcode::kLoad) {
    auto load = this->load_view(node);
    LoadRepresentation load_rep = load.loaded_rep();
    if (load_rep.IsUnsigned()) {
      switch (load_rep.representation()) {
        case MachineRepresentation::kWord8:
        case MachineRepresentation::kWord16:
        case MachineRepresentation::kWord32:
          return true;
        default:
          return false;
      }
    }
  }
  // All other 32-bit operations sign-extend to the upper 32 bits
  return false;
}

void InstructionSelector::VisitTruncateInt64ToInt32(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  auto value = op.input(0);
  if (CanCover(node, value)) {
    const Operation& input_op = Get(value);
    if (input_op.Is<Opmask::kWord64ShiftRightArithmetic>()) {
      auto shift_value = input_op.input(1);
      if (CanCover(value, input_op.input(0)) &&
          TryEmitExtendingLoad(this, value, node)) {
        return;
      } else if (int64_t constant;
                 MatchSignedIntegralConstant(shift_value, &constant)) {
        if (constant <= 63 && constant >= 32) {
          // After smi untagging no need for truncate. Combine sequence.
          Emit(kRiscvSar64, g.DefineSameAsFirst(node),
               g.UseRegister(input_op.input(0)), g.UseImmediate64(constant));
          return;
        }
      }
    }
    if ((input_op.Is<Opmask::kWord64Add>() ||
         input_op.Is<Opmask::kWord64Sub>())) {
      const WordBinopOp& binop = input_op.Cast<WordBinopOp>();
      OpIndex left = binop.left();
      OpIndex right = binop.right();
      InstructionCode opcode =
          input_op.Is<Opmask::kWord64Add>() ? kRiscvAdd32 : kRiscvSub32;
      Emit(opcode, g.DefineAsRegister(node), g.UseRegister(left),
           g.UseOperand(right, opcode));
      return;
    }
  }
  // Semantics of this machine IR is not clear. For example, x86 zero-extend
  // the truncated value; arm treats it as nop thus the upper 32-bit as
  // undefined; Riscv emits ext instruction which zero-extend the 32-bit
  // value; for riscv, we do sign-extension of the truncated value
  Emit(kRiscvSignExtendWord, g.DefineAsRegister(node), g.UseRegister(value),
       g.TempImmediate(0));
}

void InstructionSelector::VisitRoundInt64ToFloat32(OpIndex node) {
  VisitRR(this, kRiscvCvtSL, node);
}

void InstructionSelector::VisitRoundInt64ToFloat64(OpIndex node) {
  VisitRR(this, kRiscvCvtDL, node);
}

void InstructionSelector::VisitRoundUint64ToFloat32(OpIndex node) {
  VisitRR(this, kRiscvCvtSUl, node);
}

void InstructionSelector::VisitRoundUint64ToFloat64(OpIndex node) {
  VisitRR(this, kRiscvCvtDUl, node);
}

void InstructionSelector::VisitBitcastFloat32ToInt32(OpIndex node) {
  VisitRR(this, kRiscvBitcastFloat32ToInt32, node);
}

void InstructionSelector::VisitBitcastFloat64ToInt64(OpIndex node) {
  VisitRR(this, kRiscvBitcastDL, node);
}

void InstructionSelector::VisitBitcastInt32ToFloat32(OpIndex node) {
  VisitRR(this, kRiscvBitcastInt32ToFloat32, node);
}

void InstructionSelector::VisitBitcastInt64ToFloat64(OpIndex node) {
  VisitRR(this, kRiscvBitcastLD, node);
}

void InstructionSelector::VisitFloat64RoundDown(OpIndex node) {
  VisitRR(this, kRiscvFloat64RoundDown, node);
}

void InstructionSelector::VisitFloat32RoundUp(OpIndex node) {
  VisitRR(this, kRiscvFloat32RoundUp, node);
}

void InstructionSelector::VisitFloat64RoundUp(OpIndex node) {
  VisitRR(this, kRiscvFloat64RoundUp, node);
}

void InstructionSelector::VisitFloat32RoundTruncate(OpIndex node) {
  VisitRR(this, kRiscvFloat32RoundTruncate, node);
}

void InstructionSelector::VisitFloat64RoundTruncate(OpIndex node) {
  VisitRR(this, kRiscvFloat64RoundTruncate, node);
}

void InstructionSelector::VisitFloat64RoundTiesAway(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelector::VisitFloat32RoundTiesEven(OpIndex node) {
  VisitRR(this, kRiscvFloat32RoundTiesEven, node);
}

void InstructionSelector::VisitFloat64RoundTiesEven(OpIndex node) {
  VisitRR(this, kRiscvFloat64RoundTiesEven, node);
}

void InstructionSelector::VisitFloat32Neg(OpIndex node) {
  VisitRR(this, kRiscvNegS, node);
}

void InstructionSelector::VisitFloat64Neg(OpIndex node) {
  VisitRR(this, kRiscvNegD, node);
}

void InstructionSelector::VisitFloat64Ieee754Binop(OpIndex node,
                                                   InstructionCode opcode) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 2);
  Emit(opcode, g.DefineAsFixed(node, fa0), g.UseFixed(op.input(0), fa0),
       g.UseFixed(op.input(1), fa1))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat64Ieee754Unop(OpIndex node,
                                                  InstructionCode opcode) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(opcode, g.DefineAsFixed(node, fa0), g.UseFixed(op.input(0), fa1))
      ->MarkAsCall();
}

void InstructionSelector::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* call_descriptor,
    OpIndex node) {
  RiscvOperandGenerator g(this);

  // Prepare for C function call.
  if (call_descriptor->IsCFunctionCall()) {
    int gp_param_count = static_cast<int>(call_descriptor->GPParameterCount());
    int fp_param_count = static_cast<int>(call_descriptor->FPParameterCount());
    Emit(kArchPrepareCallCFunction | ParamField::encode(gp_param_count) |
             FPParamField::encode(fp_param_count),
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
        if (input.node.valid()) {
          stack_size += input.location.GetSizeInPointers();
        }
      }
      Emit(kRiscvStackClaim, g.NoOutput(),
           g.TempImmediate(stack_size << kSystemPointerSizeLog2));
    }
    for (size_t n = 0; n < arguments->size(); ++n) {
      PushParameter input = (*arguments)[n];
      if (input.node.valid()) {
        Emit(kRiscvStoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
             g.TempImmediate(static_cast<int>(n << kSystemPointerSizeLog2)));
      }
    }
  }
}

void InstructionSelector::VisitUnalignedLoad(OpIndex node) {
  auto load = this->load_view(node);
  LoadRepresentation load_rep = load.loaded_rep();
  RiscvOperandGenerator g(this);
  OpIndex base = load.base();
  OpIndex index = load.index();

  InstructionCode opcode = kArchNop;
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
    case MachineRepresentation::kIndirectPointer:    // Fall through.
    case MachineRepresentation::kProtectedPointer:   // Fall through.
    case MachineRepresentation::kFloat16:            // Fall through.
    case MachineRepresentation::kFloat16RawBits:     // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
  }
  bool traps_on_null;
  if (load.is_protected(&traps_on_null)) {
    if (traps_on_null) {
      opcode |= AccessModeField::encode(kMemoryAccessProtectedNullDereference);
    } else {
      opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
    }
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

void InstructionSelector::VisitUnalignedStore(OpIndex node) {
  RiscvOperandGenerator g(this);
  auto store_view = this->store_view(node);
  DCHECK_EQ(store_view.displacement(), 0);
  OpIndex base = store_view.base();
  OpIndex index = store_view.index().value();
  OpIndex value = store_view.value();

  MachineRepresentation rep = store_view.stored_rep().representation();

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
    case MachineRepresentation::kIndirectPointer:    // Fall through.
    case MachineRepresentation::kProtectedPointer:   // Fall through.
    case MachineRepresentation::kFloat16:            // Fall through.
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
    Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired store opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         addr_reg, g.TempImmediate(0), g.UseRegisterOrImmediateZero(value));
  }
}

namespace {

bool IsNodeUnsigned(InstructionSelector* selector, OpIndex n) {
  const Operation& op = selector->Get(n);
  if (op.Is<LoadOp>()) {
    const LoadOp& load = op.Cast<LoadOp>();
    return load.machine_type().IsUnsigned() ||
           load.machine_type().IsCompressed();
  } else if (op.Is<WordBinopOp>()) {
    const WordBinopOp& binop = op.Cast<WordBinopOp>();
    switch (binop.kind) {
      case WordBinopOp::Kind::kUnsignedDiv:
      case WordBinopOp::Kind::kUnsignedMod:
      case WordBinopOp::Kind::kUnsignedMulOverflownBits:
        return true;
      default:
        return false;
    }
  } else if (op.Is<ChangeOrDeoptOp>()) {
    const ChangeOrDeoptOp& change = op.Cast<ChangeOrDeoptOp>();
    return change.kind == ChangeOrDeoptOp::Kind::kFloat64ToUint32;
  } else if (op.Is<ConvertJSPrimitiveToUntaggedOp>()) {
    const ConvertJSPrimitiveToUntaggedOp& convert =
        op.Cast<ConvertJSPrimitiveToUntaggedOp>();
    return convert.kind ==
           ConvertJSPrimitiveToUntaggedOp::UntaggedKind::kUint32;
  } else if (op.Is<ConstantOp>()) {
    const ConstantOp& constant = op.Cast<ConstantOp>();
    return constant.kind == ConstantOp::Kind::kCompressedHeapObject;
  } else {
    return false;
  }
}

bool CanUseOptimizedWord32Compare(InstructionSelector* selector, OpIndex node) {
  if (COMPRESS_POINTERS_BOOL) {
    return false;
  }
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 2);
  if (IsNodeUnsigned(selector, op.input(0)) ==
      IsNodeUnsigned(selector, op.input(1))) {
    return true;
  }
  return false;
}

// Shared routine for multiple word compare operations.

void VisitFullWord32Compare(InstructionSelector* selector, OpIndex node,
                            InstructionCode opcode, FlagsContinuation* cont) {
  RiscvOperandGenerator g(selector);
  InstructionOperand leftOp = g.TempRegister();
  InstructionOperand rightOp = g.TempRegister();
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 2);
  selector->Emit(kRiscvShl64, leftOp, g.UseRegister(op.input(0)),
                 g.TempImmediate(32));
  selector->Emit(kRiscvShl64, rightOp, g.UseRegister(op.input(1)),
                 g.TempImmediate(32));

  Instruction* instr = VisitCompare(selector, opcode, leftOp, rightOp, cont);
  selector->UpdateSourcePosition(instr, node);
}

void VisitOptimizedWord32Compare(InstructionSelector* selector, OpIndex node,
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

    const Operation& op = selector->Get(node);
    DCHECK_EQ(op.input_count, 2);
    selector->Emit(testOpcode, optimizedResult, g.UseRegister(op.input(0)),
                   g.UseRegister(op.input(1)));
    selector->Emit(kRiscvShl64, leftOp, g.UseRegister(op.input(0)),
                   g.TempImmediate(32));
    selector->Emit(kRiscvShl64, rightOp, g.UseRegister(op.input(1)),
                   g.TempImmediate(32));
    selector->Emit(testOpcode, fullResult, leftOp, rightOp);

    selector->Emit(kRiscvAssertEqual, g.NoOutput(), optimizedResult, fullResult,
                   g.TempImmediate(static_cast<int>(
                       AbortReason::kUnsupportedNonPrimitiveCompare)));
  }

  Instruction* instr = VisitWordCompare(selector, node, opcode, cont, false);
  selector->UpdateSourcePosition(instr, node);
}

void VisitWord32Compare(InstructionSelector* selector, OpIndex node,
                        FlagsContinuation* cont) {
#ifdef USE_SIMULATOR
  const Operation& op = selector->Get(node);
  DCHECK_EQ(op.input_count, 2);
  const Operation& lhs = selector->Get(op.input(0));
  const Operation& rhs = selector->Get(op.input(1));
  if (lhs.Is<DidntThrowOp>() || rhs.Is<DidntThrowOp>()) {
    VisitFullWord32Compare(selector, node, kRiscvCmp, cont);
  } else if (!CanUseOptimizedWord32Compare(selector, node)) {
#else
  if (!CanUseOptimizedWord32Compare(selector, node)) {
#endif
    VisitFullWord32Compare(selector, node, kRiscvCmp, cont);
  } else {
    VisitOptimizedWord32Compare(selector, node, kRiscvCmp, cont);
  }
}

void VisitWord64Compare(InstructionSelector* selector, OpIndex node,
                        FlagsContinuation* cont) {
  VisitWordCompare(selector, node, kRiscvCmp, cont, false);
}

void VisitAtomicLoad(InstructionSelector* selector, OpIndex node,
                     AtomicWidth width) {
  using OpIndex = OpIndex;
  RiscvOperandGenerator g(selector);
  auto load = selector->load_view(node);
  OpIndex base = load.base();
  OpIndex index = load.index();

  // The memory order is ignored as both acquire and sequentially consistent
  // loads can emit LDAR.
  // https://www.cl.cam.ac.uk/~pes20/cpp/cpp0xmappings.html
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
      code = kRiscvWord64AtomicLoadUint64;
      break;
#ifdef V8_COMPRESS_POINTERS
    case MachineRepresentation::kTaggedSigned:
      code = kRiscvAtomicLoadDecompressTaggedSigned;
      break;
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTagged:
      code = kRiscvAtomicLoadDecompressTagged;
      break;
#else
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:
      if (kTaggedSize == 8) {
        code = kRiscvWord64AtomicLoadUint64;
      } else {
        code = kAtomicLoadWord32;
      }
      break;
#endif
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:
      DCHECK(COMPRESS_POINTERS_BOOL);
      code = kAtomicLoadWord32;
      break;
    default:
      UNREACHABLE();
  }

  bool traps_on_null;
  if (load.is_protected(&traps_on_null)) {
    code |= AccessModeField::encode(traps_on_null
                                        ? kMemoryAccessProtectedNullDereference
                                        : kMemoryAccessProtectedMemOutOfBounds);
  }

  if (g.CanBeImmediate(index, code)) {
    selector->Emit(code | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.DefineAsRegister(node), g.UseRegister(base),
                   g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(base), g.UseRegister(index));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(code | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

AtomicStoreParameters AtomicStoreParametersOf(InstructionSelector* selector,
                                              OpIndex node) {
  auto store = selector->store_view(node);
  return AtomicStoreParameters(store.stored_rep().representation(),
                               store.stored_rep().write_barrier_kind(),
                               store.memory_order().value(),
                               store.access_kind());
}

void VisitAtomicStore(InstructionSelector* selector, OpIndex node,
                      AtomicWidth width) {
  using OpIndex = OpIndex;
  RiscvOperandGenerator g(selector);
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

  DCHECK_IMPLIES(write_barrier_kind == kSkippedWriteBarrier,
                 v8_flags.verify_write_barriers);

  InstructionCode code;

  if (write_barrier_kind != kNoWriteBarrier &&
      !v8_flags.disable_write_barriers) {
    DCHECK(CanBeTaggedPointer(rep));
    DCHECK_EQ(AtomicWidthSize(width), kTaggedSize);
    AddressingMode addressing_mode;
    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    if (g.CanBeImmediate(index, kRiscvAdd64)) {
      inputs[input_count++] = g.UseImmediate(index);
      addressing_mode = kMode_MRI;
    } else {
      inputs[input_count++] = g.UseUniqueRegister(index);
      addressing_mode = kMode_MRR;
    }
    inputs[input_count++] = g.UseUniqueRegister(value);
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    size_t const temp_count = arraysize(temps);

    if (write_barrier_kind == kSkippedWriteBarrier) {
      code = kArchAtomicStoreSkippedWriteBarrier;
    } else {
      RecordWriteMode record_write_mode =
          WriteBarrierKindToRecordWriteMode(write_barrier_kind);
      code = kArchAtomicStoreWithWriteBarrier;
      code |= RecordWriteModeField::encode(record_write_mode);
    }

    if (store.is_store_trap_on_null()) {
      code |= AccessModeField::encode(kMemoryAccessProtectedNullDereference);
    } else if (store_params.kind() ==
               MemoryAccessKind::kProtectedByTrapHandler) {
      code |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
    }

    code |= AddressingModeField::encode(addressing_mode);
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
        code = kRiscvWord64AtomicStoreWord64;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:
        DCHECK_EQ(AtomicWidthSize(width), kTaggedSize);
        code = kRiscvStoreCompressTagged;
        break;
      default:
        UNREACHABLE();
    }
    code |= AtomicWidthField::encode(width);

    if (store.is_store_trap_on_null()) {
      code |= AccessModeField::encode(kMemoryAccessProtectedNullDereference);
    } else if (store_params.kind() ==
               MemoryAccessKind::kProtectedByTrapHandler) {
      code |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
    }
    if (g.CanBeImmediate(index, code)) {
      selector->Emit(code | AddressingModeField::encode(kMode_MRI) |
                         AtomicWidthField::encode(width),
                     g.NoOutput(), g.UseRegisterOrImmediateZero(value),
                     g.UseRegister(base), g.UseImmediate(index));
    } else {
      InstructionOperand addr_reg = g.TempRegister();
      selector->Emit(kRiscvAdd64 | AddressingModeField::encode(kMode_None),
                     addr_reg, g.UseRegister(index), g.UseRegister(base));
      // Emit desired store opcode, using temp addr_reg.
      selector->Emit(code | AddressingModeField::encode(kMode_MRI) |
                         AtomicWidthField::encode(width),
                     g.NoOutput(), g.UseRegisterOrImmediateZero(value),
                     addr_reg, g.TempImmediate(0));
    }
  }
}

void VisitAtomicBinop(InstructionSelector* selector, OpIndex node,
                      ArchOpcode opcode, AtomicWidth width,
                      MemoryAccessKind access_kind) {
  using OpIndex = OpIndex;
  RiscvOperandGenerator g(selector);
  const AtomicRMWOp& atomic_op = selector->Cast<AtomicRMWOp>(node);
  OpIndex base = atomic_op.base();
  OpIndex index = atomic_op.index();
  OpIndex value = atomic_op.value();

  AddressingMode addressing_mode;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  if (g.CanBeImmediate(index, kRiscvAdd64)) {
    inputs[input_count++] = g.UseImmediate(index);
    addressing_mode = kMode_MRI;
  } else {
    inputs[input_count++] = g.UseUniqueRegister(index);
    addressing_mode = kMode_MRR;
  }
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

void InstructionSelector::VisitStackPointerGreaterThan(
    OpIndex node, FlagsContinuation* cont) {
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

  InstructionOperand inputs[] = {g.UseRegisterWithMode(value, register_mode)};
  static constexpr int input_count = arraysize(inputs);

  EmitWithContinuation(opcode, output_count, outputs, input_count, inputs,
                       temp_count, temps, cont);
}

bool IsLoadWord32OrSmaller(InstructionSelector* selector, const OpIndex node) {
  const Operation& op = selector->Get(node);
  if (op.Is<LoadOp>()) {
    auto load_view = selector->load_view(node);
    LoadRepresentation load_rep = load_view.loaded_rep();
    switch (load_rep.representation()) {
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord32:
        return true;
      default:
        return false;
    }
  }
  return false;
}

void InstructionSelector::VisitWordCompareZero(OpIndex user, OpIndex value,
                                               FlagsContinuation* cont) {
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
      switch (comparison->rep.MapTaggedToWord().value()) {
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
        if (const OverflowCheckedBinopOp* binop =
                TryCast<OverflowCheckedBinopOp>(node);
            binop && CanDoBranchIfOverflowFusion(node)) {
          const bool is64 = binop->rep == WordRepresentation::Word64();
          OpIndex right_node = binop->input(1);
          RiscvOperandGenerator g(this);
          // Check if the right-hand side operand can be encoded as an immediate
          // value for a 32-bit operand add/sub. This is used to
          // determine whether we can utilize the more efficient overflow
          // checking path specifically designed for 32-bit operations with
          // immediate operands.
          const bool use_32 = g.CanBeImmediate(right_node, kRiscvAdd32);
          switch (binop->kind) {
            case OverflowCheckedBinopOp::Kind::kSignedAdd: {
              cont->OverwriteAndNegateIfEqual(kOverflow);
              ArchOpcode opcode = kRiscvAddOvfWord;
              if (!is64) {
                if (use_32)
                  opcode = kRiscvAdd32;
                else
                  opcode = kRiscvAdd64;
              }
              return VisitBinop<Int32BinopMatcher>(this, node, opcode, cont);
            }
            case OverflowCheckedBinopOp::Kind::kSignedSub: {
              cont->OverwriteAndNegateIfEqual(kOverflow);
              ArchOpcode opcode = kRiscvSubOvfWord;
              if (!is64) {
                if (use_32)
                  opcode = kRiscvSub32;
                else
                  opcode = kRiscvSub64;
              }
              return VisitBinop<Int32BinopMatcher>(this, node, opcode, cont);
            }
            case OverflowCheckedBinopOp::Kind::kSignedMul:
              cont->OverwriteAndNegateIfEqual(kOverflow);
              return VisitBinop<Int32BinopMatcher>(
                  this, node, is64 ? kRiscvMulOvf64 : kRiscvMulOvf32, cont);
          }
        }
      }
    } else if (value_op.Is<StackPointerGreaterThanOp>()) {
      // Matching these IR:
      // StackPointerGreaterThan(#6)[CodeStubAssembler]
      // Branch(#7)[B2, B1, True]
      cont->OverwriteAndNegateIfEqual(kStackPointerGreaterThanCondition);
      VisitStackPointerGreaterThan(value, cont);
      return;
    } else if (value_op.Is<Opmask::kWord32BitwiseAnd>() ||
               value_op.Is<Opmask::kWord64BitwiseAnd>()) {
      // Matching IR:
      // 7: Word64And
      // 8: Word64Equal(#7)
      VisitWordCompare(this, value, kRiscvTst64, cont, true);
      return;
    }
  }

  // Continuation could not be combined with a compare, emit compare against
  // 0.
  const ComparisonOp* comparison = this->Get(user).TryCast<ComparisonOp>();
#ifdef V8_COMPRESS_POINTERS
  if ((comparison &&
       comparison->rep.value() == RegisterRepresentation::Word64()) ||
      value_op.Is<Opmask::kWord32BitwiseAnd>() ||
      value_op.Is<Opmask::kTruncateWord64ToWord32>() ||
      IsLoadWord32OrSmaller(this, value) ||
      IsSignExtendWord32ToWord64(value_op)) {
    // If the value_op is sign-extended or lw/lhu/lh/lbu/lb, we can use
    // EmitWordCompareZero to emit a 32-bit compare zero.
    return EmitWordCompareZero(this, value, cont);
  } else {
    return EmitWord32CompareZero(this, value, cont);
  }
#else
  if (comparison &&
      comparison->rep.value() == RegisterRepresentation::Word32()) {
    return EmitWord32CompareZero(this, value, cont);
  } else {
    return EmitWordCompareZero(this, value, cont);
  }
#endif
}

void InstructionSelector::VisitWord32Equal(OpIndex node) {
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
    RiscvOperandGenerator g(this);
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
        if (g.CanBeImmediate(ptr, kRiscvCmp32)) {
          VisitCompare(this, kRiscvCmp32, g.UseRegister(left),
                       g.TempImmediate(static_cast<int32_t>(ptr)), &cont);
          return;
        }
      }
    }
  }
  VisitWord32Compare(this, node, &cont);
}

void InstructionSelector::VisitInt32LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWord32Compare(this, node, &cont);
}

void InstructionSelector::VisitInt32LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWord32Compare(this, node, &cont);
}

void InstructionSelector::VisitUint32LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWord32Compare(this, node, &cont);
}

void InstructionSelector::VisitUint32LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWord32Compare(this, node, &cont);
}

void InstructionSelector::VisitInt32AddWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid() && IsUsed(ovf.value())) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    const Operation& binop = Get(node);
    OpIndex right_node = binop.input(1);
    RiscvOperandGenerator g(this);
    // Check if the right-hand side operand can be encoded as an immediate
    // value for a 32-bit operand add/sub. This is used to
    // determine whether we can utilize the more efficient overflow
    // checking path specifically designed for 32-bit operations with
    // immediate operands.
    // TODO(yahan): Implement the 32-bit overflow fast check with Constant which
    // don't be encoded into instructions.
    const bool use_32 = g.CanBeImmediate(right_node, kRiscvAdd32);
    return VisitBinop<Int32BinopMatcher>(
        this, node, use_32 ? kRiscvAdd32 : kRiscvAdd64, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvAdd64, &cont);
}

void InstructionSelector::VisitInt32SubWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid() && IsUsed(ovf.value())) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    const Operation& binop = Get(node);
    OpIndex right_node = binop.input(1);
    RiscvOperandGenerator g(this);
    const bool use_32 = g.CanBeImmediate(right_node, kRiscvSub32);
    return VisitBinop<Int32BinopMatcher>(
        this, node, use_32 ? kRiscvSub32 : kRiscvSub64, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvSub64, &cont);
}

void InstructionSelector::VisitInt32MulWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop<Int32BinopMatcher>(this, node, kRiscvMulOvf32, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvMulOvf32, &cont);
}

void InstructionSelector::VisitInt64AddWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop<Int64BinopMatcher>(this, node, kRiscvAddOvfWord, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int64BinopMatcher>(this, node, kRiscvAddOvfWord, &cont);
}

void InstructionSelector::VisitInt64SubWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop<Int64BinopMatcher>(this, node, kRiscvSubOvfWord, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int64BinopMatcher>(this, node, kRiscvSubOvfWord, &cont);
}

void InstructionSelector::VisitInt64MulWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid()) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop<Int64BinopMatcher>(this, node, kRiscvMulOvf64, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int64BinopMatcher>(this, node, kRiscvMulOvf64, &cont);
}

void InstructionSelector::VisitWord64Equal(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  const ComparisonOp& equal = this->Get(node).template Cast<ComparisonOp>();
  DCHECK_EQ(equal.kind, ComparisonOp::Kind::kEqual);
  if (this->MatchIntegralZero(equal.right())) {
    return VisitWordCompareZero(node, equal.left(), &cont);
  }
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelector::VisitInt64LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelector::VisitInt64LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelector::VisitUint64LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelector::VisitUint64LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelector::VisitWord32AtomicLoad(OpIndex node) {
  VisitAtomicLoad(this, node, AtomicWidth::kWord32);
}

void InstructionSelector::VisitWord32AtomicStore(OpIndex node) {
  VisitAtomicStore(this, node, AtomicWidth::kWord32);
}

void InstructionSelector::VisitWord64AtomicLoad(OpIndex node) {
  VisitAtomicLoad(this, node, AtomicWidth::kWord64);
}

void InstructionSelector::VisitWord64AtomicStore(OpIndex node) {
  VisitAtomicStore(this, node, AtomicWidth::kWord64);
}

void VisitAtomicExchange(InstructionSelector* selector, OpIndex node,
                         ArchOpcode opcode, AtomicWidth width,
                         MemoryAccessKind access_kind) {
  using OpIndex = OpIndex;
  const AtomicRMWOp& atomic_op = selector->Cast<AtomicRMWOp>(node);
  RiscvOperandGenerator g(selector);
  OpIndex base = atomic_op.base();
  OpIndex index = atomic_op.index();
  OpIndex value = atomic_op.value();

  AddressingMode addressing_mode;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  if (g.CanBeImmediate(index, kRiscvAdd64)) {
    inputs[input_count++] = g.UseImmediate(index);
    addressing_mode = kMode_MRI;
  } else {
    inputs[input_count++] = g.UseUniqueRegister(index);
    addressing_mode = kMode_MRR;
  }
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

void VisitAtomicCompareExchange(InstructionSelector* selector, OpIndex node,
                                ArchOpcode opcode, AtomicWidth width,
                                MemoryAccessKind access_kind) {
  RiscvOperandGenerator g(selector);
  using OpIndex = OpIndex;
  const AtomicRMWOp& atomic_op = selector->Cast<AtomicRMWOp>(node);
  OpIndex base = atomic_op.base();
  OpIndex index = atomic_op.index();
  OpIndex old_value = atomic_op.expected().value();
  OpIndex new_value = atomic_op.value();

  bool has_write_barrier = opcode == kAtomicCompareExchangeWithWriteBarrier;
  InstructionOperand inputs[4];
  size_t input_count = 0;
  inputs[input_count++] =
      has_write_barrier ? g.UseUniqueRegister(base) : g.UseRegister(base);

  AddressingMode addressing_mode;
  if (g.CanBeImmediate(index, kRiscvAdd64)) {
    inputs[input_count++] = g.UseImmediate(index);
    addressing_mode = kMode_MRI;
  } else {
    inputs[input_count++] = g.UseUniqueRegister(index);
    addressing_mode = kMode_MRR;
  }

  inputs[input_count++] = g.UseUniqueRegister(old_value);
  inputs[input_count++] = g.UseUniqueRegister(new_value);

  InstructionOperand outputs[] = {g.UseUniqueRegister(node)};
  InstructionOperand temps[] = {g.TempRegister(), g.TempRegister(),
                                g.TempRegister()};

  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode) |
                         AtomicWidthField::encode(width);
  if (access_kind == MemoryAccessKind::kProtectedByTrapHandler) {
    code |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
  }
  selector->Emit(code, arraysize(outputs), outputs, input_count, inputs,
                 arraysize(temps), temps);
}

void InstructionSelector::VisitWord32AtomicExchange(OpIndex node) {
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

void InstructionSelector::VisitWord64AtomicExchange(OpIndex node) {
  const AtomicRMWOp& atomic_op = this->Get(node).template Cast<AtomicRMWOp>();
  ArchOpcode opcode;
  if (atomic_op.memory_rep == MemoryRepresentation::Uint8()) {
    opcode = kAtomicExchangeUint8;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint16()) {
    opcode = kAtomicExchangeUint16;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint32()) {
    opcode = kAtomicExchangeWord32;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint64()) {
    opcode = kRiscvWord64AtomicExchangeUint64;
  } else {
    UNREACHABLE();
  }
  VisitAtomicExchange(this, node, opcode, AtomicWidth::kWord64,
                      atomic_op.memory_access_kind);
}

void InstructionSelector::VisitTaggedAtomicExchange(OpIndex node) {
  const AtomicRMWOp& atomic_op = Cast<AtomicRMWOp>(node);
  AtomicWidth width =
      COMPRESS_POINTERS_BOOL ? AtomicWidth::kWord32 : AtomicWidth::kWord64;
  VisitAtomicExchange(this, node, kAtomicExchangeWithWriteBarrier, width,
                      atomic_op.memory_access_kind);
}

void InstructionSelector::VisitWord32AtomicCompareExchange(OpIndex node) {
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

void InstructionSelector::VisitWord64AtomicCompareExchange(OpIndex node) {
  const AtomicRMWOp& atomic_op = this->Get(node).template Cast<AtomicRMWOp>();
  ArchOpcode opcode;
  if (atomic_op.memory_rep == MemoryRepresentation::Uint8()) {
    opcode = kAtomicCompareExchangeUint8;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint16()) {
    opcode = kAtomicCompareExchangeUint16;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint32()) {
    opcode = kAtomicCompareExchangeWord32;
  } else if (atomic_op.memory_rep == MemoryRepresentation::Uint64()) {
    opcode = kRiscvWord64AtomicCompareExchangeUint64;
  } else {
    UNREACHABLE();
  }
  VisitAtomicCompareExchange(this, node, opcode, AtomicWidth::kWord64,
                             atomic_op.memory_access_kind);
}

void InstructionSelector::VisitTaggedAtomicCompareExchange(OpIndex node) {
  const AtomicRMWOp& atomic_op = Cast<AtomicRMWOp>(node);
  AtomicWidth width =
      COMPRESS_POINTERS_BOOL ? AtomicWidth::kWord32 : AtomicWidth::kWord64;
  VisitAtomicCompareExchange(this, node, kAtomicCompareExchangeWithWriteBarrier,
                             width, atomic_op.memory_access_kind);
}

void InstructionSelector::VisitWord32AtomicBinaryOperation(
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
                                                                         \
  void InstructionSelector::VisitWord32Atomic##op(OpIndex node) {        \
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
                                                                               \
  void InstructionSelector::VisitWord64Atomic##op(OpIndex node) {              \
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

void InstructionSelector::VisitInt32AbsWithOverflow(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelector::VisitInt64AbsWithOverflow(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelector::VisitSignExtendWord8ToInt64(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvSignExtendByte, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitSignExtendWord16ToInt64(OpIndex node) {
  RiscvOperandGenerator g(this);
  const Operation& op = this->Get(node);
  DCHECK_EQ(op.input_count, 1);
  Emit(kRiscvSignExtendShort, g.DefineAsRegister(node),
       g.UseRegister(op.input(0)));
}

void InstructionSelector::VisitSignExtendWord32ToInt64(OpIndex node) {
  EmitSignExtendWord(this, node);
}

//
// void InstructionSelectorT::Comment(const std::string msg){
//     RiscvOperandGeneratorT g(this);
//     if (!v8_flags.code_comments) return;
//     int64_t length = msg.length() + 1;
//     char* zone_buffer =
//     reinterpret_cast<char*>(this->isolate()->array_buffer_allocator()->Allocate(length));
//     memset(zone_buffer, '\0', length);
//     MemCopy(zone_buffer, msg.c_str(), length);
//     using ptrsize_int_t =
//         std::conditional_t<kSystemPointerSize == 8, int64_t, int32_t>;
//     InstructionOperand operand = this->sequence()->AddImmediate(
//         Constant{reinterpret_cast<ptrsize_int_t>(zone_buffer)});
//     InstructionOperand inputs[2];
//     inputs[0] = operand;
//     inputs[1] = g.UseImmediate64(length);
//     Emit(kArchComment, 0, nullptr, 1, inputs);
// }

// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags = MachineOperatorBuilder::kNoFlags;
  flags |= MachineOperatorBuilder::kWord32ShiftIsSafe |
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
  if (CpuFeatures::IsSupported(ZBB)) {
    flags |= MachineOperatorBuilder::kWord32Ctz |
             MachineOperatorBuilder::kWord64Ctz |
             MachineOperatorBuilder::kWord32Popcnt |
             MachineOperatorBuilder::kWord64Popcnt;
  }
  return flags;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
