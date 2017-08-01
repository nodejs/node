// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/machine-operator.h"

#include "src/base/lazy-instance.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"

namespace v8 {
namespace internal {
namespace compiler {

bool operator==(StoreRepresentation lhs, StoreRepresentation rhs) {
  return lhs.representation() == rhs.representation() &&
         lhs.write_barrier_kind() == rhs.write_barrier_kind();
}


bool operator!=(StoreRepresentation lhs, StoreRepresentation rhs) {
  return !(lhs == rhs);
}


size_t hash_value(StoreRepresentation rep) {
  return base::hash_combine(rep.representation(), rep.write_barrier_kind());
}


std::ostream& operator<<(std::ostream& os, StoreRepresentation rep) {
  return os << "(" << rep.representation() << " : " << rep.write_barrier_kind()
            << ")";
}


LoadRepresentation LoadRepresentationOf(Operator const* op) {
  DCHECK(IrOpcode::kLoad == op->opcode() ||
         IrOpcode::kProtectedLoad == op->opcode() ||
         IrOpcode::kAtomicLoad == op->opcode());
  return OpParameter<LoadRepresentation>(op);
}


StoreRepresentation const& StoreRepresentationOf(Operator const* op) {
  DCHECK(IrOpcode::kStore == op->opcode() ||
         IrOpcode::kProtectedStore == op->opcode());
  return OpParameter<StoreRepresentation>(op);
}

UnalignedLoadRepresentation UnalignedLoadRepresentationOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kUnalignedLoad, op->opcode());
  return OpParameter<UnalignedLoadRepresentation>(op);
}

UnalignedStoreRepresentation const& UnalignedStoreRepresentationOf(
    Operator const* op) {
  DCHECK_EQ(IrOpcode::kUnalignedStore, op->opcode());
  return OpParameter<UnalignedStoreRepresentation>(op);
}

CheckedLoadRepresentation CheckedLoadRepresentationOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kCheckedLoad, op->opcode());
  return OpParameter<CheckedLoadRepresentation>(op);
}


CheckedStoreRepresentation CheckedStoreRepresentationOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kCheckedStore, op->opcode());
  return OpParameter<CheckedStoreRepresentation>(op);
}

bool operator==(StackSlotRepresentation lhs, StackSlotRepresentation rhs) {
  return lhs.size() == rhs.size() && lhs.alignment() == rhs.alignment();
}

bool operator!=(StackSlotRepresentation lhs, StackSlotRepresentation rhs) {
  return !(lhs == rhs);
}

size_t hash_value(StackSlotRepresentation rep) {
  return base::hash_combine(rep.size(), rep.alignment());
}

std::ostream& operator<<(std::ostream& os, StackSlotRepresentation rep) {
  return os << "(" << rep.size() << " : " << rep.alignment() << ")";
}

StackSlotRepresentation const& StackSlotRepresentationOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kStackSlot, op->opcode());
  return OpParameter<StackSlotRepresentation>(op);
}

MachineRepresentation AtomicStoreRepresentationOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kAtomicStore, op->opcode());
  return OpParameter<MachineRepresentation>(op);
}

MachineType AtomicOpRepresentationOf(Operator const* op) {
  return OpParameter<MachineType>(op);
}

