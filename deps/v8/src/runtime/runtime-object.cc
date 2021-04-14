// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/prettyprinter.h"
#include "src/common/globals.h"
#include "src/common/message-template.h"
#include "src/debug/debug.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/messages.h"
#include "src/handles/maybe-handles.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/heap/memory-chunk.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/map-updater.h"
#include "src/objects/property-descriptor-object.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/swiss-name-dictionary-inl.h"
#include "src/runtime/runtime-utils.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

MaybeHandle<Object> Runtime::GetObjectProperty(Isolate* isolate,
                                               Handle<Object> holder,
                                               Handle<Object> key,
                                               Handle<Object> receiver,
                                               bool* is_found) {
  if (receiver.is_null()) {
    receiver = holder;
  }
  if (holder->IsNullOrUndefined(isolate)) {
    ErrorUtils::ThrowLoadFromNullOrUndefined(isolate, holder, key);
    return MaybeHandle<Object>();
  }

  bool success = false;
  LookupIterator::Key lookup_key(isolate, key, &success);
  if (!success) return MaybeHandle<Object>();
  LookupIterator it = LookupIterator(isolate, receiver, lookup_key, holder);

  MaybeHandle<Object> result = Object::GetProperty(&it);
  if (is_found) *is_found = it.IsFound();

  if (!it.IsFound() && key->IsSymbol() &&
      Symbol::cast(*key).is_private_name()) {
    Handle<Symbol> sym = Handle<Symbol>::cast(key);
    Handle<Object> name(sym->description(), isolate);
    DCHECK(name->IsString());
    Handle<String> name_string = Handle<String>::cast(name);
    if (sym->IsPrivateBrand()) {
      Handle<String> class_name = (name_string->length() == 0)
                                      ? isolate->factory()->anonymous_string()
                                      : name_string;
      THROW_NEW_ERROR(isolate,
                      NewTypeError(MessageTemplate::kInvalidPrivateBrand,
                                   class_name, holder),
                      Object);
    }
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kInvalidPrivateMemberRead,
                                 name_string, holder),
                    Object);
  }
  return result;
}

MaybeHandle<Object> Runtime::HasProperty(Isolate* isolate,
                                         Handle<Object> object,
                                         Handle<Object> key) {
  // Check that {object} is actually a receiver.
  if (!object->IsJSReceiver()) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kInvalidInOperatorUse, key, object),
        Object);
  }
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(object);

  // Convert the {key} to a name.
  Handle<Name> name;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, name, Object::ToName(isolate, key),
                             Object);

  // Lookup the {name} on {receiver}.
  Maybe<bool> maybe = JSReceiver::HasProperty(receiver, name);
  if (maybe.IsNothing()) return MaybeHandle<Object>();
  return maybe.FromJust() ? ReadOnlyRoots(isolate).true_value_handle()
                          : ReadOnlyRoots(isolate).false_value_handle();
}

namespace {

// This function sets the sentinel value in a deleted field. Thes sentinel has
// to look like a proper standalone object because the slack tracking may
// complete at any time. For this reason we use the filler map word.
// If V8_MAP_PACKING is enabled, then the filler map word is a packed filler
// map. Otherwise, the filler map word is the same as the filler map.
inline void ClearField(Isolate* isolate, JSObject object, FieldIndex index) {
  if (index.is_inobject()) {
    MapWord filler_map_word =
        ReadOnlyRoots(isolate).one_pointer_filler_map_word();
#ifndef V8_MAP_PACKING
    DCHECK_EQ(filler_map_word.ToMap(),
              ReadOnlyRoots(isolate).one_pointer_filler_map());
#endif
    int offset = index.offset();
    TaggedField<MapWord>::Release_Store(object, offset, filler_map_word);
  } else {
    object.property_array().set(
        index.outobject_array_index(),
        ReadOnlyRoots(isolate).one_pointer_filler_map());
  }
}

bool DeleteObjectPropertyFast(Isolate* isolate, Handle<JSReceiver> receiver,
                              Handle<Object> raw_key) {
  // This implements a special case for fast property deletion: when the
  // last property in an object is deleted, then instead of normalizing
  // the properties, we can undo the last map transition, with a few
  // prerequisites:
  // (1) The receiver must be a regular object and the key a unique name.
  Handle<Map> receiver_map(receiver->map(), isolate);
  if (receiver_map->IsSpecialReceiverMap()) return false;
  if (!raw_key->IsUniqueName()) return false;
  Handle<Name> key = Handle<Name>::cast(raw_key);
  // (2) The property to be deleted must be the last property.
  int nof = receiver_map->NumberOfOwnDescriptors();
  if (nof == 0) return false;
  InternalIndex descriptor(nof - 1);
  Handle<DescriptorArray> descriptors(
      receiver_map->instance_descriptors(isolate), isolate);
  if (descriptors->GetKey(descriptor) != *key) return false;
  // (3) The property to be deleted must be deletable.
  PropertyDetails details = descriptors->GetDetails(descriptor);
  if (!details.IsConfigurable()) return false;
  // (4) The map must have a back pointer.
  Handle<Object> backpointer(receiver_map->GetBackPointer(), isolate);
  if (!backpointer->IsMap()) return false;
  Handle<Map> parent_map = Handle<Map>::cast(backpointer);
  // (5) The last transition must have been caused by adding a property
  // (and not any kind of special transition).
  if (parent_map->NumberOfOwnDescriptors() != nof - 1) return false;

  // Preconditions successful. No more bailouts after this point.

  // If the {descriptor} was "const" so far, we need to update the
  // {receiver_map} here, otherwise we could get the constants wrong, i.e.
  //
  //   o.x = 1;
  //   delete o.x;
  //   o.x = 2;
  //
  // could trick V8 into thinking that `o.x` is still 1 even after the second
  // assignment.
  if (details.constness() == PropertyConstness::kConst &&
      details.location() == kField) {
    Handle<FieldType> field_type(descriptors->GetFieldType(descriptor),
                                 isolate);
    MapUpdater::GeneralizeField(isolate, receiver_map, descriptor,
                                PropertyConstness::kMutable,
                                details.representation(), field_type);
    DCHECK_EQ(PropertyConstness::kMutable,
              descriptors->GetDetails(descriptor).constness());
  }

  // Zap the property to avoid keeping objects alive. Zapping is not necessary
  // for properties stored in the descriptor array.
  if (details.location() == kField) {
    DisallowGarbageCollection no_gc;

    // Invalidate slots manually later in case we delete an in-object tagged
    // property. In this case we might later store an untagged value in the
    // recorded slot.
    isolate->heap()->NotifyObjectLayoutChange(*receiver, no_gc,
                                              InvalidateRecordedSlots::kNo);
    FieldIndex index =
        FieldIndex::ForPropertyIndex(*receiver_map, details.field_index());
    // Special case deleting the last out-of object property.
    if (!index.is_inobject() && index.outobject_array_index() == 0) {
      DCHECK(!parent_map->HasOutOfObjectProperties());
      // Clear out the properties backing store.
      receiver->SetProperties(ReadOnlyRoots(isolate).empty_fixed_array());
    } else {
      ClearField(isolate, JSObject::cast(*receiver), index);
      // We must clear any recorded slot for the deleted property, because
      // subsequent object modifications might put a raw double there.
      // Slot clearing is the reason why this entire function cannot currently
      // be implemented in the DeleteProperty stub.
      if (index.is_inobject()) {
        // We need to clear the recorded slot in this case because in-object
        // slack tracking might not be finished. This ensures that we don't
        // have recorded slots in free space.
        isolate->heap()->ClearRecordedSlot(*receiver,
                                           receiver->RawField(index.offset()));
        MemoryChunk* chunk = MemoryChunk::FromHeapObject(*receiver);
        chunk->InvalidateRecordedSlots(*receiver);
      }
    }
  }
  // If the {receiver_map} was marked stable before, then there could be
  // optimized code that depends on the assumption that no object that
  // reached this {receiver_map} transitions away from it without triggering
  // the "deoptimize dependent code" mechanism.
  receiver_map->NotifyLeafMapLayoutChange(isolate);
  // Finally, perform the map rollback.
  receiver->synchronized_set_map(*parent_map);
#if VERIFY_HEAP
  receiver->HeapObjectVerify(isolate);
  receiver->property_array().PropertyArrayVerify(isolate);
#endif
  return true;
}

}  // namespace

