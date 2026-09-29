// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROPERTY_DESCRIPTOR_OBJECT_H_
#define V8_OBJECTS_PROPERTY_DESCRIPTOR_OBJECT_H_

#include "src/base/bit-field.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

V8_OBJECT class PropertyDescriptorObject : public Struct {
 public:
  using IsEnumerableBit = base::BitField<bool, 0, 1, uint32_t>;
  using HasEnumerableBit = IsEnumerableBit::Next<bool, 1>;
  using IsConfigurableBit = HasEnumerableBit::Next<bool, 1>;
  using HasConfigurableBit = IsConfigurableBit::Next<bool, 1>;
  using IsWritableBit = HasConfigurableBit::Next<bool, 1>;
  using HasWritableBit = IsWritableBit::Next<bool, 1>;
  using HasValueBit = HasWritableBit::Next<bool, 1>;
  using HasGetBit = HasValueBit::Next<bool, 1>;
  using HasSetBit = HasGetBit::Next<bool, 1>;
  enum Flag : uint32_t {
    kNone = 0,
    kIsEnumerable = IsEnumerableBit::kMask,
    kHasEnumerable = HasEnumerableBit::kMask,
    kIsConfigurable = IsConfigurableBit::kMask,
    kHasConfigurable = HasConfigurableBit::kMask,
    kIsWritable = IsWritableBit::kMask,
    kHasWritable = HasWritableBit::kMask,
    kHasValue = HasValueBit::kMask,
    kHasGet = HasGetBit::kMask,
    kHasSet = HasSetBit::kMask,
  };
  using Flags = base::Flags<Flag>;
  static constexpr int kFlagCount = 9;

  static const int kRegularAccessorPropertyBits =
      HasEnumerableBit::kMask | HasConfigurableBit::kMask | HasGetBit::kMask |
      HasSetBit::kMask;

  static const int kRegularDataPropertyBits =
      HasEnumerableBit::kMask | HasConfigurableBit::kMask |
      HasWritableBit::kMask | HasValueBit::kMask;

  static const int kHasMask = HasEnumerableBit::kMask |
                              HasConfigurableBit::kMask |
                              HasWritableBit::kMask | HasValueBit::kMask |
                              HasGetBit::kMask | HasSetBit::kMask;

  inline int flags() const;
  inline void set_flags(int value);

  inline Tagged<Object> value() const;
  inline void set_value(Tagged<Object> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> get() const;
  inline void set_get(Tagged<Object> value,
                      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> set() const;
  inline void set_set(Tagged<Object> value,
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

  TaggedMember<Smi> flags_;
  TaggedMember<Object> value_;
  TaggedMember<Object> get_;
  TaggedMember<Object> set_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROPERTY_DESCRIPTOR_OBJECT_H_