#define PURE_BINARY_OP_LIST_32(V)                                           \
  V(Word32And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)    \
  V(Word32Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(Word32Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)    \
  V(Word32Shl, Operator::kNoProperties, 2, 0, 1)                            \
  V(Word32Shr, Operator::kNoProperties, 2, 0, 1)                            \
  V(Word32Sar, Operator::kNoProperties, 2, 0, 1)                            \
  V(Word32Ror, Operator::kNoProperties, 2, 0, 1)                            \
  V(Word32Equal, Operator::kCommutative, 2, 0, 1)                           \
  V(Int32Add, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(Int32Sub, Operator::kNoProperties, 2, 0, 1)                             \
  V(Int32Mul, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(Int32MulHigh, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Int32Div, Operator::kNoProperties, 2, 1, 1)                             \
  V(Int32Mod, Operator::kNoProperties, 2, 1, 1)                             \
  V(Int32LessThan, Operator::kNoProperties, 2, 0, 1)                        \
  V(Int32LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)                 \
  V(Uint32Div, Operator::kNoProperties, 2, 1, 1)                            \
  V(Uint32LessThan, Operator::kNoProperties, 2, 0, 1)                       \
  V(Uint32LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)                \
  V(Uint32Mod, Operator::kNoProperties, 2, 1, 1)                            \
  V(Uint32MulHigh, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)

#define PURE_BINARY_OP_LIST_64(V)                                        \
  V(Word64And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Word64Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Word64Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Word64Shl, Operator::kNoProperties, 2, 0, 1)                         \
  V(Word64Shr, Operator::kNoProperties, 2, 0, 1)                         \
  V(Word64Sar, Operator::kNoProperties, 2, 0, 1)                         \
  V(Word64Ror, Operator::kNoProperties, 2, 0, 1)                         \
  V(Word64Equal, Operator::kCommutative, 2, 0, 1)                        \
  V(Int64Add, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Int64Sub, Operator::kNoProperties, 2, 0, 1)                          \
  V(Int64Mul, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Int64Div, Operator::kNoProperties, 2, 1, 1)                          \
  V(Int64Mod, Operator::kNoProperties, 2, 1, 1)                          \
  V(Int64LessThan, Operator::kNoProperties, 2, 0, 1)                     \
  V(Int64LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)              \
  V(Uint64Div, Operator::kNoProperties, 2, 1, 1)                         \
  V(Uint64Mod, Operator::kNoProperties, 2, 1, 1)                         \
  V(Uint64LessThan, Operator::kNoProperties, 2, 0, 1)                    \
  V(Uint64LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)

#define PURE_OP_LIST(V)                                                   \
  PURE_BINARY_OP_LIST_32(V)                                               \
  PURE_BINARY_OP_LIST_64(V)                                               \
  V(Word32Clz, Operator::kNoProperties, 1, 0, 1)                          \
  V(Word64Clz, Operator::kNoProperties, 1, 0, 1)                          \
  V(BitcastTaggedToWord, Operator::kNoProperties, 1, 0, 1)                \
  V(BitcastWordToTaggedSigned, Operator::kNoProperties, 1, 0, 1)          \
  V(TruncateFloat64ToWord32, Operator::kNoProperties, 1, 0, 1)            \
  V(ChangeFloat32ToFloat64, Operator::kNoProperties, 1, 0, 1)             \
  V(ChangeFloat64ToInt32, Operator::kNoProperties, 1, 0, 1)               \
  V(ChangeFloat64ToUint32, Operator::kNoProperties, 1, 0, 1)              \
  V(ChangeFloat64ToUint64, Operator::kNoProperties, 1, 0, 1)              \
  V(TruncateFloat64ToUint32, Operator::kNoProperties, 1, 0, 1)            \
  V(TruncateFloat32ToInt32, Operator::kNoProperties, 1, 0, 1)             \
  V(TruncateFloat32ToUint32, Operator::kNoProperties, 1, 0, 1)            \
  V(TryTruncateFloat32ToInt64, Operator::kNoProperties, 1, 0, 2)          \
  V(TryTruncateFloat64ToInt64, Operator::kNoProperties, 1, 0, 2)          \
  V(TryTruncateFloat32ToUint64, Operator::kNoProperties, 1, 0, 2)         \
  V(TryTruncateFloat64ToUint64, Operator::kNoProperties, 1, 0, 2)         \
  V(ChangeInt32ToFloat64, Operator::kNoProperties, 1, 0, 1)               \
  V(Float64SilenceNaN, Operator::kNoProperties, 1, 0, 1)                  \
  V(RoundFloat64ToInt32, Operator::kNoProperties, 1, 0, 1)                \
  V(RoundInt32ToFloat32, Operator::kNoProperties, 1, 0, 1)                \
  V(RoundInt64ToFloat32, Operator::kNoProperties, 1, 0, 1)                \
  V(RoundInt64ToFloat64, Operator::kNoProperties, 1, 0, 1)                \
  V(RoundUint32ToFloat32, Operator::kNoProperties, 1, 0, 1)               \
  V(RoundUint64ToFloat32, Operator::kNoProperties, 1, 0, 1)               \
  V(RoundUint64ToFloat64, Operator::kNoProperties, 1, 0, 1)               \
  V(ChangeInt32ToInt64, Operator::kNoProperties, 1, 0, 1)                 \
  V(ChangeUint32ToFloat64, Operator::kNoProperties, 1, 0, 1)              \
  V(ChangeUint32ToUint64, Operator::kNoProperties, 1, 0, 1)               \
  V(TruncateFloat64ToFloat32, Operator::kNoProperties, 1, 0, 1)           \
  V(TruncateInt64ToInt32, Operator::kNoProperties, 1, 0, 1)               \
  V(BitcastFloat32ToInt32, Operator::kNoProperties, 1, 0, 1)              \
  V(BitcastFloat64ToInt64, Operator::kNoProperties, 1, 0, 1)              \
  V(BitcastInt32ToFloat32, Operator::kNoProperties, 1, 0, 1)              \
  V(BitcastInt64ToFloat64, Operator::kNoProperties, 1, 0, 1)              \
  V(Float32Abs, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float32Add, Operator::kCommutative, 2, 0, 1)                          \
  V(Float32Sub, Operator::kNoProperties, 2, 0, 1)                         \
  V(Float32Mul, Operator::kCommutative, 2, 0, 1)                          \
  V(Float32Div, Operator::kNoProperties, 2, 0, 1)                         \
  V(Float32Neg, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float32Sqrt, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float32Max, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Float32Min, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Float64Abs, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Acos, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Acosh, Operator::kNoProperties, 1, 0, 1)                       \
  V(Float64Asin, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Asinh, Operator::kNoProperties, 1, 0, 1)                       \
  V(Float64Atan, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Atan2, Operator::kNoProperties, 2, 0, 1)                       \
  V(Float64Atanh, Operator::kNoProperties, 1, 0, 1)                       \
  V(Float64Cbrt, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Cos, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Cosh, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Exp, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Expm1, Operator::kNoProperties, 1, 0, 1)                       \
  V(Float64Log, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Log1p, Operator::kNoProperties, 1, 0, 1)                       \
  V(Float64Log2, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Log10, Operator::kNoProperties, 1, 0, 1)                       \
  V(Float64Max, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Float64Min, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Float64Neg, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Add, Operator::kCommutative, 2, 0, 1)                          \
  V(Float64Sub, Operator::kNoProperties, 2, 0, 1)                         \
  V(Float64Mul, Operator::kCommutative, 2, 0, 1)                          \
  V(Float64Div, Operator::kNoProperties, 2, 0, 1)                         \
  V(Float64Mod, Operator::kNoProperties, 2, 0, 1)                         \
  V(Float64Pow, Operator::kNoProperties, 2, 0, 1)                         \
  V(Float64Sin, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Sinh, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Sqrt, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Tan, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Tanh, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float32Equal, Operator::kCommutative, 2, 0, 1)                        \
  V(Float32LessThan, Operator::kNoProperties, 2, 0, 1)                    \
  V(Float32LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)             \
  V(Float64Equal, Operator::kCommutative, 2, 0, 1)                        \
  V(Float64LessThan, Operator::kNoProperties, 2, 0, 1)                    \
  V(Float64LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)             \
  V(Float64ExtractLowWord32, Operator::kNoProperties, 1, 0, 1)            \
  V(Float64ExtractHighWord32, Operator::kNoProperties, 1, 0, 1)           \
  V(Float64InsertLowWord32, Operator::kNoProperties, 2, 0, 1)             \
  V(Float64InsertHighWord32, Operator::kNoProperties, 2, 0, 1)            \
  V(LoadStackPointer, Operator::kNoProperties, 0, 0, 1)                   \
  V(LoadFramePointer, Operator::kNoProperties, 0, 0, 1)                   \
  V(LoadParentFramePointer, Operator::kNoProperties, 0, 0, 1)             \
  V(Int32PairAdd, Operator::kNoProperties, 4, 0, 2)                       \
  V(Int32PairSub, Operator::kNoProperties, 4, 0, 2)                       \
  V(Int32PairMul, Operator::kNoProperties, 4, 0, 2)                       \
  V(Word32PairShl, Operator::kNoProperties, 3, 0, 2)                      \
  V(Word32PairShr, Operator::kNoProperties, 3, 0, 2)                      \
  V(Word32PairSar, Operator::kNoProperties, 3, 0, 2)                      \
  V(F32x4Splat, Operator::kNoProperties, 1, 0, 1)                         \
  V(F32x4SConvertI32x4, Operator::kNoProperties, 1, 0, 1)                 \
  V(F32x4UConvertI32x4, Operator::kNoProperties, 1, 0, 1)                 \
  V(F32x4Abs, Operator::kNoProperties, 1, 0, 1)                           \
  V(F32x4Neg, Operator::kNoProperties, 1, 0, 1)                           \
  V(F32x4RecipApprox, Operator::kNoProperties, 1, 0, 1)                   \
  V(F32x4RecipSqrtApprox, Operator::kNoProperties, 1, 0, 1)               \
  V(F32x4Add, Operator::kCommutative, 2, 0, 1)                            \
  V(F32x4AddHoriz, Operator::kNoProperties, 2, 0, 1)                      \
  V(F32x4Sub, Operator::kNoProperties, 2, 0, 1)                           \
  V(F32x4Mul, Operator::kCommutative, 2, 0, 1)                            \
  V(F32x4Min, Operator::kCommutative, 2, 0, 1)                            \
  V(F32x4Max, Operator::kCommutative, 2, 0, 1)                            \
  V(F32x4Eq, Operator::kCommutative, 2, 0, 1)                             \
  V(F32x4Ne, Operator::kCommutative, 2, 0, 1)                             \
  V(F32x4Lt, Operator::kNoProperties, 2, 0, 1)                            \
  V(F32x4Le, Operator::kNoProperties, 2, 0, 1)                            \
  V(I32x4Splat, Operator::kNoProperties, 1, 0, 1)                         \
  V(I32x4SConvertF32x4, Operator::kNoProperties, 1, 0, 1)                 \
  V(I32x4SConvertI16x8Low, Operator::kNoProperties, 1, 0, 1)              \
  V(I32x4SConvertI16x8High, Operator::kNoProperties, 1, 0, 1)             \
  V(I32x4Neg, Operator::kNoProperties, 1, 0, 1)                           \
  V(I32x4Add, Operator::kCommutative, 2, 0, 1)                            \
  V(I32x4AddHoriz, Operator::kNoProperties, 2, 0, 1)                      \
  V(I32x4Sub, Operator::kNoProperties, 2, 0, 1)                           \
  V(I32x4Mul, Operator::kCommutative, 2, 0, 1)                            \
  V(I32x4MinS, Operator::kCommutative, 2, 0, 1)                           \
  V(I32x4MaxS, Operator::kCommutative, 2, 0, 1)                           \
  V(I32x4Eq, Operator::kCommutative, 2, 0, 1)                             \
  V(I32x4Ne, Operator::kCommutative, 2, 0, 1)                             \
  V(I32x4LtS, Operator::kNoProperties, 2, 0, 1)                           \
  V(I32x4LeS, Operator::kNoProperties, 2, 0, 1)                           \
  V(I32x4UConvertF32x4, Operator::kNoProperties, 1, 0, 1)                 \
  V(I32x4UConvertI16x8Low, Operator::kNoProperties, 1, 0, 1)              \
  V(I32x4UConvertI16x8High, Operator::kNoProperties, 1, 0, 1)             \
  V(I32x4MinU, Operator::kCommutative, 2, 0, 1)                           \
  V(I32x4MaxU, Operator::kCommutative, 2, 0, 1)                           \
  V(I32x4LtU, Operator::kNoProperties, 2, 0, 1)                           \
  V(I32x4LeU, Operator::kNoProperties, 2, 0, 1)                           \
  V(I16x8Splat, Operator::kNoProperties, 1, 0, 1)                         \
  V(I16x8SConvertI8x16Low, Operator::kNoProperties, 1, 0, 1)              \
  V(I16x8SConvertI8x16High, Operator::kNoProperties, 1, 0, 1)             \
  V(I16x8Neg, Operator::kNoProperties, 1, 0, 1)                           \
  V(I16x8SConvertI32x4, Operator::kNoProperties, 2, 0, 1)                 \
  V(I16x8Add, Operator::kCommutative, 2, 0, 1)                            \
  V(I16x8AddSaturateS, Operator::kCommutative, 2, 0, 1)                   \
  V(I16x8AddHoriz, Operator::kNoProperties, 2, 0, 1)                      \
  V(I16x8Sub, Operator::kNoProperties, 2, 0, 1)                           \
  V(I16x8SubSaturateS, Operator::kNoProperties, 2, 0, 1)                  \
  V(I16x8Mul, Operator::kCommutative, 2, 0, 1)                            \
  V(I16x8MinS, Operator::kCommutative, 2, 0, 1)                           \
  V(I16x8MaxS, Operator::kCommutative, 2, 0, 1)                           \
  V(I16x8Eq, Operator::kCommutative, 2, 0, 1)                             \
  V(I16x8Ne, Operator::kCommutative, 2, 0, 1)                             \
  V(I16x8LtS, Operator::kNoProperties, 2, 0, 1)                           \
  V(I16x8LeS, Operator::kNoProperties, 2, 0, 1)                           \
  V(I16x8UConvertI8x16Low, Operator::kNoProperties, 1, 0, 1)              \
  V(I16x8UConvertI8x16High, Operator::kNoProperties, 1, 0, 1)             \
  V(I16x8UConvertI32x4, Operator::kNoProperties, 2, 0, 1)                 \
  V(I16x8AddSaturateU, Operator::kCommutative, 2, 0, 1)                   \
  V(I16x8SubSaturateU, Operator::kNoProperties, 2, 0, 1)                  \
  V(I16x8MinU, Operator::kCommutative, 2, 0, 1)                           \
  V(I16x8MaxU, Operator::kCommutative, 2, 0, 1)                           \
  V(I16x8LtU, Operator::kNoProperties, 2, 0, 1)                           \
  V(I16x8LeU, Operator::kNoProperties, 2, 0, 1)                           \
  V(I8x16Splat, Operator::kNoProperties, 1, 0, 1)                         \
  V(I8x16Neg, Operator::kNoProperties, 1, 0, 1)                           \
  V(I8x16SConvertI16x8, Operator::kNoProperties, 2, 0, 1)                 \
  V(I8x16Add, Operator::kCommutative, 2, 0, 1)                            \
  V(I8x16AddSaturateS, Operator::kCommutative, 2, 0, 1)                   \
  V(I8x16Sub, Operator::kNoProperties, 2, 0, 1)                           \
  V(I8x16SubSaturateS, Operator::kNoProperties, 2, 0, 1)                  \
  V(I8x16Mul, Operator::kCommutative, 2, 0, 1)                            \
  V(I8x16MinS, Operator::kCommutative, 2, 0, 1)                           \
  V(I8x16MaxS, Operator::kCommutative, 2, 0, 1)                           \
  V(I8x16Eq, Operator::kCommutative, 2, 0, 1)                             \
  V(I8x16Ne, Operator::kCommutative, 2, 0, 1)                             \
  V(I8x16LtS, Operator::kNoProperties, 2, 0, 1)                           \
  V(I8x16LeS, Operator::kNoProperties, 2, 0, 1)                           \
  V(I8x16UConvertI16x8, Operator::kNoProperties, 2, 0, 1)                 \
  V(I8x16AddSaturateU, Operator::kCommutative, 2, 0, 1)                   \
  V(I8x16SubSaturateU, Operator::kNoProperties, 2, 0, 1)                  \
  V(I8x16MinU, Operator::kCommutative, 2, 0, 1)                           \
  V(I8x16MaxU, Operator::kCommutative, 2, 0, 1)                           \
  V(I8x16LtU, Operator::kNoProperties, 2, 0, 1)                           \
  V(I8x16LeU, Operator::kNoProperties, 2, 0, 1)                           \
  V(S128Load, Operator::kNoProperties, 2, 0, 1)                           \
  V(S128Store, Operator::kNoProperties, 3, 0, 1)                          \
  V(S128Zero, Operator::kNoProperties, 0, 0, 1)                           \
  V(S128And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)    \
  V(S128Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(S128Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)    \
  V(S128Not, Operator::kNoProperties, 1, 0, 1)                            \
  V(S32x4Select, Operator::kNoProperties, 3, 0, 1)                        \
  V(S16x8Select, Operator::kNoProperties, 3, 0, 1)                        \
  V(S8x16Select, Operator::kNoProperties, 3, 0, 1)                        \
  V(S1x4Zero, Operator::kNoProperties, 0, 0, 1)                           \
  V(S1x4And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)    \
  V(S1x4Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(S1x4Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)    \
  V(S1x4Not, Operator::kNoProperties, 1, 0, 1)                            \
  V(S1x4AnyTrue, Operator::kNoProperties, 1, 0, 1)                        \
  V(S1x4AllTrue, Operator::kNoProperties, 1, 0, 1)                        \
  V(S1x8Zero, Operator::kNoProperties, 0, 0, 1)                           \
  V(S1x8And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)    \
  V(S1x8Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(S1x8Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)    \
  V(S1x8Not, Operator::kNoProperties, 1, 0, 1)                            \
  V(S1x8AnyTrue, Operator::kNoProperties, 1, 0, 1)                        \
  V(S1x8AllTrue, Operator::kNoProperties, 1, 0, 1)                        \
  V(S1x16Zero, Operator::kNoProperties, 0, 0, 1)                          \
  V(S1x16And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)   \
  V(S1x16Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)    \
  V(S1x16Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)   \
  V(S1x16Not, Operator::kNoProperties, 1, 0, 1)                           \
  V(S1x16AnyTrue, Operator::kNoProperties, 1, 0, 1)                       \
  V(S1x16AllTrue, Operator::kNoProperties, 1, 0, 1)

#define PURE_OPTIONAL_OP_LIST(V)                            \
  V(Word32Ctz, Operator::kNoProperties, 1, 0, 1)            \
  V(Word64Ctz, Operator::kNoProperties, 1, 0, 1)            \
  V(Word32ReverseBits, Operator::kNoProperties, 1, 0, 1)    \
  V(Word64ReverseBits, Operator::kNoProperties, 1, 0, 1)    \
  V(Word32ReverseBytes, Operator::kNoProperties, 1, 0, 1)   \
  V(Word64ReverseBytes, Operator::kNoProperties, 1, 0, 1)   \
  V(Int32AbsWithOverflow, Operator::kNoProperties, 1, 0, 1) \
  V(Int64AbsWithOverflow, Operator::kNoProperties, 1, 0, 1) \
  V(Word32Popcnt, Operator::kNoProperties, 1, 0, 1)         \
  V(Word64Popcnt, Operator::kNoProperties, 1, 0, 1)         \
  V(Float32RoundDown, Operator::kNoProperties, 1, 0, 1)     \
  V(Float64RoundDown, Operator::kNoProperties, 1, 0, 1)     \
  V(Float32RoundUp, Operator::kNoProperties, 1, 0, 1)       \
  V(Float64RoundUp, Operator::kNoProperties, 1, 0, 1)       \
  V(Float32RoundTruncate, Operator::kNoProperties, 1, 0, 1) \
  V(Float64RoundTruncate, Operator::kNoProperties, 1, 0, 1) \
  V(Float64RoundTiesAway, Operator::kNoProperties, 1, 0, 1) \
  V(Float32RoundTiesEven, Operator::kNoProperties, 1, 0, 1) \
  V(Float64RoundTiesEven, Operator::kNoProperties, 1, 0, 1)

#define OVERFLOW_OP_LIST(V)                                                \
  V(Int32AddWithOverflow, Operator::kAssociative | Operator::kCommutative) \
  V(Int32SubWithOverflow, Operator::kNoProperties)                         \
  V(Int32MulWithOverflow, Operator::kAssociative | Operator::kCommutative) \
  V(Int64AddWithOverflow, Operator::kAssociative | Operator::kCommutative) \
  V(Int64SubWithOverflow, Operator::kNoProperties)

#define MACHINE_TYPE_LIST(V) \
  V(Float32)                 \
  V(Float64)                 \
  V(Simd128)                 \
  V(Int8)                    \
  V(Uint8)                   \
  V(Int16)                   \
  V(Uint16)                  \
  V(Int32)                   \
  V(Uint32)                  \
  V(Int64)                   \
  V(Uint64)                  \
  V(Pointer)                 \
  V(TaggedSigned)            \
  V(TaggedPointer)           \
  V(AnyTagged)

#define MACHINE_REPRESENTATION_LIST(V) \
  V(kFloat32)                          \
  V(kFloat64)                          \
  V(kSimd128)                          \
  V(kWord8)                            \
  V(kWord16)                           \
  V(kWord32)                           \
  V(kWord64)                           \
  V(kTaggedSigned)                     \
  V(kTaggedPointer)                    \
  V(kTagged)

#define ATOMIC_TYPE_LIST(V) \
  V(Int8)                   \
  V(Uint8)                  \
  V(Int16)                  \
  V(Uint16)                 \
  V(Int32)                  \
  V(Uint32)

#define ATOMIC_REPRESENTATION_LIST(V) \
  V(kWord8)                           \
  V(kWord16)                          \
  V(kWord32)

#define SIMD_LANE_OP_LIST(V) \
  V(F32x4, 4)                \
  V(I32x4, 4)                \
  V(I16x8, 8)                \
  V(I8x16, 16)

#define SIMD_FORMAT_LIST(V) \
  V(32x4, 32)               \
  V(16x8, 16)               \
  V(8x16, 8)

#define STACK_SLOT_CACHED_SIZES_ALIGNMENTS_LIST(V) \
  V(4, 0) V(8, 0) V(16, 0) V(4, 4) V(8, 8) V(16, 16)

struct StackSlotOperator : public Operator1<StackSlotRepresentation> {
  explicit StackSlotOperator(int size, int alignment)
      : Operator1<StackSlotRepresentation>(
            IrOpcode::kStackSlot, Operator::kNoDeopt | Operator::kNoThrow,
            "StackSlot", 0, 0, 0, 1, 0, 0,
            StackSlotRepresentation(size, alignment)) {}
};

struct MachineOperatorGlobalCache {
#define PURE(Name, properties, value_input_count, control_input_count,         \
             output_count)                                                     \
  struct Name##Operator final : public Operator {                              \
    Name##Operator()                                                           \
        : Operator(IrOpcode::k##Name, Operator::kPure | properties, #Name,     \
                   value_input_count, 0, control_input_count, output_count, 0, \
                   0) {}                                                       \
  };                                                                           \
  Name##Operator k##Name;
  PURE_OP_LIST(PURE)
  PURE_OPTIONAL_OP_LIST(PURE)
#undef PURE

#define OVERFLOW_OP(Name, properties)                                        \
  struct Name##Operator final : public Operator {                            \
    Name##Operator()                                                         \
        : Operator(IrOpcode::k##Name,                                        \
                   Operator::kEliminatable | Operator::kNoRead | properties, \
                   #Name, 2, 0, 1, 2, 0, 0) {}                               \
  };                                                                         \
  Name##Operator k##Name;
  OVERFLOW_OP_LIST(OVERFLOW_OP)
#undef OVERFLOW_OP

#define LOAD(Type)                                                            \
  struct Load##Type##Operator final : public Operator1<LoadRepresentation> {  \
    Load##Type##Operator()                                                    \
        : Operator1<LoadRepresentation>(                                      \
              IrOpcode::kLoad,                                                \
              Operator::kNoDeopt | Operator::kNoThrow | Operator::kNoWrite,   \
              "Load", 2, 1, 1, 1, 1, 0, MachineType::Type()) {}               \
  };                                                                          \
  struct UnalignedLoad##Type##Operator final                                  \
      : public Operator1<UnalignedLoadRepresentation> {                       \
    UnalignedLoad##Type##Operator()                                           \
        : Operator1<UnalignedLoadRepresentation>(                             \
              IrOpcode::kUnalignedLoad,                                       \
              Operator::kNoDeopt | Operator::kNoThrow | Operator::kNoWrite,   \
              "UnalignedLoad", 2, 1, 1, 1, 1, 0, MachineType::Type()) {}      \
  };                                                                          \
  struct CheckedLoad##Type##Operator final                                    \
      : public Operator1<CheckedLoadRepresentation> {                         \
    CheckedLoad##Type##Operator()                                             \
        : Operator1<CheckedLoadRepresentation>(                               \
              IrOpcode::kCheckedLoad,                                         \
              Operator::kNoDeopt | Operator::kNoThrow | Operator::kNoWrite,   \
              "CheckedLoad", 3, 1, 1, 1, 1, 0, MachineType::Type()) {}        \
  };                                                                          \
  struct ProtectedLoad##Type##Operator final                                  \
      : public Operator1<LoadRepresentation> {                                \
    ProtectedLoad##Type##Operator()                                           \
        : Operator1<LoadRepresentation>(                                      \
              IrOpcode::kProtectedLoad,                                       \
              Operator::kNoDeopt | Operator::kNoThrow, "ProtectedLoad", 3, 1, \
              1, 1, 1, 0, MachineType::Type()) {}                             \
  };                                                                          \
  Load##Type##Operator kLoad##Type;                                           \
  UnalignedLoad##Type##Operator kUnalignedLoad##Type;                         \
  CheckedLoad##Type##Operator kCheckedLoad##Type;                             \
  ProtectedLoad##Type##Operator kProtectedLoad##Type;
  MACHINE_TYPE_LIST(LOAD)
#undef LOAD

#define STACKSLOT(Size, Alignment)                                     \
  struct StackSlotOfSize##Size##OfAlignment##Alignment##Operator final \
      : public StackSlotOperator {                                     \
    StackSlotOfSize##Size##OfAlignment##Alignment##Operator()          \
        : StackSlotOperator(Size, Alignment) {}                        \
  };                                                                   \
  StackSlotOfSize##Size##OfAlignment##Alignment##Operator              \
      kStackSlotOfSize##Size##OfAlignment##Alignment;
  STACK_SLOT_CACHED_SIZES_ALIGNMENTS_LIST(STACKSLOT)
#undef STACKSLOT

#define STORE(Type)                                                            \
  struct Store##Type##Operator : public Operator1<StoreRepresentation> {       \
    explicit Store##Type##Operator(WriteBarrierKind write_barrier_kind)        \
        : Operator1<StoreRepresentation>(                                      \
              IrOpcode::kStore,                                                \
              Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow,     \
              "Store", 3, 1, 1, 0, 1, 0,                                       \
              StoreRepresentation(MachineRepresentation::Type,                 \
                                  write_barrier_kind)) {}                      \
  };                                                                           \
  struct Store##Type##NoWriteBarrier##Operator final                           \
      : public Store##Type##Operator {                                         \
    Store##Type##NoWriteBarrier##Operator()                                    \
        : Store##Type##Operator(kNoWriteBarrier) {}                            \
  };                                                                           \
  struct Store##Type##MapWriteBarrier##Operator final                          \
      : public Store##Type##Operator {                                         \
    Store##Type##MapWriteBarrier##Operator()                                   \
        : Store##Type##Operator(kMapWriteBarrier) {}                           \
  };                                                                           \
  struct Store##Type##PointerWriteBarrier##Operator final                      \
      : public Store##Type##Operator {                                         \
    Store##Type##PointerWriteBarrier##Operator()                               \
        : Store##Type##Operator(kPointerWriteBarrier) {}                       \
  };                                                                           \
  struct Store##Type##FullWriteBarrier##Operator final                         \
      : public Store##Type##Operator {                                         \
    Store##Type##FullWriteBarrier##Operator()                                  \
        : Store##Type##Operator(kFullWriteBarrier) {}                          \
  };                                                                           \
  struct UnalignedStore##Type##Operator final                                  \
      : public Operator1<UnalignedStoreRepresentation> {                       \
    UnalignedStore##Type##Operator()                                           \
        : Operator1<UnalignedStoreRepresentation>(                             \
              IrOpcode::kUnalignedStore,                                       \
              Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow,     \
              "UnalignedStore", 3, 1, 1, 0, 1, 0,                              \
              MachineRepresentation::Type) {}                                  \
  };                                                                           \
  struct CheckedStore##Type##Operator final                                    \
      : public Operator1<CheckedStoreRepresentation> {                         \
    CheckedStore##Type##Operator()                                             \
        : Operator1<CheckedStoreRepresentation>(                               \
              IrOpcode::kCheckedStore,                                         \
              Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow,     \
              "CheckedStore", 4, 1, 1, 0, 1, 0, MachineRepresentation::Type) { \
    }                                                                          \
  };                                                                           \
  struct ProtectedStore##Type##Operator                                        \
      : public Operator1<StoreRepresentation> {                                \
    explicit ProtectedStore##Type##Operator()                                  \
        : Operator1<StoreRepresentation>(                                      \
              IrOpcode::kProtectedStore,                                       \
              Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow,     \
              "Store", 4, 1, 1, 0, 1, 0,                                       \
              StoreRepresentation(MachineRepresentation::Type,                 \
                                  kNoWriteBarrier)) {}                         \
  };                                                                           \
  Store##Type##NoWriteBarrier##Operator kStore##Type##NoWriteBarrier;          \
  Store##Type##MapWriteBarrier##Operator kStore##Type##MapWriteBarrier;        \
  Store##Type##PointerWriteBarrier##Operator                                   \
      kStore##Type##PointerWriteBarrier;                                       \
  Store##Type##FullWriteBarrier##Operator kStore##Type##FullWriteBarrier;      \
  UnalignedStore##Type##Operator kUnalignedStore##Type;                        \
  CheckedStore##Type##Operator kCheckedStore##Type;                            \
  ProtectedStore##Type##Operator kProtectedStore##Type;
  MACHINE_REPRESENTATION_LIST(STORE)
