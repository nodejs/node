// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/bootstrapper.h"
#include "src/debug/debug.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/objects/property-descriptor-object.h"
#include "src/property-descriptor.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

MaybeHandle<Object> Runtime::GetObjectProperty(Isolate* isolate,
                                               Handle<Object> object,
                                               Handle<Object> key,
                                               bool* is_found_out) {
  if (object->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kNonObjectPropertyLoad, key, object),
        Object);
  }

  bool success = false;
  LookupIterator it =
      LookupIterator::PropertyOrElement(isolate, object, key, &success);
  if (!success) return MaybeHandle<Object>();

  MaybeHandle<Object> result = Object::GetProperty(&it);
  if (is_found_out) *is_found_out = it.IsFound();

  if (!it.IsFound() && key->IsSymbol() &&
      Symbol::cast(*key)->is_private_field()) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kInvalidPrivateFieldAccess, key, object),
        Object);
  }
  return result;
}

static MaybeHandle<Object> KeyedGetObjectProperty(Isolate* isolate,
                                                  Handle<Object> receiver_obj,
                                                  Handle<Object> key_obj) {
  // Fast cases for getting named properties of the receiver JSObject
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
  if (key_obj->IsString() && String::cast(*key_obj)->AsArrayIndex(&index)) {
    key_obj = isolate->factory()->NewNumberFromUint(index);
  }
  if (receiver_obj->IsJSObject()) {
    if (!receiver_obj->IsJSGlobalProxy() &&
        !receiver_obj->IsAccessCheckNeeded() && key_obj->IsName()) {
      Handle<JSObject> receiver = Handle<JSObject>::cast(receiver_obj);
      Handle<Name> key = Handle<Name>::cast(key_obj);
      key_obj = key = isolate->factory()->InternalizeName(key);

      DisallowHeapAllocation no_allocation;
      if (receiver->IsJSGlobalObject()) {
        // Attempt dictionary lookup.
        GlobalDictionary* dictionary =
            JSGlobalObject::cast(*receiver)->global_dictionary();
        int entry = dictionary->FindEntry(key);
        if (entry != GlobalDictionary::kNotFound) {
          PropertyCell* cell = dictionary->CellAt(entry);
          if (cell->property_details().kind() == kData) {
            Object* value = cell->value();
            if (!value->IsTheHole(isolate)) {
              return Handle<Object>(value, isolate);
            }
            // If value is the hole (meaning, absent) do the general lookup.
          }
        }
      } else if (!receiver->HasFastProperties()) {
        // Attempt dictionary lookup.
        NameDictionary* dictionary = receiver->property_dictionary();
        int entry = dictionary->FindEntry(key);
        if ((entry != NameDictionary::kNotFound) &&
            (dictionary->DetailsAt(entry).kind() == kData)) {
          Object* value = dictionary->ValueAt(entry);
          return Handle<Object>(value, isolate);
        }
      }
    } else if (key_obj->IsSmi()) {
      // JSObject without a name key. If the key is a Smi, check for a
      // definite out-of-bounds access to elements, which is a strong indicator
      // that subsequent accesses will also call the runtime. Proactively
      // transition elements to FAST_*_ELEMENTS to avoid excessive boxing of
      // doubles for those future calls in the case that the elements would
      // become PACKED_DOUBLE_ELEMENTS.
      Handle<JSObject> js_object = Handle<JSObject>::cast(receiver_obj);
      ElementsKind elements_kind = js_object->GetElementsKind();
      if (IsDoubleElementsKind(elements_kind)) {
        if (Smi::ToInt(*key_obj) >= js_object->elements()->length()) {
          elements_kind = IsHoleyElementsKind(elements_kind) ? HOLEY_ELEMENTS
                                                             : PACKED_ELEMENTS;
          JSObject::TransitionElementsKind(js_object, elements_kind);
        }
      } else {
        DCHECK(IsSmiOrObjectElementsKind(elements_kind) ||
               !IsFastElementsKind(elements_kind));
      }
    }
  } else if (receiver_obj->IsString() && key_obj->IsSmi()) {
    // Fast case for string indexing using [] with a smi index.
    Handle<String> str = Handle<String>::cast(receiver_obj);
    int index = Handle<Smi>::cast(key_obj)->value();
    if (index >= 0 && index < str->length()) {
      Factory* factory = isolate->factory();
      return factory->LookupSingleCharacterStringFromCode(
          String::Flatten(str)->Get(index));
    }
  }

  // Fall back to GetObjectProperty.
  return Runtime::GetObjectProperty(isolate, receiver_obj, key_obj);
}

