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
         IrOpcode::kWord32AtomicLoad == op->opcode() ||
         IrOpcode::kWord64AtomicLoad == op->opcode() ||
         IrOpcode::kWord32AtomicPairLoad == op->opcode() ||
         IrOpcode::kPoisonedLoad == op->opcode() ||
         IrOpcode::kUnalignedLoad == op->opcode());
  return OpParameter<LoadRepresentation>(op);
}


StoreRepresentation const& StoreRepresentationOf(Operator const* op) {
  DCHECK(IrOpcode::kStore == op->opcode() ||
         IrOpcode::kProtectedStore == op->opcode());
  return OpParameter<StoreRepresentation>(op);
}

UnalignedStoreRepresentation const& UnalignedStoreRepresentationOf(
    Operator const* op) {
  DCHECK_EQ(IrOpcode::kUnalignedStore, op->opcode());
  return OpParameter<UnalignedStoreRepresentation>(op);
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
  DCHECK(IrOpcode::kWord32AtomicStore == op->opcode() ||
         IrOpcode::kWord64AtomicStore == op->opcode());
  return OpParameter<MachineRepresentation>(op);
}

MachineType AtomicOpType(Operator const* op) {
  return OpParameter<MachineType>(op);
}

// The format is:
// V(Name, properties, value_input_count, control_input_count, output_count)
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

// The format is:
// V(Name, properties, value_input_count, control_input_count, output_count)
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

