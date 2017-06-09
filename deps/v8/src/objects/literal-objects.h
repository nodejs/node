// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LITERAL_OBJECTS_H_
#define V8_OBJECTS_LITERAL_OBJECTS_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// BoilerplateDescription is a list of properties consisting of name value
// pairs. In addition to the properties, it provides the projected number
// of properties in the backing store. This number includes properties with
// computed names that are not
// in the list.
class BoilerplateDescription : public FixedArray {
 public:
  Object* name(int index) const;
  Object* value(int index) const;

  // The number of boilerplate properties.
  int size() const;

  // Number of boilerplate properties and properties with computed names.
  int backing_store_size() const;

  void set_backing_store_size(Isolate* isolate, int backing_store_size);

  DECLARE_CAST(BoilerplateDescription)

 private:
  bool has_number_of_properties() const;
};

// Pair of {ElementsKind} and an array of constant values for {ArrayLiteral}
// expressions. Used to communicate with the runtime for literal boilerplate
// creation within the {Runtime_CreateArrayLiteral} method.
class ConstantElementsPair : public Tuple2 {
 public:
  DECL_INT_ACCESSORS(elements_kind)
  DECL_ACCESSORS(constant_values, FixedArrayBase)

  DECLARE_CAST(ConstantElementsPair)

  static const int kElementsKindOffset = kValue1Offset;
  static const int kConstantValuesOffset = kValue2Offset;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ConstantElementsPair);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_LITERAL_OBJECTS_H_
