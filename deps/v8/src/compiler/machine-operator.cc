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

std::ostream& operator<<(std::ostream& os, TruncationMode mode) {
  switch (mode) {
    case TruncationMode::kJavaScript:
      return os << "JavaScript";
    case TruncationMode::kRoundToZero:
      return os << "RoundToZero";
  }
  UNREACHABLE();
  return os;
}


TruncationMode TruncationModeOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kTruncateFloat64ToInt32, op->opcode());
  return OpParameter<TruncationMode>(op);
}


std::ostream& operator<<(std::ostream& os, WriteBarrierKind kind) {
  switch (kind) {
    case kNoWriteBarrier:
      return os << "NoWriteBarrier";
    case kMapWriteBarrier:
      return os << "MapWriteBarrier";
    case kPointerWriteBarrier:
      return os << "PointerWriteBarrier";
    case kFullWriteBarrier:
      return os << "FullWriteBarrier";
  }
  UNREACHABLE();
  return os;
}


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
  DCHECK_EQ(IrOpcode::kLoad, op->opcode());
  return OpParameter<LoadRepresentation>(op);
}


StoreRepresentation const& StoreRepresentationOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kStore, op->opcode());
  return OpParameter<StoreRepresentation>(op);
}


CheckedLoadRepresentation CheckedLoadRepresentationOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kCheckedLoad, op->opcode());
  return OpParameter<CheckedLoadRepresentation>(op);
}


CheckedStoreRepresentation CheckedStoreRepresentationOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kCheckedStore, op->opcode());
  return OpParameter<CheckedStoreRepresentation>(op);
}


#define PURE_OP_LIST(V)                                                       \
  V(Word32And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)      \
  V(Word32Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)       \
  V(Word32Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)      \
  V(Word32Shl, Operator::kNoProperties, 2, 0, 1)                              \
  V(Word32Shr, Operator::kNoProperties, 2, 0, 1)                              \
  V(Word32Sar, Operator::kNoProperties, 2, 0, 1)                              \
  V(Word32Ror, Operator::kNoProperties, 2, 0, 1)                              \
  V(Word32Equal, Operator::kCommutative, 2, 0, 1)                             \
  V(Word32Clz, Operator::kNoProperties, 1, 0, 1)                              \
  V(Word64And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)      \
  V(Word64Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)       \
  V(Word64Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)      \
  V(Word64Shl, Operator::kNoProperties, 2, 0, 1)                              \
  V(Word64Shr, Operator::kNoProperties, 2, 0, 1)                              \
  V(Word64Sar, Operator::kNoProperties, 2, 0, 1)                              \
  V(Word64Ror, Operator::kNoProperties, 2, 0, 1)                              \
  V(Word64Clz, Operator::kNoProperties, 1, 0, 1)                              \
  V(Word64Equal, Operator::kCommutative, 2, 0, 1)                             \
  V(Int32Add, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)       \
  V(Int32AddWithOverflow, Operator::kAssociative | Operator::kCommutative, 2, \
    0, 2)                                                                     \
  V(Int32Sub, Operator::kNoProperties, 2, 0, 1)                               \
  V(Int32SubWithOverflow, Operator::kNoProperties, 2, 0, 2)                   \
  V(Int32Mul, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)       \
  V(Int32MulHigh, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)   \
  V(Int32Div, Operator::kNoProperties, 2, 1, 1)                               \
  V(Int32Mod, Operator::kNoProperties, 2, 1, 1)                               \
  V(Int32LessThan, Operator::kNoProperties, 2, 0, 1)                          \
  V(Int32LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)                   \
  V(Uint32Div, Operator::kNoProperties, 2, 1, 1)                              \
  V(Uint32LessThan, Operator::kNoProperties, 2, 0, 1)                         \
  V(Uint32LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)                  \
  V(Uint32Mod, Operator::kNoProperties, 2, 1, 1)                              \
  V(Uint32MulHigh, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Int64Add, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)       \
  V(Int64AddWithOverflow, Operator::kAssociative | Operator::kCommutative, 2, \
    0, 2)                                                                     \
  V(Int64Sub, Operator::kNoProperties, 2, 0, 1)                               \
  V(Int64SubWithOverflow, Operator::kNoProperties, 2, 0, 2)                   \
  V(Int64Mul, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)       \
  V(Int64Div, Operator::kNoProperties, 2, 1, 1)                               \
  V(Int64Mod, Operator::kNoProperties, 2, 1, 1)                               \
  V(Int64LessThan, Operator::kNoProperties, 2, 0, 1)                          \
  V(Int64LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)                   \
  V(Uint64Div, Operator::kNoProperties, 2, 1, 1)                              \
  V(Uint64Mod, Operator::kNoProperties, 2, 1, 1)                              \
  V(Uint64LessThan, Operator::kNoProperties, 2, 0, 1)                         \
  V(Uint64LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)                  \
  V(ChangeFloat32ToFloat64, Operator::kNoProperties, 1, 0, 1)                 \
  V(ChangeFloat64ToInt32, Operator::kNoProperties, 1, 0, 1)                   \
  V(ChangeFloat64ToUint32, Operator::kNoProperties, 1, 0, 1)                  \
  V(TryTruncateFloat32ToInt64, Operator::kNoProperties, 1, 0, 2)              \
  V(TryTruncateFloat64ToInt64, Operator::kNoProperties, 1, 0, 2)              \
  V(TryTruncateFloat32ToUint64, Operator::kNoProperties, 1, 0, 2)             \
  V(TryTruncateFloat64ToUint64, Operator::kNoProperties, 1, 0, 2)             \
  V(ChangeInt32ToFloat64, Operator::kNoProperties, 1, 0, 1)                   \
  V(RoundInt64ToFloat32, Operator::kNoProperties, 1, 0, 1)                    \
  V(RoundInt64ToFloat64, Operator::kNoProperties, 1, 0, 1)                    \
  V(RoundUint64ToFloat32, Operator::kNoProperties, 1, 0, 1)                   \
  V(RoundUint64ToFloat64, Operator::kNoProperties, 1, 0, 1)                   \
  V(ChangeInt32ToInt64, Operator::kNoProperties, 1, 0, 1)                     \
  V(ChangeUint32ToFloat64, Operator::kNoProperties, 1, 0, 1)                  \
  V(ChangeUint32ToUint64, Operator::kNoProperties, 1, 0, 1)                   \
  V(TruncateFloat64ToFloat32, Operator::kNoProperties, 1, 0, 1)               \
  V(TruncateInt64ToInt32, Operator::kNoProperties, 1, 0, 1)                   \
  V(BitcastFloat32ToInt32, Operator::kNoProperties, 1, 0, 1)                  \
  V(BitcastFloat64ToInt64, Operator::kNoProperties, 1, 0, 1)                  \
  V(BitcastInt32ToFloat32, Operator::kNoProperties, 1, 0, 1)                  \
  V(BitcastInt64ToFloat64, Operator::kNoProperties, 1, 0, 1)                  \
  V(Float32Abs, Operator::kNoProperties, 1, 0, 1)                             \
  V(Float32Add, Operator::kCommutative, 2, 0, 1)                              \
  V(Float32Sub, Operator::kNoProperties, 2, 0, 1)                             \
  V(Float32Mul, Operator::kCommutative, 2, 0, 1)                              \
  V(Float32Div, Operator::kNoProperties, 2, 0, 1)                             \
  V(Float32Sqrt, Operator::kNoProperties, 1, 0, 1)                            \
  V(Float64Abs, Operator::kNoProperties, 1, 0, 1)                             \
  V(Float64Add, Operator::kCommutative, 2, 0, 1)                              \
  V(Float64Sub, Operator::kNoProperties, 2, 0, 1)                             \
  V(Float64Mul, Operator::kCommutative, 2, 0, 1)                              \
  V(Float64Div, Operator::kNoProperties, 2, 0, 1)                             \
  V(Float64Mod, Operator::kNoProperties, 2, 0, 1)                             \
  V(Float64Sqrt, Operator::kNoProperties, 1, 0, 1)                            \
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
  V(LoadStackPointer, Operator::kNoProperties, 0, 0, 1)                       \
  V(LoadFramePointer, Operator::kNoProperties, 0, 0, 1)