Maybe<bool> Runtime::DeleteObjectProperty(Isolate* isolate,
                                          Handle<JSReceiver> receiver,
                                          Handle<Object> key,
                                          LanguageMode language_mode) {
  if (DeleteObjectPropertyFast(isolate, receiver, key)) return Just(true);

  bool success = false;
  LookupIterator::Key lookup_key(isolate, key, &success);
  if (!success) return Nothing<bool>();
  LookupIterator it(isolate, receiver, lookup_key, LookupIterator::OWN);

  return JSReceiver::DeleteProperty(&it, language_mode);
}

// ES #sec-object.keys
RUNTIME_FUNCTION(Runtime_ObjectKeys) {
  HandleScope scope(isolate);
  Handle<Object> object = args.at(0);

  // Convert the {object} to a proper {receiver}.
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));

  // Collect the own keys for the {receiver}.
  Handle<FixedArray> keys;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, keys,
      KeyAccumulator::GetKeys(receiver, KeyCollectionMode::kOwnOnly,
                              ENUMERABLE_STRINGS,
                              GetKeysConversion::kConvertToString));
  return *keys;
}

// ES #sec-object.getOwnPropertyNames
RUNTIME_FUNCTION(Runtime_ObjectGetOwnPropertyNames) {
  HandleScope scope(isolate);
  Handle<Object> object = args.at(0);

  // Convert the {object} to a proper {receiver}.
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));

  // Collect the own keys for the {receiver}.
  // TODO(v8:9401): We should extend the fast path of KeyAccumulator::GetKeys to
  // also use fast path even when filter = SKIP_SYMBOLS.
  Handle<FixedArray> keys;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, keys,
      KeyAccumulator::GetKeys(receiver, KeyCollectionMode::kOwnOnly,
                              SKIP_SYMBOLS,
                              GetKeysConversion::kConvertToString));
  return *keys;
}

RUNTIME_FUNCTION(Runtime_ObjectGetOwnPropertyNamesTryFast) {
  HandleScope scope(isolate);
  Handle<Object> object = args.at(0);

  // Convert the {object} to a proper {receiver}.
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));

  Handle<Map> map(receiver->map(), isolate);

  int nod = map->NumberOfOwnDescriptors();
  Handle<FixedArray> keys;
  if (nod != 0 && map->NumberOfEnumerableProperties() == nod) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, keys,
        KeyAccumulator::GetKeys(receiver, KeyCollectionMode::kOwnOnly,
                                ENUMERABLE_STRINGS,
                                GetKeysConversion::kConvertToString));
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, keys,
        KeyAccumulator::GetKeys(receiver, KeyCollectionMode::kOwnOnly,
                                SKIP_SYMBOLS,
                                GetKeysConversion::kConvertToString));
  }

  return *keys;
}