namespace {

bool DeleteObjectPropertyFast(Isolate* isolate, Handle<JSReceiver> receiver,
                              Handle<Object> raw_key) {
  DisallowHeapAllocation no_allocation;
  // This implements a special case for fast property deletion: when the
  // last property in an object is deleted, then instead of normalizing
  // the properties, we can undo the last map transition, with a few
  // prerequisites:
  // (1) The receiver must be a regular object and the key a unique name.
  Map* map = receiver->map();
  if (map->IsSpecialReceiverMap()) return false;
  if (!raw_key->IsUniqueName()) return false;
  Handle<Name> key = Handle<Name>::cast(raw_key);
  // (2) The property to be deleted must be the last property.
  int nof = map->NumberOfOwnDescriptors();
  if (nof == 0) return false;
  int descriptor = nof - 1;
  DescriptorArray* descriptors = map->instance_descriptors();
  if (descriptors->GetKey(descriptor) != *key) return false;
  // (3) The property to be deleted must be deletable.
  PropertyDetails details = descriptors->GetDetails(descriptor);
  if (!details.IsConfigurable()) return false;
  // (4) The map must have a back pointer.
  Object* backpointer = map->GetBackPointer();
  if (!backpointer->IsMap()) return false;
  // (5) The last transition must have been caused by adding a property
  // (and not any kind of special transition).
  if (Map::cast(backpointer)->NumberOfOwnDescriptors() != nof - 1) return false;

  // Preconditions successful. No more bailouts after this point.

  // Zap the property to avoid keeping objects alive. Zapping is not necessary
  // for properties stored in the descriptor array.
  if (details.location() == kField) {
    isolate->heap()->NotifyObjectLayoutChange(*receiver, map->instance_size(),
                                              no_allocation);
    FieldIndex index = FieldIndex::ForPropertyIndex(map, details.field_index());
    // Special case deleting the last out-of object property.
    if (!index.is_inobject() && index.outobject_array_index() == 0) {
      DCHECK(!Map::cast(backpointer)->HasOutOfObjectProperties());
      // Clear out the properties backing store.
      receiver->SetProperties(isolate->heap()->empty_fixed_array());
    } else {
      Object* filler = isolate->heap()->one_pointer_filler_map();
      JSObject::cast(*receiver)->RawFastPropertyAtPut(index, filler);
      // We must clear any recorded slot for the deleted property, because
      // subsequent object modifications might put a raw double there.
      // Slot clearing is the reason why this entire function cannot currently
      // be implemented in the DeleteProperty stub.
      if (index.is_inobject() && !map->IsUnboxedDoubleField(index)) {
        isolate->heap()->ClearRecordedSlot(
            *receiver, HeapObject::RawField(*receiver, index.offset()));
      }
    }
  }
  // If the map was marked stable before, then there could be optimized code
  // that depends on the assumption that no object that reached this map
  // transitions away from it without triggering the "deoptimize dependent
  // code" mechanism.
  map->NotifyLeafMapLayoutChange();
  // Finally, perform the map rollback.
  receiver->synchronized_set_map(Map::cast(backpointer));
#if VERIFY_HEAP
  receiver->HeapObjectVerify();
  receiver->property_array()->PropertyArrayVerify();
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
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, receiver, key, &success, LookupIterator::OWN);
  if (!success) return Nothing<bool>();

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

// ES6 19.1.3.2
RUNTIME_FUNCTION(Runtime_ObjectHasOwnProperty) {
  HandleScope scope(isolate);
  Handle<Object> property = args.at(1);

  Handle<Name> key;
  uint32_t index;
  bool key_is_array_index = property->ToArrayIndex(&index);

  if (!key_is_array_index) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, key,
                                       Object::ToName(isolate, property));
    key_is_array_index = key->AsArrayIndex(&index);
  }

  Handle<Object> object = args.at(0);

  if (object->IsJSModuleNamespace()) {
    if (key.is_null()) {
      DCHECK(key_is_array_index);
      // Namespace objects can't have indexed properties.
      return isolate->heap()->false_value();
    }

    Maybe<bool> result =
        JSReceiver::HasOwnProperty(Handle<JSReceiver>::cast(object), key);
    if (!result.IsJust()) return isolate->heap()->exception();
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
      LookupIterator it =
          key_is_array_index ? LookupIterator(isolate, js_obj, index, js_obj, c)
                             : LookupIterator(js_obj, key, js_obj, c);
      Maybe<bool> maybe = JSReceiver::HasProperty(&it);
      if (maybe.IsNothing()) return isolate->heap()->exception();
      DCHECK(!isolate->has_pending_exception());
      if (maybe.FromJust()) return isolate->heap()->true_value();
    }

    Map* map = js_obj->map();
    if (!map->has_hidden_prototype() &&
        (key_is_array_index ? !map->has_indexed_interceptor()
                            : !map->has_named_interceptor())) {
      return isolate->heap()->false_value();
    }

    // Slow case.
    LookupIterator::Configuration c = LookupIterator::OWN;
    LookupIterator it = key_is_array_index
                            ? LookupIterator(isolate, js_obj, index, js_obj, c)
                            : LookupIterator(js_obj, key, js_obj, c);

    Maybe<bool> maybe = JSReceiver::HasProperty(&it);
    if (maybe.IsNothing()) return isolate->heap()->exception();
    DCHECK(!isolate->has_pending_exception());
    return isolate->heap()->ToBoolean(maybe.FromJust());

  } else if (object->IsJSProxy()) {
    if (key.is_null()) {
      DCHECK(key_is_array_index);
      key = isolate->factory()->Uint32ToString(index);
    }

    Maybe<bool> result =
        JSReceiver::HasOwnProperty(Handle<JSProxy>::cast(object), key);
    if (result.IsNothing()) return isolate->heap()->exception();
    return isolate->heap()->ToBoolean(result.FromJust());

  } else if (object->IsString()) {
    return isolate->heap()->ToBoolean(
        key_is_array_index
            ? index < static_cast<uint32_t>(String::cast(*object)->length())
            : key->Equals(isolate->heap()->length_string()));
  } else if (object->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kUndefinedOrNullToObject));
  }

  return isolate->heap()->false_value();
}

