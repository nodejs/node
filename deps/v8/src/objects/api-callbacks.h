// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_API_CALLBACKS_H_
#define V8_OBJECTS_API_CALLBACKS_H_

#include "src/objects/struct.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class Undefined;
class StructBodyDescriptor;

#include "torque-generated/src/objects/api-callbacks-tq.inc"

// An accessor must have a getter, but can have no setter.
//
// When setting a property, V8 searches accessors in prototypes.
// If an accessor was found and it does not have a setter,
// the request is ignored.
//
// If the accessor in the prototype has the READ_ONLY property attribute, then
// a new value is added to the derived object when the property is set.
// This shadows the accessor in the prototype.
class AccessorInfo
    : public TorqueGeneratedAccessorInfo<AccessorInfo, HeapObject> {
 public:
  // C function pointer that can be called from native code.
  DECL_REDIRECTED_CALLBACK_ACCESSORS_MAYBE_READ_ONLY_HOST(getter, Address)
  inline bool has_getter(Isolate* isolate);

  // The field contains the address of the C function.
  DECL_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(setter, Address)
  inline bool has_setter(Isolate* isolate);

  DECL_BOOLEAN_ACCESSORS(replace_on_access)
  DECL_BOOLEAN_ACCESSORS(is_sloppy)

  inline SideEffectType getter_side_effect_type() const;
  inline void set_getter_side_effect_type(SideEffectType type);

  inline SideEffectType setter_side_effect_type() const;
  inline void set_setter_side_effect_type(SideEffectType type);

  // The property attributes used when an API object template is instantiated
  // for the first time. Changing of this value afterwards does not affect
  // the actual attributes of a property.
  inline PropertyAttributes initial_property_attributes() const;
  inline void set_initial_property_attributes(PropertyAttributes attributes);

  // Checks whether the given receiver is compatible with this accessor.
  static bool IsCompatibleReceiverMap(Handle<AccessorInfo> info,
                                      Handle<Map> map);
  inline bool IsCompatibleReceiver(Tagged<Object> receiver);

  // Append all descriptors to the array that are not already there.
  // Return number added.
  static int AppendUnique(Isolate* isolate, DirectHandle<Object> descriptors,
                          DirectHandle<FixedArray> array,
                          int valid_descriptors);

  DECL_PRINTER(AccessorInfo)

  inline void RemoveCallbackRedirectionForSerialization(
      IsolateForSandbox isolate);
  inline void RestoreCallbackRedirectionAfterDeserialization(
      IsolateForSandbox isolate);

  inline void clear_padding();

  class BodyDescriptor;

 private:
  // Bit positions in |flags|.
  DEFINE_TORQUE_GENERATED_ACCESSOR_INFO_FLAGS()

  TQ_OBJECT_CONSTRUCTORS(AccessorInfo)
};

V8_OBJECT class AccessCheckInfo : public StructLayout {
 public:
  static Tagged<AccessCheckInfo> Get(Isolate* isolate,
                                     DirectHandle<JSObject> receiver);

  inline Tagged<UnionOf<Foreign, Smi, Undefined>> callback() const;
  inline void set_callback(Tagged<UnionOf<Foreign, Smi, Undefined>> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<InterceptorInfo, Smi, Undefined>> named_interceptor()
      const;
  inline void set_named_interceptor(
      Tagged<UnionOf<InterceptorInfo, Smi, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<InterceptorInfo, Smi, Undefined>> indexed_interceptor()
      const;
  inline void set_indexed_interceptor(
      Tagged<UnionOf<InterceptorInfo, Smi, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> data() const;
  inline void set_data(Tagged<Object> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(AccessCheckInfo)
  DECL_VERIFIER(AccessCheckInfo)

  using BodyDescriptor = StructBodyDescriptor;

 private:
  friend class TorqueGeneratedAccessCheckInfoAsserts;

  TaggedMember<UnionOf<Foreign, Smi, Undefined>> callback_;
  TaggedMember<UnionOf<InterceptorInfo, Smi, Undefined>> named_interceptor_;
  TaggedMember<UnionOf<InterceptorInfo, Smi, Undefined>> indexed_interceptor_;
  TaggedMember<Object> data_;
} V8_OBJECT_END;

#define INTERCEPTOR_INFO_CALLBACK_LIST(V) \
  V(Getter, getter)                       \
  V(Setter, setter)                       \
  V(Query, query)                         \
  V(Descriptor, descriptor)               \
  V(Deleter, deleter)                     \
  V(Enumerator, enumerator)               \
  V(Definer, definer)

class InterceptorInfo
    : public TorqueGeneratedInterceptorInfo<InterceptorInfo, HeapObject> {
 public:
  // Convenient predicates without named/indexed prefix.
  inline bool has_getter() const;
  inline bool has_setter() const;
  inline bool has_query() const;
  inline bool has_descriptor() const;
  inline bool has_deleter() const;
  inline bool has_enumerator() const;
  inline bool has_definer() const;

  // Accessor callbacks for named interceptors.
  DECL_LAZY_REDIRECTED_CALLBACK_ACCESSORS_MAYBE_READ_ONLY_HOST(named_getter,
                                                               Address)
  DECL_LAZY_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(named_setter,
                                                            Address)
  DECL_LAZY_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(named_query,
                                                            Address)
  DECL_LAZY_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(named_descriptor,
                                                            Address)
  DECL_LAZY_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(named_deleter,
                                                            Address)
  DECL_LAZY_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(named_enumerator,
                                                            Address)
  DECL_LAZY_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(named_definer,
                                                            Address)

  // Accessor callbacks for indexed interceptors.
  DECL_LAZY_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(indexed_getter,
                                                            Address)
  DECL_LAZY_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(indexed_setter,
                                                            Address)
  DECL_LAZY_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(indexed_query,
                                                            Address)
  DECL_LAZY_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(indexed_descriptor,
                                                            Address)
  DECL_LAZY_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(indexed_deleter,
                                                            Address)
  DECL_LAZY_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(indexed_enumerator,
                                                            Address)
  DECL_LAZY_EXTERNAL_POINTER_ACCESSORS_MAYBE_READ_ONLY_HOST(indexed_definer,
                                                            Address)

  DECL_BOOLEAN_ACCESSORS(can_intercept_symbols)
  DECL_BOOLEAN_ACCESSORS(non_masking)
  DECL_BOOLEAN_ACCESSORS(is_named)
  DECL_BOOLEAN_ACCESSORS(has_no_side_effect)
  // TODO(ishell): remove support for old signatures once they go through
  // Api deprecation process.
  DECL_BOOLEAN_ACCESSORS(has_new_callbacks_signature)

  DEFINE_TORQUE_GENERATED_INTERCEPTOR_INFO_FLAGS()

  DECL_PRINTER(InterceptorInfo)

  inline void RemoveCallbackRedirectionForSerialization(
      IsolateForSandbox isolate);
  inline void RestoreCallbackRedirectionAfterDeserialization(
      IsolateForSandbox isolate);

  inline void clear_padding();

  class BodyDescriptor;

 private:
  friend class Factory;

  inline void AllocateExternalPointerEntries(Isolate* isolate);

  TQ_OBJECT_CONSTRUCTORS(InterceptorInfo)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_API_CALLBACKS_H_