// The format is:
// V(Name, properties, value_input_count, control_input_count, output_count)
#define MACHINE_PURE_OP_LIST(V)                                               \
  PURE_BINARY_OP_LIST_32(V)                                                   \
  PURE_BINARY_OP_LIST_64(V)                                                   \
  V(Word32Clz, Operator::kNoProperties, 1, 0, 1)                              \
  V(Word64Clz, Operator::kNoProperties, 1, 0, 1)                              \
  V(Word32ReverseBytes, Operator::kNoProperties, 1, 0, 1)                     \
  V(Word64ReverseBytes, Operator::kNoProperties, 1, 0, 1)                     \
  V(Simd128ReverseBytes, Operator::kNoProperties, 1, 0, 1)                    \
  V(BitcastTaggedToWordForTagAndSmiBits, Operator::kNoProperties, 1, 0, 1)    \
  V(BitcastWordToTaggedSigned, Operator::kNoProperties, 1, 0, 1)              \
  V(BitcastWord32ToCompressedSigned, Operator::kNoProperties, 1, 0, 1)        \
  V(BitcastCompressedSignedToWord32, Operator::kNoProperties, 1, 0, 1)        \
  V(TruncateFloat64ToWord32, Operator::kNoProperties, 1, 0, 1)                \
  V(ChangeFloat32ToFloat64, Operator::kNoProperties, 1, 0, 1)                 \
  V(ChangeFloat64ToInt32, Operator::kNoProperties, 1, 0, 1)                   \
  V(ChangeFloat64ToInt64, Operator::kNoProperties, 1, 0, 1)                   \
  V(ChangeFloat64ToUint32, Operator::kNoProperties, 1, 0, 1)                  \
  V(ChangeFloat64ToUint64, Operator::kNoProperties, 1, 0, 1)                  \
  V(TruncateFloat64ToInt64, Operator::kNoProperties, 1, 0, 1)                 \
  V(TruncateFloat64ToUint32, Operator::kNoProperties, 1, 0, 1)                \
  V(TruncateFloat32ToInt32, Operator::kNoProperties, 1, 0, 1)                 \
  V(TruncateFloat32ToUint32, Operator::kNoProperties, 1, 0, 1)                \
  V(TryTruncateFloat32ToInt64, Operator::kNoProperties, 1, 0, 2)              \
  V(TryTruncateFloat64ToInt64, Operator::kNoProperties, 1, 0, 2)              \
  V(TryTruncateFloat32ToUint64, Operator::kNoProperties, 1, 0, 2)             \
  V(TryTruncateFloat64ToUint64, Operator::kNoProperties, 1, 0, 2)             \
  V(ChangeInt32ToFloat64, Operator::kNoProperties, 1, 0, 1)                   \
  V(ChangeInt64ToFloat64, Operator::kNoProperties, 1, 0, 1)                   \
  V(Float64SilenceNaN, Operator::kNoProperties, 1, 0, 1)                      \
  V(RoundFloat64ToInt32, Operator::kNoProperties, 1, 0, 1)                    \
  V(RoundInt32ToFloat32, Operator::kNoProperties, 1, 0, 1)                    \
  V(RoundInt64ToFloat32, Operator::kNoProperties, 1, 0, 1)                    \
  V(RoundInt64ToFloat64, Operator::kNoProperties, 1, 0, 1)                    \
  V(RoundUint32ToFloat32, Operator::kNoProperties, 1, 0, 1)                   \
  V(RoundUint64ToFloat32, Operator::kNoProperties, 1, 0, 1)                   \
  V(RoundUint64ToFloat64, Operator::kNoProperties, 1, 0, 1)                   \
  V(ChangeInt32ToInt64, Operator::kNoProperties, 1, 0, 1)                     \
  V(ChangeUint32ToFloat64, Operator::kNoProperties, 1, 0, 1)                  \
  V(ChangeUint32ToUint64, Operator::kNoProperties, 1, 0, 1)                   \
  V(ChangeTaggedToCompressed, Operator::kNoProperties, 1, 0, 1)               \
  V(ChangeTaggedPointerToCompressedPointer, Operator::kNoProperties, 1, 0, 1) \
  V(ChangeTaggedSignedToCompressedSigned, Operator::kNoProperties, 1, 0, 1)   \
  V(ChangeCompressedToTagged, Operator::kNoProperties, 1, 0, 1)               \
  V(ChangeCompressedPointerToTaggedPointer, Operator::kNoProperties, 1, 0, 1) \
  V(ChangeCompressedSignedToTaggedSigned, Operator::kNoProperties, 1, 0, 1)   \
  V(TruncateFloat64ToFloat32, Operator::kNoProperties, 1, 0, 1)               \
  V(TruncateInt64ToInt32, Operator::kNoProperties, 1, 0, 1)                   \
  V(BitcastFloat32ToInt32, Operator::kNoProperties, 1, 0, 1)                  \
  V(BitcastFloat64ToInt64, Operator::kNoProperties, 1, 0, 1)                  \
  V(BitcastInt32ToFloat32, Operator::kNoProperties, 1, 0, 1)                  \
  V(BitcastInt64ToFloat64, Operator::kNoProperties, 1, 0, 1)                  \
  V(SignExtendWord8ToInt32, Operator::kNoProperties, 1, 0, 1)                 \
  V(SignExtendWord16ToInt32, Operator::kNoProperties, 1, 0, 1)                \
  V(SignExtendWord8ToInt64, Operator::kNoProperties, 1, 0, 1)                 \
  V(SignExtendWord16ToInt64, Operator::kNoProperties, 1, 0, 1)                \
  V(SignExtendWord32ToInt64, Operator::kNoProperties, 1, 0, 1)                \
  V(Float32Abs, Operator::kNoProperties, 1, 0, 1)                             \
  V(Float32Add, Operator::kCommutative, 2, 0, 1)                              \
  V(Float32Sub, Operator::kNoProperties, 2, 0, 1)                             \
  V(Float32Mul, Operator::kCommutative, 2, 0, 1)                              \
  V(Float32Div, Operator::kNoProperties, 2, 0, 1)                             \
  V(Float32Neg, Operator::kNoProperties, 1, 0, 1)                             \
  V(Float32Sqrt, Operator::kNoProperties, 1, 0, 1)                            \
  V(Float32Max, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(Float32Min, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(Float64Abs, Operator::kNoProperties, 1, 0, 1)                             \
  V(Float64Acos, Operator::kNoProperties, 1, 0, 1)                            \
  V(Float64Acosh, Operator::kNoProperties, 1, 0, 1)                           \
  V(Float64Asin, Operator::kNoProperties, 1, 0, 1)                            \
  V(Float64Asinh, Operator::kNoProperties, 1, 0, 1)                           \
  V(Float64Atan, Operator::kNoProperties, 1, 0, 1)                            \
  V(Float64Atan2, Operator::kNoProperties, 2, 0, 1)                           \
  V(Float64Atanh, Operator::kNoProperties, 1, 0, 1)                           \
  V(Float64Cbrt, Operator::kNoProperties, 1, 0, 1)                            \
  V(Float64Cos, Operator::kNoProperties, 1, 0, 1)                             \
  V(Float64Cosh, Operator::kNoProperties, 1, 0, 1)                            \
  V(Float64Exp, Operator::kNoProperties, 1, 0, 1)                             \
  V(Float64Expm1, Operator::kNoProperties, 1, 0, 1)                           \
  V(Float64Log, Operator::kNoProperties, 1, 0, 1)                             \
  V(Float64Log1p, Operator::kNoProperties, 1, 0, 1)                           \
  V(Float64Log2, Operator::kNoProperties, 1, 0, 1)                            \
  V(Float64Log10, Operator::kNoProperties, 1, 0, 1)                           \
  V(Float64Max, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(Float64Min, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(Float64Neg, Operator::kNoProperties, 1, 0, 1)                             \
  V(Float64Add, Operator::kCommutative, 2, 0, 1)                              \
  V(Float64Sub, Operator::kNoProperties, 2, 0, 1)                             \
  V(Float64Mul, Operator::kCommutative, 2, 0, 1)                              \
  V(Float64Div, Operator::kNoProperties, 2, 0, 1)                             \
  V(Float64Mod, Operator::kNoProperties, 2, 0, 1)                             \
  V(Float64Pow, Operator::kNoProperties, 2, 0, 1)                             \
  V(Float64Sin, Operator::kNoProperties, 1, 0, 1)                             \
  V(Float64Sinh, Operator::kNoProperties, 1, 0, 1)                            \
  V(Float64Sqrt, Operator::kNoProperties, 1, 0, 1)                            \
  V(Float64Tan, Operator::kNoProperties, 1, 0, 1)                             \
  V(Float64Tanh, Operator::kNoProperties, 1, 0, 1)                            \
  V(Float32Equal, Operator::kCommutative, 2, 0, 1)                            \
  V(Float32LessThan, Operator::kNoProperties, 2, 0, 1)                        \
  V(Float32LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)                 \
  V(Float64Equal, Operator::kCommutative, 2, 0, 1)                            \
  V(Float64LessThan, Operator::kNoProperties, 2, 0, 1)                        \
  V(Float64LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)                 \
  V(Float64ExtractLowWord32, Operator::kNoProperties, 1, 0, 1)                \
  V(Float64ExtractHighWord32, Operator::kNoProperties, 1, 0, 1)               \
  V(Float64InsertLowWord32, Operator::kNoProperties, 2, 0, 1)                 \
  V(Float64InsertHighWord32, Operator::kNoProperties, 2, 0, 1)                \
  V(LoadFramePointer, Operator::kNoProperties, 0, 0, 1)                       \
  V(LoadParentFramePointer, Operator::kNoProperties, 0, 0, 1)                 \
  V(Int32PairAdd, Operator::kNoProperties, 4, 0, 2)                           \
  V(Int32PairSub, Operator::kNoProperties, 4, 0, 2)                           \
  V(Int32PairMul, Operator::kNoProperties, 4, 0, 2)                           \
  V(Word32PairShl, Operator::kNoProperties, 3, 0, 2)                          \
  V(Word32PairShr, Operator::kNoProperties, 3, 0, 2)                          \
  V(Word32PairSar, Operator::kNoProperties, 3, 0, 2)                          \
  V(F64x2Splat, Operator::kNoProperties, 1, 0, 1)                             \
  V(F64x2Abs, Operator::kNoProperties, 1, 0, 1)                               \
  V(F64x2Neg, Operator::kNoProperties, 1, 0, 1)                               \
  V(F64x2Sqrt, Operator::kNoProperties, 1, 0, 1)                              \
  V(F64x2Add, Operator::kCommutative, 2, 0, 1)                                \
  V(F64x2Sub, Operator::kNoProperties, 2, 0, 1)                               \
  V(F64x2Mul, Operator::kCommutative, 2, 0, 1)                                \
  V(F64x2Div, Operator::kNoProperties, 2, 0, 1)                               \
  V(F64x2Min, Operator::kCommutative, 2, 0, 1)                                \
  V(F64x2Max, Operator::kCommutative, 2, 0, 1)                                \
  V(F64x2Eq, Operator::kCommutative, 2, 0, 1)                                 \
  V(F64x2Ne, Operator::kCommutative, 2, 0, 1)                                 \
  V(F64x2Lt, Operator::kNoProperties, 2, 0, 1)                                \
  V(F64x2Le, Operator::kNoProperties, 2, 0, 1)                                \
  V(F64x2Qfma, Operator::kNoProperties, 3, 0, 1)                              \
  V(F64x2Qfms, Operator::kNoProperties, 3, 0, 1)                              \
  V(F32x4Splat, Operator::kNoProperties, 1, 0, 1)                             \
  V(F32x4SConvertI32x4, Operator::kNoProperties, 1, 0, 1)                     \
  V(F32x4UConvertI32x4, Operator::kNoProperties, 1, 0, 1)                     \
  V(F32x4Abs, Operator::kNoProperties, 1, 0, 1)                               \
  V(F32x4Neg, Operator::kNoProperties, 1, 0, 1)                               \
  V(F32x4Sqrt, Operator::kNoProperties, 1, 0, 1)                              \
  V(F32x4RecipApprox, Operator::kNoProperties, 1, 0, 1)                       \
  V(F32x4RecipSqrtApprox, Operator::kNoProperties, 1, 0, 1)                   \
  V(F32x4Add, Operator::kCommutative, 2, 0, 1)                                \
  V(F32x4AddHoriz, Operator::kNoProperties, 2, 0, 1)                          \
  V(F32x4Sub, Operator::kNoProperties, 2, 0, 1)                               \
  V(F32x4Mul, Operator::kCommutative, 2, 0, 1)                                \
  V(F32x4Div, Operator::kNoProperties, 2, 0, 1)                               \
  V(F32x4Min, Operator::kCommutative, 2, 0, 1)                                \
  V(F32x4Max, Operator::kCommutative, 2, 0, 1)                                \
  V(F32x4Eq, Operator::kCommutative, 2, 0, 1)                                 \
  V(F32x4Ne, Operator::kCommutative, 2, 0, 1)                                 \
  V(F32x4Lt, Operator::kNoProperties, 2, 0, 1)                                \
  V(F32x4Le, Operator::kNoProperties, 2, 0, 1)                                \
  V(F32x4Qfma, Operator::kNoProperties, 3, 0, 1)                              \
  V(F32x4Qfms, Operator::kNoProperties, 3, 0, 1)                              \
  V(I64x2Splat, Operator::kNoProperties, 1, 0, 1)                             \
  V(I64x2Neg, Operator::kNoProperties, 1, 0, 1)                               \
  V(I64x2Shl, Operator::kNoProperties, 2, 0, 1)                               \
  V(I64x2ShrS, Operator::kNoProperties, 2, 0, 1)                              \
  V(I64x2Add, Operator::kCommutative, 2, 0, 1)                                \
  V(I64x2Sub, Operator::kNoProperties, 2, 0, 1)                               \
  V(I64x2Mul, Operator::kCommutative, 2, 0, 1)                                \
  V(I64x2MinS, Operator::kCommutative, 2, 0, 1)                               \
  V(I64x2MaxS, Operator::kCommutative, 2, 0, 1)                               \
  V(I64x2Eq, Operator::kCommutative, 2, 0, 1)                                 \
  V(I64x2Ne, Operator::kCommutative, 2, 0, 1)                                 \
  V(I64x2GtS, Operator::kNoProperties, 2, 0, 1)                               \
  V(I64x2GeS, Operator::kNoProperties, 2, 0, 1)                               \
  V(I64x2ShrU, Operator::kNoProperties, 2, 0, 1)                              \
  V(I64x2MinU, Operator::kCommutative, 2, 0, 1)                               \
  V(I64x2MaxU, Operator::kCommutative, 2, 0, 1)                               \
  V(I64x2GtU, Operator::kNoProperties, 2, 0, 1)                               \
  V(I64x2GeU, Operator::kNoProperties, 2, 0, 1)                               \
  V(I32x4Splat, Operator::kNoProperties, 1, 0, 1)                             \
  V(I32x4SConvertF32x4, Operator::kNoProperties, 1, 0, 1)                     \
  V(I32x4SConvertI16x8Low, Operator::kNoProperties, 1, 0, 1)                  \
  V(I32x4SConvertI16x8High, Operator::kNoProperties, 1, 0, 1)                 \
  V(I32x4Neg, Operator::kNoProperties, 1, 0, 1)                               \
  V(I32x4Shl, Operator::kNoProperties, 2, 0, 1)                               \
  V(I32x4ShrS, Operator::kNoProperties, 2, 0, 1)                              \
  V(I32x4Add, Operator::kCommutative, 2, 0, 1)                                \
  V(I32x4AddHoriz, Operator::kNoProperties, 2, 0, 1)                          \
  V(I32x4Sub, Operator::kNoProperties, 2, 0, 1)                               \
  V(I32x4Mul, Operator::kCommutative, 2, 0, 1)                                \
  V(I32x4MinS, Operator::kCommutative, 2, 0, 1)                               \
  V(I32x4MaxS, Operator::kCommutative, 2, 0, 1)                               \
  V(I32x4Eq, Operator::kCommutative, 2, 0, 1)                                 \
  V(I32x4Ne, Operator::kCommutative, 2, 0, 1)                                 \
  V(I32x4GtS, Operator::kNoProperties, 2, 0, 1)                               \
  V(I32x4GeS, Operator::kNoProperties, 2, 0, 1)                               \
  V(I32x4UConvertF32x4, Operator::kNoProperties, 1, 0, 1)                     \
  V(I32x4UConvertI16x8Low, Operator::kNoProperties, 1, 0, 1)                  \
  V(I32x4UConvertI16x8High, Operator::kNoProperties, 1, 0, 1)                 \
  V(I32x4ShrU, Operator::kNoProperties, 2, 0, 1)                              \
  V(I32x4MinU, Operator::kCommutative, 2, 0, 1)                               \
  V(I32x4MaxU, Operator::kCommutative, 2, 0, 1)                               \
  V(I32x4GtU, Operator::kNoProperties, 2, 0, 1)                               \
  V(I32x4GeU, Operator::kNoProperties, 2, 0, 1)                               \
  V(I16x8Splat, Operator::kNoProperties, 1, 0, 1)                             \
  V(I16x8SConvertI8x16Low, Operator::kNoProperties, 1, 0, 1)                  \
  V(I16x8SConvertI8x16High, Operator::kNoProperties, 1, 0, 1)                 \
  V(I16x8Neg, Operator::kNoProperties, 1, 0, 1)                               \
  V(I16x8Shl, Operator::kNoProperties, 2, 0, 1)                               \
  V(I16x8ShrS, Operator::kNoProperties, 2, 0, 1)                              \
  V(I16x8SConvertI32x4, Operator::kNoProperties, 2, 0, 1)                     \
  V(I16x8Add, Operator::kCommutative, 2, 0, 1)                                \
  V(I16x8AddSaturateS, Operator::kCommutative, 2, 0, 1)                       \
  V(I16x8AddHoriz, Operator::kNoProperties, 2, 0, 1)                          \
  V(I16x8Sub, Operator::kNoProperties, 2, 0, 1)                               \
  V(I16x8SubSaturateS, Operator::kNoProperties, 2, 0, 1)                      \
  V(I16x8Mul, Operator::kCommutative, 2, 0, 1)                                \
  V(I16x8MinS, Operator::kCommutative, 2, 0, 1)                               \
  V(I16x8MaxS, Operator::kCommutative, 2, 0, 1)                               \
  V(I16x8Eq, Operator::kCommutative, 2, 0, 1)                                 \
  V(I16x8Ne, Operator::kCommutative, 2, 0, 1)                                 \
  V(I16x8GtS, Operator::kNoProperties, 2, 0, 1)                               \
  V(I16x8GeS, Operator::kNoProperties, 2, 0, 1)                               \
  V(I16x8UConvertI8x16Low, Operator::kNoProperties, 1, 0, 1)                  \
  V(I16x8UConvertI8x16High, Operator::kNoProperties, 1, 0, 1)                 \
  V(I16x8ShrU, Operator::kNoProperties, 2, 0, 1)                              \
  V(I16x8UConvertI32x4, Operator::kNoProperties, 2, 0, 1)                     \
  V(I16x8AddSaturateU, Operator::kCommutative, 2, 0, 1)                       \
  V(I16x8SubSaturateU, Operator::kNoProperties, 2, 0, 1)                      \
  V(I16x8MinU, Operator::kCommutative, 2, 0, 1)                               \
  V(I16x8MaxU, Operator::kCommutative, 2, 0, 1)                               \
  V(I16x8GtU, Operator::kNoProperties, 2, 0, 1)                               \
  V(I16x8GeU, Operator::kNoProperties, 2, 0, 1)                               \
  V(I8x16Splat, Operator::kNoProperties, 1, 0, 1)                             \
  V(I8x16Neg, Operator::kNoProperties, 1, 0, 1)                               \
  V(I8x16Shl, Operator::kNoProperties, 2, 0, 1)                               \
  V(I8x16ShrS, Operator::kNoProperties, 2, 0, 1)                              \
  V(I8x16SConvertI16x8, Operator::kNoProperties, 2, 0, 1)                     \
  V(I8x16Add, Operator::kCommutative, 2, 0, 1)                                \
  V(I8x16AddSaturateS, Operator::kCommutative, 2, 0, 1)                       \
  V(I8x16Sub, Operator::kNoProperties, 2, 0, 1)                               \
  V(I8x16SubSaturateS, Operator::kNoProperties, 2, 0, 1)                      \
  V(I8x16Mul, Operator::kCommutative, 2, 0, 1)                                \
  V(I8x16MinS, Operator::kCommutative, 2, 0, 1)                               \
  V(I8x16MaxS, Operator::kCommutative, 2, 0, 1)                               \
  V(I8x16Eq, Operator::kCommutative, 2, 0, 1)                                 \
  V(I8x16Ne, Operator::kCommutative, 2, 0, 1)                                 \
  V(I8x16GtS, Operator::kNoProperties, 2, 0, 1)                               \
  V(I8x16GeS, Operator::kNoProperties, 2, 0, 1)                               \
  V(I8x16ShrU, Operator::kNoProperties, 2, 0, 1)                              \
  V(I8x16UConvertI16x8, Operator::kNoProperties, 2, 0, 1)                     \
  V(I8x16AddSaturateU, Operator::kCommutative, 2, 0, 1)                       \
  V(I8x16SubSaturateU, Operator::kNoProperties, 2, 0, 1)                      \
  V(I8x16MinU, Operator::kCommutative, 2, 0, 1)                               \
  V(I8x16MaxU, Operator::kCommutative, 2, 0, 1)                               \
  V(I8x16GtU, Operator::kNoProperties, 2, 0, 1)                               \
  V(I8x16GeU, Operator::kNoProperties, 2, 0, 1)                               \
  V(S128Load, Operator::kNoProperties, 2, 0, 1)                               \
  V(S128Store, Operator::kNoProperties, 3, 0, 1)                              \
  V(S128Zero, Operator::kNoProperties, 0, 0, 1)                               \
  V(S128And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)        \
  V(S128Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)         \
  V(S128Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)        \
  V(S128Not, Operator::kNoProperties, 1, 0, 1)                                \
  V(S128Select, Operator::kNoProperties, 3, 0, 1)                             \
  V(S1x2AnyTrue, Operator::kNoProperties, 1, 0, 1)                            \
  V(S1x2AllTrue, Operator::kNoProperties, 1, 0, 1)                            \
  V(S1x4AnyTrue, Operator::kNoProperties, 1, 0, 1)                            \
  V(S1x4AllTrue, Operator::kNoProperties, 1, 0, 1)                            \
  V(S1x8AnyTrue, Operator::kNoProperties, 1, 0, 1)                            \
  V(S1x8AllTrue, Operator::kNoProperties, 1, 0, 1)                            \
  V(S1x16AnyTrue, Operator::kNoProperties, 1, 0, 1)                           \
  V(S1x16AllTrue, Operator::kNoProperties, 1, 0, 1)                           \
  V(S8x16Swizzle, Operator::kNoProperties, 2, 0, 1)                           \
  V(StackPointerGreaterThan, Operator::kNoProperties, 1, 0, 1)

// The format is:
// V(Name, properties, value_input_count, control_input_count, output_count)
#define PURE_OPTIONAL_OP_LIST(V)                            \
  V(Word32Ctz, Operator::kNoProperties, 1, 0, 1)            \
  V(Word64Ctz, Operator::kNoProperties, 1, 0, 1)            \
  V(Word32ReverseBits, Operator::kNoProperties, 1, 0, 1)    \
  V(Word64ReverseBits, Operator::kNoProperties, 1, 0, 1)    \
  V(Int32AbsWithOverflow, Operator::kNoProperties, 1, 0, 2) \
  V(Int64AbsWithOverflow, Operator::kNoProperties, 1, 0, 2) \
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

// The format is:
// V(Name, properties, value_input_count, control_input_count, output_count)
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
  V(AnyTagged)               \
  V(CompressedSigned)        \
  V(CompressedPointer)       \
  V(AnyCompressed)

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
  V(kTagged)                           \
  V(kCompressedSigned)                 \
  V(kCompressedPointer)                \
  V(kCompressed)

#define ATOMIC_U32_TYPE_LIST(V) \
  V(Uint8)                      \
  V(Uint16)                     \
  V(Uint32)

#define ATOMIC_TYPE_LIST(V) \
  ATOMIC_U32_TYPE_LIST(V)   \
  V(Int8)                   \
  V(Int16)                  \
  V(Int32)

#define ATOMIC_U64_TYPE_LIST(V) \
  ATOMIC_U32_TYPE_LIST(V)       \
  V(Uint64)

#define ATOMIC_REPRESENTATION_LIST(V) \
  V(kWord8)                           \
  V(kWord16)                          \
  V(kWord32)

#define ATOMIC64_REPRESENTATION_LIST(V) \
  ATOMIC_REPRESENTATION_LIST(V)         \
  V(kWord64)

#define ATOMIC_PAIR_BINOP_LIST(V) \
  V(Add)                          \
  V(Sub)                          \
  V(And)                          \
  V(Or)                           \
  V(Xor)                          \
  V(Exchange)

#define SIMD_LANE_OP_LIST(V) \
  V(F64x2, 2)                \
  V(F32x4, 4)                \
  V(I64x2, 2)                \
  V(I32x4, 4)                \
  V(I16x8, 8)                \
  V(I8x16, 16)

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
  MACHINE_PURE_OP_LIST(PURE)
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
  struct PoisonedLoad##Type##Operator final                                   \
      : public Operator1<LoadRepresentation> {                                \
    PoisonedLoad##Type##Operator()                                            \
        : Operator1<LoadRepresentation>(                                      \
              IrOpcode::kPoisonedLoad,                                        \
              Operator::kNoDeopt | Operator::kNoThrow | Operator::kNoWrite,   \
              "PoisonedLoad", 2, 1, 1, 1, 1, 0, MachineType::Type()) {}       \
  };                                                                          \
  struct UnalignedLoad##Type##Operator final                                  \
      : public Operator1<LoadRepresentation> {                                \
    UnalignedLoad##Type##Operator()                                           \
        : Operator1<LoadRepresentation>(                                      \
              IrOpcode::kUnalignedLoad,                                       \
              Operator::kNoDeopt | Operator::kNoThrow | Operator::kNoWrite,   \
              "UnalignedLoad", 2, 1, 1, 1, 1, 0, MachineType::Type()) {}      \
  };                                                                          \
  struct ProtectedLoad##Type##Operator final                                  \
      : public Operator1<LoadRepresentation> {                                \
    ProtectedLoad##Type##Operator()                                           \
        : Operator1<LoadRepresentation>(                                      \
              IrOpcode::kProtectedLoad,                                       \
              Operator::kNoDeopt | Operator::kNoThrow, "ProtectedLoad", 2, 1, \
              1, 1, 1, 0, MachineType::Type()) {}                             \
  };                                                                          \
  Load##Type##Operator kLoad##Type;                                           \
  PoisonedLoad##Type##Operator kPoisonedLoad##Type;                           \
  UnalignedLoad##Type##Operator kUnalignedLoad##Type;                         \
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

#define STORE(Type)                                                        \
  struct Store##Type##Operator : public Operator1<StoreRepresentation> {   \
    explicit Store##Type##Operator(WriteBarrierKind write_barrier_kind)    \
        : Operator1<StoreRepresentation>(                                  \
              IrOpcode::kStore,                                            \
              Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow, \
              "Store", 3, 1, 1, 0, 1, 0,                                   \
              StoreRepresentation(MachineRepresentation::Type,             \
                                  write_barrier_kind)) {}                  \
  };                                                                       \
  struct Store##Type##NoWriteBarrier##Operator final                       \
      : public Store##Type##Operator {                                     \
    Store##Type##NoWriteBarrier##Operator()                                \
        : Store##Type##Operator(kNoWriteBarrier) {}                        \
  };                                                                       \
  struct Store##Type##AssertNoWriteBarrier##Operator final                 \
      : public Store##Type##Operator {                                     \
    Store##Type##AssertNoWriteBarrier##Operator()                          \
        : Store##Type##Operator(kAssertNoWriteBarrier) {}                  \
  };                                                                       \
  struct Store##Type##MapWriteBarrier##Operator final                      \
      : public Store##Type##Operator {                                     \
    Store##Type##MapWriteBarrier##Operator()                               \
        : Store##Type##Operator(kMapWriteBarrier) {}                       \
  };                                                                       \
  struct Store##Type##PointerWriteBarrier##Operator final                  \
      : public Store##Type##Operator {                                     \
    Store##Type##PointerWriteBarrier##Operator()                           \
        : Store##Type##Operator(kPointerWriteBarrier) {}                   \
  };                                                                       \
  struct Store##Type##EphemeronKeyWriteBarrier##Operator final             \
      : public Store##Type##Operator {                                     \
    Store##Type##EphemeronKeyWriteBarrier##Operator()                      \
        : Store##Type##Operator(kEphemeronKeyWriteBarrier) {}              \
  };                                                                       \
  struct Store##Type##FullWriteBarrier##Operator final                     \
      : public Store##Type##Operator {                                     \
    Store##Type##FullWriteBarrier##Operator()                              \
        : Store##Type##Operator(kFullWriteBarrier) {}                      \
  };                                                                       \
  struct UnalignedStore##Type##Operator final                              \
      : public Operator1<UnalignedStoreRepresentation> {                   \
    UnalignedStore##Type##Operator()                                       \
        : Operator1<UnalignedStoreRepresentation>(                         \
              IrOpcode::kUnalignedStore,                                   \
              Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow, \
              "UnalignedStore", 3, 1, 1, 0, 1, 0,                          \
              MachineRepresentation::Type) {}                              \
  };                                                                       \
  struct ProtectedStore##Type##Operator                                    \
      : public Operator1<StoreRepresentation> {                            \
    explicit ProtectedStore##Type##Operator()                              \
        : Operator1<StoreRepresentation>(                                  \
              IrOpcode::kProtectedStore,                                   \
              Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow, \
              "Store", 3, 1, 1, 0, 1, 0,                                   \
              StoreRepresentation(MachineRepresentation::Type,             \
                                  kNoWriteBarrier)) {}                     \
  };                                                                       \
  Store##Type##NoWriteBarrier##Operator kStore##Type##NoWriteBarrier;      \
  Store##Type##AssertNoWriteBarrier##Operator                              \
      kStore##Type##AssertNoWriteBarrier;                                  \
  Store##Type##MapWriteBarrier##Operator kStore##Type##MapWriteBarrier;    \
  Store##Type##PointerWriteBarrier##Operator                               \
      kStore##Type##PointerWriteBarrier;                                   \
  Store##Type##EphemeronKeyWriteBarrier##Operator                          \
      kStore##Type##EphemeronKeyWriteBarrier;                              \
  Store##Type##FullWriteBarrier##Operator kStore##Type##FullWriteBarrier;  \
  UnalignedStore##Type##Operator kUnalignedStore##Type;                    \
  ProtectedStore##Type##Operator kProtectedStore##Type;
  MACHINE_REPRESENTATION_LIST(STORE)
