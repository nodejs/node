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

class PropertyDescriptorObject
    : public TorqueGeneratedPropertyDescriptorObject<PropertyDescriptorObject,
                                                     Struct> {
 public:
#define FLAGS_BIT_FIELDS(V, _)      \
  V(IsEnumerableBit, bool, 1, _)    \
  V(HasEnumerableBit, bool, 1, _)   \
  V(IsConfigurableBit, bool, 1, _)  \
  V(HasConfigurableBit, bool, 1, _) \
  V(IsWritableBit, bool, 1, _)      \
  V(HasWritableBit, bool, 1, _)     \
  V(HasValueBit, bool, 1, _)        \
  V(HasGetBit, bool, 1, _)          \
  V(HasSetBit, bool, 1, _)

  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

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

  TQ_OBJECT_CONSTRUCTORS(PropertyDescriptorObject)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROPERTY_DESCRIPTOR_OBJECT_H_