#define PURE_OPTIONAL_OP_LIST(V)                            \
  V(Word32Ctz, Operator::kNoProperties, 1, 0, 1)            \
  V(Word64Ctz, Operator::kNoProperties, 1, 0, 1)            \
  V(Word32Popcnt, Operator::kNoProperties, 1, 0, 1)         \
  V(Word64Popcnt, Operator::kNoProperties, 1, 0, 1)         \
  V(Float32Max, Operator::kNoProperties, 2, 0, 1)           \
  V(Float32Min, Operator::kNoProperties, 2, 0, 1)           \
  V(Float64Max, Operator::kNoProperties, 2, 0, 1)           \
  V(Float64Min, Operator::kNoProperties, 2, 0, 1)           \
  V(Float32RoundDown, Operator::kNoProperties, 1, 0, 1)     \
  V(Float64RoundDown, Operator::kNoProperties, 1, 0, 1)     \
  V(Float32RoundUp, Operator::kNoProperties, 1, 0, 1)       \
  V(Float64RoundUp, Operator::kNoProperties, 1, 0, 1)       \
  V(Float32RoundTruncate, Operator::kNoProperties, 1, 0, 1) \
  V(Float64RoundTruncate, Operator::kNoProperties, 1, 0, 1) \
  V(Float64RoundTiesAway, Operator::kNoProperties, 1, 0, 1) \
  V(Float32RoundTiesEven, Operator::kNoProperties, 1, 0, 1) \
  V(Float64RoundTiesEven, Operator::kNoProperties, 1, 0, 1)


#define MACHINE_TYPE_LIST(V) \
  V(Float32)                 \
  V(Float64)                 \
  V(Int8)                    \
  V(Uint8)                   \
  V(Int16)                   \
  V(Uint16)                  \
  V(Int32)                   \
  V(Uint32)                  \
  V(Int64)                   \
  V(Uint64)                  \
  V(Pointer)                 \
  V(AnyTagged)