RUNTIME_FUNCTION(Runtime_AddDictionaryProperty) {
  HandleScope scope(isolate);
  Handle<JSObject> receiver = args.at<JSObject>(0);
  Handle<Name> name = args.at<Name>(1);
  Handle<Object> value = args.at(2);

  DCHECK(name->IsUniqueName());

  Handle<NameDictionary> dictionary(receiver->property_dictionary(), isolate);
  PropertyDetails property_details(kData, NONE, PropertyCellType::kNoCell);
  dictionary = NameDictionary::Add(dictionary, name, value, property_details);
  receiver->SetProperties(*dictionary);
  return *value;
}

// ES6 section 19.1.2.2 Object.create ( O [ , Properties ] )
// TODO(verwaest): Support the common cases with precached map directly in
// an Object.create stub.
RUNTIME_FUNCTION(Runtime_ObjectCreate) {
  HandleScope scope(isolate);
  Handle<Object> prototype = args.at(0);
  if (!prototype->IsNull(isolate) && !prototype->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kProtoObjectOrNull, prototype));
  }

  // Generate the map with the specified {prototype} based on the Object
  // function's initial map from the current native context.
  // TODO(bmeurer): Use a dedicated cache for Object.create; think about
  // slack tracking for Object.create.
  Handle<Map> map =
      Map::GetObjectCreateMap(Handle<HeapObject>::cast(prototype));

  // Actually allocate the object.
  Handle<JSObject> object;
  if (map->is_dictionary_map()) {
    object = isolate->factory()->NewSlowJSObjectFromMap(map);
  } else {
    object = isolate->factory()->NewJSObjectFromMap(map);
  }

  // Define the properties if properties was specified and is not undefined.
  Handle<Object> properties = args.at(1);
  if (!properties->IsUndefined(isolate)) {
    RETURN_FAILURE_ON_EXCEPTION(
        isolate, JSReceiver::DefineProperties(isolate, object, properties));
  }

  return *object;
}

