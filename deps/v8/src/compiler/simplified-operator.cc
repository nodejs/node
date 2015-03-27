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

std::ostream& operator<<(std::ostream& os, BaseTaggedness base_taggedness) {
  switch (base_taggedness) {
    case kUntaggedBase:
      return os << "untagged base";
    case kTaggedBase:
      return os << "tagged base";
  }
  UNREACHABLE();
  return os;
}


MachineType BufferAccess::machine_type() const {
  switch (external_array_type_) {
    case kExternalUint8Array:
    case kExternalUint8ClampedArray:
      return kMachUint8;
    case kExternalInt8Array:
      return kMachInt8;
    case kExternalUint16Array:
      return kMachUint16;
    case kExternalInt16Array:
      return kMachInt16;
    case kExternalUint32Array:
      return kMachUint32;
    case kExternalInt32Array:
      return kMachInt32;
    case kExternalFloat32Array:
      return kMachFloat32;
    case kExternalFloat64Array:
      return kMachFloat64;
  }
  UNREACHABLE();
  return kMachNone;
}


bool operator==(BufferAccess lhs, BufferAccess rhs) {
  return lhs.external_array_type() == rhs.external_array_type();
}


bool operator!=(BufferAccess lhs, BufferAccess rhs) { return !(lhs == rhs); }


size_t hash_value(BufferAccess access) {
  return base::hash<ExternalArrayType>()(access.external_array_type());
}


std::ostream& operator<<(std::ostream& os, BufferAccess access) {
  switch (access.external_array_type()) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                          \
    return os << #Type;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  }
  UNREACHABLE();
  return os;
}


BufferAccess const BufferAccessOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kLoadBuffer ||
         op->opcode() == IrOpcode::kStoreBuffer);
  return OpParameter<BufferAccess>(op);
}


bool operator==(FieldAccess const& lhs, FieldAccess const& rhs) {
  return lhs.base_is_tagged == rhs.base_is_tagged && lhs.offset == rhs.offset &&
         lhs.machine_type == rhs.machine_type;
}


