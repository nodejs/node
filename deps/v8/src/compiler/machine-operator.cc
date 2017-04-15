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

int StackSlotSizeOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kStackSlot, op->opcode());
  return OpParameter<int>(op);
}

MachineRepresentation AtomicStoreRepresentationOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kAtomicStore, op->opcode());
  return OpParameter<MachineRepresentation>(op);
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

#define PURE_OP_LIST(V)                                                    \
  PURE_BINARY_OP_LIST_32(V)                                                \
  PURE_BINARY_OP_LIST_64(V)                                                \
  V(Word32Clz, Operator::kNoProperties, 1, 0, 1)                           \
  V(Word64Clz, Operator::kNoProperties, 1, 0, 1)                           \
  V(BitcastTaggedToWord, Operator::kNoProperties, 1, 0, 1)                 \
  V(BitcastWordToTagged, Operator::kNoProperties, 1, 0, 1)                 \
  V(BitcastWordToTaggedSigned, Operator::kNoProperties, 1, 0, 1)           \
  V(TruncateFloat64ToWord32, Operator::kNoProperties, 1, 0, 1)             \
  V(ChangeFloat32ToFloat64, Operator::kNoProperties, 1, 0, 1)              \
  V(ChangeFloat64ToInt32, Operator::kNoProperties, 1, 0, 1)                \
  V(ChangeFloat64ToUint32, Operator::kNoProperties, 1, 0, 1)               \
  V(TruncateFloat64ToUint32, Operator::kNoProperties, 1, 0, 1)             \
  V(TruncateFloat32ToInt32, Operator::kNoProperties, 1, 0, 1)              \
  V(TruncateFloat32ToUint32, Operator::kNoProperties, 1, 0, 1)             \
  V(TryTruncateFloat32ToInt64, Operator::kNoProperties, 1, 0, 2)           \
  V(TryTruncateFloat64ToInt64, Operator::kNoProperties, 1, 0, 2)           \
  V(TryTruncateFloat32ToUint64, Operator::kNoProperties, 1, 0, 2)          \
  V(TryTruncateFloat64ToUint64, Operator::kNoProperties, 1, 0, 2)          \
  V(ChangeInt32ToFloat64, Operator::kNoProperties, 1, 0, 1)                \
  V(Float64SilenceNaN, Operator::kNoProperties, 1, 0, 1)                   \
  V(RoundFloat64ToInt32, Operator::kNoProperties, 1, 0, 1)                 \
  V(RoundInt32ToFloat32, Operator::kNoProperties, 1, 0, 1)                 \
  V(RoundInt64ToFloat32, Operator::kNoProperties, 1, 0, 1)                 \
  V(RoundInt64ToFloat64, Operator::kNoProperties, 1, 0, 1)                 \
  V(RoundUint32ToFloat32, Operator::kNoProperties, 1, 0, 1)                \
  V(RoundUint64ToFloat32, Operator::kNoProperties, 1, 0, 1)                \
  V(RoundUint64ToFloat64, Operator::kNoProperties, 1, 0, 1)                \
  V(ChangeInt32ToInt64, Operator::kNoProperties, 1, 0, 1)                  \
  V(ChangeUint32ToFloat64, Operator::kNoProperties, 1, 0, 1)               \
  V(ChangeUint32ToUint64, Operator::kNoProperties, 1, 0, 1)                \
  V(TruncateFloat64ToFloat32, Operator::kNoProperties, 1, 0, 1)            \
  V(TruncateInt64ToInt32, Operator::kNoProperties, 1, 0, 1)                \
  V(BitcastFloat32ToInt32, Operator::kNoProperties, 1, 0, 1)               \
  V(BitcastFloat64ToInt64, Operator::kNoProperties, 1, 0, 1)               \
  V(BitcastInt32ToFloat32, Operator::kNoProperties, 1, 0, 1)               \
  V(BitcastInt64ToFloat64, Operator::kNoProperties, 1, 0, 1)               \
  V(Float32Abs, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float32Add, Operator::kCommutative, 2, 0, 1)                           \
  V(Float32Sub, Operator::kNoProperties, 2, 0, 1)                          \
  V(Float32Mul, Operator::kCommutative, 2, 0, 1)                           \
  V(Float32Div, Operator::kNoProperties, 2, 0, 1)                          \
  V(Float32Neg, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float32Sqrt, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float32Max, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Float32Min, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Float64Abs, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Acos, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Acosh, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Asin, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Asinh, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Atan, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Atan2, Operator::kNoProperties, 2, 0, 1)                        \
  V(Float64Atanh, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Cbrt, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Cos, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Cosh, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Exp, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Expm1, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Log, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Log1p, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Log2, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Log10, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Max, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Float64Min, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Float64Neg, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Add, Operator::kCommutative, 2, 0, 1)                           \
  V(Float64Sub, Operator::kNoProperties, 2, 0, 1)                          \
  V(Float64Mul, Operator::kCommutative, 2, 0, 1)                           \
  V(Float64Div, Operator::kNoProperties, 2, 0, 1)                          \
  V(Float64Mod, Operator::kNoProperties, 2, 0, 1)                          \
  V(Float64Pow, Operator::kNoProperties, 2, 0, 1)                          \
  V(Float64Sin, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Sinh, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Sqrt, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Tan, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Tanh, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float32Equal, Operator::kCommutative, 2, 0, 1)                         \
  V(Float32LessThan, Operator::kNoProperties, 2, 0, 1)                     \
  V(Float32LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)              \
  V(Float64Equal, Operator::kCommutative, 2, 0, 1)                         \
  V(Float64LessThan, Operator::kNoProperties, 2, 0, 1)                     \
  V(Float64LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)              \
  V(Float64ExtractLowWord32, Operator::kNoProperties, 1, 0, 1)             \
  V(Float64ExtractHighWord32, Operator::kNoProperties, 1, 0, 1)            \
  V(Float64InsertLowWord32, Operator::kNoProperties, 2, 0, 1)              \
  V(Float64InsertHighWord32, Operator::kNoProperties, 2, 0, 1)             \
  V(LoadStackPointer, Operator::kNoProperties, 0, 0, 1)                    \
  V(LoadFramePointer, Operator::kNoProperties, 0, 0, 1)                    \
  V(LoadParentFramePointer, Operator::kNoProperties, 0, 0, 1)              \
  V(Int32PairAdd, Operator::kNoProperties, 4, 0, 2)                        \
  V(Int32PairSub, Operator::kNoProperties, 4, 0, 2)                        \
  V(Int32PairMul, Operator::kNoProperties, 4, 0, 2)                        \
  V(Word32PairShl, Operator::kNoProperties, 3, 0, 2)                       \
  V(Word32PairShr, Operator::kNoProperties, 3, 0, 2)                       \
  V(Word32PairSar, Operator::kNoProperties, 3, 0, 2)                       \
  V(CreateFloat32x4, Operator::kNoProperties, 4, 0, 1)                     \
  V(Float32x4ExtractLane, Operator::kNoProperties, 2, 0, 1)                \
  V(Float32x4ReplaceLane, Operator::kNoProperties, 3, 0, 1)                \
  V(Float32x4Abs, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float32x4Neg, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float32x4Sqrt, Operator::kNoProperties, 1, 0, 1)                       \
  V(Float32x4RecipApprox, Operator::kNoProperties, 1, 0, 1)                \
  V(Float32x4RecipSqrtApprox, Operator::kNoProperties, 1, 0, 1)            \
  V(Float32x4Add, Operator::kCommutative, 2, 0, 1)                         \
  V(Float32x4Sub, Operator::kNoProperties, 2, 0, 1)                        \
  V(Float32x4Mul, Operator::kCommutative, 2, 0, 1)                         \
  V(Float32x4Div, Operator::kNoProperties, 2, 0, 1)                        \
  V(Float32x4Min, Operator::kCommutative, 2, 0, 1)                         \
  V(Float32x4Max, Operator::kCommutative, 2, 0, 1)                         \
  V(Float32x4MinNum, Operator::kCommutative, 2, 0, 1)                      \
  V(Float32x4MaxNum, Operator::kCommutative, 2, 0, 1)                      \
  V(Float32x4Equal, Operator::kCommutative, 2, 0, 1)                       \
  V(Float32x4NotEqual, Operator::kCommutative, 2, 0, 1)                    \
  V(Float32x4LessThan, Operator::kNoProperties, 2, 0, 1)                   \
  V(Float32x4LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)            \
  V(Float32x4GreaterThan, Operator::kNoProperties, 2, 0, 1)                \
  V(Float32x4GreaterThanOrEqual, Operator::kNoProperties, 2, 0, 1)         \
  V(Float32x4FromInt32x4, Operator::kNoProperties, 1, 0, 1)                \
  V(Float32x4FromUint32x4, Operator::kNoProperties, 1, 0, 1)               \
  V(CreateInt32x4, Operator::kNoProperties, 4, 0, 1)                       \
  V(Int32x4ExtractLane, Operator::kNoProperties, 2, 0, 1)                  \
  V(Int32x4ReplaceLane, Operator::kNoProperties, 3, 0, 1)                  \
  V(Int32x4Neg, Operator::kNoProperties, 1, 0, 1)                          \
  V(Int32x4Add, Operator::kCommutative, 2, 0, 1)                           \
  V(Int32x4Sub, Operator::kNoProperties, 2, 0, 1)                          \
  V(Int32x4Mul, Operator::kCommutative, 2, 0, 1)                           \
  V(Int32x4Min, Operator::kCommutative, 2, 0, 1)                           \
  V(Int32x4Max, Operator::kCommutative, 2, 0, 1)                           \
  V(Int32x4ShiftLeftByScalar, Operator::kNoProperties, 2, 0, 1)            \
  V(Int32x4ShiftRightByScalar, Operator::kNoProperties, 2, 0, 1)           \
  V(Int32x4Equal, Operator::kCommutative, 2, 0, 1)                         \
  V(Int32x4NotEqual, Operator::kCommutative, 2, 0, 1)                      \
  V(Int32x4LessThan, Operator::kNoProperties, 2, 0, 1)                     \
  V(Int32x4LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)              \
  V(Int32x4GreaterThan, Operator::kNoProperties, 2, 0, 1)                  \
  V(Int32x4GreaterThanOrEqual, Operator::kNoProperties, 2, 0, 1)           \
  V(Int32x4FromFloat32x4, Operator::kNoProperties, 1, 0, 1)                \
  V(Uint32x4Min, Operator::kCommutative, 2, 0, 1)                          \
  V(Uint32x4Max, Operator::kCommutative, 2, 0, 1)                          \
  V(Uint32x4ShiftLeftByScalar, Operator::kNoProperties, 2, 0, 1)           \
  V(Uint32x4ShiftRightByScalar, Operator::kNoProperties, 2, 0, 1)          \
  V(Uint32x4LessThan, Operator::kNoProperties, 2, 0, 1)                    \
  V(Uint32x4LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)             \
  V(Uint32x4GreaterThan, Operator::kNoProperties, 2, 0, 1)                 \
  V(Uint32x4GreaterThanOrEqual, Operator::kNoProperties, 2, 0, 1)          \
  V(Uint32x4FromFloat32x4, Operator::kNoProperties, 1, 0, 1)               \
  V(CreateBool32x4, Operator::kNoProperties, 4, 0, 1)                      \
  V(Bool32x4ExtractLane, Operator::kNoProperties, 2, 0, 1)                 \
  V(Bool32x4ReplaceLane, Operator::kNoProperties, 3, 0, 1)                 \
  V(Bool32x4And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Bool32x4Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Bool32x4Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Bool32x4Not, Operator::kNoProperties, 1, 0, 1)                         \
  V(Bool32x4AnyTrue, Operator::kNoProperties, 1, 0, 1)                     \
  V(Bool32x4AllTrue, Operator::kNoProperties, 1, 0, 1)                     \
  V(Bool32x4Swizzle, Operator::kNoProperties, 5, 0, 1)                     \
  V(Bool32x4Shuffle, Operator::kNoProperties, 6, 0, 1)                     \
  V(Bool32x4Equal, Operator::kCommutative, 2, 0, 1)                        \
  V(Bool32x4NotEqual, Operator::kCommutative, 2, 0, 1)                     \
  V(CreateInt16x8, Operator::kNoProperties, 8, 0, 1)                       \
  V(Int16x8ExtractLane, Operator::kNoProperties, 2, 0, 1)                  \
  V(Int16x8ReplaceLane, Operator::kNoProperties, 3, 0, 1)                  \
  V(Int16x8Neg, Operator::kNoProperties, 1, 0, 1)                          \
  V(Int16x8Add, Operator::kCommutative, 2, 0, 1)                           \
  V(Int16x8AddSaturate, Operator::kCommutative, 2, 0, 1)                   \
  V(Int16x8Sub, Operator::kNoProperties, 2, 0, 1)                          \
  V(Int16x8SubSaturate, Operator::kNoProperties, 2, 0, 1)                  \
  V(Int16x8Mul, Operator::kCommutative, 2, 0, 1)                           \
  V(Int16x8Min, Operator::kCommutative, 2, 0, 1)                           \
  V(Int16x8Max, Operator::kCommutative, 2, 0, 1)                           \
  V(Int16x8ShiftLeftByScalar, Operator::kNoProperties, 2, 0, 1)            \
  V(Int16x8ShiftRightByScalar, Operator::kNoProperties, 2, 0, 1)           \
  V(Int16x8Equal, Operator::kCommutative, 2, 0, 1)                         \
  V(Int16x8NotEqual, Operator::kCommutative, 2, 0, 1)                      \
  V(Int16x8LessThan, Operator::kNoProperties, 2, 0, 1)                     \
  V(Int16x8LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)              \
  V(Int16x8GreaterThan, Operator::kNoProperties, 2, 0, 1)                  \
  V(Int16x8GreaterThanOrEqual, Operator::kNoProperties, 2, 0, 1)           \
  V(Int16x8Select, Operator::kNoProperties, 3, 0, 1)                       \
  V(Int16x8Swizzle, Operator::kNoProperties, 9, 0, 1)                      \
  V(Int16x8Shuffle, Operator::kNoProperties, 10, 0, 1)                     \
  V(Uint16x8AddSaturate, Operator::kCommutative, 2, 0, 1)                  \
  V(Uint16x8SubSaturate, Operator::kNoProperties, 2, 0, 1)                 \
  V(Uint16x8Min, Operator::kCommutative, 2, 0, 1)                          \
  V(Uint16x8Max, Operator::kCommutative, 2, 0, 1)                          \
  V(Uint16x8ShiftLeftByScalar, Operator::kNoProperties, 2, 0, 1)           \
  V(Uint16x8ShiftRightByScalar, Operator::kNoProperties, 2, 0, 1)          \
  V(Uint16x8LessThan, Operator::kNoProperties, 2, 0, 1)                    \
  V(Uint16x8LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)             \
  V(Uint16x8GreaterThan, Operator::kNoProperties, 2, 0, 1)                 \
  V(Uint16x8GreaterThanOrEqual, Operator::kNoProperties, 2, 0, 1)          \
  V(CreateBool16x8, Operator::kNoProperties, 8, 0, 1)                      \
  V(Bool16x8ExtractLane, Operator::kNoProperties, 2, 0, 1)                 \
  V(Bool16x8ReplaceLane, Operator::kNoProperties, 3, 0, 1)                 \
  V(Bool16x8And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Bool16x8Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Bool16x8Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Bool16x8Not, Operator::kNoProperties, 1, 0, 1)                         \
  V(Bool16x8AnyTrue, Operator::kNoProperties, 1, 0, 1)                     \
  V(Bool16x8AllTrue, Operator::kNoProperties, 1, 0, 1)                     \
  V(Bool16x8Swizzle, Operator::kNoProperties, 9, 0, 1)                     \
  V(Bool16x8Shuffle, Operator::kNoProperties, 10, 0, 1)                    \
  V(Bool16x8Equal, Operator::kCommutative, 2, 0, 1)                        \
  V(Bool16x8NotEqual, Operator::kCommutative, 2, 0, 1)                     \
  V(CreateInt8x16, Operator::kNoProperties, 16, 0, 1)                      \
  V(Int8x16ExtractLane, Operator::kNoProperties, 2, 0, 1)                  \
  V(Int8x16ReplaceLane, Operator::kNoProperties, 3, 0, 1)                  \
  V(Int8x16Neg, Operator::kNoProperties, 1, 0, 1)                          \
  V(Int8x16Add, Operator::kCommutative, 2, 0, 1)                           \
  V(Int8x16AddSaturate, Operator::kCommutative, 2, 0, 1)                   \
  V(Int8x16Sub, Operator::kNoProperties, 2, 0, 1)                          \
  V(Int8x16SubSaturate, Operator::kNoProperties, 2, 0, 1)                  \
  V(Int8x16Mul, Operator::kCommutative, 2, 0, 1)                           \
  V(Int8x16Min, Operator::kCommutative, 2, 0, 1)                           \
  V(Int8x16Max, Operator::kCommutative, 2, 0, 1)                           \
  V(Int8x16ShiftLeftByScalar, Operator::kNoProperties, 2, 0, 1)            \
  V(Int8x16ShiftRightByScalar, Operator::kNoProperties, 2, 0, 1)           \
  V(Int8x16Equal, Operator::kCommutative, 2, 0, 1)                         \
  V(Int8x16NotEqual, Operator::kCommutative, 2, 0, 1)                      \
  V(Int8x16LessThan, Operator::kNoProperties, 2, 0, 1)                     \
  V(Int8x16LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)              \
  V(Int8x16GreaterThan, Operator::kNoProperties, 2, 0, 1)                  \
  V(Int8x16GreaterThanOrEqual, Operator::kNoProperties, 2, 0, 1)           \
  V(Int8x16Select, Operator::kNoProperties, 3, 0, 1)                       \
  V(Int8x16Swizzle, Operator::kNoProperties, 17, 0, 1)                     \
  V(Int8x16Shuffle, Operator::kNoProperties, 18, 0, 1)                     \
  V(Uint8x16AddSaturate, Operator::kCommutative, 2, 0, 1)                  \
  V(Uint8x16SubSaturate, Operator::kNoProperties, 2, 0, 1)                 \
  V(Uint8x16Min, Operator::kCommutative, 2, 0, 1)                          \
  V(Uint8x16Max, Operator::kCommutative, 2, 0, 1)                          \
  V(Uint8x16ShiftLeftByScalar, Operator::kNoProperties, 2, 0, 1)           \
  V(Uint8x16ShiftRightByScalar, Operator::kNoProperties, 2, 0, 1)          \
  V(Uint8x16LessThan, Operator::kNoProperties, 2, 0, 1)                    \
  V(Uint8x16LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)             \
  V(Uint8x16GreaterThan, Operator::kNoProperties, 2, 0, 1)                 \
  V(Uint8x16GreaterThanOrEqual, Operator::kNoProperties, 2, 0, 1)          \
  V(CreateBool8x16, Operator::kNoProperties, 16, 0, 1)                     \
  V(Bool8x16ExtractLane, Operator::kNoProperties, 2, 0, 1)                 \
  V(Bool8x16ReplaceLane, Operator::kNoProperties, 3, 0, 1)                 \
  V(Bool8x16And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Bool8x16Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Bool8x16Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Bool8x16Not, Operator::kNoProperties, 1, 0, 1)                         \
  V(Bool8x16AnyTrue, Operator::kNoProperties, 1, 0, 1)                     \
  V(Bool8x16AllTrue, Operator::kNoProperties, 1, 0, 1)                     \
  V(Bool8x16Swizzle, Operator::kNoProperties, 17, 0, 1)                    \
  V(Bool8x16Shuffle, Operator::kNoProperties, 18, 0, 1)                    \
  V(Bool8x16Equal, Operator::kCommutative, 2, 0, 1)                        \
  V(Bool8x16NotEqual, Operator::kCommutative, 2, 0, 1)                     \
  V(Simd128Load, Operator::kNoProperties, 2, 0, 1)                         \
  V(Simd128Load1, Operator::kNoProperties, 2, 0, 1)                        \
  V(Simd128Load2, Operator::kNoProperties, 2, 0, 1)                        \
  V(Simd128Load3, Operator::kNoProperties, 2, 0, 1)                        \
  V(Simd128Store, Operator::kNoProperties, 3, 0, 1)                        \
  V(Simd128Store1, Operator::kNoProperties, 3, 0, 1)                       \
  V(Simd128Store2, Operator::kNoProperties, 3, 0, 1)                       \
  V(Simd128Store3, Operator::kNoProperties, 3, 0, 1)                       \
  V(Simd128And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Simd128Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)   \
  V(Simd128Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Simd128Not, Operator::kNoProperties, 1, 0, 1)                          \
  V(Simd32x4Select, Operator::kNoProperties, 3, 0, 1)                      \
  V(Simd32x4Swizzle, Operator::kNoProperties, 5, 0, 1)                     \
  V(Simd32x4Shuffle, Operator::kNoProperties, 6, 0, 1)