// ES6 19.1.3.2
RUNTIME_FUNCTION(Runtime_ObjectHasOwnProperty) {
  HandleScope scope(isolate);
  Handle<Object> property = args.at(1);

  // TODO(ishell): To improve performance, consider performing the to-string
  // conversion of {property} before calling into the runtime.
  bool success;
  LookupIterator::Key key(isolate, property, &success);
  if (!success) return ReadOnlyRoots(isolate).exception();

  Handle<Object> object = args.at(0);

  if (object->IsJSModuleNamespace()) {
    if (key.is_element()) {
      // Namespace objects can't have indexed properties.
      return ReadOnlyRoots(isolate).false_value();
    }
    LookupIterator it(isolate, object, key, LookupIterator::OWN);
    PropertyDescriptor desc;
    Maybe<bool> result = JSReceiver::GetOwnPropertyDescriptor(&it, &desc);
    if (!result.IsJust()) return ReadOnlyRoots(isolate).exception();
    return isolate->heap()->ToBoolean(result.FromJust());

  } else if (object->IsJSObject()) {
    Handle<JSObject> js_obj = Handle<JSObject>::cast(object);
    // Fast case: either the key is a real named property or it is not
    // an array index and there are no interceptors or hidden
    // prototypes.
    // TODO(jkummerow): Make JSReceiver::HasOwnProperty fast enough to
    // handle all cases directly (without this custom fast path).
    {
      LookupIterator::Configuration c = LookupIterator::OWN_SKIP_INTERCEPTOR;
      LookupIterator it(isolate, js_obj, key, js_obj, c);
      Maybe<bool> maybe = JSReceiver::HasProperty(&it);
      if (maybe.IsNothing()) return ReadOnlyRoots(isolate).exception();
      DCHECK(!isolate->has_pending_exception());
      if (maybe.FromJust()) return ReadOnlyRoots(isolate).true_value();
    }

    Map map = js_obj->map();
    if (!map.IsJSGlobalProxyMap() &&
        (key.is_element() && key.index() <= JSObject::kMaxElementIndex
             ? !map.has_indexed_interceptor()
             : !map.has_named_interceptor())) {
      return ReadOnlyRoots(isolate).false_value();
    }

    // Slow case.
    LookupIterator it(isolate, js_obj, key, js_obj, LookupIterator::OWN);
    Maybe<bool> maybe = JSReceiver::HasProperty(&it);
    if (maybe.IsNothing()) return ReadOnlyRoots(isolate).exception();
    DCHECK(!isolate->has_pending_exception());
    return isolate->heap()->ToBoolean(maybe.FromJust());

  } else if (object->IsJSProxy()) {
    LookupIterator it(isolate, object, key, Handle<JSProxy>::cast(object),
                      LookupIterator::OWN);
    Maybe<PropertyAttributes> attributes =
        JSReceiver::GetPropertyAttributes(&it);
    if (attributes.IsNothing()) return ReadOnlyRoots(isolate).exception();
    return isolate->heap()->ToBoolean(attributes.FromJust() != ABSENT);

  } else if (object->IsString()) {
    return isolate->heap()->ToBoolean(
        key.is_element()
            ? key.index() < static_cast<size_t>(String::cast(*object).length())
            : key.name()->Equals(ReadOnlyRoots(isolate).length_string()));
  } else if (object->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kUndefinedOrNullToObject));
  }

  return ReadOnlyRoots(isolate).false_value();
}

RUNTIME_FUNCTION(Runtime_HasOwnConstDataProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, property, 1);

  bool success;
  LookupIterator::Key key(isolate, property, &success);
  if (!success) return ReadOnlyRoots(isolate).undefined_value();

  if (object->IsJSObject()) {
    Handle<JSObject> js_obj = Handle<JSObject>::cast(object);
    LookupIterator it(isolate, js_obj, key, js_obj, LookupIterator::OWN);

    switch (it.state()) {
      case LookupIterator::NOT_FOUND:
        return isolate->heap()->ToBoolean(false);
      case LookupIterator::DATA:
        return isolate->heap()->ToBoolean(it.constness() ==
                                          PropertyConstness::kConst);
      default:
        return ReadOnlyRoots(isolate).undefined_value();
    }
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_IsDictPropertyConstTrackingEnabled) {
  return isolate->heap()->ToBoolean(V8_DICT_PROPERTY_CONST_TRACKING_BOOL);
}

RUNTIME_FUNCTION(Runtime_AddDictionaryProperty) {
  HandleScope scope(isolate);
  Handle<JSObject> receiver = args.at<JSObject>(0);
  Handle<Name> name = args.at<Name>(1);
  Handle<Object> value = args.at(2);

  DCHECK(name->IsUniqueName());

  PropertyDetails property_details(
      kData, NONE, PropertyDetails::kConstIfDictConstnessTracking);
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    Handle<SwissNameDictionary> dictionary(
        receiver->property_dictionary_swiss(), isolate);
    dictionary = SwissNameDictionary::Add(isolate, dictionary, name, value,
                                          property_details);
    receiver->SetProperties(*dictionary);
  } else {
    Handle<NameDictionary> dictionary(receiver->property_dictionary(), isolate);
    dictionary =
        NameDictionary::Add(isolate, dictionary, name, value, property_details);
    receiver->SetProperties(*dictionary);
  }

  return *value;
}

// ES6 section 19.1.2.2 Object.create ( O [ , Properties ] )
// TODO(verwaest): Support the common cases with precached map directly in
// an Object.create stub.
RUNTIME_FUNCTION(Runtime_ObjectCreate) {
  HandleScope scope(isolate);
  Handle<Object> prototype = args.at(0);
  Handle<Object> properties = args.at(1);
  Handle<JSObject> obj;
  // 1. If Type(O) is neither Object nor Null, throw a TypeError exception.
  if (!prototype->IsNull(isolate) && !prototype->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kProtoObjectOrNull, prototype));
  }
  // 2. Let obj be ObjectCreate(O).
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, obj, JSObject::ObjectCreate(isolate, prototype));

  // 3. If Properties is not undefined, then
  if (!properties->IsUndefined(isolate)) {
    // a. Return ? ObjectDefineProperties(obj, Properties).
    // Define the properties if properties was specified and is not undefined.
    RETURN_RESULT_OR_FAILURE(
        isolate, JSReceiver::DefineProperties(isolate, obj, properties));
  }
  // 4. Return obj.
  return *obj;
}

