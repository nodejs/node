// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SIMPLIFIED_OPERATOR_H_
#define V8_COMPILER_SIMPLIFIED_OPERATOR_H_

#include "src/compiler/machine-operator.h"
#include "src/compiler/opcodes.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

enum BaseTaggedness { kUntaggedBase, kTaggedBase };

// An access descriptor for loads/stores of fixed structures like field
// accesses of heap objects. Accesses from either tagged or untagged base
// pointers are supported; untagging is done automatically during lowering.
struct FieldAccess {
  BaseTaggedness base_is_tagged;  // specifies if the base pointer is tagged.
  int offset;                     // offset of the field, without tag.
  Handle<Name> name;              // debugging only.
  Type* type;                     // type of the field.
  MachineType representation;     // machine representation of field.

  int tag() const { return base_is_tagged == kTaggedBase ? kHeapObjectTag : 0; }
};


// An access descriptor for loads/stores of indexed structures like characters
// in strings or off-heap backing stores. Accesses from either tagged or
// untagged base pointers are supported; untagging is done automatically during
// lowering.
struct ElementAccess {
  BaseTaggedness base_is_tagged;  // specifies if the base pointer is tagged.
  int header_size;                // size of the header, without tag.
  Type* type;                     // type of the element.
  MachineType representation;     // machine representation of element.

  int tag() const { return base_is_tagged == kTaggedBase ? kHeapObjectTag : 0; }
};


// If the accessed object is not a heap object, add this to the header_size.
static const int kNonHeapObjectHeaderSize = kHeapObjectTag;


// Specialization for static parameters of type {FieldAccess}.
template <>
struct StaticParameterTraits<const FieldAccess> {
  static OStream& PrintTo(OStream& os, const FieldAccess& val) {  // NOLINT
    return os << val.offset;
  }
  static int HashCode(const FieldAccess& val) {
    return (val.offset < 16) | (val.representation & 0xffff);
  }
  static bool Equals(const FieldAccess& a, const FieldAccess& b) {
    return a.base_is_tagged == b.base_is_tagged && a.offset == b.offset &&
           a.representation == b.representation && a.type->Is(b.type);
  }
};


// Specialization for static parameters of type {ElementAccess}.
template <>
struct StaticParameterTraits<const ElementAccess> {
  static OStream& PrintTo(OStream& os, const ElementAccess& val) {  // NOLINT
    return os << val.header_size;
  }
  static int HashCode(const ElementAccess& val) {
    return (val.header_size < 16) | (val.representation & 0xffff);
  }
  static bool Equals(const ElementAccess& a, const ElementAccess& b) {
    return a.base_is_tagged == b.base_is_tagged &&
           a.header_size == b.header_size &&
           a.representation == b.representation && a.type->Is(b.type);
  }
};


inline const FieldAccess FieldAccessOf(Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kLoadField ||
         op->opcode() == IrOpcode::kStoreField);
  return static_cast<Operator1<FieldAccess>*>(op)->parameter();
}


inline const ElementAccess ElementAccessOf(Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kLoadElement ||
         op->opcode() == IrOpcode::kStoreElement);
  return static_cast<Operator1<ElementAccess>*>(op)->parameter();
}


// Interface for building simplified operators, which represent the
// medium-level operations of V8, including adding numbers, allocating objects,
// indexing into objects and arrays, etc.
// All operators are typed but many are representation independent.

// Number values from JS can be in one of these representations:
//   - Tagged: word-sized integer that is either
//     - a signed small integer (31 or 32 bits plus a tag)
//     - a tagged pointer to a HeapNumber object that has a float64 field
//   - Int32: an untagged signed 32-bit integer
//   - Uint32: an untagged unsigned 32-bit integer
//   - Float64: an untagged float64

// Additional representations for intermediate code or non-JS code:
//   - Int64: an untagged signed 64-bit integer
//   - Uint64: an untagged unsigned 64-bit integer
//   - Float32: an untagged float32

// Boolean values can be:
//   - Bool: a tagged pointer to either the canonical JS #false or
//           the canonical JS #true object
//   - Bit: an untagged integer 0 or 1, but word-sized
class SimplifiedOperatorBuilder {
 public:
  explicit inline SimplifiedOperatorBuilder(Zone* zone) : zone_(zone) {}

#define SIMPLE(name, properties, inputs, outputs) \
  return new (zone_)                              \
      SimpleOperator(IrOpcode::k##name, properties, inputs, outputs, #name);

#define OP1(name, ptype, pname, properties, inputs, outputs)               \
  return new (zone_)                                                       \
      Operator1<ptype>(IrOpcode::k##name, properties | Operator::kNoThrow, \
                       inputs, outputs, #name, pname)

#define UNOP(name) SIMPLE(name, Operator::kPure, 1, 1)
#define BINOP(name) SIMPLE(name, Operator::kPure, 2, 1)

  Operator* BooleanNot() const { UNOP(BooleanNot); }

  Operator* NumberEqual() const { BINOP(NumberEqual); }
  Operator* NumberLessThan() const { BINOP(NumberLessThan); }
  Operator* NumberLessThanOrEqual() const { BINOP(NumberLessThanOrEqual); }
  Operator* NumberAdd() const { BINOP(NumberAdd); }
  Operator* NumberSubtract() const { BINOP(NumberSubtract); }
  Operator* NumberMultiply() const { BINOP(NumberMultiply); }
  Operator* NumberDivide() const { BINOP(NumberDivide); }
  Operator* NumberModulus() const { BINOP(NumberModulus); }
  Operator* NumberToInt32() const { UNOP(NumberToInt32); }
  Operator* NumberToUint32() const { UNOP(NumberToUint32); }

  Operator* ReferenceEqual(Type* type) const { BINOP(ReferenceEqual); }

  Operator* StringEqual() const { BINOP(StringEqual); }
  Operator* StringLessThan() const { BINOP(StringLessThan); }
  Operator* StringLessThanOrEqual() const { BINOP(StringLessThanOrEqual); }
  Operator* StringAdd() const { BINOP(StringAdd); }

  Operator* ChangeTaggedToInt32() const { UNOP(ChangeTaggedToInt32); }
  Operator* ChangeTaggedToUint32() const { UNOP(ChangeTaggedToUint32); }
  Operator* ChangeTaggedToFloat64() const { UNOP(ChangeTaggedToFloat64); }
  Operator* ChangeInt32ToTagged() const { UNOP(ChangeInt32ToTagged); }
  Operator* ChangeUint32ToTagged() const { UNOP(ChangeUint32ToTagged); }
  Operator* ChangeFloat64ToTagged() const { UNOP(ChangeFloat64ToTagged); }
  Operator* ChangeBoolToBit() const { UNOP(ChangeBoolToBit); }
  Operator* ChangeBitToBool() const { UNOP(ChangeBitToBool); }

  Operator* LoadField(const FieldAccess& access) const {
    OP1(LoadField, FieldAccess, access, Operator::kNoWrite, 1, 1);
  }
  Operator* StoreField(const FieldAccess& access) const {
    OP1(StoreField, FieldAccess, access, Operator::kNoRead, 2, 0);
  }
  Operator* LoadElement(const ElementAccess& access) const {
    OP1(LoadElement, ElementAccess, access, Operator::kNoWrite, 2, 1);
  }
  Operator* StoreElement(const ElementAccess& access) const {
    OP1(StoreElement, ElementAccess, access, Operator::kNoRead, 3, 0);
  }

#undef BINOP
#undef UNOP
#undef OP1
#undef SIMPLE

 private:
  Zone* zone_;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_SIMPLIFIED_OPERATOR_H_