MaybeHandle<Object> Runtime::SetObjectProperty(Isolate* isolate,
                                               Handle<Object> object,
                                               Handle<Object> key,
                                               Handle<Object> value,
                                               LanguageMode language_mode) {
  if (object->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kNonObjectPropertyStore, key, object),
        Object);
  }

  // Check if the given key is an array index.
  bool success = false;
  LookupIterator it =
      LookupIterator::PropertyOrElement(isolate, object, key, &success);
  if (!success) return MaybeHandle<Object>();

  if (!it.IsFound() && key->IsSymbol() &&
      Symbol::cast(*key)->is_private_field()) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kInvalidPrivateFieldAccess, key, object),
        Object);
  }

  MAYBE_RETURN_NULL(Object::SetProperty(&it, value, language_mode,
                                        Object::MAY_BE_STORE_FROM_KEYED));
  return value;
}


RUNTIME_FUNCTION(Runtime_GetPrototype) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, obj, 0);
  RETURN_RESULT_OR_FAILURE(isolate, JSReceiver::GetPrototype(isolate, obj));
}


RUNTIME_FUNCTION(Runtime_InternalSetPrototype) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  if (prototype->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(prototype);
    if (!function->shared()->HasSharedName()) {
      Handle<Map> function_map(function->map(), isolate);
      if (!JSFunction::SetName(function, isolate->factory()->proto_string(),
                               isolate->factory()->empty_string())) {
        return isolate->heap()->exception();
      }
      CHECK_EQ(*function_map, function->map());
    }
  }
  MAYBE_RETURN(JSReceiver::SetPrototype(obj, prototype, false, kThrowOnError),
               isolate->heap()->exception());
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
    JSObject::NormalizeProperties(object, KEEP_INOBJECT_PROPERTIES, properties,
                                  "OptimizeForAdding");
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

RUNTIME_FUNCTION(Runtime_GetProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);

  RETURN_RESULT_OR_FAILURE(isolate,
                           Runtime::GetObjectProperty(isolate, object, key));
}

// KeyedGetProperty is called from KeyedLoadIC::GenerateGeneric.
RUNTIME_FUNCTION(Runtime_KeyedGetProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Object, receiver_obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key_obj, 1);

  RETURN_RESULT_OR_FAILURE(
      isolate, KeyedGetObjectProperty(isolate, receiver_obj, key_obj));
}

RUNTIME_FUNCTION(Runtime_AddNamedProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_PROPERTY_ATTRIBUTES_CHECKED(attrs, 3);

#ifdef DEBUG
  uint32_t index = 0;
  DCHECK(!name->ToArrayIndex(&index));
  LookupIterator it(object, name, object, LookupIterator::OWN_SKIP_INTERCEPTOR);
  Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
  if (maybe.IsNothing()) return isolate->heap()->exception();
  DCHECK(!it.IsFound());
#endif

  RETURN_RESULT_OR_FAILURE(isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                                        object, name, value, attrs));
}


