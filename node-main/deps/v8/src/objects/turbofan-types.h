// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TURBOFAN_TYPES_H_
#define V8_OBJECTS_TURBOFAN_TYPES_H_

#include "src/common/globals.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects.h"
#include "src/objects/tagged.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/turbofan-types-tq.inc"

class TurbofanTypeLowBits {
 public:
  DEFINE_TORQUE_GENERATED_TURBOFAN_TYPE_LOW_BITS()
};

class TurbofanTypeHighBits {
 public:
  DEFINE_TORQUE_GENERATED_TURBOFAN_TYPE_HIGH_BITS()
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TURBOFAN_TYPES_H_
