// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROPERTY_DESCRIPTOR_OBJECT_H_
#define V8_OBJECTS_PROPERTY_DESCRIPTOR_OBJECT_H_

#include "src/base/bit-field.h"
#include "src/objects/bit-field-group.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

V8_OBJECT class PropertyDescriptorObject : public Struct {
 public:
  struct Flags : BitFieldGroup<Flags, uint32_t> {
    using BitFieldGroup::BitFieldGroup;

    using IsEnumerableBit = First<bool, 1>;
    using HasEnumerableBit = IsEnumerableBit::Next<bool, 1>;
    using IsConfigurableBit = HasEnumerableBit::Next<bool, 1>;
    using HasConfigurableBit = IsConfigurableBit::Next<bool, 1>;
    using IsWritableBit = HasConfigurableBit::Next<bool, 1>;
    using HasWritableBit = IsWritableBit::Next<bool, 1>;
    using HasValueBit = HasWritableBit::Next<bool, 1>;
    using HasGetBit = HasValueBit::Next<bool, 1>;
    using HasSetBit = HasGetBit::Next<bool, 1>;

    BIT_FIELD_GROUP_ACCESSORS(is_enumerable, IsEnumerableBit)
    BIT_FIELD_GROUP_ACCESSORS(has_enumerable, HasEnumerableBit)
    BIT_FIELD_GROUP_ACCESSORS(is_configurable, IsConfigurableBit)
    BIT_FIELD_GROUP_ACCESSORS(has_configurable, HasConfigurableBit)
    BIT_FIELD_GROUP_ACCESSORS(is_writable, IsWritableBit)
    BIT_FIELD_GROUP_ACCESSORS(has_writable, HasWritableBit)
    BIT_FIELD_GROUP_ACCESSORS(has_value, HasValueBit)
    BIT_FIELD_GROUP_ACCESSORS(has_get, HasGetBit)
    BIT_FIELD_GROUP_ACCESSORS(has_set, HasSetBit)
  };

  enum Flag : uint32_t {
    kNone = 0,
    kIsEnumerable = Flags::IsEnumerableBit::kMask,
    kHasEnumerable = Flags::HasEnumerableBit::kMask,
    kIsConfigurable = Flags::IsConfigurableBit::kMask,
    kHasConfigurable = Flags::HasConfigurableBit::kMask,
    kIsWritable = Flags::IsWritableBit::kMask,
    kHasWritable = Flags::HasWritableBit::kMask,
    kHasValue = Flags::HasValueBit::kMask,
    kHasGet = Flags::HasGetBit::kMask,
    kHasSet = Flags::HasSetBit::kMask,
  };

  static const int kRegularAccessorPropertyBits =
      Flags::HasEnumerableBit::kMask | Flags::HasConfigurableBit::kMask |
      Flags::HasGetBit::kMask | Flags::HasSetBit::kMask;

  static const int kRegularDataPropertyBits =
      Flags::HasEnumerableBit::kMask | Flags::HasConfigurableBit::kMask |
      Flags::HasWritableBit::kMask | Flags::HasValueBit::kMask;

  static const int kHasMask =
      Flags::HasEnumerableBit::kMask | Flags::HasConfigurableBit::kMask |
      Flags::HasWritableBit::kMask | Flags::HasValueBit::kMask |
      Flags::HasGetBit::kMask | Flags::HasSetBit::kMask;

  inline explicit PropertyDescriptorObject(const AllocationWitness& witness);

  inline Flags flags() const;
  inline void set_flags(Flags value);

  inline Tagged<UnionOf<JSAny, TheHole>> value() const;
  inline void set_value(Tagged<UnionOf<JSAny, TheHole>> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<FunctionTemplateInfo, JSAny, TheHole>> get() const;
  inline void set_get(
      Tagged<UnionOf<FunctionTemplateInfo, JSAny, TheHole>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<FunctionTemplateInfo, JSAny, TheHole>> set() const;
  inline void set_set(
      Tagged<UnionOf<FunctionTemplateInfo, JSAny, TheHole>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  using BodyDescriptor = StructBodyDescriptor;

  DECL_PRINTER(PropertyDescriptorObject)
  DECL_VERIFIER(PropertyDescriptorObject)

 private:
  friend class Factory;
  friend class TorqueGeneratedPropertyDescriptorObjectAsserts;
  friend class CodeStubAssembler;
  friend class ObjectBuiltinsAssembler;
  friend class MacroAssembler;

  SmiBitFieldMember<Flags> flags_;
  TaggedMember<UnionOf<JSAny, TheHole>> value_;
  TaggedMember<UnionOf<FunctionTemplateInfo, JSAny, TheHole>> get_;
  TaggedMember<UnionOf<FunctionTemplateInfo, JSAny, TheHole>> set_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROPERTY_DESCRIPTOR_OBJECT_H_