bool operator!=(FieldAccess const& lhs, FieldAccess const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(FieldAccess const& access) {
  return base::hash_combine(access.base_is_tagged, access.offset,
                            access.machine_type);
}


std::ostream& operator<<(std::ostream& os, FieldAccess const& access) {
  os << "[" << access.base_is_tagged << ", " << access.offset << ", ";
#ifdef OBJECT_PRINT
  Handle<Name> name;
  if (access.name.ToHandle(&name)) {
    name->Print(os);
    os << ", ";
  }
#endif
  access.type->PrintTo(os);
  os << ", " << access.machine_type << "]";
  return os;
}


bool operator==(ElementAccess const& lhs, ElementAccess const& rhs) {
  return lhs.base_is_tagged == rhs.base_is_tagged &&
         lhs.header_size == rhs.header_size &&
         lhs.machine_type == rhs.machine_type;
}


bool operator!=(ElementAccess const& lhs, ElementAccess const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(ElementAccess const& access) {
  return base::hash_combine(access.base_is_tagged, access.header_size,
                            access.machine_type);
}


std::ostream& operator<<(std::ostream& os, ElementAccess const& access) {
  os << access.base_is_tagged << ", " << access.header_size << ", ";
  access.type->PrintTo(os);
  os << ", " << access.machine_type;
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


#define PURE_OP_LIST(V)                                 \
  V(AnyToBoolean, Operator::kNoProperties, 1)           \
  V(BooleanNot, Operator::kNoProperties, 1)             \
  V(BooleanToNumber, Operator::kNoProperties, 1)        \
  V(NumberEqual, Operator::kCommutative, 2)             \
  V(NumberLessThan, Operator::kNoProperties, 2)         \
  V(NumberLessThanOrEqual, Operator::kNoProperties, 2)  \
  V(NumberAdd, Operator::kCommutative, 2)               \
  V(NumberSubtract, Operator::kNoProperties, 2)         \
  V(NumberMultiply, Operator::kCommutative, 2)          \
  V(NumberDivide, Operator::kNoProperties, 2)           \
  V(NumberModulus, Operator::kNoProperties, 2)          \
  V(NumberToInt32, Operator::kNoProperties, 1)          \
  V(NumberToUint32, Operator::kNoProperties, 1)         \
  V(PlainPrimitiveToNumber, Operator::kNoProperties, 1) \
  V(StringEqual, Operator::kCommutative, 2)             \
  V(StringLessThan, Operator::kNoProperties, 2)         \
  V(StringLessThanOrEqual, Operator::kNoProperties, 2)  \
  V(StringAdd, Operator::kNoProperties, 2)              \
  V(ChangeTaggedToInt32, Operator::kNoProperties, 1)    \
  V(ChangeTaggedToUint32, Operator::kNoProperties, 1)   \
  V(ChangeTaggedToFloat64, Operator::kNoProperties, 1)  \
  V(ChangeInt32ToTagged, Operator::kNoProperties, 1)    \
  V(ChangeUint32ToTagged, Operator::kNoProperties, 1)   \
  V(ChangeFloat64ToTagged, Operator::kNoProperties, 1)  \
  V(ChangeBoolToBit, Operator::kNoProperties, 1)        \
  V(ChangeBitToBool, Operator::kNoProperties, 1)        \
  V(ObjectIsSmi, Operator::kNoProperties, 1)            \
  V(ObjectIsNonNegativeSmi, Operator::kNoProperties, 1)


struct SimplifiedOperatorGlobalCache FINAL {
#define PURE(Name, properties, input_count)                                \
  struct Name##Operator FINAL : public Operator {                          \
    Name##Operator()                                                       \
        : Operator(IrOpcode::k##Name, Operator::kPure | properties, #Name, \
                   input_count, 0, 0, 1, 0, 0) {}                          \
  };                                                                       \
  Name##Operator k##Name;
  PURE_OP_LIST(PURE)
#undef PURE

#define BUFFER_ACCESS(Type, type, TYPE, ctype, size)                          \
  struct LoadBuffer##Type##Operator FINAL : public Operator1<BufferAccess> {  \
    LoadBuffer##Type##Operator()                                              \
        : Operator1<BufferAccess>(IrOpcode::kLoadBuffer,                      \
                                  Operator::kNoThrow | Operator::kNoWrite,    \
                                  "LoadBuffer", 3, 1, 1, 1, 1, 0,             \
                                  BufferAccess(kExternal##Type##Array)) {}    \
  };                                                                          \
  struct StoreBuffer##Type##Operator FINAL : public Operator1<BufferAccess> { \
    StoreBuffer##Type##Operator()                                             \
        : Operator1<BufferAccess>(IrOpcode::kStoreBuffer,                     \
                                  Operator::kNoRead | Operator::kNoThrow,     \
                                  "StoreBuffer", 4, 1, 1, 0, 1, 0,            \
                                  BufferAccess(kExternal##Type##Array)) {}    \
  };                                                                          \
  LoadBuffer##Type##Operator kLoadBuffer##Type;                               \
  StoreBuffer##Type##Operator kStoreBuffer##Type;
  TYPED_ARRAYS(BUFFER_ACCESS)
#undef BUFFER_ACCESS
};


static base::LazyInstance<SimplifiedOperatorGlobalCache>::type kCache =
    LAZY_INSTANCE_INITIALIZER;


SimplifiedOperatorBuilder::SimplifiedOperatorBuilder(Zone* zone)
    : cache_(kCache.Get()), zone_(zone) {}


#define PURE(Name, properties, input_count) \
  const Operator* SimplifiedOperatorBuilder::Name() { return &cache_.k##Name; }
PURE_OP_LIST(PURE)
#undef PURE


const Operator* SimplifiedOperatorBuilder::ReferenceEqual(Type* type) {
  // TODO(titzer): What about the type parameter?
  return new (zone()) Operator(IrOpcode::kReferenceEqual,
                               Operator::kCommutative | Operator::kPure,
                               "ReferenceEqual", 2, 0, 0, 1, 0, 0);
}


const Operator* SimplifiedOperatorBuilder::LoadBuffer(BufferAccess access) {
  switch (access.external_array_type()) {
#define LOAD_BUFFER(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                     \
    return &cache_.kLoadBuffer##Type;
    TYPED_ARRAYS(LOAD_BUFFER)
#undef LOAD_BUFFER
  }
  UNREACHABLE();
  return nullptr;
}


const Operator* SimplifiedOperatorBuilder::StoreBuffer(BufferAccess access) {
  switch (access.external_array_type()) {
#define STORE_BUFFER(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                      \
    return &cache_.kStoreBuffer##Type;
    TYPED_ARRAYS(STORE_BUFFER)
#undef STORE_BUFFER
  }
  UNREACHABLE();
  return nullptr;
}


#define ACCESS_OP_LIST(V)                                    \
  V(LoadField, FieldAccess, Operator::kNoWrite, 1, 1, 1)     \
  V(StoreField, FieldAccess, Operator::kNoRead, 2, 1, 0)     \
  V(LoadElement, ElementAccess, Operator::kNoWrite, 2, 1, 1) \
  V(StoreElement, ElementAccess, Operator::kNoRead, 3, 1, 0)


#define ACCESS(Name, Type, properties, value_input_count, control_input_count, \
               output_count)                                                   \
  const Operator* SimplifiedOperatorBuilder::Name(const Type& access) {        \
    return new (zone())                                                        \
        Operator1<Type>(IrOpcode::k##Name, Operator::kNoThrow | properties,    \
                        #Name, value_input_count, 1, control_input_count,      \
                        output_count, 1, 0, access);                           \
  }
ACCESS_OP_LIST(ACCESS)
#undef ACCESS

}  // namespace compiler
}  // namespace internal
}  // namespace v8
