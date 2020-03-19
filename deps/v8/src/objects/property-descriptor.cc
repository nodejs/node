// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/property-descriptor.h"

#include "src/execution/isolate-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/init/bootstrapper.h"
#include "src/objects/lookup.h"
#include "src/objects/objects-inl.h"
#include "src/objects/property-descriptor-object-inl.h"

namespace v8 {
namespace internal {

namespace {

// Helper function for ToPropertyDescriptor. Comments describe steps for
// "enumerable", other properties are handled the same way.
// Returns false if an exception was thrown.
bool GetPropertyIfPresent(Handle<JSReceiver> receiver, Handle<String> name,
                          Handle<Object>* value) {
  LookupIterator it(receiver->GetIsolate(), receiver, name, receiver);
  // 4. Let hasEnumerable be HasProperty(Obj, "enumerable").
  Maybe<bool> has_property = JSReceiver::HasProperty(&it);
  // 5. ReturnIfAbrupt(hasEnumerable).
  if (has_property.IsNothing()) return false;
  // 6. If hasEnumerable is true, then
  if (has_property.FromJust() == true) {
    // 6a. Let enum be ToBoolean(Get(Obj, "enumerable")).
    // 6b. ReturnIfAbrupt(enum).
    if (!Object::GetProperty(&it).ToHandle(value)) return false;
  }
  return true;
}

// Helper function for ToPropertyDescriptor. Handles the case of "simple"
// objects: nothing on the prototype chain, just own fast data properties.
// Must not have observable side effects, because the slow path will restart
// the entire conversion!
bool ToPropertyDescriptorFastPath(Isolate* isolate, Handle<JSReceiver> obj,
                                  PropertyDescriptor* desc) {
  if (!obj->IsJSObject()) return false;
  Map map = Handle<JSObject>::cast(obj)->map();
  if (map.instance_type() != JS_OBJECT_TYPE) return false;
  if (map.is_access_check_needed()) return false;
  if (map.prototype() != *isolate->initial_object_prototype()) return false;
  // During bootstrapping, the object_function_prototype_map hasn't been
  // set up yet.
  if (isolate->bootstrapper()->IsActive()) return false;
  if (JSObject::cast(map.prototype()).map() !=
      isolate->native_context()->object_function_prototype_map()) {
    return false;
  }
  // TODO(jkummerow): support dictionary properties?
  if (map.is_dictionary_map()) return false;
  Handle<DescriptorArray> descs =
      Handle<DescriptorArray>(map.instance_descriptors(), isolate);
  for (InternalIndex i : map.IterateOwnDescriptors()) {
    PropertyDetails details = descs->GetDetails(i);
    Handle<Object> value;
    if (details.location() == kField) {
      if (details.kind() == kData) {
        value = JSObject::FastPropertyAt(Handle<JSObject>::cast(obj),
                                         details.representation(),
                                         FieldIndex::ForDescriptor(map, i));
      } else {
        DCHECK_EQ(kAccessor, details.kind());
        // Bail out to slow path.
        return false;
      }

    } else {
      DCHECK_EQ(kDescriptor, details.location());
      if (details.kind() == kData) {
        value = handle(descs->GetStrongValue(i), isolate);
      } else {
        DCHECK_EQ(kAccessor, details.kind());
        // Bail out to slow path.
        return false;
      }
    }
    Name key = descs->GetKey(i);
    ReadOnlyRoots roots(isolate);
    if (key == roots.enumerable_string()) {
      desc->set_enumerable(value->BooleanValue(isolate));
    } else if (key == roots.configurable_string()) {
      desc->set_configurable(value->BooleanValue(isolate));
    } else if (key == roots.value_string()) {
      desc->set_value(value);
    } else if (key == roots.writable_string()) {
      desc->set_writable(value->BooleanValue(isolate));
    } else if (key == roots.get_string()) {
      // Bail out to slow path to throw an exception if necessary.
      if (!value->IsCallable()) return false;
      desc->set_get(value);
    } else if (key == roots.set_string()) {
      // Bail out to slow path to throw an exception if necessary.
      if (!value->IsCallable()) return false;
      desc->set_set(value);
    }
  }
  if ((desc->has_get() || desc->has_set()) &&
      (desc->has_value() || desc->has_writable())) {
    // Bail out to slow path to throw an exception.
    return false;
  }
  return true;
}

void CreateDataProperty(Handle<JSObject> object, Handle<String> name,
                        Handle<Object> value) {
  LookupIterator it(object->GetIsolate(), object, name, object,
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  Maybe<bool> result = JSObject::CreateDataProperty(&it, value);
  CHECK(result.IsJust() && result.FromJust());
}

}  // namespace

// ES6 6.2.4.4 "FromPropertyDescriptor"
Handle<Object> PropertyDescriptor::ToObject(Isolate* isolate) {
  DCHECK(!(PropertyDescriptor::IsAccessorDescriptor(this) &&
           PropertyDescriptor::IsDataDescriptor(this)));
  Factory* factory = isolate->factory();
  if (IsRegularAccessorProperty()) {
    // Fast case for regular accessor properties.
    Handle<JSObject> result = factory->NewJSObjectFromMap(
        isolate->accessor_property_descriptor_map());
    result->InObjectPropertyAtPut(JSAccessorPropertyDescriptor::kGetIndex,
                                  *get());
    result->InObjectPropertyAtPut(JSAccessorPropertyDescriptor::kSetIndex,
                                  *set());
    result->InObjectPropertyAtPut(
        JSAccessorPropertyDescriptor::kEnumerableIndex,
        isolate->heap()->ToBoolean(enumerable()));
    result->InObjectPropertyAtPut(
        JSAccessorPropertyDescriptor::kConfigurableIndex,
        isolate->heap()->ToBoolean(configurable()));
    return result;
  }
  if (IsRegularDataProperty()) {
    // Fast case for regular data properties.
    Handle<JSObject> result =
        factory->NewJSObjectFromMap(isolate->data_property_descriptor_map());
    result->InObjectPropertyAtPut(JSDataPropertyDescriptor::kValueIndex,
                                  *value());
    result->InObjectPropertyAtPut(JSDataPropertyDescriptor::kWritableIndex,
                                  isolate->heap()->ToBoolean(writable()));
    result->InObjectPropertyAtPut(JSDataPropertyDescriptor::kEnumerableIndex,
                                  isolate->heap()->ToBoolean(enumerable()));
    result->InObjectPropertyAtPut(JSDataPropertyDescriptor::kConfigurableIndex,
                                  isolate->heap()->ToBoolean(configurable()));
    return result;
  }
  Handle<JSObject> result = factory->NewJSObject(isolate->object_function());
  if (has_value()) {
    CreateDataProperty(result, factory->value_string(), value());
  }
  if (has_writable()) {
    CreateDataProperty(result, factory->writable_string(),
                       factory->ToBoolean(writable()));
  }
  if (has_get()) {
    CreateDataProperty(result, factory->get_string(), get());
  }
  if (has_set()) {
    CreateDataProperty(result, factory->set_string(), set());
  }
  if (has_enumerable()) {
    CreateDataProperty(result, factory->enumerable_string(),
                       factory->ToBoolean(enumerable()));
  }
  if (has_configurable()) {
    CreateDataProperty(result, factory->configurable_string(),
                       factory->ToBoolean(configurable()));
  }
  return result;
}

// ES6 6.2.4.5
// Returns false in case of exception.
// static
bool PropertyDescriptor::ToPropertyDescriptor(Isolate* isolate,
                                              Handle<Object> obj,
                                              PropertyDescriptor* desc) {
  // 1. ReturnIfAbrupt(Obj).
  // 2. If Type(Obj) is not Object, throw a TypeError exception.
  if (!obj->IsJSReceiver()) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kPropertyDescObject, obj));
    return false;
  }
  // 3. Let desc be a new Property Descriptor that initially has no fields.
  DCHECK(desc->is_empty());

  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(obj);
  if (ToPropertyDescriptorFastPath(isolate, receiver, desc)) {
    return true;
  }

