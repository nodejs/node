// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ARGUMENTS_H_
#define V8_OBJECTS_ARGUMENTS_H_

#include "src/objects/fixed-array.h"
#include "src/objects/js-objects.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class StructBodyDescriptor;

#include "torque-generated/src/objects/arguments-tq.inc"

// Superclass for all objects with instance type {JS_ARGUMENTS_OBJECT_TYPE}
class JSArgumentsObject
    : public TorqueGeneratedJSArgumentsObject<JSArgumentsObject, JSObject> {
 public:
  DECL_VERIFIER(JSArgumentsObject)
  DECL_PRINTER(JSArgumentsObject)
  TQ_OBJECT_CONSTRUCTORS(JSArgumentsObject)
};

// JSSloppyArgumentsObject is just a JSArgumentsObject with specific initial
// map. This initial map adds in-object properties for "length" and "callee".
class JSSloppyArgumentsObject
    : public TorqueGeneratedJSSloppyArgumentsObject<JSSloppyArgumentsObject,
                                                    JSArgumentsObject> {
 public:
  // Indices of in-object properties.
  static const int kLengthIndex = 0;
  static const int kCalleeIndex = kLengthIndex + 1;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSSloppyArgumentsObject);
};

// JSStrictArgumentsObject is just a JSArgumentsObject with specific initial
// map. This initial map adds an in-object property for "length".
class JSStrictArgumentsObject
    : public TorqueGeneratedJSStrictArgumentsObject<JSStrictArgumentsObject,
                                                    JSArgumentsObject> {
 public:
  // Indices of in-object properties.
  static const int kLengthIndex = 0;
  static_assert(kLengthIndex == JSSloppyArgumentsObject::kLengthIndex);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSStrictArgumentsObject);
};

// Representation of a slow alias as part of a sloppy arguments objects.
// For fast aliases (if HasSloppyArgumentsElements()):
// - the parameter map contains an index into the context
// - all attributes of the element have default values
// For slow aliases (if HasDictionaryArgumentsElements()):
// - the parameter map contains no fast alias mapping (i.e. the hole)
// - this struct (in the slow backing store) contains an index into the context
// - all attributes are available as part if the property details
class AliasedArgumentsEntry
    : public TorqueGeneratedAliasedArgumentsEntry<AliasedArgumentsEntry,
                                                  Struct> {
 public:
  using BodyDescriptor = StructBodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(AliasedArgumentsEntry)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ARGUMENTS_H_