#undef STORE

#define ATOMIC_LOAD(Type)                                                   \
  struct AtomicLoad##Type##Operator final                                   \
      : public Operator1<LoadRepresentation> {                              \
    AtomicLoad##Type##Operator()                                            \
        : Operator1<LoadRepresentation>(                                    \
              IrOpcode::kAtomicLoad,                                        \
              Operator::kNoDeopt | Operator::kNoThrow | Operator::kNoWrite, \
              "AtomicLoad", 2, 1, 1, 1, 1, 0, MachineType::Type()) {}       \
  };                                                                        \
  AtomicLoad##Type##Operator kAtomicLoad##Type;
  ATOMIC_TYPE_LIST(ATOMIC_LOAD)
#undef ATOMIC_LOAD

#define ATOMIC_STORE(Type)                                                     \
  struct AtomicStore##Type##Operator                                           \
      : public Operator1<MachineRepresentation> {                              \
    AtomicStore##Type##Operator()                                              \
        : Operator1<MachineRepresentation>(                                    \
              IrOpcode::kAtomicStore,                                          \
              Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow,     \
              "AtomicStore", 3, 1, 1, 0, 1, 0, MachineRepresentation::Type) {} \
  };                                                                           \
  AtomicStore##Type##Operator kAtomicStore##Type;
  ATOMIC_REPRESENTATION_LIST(ATOMIC_STORE)
