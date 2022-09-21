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

#define DECL_MULTI_REP_BINOP(name, operation, kind)                        \
  OpIndex name(OpIndex left, OpIndex right, MachineRepresentation rep) {   \
    return subclass().operation(left, right, operation##Op::Kind::k##kind, \
                                rep);                                      \
  }
#define DECL_SINGLE_REP_BINOP(name, operation, kind, rep)                  \
  OpIndex name(OpIndex left, OpIndex right) {                              \
    return subclass().operation(left, right, operation##Op::Kind::k##kind, \
                                MachineRepresentation::k##rep);            \
  }
#define DECL_SINGLE_REP_BINOP_NO_KIND(name, operation, rep)                  \
  OpIndex name(OpIndex left, OpIndex right) {                                \
    return subclass().operation(left, right, MachineRepresentation::k##rep); \
  }
  DECL_MULTI_REP_BINOP(WordAdd, WordBinop, Add)
  DECL_SINGLE_REP_BINOP(Word32Add, WordBinop, Add, Word32)
  DECL_SINGLE_REP_BINOP(Word64Add, WordBinop, Add, Word64)

  DECL_MULTI_REP_BINOP(WordMul, WordBinop, Mul)
  DECL_SINGLE_REP_BINOP(Word32Mul, WordBinop, Mul, Word32)
  DECL_SINGLE_REP_BINOP(Word64Mul, WordBinop, Mul, Word64)

  DECL_MULTI_REP_BINOP(WordBitwiseAnd, WordBinop, BitwiseAnd)
  DECL_SINGLE_REP_BINOP(Word32BitwiseAnd, WordBinop, BitwiseAnd, Word32)
  DECL_SINGLE_REP_BINOP(Word64BitwiseAnd, WordBinop, BitwiseAnd, Word64)

  DECL_MULTI_REP_BINOP(WordBitwiseOr, WordBinop, BitwiseOr)
  DECL_SINGLE_REP_BINOP(Word32BitwiseOr, WordBinop, BitwiseOr, Word32)
  DECL_SINGLE_REP_BINOP(Word64BitwiseOr, WordBinop, BitwiseOr, Word64)

  DECL_MULTI_REP_BINOP(WordBitwiseXor, WordBinop, BitwiseXor)
  DECL_SINGLE_REP_BINOP(Word32BitwiseXor, WordBinop, BitwiseXor, Word32)
  DECL_SINGLE_REP_BINOP(Word64BitwiseXor, WordBinop, BitwiseXor, Word64)

  DECL_MULTI_REP_BINOP(WordSub, WordBinop, Sub)
  DECL_SINGLE_REP_BINOP(Word32Sub, WordBinop, Sub, Word32)
  DECL_SINGLE_REP_BINOP(Word64Sub, WordBinop, Sub, Word64)

  DECL_MULTI_REP_BINOP(IntDiv, WordBinop, SignedDiv)
  DECL_SINGLE_REP_BINOP(Int32Div, WordBinop, SignedDiv, Word32)
  DECL_SINGLE_REP_BINOP(Int64Div, WordBinop, SignedDiv, Word64)
  DECL_MULTI_REP_BINOP(UintDiv, WordBinop, UnsignedDiv)
  DECL_SINGLE_REP_BINOP(Uint32Div, WordBinop, UnsignedDiv, Word32)
  DECL_SINGLE_REP_BINOP(Uint64Div, WordBinop, UnsignedDiv, Word64)
  DECL_MULTI_REP_BINOP(IntMod, WordBinop, SignedMod)
  DECL_SINGLE_REP_BINOP(Int32Mod, WordBinop, SignedMod, Word32)
  DECL_SINGLE_REP_BINOP(Int64Mod, WordBinop, SignedMod, Word64)
  DECL_MULTI_REP_BINOP(UintMod, WordBinop, UnsignedMod)
  DECL_SINGLE_REP_BINOP(Uint32Mod, WordBinop, UnsignedMod, Word32)
  DECL_SINGLE_REP_BINOP(Uint64Mod, WordBinop, UnsignedMod, Word64)
  DECL_SINGLE_REP_BINOP(Int32MulOverflownBits, WordBinop,
                        SignedMulOverflownBits, Word32)
  DECL_SINGLE_REP_BINOP(Uint32MulOverflownBits, WordBinop,
                        UnsignedMulOverflownBits, Word32)

  DECL_MULTI_REP_BINOP(IntAddCheckOverflow, OverflowCheckedBinop, SignedAdd)
  DECL_SINGLE_REP_BINOP(Int32AddCheckOverflow, OverflowCheckedBinop, SignedAdd,
                        Word32)
  DECL_SINGLE_REP_BINOP(Int64AddCheckOverflow, OverflowCheckedBinop, SignedAdd,
                        Word64)
  DECL_MULTI_REP_BINOP(IntSubCheckOverflow, OverflowCheckedBinop, SignedSub)
  DECL_SINGLE_REP_BINOP(Int32SubCheckOverflow, OverflowCheckedBinop, SignedSub,
                        Word32)
  DECL_SINGLE_REP_BINOP(Int64SubCheckOverflow, OverflowCheckedBinop, SignedSub,
                        Word64)
  DECL_MULTI_REP_BINOP(IntMulCheckOverflow, OverflowCheckedBinop, SignedMul)
  DECL_SINGLE_REP_BINOP(Int32MulCheckOverflow, OverflowCheckedBinop, SignedMul,
                        Word32)
  DECL_SINGLE_REP_BINOP(Int64MulCheckOverflow, OverflowCheckedBinop, SignedMul,
                        Word64)

  DECL_MULTI_REP_BINOP(FloatAdd, FloatBinop, Add)
  DECL_SINGLE_REP_BINOP(Float32Add, FloatBinop, Add, Float32)
  DECL_SINGLE_REP_BINOP(Float64Add, FloatBinop, Add, Float64)
  DECL_MULTI_REP_BINOP(FloatMul, FloatBinop, Mul)
  DECL_SINGLE_REP_BINOP(Float32Mul, FloatBinop, Mul, Float32)
  DECL_SINGLE_REP_BINOP(Float64Mul, FloatBinop, Mul, Float64)
  DECL_MULTI_REP_BINOP(FloatSub, FloatBinop, Sub)
  DECL_SINGLE_REP_BINOP(Float32Sub, FloatBinop, Sub, Float32)
  DECL_SINGLE_REP_BINOP(Float64Sub, FloatBinop, Sub, Float64)
  DECL_MULTI_REP_BINOP(FloatDiv, FloatBinop, Div)
  DECL_SINGLE_REP_BINOP(Float32Div, FloatBinop, Div, Float32)
  DECL_SINGLE_REP_BINOP(Float64Div, FloatBinop, Div, Float64)
  DECL_MULTI_REP_BINOP(FloatMin, FloatBinop, Min)
  DECL_SINGLE_REP_BINOP(Float32Min, FloatBinop, Min, Float32)
  DECL_SINGLE_REP_BINOP(Float64Min, FloatBinop, Min, Float64)
  DECL_MULTI_REP_BINOP(FloatMax, FloatBinop, Max)
  DECL_SINGLE_REP_BINOP(Float32Max, FloatBinop, Max, Float32)
  DECL_SINGLE_REP_BINOP(Float64Max, FloatBinop, Max, Float64)
  DECL_SINGLE_REP_BINOP(Float64Mod, FloatBinop, Mod, Float64)
  DECL_SINGLE_REP_BINOP(Float64Power, FloatBinop, Power, Float64)
  DECL_SINGLE_REP_BINOP(Float64Atan2, FloatBinop, Atan2, Float64)

  DECL_MULTI_REP_BINOP(ShiftRightArithmeticShiftOutZeros, Shift,
                       ShiftRightArithmeticShiftOutZeros)
  DECL_SINGLE_REP_BINOP(Word32ShiftRightArithmeticShiftOutZeros, Shift,
                        ShiftRightArithmeticShiftOutZeros, Word32)
  DECL_SINGLE_REP_BINOP(Word64ShiftRightArithmeticShiftOutZeros, Shift,
                        ShiftRightArithmeticShiftOutZeros, Word64)
  DECL_MULTI_REP_BINOP(ShiftRightArithmetic, Shift, ShiftRightArithmetic)
  DECL_SINGLE_REP_BINOP(Word32ShiftRightArithmetic, Shift, ShiftRightArithmetic,
                        Word32)
  DECL_SINGLE_REP_BINOP(Word64ShiftRightArithmetic, Shift, ShiftRightArithmetic,
                        Word64)
  DECL_MULTI_REP_BINOP(ShiftRightLogical, Shift, ShiftRightLogical)
  DECL_SINGLE_REP_BINOP(Word32ShiftRightLogical, Shift, ShiftRightLogical,
                        Word32)
  DECL_SINGLE_REP_BINOP(Word64ShiftRightLogical, Shift, ShiftRightLogical,
                        Word64)
  DECL_MULTI_REP_BINOP(ShiftLeft, Shift, ShiftLeft)
  DECL_SINGLE_REP_BINOP(Word32ShiftLeft, Shift, ShiftLeft, Word32)
  DECL_SINGLE_REP_BINOP(Word64ShiftLeft, Shift, ShiftLeft, Word64)
  DECL_MULTI_REP_BINOP(RotateRight, Shift, RotateRight)
  DECL_SINGLE_REP_BINOP(Word32RotateRight, Shift, RotateRight, Word32)
  DECL_SINGLE_REP_BINOP(Word64RotateRight, Shift, RotateRight, Word64)
  DECL_MULTI_REP_BINOP(RotateLeft, Shift, RotateLeft)
  DECL_SINGLE_REP_BINOP(Word32RotateLeft, Shift, RotateLeft, Word32)
  DECL_SINGLE_REP_BINOP(Word64RotateLeft, Shift, RotateLeft, Word64)

  OpIndex ShiftRightLogical(OpIndex left, uint32_t right,
                            MachineRepresentation rep) {
    DCHECK_GE(right, 0);
    DCHECK_LT(right, ElementSizeInBits(rep));
    return ShiftRightLogical(left, Word32Constant(right), rep);
  }
  OpIndex ShiftRightArithmetic(OpIndex left, uint32_t right,
                               MachineRepresentation rep) {
    DCHECK_GE(right, 0);
    DCHECK_LT(right, ElementSizeInBits(rep));
    return ShiftRightArithmetic(left, Word32Constant(right), rep);
  }

  DECL_SINGLE_REP_BINOP_NO_KIND(Word32Equal, Equal, Word32)
  DECL_SINGLE_REP_BINOP_NO_KIND(Word64Equal, Equal, Word64)
  DECL_SINGLE_REP_BINOP_NO_KIND(Float32Equal, Equal, Float32)
  DECL_SINGLE_REP_BINOP_NO_KIND(Float64Equal, Equal, Float64)

  DECL_MULTI_REP_BINOP(IntLessThan, Comparison, SignedLessThan)
  DECL_SINGLE_REP_BINOP(Int32LessThan, Comparison, SignedLessThan, Word32)
  DECL_SINGLE_REP_BINOP(Int64LessThan, Comparison, SignedLessThan, Word64)
  DECL_MULTI_REP_BINOP(UintLessThan, Comparison, UnsignedLessThan)
  DECL_SINGLE_REP_BINOP(Uint32LessThan, Comparison, UnsignedLessThan, Word32)
  DECL_SINGLE_REP_BINOP(Uint64LessThan, Comparison, UnsignedLessThan, Word64)
  DECL_MULTI_REP_BINOP(FloatLessThan, Comparison, SignedLessThan)
  DECL_SINGLE_REP_BINOP(Float32LessThan, Comparison, SignedLessThan, Float32)
  DECL_SINGLE_REP_BINOP(Float64LessThan, Comparison, SignedLessThan, Float64)

  DECL_MULTI_REP_BINOP(IntLessThanOrEqual, Comparison, SignedLessThanOrEqual)
  DECL_SINGLE_REP_BINOP(Int32LessThanOrEqual, Comparison, SignedLessThanOrEqual,
                        Word32)
  DECL_SINGLE_REP_BINOP(Int64LessThanOrEqual, Comparison, SignedLessThanOrEqual,
                        Word64)
  DECL_MULTI_REP_BINOP(UintLessThanOrEqual, Comparison, UnsignedLessThanOrEqual)
  DECL_SINGLE_REP_BINOP(Uint32LessThanOrEqual, Comparison,
                        UnsignedLessThanOrEqual, Word32)
  DECL_SINGLE_REP_BINOP(Uint64LessThanOrEqual, Comparison,
                        UnsignedLessThanOrEqual, Word64)
  DECL_MULTI_REP_BINOP(FloatLessThanOrEqual, Comparison, SignedLessThanOrEqual)
  DECL_SINGLE_REP_BINOP(Float32LessThanOrEqual, Comparison,
                        SignedLessThanOrEqual, Float32)
  DECL_SINGLE_REP_BINOP(Float64LessThanOrEqual, Comparison,
                        SignedLessThanOrEqual, Float64)

#undef DECL_SINGLE_REP_BINOP
#undef DECL_MULTI_REP_BINOP
#undef DECL_SINGLE_REP_BINOP_NO_KIND

#define DECL_MULTI_REP_UNARY(name, operation, kind)                        \
  OpIndex name(OpIndex input, MachineRepresentation rep) {                 \
    return subclass().operation(input, operation##Op::Kind::k##kind, rep); \
  }
#define DECL_SINGLE_REP_UNARY(name, operation, kind, rep)            \
  OpIndex name(OpIndex input) {                                      \
    return subclass().operation(input, operation##Op::Kind::k##kind, \
                                MachineRepresentation::k##rep);      \
  }

  DECL_MULTI_REP_UNARY(FloatAbs, FloatUnary, Abs)
  DECL_SINGLE_REP_UNARY(Float32Abs, FloatUnary, Abs, Float32)
  DECL_SINGLE_REP_UNARY(Float64Abs, FloatUnary, Abs, Float64)
  DECL_MULTI_REP_UNARY(FloatNegate, FloatUnary, Negate)
  DECL_SINGLE_REP_UNARY(Float32Negate, FloatUnary, Negate, Float32)
  DECL_SINGLE_REP_UNARY(Float64Negate, FloatUnary, Negate, Float64)
  DECL_SINGLE_REP_UNARY(Float64SilenceNaN, FloatUnary, SilenceNaN, Float64)
  DECL_MULTI_REP_UNARY(FloatRoundDown, FloatUnary, RoundDown)
  DECL_SINGLE_REP_UNARY(Float32RoundDown, FloatUnary, RoundDown, Float32)
  DECL_SINGLE_REP_UNARY(Float64RoundDown, FloatUnary, RoundDown, Float64)
  DECL_MULTI_REP_UNARY(FloatRoundUp, FloatUnary, RoundUp)
  DECL_SINGLE_REP_UNARY(Float32RoundUp, FloatUnary, RoundUp, Float32)
  DECL_SINGLE_REP_UNARY(Float64RoundUp, FloatUnary, RoundUp, Float64)
  DECL_MULTI_REP_UNARY(FloatRoundToZero, FloatUnary, RoundToZero)
  DECL_SINGLE_REP_UNARY(Float32RoundToZero, FloatUnary, RoundToZero, Float32)
  DECL_SINGLE_REP_UNARY(Float64RoundToZero, FloatUnary, RoundToZero, Float64)
  DECL_MULTI_REP_UNARY(FloatRoundTiesEven, FloatUnary, RoundTiesEven)
  DECL_SINGLE_REP_UNARY(Float32RoundTiesEven, FloatUnary, RoundTiesEven,
                        Float32)
  DECL_SINGLE_REP_UNARY(Float64RoundTiesEven, FloatUnary, RoundTiesEven,
                        Float64)
  DECL_SINGLE_REP_UNARY(Float64Log, FloatUnary, Log, Float64)
  DECL_MULTI_REP_UNARY(FloatSqrt, FloatUnary, Sqrt)
  DECL_SINGLE_REP_UNARY(Float32Sqrt, FloatUnary, Sqrt, Float32)
  DECL_SINGLE_REP_UNARY(Float64Sqrt, FloatUnary, Sqrt, Float64)
  DECL_SINGLE_REP_UNARY(Float64Exp, FloatUnary, Exp, Float64)
  DECL_SINGLE_REP_UNARY(Float64Expm1, FloatUnary, Expm1, Float64)
  DECL_SINGLE_REP_UNARY(Float64Sin, FloatUnary, Sin, Float64)
  DECL_SINGLE_REP_UNARY(Float64Cos, FloatUnary, Cos, Float64)
  DECL_SINGLE_REP_UNARY(Float64Sinh, FloatUnary, Sinh, Float64)
  DECL_SINGLE_REP_UNARY(Float64Cosh, FloatUnary, Cosh, Float64)
  DECL_SINGLE_REP_UNARY(Float64Asin, FloatUnary, Asin, Float64)
  DECL_SINGLE_REP_UNARY(Float64Acos, FloatUnary, Acos, Float64)
  DECL_SINGLE_REP_UNARY(Float64Asinh, FloatUnary, Asinh, Float64)
  DECL_SINGLE_REP_UNARY(Float64Acosh, FloatUnary, Acosh, Float64)
  DECL_SINGLE_REP_UNARY(Float64Tan, FloatUnary, Tan, Float64)
  DECL_SINGLE_REP_UNARY(Float64Tanh, FloatUnary, Tanh, Float64)

  DECL_MULTI_REP_UNARY(WordReverseBytes, WordUnary, ReverseBytes)
  DECL_SINGLE_REP_UNARY(Word32ReverseBytes, WordUnary, ReverseBytes, Word32)
  DECL_SINGLE_REP_UNARY(Word64ReverseBytes, WordUnary, ReverseBytes, Word64)
  DECL_MULTI_REP_UNARY(WordCountLeadingZeros, WordUnary, CountLeadingZeros)
  DECL_SINGLE_REP_UNARY(Word32CountLeadingZeros, WordUnary, CountLeadingZeros,
                        Word32)
  DECL_SINGLE_REP_UNARY(Word64CountLeadingZeros, WordUnary, CountLeadingZeros,
                        Word64)
#undef DECL_SINGLE_REP_UNARY
#undef DECL_MULTI_REP_UNARY

  OpIndex Word32Constant(uint32_t value) {
    return subclass().Constant(ConstantOp::Kind::kWord32, uint64_t{value});
  }
  OpIndex Word32Constant(int32_t value) {
    return Word32Constant(static_cast<uint32_t>(value));
  }
  OpIndex Word64Constant(uint64_t value) {
    return subclass().Constant(ConstantOp::Kind::kWord64, value);
  }
  OpIndex Word64Constant(int64_t value) {
    return Word64Constant(static_cast<uint64_t>(value));
  }
  OpIndex WordConstant(uint64_t value, MachineRepresentation rep) {
    switch (rep) {
      case MachineRepresentation::kWord32:
        DCHECK(value <= MaxUnsignedValue(MachineRepresentation::kWord32));
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
  OpIndex FloatConstant(double value, MachineRepresentation rep) {
    switch (rep) {
      case MachineRepresentation::kFloat32:
        return Float32Constant(static_cast<float>(value));
      case MachineRepresentation::kFloat64:
        return Float64Constant(value);
      default:
        UNREACHABLE();
    }
  }
  OpIndex NumberConstant(double value) {
    return subclass().Constant(ConstantOp::Kind::kNumber, value);
  }
  OpIndex TaggedIndexConstant(int32_t value) {
    return subclass().Constant(ConstantOp::Kind::kTaggedIndex,
                               uint64_t{static_cast<uint32_t>(value)});
  }
  OpIndex HeapConstant(Handle<HeapObject> value) {
    return subclass().Constant(ConstantOp::Kind::kHeapObject, value);
  }
  OpIndex CompressedHeapConstant(Handle<HeapObject> value) {
    return subclass().Constant(ConstantOp::Kind::kHeapObject, value);
  }
  OpIndex ExternalConstant(ExternalReference value) {
    return subclass().Constant(ConstantOp::Kind::kExternal, value);
  }

#define DECL_CHANGE(name, kind, from, to)                    \
  OpIndex name(OpIndex input) {                              \
    return subclass().Change(input, ChangeOp::Kind::k##kind, \
                             MachineRepresentation::k##from, \
                             MachineRepresentation::k##to);  \
  }

  DECL_CHANGE(BitcastWord32ToWord64, Bitcast, Word32, Word64)
  DECL_CHANGE(BitcastFloat32ToWord32, Bitcast, Float32, Word32)
  DECL_CHANGE(BitcastWord32ToFloat32, Bitcast, Word32, Float32)
  DECL_CHANGE(BitcastFloat64ToWord64, Bitcast, Float64, Word64)
  DECL_CHANGE(BitcastWord6464ToFloat64, Bitcast, Word64, Float64)
  DECL_CHANGE(ChangeUint32ToUint64, ZeroExtend, Word32, Word64)
  DECL_CHANGE(ChangeInt32ToInt64, SignExtend, Word32, Word64)
  DECL_CHANGE(ChangeInt32ToFloat64, SignedToFloat, Word32, Float64)
  DECL_CHANGE(ChangeInt64ToFloat64, SignedToFloat, Word64, Float64)
  DECL_CHANGE(ChangeUint32ToFloat64, UnsignedToFloat, Word32, Float64)
  DECL_CHANGE(ChangeFloat64ToFloat32, FloatConversion, Float64, Float32)
  DECL_CHANGE(ChangeFloat32ToFloat64, FloatConversion, Float32, Float64)
  DECL_CHANGE(JSTruncateFloat64ToWord32, JSFloatTruncate, Float64, Word32)
  DECL_CHANGE(TruncateFloat64ToInt32OverflowUndefined, SignedFloatTruncate,
              Float64, Word32)
  DECL_CHANGE(TruncateFloat64ToInt32OverflowToMin,
              SignedFloatTruncateOverflowToMin, Float64, Word32)
  DECL_CHANGE(NarrowFloat64ToInt32, SignedNarrowing, Float64, Word32)
  DECL_CHANGE(NarrowFloat64ToUint32, UnsignedNarrowing, Float64, Word32)
  DECL_CHANGE(NarrowFloat64ToInt64, SignedNarrowing, Float64, Word64)
  DECL_CHANGE(NarrowFloat64ToUint64, UnsignedNarrowing, Float64, Word64)
  DECL_CHANGE(Float64ExtractLowWord32, ExtractLowHalf, Float64, Word32)
  DECL_CHANGE(Float64ExtractHighWord32, ExtractHighHalf, Float64, Word32)
#undef DECL_CHANGE

  using Base::Tuple;
  OpIndex Tuple(OpIndex a, OpIndex b) {
    return subclass().Tuple(base::VectorOf({a, b}));
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

  void EnterBlock(const Block& block) { USE(block); }
  void ExitBlock(const Block& block) { USE(block); }

  V8_INLINE bool Bind(Block* block) {
    if (!graph().Add(block)) return false;
    DCHECK_NULL(current_block_);
    current_block_ = block;
    return true;
  }

  void SetCurrentOrigin(OpIndex operation_origin) {
    current_operation_origin_ = operation_origin;
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
    SupportedOperations::Initialize();
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
    graph().operation_origins()[result] = current_operation_origin_;
    if (Op::properties.is_block_terminator) FinalizeBlock();
    return result;
  }

  Block* current_block_ = nullptr;
  Graph& graph_;
  OpIndex current_operation_origin_ = OpIndex::Invalid();
  Zone* const phase_zone_;
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_ASSEMBLER_H_