#undef STORE

#define ATOMIC_LOAD(Type)                                                   \
  struct Word32AtomicLoad##Type##Operator final                             \
      : public Operator1<LoadRepresentation> {                              \
    Word32AtomicLoad##Type##Operator()                                      \
        : Operator1<LoadRepresentation>(                                    \
              IrOpcode::kWord32AtomicLoad,                                  \
              Operator::kNoDeopt | Operator::kNoThrow | Operator::kNoWrite, \
              "Word32AtomicLoad", 2, 1, 1, 1, 1, 0, MachineType::Type()) {} \
  };                                                                        \
  Word32AtomicLoad##Type##Operator kWord32AtomicLoad##Type;
  ATOMIC_TYPE_LIST(ATOMIC_LOAD)
#undef ATOMIC_LOAD

#define ATOMIC_LOAD(Type)                                                   \
  struct Word64AtomicLoad##Type##Operator final                             \
      : public Operator1<LoadRepresentation> {                              \
    Word64AtomicLoad##Type##Operator()                                      \
        : Operator1<LoadRepresentation>(                                    \
              IrOpcode::kWord64AtomicLoad,                                  \
              Operator::kNoDeopt | Operator::kNoThrow | Operator::kNoWrite, \
              "Word64AtomicLoad", 2, 1, 1, 1, 1, 0, MachineType::Type()) {} \
  };                                                                        \
  Word64AtomicLoad##Type##Operator kWord64AtomicLoad##Type;
  ATOMIC_U64_TYPE_LIST(ATOMIC_LOAD)