MaybeHandle<Object> Runtime::SetObjectProperty(
    Isolate* isolate, Handle<Object> object, Handle<Object> key,
    Handle<Object> value, StoreOrigin store_origin,
    Maybe<ShouldThrow> should_throw) {
  if (object->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kNonObjectPropertyStore, key, object),
        Object);
  }

  // Check if the given key is an array index.
  bool success = false;
  LookupIterator::Key lookup_key(isolate, key, &success);
  if (!success) return MaybeHandle<Object>();
  LookupIterator it(isolate, object, lookup_key);

  if (!it.IsFound() && key->IsSymbol() &&
      Symbol::cast(*key).is_private_name()) {
    Handle<Object> name_string(Symbol::cast(*key).description(), isolate);
    DCHECK(name_string->IsString());
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kInvalidPrivateMemberWrite,
                                 name_string, object),
                    Object);
  }

  MAYBE_RETURN_NULL(
      Object::SetProperty(&it, value, store_origin, should_throw));

  return value;
}

RUNTIME_FUNCTION(Runtime_InternalSetPrototype) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  MAYBE_RETURN(JSReceiver::SetPrototype(obj, prototype, false, kThrowOnError),
               ReadOnlyRoots(isolate).exception());
  return *obj;
}

RUNTIME_FUNCTION(Runtime_OptimizeObjectForAddingMultipleProperties) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_SMI_ARG_CHECKED(properties, 1);
  // Conservative upper limit to prevent fuzz tests from going OOM.
  if (properties > 100000) return isolate->ThrowIllegalOperation();
  if (object->HasFastProperties() && !object->IsJSGlobalProxy()) {
    JSObject::NormalizeProperties(isolate, object, KEEP_INOBJECT_PROPERTIES,
                                  properties, "OptimizeForAdding");
  }
  return *object;
}

RUNTIME_FUNCTION(Runtime_ObjectValues) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);

  Handle<FixedArray> values;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, values,
      JSReceiver::GetOwnValues(receiver, PropertyFilter::ENUMERABLE_STRINGS,
                               true));
  return *isolate->factory()->NewJSArrayWithElements(values);
}

RUNTIME_FUNCTION(Runtime_ObjectValuesSkipFastPath) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);

  Handle<FixedArray> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, value,
      JSReceiver::GetOwnValues(receiver, PropertyFilter::ENUMERABLE_STRINGS,
                               false));
  return *isolate->factory()->NewJSArrayWithElements(value);
}

RUNTIME_FUNCTION(Runtime_ObjectEntries) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);

  Handle<FixedArray> entries;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, entries,
      JSReceiver::GetOwnEntries(receiver, PropertyFilter::ENUMERABLE_STRINGS,
                                true));
  return *isolate->factory()->NewJSArrayWithElements(entries);
}

RUNTIME_FUNCTION(Runtime_ObjectEntriesSkipFastPath) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);

  Handle<FixedArray> entries;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, entries,
      JSReceiver::GetOwnEntries(receiver, PropertyFilter::ENUMERABLE_STRINGS,
                                false));
  return *isolate->factory()->NewJSArrayWithElements(entries);
}

RUNTIME_FUNCTION(Runtime_ObjectIsExtensible) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);

  Maybe<bool> result =
      object->IsJSReceiver()
          ? JSReceiver::IsExtensible(Handle<JSReceiver>::cast(object))
          : Just(false);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}

RUNTIME_FUNCTION(Runtime_JSReceiverPreventExtensionsThrow) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);

  MAYBE_RETURN(JSReceiver::PreventExtensions(Handle<JSReceiver>::cast(object),
                                             kThrowOnError),
               ReadOnlyRoots(isolate).exception());
  return *object;
}

RUNTIME_FUNCTION(Runtime_JSReceiverPreventExtensionsDontThrow) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);

  Maybe<bool> result = JSReceiver::PreventExtensions(
      Handle<JSReceiver>::cast(object), kDontThrow);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}

RUNTIME_FUNCTION(Runtime_JSReceiverGetPrototypeOf) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);

  RETURN_RESULT_OR_FAILURE(isolate,
                           JSReceiver::GetPrototype(isolate, receiver));
}

RUNTIME_FUNCTION(Runtime_JSReceiverSetPrototypeOfThrow) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, proto, 1);

  MAYBE_RETURN(JSReceiver::SetPrototype(object, proto, true, kThrowOnError),
               ReadOnlyRoots(isolate).exception());

  return *object;
}

RUNTIME_FUNCTION(Runtime_JSReceiverSetPrototypeOfDontThrow) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, proto, 1);

  Maybe<bool> result =
      JSReceiver::SetPrototype(object, proto, true, kDontThrow);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}