// Adds an element to an array.
// This is used to create an indexed data property into an array.
RUNTIME_FUNCTION(Runtime_AddElement) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);

  uint32_t index = 0;
  CHECK(key->ToArrayIndex(&index));

#ifdef DEBUG
  LookupIterator it(isolate, object, index, object,
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
  if (maybe.IsNothing()) return isolate->heap()->exception();
  DCHECK(!it.IsFound());

  if (object->IsJSArray()) {
    Handle<JSArray> array = Handle<JSArray>::cast(object);
    DCHECK(!JSArray::WouldChangeReadOnlyLength(array, index));
  }
#endif

  RETURN_RESULT_OR_FAILURE(isolate, JSObject::SetOwnElementIgnoreAttributes(
                                        object, index, value, NONE));
}


RUNTIME_FUNCTION(Runtime_SetProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_LANGUAGE_MODE_ARG_CHECKED(language_mode, 3);

  RETURN_RESULT_OR_FAILURE(
      isolate,
      Runtime::SetObjectProperty(isolate, object, key, value, language_mode));
}


namespace {

// ES6 section 12.5.4.
Object* DeleteProperty(Isolate* isolate, Handle<Object> object,
                       Handle<Object> key, LanguageMode language_mode) {
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));
  Maybe<bool> result =
      Runtime::DeleteObjectProperty(isolate, receiver, key, language_mode);
  MAYBE_RETURN(result, isolate->heap()->exception());
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

RUNTIME_FUNCTION(Runtime_ShrinkPropertyDictionary) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  Handle<NameDictionary> dictionary(receiver->property_dictionary(), isolate);
  Handle<NameDictionary> new_properties = NameDictionary::Shrink(dictionary);
  receiver->SetProperties(*new_properties);
  return Smi::kZero;
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
  if (maybe.IsNothing()) return isolate->heap()->exception();
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


// Return information on whether an object has a named or indexed interceptor.
// args[0]: object
RUNTIME_FUNCTION(Runtime_GetInterceptorInfo) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  if (!args[0]->IsJSObject()) {
    return Smi::kZero;
  }
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  int result = 0;
  if (obj->HasNamedInterceptor()) result |= 2;
  if (obj->HasIndexedInterceptor()) result |= 1;

  return Smi::FromInt(result);
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
  RETURN_RESULT_OR_FAILURE(isolate, JSObject::New(target, new_target));
}

