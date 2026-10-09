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

// An abstract superclass, a marker class really, for simple structure classes.
// It doesn't carry any functionality but allows struct classes to be
// identified in the type system.
V8_OBJECT class Struct : public HeapObject {
  V8_IT_ABSTRACT;

 public:
  inline explicit Struct(Tagged<ReadOnly<Map>> map);

  void BriefPrintDetails(std::ostream& os);

  using BodyDescriptor = StructBodyDescriptor;
} V8_OBJECT_END;
static_assert(sizeof(Struct) == sizeof(HeapObject));

V8_OBJECT class Tuple2 : public Struct {
  V8_IT_NO_AUTO_DISPATCH;

 public:
  void BriefPrintDetails(std::ostream& os);

  inline Tagged<Object> value1() const;
  inline void set_value1(Tagged<Object> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Tagged<Object> value1(RelaxedLoadTag) const;
  inline void set_value1(Tagged<Object> value, RelaxedStoreTag,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Tagged<Object> value1(AcquireLoadTag) const;
  inline void set_value1(Tagged<Object> value, ReleaseStoreTag,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> value2() const;
  inline void set_value2(Tagged<Object> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Tagged<Object> value2(RelaxedLoadTag) const;
  inline void set_value2(Tagged<Object> value, RelaxedStoreTag,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_VERIFIER(Tuple2)
  DECL_PRINTER(Tuple2)

 private:
  friend class CodeStubAssembler;
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
V8_OBJECT class AccessorPair : public Struct {
  V8_IT_NO_AUTO_DISPATCH;

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

V8_OBJECT class ClassPositions : public Struct {
  V8_IT_NO_AUTO_DISPATCH;

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

V8_OBJECT class ForInEnumeratorHolder : public Struct {
 public:
  inline Tagged<Map> enum_cache_map() const;
  inline void set_enum_cache_map(Tagged<Map> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArray> named_keys() const;
  inline void set_named_keys(Tagged<FixedArray> value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Smi> elements_length() const;
  inline void set_elements_length(Tagged<Smi> value,
                                  WriteBarrierMode mode = SKIP_WRITE_BARRIER);

  inline Tagged<Smi> cache_length() const;
  inline void set_cache_length(Tagged<Smi> value,
                               WriteBarrierMode mode = SKIP_WRITE_BARRIER);

  DECL_PRINTER(ForInEnumeratorHolder)
  DECL_VERIFIER(ForInEnumeratorHolder)

  using BodyDescriptor = StructBodyDescriptor;

 private:
  friend class TorqueGeneratedForInEnumeratorHolderAsserts;
  friend struct ObjectTraits<ForInEnumeratorHolder>;

  TaggedMember<Map> enum_cache_map_;
  TaggedMember<FixedArray> named_keys_;
  TaggedMember<Smi> elements_length_;
  TaggedMember<Smi> cache_length_;
} V8_OBJECT_END;

template <>
struct ObjectTraits<ForInEnumeratorHolder> {
  static constexpr int kEnumCacheMapOffset =
      offsetof(ForInEnumeratorHolder, enum_cache_map_);
  static constexpr int kNamedKeysOffset =
      offsetof(ForInEnumeratorHolder, named_keys_);
  static constexpr int kElementsLengthOffset =
      offsetof(ForInEnumeratorHolder, elements_length_);
  static constexpr int kCacheLengthOffset =
      offsetof(ForInEnumeratorHolder, cache_length_);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRUCT_H_
