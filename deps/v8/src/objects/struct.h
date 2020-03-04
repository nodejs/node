// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRUCT_H_
#define V8_OBJECTS_STRUCT_H_

#include "src/objects/heap-object.h"
#include "src/objects/objects.h"
#include "torque-generated/class-definitions-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// An abstract superclass, a marker class really, for simple structure classes.
// It doesn't carry any functionality but allows struct classes to be
// identified in the type system.
class Struct : public TorqueGeneratedStruct<Struct, HeapObject> {
 public:
  inline void InitializeBody(int object_size);
  void BriefPrintDetails(std::ostream& os);
  STATIC_ASSERT(kHeaderSize == HeapObject::kHeaderSize);

  TQ_OBJECT_CONSTRUCTORS(Struct)
};

class Tuple2 : public TorqueGeneratedTuple2<Tuple2, Struct> {
 public:
  void BriefPrintDetails(std::ostream& os);

  TQ_OBJECT_CONSTRUCTORS(Tuple2)
};

class Tuple3 : public TorqueGeneratedTuple3<Tuple3, Struct> {
 public:
  void BriefPrintDetails(std::ostream& os);

  TQ_OBJECT_CONSTRUCTORS(Tuple3)
};

// Support for JavaScript accessors: A pair of a getter and a setter. Each
// accessor can either be
//   * a JavaScript function or proxy: a real accessor
//   * a FunctionTemplateInfo: a real (lazy) accessor
//   * undefined: considered an accessor by the spec, too, strangely enough
//   * null: an accessor which has not been set
class AccessorPair : public TorqueGeneratedAccessorPair<AccessorPair, Struct> {
 public:
  static Handle<AccessorPair> Copy(Isolate* isolate, Handle<AccessorPair> pair);

  inline Object get(AccessorComponent component);
  inline void set(AccessorComponent component, Object value);

  // Note: Returns undefined if the component is not set.
  static Handle<Object> GetComponent(Isolate* isolate,
                                     Handle<NativeContext> native_context,
                                     Handle<AccessorPair> accessor_pair,
                                     AccessorComponent component);

  // Set both components, skipping arguments which are a JavaScript null.
  inline void SetComponents(Object getter, Object setter);

  inline bool Equals(Object getter_value, Object setter_value);

  // Dispatched behavior.
  DECL_PRINTER(AccessorPair)

  TQ_OBJECT_CONSTRUCTORS(AccessorPair)
};

class ClassPositions
    : public TorqueGeneratedClassPositions<ClassPositions, Struct> {
 public:
  DECL_INT_ACCESSORS(start)
  DECL_INT_ACCESSORS(end)

  // Dispatched behavior.
  DECL_PRINTER(ClassPositions)
  void BriefPrintDetails(std::ostream& os);

  TQ_OBJECT_CONSTRUCTORS(ClassPositions)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRUCT_H_