RUNTIME_FUNCTION(Runtime_GetProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3 || args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, holder_obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key_obj, 1);
  Handle<Object> receiver_obj = holder_obj;
  if (args.length() == 3) {
    CHECK(args[2].IsObject());
    receiver_obj = args.at<Object>(2);
  }

  // Fast cases for getting named properties of the holder JSObject
  // itself.
  //
  // The global proxy objects has to be excluded since LookupOwn on
  // the global proxy object can return a valid result even though the
  // global proxy object never has properties.  This is the case
  // because the global proxy object forwards everything to its hidden
  // prototype including own lookups.
  //
  // Additionally, we need to make sure that we do not cache results
  // for objects that require access checks.

  // Convert string-index keys to their number variant to avoid internalization
  // below; and speed up subsequent conversion to index.
  uint32_t index;
  if (key_obj->IsString() && String::cast(*key_obj).AsArrayIndex(&index)) {
    key_obj = isolate->factory()->NewNumberFromUint(index);
  }
  if (holder_obj->IsJSObject()) {
    if (!holder_obj->IsJSGlobalProxy() && !holder_obj->IsAccessCheckNeeded() &&
        key_obj->IsName()) {
      Handle<JSObject> holder = Handle<JSObject>::cast(holder_obj);
      Handle<Name> key = Handle<Name>::cast(key_obj);
      key_obj = key = isolate->factory()->InternalizeName(key);

      DisallowGarbageCollection no_gc;
      if (holder->IsJSGlobalObject()) {
        // Attempt dictionary lookup.
        GlobalDictionary dictionary =
            JSGlobalObject::cast(*holder).global_dictionary(kAcquireLoad);
        InternalIndex entry = dictionary.FindEntry(isolate, key);
        if (entry.is_found()) {
          PropertyCell cell = dictionary.CellAt(entry);
          if (cell.property_details().kind() == kData) {
            Object value = cell.value();
            if (!value.IsTheHole(isolate)) return value;
            // If value is the hole (meaning, absent) do the general lookup.
          }
        }
      } else if (!holder->HasFastProperties()) {
        // Attempt dictionary lookup.
        if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
          SwissNameDictionary dictionary = holder->property_dictionary_swiss();
          InternalIndex entry = dictionary.FindEntry(isolate, *key);
          if (entry.is_found() &&
              (dictionary.DetailsAt(entry).kind() == kData)) {
            return dictionary.ValueAt(entry);
          }
        } else {
          NameDictionary dictionary = holder->property_dictionary();
          InternalIndex entry = dictionary.FindEntry(isolate, key);
          if ((entry.is_found()) &&
              (dictionary.DetailsAt(entry).kind() == kData)) {
            return dictionary.ValueAt(entry);
          }
        }
      }
    } else if (key_obj->IsSmi()) {
      // JSObject without a name key. If the key is a Smi, check for a
      // definite out-of-bounds access to elements, which is a strong indicator
      // that subsequent accesses will also call the runtime. Proactively
      // transition elements to FAST_*_ELEMENTS to avoid excessive boxing of
      // doubles for those future calls in the case that the elements would
      // become PACKED_DOUBLE_ELEMENTS.
      Handle<JSObject> js_object = Handle<JSObject>::cast(holder_obj);
      ElementsKind elements_kind = js_object->GetElementsKind();
      if (IsDoubleElementsKind(elements_kind)) {
        if (Smi::ToInt(*key_obj) >= js_object->elements().length()) {
          elements_kind = IsHoleyElementsKind(elements_kind) ? HOLEY_ELEMENTS
                                                             : PACKED_ELEMENTS;
          JSObject::TransitionElementsKind(js_object, elements_kind);
        }
      } else {
        DCHECK(IsSmiOrObjectElementsKind(elements_kind) ||
               !IsFastElementsKind(elements_kind));
      }
    }
  } else if (holder_obj->IsString() && key_obj->IsSmi()) {
    // Fast case for string indexing using [] with a smi index.
    Handle<String> str = Handle<String>::cast(holder_obj);
    int index = Handle<Smi>::cast(key_obj)->value();
    if (index >= 0 && index < str->length()) {
      Factory* factory = isolate->factory();
      return *factory->LookupSingleCharacterStringFromCode(
          String::Flatten(isolate, str)->Get(index));
    }
  }

  // Fall back to GetObjectProperty.
  RETURN_RESULT_OR_FAILURE(
      isolate,
      Runtime::GetObjectProperty(isolate, holder_obj, key_obj, receiver_obj));
}

RUNTIME_FUNCTION(Runtime_SetKeyedProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Runtime::SetObjectProperty(isolate, object, key, value,
                                          StoreOrigin::kMaybeKeyed));
}

RUNTIME_FUNCTION(Runtime_SetNamedProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Runtime::SetObjectProperty(isolate, object, key, value,
                                          StoreOrigin::kNamed));
}

// Similar to DefineDataPropertyInLiteral, but does not update feedback, and
// and does not have a flags parameter for performing SetFunctionName().
//
// Currently, this is used for ObjectLiteral spread properties.
RUNTIME_FUNCTION(Runtime_StoreDataPropertyInLiteral) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);

  LookupIterator::Key lookup_key(isolate, key);
  LookupIterator it(isolate, object, lookup_key, LookupIterator::OWN);

  Maybe<bool> result = JSObject::DefineOwnPropertyIgnoreAttributes(
      &it, value, NONE, Just(kDontThrow));
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  DCHECK(result.IsJust());
  USE(result);

  return *value;
}

namespace {

// ES6 section 12.5.4.
Object DeleteProperty(Isolate* isolate, Handle<Object> object,
                      Handle<Object> key, LanguageMode language_mode) {
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));
  Maybe<bool> result =
      Runtime::DeleteObjectProperty(isolate, receiver, key, language_mode);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}

}  // namespace

RUNTIME_FUNCTION(Runtime_DeleteProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_SMI_ARG_CHECKED(language_mode, 2);
  return DeleteProperty(isolate, object, key,
                        static_cast<LanguageMode>(language_mode));
}

RUNTIME_FUNCTION(Runtime_ShrinkNameDictionary) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(NameDictionary, dictionary, 0);

  return *NameDictionary::Shrink(isolate, dictionary);
}

RUNTIME_FUNCTION(Runtime_ShrinkSwissNameDictionary) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(SwissNameDictionary, dictionary, 0);

  return *SwissNameDictionary::Shrink(isolate, dictionary);
}

