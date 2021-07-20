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
class AccessorInfo : public TorqueGeneratedAccessorInfo<AccessorInfo, Struct> {
 public:
  // This directly points at a foreign C function to be used from the runtime.
  DECL_ACCESSORS(getter, Object)
  inline bool has_getter();
  DECL_ACCESSORS(setter, Object)
  inline bool has_setter();

  static Address redirect(Address address, AccessorComponent component);
  Address redirected_getter() const;

  DECL_BOOLEAN_ACCESSORS(all_can_read)
  DECL_BOOLEAN_ACCESSORS(all_can_write)
  DECL_BOOLEAN_ACCESSORS(is_special_data_property)
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
  inline bool IsCompatibleReceiver(Object receiver);

  // Append all descriptors to the array that are not already there.
  // Return number added.
  static int AppendUnique(Isolate* isolate, Handle<Object> descriptors,
                          Handle<FixedArray> array, int valid_descriptors);

  DECL_PRINTER(AccessorInfo)

 private:
  inline bool HasExpectedReceiverType();

  // Bit positions in |flags|.
  DEFINE_TORQUE_GENERATED_ACCESSOR_INFO_FLAGS()

  TQ_OBJECT_CONSTRUCTORS(AccessorInfo)
};

class AccessCheckInfo
    : public TorqueGeneratedAccessCheckInfo<AccessCheckInfo, Struct> {
 public:
  static AccessCheckInfo Get(Isolate* isolate, Handle<JSObject> receiver);

  TQ_OBJECT_CONSTRUCTORS(AccessCheckInfo)
};

class InterceptorInfo
    : public TorqueGeneratedInterceptorInfo<InterceptorInfo, Struct> {
 public:
  DECL_BOOLEAN_ACCESSORS(can_intercept_symbols)
  DECL_BOOLEAN_ACCESSORS(all_can_read)
  DECL_BOOLEAN_ACCESSORS(non_masking)
  DECL_BOOLEAN_ACCESSORS(is_named)
  DECL_BOOLEAN_ACCESSORS(has_no_side_effect)

  DEFINE_TORQUE_GENERATED_INTERCEPTOR_INFO_FLAGS()

  TQ_OBJECT_CONSTRUCTORS(InterceptorInfo)
};

class CallHandlerInfo
    : public TorqueGeneratedCallHandlerInfo<CallHandlerInfo, Struct> {
 public:
  inline bool IsSideEffectFreeCallHandlerInfo() const;
  inline bool IsSideEffectCallHandlerInfo() const;
  inline void SetNextCallHasNoSideEffect();
  // Returns whether or not the next call can be side effect free.
  // Calling this will change the state back to having a side effect.
  inline bool NextCallHasNoSideEffect();

  // Dispatched behavior.
  DECL_PRINTER(CallHandlerInfo)
  DECL_VERIFIER(CallHandlerInfo)

  Address redirected_callback() const;

  TQ_OBJECT_CONSTRUCTORS(CallHandlerInfo)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_API_CALLBACKS_H_