RUNTIME_FUNCTION(Runtime_CompleteInobjectSlackTrackingForMap) {
  DisallowHeapAllocation no_gc;
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Map, initial_map, 0);
  initial_map->CompleteInobjectSlackTracking();

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_TryMigrateInstance) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  if (!object->IsJSObject()) return Smi::kZero;
  Handle<JSObject> js_object = Handle<JSObject>::cast(object);
  // It could have been a DCHECK but we call this function directly from tests.
  if (!js_object->map()->is_deprecated()) return Smi::kZero;
  // This call must not cause lazy deopts, because it's called from deferred
  // code where we can't handle lazy deopts for lack of a suitable bailout
  // ID. So we just try migration and signal failure if necessary,
  // which will also trigger a deopt.
  if (!JSObject::TryMigrateInstance(js_object)) return Smi::kZero;
  return *object;
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
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DefineDataPropertyInLiteral) {
  HandleScope scope(isolate);
  DCHECK_EQ(6, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_SMI_ARG_CHECKED(flag, 3);
  CONVERT_ARG_HANDLE_CHECKED(FeedbackVector, vector, 4);
  CONVERT_SMI_ARG_CHECKED(index, 5);

  FeedbackNexus nexus(vector, FeedbackVector::ToSlot(index));
  if (nexus.ic_state() == UNINITIALIZED) {
    if (name->IsUniqueName()) {
      nexus.ConfigureMonomorphic(name, handle(object->map()),
                                 Handle<Code>::null());
    } else {
      nexus.ConfigureMegamorphic(PROPERTY);
    }
  } else if (nexus.ic_state() == MONOMORPHIC) {
    if (nexus.FindFirstMap() != object->map() ||
        nexus.GetFeedbackExtra() != *name) {
      nexus.ConfigureMegamorphic(PROPERTY);
    }
  }

  DataPropertyInLiteralFlags flags =
      static_cast<DataPropertyInLiteralFlag>(flag);

  PropertyAttributes attrs = (flags & DataPropertyInLiteralFlag::kDontEnum)
                                 ? PropertyAttributes::DONT_ENUM
                                 : PropertyAttributes::NONE;

  if (flags & DataPropertyInLiteralFlag::kSetFunctionName) {
    DCHECK(value->IsJSFunction());
    Handle<JSFunction> function = Handle<JSFunction>::cast(value);
    DCHECK(!function->shared()->HasSharedName());
    Handle<Map> function_map(function->map(), isolate);
    if (!JSFunction::SetName(function, name,
                             isolate->factory()->empty_string())) {
      return isolate->heap()->exception();
    }
    // Class constructors do not reserve in-object space for name field.
    CHECK_IMPLIES(!IsClassConstructor(function->shared()->kind()),
                  *function_map == function->map());
  }

  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, object, LookupIterator::OWN);
  // Cannot fail since this should only be called when
  // creating an object literal.
  CHECK(
      JSObject::DefineOwnPropertyIgnoreAttributes(&it, value, attrs, kDontThrow)
          .IsJust());
  return *object;
}

RUNTIME_FUNCTION(Runtime_CollectTypeProfile) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Smi, position, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  CONVERT_ARG_HANDLE_CHECKED(FeedbackVector, vector, 2);

  Handle<String> type = Object::TypeOf(isolate, value);
  if (value->IsJSReceiver()) {
    Handle<JSReceiver> object = Handle<JSReceiver>::cast(value);
    type = JSReceiver::GetConstructorName(object);
  } else if (value->IsNull(isolate)) {
    // typeof(null) is object. But it's more user-friendly to annotate
    // null as type "null".
    type = Handle<String>(isolate->heap()->null_string());
  }

  DCHECK(vector->metadata()->HasTypeProfileSlot());
  FeedbackNexus nexus(vector, vector->GetTypeProfileSlot());
  nexus.Collect(type, position->value());

  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_HasFastPackedElements) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(HeapObject, obj, 0);
  return isolate->heap()->ToBoolean(
      IsFastPackedElementsKind(obj->map()->elements_kind()));
}


RUNTIME_FUNCTION(Runtime_ValueOf) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (!obj->IsJSValue()) return obj;
  return JSValue::cast(obj)->value();
}


RUNTIME_FUNCTION(Runtime_IsJSReceiver) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSReceiver());
}


