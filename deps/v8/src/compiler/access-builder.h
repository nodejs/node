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
// used FieldAccess and ElementAccess descriptors. These descriptors serve as
// parameters to simplified load/store operators.
class AccessBuilder final : public AllStatic {
 public:
  // ===========================================================================
  // Access to heap object fields and elements (based on tagged pointer).

  // Provides access to HeapObject::map() field.
  static FieldAccess ForMap();

  // Provides access to HeapNumber::value() field.
  static FieldAccess ForHeapNumberValue();

  // Provides access to JSObject::properties() field.
  static FieldAccess ForJSObjectProperties();

  // Provides access to JSObject::elements() field.
  static FieldAccess ForJSObjectElements();

  // Provides access to JSObject inobject property fields.
  static FieldAccess ForJSObjectInObjectProperty(Handle<Map> map, int index);

  // Provides access to JSFunction::context() field.
  static FieldAccess ForJSFunctionContext();

  // Provides access to JSFunction::shared() field.
  static FieldAccess ForJSFunctionSharedFunctionInfo();

  // Provides access to JSArray::length() field.
  static FieldAccess ForJSArrayLength(ElementsKind elements_kind);

  // Provides access to JSArrayBuffer::backing_store() field.
  static FieldAccess ForJSArrayBufferBackingStore();

  // Provides access to JSArrayBuffer::bit_field() field.
  static FieldAccess ForJSArrayBufferBitField();

  // Provides access to JSArrayBufferView::buffer() field.
  static FieldAccess ForJSArrayBufferViewBuffer();

  // Provides access to JSDate fields.
  static FieldAccess ForJSDateField(JSDate::FieldIndex index);

  // Provides access to JSIteratorResult::done() field.
  static FieldAccess ForJSIteratorResultDone();

  // Provides access to JSIteratorResult::value() field.
  static FieldAccess ForJSIteratorResultValue();

  // Provides access to JSRegExp::flags() field.
  static FieldAccess ForJSRegExpFlags();

  // Provides access to JSRegExp::source() field.
  static FieldAccess ForJSRegExpSource();

  // Provides access to FixedArray::length() field.
  static FieldAccess ForFixedArrayLength();

  // Provides access to DescriptorArray::enum_cache() field.
  static FieldAccess ForDescriptorArrayEnumCache();

  // Provides access to DescriptorArray::enum_cache_bridge_cache() field.
  static FieldAccess ForDescriptorArrayEnumCacheBridgeCache();

  // Provides access to Map::bit_field() byte.
  static FieldAccess ForMapBitField();

  // Provides access to Map::bit_field3() field.
  static FieldAccess ForMapBitField3();

  // Provides access to Map::descriptors() field.
  static FieldAccess ForMapDescriptors();

  // Provides access to Map::instance_type() field.
  static FieldAccess ForMapInstanceType();

  // Provides access to Map::prototype() field.
  static FieldAccess ForMapPrototype();

  // Provides access to String::length() field.
  static FieldAccess ForStringLength();

  // Provides access to JSGlobalObject::global_proxy() field.
  static FieldAccess ForJSGlobalObjectGlobalProxy();

  // Provides access to JSGlobalObject::native_context() field.
  static FieldAccess ForJSGlobalObjectNativeContext();

  // Provides access to JSValue::value() field.
  static FieldAccess ForValue();

  // Provides access to arguments object fields.
  static FieldAccess ForArgumentsLength();
  static FieldAccess ForArgumentsCallee();

  // Provides access to FixedArray slots.
  static FieldAccess ForFixedArraySlot(size_t index);

  // Provides access to Context slots.
  static FieldAccess ForContextSlot(size_t index);

  // Provides access to PropertyCell::value() field.
  static FieldAccess ForPropertyCellValue();
  static FieldAccess ForPropertyCellValue(Type* type);

  // Provides access to SharedFunctionInfo::feedback_vector() field.
  static FieldAccess ForSharedFunctionInfoTypeFeedbackVector();

  // Provides access to FixedArray elements.
  static ElementAccess ForFixedArrayElement();

  // Provides access to FixedDoubleArray elements.
  static ElementAccess ForFixedDoubleArrayElement();

  // Provides access to Fixed{type}TypedArray and External{type}Array elements.
  static ElementAccess ForTypedArrayElement(ExternalArrayType type,
                                            bool is_external);

  // ===========================================================================
  // Access to global per-isolate variables (based on external reference).

  // Provides access to the backing store of a StatsCounter.
  static FieldAccess ForStatsCounter();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AccessBuilder);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ACCESS_BUILDER_H_