#undef ATOMIC_LOAD

#define ATOMIC_STORE(Type)                                                 \
  struct Word32AtomicStore##Type##Operator                                 \
      : public Operator1<MachineRepresentation> {                          \
    Word32AtomicStore##Type##Operator()                                    \
        : Operator1<MachineRepresentation>(                                \
              IrOpcode::kWord32AtomicStore,                                \
              Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow, \
              "Word32AtomicStore", 3, 1, 1, 0, 1, 0,                       \
              MachineRepresentation::Type) {}                              \
  };                                                                       \
  Word32AtomicStore##Type##Operator kWord32AtomicStore##Type;
  ATOMIC_REPRESENTATION_LIST(ATOMIC_STORE)
#undef ATOMIC_STORE

#define ATOMIC_STORE(Type)                                                 \
  struct Word64AtomicStore##Type##Operator                                 \
      : public Operator1<MachineRepresentation> {                          \
    Word64AtomicStore##Type##Operator()                                    \
        : Operator1<MachineRepresentation>(                                \
              IrOpcode::kWord64AtomicStore,                                \
              Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow, \
              "Word64AtomicStore", 3, 1, 1, 0, 1, 0,                       \
              MachineRepresentation::Type) {}                              \
  };                                                                       \
  Word64AtomicStore##Type##Operator kWord64AtomicStore##Type;
  ATOMIC64_REPRESENTATION_LIST(ATOMIC_STORE)