#undef STORE

#define ATOMIC_OP(op, type)                                                    \
  struct op##type##Operator : public Operator1<MachineType> {                  \
    op##type##Operator()                                                       \
        : Operator1<MachineType>(IrOpcode::k##op,                              \
                                 Operator::kNoDeopt | Operator::kNoThrow, #op, \
                                 3, 1, 1, 1, 1, 0, MachineType::type()) {}     \
  };                                                                           \
  op##type##Operator k##op##type;
#define ATOMIC_OP_LIST(type)      \
  ATOMIC_OP(AtomicExchange, type) \
  ATOMIC_OP(AtomicAdd, type)      \
  ATOMIC_OP(AtomicSub, type)      \
  ATOMIC_OP(AtomicAnd, type)      \
  ATOMIC_OP(AtomicOr, type)       \
  ATOMIC_OP(AtomicXor, type)
  ATOMIC_TYPE_LIST(ATOMIC_OP_LIST)
#undef ATOMIC_OP_LIST
#undef ATOMIC_OP

#define ATOMIC_COMPARE_EXCHANGE(Type)                                       \
  struct AtomicCompareExchange##Type##Operator                              \
      : public Operator1<MachineType> {                                     \
    AtomicCompareExchange##Type##Operator()                                 \
        : Operator1<MachineType>(IrOpcode::kAtomicCompareExchange,          \
                                 Operator::kNoDeopt | Operator::kNoThrow,   \
                                 "AtomicCompareExchange", 4, 1, 1, 1, 1, 0, \
                                 MachineType::Type()) {}                    \
  };                                                                        \
  AtomicCompareExchange##Type##Operator kAtomicCompareExchange##Type;
  ATOMIC_TYPE_LIST(ATOMIC_COMPARE_EXCHANGE)
