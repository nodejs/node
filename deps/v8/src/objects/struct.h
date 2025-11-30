// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRUCT_H_
#define V8_OBJECTS_STRUCT_H_

#include "src/objects/heap-object.h"
#include "src/objects/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class StructBodyDescriptor;

#include "torque-generated/src/objects/struct-tq.inc"

// An abstract superclass, a marker class really, for simple structure classes.
// It doesn't carry any functionality but allows struct classes to be
// identified in the type system.
class Struct : public TorqueGeneratedStruct<Struct, HeapObject> {
 public:
  void BriefPrintDetails(std::ostream& os);
  static_assert(kHeaderSize == HeapObject::kHeaderSize);

  TQ_OBJECT_CONSTRUCTORS(Struct)
};

// Temporary mirror of Struct for subtypes with the new layout.
V8_OBJECT class StructLayout : public HeapObjectLayout {
 public:
  void BriefPrintDetails(std::ostream& os);

  DECL_VERIFIER(Struct)

  using BodyDescriptor = StructBodyDescriptor;
} V8_OBJECT_END;
static_assert(sizeof(StructLayout) == sizeof(HeapObjectLayout));

V8_OBJECT class Tuple2 : public StructLayout {
 public:
  void BriefPrintDetails(std::ostream& os);

  inline Tagged<Object> value1() const;
  inline void set_value1(Tagged<Object> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> value2() const;
  inline void set_value2(Tagged<Object> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_VERIFIER(Tuple2)
  DECL_PRINTER(Tuple2)

 private:
  friend class TorqueGeneratedTuple2Asserts;
  TaggedMember<Object> value1_;
  TaggedMember<Object> value2_;
} V8_OBJECT_END;

// Support for JavaScript accessors: A pair of a getter and a setter. Each
// accessor can either be
//   * a JavaScript function or proxy: a real accessor
//   * a FunctionTemplateInfo: a real (lazy) accessor
//   * undefined: considered an accessor by the spec, too, strangely enough
//   * null: an accessor which has not been set
V8_OBJECT class AccessorPair : public StructLayout {
 public:
  static DirectHandle<AccessorPair> Copy(Isolate* isolate,
                                         DirectHandle<AccessorPair> pair);

  inline Tagged<Object> get(AccessorComponent component);
  inline void set(AccessorComponent component, Tagged<Object> value);
  inline void set(AccessorComponent component, Tagged<Object> value,
                  ReleaseStoreTag tag);

  inline Tagged<Object> getter() const;
  inline void set_getter(Tagged<Object> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Tagged<Object> getter(AcquireLoadTag) const;
  inline void set_getter(Tagged<Object> value, ReleaseStoreTag,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> setter() const;
  inline void set_setter(Tagged<Object> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Tagged<Object> setter(AcquireLoadTag) const;
  inline void set_setter(Tagged<Object> value, ReleaseStoreTag,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Note: Returns undefined if the component is not set.
  static Handle<JSAny> GetComponent(Isolate* isolate,
                                    DirectHandle<NativeContext> native_context,
                                    DirectHandle<AccessorPair> accessor_pair,
                                    AccessorComponent component);

  // Set both components, skipping arguments which are a JavaScript null.
  inline void SetComponents(Tagged<Object> getter, Tagged<Object> setter);

  inline bool Equals(Tagged<Object> getter_value, Tagged<Object> setter_value);

  DECL_VERIFIER(AccessorPair)
  DECL_PRINTER(AccessorPair)

 private:
  friend class CodeStubAssembler;
  friend class V8HeapExplorer;
  friend class TorqueGeneratedAccessorPairAsserts;

  TaggedMember<Object> getter_;
  TaggedMember<Object> setter_;
} V8_OBJECT_END;

V8_OBJECT class ClassPositions : public StructLayout {
 public:
  inline int start() const;
  inline void set_start(int value);

  inline int end() const;
  inline void set_end(int value);

  // Dispatched behavior.
  void BriefPrintDetails(std::ostream& os);

  DECL_VERIFIER(ClassPositions)
  DECL_PRINTER(ClassPositions)

 private:
  friend class TorqueGeneratedClassPositionsAsserts;

  TaggedMember<Smi> start_;
  TaggedMember<Smi> end_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRUCT_H_