#undef ATOMIC_STORE

#define ATOMIC_OP(op, type)                                                    \
  struct op##type##Operator : public Operator1<MachineType> {                  \
    op##type##Operator()                                                       \
        : Operator1<MachineType>(IrOpcode::k##op,                              \
                                 Operator::kNoDeopt | Operator::kNoThrow, #op, \
                                 3, 1, 1, 1, 1, 0, MachineType::type()) {}     \
  };                                                                           \
  op##type##Operator k##op##type;
#define ATOMIC_OP_LIST(type)       \
  ATOMIC_OP(Word32AtomicAdd, type) \
  ATOMIC_OP(Word32AtomicSub, type) \
  ATOMIC_OP(Word32AtomicAnd, type) \
  ATOMIC_OP(Word32AtomicOr, type)  \
  ATOMIC_OP(Word32AtomicXor, type) \
  ATOMIC_OP(Word32AtomicExchange, type)
  ATOMIC_TYPE_LIST(ATOMIC_OP_LIST)
#undef ATOMIC_OP_LIST
#define ATOMIC64_OP_LIST(type)     \
  ATOMIC_OP(Word64AtomicAdd, type) \
  ATOMIC_OP(Word64AtomicSub, type) \
  ATOMIC_OP(Word64AtomicAnd, type) \
  ATOMIC_OP(Word64AtomicOr, type)  \
  ATOMIC_OP(Word64AtomicXor, type) \
  ATOMIC_OP(Word64AtomicExchange, type)
  ATOMIC_U64_TYPE_LIST(ATOMIC64_OP_LIST)
