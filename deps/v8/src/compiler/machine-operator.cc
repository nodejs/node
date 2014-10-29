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

OStream& operator<<(OStream& os, const WriteBarrierKind& write_barrier_kind) {
  switch (write_barrier_kind) {
    case kNoWriteBarrier:
      return os << "NoWriteBarrier";
    case kFullWriteBarrier:
      return os << "FullWriteBarrier";
  }
  UNREACHABLE();
  return os;
}


OStream& operator<<(OStream& os, const StoreRepresentation& rep) {
  return os << "(" << rep.machine_type() << " : " << rep.write_barrier_kind()
            << ")";
}


template <>
struct StaticParameterTraits<StoreRepresentation> {
  static OStream& PrintTo(OStream& os, const StoreRepresentation& rep) {
    return os << rep;
  }
  static int HashCode(const StoreRepresentation& rep) {
    return rep.machine_type() + rep.write_barrier_kind();
  }
  static bool Equals(const StoreRepresentation& rep1,
                     const StoreRepresentation& rep2) {
    return rep1 == rep2;
  }
};


template <>
struct StaticParameterTraits<LoadRepresentation> {
  static OStream& PrintTo(OStream& os, LoadRepresentation type) {  // NOLINT
    return os << type;
  }
  static int HashCode(LoadRepresentation type) { return type; }
  static bool Equals(LoadRepresentation lhs, LoadRepresentation rhs) {
    return lhs == rhs;
  }
};


#define PURE_OP_LIST(V)                                                       \
  V(Word32And, Operator::kAssociative | Operator::kCommutative, 2, 1)         \
  V(Word32Or, Operator::kAssociative | Operator::kCommutative, 2, 1)          \
  V(Word32Xor, Operator::kAssociative | Operator::kCommutative, 2, 1)         \
  V(Word32Shl, Operator::kNoProperties, 2, 1)                                 \
  V(Word32Shr, Operator::kNoProperties, 2, 1)                                 \
  V(Word32Sar, Operator::kNoProperties, 2, 1)                                 \
  V(Word32Ror, Operator::kNoProperties, 2, 1)                                 \
  V(Word32Equal, Operator::kCommutative, 2, 1)                                \
  V(Word64And, Operator::kAssociative | Operator::kCommutative, 2, 1)         \
  V(Word64Or, Operator::kAssociative | Operator::kCommutative, 2, 1)          \
  V(Word64Xor, Operator::kAssociative | Operator::kCommutative, 2, 1)         \
  V(Word64Shl, Operator::kNoProperties, 2, 1)                                 \
  V(Word64Shr, Operator::kNoProperties, 2, 1)                                 \
  V(Word64Sar, Operator::kNoProperties, 2, 1)                                 \
  V(Word64Ror, Operator::kNoProperties, 2, 1)                                 \
  V(Word64Equal, Operator::kCommutative, 2, 1)                                \
  V(Int32Add, Operator::kAssociative | Operator::kCommutative, 2, 1)          \
  V(Int32AddWithOverflow, Operator::kAssociative | Operator::kCommutative, 2, \
    2)                                                                        \
  V(Int32Sub, Operator::kNoProperties, 2, 1)                                  \
  V(Int32SubWithOverflow, Operator::kNoProperties, 2, 2)                      \
  V(Int32Mul, Operator::kAssociative | Operator::kCommutative, 2, 1)          \
  V(Int32Div, Operator::kNoProperties, 2, 1)                                  \
  V(Int32UDiv, Operator::kNoProperties, 2, 1)                                 \
  V(Int32Mod, Operator::kNoProperties, 2, 1)                                  \
  V(Int32UMod, Operator::kNoProperties, 2, 1)                                 \
  V(Int32LessThan, Operator::kNoProperties, 2, 1)                             \
  V(Int32LessThanOrEqual, Operator::kNoProperties, 2, 1)                      \
  V(Uint32LessThan, Operator::kNoProperties, 2, 1)                            \
  V(Uint32LessThanOrEqual, Operator::kNoProperties, 2, 1)                     \
  V(Int64Add, Operator::kAssociative | Operator::kCommutative, 2, 1)          \
  V(Int64Sub, Operator::kNoProperties, 2, 1)                                  \
  V(Int64Mul, Operator::kAssociative | Operator::kCommutative, 2, 1)          \
  V(Int64Div, Operator::kNoProperties, 2, 1)                                  \
  V(Int64UDiv, Operator::kNoProperties, 2, 1)                                 \
  V(Int64Mod, Operator::kNoProperties, 2, 1)                                  \
  V(Int64UMod, Operator::kNoProperties, 2, 1)                                 \
  V(Int64LessThan, Operator::kNoProperties, 2, 1)                             \
  V(Int64LessThanOrEqual, Operator::kNoProperties, 2, 1)                      \
  V(ChangeFloat32ToFloat64, Operator::kNoProperties, 1, 1)                    \
  V(ChangeFloat64ToInt32, Operator::kNoProperties, 1, 1)                      \
  V(ChangeFloat64ToUint32, Operator::kNoProperties, 1, 1)                     \
  V(ChangeInt32ToFloat64, Operator::kNoProperties, 1, 1)                      \
  V(ChangeInt32ToInt64, Operator::kNoProperties, 1, 1)                        \
  V(ChangeUint32ToFloat64, Operator::kNoProperties, 1, 1)                     \
  V(ChangeUint32ToUint64, Operator::kNoProperties, 1, 1)                      \
  V(TruncateFloat64ToFloat32, Operator::kNoProperties, 1, 1)                  \
  V(TruncateFloat64ToInt32, Operator::kNoProperties, 1, 1)                    \
  V(TruncateInt64ToInt32, Operator::kNoProperties, 1, 1)                      \
  V(Float64Add, Operator::kCommutative, 2, 1)                                 \
  V(Float64Sub, Operator::kNoProperties, 2, 1)                                \
  V(Float64Mul, Operator::kCommutative, 2, 1)                                 \
  V(Float64Div, Operator::kNoProperties, 2, 1)                                \
  V(Float64Mod, Operator::kNoProperties, 2, 1)                                \
  V(Float64Sqrt, Operator::kNoProperties, 1, 1)                               \
  V(Float64Equal, Operator::kCommutative, 2, 1)                               \
  V(Float64LessThan, Operator::kNoProperties, 2, 1)                           \
  V(Float64LessThanOrEqual, Operator::kNoProperties, 2, 1)


