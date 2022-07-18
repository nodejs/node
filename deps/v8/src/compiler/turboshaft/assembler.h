// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_ASSEMBLER_H_
#define V8_COMPILER_TURBOSHAFT_ASSEMBLER_H_

#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>

#include "src/base/iterator.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/small-vector.h"
#include "src/base/template-utils.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/source-position.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/zone/zone-containers.h"

namespace v8::internal::compiler::turboshaft {

// This class is used to extend an assembler with useful short-hands that still
// forward to the regular operations of the deriving assembler.
template <class Subclass, class Superclass>
class AssemblerInterface : public Superclass {
 public:
  using Superclass::Superclass;
  using Base = Superclass;

  OpIndex Add(OpIndex left, OpIndex right, MachineRepresentation rep) {
    return subclass().Binop(left, right, BinopOp::Kind::kAdd, rep);
  }
  OpIndex AddWithOverflow(OpIndex left, OpIndex right,
                          MachineRepresentation rep) {
    DCHECK(rep == MachineRepresentation::kWord32 ||
           rep == MachineRepresentation::kWord64);
    return subclass().OverflowCheckedBinop(
        left, right, OverflowCheckedBinopOp::Kind::kSignedAdd, rep);
  }
  OpIndex Sub(OpIndex left, OpIndex right, MachineRepresentation rep) {
    return subclass().Binop(left, right, BinopOp::Kind::kSub, rep);
  }
  OpIndex SubWithOverflow(OpIndex left, OpIndex right,
                          MachineRepresentation rep) {
    return subclass().OverflowCheckedBinop(
        left, right, OverflowCheckedBinopOp::Kind::kSignedSub, rep);
  }
  OpIndex Mul(OpIndex left, OpIndex right, MachineRepresentation rep) {
    return subclass().Binop(left, right, BinopOp::Kind::kMul, rep);
  }
  OpIndex SignedMulOverflownBits(OpIndex left, OpIndex right,
                                 MachineRepresentation rep) {
    return subclass().Binop(left, right, BinopOp::Kind::kSignedMulOverflownBits,
                            rep);
  }
  OpIndex UnsignedMulOverflownBits(OpIndex left, OpIndex right,
                                   MachineRepresentation rep) {
    return subclass().Binop(left, right,
                            BinopOp::Kind::kUnsignedMulOverflownBits, rep);
  }
  OpIndex MulWithOverflow(OpIndex left, OpIndex right,
                          MachineRepresentation rep) {
    DCHECK(rep == MachineRepresentation::kWord32 ||
           rep == MachineRepresentation::kWord64);
    return subclass().OverflowCheckedBinop(
        left, right, OverflowCheckedBinopOp::Kind::kSignedMul, rep);
  }
  OpIndex SignedDiv(OpIndex left, OpIndex right, MachineRepresentation rep) {
    return subclass().Binop(left, right, BinopOp::Kind::kSignedDiv, rep);
  }
  OpIndex UnsignedDiv(OpIndex left, OpIndex right, MachineRepresentation rep) {
    return subclass().Binop(left, right, BinopOp::Kind::kUnsignedDiv, rep);
  }
  OpIndex SignedMod(OpIndex left, OpIndex right, MachineRepresentation rep) {
    return subclass().Binop(left, right, BinopOp::Kind::kSignedMod, rep);
  }
  OpIndex UnsignedMod(OpIndex left, OpIndex right, MachineRepresentation rep) {
    return subclass().Binop(left, right, BinopOp::Kind::kUnsignedMod, rep);
  }
  OpIndex BitwiseAnd(OpIndex left, OpIndex right, MachineRepresentation rep) {
    DCHECK(rep == MachineRepresentation::kWord32 ||
           rep == MachineRepresentation::kWord64);
    return subclass().Binop(left, right, BinopOp::Kind::kBitwiseAnd, rep);
  }
  OpIndex BitwiseOr(OpIndex left, OpIndex right, MachineRepresentation rep) {
    DCHECK(rep == MachineRepresentation::kWord32 ||
           rep == MachineRepresentation::kWord64);
    return subclass().Binop(left, right, BinopOp::Kind::kBitwiseOr, rep);
  }
  OpIndex Min(OpIndex left, OpIndex right, MachineRepresentation rep) {
    DCHECK_EQ(rep, MachineRepresentation::kFloat64);
    return subclass().Binop(left, right, BinopOp::Kind::kMin, rep);
  }
  OpIndex Max(OpIndex left, OpIndex right, MachineRepresentation rep) {
    DCHECK_EQ(rep, MachineRepresentation::kFloat64);
    return subclass().Binop(left, right, BinopOp::Kind::kMax, rep);
  }
  OpIndex Power(OpIndex left, OpIndex right, MachineRepresentation rep) {
    DCHECK_EQ(rep, MachineRepresentation::kFloat64);
    return subclass().Binop(left, right, BinopOp::Kind::kPower, rep);
  }
  OpIndex Atan2(OpIndex left, OpIndex right, MachineRepresentation rep) {
    DCHECK_EQ(rep, MachineRepresentation::kFloat64);
    return subclass().Binop(left, right, BinopOp::Kind::kAtan2, rep);
  }
  OpIndex BitwiseXor(OpIndex left, OpIndex right, MachineRepresentation rep) {
    DCHECK(rep == MachineRepresentation::kWord32 ||
           rep == MachineRepresentation::kWord64);
    return subclass().Binop(left, right, BinopOp::Kind::kBitwiseXor, rep);
  }
  OpIndex ShiftLeft(OpIndex left, OpIndex right, MachineRepresentation rep) {
    DCHECK(rep == MachineRepresentation::kWord32 ||
           rep == MachineRepresentation::kWord64);
    return subclass().Shift(left, right, ShiftOp::Kind::kShiftLeft, rep);
  }
  OpIndex ShiftRightArithmetic(OpIndex left, OpIndex right,
                               MachineRepresentation rep) {
    DCHECK(rep == MachineRepresentation::kWord32 ||
           rep == MachineRepresentation::kWord64);
    return subclass().Shift(left, right, ShiftOp::Kind::kShiftRightArithmetic,
                            rep);
  }
  OpIndex ShiftRightArithmeticShiftOutZeros(OpIndex left, OpIndex right,
                                            MachineRepresentation rep) {
    DCHECK(rep == MachineRepresentation::kWord32 ||
           rep == MachineRepresentation::kWord64);
    return subclass().Shift(
        left, right, ShiftOp::Kind::kShiftRightArithmeticShiftOutZeros, rep);
  }
  OpIndex ShiftRightLogical(OpIndex left, OpIndex right,
                            MachineRepresentation rep) {
    DCHECK(rep == MachineRepresentation::kWord32 ||
           rep == MachineRepresentation::kWord64);
    return subclass().Shift(left, right, ShiftOp::Kind::kShiftRightLogical,
                            rep);
  }
  OpIndex RotateLeft(OpIndex left, OpIndex right, MachineRepresentation rep) {
    DCHECK(rep == MachineRepresentation::kWord32 ||
           rep == MachineRepresentation::kWord64);
    return subclass().Shift(left, right, ShiftOp::Kind::kRotateLeft, rep);
  }
  OpIndex RotateRight(OpIndex left, OpIndex right, MachineRepresentation rep) {
    DCHECK(rep == MachineRepresentation::kWord32 ||
           rep == MachineRepresentation::kWord64);
    return subclass().Shift(left, right, ShiftOp::Kind::kRotateRight, rep);
  }
  OpIndex Word32Constant(uint32_t value) {
    return subclass().Constant(ConstantOp::Kind::kWord32, uint64_t{value});
  }
  OpIndex Word64Constant(uint64_t value) {
    return subclass().Constant(ConstantOp::Kind::kWord64, value);
  }
  OpIndex IntegralConstant(uint64_t value, MachineRepresentation rep) {
    switch (rep) {
      case MachineRepresentation::kWord32:
        return Word32Constant(static_cast<uint32_t>(value));
      case MachineRepresentation::kWord64:
        return Word64Constant(value);
      default:
        UNREACHABLE();
    }
  }
  OpIndex Float32Constant(float value) {
    return subclass().Constant(ConstantOp::Kind::kFloat32, value);
  }
  OpIndex Float64Constant(double value) {
    return subclass().Constant(ConstantOp::Kind::kFloat64, value);
  }