RUNTIME_FUNCTION(Runtime_ClassOf) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (!obj->IsJSReceiver()) return isolate->heap()->null_value();
  return JSReceiver::cast(obj)->class_name();
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

  if (String::cast(getter->shared()->Name())->length() == 0) {
    Handle<Map> getter_map(getter->map(), isolate);
    if (!JSFunction::SetName(getter, name, isolate->factory()->get_string())) {
      return isolate->heap()->exception();
    }
    CHECK_EQ(*getter_map, getter->map());
  }

  RETURN_FAILURE_ON_EXCEPTION(
      isolate,
      JSObject::DefineAccessor(object, name, getter,
                               isolate->factory()->null_value(), attrs));
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_CopyDataProperties) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, source, 1);

  // 2. If source is undefined or null, let keys be an empty List.
  if (source->IsUndefined(isolate) || source->IsNull(isolate)) {
    return isolate->heap()->undefined_value();
  }

  MAYBE_RETURN(JSReceiver::SetOrCopyDataProperties(isolate, target, source,
                                                   nullptr, false),
               isolate->heap()->exception());
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_CopyDataPropertiesWithExcludedProperties) {
  HandleScope scope(isolate);
  DCHECK_LE(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, source, 0);

  // 2. If source is undefined or null, let keys be an empty List.
  if (source->IsUndefined(isolate) || source->IsNull(isolate)) {
    return isolate->heap()->undefined_value();
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
        String::cast(*property)->AsArrayIndex(&property_num)) {
      property = isolate->factory()->NewNumberFromUint(property_num);
    }

    excluded_properties[i - 1] = property;
  }

  Handle<JSObject> target =
      isolate->factory()->NewJSObject(isolate->object_function());
  MAYBE_RETURN(JSReceiver::SetOrCopyDataProperties(isolate, target, source,
                                                   &excluded_properties, false),
               isolate->heap()->exception());
  return *target;
}

namespace {

inline void TrySetNative(Handle<Object> maybe_func) {
  if (!maybe_func->IsJSFunction()) return;
  JSFunction::cast(*maybe_func)->shared()->set_native(true);
}

inline void TrySetNativeAndLength(Handle<Object> maybe_func, int length) {
  if (!maybe_func->IsJSFunction()) return;
  SharedFunctionInfo* shared = JSFunction::cast(*maybe_func)->shared();
  shared->set_native(true);
  if (length >= 0) {
    shared->set_length(length);
  }
}

}  // namespace

RUNTIME_FUNCTION(Runtime_DefineMethodsInternal) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CHECK(isolate->bootstrapper()->IsActive());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, source_class, 1);
  CONVERT_SMI_ARG_CHECKED(length, 2);

  DCHECK(source_class->prototype()->IsJSObject());
  Handle<JSObject> source(JSObject::cast(source_class->prototype()), isolate);

  Handle<FixedArray> keys;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, keys,
      KeyAccumulator::GetKeys(source, KeyCollectionMode::kOwnOnly,
                              ALL_PROPERTIES,
                              GetKeysConversion::kConvertToString));

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Name> key = Handle<Name>::cast(FixedArray::get(*keys, i, isolate));
    if (*key == isolate->heap()->constructor_string()) continue;

    PropertyDescriptor descriptor;
    Maybe<bool> did_get_descriptor =
        JSReceiver::GetOwnPropertyDescriptor(isolate, source, key, &descriptor);
    CHECK(did_get_descriptor.FromJust());
    if (descriptor.has_value()) {
      TrySetNativeAndLength(descriptor.value(), length);
    } else {
      if (descriptor.has_get()) TrySetNative(descriptor.get());
      if (descriptor.has_set()) TrySetNative(descriptor.set());
    }

    Maybe<bool> success = JSReceiver::DefineOwnProperty(
        isolate, target, key, &descriptor, kDontThrow);
    CHECK(success.FromJust());
  }
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_DefineSetterPropertyUnchecked) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, setter, 2);
  CONVERT_PROPERTY_ATTRIBUTES_CHECKED(attrs, 3);

  if (String::cast(setter->shared()->Name())->length() == 0) {
    Handle<Map> setter_map(setter->map(), isolate);
    if (!JSFunction::SetName(setter, name, isolate->factory()->set_string())) {
      return isolate->heap()->exception();
    }
    CHECK_EQ(*setter_map, setter->map());
  }

  RETURN_FAILURE_ON_EXCEPTION(
      isolate,
      JSObject::DefineAccessor(object, name, isolate->factory()->null_value(),
                               setter, attrs));
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_ToObject) {
  // Runtime call is implemented in InterpreterIntrinsics and lowered in
  // JSIntrinsicLowering.
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_ToPrimitive) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToPrimitive(input));
}


