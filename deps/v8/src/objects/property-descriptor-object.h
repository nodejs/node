// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROPERTY_DESCRIPTOR_OBJECT_H_
#define V8_OBJECTS_PROPERTY_DESCRIPTOR_OBJECT_H_

#include "src/objects/struct.h"
#include "torque-generated/bit-fields-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class PropertyDescriptorObject
    : public TorqueGeneratedPropertyDescriptorObject<PropertyDescriptorObject,
                                                     Struct> {
 public:
  DEFINE_TORQUE_GENERATED_PROPERTY_DESCRIPTOR_OBJECT_FLAGS()

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