// ES6 section 12.9.3, operator in.
RUNTIME_FUNCTION(Runtime_HasProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);

  // Check that {object} is actually a receiver.
  if (!object->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kInvalidInOperatorUse, key, object));
  }
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(object);

  // Convert the {key} to a name.
  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));

  // Lookup the {name} on {receiver}.
  Maybe<bool> maybe = JSReceiver::HasProperty(receiver, name);
  if (maybe.IsNothing()) return ReadOnlyRoots(isolate).exception();
  return isolate->heap()->ToBoolean(maybe.FromJust());
}

RUNTIME_FUNCTION(Runtime_GetOwnPropertyKeys) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_SMI_ARG_CHECKED(filter_value, 1);
  PropertyFilter filter = static_cast<PropertyFilter>(filter_value);

  Handle<FixedArray> keys;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, keys,
      KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly, filter,
                              GetKeysConversion::kConvertToString));

  return *isolate->factory()->NewJSArrayWithElements(keys);
}

RUNTIME_FUNCTION(Runtime_ToFastProperties) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  if (object->IsJSObject() && !object->IsJSGlobalObject()) {
    JSObject::MigrateSlowToFast(Handle<JSObject>::cast(object), 0,
                                "RuntimeToFastProperties");
  }
  return *object;
}

RUNTIME_FUNCTION(Runtime_AllocateHeapNumber) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  return *isolate->factory()->NewHeapNumber(0);
}

RUNTIME_FUNCTION(Runtime_NewObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, new_target, 1);
  RETURN_RESULT_OR_FAILURE(
      isolate,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
}

RUNTIME_FUNCTION(Runtime_GetDerivedMap) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, new_target, 1);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSFunction::GetDerivedMap(isolate, target, new_target));
}

RUNTIME_FUNCTION(Runtime_CompleteInobjectSlackTrackingForMap) {
  DisallowGarbageCollection no_gc;
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Map, initial_map, 0);
  initial_map->CompleteInobjectSlackTracking(isolate);

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_TryMigrateInstance) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, js_object, 0);
  // It could have been a DCHECK but we call this function directly from tests.
  if (!js_object->map().is_deprecated()) return Smi::zero();
  // This call must not cause lazy deopts, because it's called from deferred
  // code where we can't handle lazy deopts for lack of a suitable bailout
  // ID. So we just try migration and signal failure if necessary,
  // which will also trigger a deopt.
  if (!JSObject::TryMigrateInstance(isolate, js_object)) return Smi::zero();
  return *js_object;
}

static bool IsValidAccessor(Isolate* isolate, Handle<Object> obj) {
  return obj->IsNullOrUndefined(isolate) || obj->IsCallable();
}

// Implements part of 8.12.9 DefineOwnProperty.
// There are 3 cases that lead here:
// Step 4b - define a new accessor property.
// Steps 9c & 12 - replace an existing data property with an accessor property.
// Step 12 - update an existing accessor property with an accessor or generic
//           descriptor.
RUNTIME_FUNCTION(Runtime_DefineAccessorPropertyUnchecked) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CHECK(!obj->IsNull(isolate));
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, getter, 2);
  CHECK(IsValidAccessor(isolate, getter));
  CONVERT_ARG_HANDLE_CHECKED(Object, setter, 3);
  CHECK(IsValidAccessor(isolate, setter));
  CONVERT_PROPERTY_ATTRIBUTES_CHECKED(attrs, 4);

  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::DefineAccessor(obj, name, getter, setter, attrs));
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_DefineDataPropertyInLiteral) {
  HandleScope scope(isolate);
  DCHECK_EQ(6, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_SMI_ARG_CHECKED(flag, 3);
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, maybe_vector, 4);
  CONVERT_TAGGED_INDEX_ARG_CHECKED(index, 5);

  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    Handle<FeedbackVector> vector = Handle<FeedbackVector>::cast(maybe_vector);
    FeedbackNexus nexus(vector, FeedbackVector::ToSlot(index));
    if (nexus.ic_state() == UNINITIALIZED) {
      if (name->IsUniqueName()) {
        nexus.ConfigureMonomorphic(name, handle(object->map(), isolate),
                                   MaybeObjectHandle());
      } else {
        nexus.ConfigureMegamorphic(PROPERTY);
      }
    } else if (nexus.ic_state() == MONOMORPHIC) {
      if (nexus.GetFirstMap() != object->map() || nexus.GetName() != *name) {
        nexus.ConfigureMegamorphic(PROPERTY);
      }
    }
  }

  DataPropertyInLiteralFlags flags(flag);
  PropertyAttributes attrs = (flags & DataPropertyInLiteralFlag::kDontEnum)
                                 ? PropertyAttributes::DONT_ENUM
                                 : PropertyAttributes::NONE;

  if (flags & DataPropertyInLiteralFlag::kSetFunctionName) {
    DCHECK(value->IsJSFunction());
    Handle<JSFunction> function = Handle<JSFunction>::cast(value);
    DCHECK(!function->shared().HasSharedName());
    Handle<Map> function_map(function->map(), isolate);
    if (!JSFunction::SetName(function, name,
                             isolate->factory()->empty_string())) {
      return ReadOnlyRoots(isolate).exception();
    }
    // Class constructors do not reserve in-object space for name field.
    CHECK_IMPLIES(!IsClassConstructor(function->shared().kind()),
                  *function_map == function->map());
  }

  LookupIterator::Key key(isolate, name);
  LookupIterator it(isolate, object, key, object, LookupIterator::OWN);
  // Cannot fail since this should only be called when
  // creating an object literal.
  CHECK(JSObject::DefineOwnPropertyIgnoreAttributes(&it, value, attrs,
                                                    Just(kDontThrow))
            .IsJust());
  return *object;
}