#undef ATOMIC64_OP_LIST
#undef ATOMIC_OP

#define ATOMIC_COMPARE_EXCHANGE(Type)                                          \
  struct Word32AtomicCompareExchange##Type##Operator                           \
      : public Operator1<MachineType> {                                        \
    Word32AtomicCompareExchange##Type##Operator()                              \
        : Operator1<MachineType>(IrOpcode::kWord32AtomicCompareExchange,       \
                                 Operator::kNoDeopt | Operator::kNoThrow,      \
                                 "Word32AtomicCompareExchange", 4, 1, 1, 1, 1, \
                                 0, MachineType::Type()) {}                    \
  };                                                                           \
  Word32AtomicCompareExchange##Type##Operator                                  \
      kWord32AtomicCompareExchange##Type;
  ATOMIC_TYPE_LIST(ATOMIC_COMPARE_EXCHANGE)
#undef ATOMIC_COMPARE_EXCHANGE

#define ATOMIC_COMPARE_EXCHANGE(Type)                                          \
  struct Word64AtomicCompareExchange##Type##Operator                           \
      : public Operator1<MachineType> {                                        \
    Word64AtomicCompareExchange##Type##Operator()                              \
        : Operator1<MachineType>(IrOpcode::kWord64AtomicCompareExchange,       \
                                 Operator::kNoDeopt | Operator::kNoThrow,      \
                                 "Word64AtomicCompareExchange", 4, 1, 1, 1, 1, \
                                 0, MachineType::Type()) {}                    \
  };                                                                           \
  Word64AtomicCompareExchange##Type##Operator                                  \
      kWord64AtomicCompareExchange##Type;
  ATOMIC_U64_TYPE_LIST(ATOMIC_COMPARE_EXCHANGE)
#undef ATOMIC_COMPARE_EXCHANGE

  struct Word32AtomicPairLoadOperator : public Operator {
    Word32AtomicPairLoadOperator()
        : Operator(IrOpcode::kWord32AtomicPairLoad,
                   Operator::kNoDeopt | Operator::kNoThrow,
                   "Word32AtomicPairLoad", 2, 1, 1, 2, 1, 0) {}
  };
  Word32AtomicPairLoadOperator kWord32AtomicPairLoad;

  struct Word32AtomicPairStoreOperator : public Operator {
    Word32AtomicPairStoreOperator()
        : Operator(IrOpcode::kWord32AtomicPairStore,
                   Operator::kNoDeopt | Operator::kNoThrow,
                   "Word32AtomicPairStore", 4, 1, 1, 0, 1, 0) {}
  };
  Word32AtomicPairStoreOperator kWord32AtomicPairStore;

#define ATOMIC_PAIR_OP(op)                                      \
  struct Word32AtomicPair##op##Operator : public Operator {     \
    Word32AtomicPair##op##Operator()                            \
        : Operator(IrOpcode::kWord32AtomicPair##op,             \
                   Operator::kNoDeopt | Operator::kNoThrow,     \
                   "Word32AtomicPair##op", 4, 1, 1, 2, 1, 0) {} \
  };                                                            \
  Word32AtomicPair##op##Operator kWord32AtomicPair##op;
  ATOMIC_PAIR_BINOP_LIST(ATOMIC_PAIR_OP)
