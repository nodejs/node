// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRUCT_H_
#define V8_OBJECTS_STRUCT_H_

#include "src/objects.h"
#include "src/objects/heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// An abstract superclass, a marker class really, for simple structure classes.
// It doesn't carry much functionality but allows struct classes to be
// identified in the type system.
class Struct : public HeapObject {
 public:
  inline void InitializeBody(int object_size);
  DECL_CAST2(Struct)
  void BriefPrintDetails(std::ostream& os);

  OBJECT_CONSTRUCTORS(Struct, HeapObject)
};

class Tuple2 : public Struct {
 public:
  DECL_ACCESSORS(value1, Object)
  DECL_ACCESSORS(value2, Object)

  DECL_CAST2(Tuple2)

  // Dispatched behavior.
  DECL_PRINTER(Tuple2)
  DECL_VERIFIER(Tuple2)
  void BriefPrintDetails(std::ostream& os);

  static const int kValue1Offset = HeapObject::kHeaderSize;
  static const int kValue2Offset = kValue1Offset + kPointerSize;
  static const int kSize = kValue2Offset + kPointerSize;

  OBJECT_CONSTRUCTORS(Tuple2, Struct);
};

class Tuple3 : public Tuple2 {
 public:
  DECL_ACCESSORS(value3, Object)

  DECL_CAST2(Tuple3)

  // Dispatched behavior.
  DECL_PRINTER(Tuple3)
  DECL_VERIFIER(Tuple3)
  void BriefPrintDetails(std::ostream& os);

  static const int kValue3Offset = Tuple2::kSize;
  static const int kSize = kValue3Offset + kPointerSize;

  OBJECT_CONSTRUCTORS(Tuple3, Tuple2);
};

// Support for JavaScript accessors: A pair of a getter and a setter. Each
// accessor can either be
//   * a JavaScript function or proxy: a real accessor
//   * a FunctionTemplateInfo: a real (lazy) accessor
//   * undefined: considered an accessor by the spec, too, strangely enough
//   * null: an accessor which has not been set
class AccessorPair : public Struct {
 public:
  DECL_ACCESSORS(getter, Object)
  DECL_ACCESSORS(setter, Object)

  DECL_CAST2(AccessorPair)

  static Handle<AccessorPair> Copy(Isolate* isolate, Handle<AccessorPair> pair);

  inline Object get(AccessorComponent component);
  inline void set(AccessorComponent component, Object value);

  // Note: Returns undefined if the component is not set.
  static Handle<Object> GetComponent(Isolate* isolate,
                                     Handle<AccessorPair> accessor_pair,
                                     AccessorComponent component);

  // Set both components, skipping arguments which are a JavaScript null.
  inline void SetComponents(Object getter, Object setter);

  inline bool Equals(Object getter_value, Object setter_value);

  // Dispatched behavior.
  DECL_PRINTER(AccessorPair)
  DECL_VERIFIER(AccessorPair)

  static const int kGetterOffset = HeapObject::kHeaderSize;
  static const int kSetterOffset = kGetterOffset + kPointerSize;
  static const int kSize = kSetterOffset + kPointerSize;

  OBJECT_CONSTRUCTORS(AccessorPair, Struct);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRUCT_H_