#define MACHINE_REPRESENTATION_LIST(V) \
  V(kFloat32)                          \
  V(kFloat64)                          \
  V(kWord8)                            \
  V(kWord16)                           \
  V(kWord32)                           \
  V(kWord64)                           \
  V(kTagged)


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

  template <TruncationMode kMode>
  struct TruncateFloat64ToInt32Operator final
      : public Operator1<TruncationMode> {
    TruncateFloat64ToInt32Operator()
        : Operator1<TruncationMode>(IrOpcode::kTruncateFloat64ToInt32,
                                    Operator::kPure, "TruncateFloat64ToInt32",
                                    1, 0, 0, 1, 0, 0, kMode) {}
  };
  TruncateFloat64ToInt32Operator<TruncationMode::kJavaScript>
      kTruncateFloat64ToInt32JavaScript;
  TruncateFloat64ToInt32Operator<TruncationMode::kRoundToZero>
      kTruncateFloat64ToInt32RoundToZero;

#define LOAD(Type)                                                             \
  struct Load##Type##Operator final : public Operator1<LoadRepresentation> {   \
    Load##Type##Operator()                                                     \
        : Operator1<LoadRepresentation>(                                       \
              IrOpcode::kLoad, Operator::kNoThrow | Operator::kNoWrite,        \
              "Load", 2, 1, 1, 1, 1, 0, MachineType::Type()) {}                \
  };                                                                           \
  struct CheckedLoad##Type##Operator final                                     \
      : public Operator1<CheckedLoadRepresentation> {                          \
    CheckedLoad##Type##Operator()                                              \
        : Operator1<CheckedLoadRepresentation>(                                \
              IrOpcode::kCheckedLoad, Operator::kNoThrow | Operator::kNoWrite, \
              "CheckedLoad", 3, 1, 1, 1, 1, 0, MachineType::Type()) {}         \
  };                                                                           \
  Load##Type##Operator kLoad##Type;                                            \
  CheckedLoad##Type##Operator kCheckedLoad##Type;
  MACHINE_TYPE_LIST(LOAD)
#undef LOAD

#define STORE(Type)                                                            \
  struct Store##Type##Operator : public Operator1<StoreRepresentation> {       \
    explicit Store##Type##Operator(WriteBarrierKind write_barrier_kind)        \
        : Operator1<StoreRepresentation>(                                      \
              IrOpcode::kStore, Operator::kNoRead | Operator::kNoThrow,        \
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
  struct CheckedStore##Type##Operator final                                    \
      : public Operator1<CheckedStoreRepresentation> {                         \
    CheckedStore##Type##Operator()                                             \
        : Operator1<CheckedStoreRepresentation>(                               \
              IrOpcode::kCheckedStore, Operator::kNoRead | Operator::kNoThrow, \
              "CheckedStore", 4, 1, 1, 0, 1, 0, MachineRepresentation::Type) { \
    }                                                                          \
  };                                                                           \
  Store##Type##NoWriteBarrier##Operator kStore##Type##NoWriteBarrier;          \
  Store##Type##MapWriteBarrier##Operator kStore##Type##MapWriteBarrier;        \
  Store##Type##PointerWriteBarrier##Operator                                   \
      kStore##Type##PointerWriteBarrier;                                       \
  Store##Type##FullWriteBarrier##Operator kStore##Type##FullWriteBarrier;      \
  CheckedStore##Type##Operator kCheckedStore##Type;
  MACHINE_REPRESENTATION_LIST(STORE)
#undef STORE
};


static base::LazyInstance<MachineOperatorGlobalCache>::type kCache =
    LAZY_INSTANCE_INITIALIZER;


MachineOperatorBuilder::MachineOperatorBuilder(Zone* zone,
                                               MachineRepresentation word,
                                               Flags flags)
    : cache_(kCache.Get()), word_(word), flags_(flags) {
  DCHECK(word == MachineRepresentation::kWord32 ||
         word == MachineRepresentation::kWord64);
}


#define PURE(Name, properties, value_input_count, control_input_count, \
             output_count)                                             \
  const Operator* MachineOperatorBuilder::Name() { return &cache_.k##Name; }
PURE_OP_LIST(PURE)
#undef PURE

#define PURE(Name, properties, value_input_count, control_input_count,     \
             output_count)                                                 \
  const OptionalOperator MachineOperatorBuilder::Name() {                  \
    return OptionalOperator(flags_ & k##Name ? &cache_.k##Name : nullptr); \
  }
PURE_OPTIONAL_OP_LIST(PURE)
#undef PURE


const Operator* MachineOperatorBuilder::TruncateFloat64ToInt32(
    TruncationMode mode) {
  switch (mode) {
    case TruncationMode::kJavaScript:
      return &cache_.kTruncateFloat64ToInt32JavaScript;
    case TruncationMode::kRoundToZero:
      return &cache_.kTruncateFloat64ToInt32RoundToZero;
  }
  UNREACHABLE();
  return nullptr;
}


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

}  // namespace compiler
}  // namespace internal
}  // namespace v8