#define MACHINE_TYPE_LIST(V) \
  V(MachFloat32)             \
  V(MachFloat64)             \
  V(MachInt8)                \
  V(MachUint8)               \
  V(MachInt16)               \
  V(MachUint16)              \
  V(MachInt32)               \
  V(MachUint32)              \
  V(MachInt64)               \
  V(MachUint64)              \
  V(MachAnyTagged)           \
  V(RepBit)                  \
  V(RepWord8)                \
  V(RepWord16)               \
  V(RepWord32)               \
  V(RepWord64)               \
  V(RepFloat32)              \
  V(RepFloat64)              \
  V(RepTagged)


struct MachineOperatorBuilderImpl {
#define PURE(Name, properties, input_count, output_count)                 \
  struct Name##Operator FINAL : public SimpleOperator {                   \
    Name##Operator()                                                      \
        : SimpleOperator(IrOpcode::k##Name, Operator::kPure | properties, \
                         input_count, output_count, #Name) {}             \
  };                                                                      \
  Name##Operator k##Name;
  PURE_OP_LIST(PURE)
#undef PURE

#define LOAD(Type)                                                            \
  struct Load##Type##Operator FINAL : public Operator1<LoadRepresentation> {  \
    Load##Type##Operator()                                                    \
        : Operator1<LoadRepresentation>(                                      \
              IrOpcode::kLoad, Operator::kNoThrow | Operator::kNoWrite, 2, 1, \
              "Load", k##Type) {}                                             \
  };                                                                          \
  Load##Type##Operator k##Load##Type;
  MACHINE_TYPE_LIST(LOAD)
#undef LOAD

#define STORE(Type)                                                           \
  struct Store##Type##Operator : public Operator1<StoreRepresentation> {      \
    explicit Store##Type##Operator(WriteBarrierKind write_barrier_kind)       \
        : Operator1<StoreRepresentation>(                                     \
              IrOpcode::kStore, Operator::kNoRead | Operator::kNoThrow, 3, 0, \
              "Store", StoreRepresentation(k##Type, write_barrier_kind)) {}   \
  };                                                                          \
  struct Store##Type##NoWriteBarrier##Operator FINAL                          \
      : public Store##Type##Operator {                                        \
    Store##Type##NoWriteBarrier##Operator()                                   \
        : Store##Type##Operator(kNoWriteBarrier) {}                           \
  };                                                                          \
  struct Store##Type##FullWriteBarrier##Operator FINAL                        \
      : public Store##Type##Operator {                                        \
    Store##Type##FullWriteBarrier##Operator()                                 \
        : Store##Type##Operator(kFullWriteBarrier) {}                         \
  };                                                                          \
  Store##Type##NoWriteBarrier##Operator k##Store##Type##NoWriteBarrier;       \
  Store##Type##FullWriteBarrier##Operator k##Store##Type##FullWriteBarrier;
  MACHINE_TYPE_LIST(STORE)
#undef STORE
};


static base::LazyInstance<MachineOperatorBuilderImpl>::type kImpl =
    LAZY_INSTANCE_INITIALIZER;


MachineOperatorBuilder::MachineOperatorBuilder(MachineType word)
    : impl_(kImpl.Get()), word_(word) {
  DCHECK(word == kRepWord32 || word == kRepWord64);
}


#define PURE(Name, properties, input_count, output_count) \
  const Operator* MachineOperatorBuilder::Name() { return &impl_.k##Name; }
PURE_OP_LIST(PURE)
#undef PURE


const Operator* MachineOperatorBuilder::Load(LoadRepresentation rep) {
  switch (rep) {
#define LOAD(Type) \
  case k##Type:    \
    return &impl_.k##Load##Type;
    MACHINE_TYPE_LIST(LOAD)
#undef LOAD

    default:
      break;
  }
  UNREACHABLE();
  return NULL;
}


const Operator* MachineOperatorBuilder::Store(StoreRepresentation rep) {
  switch (rep.machine_type()) {
#define STORE(Type)                                     \
  case k##Type:                                         \
    switch (rep.write_barrier_kind()) {                 \
      case kNoWriteBarrier:                             \
        return &impl_.k##Store##Type##NoWriteBarrier;   \
      case kFullWriteBarrier:                           \
        return &impl_.k##Store##Type##FullWriteBarrier; \
    }                                                   \
    break;
    MACHINE_TYPE_LIST(STORE)
#undef STORE

    default:
      break;
  }
  UNREACHABLE();
  return NULL;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