RUNTIME_FUNCTION(Runtime_CollectTypeProfile) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Smi, position, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, maybe_vector, 2);

  if (maybe_vector->IsUndefined()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  CONVERT_ARG_HANDLE_CHECKED(FeedbackVector, vector, 2);

  Handle<String> type = Object::TypeOf(isolate, value);
  if (value->IsJSReceiver()) {
    Handle<JSReceiver> object = Handle<JSReceiver>::cast(value);
    type = JSReceiver::GetConstructorName(object);
  } else if (value->IsNull(isolate)) {
    // typeof(null) is object. But it's more user-friendly to annotate
    // null as type "null".
    type = Handle<String>(ReadOnlyRoots(isolate).null_string(), isolate);
  }

  DCHECK(vector->metadata().HasTypeProfileSlot());
  FeedbackNexus nexus(vector, vector->GetTypeProfileSlot());
  nexus.Collect(type, position->value());

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_HasFastPackedElements) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(HeapObject, obj, 0);
  return isolate->heap()->ToBoolean(
      IsFastPackedElementsKind(obj.map().elements_kind()));
}

RUNTIME_FUNCTION(Runtime_IsJSReceiver) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj.IsJSReceiver());
}

RUNTIME_FUNCTION(Runtime_GetFunctionName) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  return *JSFunction::GetName(isolate, function);
}

RUNTIME_FUNCTION(Runtime_DefineGetterPropertyUnchecked) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, getter, 2);
  CONVERT_PROPERTY_ATTRIBUTES_CHECKED(attrs, 3);

  if (String::cast(getter->shared().Name()).length() == 0) {
    Handle<Map> getter_map(getter->map(), isolate);
    if (!JSFunction::SetName(getter, name, isolate->factory()->get_string())) {
      return ReadOnlyRoots(isolate).exception();
    }
    CHECK_EQ(*getter_map, getter->map());
  }

  RETURN_FAILURE_ON_EXCEPTION(
      isolate,
      JSObject::DefineAccessor(object, name, getter,
                               isolate->factory()->null_value(), attrs));
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_SetDataProperties) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, source, 1);

  // 2. If source is undefined or null, let keys be an empty List.
  if (source->IsUndefined(isolate) || source->IsNull(isolate)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  MAYBE_RETURN(JSReceiver::SetOrCopyDataProperties(
                   isolate, target, source,
                   PropertiesEnumerationMode::kEnumerationOrder),
               ReadOnlyRoots(isolate).exception());
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_CopyDataProperties) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, source, 1);

  // 2. If source is undefined or null, let keys be an empty List.
  if (source->IsUndefined(isolate) || source->IsNull(isolate)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  MAYBE_RETURN(
      JSReceiver::SetOrCopyDataProperties(
          isolate, target, source,
          PropertiesEnumerationMode::kPropertyAdditionOrder, nullptr, false),
      ReadOnlyRoots(isolate).exception());
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_CopyDataPropertiesWithExcludedProperties) {
  HandleScope scope(isolate);
  DCHECK_LE(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, source, 0);

  // If source is undefined or null, throw a non-coercible error.
  if (source->IsNullOrUndefined(isolate)) {
    return ErrorUtils::ThrowLoadFromNullOrUndefined(isolate, source,
                                                    MaybeHandle<Object>());
  }

  ScopedVector<Handle<Object>> excluded_properties(args.length() - 1);
  for (int i = 1; i < args.length(); i++) {
    Handle<Object> property = args.at(i);
    uint32_t property_num;
    // We convert string to number if possible, in cases of computed
    // properties resolving to numbers, which would've been strings
    // instead because of our call to %ToName() in the desugaring for
    // computed properties.
    if (property->IsString() &&
        String::cast(*property).AsArrayIndex(&property_num)) {
      property = isolate->factory()->NewNumberFromUint(property_num);
    }

    excluded_properties[i - 1] = property;
  }

  Handle<JSObject> target =
      isolate->factory()->NewJSObject(isolate->object_function());
  MAYBE_RETURN(JSReceiver::SetOrCopyDataProperties(
                   isolate, target, source,
                   PropertiesEnumerationMode::kPropertyAdditionOrder,
                   &excluded_properties, false),
               ReadOnlyRoots(isolate).exception());
  return *target;
}

RUNTIME_FUNCTION(Runtime_DefineSetterPropertyUnchecked) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, setter, 2);
  CONVERT_PROPERTY_ATTRIBUTES_CHECKED(attrs, 3);

  if (String::cast(setter->shared().Name()).length() == 0) {
    Handle<Map> setter_map(setter->map(), isolate);
    if (!JSFunction::SetName(setter, name, isolate->factory()->set_string())) {
      return ReadOnlyRoots(isolate).exception();
    }
    CHECK_EQ(*setter_map, setter->map());
  }

  RETURN_FAILURE_ON_EXCEPTION(
      isolate,
      JSObject::DefineAccessor(object, name, isolate->factory()->null_value(),
                               setter, attrs));
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_ToObject) {
  // Runtime call is implemented in InterpreterIntrinsics and lowered in
  // JSIntrinsicLowering.
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_ToNumber) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToNumber(isolate, input));
}

RUNTIME_FUNCTION(Runtime_ToNumeric) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToNumeric(isolate, input));
}

RUNTIME_FUNCTION(Runtime_ToLength) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToLength(isolate, input));
}

RUNTIME_FUNCTION(Runtime_ToString) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToString(isolate, input));
}

RUNTIME_FUNCTION(Runtime_ToName) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToName(isolate, input));
}

RUNTIME_FUNCTION(Runtime_HasInPrototypeChain) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  if (!object->IsJSReceiver()) return ReadOnlyRoots(isolate).false_value();
  Maybe<bool> result = JSReceiver::HasInPrototypeChain(
      isolate, Handle<JSReceiver>::cast(object), prototype);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}

