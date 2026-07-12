// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TURBOSHAFT_TYPES_H_
#define V8_OBJECTS_TURBOSHAFT_TYPES_H_

#include "src/common/globals.h"
#include "src/objects/heap-object.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

#include "torque-generated/src/objects/turboshaft-types-tq.inc"

class TurboshaftFloatSpecialValues {
 public:
  DEFINE_TORQUE_GENERATED_TURBOSHAFT_FLOAT_SPECIAL_VALUES()
};

class TurboshaftType
    : public TorqueGeneratedTurboshaftType<TurboshaftType, HeapObject> {
 public:
  TQ_OBJECT_CONSTRUCTORS(TurboshaftType)
};

class TurboshaftWord32Type
    : public TorqueGeneratedTurboshaftWord32Type<TurboshaftWord32Type,
                                                 TurboshaftType> {
 public:
  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(TurboshaftWord32Type)
};

class TurboshaftWord32RangeType
    : public TorqueGeneratedTurboshaftWord32RangeType<TurboshaftWord32RangeType,
                                                      TurboshaftWord32Type> {
 public:
  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(TurboshaftWord32RangeType)
};

class TurboshaftWord32SetType
    : public TorqueGeneratedTurboshaftWord32SetType<TurboshaftWord32SetType,
                                                    TurboshaftWord32Type> {
 public:
  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(TurboshaftWord32SetType)
};

class TurboshaftWord64Type
    : public TorqueGeneratedTurboshaftWord64Type<TurboshaftWord64Type,
                                                 TurboshaftType> {
 public:
  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(TurboshaftWord64Type)
};

class TurboshaftWord64RangeType
    : public TorqueGeneratedTurboshaftWord64RangeType<TurboshaftWord64RangeType,
                                                      TurboshaftWord64Type> {
 public:
  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(TurboshaftWord64RangeType)
};

class TurboshaftWord64SetType
    : public TorqueGeneratedTurboshaftWord64SetType<TurboshaftWord64SetType,
                                                    TurboshaftWord64Type> {
 public:
  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(TurboshaftWord64SetType)
};

class TurboshaftFloat64Type
    : public TorqueGeneratedTurboshaftFloat64Type<TurboshaftFloat64Type,
                                                  TurboshaftType> {
 public:
  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(TurboshaftFloat64Type)
};

class TurboshaftFloat64RangeType
    : public TorqueGeneratedTurboshaftFloat64RangeType<
          TurboshaftFloat64RangeType, TurboshaftFloat64Type> {
 public:
  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(TurboshaftFloat64RangeType)
};

class TurboshaftFloat64SetType
    : public TorqueGeneratedTurboshaftFloat64SetType<TurboshaftFloat64SetType,
                                                     TurboshaftFloat64Type> {
 public:
  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(TurboshaftFloat64SetType)
};

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TURBOSHAFT_TYPES_H_