#undef ATOMIC_COMPARE_EXCHANGE

  // The {BitcastWordToTagged} operator must not be marked as pure (especially
  // not idempotent), because otherwise the splitting logic in the Scheduler
  // might decide to split these operators, thus potentially creating live
  // ranges of allocation top across calls or other things that might allocate.
  // See https://bugs.chromium.org/p/v8/issues/detail?id=6059 for more details.
  struct BitcastWordToTaggedOperator : public Operator {
    BitcastWordToTaggedOperator()
        : Operator(IrOpcode::kBitcastWordToTagged,
                   Operator::kEliminatable | Operator::kNoWrite,
                   "BitcastWordToTagged", 1, 0, 0, 1, 0, 0) {}
  };
  BitcastWordToTaggedOperator kBitcastWordToTagged;

  struct DebugBreakOperator : public Operator {
    DebugBreakOperator()
        : Operator(IrOpcode::kDebugBreak, Operator::kNoThrow, "DebugBreak", 0,
                   0, 0, 0, 0, 0) {}
  };
  DebugBreakOperator kDebugBreak;

  struct UnsafePointerAddOperator final : public Operator {
    UnsafePointerAddOperator()
        : Operator(IrOpcode::kUnsafePointerAdd, Operator::kKontrol,
                   "UnsafePointerAdd", 2, 1, 1, 1, 1, 0) {}
  };
  UnsafePointerAddOperator kUnsafePointerAdd;
};