#undef ATOMIC_PAIR_OP
#undef ATOMIC_PAIR_BINOP_LIST

  struct Word32AtomicPairCompareExchangeOperator : public Operator {
    Word32AtomicPairCompareExchangeOperator()
        : Operator(IrOpcode::kWord32AtomicPairCompareExchange,
                   Operator::kNoDeopt | Operator::kNoThrow,
                   "Word32AtomicPairCompareExchange", 6, 1, 1, 2, 1, 0) {}
  };
  Word32AtomicPairCompareExchangeOperator kWord32AtomicPairCompareExchange;

  struct MemoryBarrierOperator : public Operator {
    MemoryBarrierOperator()
        : Operator(IrOpcode::kMemoryBarrier,
                   Operator::kNoDeopt | Operator::kNoThrow, "MemoryBarrier", 0,
                   1, 1, 0, 1, 0) {}
  };
  MemoryBarrierOperator kMemoryBarrier;

  // The {BitcastWordToTagged} operator must not be marked as pure (especially
  // not idempotent), because otherwise the splitting logic in the Scheduler
  // might decide to split these operators, thus potentially creating live
  // ranges of allocation top across calls or other things that might allocate.
  // See https://bugs.chromium.org/p/v8/issues/detail?id=6059 for more details.
  struct BitcastWordToTaggedOperator : public Operator {
    BitcastWordToTaggedOperator()
        : Operator(IrOpcode::kBitcastWordToTagged,
                   Operator::kEliminatable | Operator::kNoWrite,
                   "BitcastWordToTagged", 1, 1, 1, 1, 1, 0) {}
  };
  BitcastWordToTaggedOperator kBitcastWordToTagged;

  struct BitcastTaggedToWordOperator : public Operator {
    BitcastTaggedToWordOperator()
        : Operator(IrOpcode::kBitcastTaggedToWord,
                   Operator::kEliminatable | Operator::kNoWrite,
                   "BitcastTaggedToWord", 1, 1, 1, 1, 1, 0) {}
  };
  BitcastTaggedToWordOperator kBitcastTaggedToWord;

  struct BitcastMaybeObjectToWordOperator : public Operator {
    BitcastMaybeObjectToWordOperator()
        : Operator(IrOpcode::kBitcastTaggedToWord,
                   Operator::kEliminatable | Operator::kNoWrite,
                   "BitcastMaybeObjectToWord", 1, 1, 1, 1, 1, 0) {}
  };
  BitcastMaybeObjectToWordOperator kBitcastMaybeObjectToWord;

  struct TaggedPoisonOnSpeculation : public Operator {
    TaggedPoisonOnSpeculation()
        : Operator(IrOpcode::kTaggedPoisonOnSpeculation,
                   Operator::kEliminatable | Operator::kNoWrite,
                   "TaggedPoisonOnSpeculation", 1, 1, 1, 1, 1, 0) {}
  };
  TaggedPoisonOnSpeculation kTaggedPoisonOnSpeculation;

  struct Word32PoisonOnSpeculation : public Operator {
    Word32PoisonOnSpeculation()
        : Operator(IrOpcode::kWord32PoisonOnSpeculation,
                   Operator::kEliminatable | Operator::kNoWrite,
                   "Word32PoisonOnSpeculation", 1, 1, 1, 1, 1, 0) {}
  };
  Word32PoisonOnSpeculation kWord32PoisonOnSpeculation;

  struct Word64PoisonOnSpeculation : public Operator {
    Word64PoisonOnSpeculation()
        : Operator(IrOpcode::kWord64PoisonOnSpeculation,
                   Operator::kEliminatable | Operator::kNoWrite,
                   "Word64PoisonOnSpeculation", 1, 1, 1, 1, 1, 0) {}
  };
  Word64PoisonOnSpeculation kWord64PoisonOnSpeculation;

  struct AbortCSAAssertOperator : public Operator {
    AbortCSAAssertOperator()
        : Operator(IrOpcode::kAbortCSAAssert, Operator::kNoThrow,
                   "AbortCSAAssert", 1, 1, 1, 0, 1, 0) {}
  };
  AbortCSAAssertOperator kAbortCSAAssert;

  struct DebugBreakOperator : public Operator {
    DebugBreakOperator()
        : Operator(IrOpcode::kDebugBreak, Operator::kNoThrow, "DebugBreak", 0,
                   1, 1, 0, 1, 0) {}
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
                               "Comment", 0, 1, 1, 0, 1, 0, msg) {}
};

namespace {
DEFINE_LAZY_LEAKY_OBJECT_GETTER(MachineOperatorGlobalCache,
                                GetMachineOperatorGlobalCache)
}

MachineOperatorBuilder::MachineOperatorBuilder(
    Zone* zone, MachineRepresentation word, Flags flags,
    AlignmentRequirements alignmentRequirements)
    : zone_(zone),
      cache_(*GetMachineOperatorGlobalCache()),
      word_(word),
      flags_(flags),
      alignment_requirements_(alignmentRequirements) {
  DCHECK(word == MachineRepresentation::kWord32 ||
         word == MachineRepresentation::kWord64);
}

const Operator* MachineOperatorBuilder::UnalignedLoad(LoadRepresentation rep) {
#define LOAD(Type)                       \
  if (rep == MachineType::Type()) {      \
    return &cache_.kUnalignedLoad##Type; \
  }
  MACHINE_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
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
}

#define PURE(Name, properties, value_input_count, control_input_count, \
             output_count)                                             \
  const Operator* MachineOperatorBuilder::Name() { return &cache_.k##Name; }
MACHINE_PURE_OP_LIST(PURE)
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
}