  // enumerable?
  Handle<Object> enumerable;
  // 4 through 6b.
  if (!GetPropertyIfPresent(receiver, isolate->factory()->enumerable_string(),
                            &enumerable)) {
    return false;
  }
  // 6c. Set the [[Enumerable]] field of desc to enum.
  if (!enumerable.is_null()) {
    desc->set_enumerable(enumerable->BooleanValue(isolate));
  }

  // configurable?
  Handle<Object> configurable;
  // 7 through 9b.
  if (!GetPropertyIfPresent(receiver, isolate->factory()->configurable_string(),
                            &configurable)) {
    return false;
  }
  // 9c. Set the [[Configurable]] field of desc to conf.
  if (!configurable.is_null()) {
    desc->set_configurable(configurable->BooleanValue(isolate));
  }

  // value?
  Handle<Object> value;
  // 10 through 12b.
  if (!GetPropertyIfPresent(receiver, isolate->factory()->value_string(),
                            &value)) {
    return false;
  }
  // 12c. Set the [[Value]] field of desc to value.
  if (!value.is_null()) desc->set_value(value);

  // writable?
  Handle<Object> writable;
  // 13 through 15b.
  if (!GetPropertyIfPresent(receiver, isolate->factory()->writable_string(),
                            &writable)) {
    return false;
  }
  // 15c. Set the [[Writable]] field of desc to writable.
  if (!writable.is_null()) desc->set_writable(writable->BooleanValue(isolate));