RUNTIME_FUNCTION(Runtime_ToPrimitive_Number) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  RETURN_RESULT_OR_FAILURE(
      isolate, Object::ToPrimitive(input, ToPrimitiveHint::kNumber));
}

RUNTIME_FUNCTION(Runtime_ToNumber) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToNumber(input));
}

RUNTIME_FUNCTION(Runtime_ToNumeric) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToNumeric(input));
}

RUNTIME_FUNCTION(Runtime_ToInteger) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToInteger(isolate, input));
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


RUNTIME_FUNCTION(Runtime_SameValue) {
  SealHandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(Object, x, 0);
  CONVERT_ARG_CHECKED(Object, y, 1);
  return isolate->heap()->ToBoolean(x->SameValue(y));
}


RUNTIME_FUNCTION(Runtime_SameValueZero) {
  SealHandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(Object, x, 0);
  CONVERT_ARG_CHECKED(Object, y, 1);
  return isolate->heap()->ToBoolean(x->SameValueZero(y));
}

RUNTIME_FUNCTION(Runtime_HasInPrototypeChain) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  if (!object->IsJSReceiver()) return isolate->heap()->false_value();
  Maybe<bool> result = JSReceiver::HasInPrototypeChain(
      isolate, Handle<JSReceiver>::cast(object), prototype);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}


// ES6 section 7.4.7 CreateIterResultObject ( value, done )
RUNTIME_FUNCTION(Runtime_CreateIterResultObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, done, 1);
  return *isolate->factory()->NewJSIteratorResult(value, done->BooleanValue());
}

RUNTIME_FUNCTION(Runtime_CreateDataProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, o, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  bool success;
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, o, key, &success, LookupIterator::OWN);
  if (!success) return isolate->heap()->exception();
  MAYBE_RETURN(JSReceiver::CreateDataProperty(&it, value, kThrowOnError),
               isolate->heap()->exception());
  return *value;
}

// Checks that 22.2.2.1.1 Runtime Semantics: IterableToList produces exactly the
// same result as doing nothing.
RUNTIME_FUNCTION(Runtime_IterableToListCanBeElided) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, obj, 0);

  // If an iterator symbol is added to the Number prototype, we could see a Smi.
  if (obj->IsSmi()) return isolate->heap()->ToBoolean(false);
  if (!HeapObject::cast(*obj)->IsJSObject()) {
    return isolate->heap()->ToBoolean(false);
  }

  // While iteration alone may not have observable side-effects, calling
  // toNumber on an object will. Make sure the arg is not an array of objects.
  ElementsKind kind = JSObject::cast(*obj)->GetElementsKind();
  if (!IsFastNumberElementsKind(kind)) return isolate->heap()->ToBoolean(false);

  return isolate->heap()->ToBoolean(!obj->IterationHasObservableEffects());
}

RUNTIME_FUNCTION(Runtime_GetOwnPropertyDescriptor) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);

  PropertyDescriptor desc;
  Maybe<bool> found =
      JSReceiver::GetOwnPropertyDescriptor(isolate, object, name, &desc);
  MAYBE_RETURN(found, isolate->heap()->exception());

  if (!found.FromJust()) return isolate->heap()->undefined_value();
  return *desc.ToPropertyDescriptorObject(isolate);
}

RUNTIME_FUNCTION(Runtime_AddPrivateField) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, o, 0);
  CONVERT_ARG_HANDLE_CHECKED(Symbol, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  DCHECK(key->is_private_field());

  LookupIterator it =
      LookupIterator::PropertyOrElement(isolate, o, key, LookupIterator::OWN);

  if (it.IsFound()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kVarRedeclaration, key));
  }

  CHECK(Object::AddDataProperty(&it, value, NONE, kDontThrow,
                                Object::MAY_BE_STORE_FROM_KEYED)
            .FromJust());
  return isolate->heap()->undefined_value();
}

}  // namespace internal
}  // namespace v8
