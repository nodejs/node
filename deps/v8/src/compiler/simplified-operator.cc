// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-operator.h"

#include "src/base/lazy-instance.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/types-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

OStream& operator<<(OStream& os, BaseTaggedness base_taggedness) {
  switch (base_taggedness) {
    case kUntaggedBase:
      return os << "untagged base";
    case kTaggedBase:
      return os << "tagged base";
  }
  UNREACHABLE();
  return os;
}


bool operator==(ElementAccess const& lhs, ElementAccess const& rhs) {
  return lhs.base_is_tagged == rhs.base_is_tagged &&
         lhs.header_size == rhs.header_size && lhs.type == rhs.type &&
         lhs.machine_type == rhs.machine_type;
}


bool operator!=(ElementAccess const& lhs, ElementAccess const& rhs) {
  return !(lhs == rhs);
}


OStream& operator<<(OStream& os, ElementAccess const& access) {
  os << "[" << access.base_is_tagged << ", " << access.header_size << ", ";
  access.type->PrintTo(os);
  os << ", " << access.machine_type << "]";
  return os;
}


const FieldAccess& FieldAccessOf(const Operator* op) {
  DCHECK_NOT_NULL(op);
  DCHECK(op->opcode() == IrOpcode::kLoadField ||
         op->opcode() == IrOpcode::kStoreField);
  return OpParameter<FieldAccess>(op);
}


const ElementAccess& ElementAccessOf(const Operator* op) {
  DCHECK_NOT_NULL(op);
  DCHECK(op->opcode() == IrOpcode::kLoadElement ||
         op->opcode() == IrOpcode::kStoreElement);
  return OpParameter<ElementAccess>(op);
}


// Specialization for static parameters of type {FieldAccess}.
template <>
struct StaticParameterTraits<FieldAccess> {
  static OStream& PrintTo(OStream& os, const FieldAccess& val) {
    return os << val.offset;
  }
  static int HashCode(const FieldAccess& val) {
    return (val.offset < 16) | (val.machine_type & 0xffff);
  }
  static bool Equals(const FieldAccess& lhs, const FieldAccess& rhs) {
    return lhs.base_is_tagged == rhs.base_is_tagged &&
           lhs.offset == rhs.offset && lhs.machine_type == rhs.machine_type &&
           lhs.type->Is(rhs.type);
  }
};


// Specialization for static parameters of type {ElementAccess}.
template <>
struct StaticParameterTraits<ElementAccess> {
  static OStream& PrintTo(OStream& os, const ElementAccess& access) {
    return os << access;
  }
  static int HashCode(const ElementAccess& access) {
    return (access.header_size < 16) | (access.machine_type & 0xffff);
  }
  static bool Equals(const ElementAccess& lhs, const ElementAccess& rhs) {
    return lhs.base_is_tagged == rhs.base_is_tagged &&
           lhs.header_size == rhs.header_size &&
           lhs.machine_type == rhs.machine_type && lhs.type->Is(rhs.type);
  }
};


#define PURE_OP_LIST(V)                                \
  V(BooleanNot, Operator::kNoProperties, 1)            \
  V(BooleanToNumber, Operator::kNoProperties, 1)       \
  V(NumberEqual, Operator::kCommutative, 2)            \
  V(NumberLessThan, Operator::kNoProperties, 2)        \
  V(NumberLessThanOrEqual, Operator::kNoProperties, 2) \
  V(NumberAdd, Operator::kCommutative, 2)              \
  V(NumberSubtract, Operator::kNoProperties, 2)        \
  V(NumberMultiply, Operator::kCommutative, 2)         \
  V(NumberDivide, Operator::kNoProperties, 2)          \
  V(NumberModulus, Operator::kNoProperties, 2)         \
  V(NumberToInt32, Operator::kNoProperties, 1)         \
  V(NumberToUint32, Operator::kNoProperties, 1)        \
  V(StringEqual, Operator::kCommutative, 2)            \
  V(StringLessThan, Operator::kNoProperties, 2)        \
  V(StringLessThanOrEqual, Operator::kNoProperties, 2) \
  V(StringAdd, Operator::kNoProperties, 2)             \
  V(ChangeTaggedToInt32, Operator::kNoProperties, 1)   \
  V(ChangeTaggedToUint32, Operator::kNoProperties, 1)  \
  V(ChangeTaggedToFloat64, Operator::kNoProperties, 1) \
  V(ChangeInt32ToTagged, Operator::kNoProperties, 1)   \
  V(ChangeUint32ToTagged, Operator::kNoProperties, 1)  \
  V(ChangeFloat64ToTagged, Operator::kNoProperties, 1) \
  V(ChangeBoolToBit, Operator::kNoProperties, 1)       \
  V(ChangeBitToBool, Operator::kNoProperties, 1)


#define ACCESS_OP_LIST(V)                                 \
  V(LoadField, FieldAccess, Operator::kNoWrite, 1, 1)     \
  V(StoreField, FieldAccess, Operator::kNoRead, 2, 0)     \
  V(LoadElement, ElementAccess, Operator::kNoWrite, 3, 1) \
  V(StoreElement, ElementAccess, Operator::kNoRead, 4, 0)


struct SimplifiedOperatorBuilderImpl FINAL {
#define PURE(Name, properties, input_count)                               \
  struct Name##Operator FINAL : public SimpleOperator {                   \
    Name##Operator()                                                      \
        : SimpleOperator(IrOpcode::k##Name, Operator::kPure | properties, \
                         input_count, 1, #Name) {}                        \
  };                                                                      \
  Name##Operator k##Name;
  PURE_OP_LIST(PURE)
#undef PURE
};


static base::LazyInstance<SimplifiedOperatorBuilderImpl>::type kImpl =
    LAZY_INSTANCE_INITIALIZER;


SimplifiedOperatorBuilder::SimplifiedOperatorBuilder(Zone* zone)
    : impl_(kImpl.Get()), zone_(zone) {}


#define PURE(Name, properties, input_count) \
  const Operator* SimplifiedOperatorBuilder::Name() { return &impl_.k##Name; }
PURE_OP_LIST(PURE)
#undef PURE


const Operator* SimplifiedOperatorBuilder::ReferenceEqual(Type* type) {
  // TODO(titzer): What about the type parameter?
  return new (zone()) SimpleOperator(IrOpcode::kReferenceEqual,
                                     Operator::kCommutative | Operator::kPure,
                                     2, 1, "ReferenceEqual");
}


#define ACCESS(Name, Type, properties, input_count, output_count)           \
  const Operator* SimplifiedOperatorBuilder::Name(const Type& access) {     \
    return new (zone())                                                     \
        Operator1<Type>(IrOpcode::k##Name, Operator::kNoThrow | properties, \
                        input_count, output_count, #Name, access);          \
  }
ACCESS_OP_LIST(ACCESS)
#undef ACCESS

}  // namespace compiler
}  // namespace internal
}  // namespace v8