  // getter?
  Handle<Object> getter;
  // 16 through 18b.
  if (!GetPropertyIfPresent(receiver, isolate->factory()->get_string(),
                            &getter)) {
    return false;
  }
  if (!getter.is_null()) {
    // 18c. If IsCallable(getter) is false and getter is not undefined,
    // throw a TypeError exception.
    if (!getter->IsCallable() && !getter->IsUndefined(isolate)) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kObjectGetterCallable, getter));
      return false;
    }
    // 18d. Set the [[Get]] field of desc to getter.
    desc->set_get(getter);
  }
  // setter?
  Handle<Object> setter;
  // 19 through 21b.
  if (!GetPropertyIfPresent(receiver, isolate->factory()->set_string(),
                            &setter)) {
    return false;
  }
  if (!setter.is_null()) {
    // 21c. If IsCallable(setter) is false and setter is not undefined,
    // throw a TypeError exception.
    if (!setter->IsCallable() && !setter->IsUndefined(isolate)) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kObjectSetterCallable, setter));
      return false;
    }
    // 21d. Set the [[Set]] field of desc to setter.
    desc->set_set(setter);
  }

  // 22. If either desc.[[Get]] or desc.[[Set]] is present, then
  // 22a. If either desc.[[Value]] or desc.[[Writable]] is present,
  // throw a TypeError exception.
  if ((desc->has_get() || desc->has_set()) &&
      (desc->has_value() || desc->has_writable())) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kValueAndAccessor, obj));
    return false;
  }

  // 23. Return desc.
  return true;
}

// ES6 6.2.4.6
// static
void PropertyDescriptor::CompletePropertyDescriptor(Isolate* isolate,
                                                    PropertyDescriptor* desc) {
  // 1. ReturnIfAbrupt(Desc).
  // 2. Assert: Desc is a Property Descriptor.
  // 3. Let like be Record{
  //        [[Value]]: undefined, [[Writable]]: false,
  //        [[Get]]: undefined, [[Set]]: undefined,
  //        [[Enumerable]]: false, [[Configurable]]: false}.
  // 4. If either IsGenericDescriptor(Desc) or IsDataDescriptor(Desc) is true,
  // then:
  if (!IsAccessorDescriptor(desc)) {
    // 4a. If Desc does not have a [[Value]] field, set Desc.[[Value]] to
    //     like.[[Value]].
    if (!desc->has_value()) {
      desc->set_value(isolate->factory()->undefined_value());
    }
    // 4b. If Desc does not have a [[Writable]] field, set Desc.[[Writable]]
    //     to like.[[Writable]].
    if (!desc->has_writable()) desc->set_writable(false);
  } else {
    // 5. Else,
    // 5a. If Desc does not have a [[Get]] field, set Desc.[[Get]] to
    //     like.[[Get]].
    if (!desc->has_get()) {
      desc->set_get(isolate->factory()->undefined_value());
    }
    // 5b. If Desc does not have a [[Set]] field, set Desc.[[Set]] to
    //     like.[[Set]].
    if (!desc->has_set()) {
      desc->set_set(isolate->factory()->undefined_value());
    }
  }
  // 6. If Desc does not have an [[Enumerable]] field, set
  //    Desc.[[Enumerable]] to like.[[Enumerable]].
  if (!desc->has_enumerable()) desc->set_enumerable(false);
  // 7. If Desc does not have a [[Configurable]] field, set
  //    Desc.[[Configurable]] to like.[[Configurable]].
  if (!desc->has_configurable()) desc->set_configurable(false);
  // 8. Return Desc.
}

Handle<PropertyDescriptorObject> PropertyDescriptor::ToPropertyDescriptorObject(
    Isolate* isolate) {
  Handle<PropertyDescriptorObject> obj =
      isolate->factory()->NewPropertyDescriptorObject();

  int flags =
      PropertyDescriptorObject::IsEnumerableBit::encode(enumerable_) |
      PropertyDescriptorObject::HasEnumerableBit::encode(has_enumerable_) |
      PropertyDescriptorObject::IsConfigurableBit::encode(configurable_) |
      PropertyDescriptorObject::HasConfigurableBit::encode(has_configurable_) |
      PropertyDescriptorObject::IsWritableBit::encode(writable_) |
      PropertyDescriptorObject::HasWritableBit::encode(has_writable_) |
      PropertyDescriptorObject::HasValueBit::encode(has_value()) |
      PropertyDescriptorObject::HasGetBit::encode(has_get()) |
      PropertyDescriptorObject::HasSetBit::encode(has_set());

  obj->set_flags(Smi::FromInt(flags));

  if (has_value()) obj->set_value(*value_);
  if (has_get()) obj->set_get(*get_);
  if (has_set()) obj->set_set(*set_);

  return obj;
}

}  // namespace internal
}  // namespace v8