const Operator* MachineOperatorBuilder::PoisonedLoad(LoadRepresentation rep) {
#define LOAD(Type)                      \
  if (rep == MachineType::Type()) {     \
    return &cache_.kPoisonedLoad##Type; \
  }
  MACHINE_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::ProtectedLoad(LoadRepresentation rep) {
#define LOAD(Type)                       \
  if (rep == MachineType::Type()) {      \
    return &cache_.kProtectedLoad##Type; \
  }
  MACHINE_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
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
#define STORE(kRep)                                              \
  case MachineRepresentation::kRep:                              \
    switch (store_rep.write_barrier_kind()) {                    \
      case kNoWriteBarrier:                                      \
        return &cache_.k##Store##kRep##NoWriteBarrier;           \
      case kAssertNoWriteBarrier:                                \
        return &cache_.k##Store##kRep##AssertNoWriteBarrier;     \
      case kMapWriteBarrier:                                     \
        return &cache_.k##Store##kRep##MapWriteBarrier;          \
      case kPointerWriteBarrier:                                 \
        return &cache_.k##Store##kRep##PointerWriteBarrier;      \
      case kEphemeronKeyWriteBarrier:                            \
        return &cache_.k##Store##kRep##EphemeronKeyWriteBarrier; \
      case kFullWriteBarrier:                                    \
        return &cache_.k##Store##kRep##FullWriteBarrier;         \
    }                                                            \
    break;
    MACHINE_REPRESENTATION_LIST(STORE)
#undef STORE
    case MachineRepresentation::kBit:
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
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
}

const Operator* MachineOperatorBuilder::UnsafePointerAdd() {
  return &cache_.kUnsafePointerAdd;
}

const Operator* MachineOperatorBuilder::BitcastWordToTagged() {
  return &cache_.kBitcastWordToTagged;
}

const Operator* MachineOperatorBuilder::BitcastTaggedToWord() {
  return &cache_.kBitcastTaggedToWord;
}

const Operator* MachineOperatorBuilder::BitcastMaybeObjectToWord() {
  return &cache_.kBitcastMaybeObjectToWord;
}

const Operator* MachineOperatorBuilder::AbortCSAAssert() {
  return &cache_.kAbortCSAAssert;
}

const Operator* MachineOperatorBuilder::DebugBreak() {
  return &cache_.kDebugBreak;
}

const Operator* MachineOperatorBuilder::Comment(const char* msg) {
  return new (zone_) CommentOperator(msg);
}

const Operator* MachineOperatorBuilder::MemBarrier() {
  return &cache_.kMemoryBarrier;
}

const Operator* MachineOperatorBuilder::Word32AtomicLoad(
    LoadRepresentation rep) {
#define LOAD(Type)                          \
  if (rep == MachineType::Type()) {         \
    return &cache_.kWord32AtomicLoad##Type; \
  }
  ATOMIC_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicStore(
    MachineRepresentation rep) {
#define STORE(kRep)                          \
  if (rep == MachineRepresentation::kRep) {  \
    return &cache_.kWord32AtomicStore##kRep; \
  }
  ATOMIC_REPRESENTATION_LIST(STORE)
#undef STORE
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicExchange(MachineType type) {
#define EXCHANGE(kType)                          \
  if (type == MachineType::kType()) {            \
    return &cache_.kWord32AtomicExchange##kType; \
  }
  ATOMIC_TYPE_LIST(EXCHANGE)
#undef EXCHANGE
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicCompareExchange(
    MachineType type) {
#define COMPARE_EXCHANGE(kType)                         \
  if (type == MachineType::kType()) {                   \
    return &cache_.kWord32AtomicCompareExchange##kType; \
  }
  ATOMIC_TYPE_LIST(COMPARE_EXCHANGE)
#undef COMPARE_EXCHANGE
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicAdd(MachineType type) {
#define ADD(kType)                          \
  if (type == MachineType::kType()) {       \
    return &cache_.kWord32AtomicAdd##kType; \
  }
  ATOMIC_TYPE_LIST(ADD)
#undef ADD
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicSub(MachineType type) {
#define SUB(kType)                          \
  if (type == MachineType::kType()) {       \
    return &cache_.kWord32AtomicSub##kType; \
  }
  ATOMIC_TYPE_LIST(SUB)
#undef SUB
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicAnd(MachineType type) {
#define AND(kType)                          \
  if (type == MachineType::kType()) {       \
    return &cache_.kWord32AtomicAnd##kType; \
  }
  ATOMIC_TYPE_LIST(AND)
#undef AND
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicOr(MachineType type) {
#define OR(kType)                          \
  if (type == MachineType::kType()) {      \
    return &cache_.kWord32AtomicOr##kType; \
  }
  ATOMIC_TYPE_LIST(OR)
#undef OR
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicXor(MachineType type) {
#define XOR(kType)                          \
  if (type == MachineType::kType()) {       \
    return &cache_.kWord32AtomicXor##kType; \
  }
  ATOMIC_TYPE_LIST(XOR)
#undef XOR
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicLoad(
    LoadRepresentation rep) {
#define LOAD(Type)                          \
  if (rep == MachineType::Type()) {         \
    return &cache_.kWord64AtomicLoad##Type; \
  }
  ATOMIC_U64_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicStore(
    MachineRepresentation rep) {
#define STORE(kRep)                          \
  if (rep == MachineRepresentation::kRep) {  \
    return &cache_.kWord64AtomicStore##kRep; \
  }
  ATOMIC64_REPRESENTATION_LIST(STORE)
#undef STORE
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicAdd(MachineType type) {
#define ADD(kType)                          \
  if (type == MachineType::kType()) {       \
    return &cache_.kWord64AtomicAdd##kType; \
  }
  ATOMIC_U64_TYPE_LIST(ADD)
#undef ADD
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicSub(MachineType type) {
#define SUB(kType)                          \
  if (type == MachineType::kType()) {       \
    return &cache_.kWord64AtomicSub##kType; \
  }
  ATOMIC_U64_TYPE_LIST(SUB)
#undef SUB
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicAnd(MachineType type) {
#define AND(kType)                          \
  if (type == MachineType::kType()) {       \
    return &cache_.kWord64AtomicAnd##kType; \
  }
  ATOMIC_U64_TYPE_LIST(AND)
#undef AND
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicOr(MachineType type) {
#define OR(kType)                          \
  if (type == MachineType::kType()) {      \
    return &cache_.kWord64AtomicOr##kType; \
  }
  ATOMIC_U64_TYPE_LIST(OR)
#undef OR
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicXor(MachineType type) {
#define XOR(kType)                          \
  if (type == MachineType::kType()) {       \
    return &cache_.kWord64AtomicXor##kType; \
  }
  ATOMIC_U64_TYPE_LIST(XOR)
#undef XOR
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicExchange(MachineType type) {
#define EXCHANGE(kType)                          \
  if (type == MachineType::kType()) {            \
    return &cache_.kWord64AtomicExchange##kType; \
  }
  ATOMIC_U64_TYPE_LIST(EXCHANGE)
#undef EXCHANGE
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicCompareExchange(
    MachineType type) {
#define COMPARE_EXCHANGE(kType)                         \
  if (type == MachineType::kType()) {                   \
    return &cache_.kWord64AtomicCompareExchange##kType; \
  }
  ATOMIC_U64_TYPE_LIST(COMPARE_EXCHANGE)
#undef COMPARE_EXCHANGE
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicPairLoad() {
  return &cache_.kWord32AtomicPairLoad;
}

const Operator* MachineOperatorBuilder::Word32AtomicPairStore() {
  return &cache_.kWord32AtomicPairStore;
}

const Operator* MachineOperatorBuilder::Word32AtomicPairAdd() {
  return &cache_.kWord32AtomicPairAdd;
}

const Operator* MachineOperatorBuilder::Word32AtomicPairSub() {
  return &cache_.kWord32AtomicPairSub;
}

const Operator* MachineOperatorBuilder::Word32AtomicPairAnd() {
  return &cache_.kWord32AtomicPairAnd;
}

const Operator* MachineOperatorBuilder::Word32AtomicPairOr() {
  return &cache_.kWord32AtomicPairOr;
}

const Operator* MachineOperatorBuilder::Word32AtomicPairXor() {
  return &cache_.kWord32AtomicPairXor;
}

const Operator* MachineOperatorBuilder::Word32AtomicPairExchange() {
  return &cache_.kWord32AtomicPairExchange;
}

const Operator* MachineOperatorBuilder::Word32AtomicPairCompareExchange() {
  return &cache_.kWord32AtomicPairCompareExchange;
}

const Operator* MachineOperatorBuilder::TaggedPoisonOnSpeculation() {
  return &cache_.kTaggedPoisonOnSpeculation;
}

const Operator* MachineOperatorBuilder::Word32PoisonOnSpeculation() {
  return &cache_.kWord32PoisonOnSpeculation;
}

const Operator* MachineOperatorBuilder::Word64PoisonOnSpeculation() {
  return &cache_.kWord64PoisonOnSpeculation;
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

const Operator* MachineOperatorBuilder::S8x16Shuffle(
    const uint8_t shuffle[16]) {
  uint8_t* array = zone_->NewArray<uint8_t>(16);
  memcpy(array, shuffle, 16);
  return new (zone_)
      Operator1<uint8_t*>(IrOpcode::kS8x16Shuffle, Operator::kPure, "Shuffle",
                          2, 0, 0, 1, 0, 0, array);
}

const uint8_t* S8x16ShuffleOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kS8x16Shuffle, op->opcode());
  return OpParameter<uint8_t*>(op);
}

#undef PURE_BINARY_OP_LIST_32
#undef PURE_BINARY_OP_LIST_64
#undef MACHINE_PURE_OP_LIST
#undef PURE_OPTIONAL_OP_LIST
#undef OVERFLOW_OP_LIST
#undef MACHINE_TYPE_LIST
#undef MACHINE_REPRESENTATION_LIST
#undef ATOMIC_TYPE_LIST
#undef ATOMIC_U64_TYPE_LIST
#undef ATOMIC_U32_TYPE_LIST
#undef ATOMIC_REPRESENTATION_LIST
#undef ATOMIC64_REPRESENTATION_LIST
#undef SIMD_LANE_OP_LIST
#undef STACK_SLOT_CACHED_SIZES_ALIGNMENTS_LIST

}  // namespace compiler
}  // namespace internal
}  // namespace v8
