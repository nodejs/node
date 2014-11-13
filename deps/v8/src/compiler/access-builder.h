// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ACCESS_BUILDER_H_
#define V8_COMPILER_ACCESS_BUILDER_H_

#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// This access builder provides a set of static methods constructing commonly
// used FieldAccess and ElementAccess descriptors. These descriptors server as
// parameters to simplified load/store operators.
class AccessBuilder FINAL : public AllStatic {
 public:
  // Provides access to HeapObject::map() field.
  static FieldAccess ForMap();

  // Provides access to JSObject::properties() field.
  static FieldAccess ForJSObjectProperties();

  // Provides access to JSObject::elements() field.
  static FieldAccess ForJSObjectElements();

  // Provides access to JSFunction::context() field.
  static FieldAccess ForJSFunctionContext();

  // Provides access to JSArrayBuffer::backing_store() field.
  static FieldAccess ForJSArrayBufferBackingStore();

  // Provides access to ExternalArray::external_pointer() field.
  static FieldAccess ForExternalArrayPointer();

  // Provides access to Map::instance_type() field.
  static FieldAccess ForMapInstanceType();

  // Provides access to String::length() field.
  static FieldAccess ForStringLength();

  // Provides access to JSValue::value() field.
  static FieldAccess ForValue();

  // Provides access to FixedArray elements.
  static ElementAccess ForFixedArrayElement();

  // Provides access to Fixed{type}TypedArray and External{type}Array elements.
  static ElementAccess ForTypedArrayElement(ExternalArrayType type,
                                            bool is_external);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AccessBuilder);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ACCESS_BUILDER_H_