struct CommentOperator : public Operator1<const char*> {
  explicit CommentOperator(const char* msg)
      : Operator1<const char*>(IrOpcode::kComment, Operator::kNoThrow,
                               "Comment", 0, 0, 0, 0, 0, 0, msg) {}
};

static base::LazyInstance<MachineOperatorGlobalCache>::type kCache =
    LAZY_INSTANCE_INITIALIZER;

MachineOperatorBuilder::MachineOperatorBuilder(
    Zone* zone, MachineRepresentation word, Flags flags,
    AlignmentRequirements alignmentRequirements)
    : zone_(zone),
      cache_(kCache.Get()),
      word_(word),
      flags_(flags),
      alignment_requirements_(alignmentRequirements) {
  DCHECK(word == MachineRepresentation::kWord32 ||
         word == MachineRepresentation::kWord64);
}

const Operator* MachineOperatorBuilder::UnalignedLoad(
    UnalignedLoadRepresentation rep) {
#define LOAD(Type)                       \
  if (rep == MachineType::Type()) {      \
    return &cache_.kUnalignedLoad##Type; \
  }
  MACHINE_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::UnalignedStore(
    UnalignedStoreRepresentation rep) {
  switch (rep) {
#define STORE(kRep)                 \
  case MachineRepresentation::kRep: \
    return &cache_.kUnalignedStore##kRep;
    MACHINE_REPRESENTATION_LIST(STORE)
#undef STORE
    case MachineRepresentation::kBit:
    case MachineRepresentation::kSimd1x4:
    case MachineRepresentation::kSimd1x8:
    case MachineRepresentation::kSimd1x16:
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
  return nullptr;
}

#define PURE(Name, properties, value_input_count, control_input_count, \
             output_count)                                             \
  const Operator* MachineOperatorBuilder::Name() { return &cache_.k##Name; }
PURE_OP_LIST(PURE)
#undef PURE

#define PURE(Name, properties, value_input_count, control_input_count, \
             output_count)                                             \
  const OptionalOperator MachineOperatorBuilder::Name() {              \
    return OptionalOperator(flags_ & k##Name, &cache_.k##Name);        \
  }
PURE_OPTIONAL_OP_LIST(PURE)
#undef PURE

#define OVERFLOW_OP(Name, properties) \
  const Operator* MachineOperatorBuilder::Name() { return &cache_.k##Name; }
OVERFLOW_OP_LIST(OVERFLOW_OP)
#undef OVERFLOW_OP

const Operator* MachineOperatorBuilder::Load(LoadRepresentation rep) {
#define LOAD(Type)                  \
  if (rep == MachineType::Type()) { \
    return &cache_.kLoad##Type;     \
  }
    MACHINE_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::ProtectedLoad(LoadRepresentation rep) {
#define LOAD(Type)                       \
  if (rep == MachineType::Type()) {      \
    return &cache_.kProtectedLoad##Type; \
  }
  MACHINE_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::StackSlot(int size, int alignment) {
  DCHECK_LE(0, size);
  DCHECK(alignment == 0 || alignment == 4 || alignment == 8 || alignment == 16);
#define CASE_CACHED_SIZE(Size, Alignment)                          \
  if (size == Size && alignment == Alignment) {                    \
    return &cache_.kStackSlotOfSize##Size##OfAlignment##Alignment; \
  }

  STACK_SLOT_CACHED_SIZES_ALIGNMENTS_LIST(CASE_CACHED_SIZE)

#undef CASE_CACHED_SIZE
  return new (zone_) StackSlotOperator(size, alignment);
}

const Operator* MachineOperatorBuilder::StackSlot(MachineRepresentation rep,
                                                  int alignment) {
  return StackSlot(1 << ElementSizeLog2Of(rep), alignment);
}

const Operator* MachineOperatorBuilder::Store(StoreRepresentation store_rep) {
  switch (store_rep.representation()) {
#define STORE(kRep)                                         \
  case MachineRepresentation::kRep:                         \
    switch (store_rep.write_barrier_kind()) {               \
      case kNoWriteBarrier:                                 \
        return &cache_.k##Store##kRep##NoWriteBarrier;      \
      case kMapWriteBarrier:                                \
        return &cache_.k##Store##kRep##MapWriteBarrier;     \
      case kPointerWriteBarrier:                            \
        return &cache_.k##Store##kRep##PointerWriteBarrier; \
      case kFullWriteBarrier:                               \
        return &cache_.k##Store##kRep##FullWriteBarrier;    \
    }                                                       \
    break;
    MACHINE_REPRESENTATION_LIST(STORE)
#undef STORE
    case MachineRepresentation::kBit:
    case MachineRepresentation::kSimd1x4:
    case MachineRepresentation::kSimd1x8:
    case MachineRepresentation::kSimd1x16:
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::ProtectedStore(
    MachineRepresentation rep) {
  switch (rep) {
#define STORE(kRep)                       \
  case MachineRepresentation::kRep:       \
    return &cache_.kProtectedStore##kRep; \
    break;
    MACHINE_REPRESENTATION_LIST(STORE)
#undef STORE
    case MachineRepresentation::kBit:
    case MachineRepresentation::kSimd1x4:
    case MachineRepresentation::kSimd1x8:
    case MachineRepresentation::kSimd1x16:
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::UnsafePointerAdd() {
  return &cache_.kUnsafePointerAdd;
}

const Operator* MachineOperatorBuilder::BitcastWordToTagged() {
  return &cache_.kBitcastWordToTagged;
}

const Operator* MachineOperatorBuilder::DebugBreak() {
  return &cache_.kDebugBreak;
}

const Operator* MachineOperatorBuilder::Comment(const char* msg) {
  return new (zone_) CommentOperator(msg);
}

const Operator* MachineOperatorBuilder::CheckedLoad(
    CheckedLoadRepresentation rep) {
#define LOAD(Type)                     \
  if (rep == MachineType::Type()) {    \
    return &cache_.kCheckedLoad##Type; \
  }
    MACHINE_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
  return nullptr;
}


const Operator* MachineOperatorBuilder::CheckedStore(
    CheckedStoreRepresentation rep) {
  switch (rep) {
#define STORE(kRep)                 \
  case MachineRepresentation::kRep: \
    return &cache_.kCheckedStore##kRep;
    MACHINE_REPRESENTATION_LIST(STORE)
#undef STORE
    case MachineRepresentation::kBit:
    case MachineRepresentation::kSimd1x4:
    case MachineRepresentation::kSimd1x8:
    case MachineRepresentation::kSimd1x16:
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::AtomicLoad(LoadRepresentation rep) {
#define LOAD(Type)                    \
  if (rep == MachineType::Type()) {   \
    return &cache_.kAtomicLoad##Type; \
  }
  ATOMIC_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::AtomicStore(MachineRepresentation rep) {
#define STORE(kRep)                         \
  if (rep == MachineRepresentation::kRep) { \
    return &cache_.kAtomicStore##kRep;      \
  }
  ATOMIC_REPRESENTATION_LIST(STORE)
#undef STORE
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::AtomicExchange(MachineType rep) {
#define EXCHANGE(kRep)                    \
  if (rep == MachineType::kRep()) {       \
    return &cache_.kAtomicExchange##kRep; \
  }
  ATOMIC_TYPE_LIST(EXCHANGE)
#undef EXCHANGE
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::AtomicCompareExchange(MachineType rep) {
#define COMPARE_EXCHANGE(kRep)                   \
  if (rep == MachineType::kRep()) {              \
    return &cache_.kAtomicCompareExchange##kRep; \
  }
  ATOMIC_TYPE_LIST(COMPARE_EXCHANGE)
#undef COMPARE_EXCHANGE
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::AtomicAdd(MachineType rep) {
#define ADD(kRep)                    \
  if (rep == MachineType::kRep()) {  \
    return &cache_.kAtomicAdd##kRep; \
  }
  ATOMIC_TYPE_LIST(ADD)
#undef ADD
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::AtomicSub(MachineType rep) {
#define SUB(kRep)                    \
  if (rep == MachineType::kRep()) {  \
    return &cache_.kAtomicSub##kRep; \
  }
  ATOMIC_TYPE_LIST(SUB)
#undef SUB
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::AtomicAnd(MachineType rep) {
#define AND(kRep)                    \
  if (rep == MachineType::kRep()) {  \
    return &cache_.kAtomicAnd##kRep; \
  }
  ATOMIC_TYPE_LIST(AND)
#undef AND
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::AtomicOr(MachineType rep) {
#define OR(kRep)                    \
  if (rep == MachineType::kRep()) { \
    return &cache_.kAtomicOr##kRep; \
  }
  ATOMIC_TYPE_LIST(OR)
#undef OR
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::AtomicXor(MachineType rep) {
#define XOR(kRep)                    \
  if (rep == MachineType::kRep()) {  \
    return &cache_.kAtomicXor##kRep; \
  }
  ATOMIC_TYPE_LIST(XOR)
#undef XOR
  UNREACHABLE();
  return nullptr;
}

#define SIMD_LANE_OPS(Type, lane_count)                                     \
  const Operator* MachineOperatorBuilder::Type##ExtractLane(                \
      int32_t lane_index) {                                                 \
    DCHECK(0 <= lane_index && lane_index < lane_count);                     \
    return new (zone_)                                                      \
        Operator1<int32_t>(IrOpcode::k##Type##ExtractLane, Operator::kPure, \
                           "Extract lane", 1, 0, 0, 1, 0, 0, lane_index);   \
  }                                                                         \
  const Operator* MachineOperatorBuilder::Type##ReplaceLane(                \
      int32_t lane_index) {                                                 \
    DCHECK(0 <= lane_index && lane_index < lane_count);                     \
    return new (zone_)                                                      \
        Operator1<int32_t>(IrOpcode::k##Type##ReplaceLane, Operator::kPure, \
                           "Replace lane", 2, 0, 0, 1, 0, 0, lane_index);   \
  }
SIMD_LANE_OP_LIST(SIMD_LANE_OPS)
#undef SIMD_LANE_OPS

#define SIMD_SHIFT_OPS(format, bits)                                           \
  const Operator* MachineOperatorBuilder::I##format##Shl(int32_t shift) {      \
    DCHECK(0 <= shift && shift < bits);                                        \
    return new (zone_)                                                         \
        Operator1<int32_t>(IrOpcode::kI##format##Shl, Operator::kPure,         \
                           "Shift left", 1, 0, 0, 1, 0, 0, shift);             \
  }                                                                            \
  const Operator* MachineOperatorBuilder::I##format##ShrS(int32_t shift) {     \
    DCHECK(0 < shift && shift <= bits);                                        \
    return new (zone_)                                                         \
        Operator1<int32_t>(IrOpcode::kI##format##ShrS, Operator::kPure,        \
                           "Arithmetic shift right", 1, 0, 0, 1, 0, 0, shift); \
  }                                                                            \
  const Operator* MachineOperatorBuilder::I##format##ShrU(int32_t shift) {     \
    DCHECK(0 <= shift && shift < bits);                                        \
    return new (zone_)                                                         \
        Operator1<int32_t>(IrOpcode::kI##format##ShrU, Operator::kPure,        \
                           "Shift right", 1, 0, 0, 1, 0, 0, shift);            \
  }
SIMD_FORMAT_LIST(SIMD_SHIFT_OPS)
#undef SIMD_SHIFT_OPS

const Operator* MachineOperatorBuilder::S32x4Shuffle(uint8_t shuffle[16]) {
  uint8_t* array = zone_->NewArray<uint8_t>(4);
  memcpy(array, shuffle, 4);
  return new (zone_)
      Operator1<uint8_t*>(IrOpcode::kS32x4Shuffle, Operator::kPure, "Shuffle",
                          2, 0, 0, 1, 0, 0, array);
}

const Operator* MachineOperatorBuilder::S16x8Shuffle(uint8_t shuffle[16]) {
  uint8_t* array = zone_->NewArray<uint8_t>(8);
  memcpy(array, shuffle, 8);
  return new (zone_)
      Operator1<uint8_t*>(IrOpcode::kS16x8Shuffle, Operator::kPure, "Shuffle",
                          2, 0, 0, 1, 0, 0, array);
}

const Operator* MachineOperatorBuilder::S8x16Shuffle(uint8_t shuffle[16]) {
  uint8_t* array = zone_->NewArray<uint8_t>(16);
  memcpy(array, shuffle, 16);
  return new (zone_)
      Operator1<uint8_t*>(IrOpcode::kS8x16Shuffle, Operator::kPure, "Shuffle",
                          2, 0, 0, 1, 0, 0, array);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