  OpIndex TrucateWord64ToWord32(OpIndex value) {
    return subclass().Change(value, ChangeOp::Kind::kIntegerTruncate,
                             MachineRepresentation::kWord64,
                             MachineRepresentation::kWord32);
  }

  OpIndex ExceptionValueProjection(OpIndex value) {
    return subclass().Projection(value, ProjectionOp::Kind::kExceptionValue, 0);
  }
  OpIndex TupleProjection(OpIndex value, uint16_t index) {
    return subclass().Projection(value, ProjectionOp::Kind::kTuple, index);
  }

 private:
  Subclass& subclass() { return *static_cast<Subclass*>(this); }
};

// This empty base-class is used to provide default-implementations of plain
// methods emitting operations.
template <class Assembler>
class AssemblerBase {
 public:
#define EMIT_OP(Name)                                                       \
  template <class... Args>                                                  \
  OpIndex Name(Args... args) {                                              \
    return static_cast<Assembler*>(this)->template Emit<Name##Op>(args...); \
  }
  TURBOSHAFT_OPERATION_LIST(EMIT_OP)
#undef EMIT_OP
};

class Assembler
    : public AssemblerInterface<Assembler, AssemblerBase<Assembler>> {
 public:
  Block* NewBlock(Block::Kind kind) { return graph_.NewBlock(kind); }

  V8_INLINE bool Bind(Block* block) {
    if (!graph().Add(block)) return false;
    DCHECK_NULL(current_block_);
    current_block_ = block;
    return true;
  }

  void SetCurrentSourcePosition(SourcePosition position) {
    current_source_position_ = position;
  }

  OpIndex Phi(base::Vector<const OpIndex> inputs, MachineRepresentation rep) {
    DCHECK(current_block()->IsMerge() &&
           inputs.size() == current_block()->Predecessors().size());
    return Base::Phi(inputs, rep);
  }

  template <class... Args>
  OpIndex PendingLoopPhi(Args... args) {
    DCHECK(current_block()->IsLoop());
    return Base::PendingLoopPhi(args...);
  }

  OpIndex Goto(Block* destination) {
    destination->AddPredecessor(current_block());
    return Base::Goto(destination);
  }

  OpIndex Branch(OpIndex condition, Block* if_true, Block* if_false) {
    if_true->AddPredecessor(current_block());
    if_false->AddPredecessor(current_block());
    return Base::Branch(condition, if_true, if_false);
  }

  OpIndex CatchException(OpIndex call, Block* if_success, Block* if_exception) {
    if_success->AddPredecessor(current_block());
    if_exception->AddPredecessor(current_block());
    return Base::CatchException(call, if_success, if_exception);
  }

  OpIndex Switch(OpIndex input, base::Vector<const SwitchOp::Case> cases,
                 Block* default_case) {
    for (SwitchOp::Case c : cases) {
      c.destination->AddPredecessor(current_block());
    }
    default_case->AddPredecessor(current_block());
    return Base::Switch(input, cases, default_case);
  }

  explicit Assembler(Graph* graph, Zone* phase_zone)
      : graph_(*graph), phase_zone_(phase_zone) {
    graph_.Reset();
  }

  Block* current_block() { return current_block_; }
  Zone* graph_zone() { return graph().graph_zone(); }
  Graph& graph() { return graph_; }
  Zone* phase_zone() { return phase_zone_; }

 private:
  friend class AssemblerBase<Assembler>;
  void FinalizeBlock() {
    graph().Finalize(current_block_);
    current_block_ = nullptr;
  }

  template <class Op, class... Args>
  OpIndex Emit(Args... args) {
    static_assert((std::is_base_of<Operation, Op>::value));
    static_assert(!(std::is_same<Op, Operation>::value));
    DCHECK_NOT_NULL(current_block_);
    OpIndex result = graph().Add<Op>(args...);
    if (current_source_position_.IsKnown()) {
      graph().source_positions()[result] = current_source_position_;
    }
    if (Op::properties.is_block_terminator) FinalizeBlock();
    return result;
  }

  Block* current_block_ = nullptr;
  Graph& graph_;
  SourcePosition current_source_position_ = SourcePosition::Unknown();
  Zone* const phase_zone_;
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_ASSEMBLER_H_
