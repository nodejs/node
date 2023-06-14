// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-struct.h"

#include "src/objects/lookup-inl.h"
#include "src/objects/map-inl.h"
#include "src/objects/property-descriptor.h"

namespace v8 {
namespace internal {

// static
Maybe<bool> AlwaysSharedSpaceJSObject::DefineOwnProperty(
    Isolate* isolate, Handle<AlwaysSharedSpaceJSObject> shared_obj,
    Handle<Object> key, PropertyDescriptor* desc,
    Maybe<ShouldThrow> should_throw) {
  // Shared objects are designed to have fixed layout, i.e. their maps are
  // effectively immutable. They are constructed seal, but the semantics of
  // ordinary ECMAScript objects allow writable properties to be upgraded to
  // non-writable properties. This upgrade violates the fixed layout invariant
  // and is disallowed.

  DCHECK(key->IsName() || key->IsNumber());  // |key| is a PropertyKey.
  PropertyKey lookup_key(isolate, key);
  LookupIterator it(isolate, shared_obj, lookup_key, LookupIterator::OWN);
  PropertyDescriptor current;
  MAYBE_RETURN(GetOwnPropertyDescriptor(&it, &current), Nothing<bool>());

  // The only redefinition allowed is to set the value if all attributes match.
  if (!it.IsFound() ||
      PropertyDescriptor::IsDataDescriptor(desc) !=
          PropertyDescriptor::IsDataDescriptor(&current) ||
      desc->ToAttributes() != current.ToAttributes()) {
    DCHECK(!shared_obj->map().is_extensible());
    RETURN_FAILURE(isolate, GetShouldThrow(isolate, should_throw),
                   NewTypeError(MessageTemplate::kDefineDisallowedFixedLayout,
                                it.GetName()));
  }
  DCHECK(it.property_attributes() == desc->ToAttributes());
  if (desc->has_value()) {
    return Object::SetDataProperty(&it, desc->value());
  }
  return Just(true);
}

Maybe<bool> AlwaysSharedSpaceJSObject::HasInstance(
    Isolate* isolate, Handle<JSFunction> constructor, Handle<Object> object) {
  if (!constructor->has_prototype_slot() || !constructor->has_initial_map() ||
      !object->IsJSReceiver()) {
    return Just(false);
  }
  Handle<Map> constructor_map = handle(constructor->initial_map(), isolate);
  PrototypeIterator iter(isolate, Handle<JSReceiver>::cast(object),
                         kStartAtReceiver);
  Handle<Map> current_map;
  while (true) {
    current_map = handle(PrototypeIterator::GetCurrent(iter)->map(), isolate);
    if (current_map.is_identical_to(constructor_map)) {
      return Just(true);
    }
    if (!iter.AdvanceFollowingProxies()) return Nothing<bool>();
    if (iter.IsAtEnd()) return Just(false);
  }
}

}  // namespace internal
}  // namespace v8