#define PURE_OPTIONAL_OP_LIST(V)                            \
  V(Word32Ctz, Operator::kNoProperties, 1, 0, 1)            \
  V(Word64Ctz, Operator::kNoProperties, 1, 0, 1)            \
  V(Word32ReverseBits, Operator::kNoProperties, 1, 0, 1)    \
  V(Word64ReverseBits, Operator::kNoProperties, 1, 0, 1)    \
  V(Word32ReverseBytes, Operator::kNoProperties, 1, 0, 1)   \
  V(Word64ReverseBytes, Operator::kNoProperties, 1, 0, 1)   \
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

#define STACK_SLOT_CACHED_SIZES_LIST(V) V(4) V(8) V(16)

struct StackSlotOperator : public Operator1<int> {
  explicit StackSlotOperator(int size)
      : Operator1<int>(IrOpcode::kStackSlot,
                       Operator::kNoDeopt | Operator::kNoThrow, "StackSlot", 0,
                       0, 0, 1, 0, 0, size) {}
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

#define STACKSLOT(Size)                                                     \
  struct StackSlotOfSize##Size##Operator final : public StackSlotOperator { \
    StackSlotOfSize##Size##Operator() : StackSlotOperator(Size) {}          \
  };                                                                        \
  StackSlotOfSize##Size##Operator kStackSlotSize##Size;
  STACK_SLOT_CACHED_SIZES_LIST(STACKSLOT)
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

const Operator* MachineOperatorBuilder::StackSlot(int size) {
  DCHECK_LE(0, size);
#define CASE_CACHED_SIZE(Size) \
  case Size:                   \
    return &cache_.kStackSlotSize##Size;
  switch (size) {
    STACK_SLOT_CACHED_SIZES_LIST(CASE_CACHED_SIZE);
    default:
      return new (zone_) StackSlotOperator(size);
  }
#undef CASE_CACHED_SIZE
}

const Operator* MachineOperatorBuilder::StackSlot(MachineRepresentation rep) {
  return StackSlot(1 << ElementSizeLog2Of(rep));
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
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
  return nullptr;
}

const Operator* MachineOperatorBuilder::UnsafePointerAdd() {
  return &cache_.kUnsafePointerAdd;
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8