// ES6 section 7.4.7 CreateIterResultObject ( value, done )
RUNTIME_FUNCTION(Runtime_CreateIterResultObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, done, 1);
  return *isolate->factory()->NewJSIteratorResult(value,
                                                  done->BooleanValue(isolate));
}

RUNTIME_FUNCTION(Runtime_CreateDataProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, o, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  bool success;
  LookupIterator::Key lookup_key(isolate, key, &success);
  if (!success) return ReadOnlyRoots(isolate).exception();
  LookupIterator it(isolate, o, lookup_key, LookupIterator::OWN);
  MAYBE_RETURN(JSReceiver::CreateDataProperty(&it, value, Just(kThrowOnError)),
               ReadOnlyRoots(isolate).exception());
  return *value;
}

RUNTIME_FUNCTION(Runtime_SetOwnPropertyIgnoreAttributes) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, o, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_ARG_HANDLE_CHECKED(Smi, attributes, 3);

  RETURN_RESULT_OR_FAILURE(
      isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                   o, key, value, PropertyAttributes(attributes->value())));
}

RUNTIME_FUNCTION(Runtime_GetOwnPropertyDescriptor) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);

  PropertyDescriptor desc;
  Maybe<bool> found =
      JSReceiver::GetOwnPropertyDescriptor(isolate, object, name, &desc);
  MAYBE_RETURN(found, ReadOnlyRoots(isolate).exception());

  if (!found.FromJust()) return ReadOnlyRoots(isolate).undefined_value();
  return *desc.ToPropertyDescriptorObject(isolate);
}

RUNTIME_FUNCTION(Runtime_LoadPrivateSetter) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(AccessorPair, pair, 0);
  DCHECK(pair->setter().IsJSFunction());
  return pair->setter();
}

RUNTIME_FUNCTION(Runtime_LoadPrivateGetter) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(AccessorPair, pair, 0);
  DCHECK(pair->getter().IsJSFunction());
  return pair->getter();
}

RUNTIME_FUNCTION(Runtime_CreatePrivateAccessors) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 2);
  DCHECK(args[0].IsNull() || args[0].IsJSFunction());
  DCHECK(args[1].IsNull() || args[1].IsJSFunction());
  Handle<AccessorPair> pair = isolate->factory()->NewAccessorPair();
  pair->SetComponents(args[0], args[1]);
  return *pair;
}

RUNTIME_FUNCTION(Runtime_AddPrivateBrand) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 3);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(Symbol, brand, 1);
  CONVERT_ARG_HANDLE_CHECKED(Context, context, 2);
  DCHECK(brand->is_private_name());

  LookupIterator it(isolate, receiver, brand, LookupIterator::OWN);

  if (it.IsFound()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kVarRedeclaration, brand));
  }

  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  CHECK(Object::AddDataProperty(&it, context, attributes, Just(kDontThrow),
                                StoreOrigin::kMaybeKeyed)
            .FromJust());
  return *receiver;
}

RUNTIME_FUNCTION(Runtime_AddPrivateField) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, o, 0);
  CONVERT_ARG_HANDLE_CHECKED(Symbol, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  DCHECK(key->is_private_name());

  LookupIterator it(isolate, o, key, LookupIterator::OWN);

  if (it.IsFound()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kVarRedeclaration, key));
  }

  CHECK(Object::AddDataProperty(&it, value, NONE, Just(kDontThrow),
                                StoreOrigin::kMaybeKeyed)
            .FromJust());
  return ReadOnlyRoots(isolate).undefined_value();
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableAllocate) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(Smi, at_least_space_for, 0);

  return *isolate->factory()->NewSwissNameDictionary(
      at_least_space_for->value(), AllocationType::kYoung);
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableAdd) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(SwissNameDictionary, table, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_ARG_HANDLE_CHECKED(Smi, details_smi, 3);

  DCHECK(key->IsUniqueName());

  return *SwissNameDictionary::Add(isolate, table, key, value,
                                   PropertyDetails{*details_smi});
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableFindEntry) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(SwissNameDictionary, table, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);

  InternalIndex index = table->FindEntry(isolate, *key);
  return Smi::FromInt(index.is_found()
                          ? index.as_int()
                          : SwissNameDictionary::kNotFoundSentinel);
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableUpdate) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(SwissNameDictionary, table, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, index, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_ARG_HANDLE_CHECKED(Smi, details_smi, 3);

  InternalIndex i(Smi::ToInt(*index));

  table->ValueAtPut(i, *value);
  table->DetailsAtPut(i, PropertyDetails{*details_smi});

  return ReadOnlyRoots(isolate).undefined_value();
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableDelete) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(SwissNameDictionary, table, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, entry, 1);

  InternalIndex i(Smi::ToInt(*entry));

  return *SwissNameDictionary::DeleteEntry(isolate, table, i);
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableEquals) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(SwissNameDictionary, table, 0);
  CONVERT_ARG_HANDLE_CHECKED(SwissNameDictionary, other, 1);

  return Smi::FromInt(table->EqualsForTesting(*other));
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableElementsCount) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(SwissNameDictionary, table, 0);

  return Smi::FromInt(table->NumberOfElements());
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableKeyAt) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(SwissNameDictionary, table, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, entry, 1);

  return table->KeyAt(InternalIndex(Smi::ToInt(*entry)));
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableValueAt) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(SwissNameDictionary, table, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, entry, 1);

  return table->ValueAt(InternalIndex(Smi::ToInt(*entry)));
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableDetailsAt) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(SwissNameDictionary, table, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, entry, 1);

  PropertyDetails d = table->DetailsAt(InternalIndex(Smi::ToInt(*entry)));
  return d.AsSmi();
}

}  // namespace internal
}  // namespace v8
