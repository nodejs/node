// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/bootstrapper.h"
#include "src/debug.h"
#include "src/runtime/runtime.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

// Returns a single character string where first character equals
// string->Get(index).
static Handle<Object> GetCharAt(Handle<String> string, uint32_t index) {
  if (index < static_cast<uint32_t>(string->length())) {
    Factory* factory = string->GetIsolate()->factory();
    return factory->LookupSingleCharacterStringFromCode(
        String::Flatten(string)->Get(index));
  }
  return Execution::CharAt(string, index);
}


MaybeHandle<Object> Runtime::GetElementOrCharAt(Isolate* isolate,
                                                Handle<Object> object,
                                                uint32_t index) {
  // Handle [] indexing on Strings
  if (object->IsString()) {
    Handle<Object> result = GetCharAt(Handle<String>::cast(object), index);
    if (!result->IsUndefined()) return result;
  }

  // Handle [] indexing on String objects
  if (object->IsStringObjectWithCharacterAt(index)) {
    Handle<JSValue> js_value = Handle<JSValue>::cast(object);
    Handle<Object> result =
        GetCharAt(Handle<String>(String::cast(js_value->value())), index);
    if (!result->IsUndefined()) return result;
  }

  Handle<Object> result;
  if (object->IsString() || object->IsNumber() || object->IsBoolean()) {
    PrototypeIterator iter(isolate, object);
    return Object::GetElement(isolate, PrototypeIterator::GetCurrent(iter),
                              index);
  } else {
    return Object::GetElement(isolate, object, index);
  }
}


MaybeHandle<Name> Runtime::ToName(Isolate* isolate, Handle<Object> key) {
  if (key->IsName()) {
    return Handle<Name>::cast(key);
  } else {
    Handle<Object> converted;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, converted,
                               Execution::ToString(isolate, key), Name);
    return Handle<Name>::cast(converted);
  }
}


MaybeHandle<Object> Runtime::GetObjectProperty(Isolate* isolate,
                                               Handle<Object> object,
                                               Handle<Object> key) {
  if (object->IsUndefined() || object->IsNull()) {
    Handle<Object> args[2] = {key, object};
    THROW_NEW_ERROR(isolate, NewTypeError("non_object_property_load",
                                          HandleVector(args, 2)),
                    Object);
  }

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    return GetElementOrCharAt(isolate, object, index);
  }

  // Convert the key to a name - possibly by calling back into JavaScript.
  Handle<Name> name;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, name, ToName(isolate, key), Object);

  // Check if the name is trivially convertible to an index and get
  // the element if so.
  if (name->AsArrayIndex(&index)) {
    return GetElementOrCharAt(isolate, object, index);
  } else {
    return Object::GetProperty(object, name);
  }
}


MaybeHandle<Object> Runtime::SetObjectProperty(Isolate* isolate,
                                               Handle<Object> object,
                                               Handle<Object> key,
                                               Handle<Object> value,
                                               LanguageMode language_mode) {
  if (object->IsUndefined() || object->IsNull()) {
    Handle<Object> args[2] = {key, object};
    THROW_NEW_ERROR(isolate, NewTypeError("non_object_property_store",
                                          HandleVector(args, 2)),
                    Object);
  }

  if (object->IsJSProxy()) {
    Handle<Object> name_object;
    if (key->IsSymbol()) {
      name_object = key;
    } else {
      ASSIGN_RETURN_ON_EXCEPTION(isolate, name_object,
                                 Execution::ToString(isolate, key), Object);
    }
    Handle<Name> name = Handle<Name>::cast(name_object);
    return Object::SetProperty(Handle<JSProxy>::cast(object), name, value,
                               language_mode);
  }

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    // TODO(verwaest): Support non-JSObject receivers.
    if (!object->IsJSObject()) return value;
    Handle<JSObject> js_object = Handle<JSObject>::cast(object);

    // In Firefox/SpiderMonkey, Safari and Opera you can access the characters
    // of a string using [] notation.  We need to support this too in
    // JavaScript.
    // In the case of a String object we just need to redirect the assignment to
    // the underlying string if the index is in range.  Since the underlying
    // string does nothing with the assignment then we can ignore such
    // assignments.
    if (js_object->IsStringObjectWithCharacterAt(index)) {
      return value;
    }

    JSObject::ValidateElements(js_object);
    if (js_object->HasExternalArrayElements() ||
        js_object->HasFixedTypedArrayElements()) {
      if (!value->IsNumber() && !value->IsUndefined()) {
        ASSIGN_RETURN_ON_EXCEPTION(isolate, value,
                                   Execution::ToNumber(isolate, value), Object);
      }
    }

    MaybeHandle<Object> result = JSObject::SetElement(
        js_object, index, value, NONE, language_mode, true, SET_PROPERTY);
    JSObject::ValidateElements(js_object);

    return result.is_null() ? result : value;
  }

  if (key->IsName()) {
    Handle<Name> name = Handle<Name>::cast(key);
    if (name->AsArrayIndex(&index)) {
      // TODO(verwaest): Support non-JSObject receivers.
      if (!object->IsJSObject()) return value;
      Handle<JSObject> js_object = Handle<JSObject>::cast(object);
      if (js_object->HasExternalArrayElements()) {
        if (!value->IsNumber() && !value->IsUndefined()) {
          ASSIGN_RETURN_ON_EXCEPTION(
              isolate, value, Execution::ToNumber(isolate, value), Object);
        }
      }
      return JSObject::SetElement(js_object, index, value, NONE, language_mode,
                                  true, SET_PROPERTY);
    } else {
      if (name->IsString()) name = String::Flatten(Handle<String>::cast(name));
      return Object::SetProperty(object, name, value, language_mode);
    }
  }

  // Call-back into JavaScript to convert the key to a string.
  Handle<Object> converted;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, converted,
                             Execution::ToString(isolate, key), Object);
  Handle<String> name = Handle<String>::cast(converted);

  if (name->AsArrayIndex(&index)) {
    // TODO(verwaest): Support non-JSObject receivers.
    if (!object->IsJSObject()) return value;
    Handle<JSObject> js_object = Handle<JSObject>::cast(object);
    return JSObject::SetElement(js_object, index, value, NONE, language_mode,
                                true, SET_PROPERTY);
  }
  return Object::SetProperty(object, name, value, language_mode);
}


MaybeHandle<Object> Runtime::DefineObjectProperty(Handle<JSObject> js_object,
                                                  Handle<Object> key,
                                                  Handle<Object> value,
                                                  PropertyAttributes attrs) {
  Isolate* isolate = js_object->GetIsolate();
  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    // In Firefox/SpiderMonkey, Safari and Opera you can access the characters
    // of a string using [] notation.  We need to support this too in
    // JavaScript.
    // In the case of a String object we just need to redirect the assignment to
    // the underlying string if the index is in range.  Since the underlying
    // string does nothing with the assignment then we can ignore such
    // assignments.
    if (js_object->IsStringObjectWithCharacterAt(index)) {
      return value;
    }

    return JSObject::SetElement(js_object, index, value, attrs, SLOPPY, false,
                                DEFINE_PROPERTY);
  }

  if (key->IsName()) {
    Handle<Name> name = Handle<Name>::cast(key);
    if (name->AsArrayIndex(&index)) {
      return JSObject::SetElement(js_object, index, value, attrs, SLOPPY, false,
                                  DEFINE_PROPERTY);
    } else {
      if (name->IsString()) name = String::Flatten(Handle<String>::cast(name));
      return JSObject::SetOwnPropertyIgnoreAttributes(js_object, name, value,
                                                      attrs);
    }
  }

  // Call-back into JavaScript to convert the key to a string.
  Handle<Object> converted;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, converted,
                             Execution::ToString(isolate, key), Object);
  Handle<String> name = Handle<String>::cast(converted);

  if (name->AsArrayIndex(&index)) {
    return JSObject::SetElement(js_object, index, value, attrs, SLOPPY, false,
                                DEFINE_PROPERTY);
  } else {
    return JSObject::SetOwnPropertyIgnoreAttributes(js_object, name, value,
                                                    attrs);
  }
}


MaybeHandle<Object> Runtime::GetPrototype(Isolate* isolate,
                                          Handle<Object> obj) {
  // We don't expect access checks to be needed on JSProxy objects.
  DCHECK(!obj->IsAccessCheckNeeded() || obj->IsJSObject());
  PrototypeIterator iter(isolate, obj, PrototypeIterator::START_AT_RECEIVER);
  do {
    if (PrototypeIterator::GetCurrent(iter)->IsAccessCheckNeeded() &&
        !isolate->MayAccess(
            Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)))) {
      isolate->ReportFailedAccessCheck(
          Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)));
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
      return isolate->factory()->undefined_value();
    }
    iter.AdvanceIgnoringProxies();
    if (PrototypeIterator::GetCurrent(iter)->IsJSProxy()) {
      return PrototypeIterator::GetCurrent(iter);
    }
  } while (!iter.IsAtEnd(PrototypeIterator::END_AT_NON_HIDDEN));
  return PrototypeIterator::GetCurrent(iter);
}


RUNTIME_FUNCTION(Runtime_GetPrototype) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, obj, 0);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     Runtime::GetPrototype(isolate, obj));
  return *result;
}


RUNTIME_FUNCTION(Runtime_InternalSetPrototype) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  DCHECK(!obj->IsAccessCheckNeeded());
  DCHECK(!obj->map()->is_observed());
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, JSObject::SetPrototype(obj, prototype, false));
  return *result;
}


RUNTIME_FUNCTION(Runtime_SetPrototype) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  if (obj->IsAccessCheckNeeded() && !isolate->MayAccess(obj)) {
    isolate->ReportFailedAccessCheck(obj);
    RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
    return isolate->heap()->undefined_value();
  }
  if (obj->map()->is_observed()) {
    Handle<Object> old_value =
        Object::GetPrototypeSkipHiddenPrototypes(isolate, obj);
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, JSObject::SetPrototype(obj, prototype, true));

    Handle<Object> new_value =
        Object::GetPrototypeSkipHiddenPrototypes(isolate, obj);
    if (!new_value->SameValue(*old_value)) {
      RETURN_FAILURE_ON_EXCEPTION(
          isolate, JSObject::EnqueueChangeRecord(
                       obj, "setPrototype", isolate->factory()->proto_string(),
                       old_value));
    }
    return *result;
  }
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, JSObject::SetPrototype(obj, prototype, true));
  return *result;
}


RUNTIME_FUNCTION(Runtime_IsInPrototypeChain) {
  HandleScope shs(isolate);
  DCHECK(args.length() == 2);
  // See ECMA-262, section 15.3.5.3, page 88 (steps 5 - 8).
  CONVERT_ARG_HANDLE_CHECKED(Object, O, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, V, 1);
  PrototypeIterator iter(isolate, V, PrototypeIterator::START_AT_RECEIVER);
  while (true) {
    iter.AdvanceIgnoringProxies();
    if (iter.IsAtEnd()) return isolate->heap()->false_value();
    if (iter.IsAtEnd(O)) return isolate->heap()->true_value();
  }
}


// Enumerator used as indices into the array returned from GetOwnProperty
enum PropertyDescriptorIndices {
  IS_ACCESSOR_INDEX,
  VALUE_INDEX,
  GETTER_INDEX,
  SETTER_INDEX,
  WRITABLE_INDEX,
  ENUMERABLE_INDEX,
  CONFIGURABLE_INDEX,
  DESCRIPTOR_SIZE
};


MUST_USE_RESULT static MaybeHandle<Object> GetOwnProperty(Isolate* isolate,
                                                          Handle<JSObject> obj,
                                                          Handle<Name> name) {
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();

  PropertyAttributes attrs;
  uint32_t index = 0;
  Handle<Object> value;
  MaybeHandle<AccessorPair> maybe_accessors;
  // TODO(verwaest): Unify once indexed properties can be handled by the
  // LookupIterator.
  if (name->AsArrayIndex(&index)) {
    // Get attributes.
    Maybe<PropertyAttributes> maybe =
        JSReceiver::GetOwnElementAttribute(obj, index);
    if (!maybe.IsJust()) return MaybeHandle<Object>();
    attrs = maybe.FromJust();
    if (attrs == ABSENT) return factory->undefined_value();

    // Get AccessorPair if present.
    maybe_accessors = JSObject::GetOwnElementAccessorPair(obj, index);

    // Get value if not an AccessorPair.
    if (maybe_accessors.is_null()) {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, value, Runtime::GetElementOrCharAt(isolate, obj, index),
          Object);
    }
  } else {
    // Get attributes.
    LookupIterator it(obj, name, LookupIterator::HIDDEN);
    Maybe<PropertyAttributes> maybe = JSObject::GetPropertyAttributes(&it);
    if (!maybe.IsJust()) return MaybeHandle<Object>();
    attrs = maybe.FromJust();
    if (attrs == ABSENT) return factory->undefined_value();

    // Get AccessorPair if present.
    if (it.state() == LookupIterator::ACCESSOR &&
        it.GetAccessors()->IsAccessorPair()) {
      maybe_accessors = Handle<AccessorPair>::cast(it.GetAccessors());
    }

    // Get value if not an AccessorPair.
    if (maybe_accessors.is_null()) {
      ASSIGN_RETURN_ON_EXCEPTION(isolate, value, Object::GetProperty(&it),
                                 Object);
    }
  }
  DCHECK(!isolate->has_pending_exception());
  Handle<FixedArray> elms = factory->NewFixedArray(DESCRIPTOR_SIZE);
  elms->set(ENUMERABLE_INDEX, heap->ToBoolean((attrs & DONT_ENUM) == 0));
  elms->set(CONFIGURABLE_INDEX, heap->ToBoolean((attrs & DONT_DELETE) == 0));
  elms->set(IS_ACCESSOR_INDEX, heap->ToBoolean(!maybe_accessors.is_null()));

  Handle<AccessorPair> accessors;
  if (maybe_accessors.ToHandle(&accessors)) {
    Handle<Object> getter(accessors->GetComponent(ACCESSOR_GETTER), isolate);
    Handle<Object> setter(accessors->GetComponent(ACCESSOR_SETTER), isolate);
    elms->set(GETTER_INDEX, *getter);
    elms->set(SETTER_INDEX, *setter);
  } else {
    elms->set(WRITABLE_INDEX, heap->ToBoolean((attrs & READ_ONLY) == 0));
    elms->set(VALUE_INDEX, *value);
  }

  return factory->NewJSArrayWithElements(elms);
}


// Returns an array with the property description:
//  if args[1] is not a property on args[0]
//          returns undefined
//  if args[1] is a data property on args[0]
//         [false, value, Writeable, Enumerable, Configurable]
//  if args[1] is an accessor on args[0]
//         [true, GetFunction, SetFunction, Enumerable, Configurable]
RUNTIME_FUNCTION(Runtime_GetOwnProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     GetOwnProperty(isolate, obj, name));
  return *result;
}


RUNTIME_FUNCTION(Runtime_PreventExtensions) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSObject::PreventExtensions(obj));
  return *result;
}


RUNTIME_FUNCTION(Runtime_IsExtensible) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  if (obj->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, obj);
    if (iter.IsAtEnd()) return isolate->heap()->false_value();
    DCHECK(iter.GetCurrent()->IsJSGlobalObject());
    obj = JSObject::cast(iter.GetCurrent());
  }
  return isolate->heap()->ToBoolean(obj->map()->is_extensible());
}


RUNTIME_FUNCTION(Runtime_DisableAccessChecks) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, object, 0);
  Handle<Map> old_map(object->map());
  bool needs_access_checks = old_map->is_access_check_needed();
  if (needs_access_checks) {
    // Copy map so it won't interfere constructor's initial map.
    Handle<Map> new_map = Map::Copy(old_map, "DisableAccessChecks");
    new_map->set_is_access_check_needed(false);
    JSObject::MigrateToMap(Handle<JSObject>::cast(object), new_map);
  }
  return isolate->heap()->ToBoolean(needs_access_checks);
}


RUNTIME_FUNCTION(Runtime_EnableAccessChecks) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  Handle<Map> old_map(object->map());
  RUNTIME_ASSERT(!old_map->is_access_check_needed());
  // Copy map so it won't interfere constructor's initial map.
  Handle<Map> new_map = Map::Copy(old_map, "EnableAccessChecks");
  new_map->set_is_access_check_needed(true);
  JSObject::MigrateToMap(object, new_map);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_OptimizeObjectForAddingMultipleProperties) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_SMI_ARG_CHECKED(properties, 1);
  // Conservative upper limit to prevent fuzz tests from going OOM.
  RUNTIME_ASSERT(properties <= 100000);
  if (object->HasFastProperties() && !object->IsJSGlobalProxy()) {
    JSObject::NormalizeProperties(object, KEEP_INOBJECT_PROPERTIES, properties,
                                  "OptimizeForAdding");
  }
  return *object;
}


RUNTIME_FUNCTION(Runtime_ObjectFreeze) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);

  // %ObjectFreeze is a fast path and these cases are handled elsewhere.
  RUNTIME_ASSERT(!object->HasSloppyArgumentsElements() &&
                 !object->map()->is_observed() && !object->IsJSProxy());

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, JSObject::Freeze(object));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ObjectSeal) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);

  // %ObjectSeal is a fast path and these cases are handled elsewhere.
  RUNTIME_ASSERT(!object->HasSloppyArgumentsElements() &&
                 !object->map()->is_observed() && !object->IsJSProxy());

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, JSObject::Seal(object));
  return *result;
}


RUNTIME_FUNCTION(Runtime_GetProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Runtime::GetObjectProperty(isolate, object, key));
  return *result;
}


MUST_USE_RESULT static MaybeHandle<Object> TransitionElements(
    Handle<Object> object, ElementsKind to_kind, Isolate* isolate) {
  HandleScope scope(isolate);
  if (!object->IsJSObject()) {
    isolate->ThrowIllegalOperation();
    return MaybeHandle<Object>();
  }
  ElementsKind from_kind =
      Handle<JSObject>::cast(object)->map()->elements_kind();
  if (Map::IsValidElementsTransition(from_kind, to_kind)) {
    JSObject::TransitionElementsKind(Handle<JSObject>::cast(object), to_kind);
    return object;
  }
  isolate->ThrowIllegalOperation();
  return MaybeHandle<Object>();
}


// KeyedGetProperty is called from KeyedLoadIC::GenerateGeneric.
RUNTIME_FUNCTION(Runtime_KeyedGetProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(Object, receiver_obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key_obj, 1);

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
  if (receiver_obj->IsJSObject()) {
    if (!receiver_obj->IsJSGlobalProxy() &&
        !receiver_obj->IsAccessCheckNeeded() && key_obj->IsName()) {
      DisallowHeapAllocation no_allocation;
      Handle<JSObject> receiver = Handle<JSObject>::cast(receiver_obj);
      Handle<Name> key = Handle<Name>::cast(key_obj);
      if (!receiver->HasFastProperties()) {
        // Attempt dictionary lookup.
        NameDictionary* dictionary = receiver->property_dictionary();
        int entry = dictionary->FindEntry(key);
        if ((entry != NameDictionary::kNotFound) &&
            (dictionary->DetailsAt(entry).type() == DATA)) {
          Object* value = dictionary->ValueAt(entry);
          if (!receiver->IsGlobalObject()) return value;
          DCHECK(value->IsPropertyCell());
          value = PropertyCell::cast(value)->value();
          if (!value->IsTheHole()) return value;
          // If value is the hole (meaning, absent) do the general lookup.
        }
      }
    } else if (key_obj->IsSmi()) {
      // JSObject without a name key. If the key is a Smi, check for a
      // definite out-of-bounds access to elements, which is a strong indicator
      // that subsequent accesses will also call the runtime. Proactively
      // transition elements to FAST_*_ELEMENTS to avoid excessive boxing of
      // doubles for those future calls in the case that the elements would
      // become FAST_DOUBLE_ELEMENTS.
      Handle<JSObject> js_object = Handle<JSObject>::cast(receiver_obj);
      ElementsKind elements_kind = js_object->GetElementsKind();
      if (IsFastDoubleElementsKind(elements_kind)) {
        Handle<Smi> key = Handle<Smi>::cast(key_obj);
        if (key->value() >= js_object->elements()->length()) {
          if (IsFastHoleyElementsKind(elements_kind)) {
            elements_kind = FAST_HOLEY_ELEMENTS;
          } else {
            elements_kind = FAST_ELEMENTS;
          }
          RETURN_FAILURE_ON_EXCEPTION(
              isolate, TransitionElements(js_object, elements_kind, isolate));
        }
      } else {
        DCHECK(IsFastSmiOrObjectElementsKind(elements_kind) ||
               !IsFastElementsKind(elements_kind));
      }
    }
  } else if (receiver_obj->IsString() && key_obj->IsSmi()) {
    // Fast case for string indexing using [] with a smi index.
    Handle<String> str = Handle<String>::cast(receiver_obj);
    int index = args.smi_at(1);
    if (index >= 0 && index < str->length()) {
      return *GetCharAt(str, index);
    }
  }

  // Fall back to GetObjectProperty.
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::GetObjectProperty(isolate, receiver_obj, key_obj));
  return *result;
}


RUNTIME_FUNCTION(Runtime_AddNamedProperty) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_PROPERTY_ATTRIBUTES_CHECKED(attrs, 3);

#ifdef DEBUG
  uint32_t index = 0;
  DCHECK(!key->ToArrayIndex(&index));
  LookupIterator it(object, key, LookupIterator::OWN_SKIP_INTERCEPTOR);
  Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
  if (!maybe.IsJust()) return isolate->heap()->exception();
  RUNTIME_ASSERT(!it.IsFound());
#endif

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::SetOwnPropertyIgnoreAttributes(object, key, value, attrs));
  return *result;
}


RUNTIME_FUNCTION(Runtime_SetProperty) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_LANGUAGE_MODE_ARG_CHECKED(language_mode_arg, 3);
  LanguageMode language_mode = language_mode_arg;

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::SetObjectProperty(isolate, object, key, value, language_mode));
  return *result;
}


// Adds an element to an array.
// This is used to create an indexed data property into an array.
RUNTIME_FUNCTION(Runtime_AddElement) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_PROPERTY_ATTRIBUTES_CHECKED(attrs, 3);

  uint32_t index = 0;
  key->ToArrayIndex(&index);

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, JSObject::SetElement(object, index, value, attrs, SLOPPY,
                                            false, DEFINE_PROPERTY));
  return *result;
}


RUNTIME_FUNCTION(Runtime_DeleteProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);
  CONVERT_LANGUAGE_MODE_ARG_CHECKED(language_mode, 2);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, JSReceiver::DeleteProperty(object, key, language_mode));
  return *result;
}


static Object* HasOwnPropertyImplementation(Isolate* isolate,
                                            Handle<JSObject> object,
                                            Handle<Name> key) {
  Maybe<bool> maybe = JSReceiver::HasOwnProperty(object, key);
  if (!maybe.IsJust()) return isolate->heap()->exception();
  if (maybe.FromJust()) return isolate->heap()->true_value();
  // Handle hidden prototypes.  If there's a hidden prototype above this thing
  // then we have to check it for properties, because they are supposed to
  // look like they are on this object.
  PrototypeIterator iter(isolate, object);
  if (!iter.IsAtEnd() &&
      Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter))
          ->map()
          ->is_hidden_prototype()) {
    // TODO(verwaest): The recursion is not necessary for keys that are array
    // indices. Removing this.
    return HasOwnPropertyImplementation(
        isolate, Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)),
        key);
  }
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  return isolate->heap()->false_value();
}


RUNTIME_FUNCTION(Runtime_HasOwnProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0)
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);

  uint32_t index;
  const bool key_is_array_index = key->AsArrayIndex(&index);

  // Only JS objects can have properties.
  if (object->IsJSObject()) {
    Handle<JSObject> js_obj = Handle<JSObject>::cast(object);
    // Fast case: either the key is a real named property or it is not
    // an array index and there are no interceptors or hidden
    // prototypes.
    Maybe<bool> maybe = Nothing<bool>();
    if (key_is_array_index) {
      maybe = JSObject::HasOwnElement(js_obj, index);
    } else {
      maybe = JSObject::HasRealNamedProperty(js_obj, key);
    }
    if (!maybe.IsJust()) return isolate->heap()->exception();
    DCHECK(!isolate->has_pending_exception());
    if (maybe.FromJust()) {
      return isolate->heap()->true_value();
    }
    Map* map = js_obj->map();
    if (!key_is_array_index && !map->has_named_interceptor() &&
        !HeapObject::cast(map->prototype())->map()->is_hidden_prototype()) {
      return isolate->heap()->false_value();
    }
    // Slow case.
    return HasOwnPropertyImplementation(isolate, Handle<JSObject>(js_obj),
                                        Handle<Name>(key));
  } else if (object->IsString() && key_is_array_index) {
    // Well, there is one exception:  Handle [] on strings.
    Handle<String> string = Handle<String>::cast(object);
    if (index < static_cast<uint32_t>(string->length())) {
      return isolate->heap()->true_value();
    }
  }
  return isolate->heap()->false_value();
}


RUNTIME_FUNCTION(Runtime_HasProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);

  Maybe<bool> maybe = JSReceiver::HasProperty(receiver, key);
  if (!maybe.IsJust()) return isolate->heap()->exception();
  return isolate->heap()->ToBoolean(maybe.FromJust());
}


RUNTIME_FUNCTION(Runtime_HasElement) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);

  Maybe<bool> maybe = JSReceiver::HasElement(receiver, index);
  if (!maybe.IsJust()) return isolate->heap()->exception();
  return isolate->heap()->ToBoolean(maybe.FromJust());
}


RUNTIME_FUNCTION(Runtime_IsPropertyEnumerable) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);

  Maybe<PropertyAttributes> maybe =
      JSReceiver::GetOwnPropertyAttributes(object, key);
  if (!maybe.IsJust()) return isolate->heap()->exception();
  if (maybe.FromJust() == ABSENT) maybe = Just(DONT_ENUM);
  return isolate->heap()->ToBoolean((maybe.FromJust() & DONT_ENUM) == 0);
}


RUNTIME_FUNCTION(Runtime_GetPropertyNames) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  Handle<JSArray> result;

  isolate->counters()->for_in()->Increment();
  Handle<FixedArray> elements;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, elements,
      JSReceiver::GetKeys(object, JSReceiver::INCLUDE_PROTOS));
  return *isolate->factory()->NewJSArrayWithElements(elements);
}


// Returns either a FixedArray as Runtime_GetPropertyNames,
// or, if the given object has an enum cache that contains
// all enumerable properties of the object and its prototypes
// have none, the map of the object. This is used to speed up
// the check for deletions during a for-in.
RUNTIME_FUNCTION(Runtime_GetPropertyNamesFast) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_CHECKED(JSReceiver, raw_object, 0);

  if (raw_object->IsSimpleEnum()) return raw_object->map();

  HandleScope scope(isolate);
  Handle<JSReceiver> object(raw_object);
  Handle<FixedArray> content;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, content,
      JSReceiver::GetKeys(object, JSReceiver::INCLUDE_PROTOS));

  // Test again, since cache may have been built by preceding call.
  if (object->IsSimpleEnum()) return object->map();

  return *content;
}


// Find the length of the prototype chain that is to be handled as one. If a
// prototype object is hidden it is to be viewed as part of the the object it
// is prototype for.
static int OwnPrototypeChainLength(JSObject* obj) {
  int count = 1;
  for (PrototypeIterator iter(obj->GetIsolate(), obj);
       !iter.IsAtEnd(PrototypeIterator::END_AT_NON_HIDDEN); iter.Advance()) {
    count++;
  }
  return count;
}


// Return the names of the own named properties.
// args[0]: object
// args[1]: PropertyAttributes as int
RUNTIME_FUNCTION(Runtime_GetOwnPropertyNames) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  if (!args[0]->IsJSObject()) {
    return isolate->heap()->undefined_value();
  }
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_SMI_ARG_CHECKED(filter_value, 1);
  PropertyAttributes filter = static_cast<PropertyAttributes>(filter_value);

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (obj->IsJSGlobalProxy()) {
    // Only collect names if access is permitted.
    if (obj->IsAccessCheckNeeded() && !isolate->MayAccess(obj)) {
      isolate->ReportFailedAccessCheck(obj);
      RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
      return *isolate->factory()->NewJSArray(0);
    }
    PrototypeIterator iter(isolate, obj);
    obj = Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
  }

  // Find the number of objects making up this.
  int length = OwnPrototypeChainLength(*obj);

  // Find the number of own properties for each of the objects.
  ScopedVector<int> own_property_count(length);
  int total_property_count = 0;
  {
    PrototypeIterator iter(isolate, obj, PrototypeIterator::START_AT_RECEIVER);
    for (int i = 0; i < length; i++) {
      DCHECK(!iter.IsAtEnd());
      Handle<JSObject> jsproto =
          Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
      // Only collect names if access is permitted.
      if (jsproto->IsAccessCheckNeeded() && !isolate->MayAccess(jsproto)) {
        isolate->ReportFailedAccessCheck(jsproto);
        RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
        return *isolate->factory()->NewJSArray(0);
      }
      int n;
      n = jsproto->NumberOfOwnProperties(filter);
      own_property_count[i] = n;
      total_property_count += n;
      iter.Advance();
    }
  }

  // Allocate an array with storage for all the property names.
  Handle<FixedArray> names =
      isolate->factory()->NewFixedArray(total_property_count);

  // Get the property names.
  int next_copy_index = 0;
  int hidden_strings = 0;
  {
    PrototypeIterator iter(isolate, obj, PrototypeIterator::START_AT_RECEIVER);
    for (int i = 0; i < length; i++) {
      DCHECK(!iter.IsAtEnd());
      Handle<JSObject> jsproto =
          Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
      jsproto->GetOwnPropertyNames(*names, next_copy_index, filter);
      // Names from hidden prototypes may already have been added
      // for inherited function template instances. Count the duplicates
      // and stub them out; the final copy pass at the end ignores holes.
      for (int j = next_copy_index; j < next_copy_index + own_property_count[i];
           j++) {
        Object* name_from_hidden_proto = names->get(j);
        if (isolate->IsInternallyUsedPropertyName(name_from_hidden_proto)) {
          hidden_strings++;
        } else {
          for (int k = 0; k < next_copy_index; k++) {
            Object* name = names->get(k);
            if (name_from_hidden_proto == name) {
              names->set(j, isolate->heap()->hidden_string());
              hidden_strings++;
              break;
            }
          }
        }
      }
      next_copy_index += own_property_count[i];

      iter.Advance();
    }
  }

  // Filter out name of hidden properties object and
  // hidden prototype duplicates.
  if (hidden_strings > 0) {
    Handle<FixedArray> old_names = names;
    names = isolate->factory()->NewFixedArray(names->length() - hidden_strings);
    int dest_pos = 0;
    for (int i = 0; i < total_property_count; i++) {
      Object* name = old_names->get(i);
      if (isolate->IsInternallyUsedPropertyName(name)) {
        hidden_strings--;
        continue;
      }
      names->set(dest_pos++, name);
    }
    DCHECK_EQ(0, hidden_strings);
  }

  return *isolate->factory()->NewJSArrayWithElements(names);
}


// Return the names of the own indexed properties.
// args[0]: object
RUNTIME_FUNCTION(Runtime_GetOwnElementNames) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  if (!args[0]->IsJSObject()) {
    return isolate->heap()->undefined_value();
  }
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  int n = obj->NumberOfOwnElements(NONE);
  Handle<FixedArray> names = isolate->factory()->NewFixedArray(n);
  obj->GetOwnElementKeys(*names, NONE);
  return *isolate->factory()->NewJSArrayWithElements(names);
}


// Return information on whether an object has a named or indexed interceptor.
// args[0]: object
RUNTIME_FUNCTION(Runtime_GetInterceptorInfo) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  if (!args[0]->IsJSObject()) {
    return Smi::FromInt(0);
  }
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  int result = 0;
  if (obj->HasNamedInterceptor()) result |= 2;
  if (obj->HasIndexedInterceptor()) result |= 1;

  return Smi::FromInt(result);
}


// Return property names from named interceptor.
// args[0]: object
RUNTIME_FUNCTION(Runtime_GetNamedInterceptorPropertyNames) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  if (obj->HasNamedInterceptor()) {
    Handle<JSObject> result;
    if (JSObject::GetKeysForNamedInterceptor(obj, obj).ToHandle(&result)) {
      return *result;
    }
  }
  return isolate->heap()->undefined_value();
}


// Return element names from indexed interceptor.
// args[0]: object
RUNTIME_FUNCTION(Runtime_GetIndexedInterceptorElementNames) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  if (obj->HasIndexedInterceptor()) {
    Handle<JSObject> result;
    if (JSObject::GetKeysForIndexedInterceptor(obj, obj).ToHandle(&result)) {
      return *result;
    }
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_OwnKeys) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, raw_object, 0);
  Handle<JSObject> object(raw_object);

  if (object->IsJSGlobalProxy()) {
    // Do access checks before going to the global object.
    if (object->IsAccessCheckNeeded() && !isolate->MayAccess(object)) {
      isolate->ReportFailedAccessCheck(object);
      RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
      return *isolate->factory()->NewJSArray(0);
    }

    PrototypeIterator iter(isolate, object);
    // If proxy is detached we simply return an empty array.
    if (iter.IsAtEnd()) return *isolate->factory()->NewJSArray(0);
    object = Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
  }

  Handle<FixedArray> contents;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, contents, JSReceiver::GetKeys(object, JSReceiver::OWN_ONLY));

  // Some fast paths through GetKeysInFixedArrayFor reuse a cached
  // property array and since the result is mutable we have to create
  // a fresh clone on each invocation.
  int length = contents->length();
  Handle<FixedArray> copy = isolate->factory()->NewFixedArray(length);
  for (int i = 0; i < length; i++) {
    Object* entry = contents->get(i);
    if (entry->IsString()) {
      copy->set(i, entry);
    } else {
      DCHECK(entry->IsNumber());
      HandleScope scope(isolate);
      Handle<Object> entry_handle(entry, isolate);
      Handle<Object> entry_str =
          isolate->factory()->NumberToString(entry_handle);
      copy->set(i, *entry_str);
    }
  }
  return *isolate->factory()->NewJSArrayWithElements(copy);
}


RUNTIME_FUNCTION(Runtime_ToFastProperties) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  if (object->IsJSObject() && !object->IsGlobalObject()) {
    JSObject::MigrateSlowToFast(Handle<JSObject>::cast(object), 0,
                                "RuntimeToFastProperties");
  }
  return *object;
}


RUNTIME_FUNCTION(Runtime_ToBool) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, object, 0);

  return isolate->heap()->ToBoolean(object->BooleanValue());
}


// Returns the type string of a value; see ECMA-262, 11.4.3 (p 47).
// Possible optimizations: put the type string into the oddballs.
RUNTIME_FUNCTION(Runtime_Typeof) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (obj->IsNumber()) return isolate->heap()->number_string();
  HeapObject* heap_obj = HeapObject::cast(obj);

  // typeof an undetectable object is 'undefined'
  if (heap_obj->map()->is_undetectable()) {
    return isolate->heap()->undefined_string();
  }

  InstanceType instance_type = heap_obj->map()->instance_type();
  if (instance_type < FIRST_NONSTRING_TYPE) {
    return isolate->heap()->string_string();
  }

  switch (instance_type) {
    case ODDBALL_TYPE:
      if (heap_obj->IsTrue() || heap_obj->IsFalse()) {
        return isolate->heap()->boolean_string();
      }
      if (heap_obj->IsNull()) {
        return isolate->heap()->object_string();
      }
      DCHECK(heap_obj->IsUndefined());
      return isolate->heap()->undefined_string();
    case SYMBOL_TYPE:
      return isolate->heap()->symbol_string();
    case JS_FUNCTION_TYPE:
    case JS_FUNCTION_PROXY_TYPE:
      return isolate->heap()->function_string();
    default:
      // For any kind of object not handled above, the spec rule for
      // host objects gives that it is okay to return "object"
      return isolate->heap()->object_string();
  }
}


RUNTIME_FUNCTION(Runtime_NewStringWrapper) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, value, 0);
  return *Object::ToObject(isolate, value).ToHandleChecked();
}


RUNTIME_FUNCTION(Runtime_AllocateHeapNumber) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  return *isolate->factory()->NewHeapNumber(0);
}


static Object* Runtime_NewObjectHelper(Isolate* isolate,
                                       Handle<Object> constructor,
                                       Handle<Object> original_constructor,
                                       Handle<AllocationSite> site) {
  // If the constructor isn't a proper function we throw a type error.
  if (!constructor->IsJSFunction()) {
    Vector<Handle<Object> > arguments = HandleVector(&constructor, 1);
    THROW_NEW_ERROR_RETURN_FAILURE(isolate,
                                   NewTypeError("not_constructor", arguments));
  }

  Handle<JSFunction> function = Handle<JSFunction>::cast(constructor);

  CHECK(original_constructor->IsJSFunction());
  Handle<JSFunction> original_function =
      Handle<JSFunction>::cast(original_constructor);


  // If function should not have prototype, construction is not allowed. In this
  // case generated code bailouts here, since function has no initial_map.
  if (!function->should_have_prototype() && !function->shared()->bound()) {
    Vector<Handle<Object> > arguments = HandleVector(&constructor, 1);
    THROW_NEW_ERROR_RETURN_FAILURE(isolate,
                                   NewTypeError("not_constructor", arguments));
  }

  Debug* debug = isolate->debug();
  // Handle stepping into constructors if step into is active.
  if (debug->StepInActive()) {
    debug->HandleStepIn(function, Handle<Object>::null(), 0, true);
  }

  if (function->has_initial_map()) {
    if (function->initial_map()->instance_type() == JS_FUNCTION_TYPE) {
      // The 'Function' function ignores the receiver object when
      // called using 'new' and creates a new JSFunction object that
      // is returned.  The receiver object is only used for error
      // reporting if an error occurs when constructing the new
      // JSFunction. Factory::NewJSObject() should not be used to
      // allocate JSFunctions since it does not properly initialize
      // the shared part of the function. Since the receiver is
      // ignored anyway, we use the global object as the receiver
      // instead of a new JSFunction object. This way, errors are
      // reported the same way whether or not 'Function' is called
      // using 'new'.
      return isolate->global_proxy();
    }
  }

  // The function should be compiled for the optimization hints to be
  // available.
  Compiler::EnsureCompiled(function, CLEAR_EXCEPTION);

  Handle<JSObject> result;
  if (site.is_null()) {
    result = isolate->factory()->NewJSObject(function);
  } else {
    result = isolate->factory()->NewJSObjectWithMemento(function, site);
  }

  // Set up the prototoype using original function.
  // TODO(dslomov): instead of setting the __proto__,
  // use and cache the correct map.
  if (*original_function != *function) {
    if (original_function->has_instance_prototype()) {
      Handle<Object> prototype =
          handle(original_function->instance_prototype(), isolate);
      RETURN_FAILURE_ON_EXCEPTION(
          isolate, JSObject::SetPrototype(result, prototype, false));
    }
  }

  isolate->counters()->constructed_objects()->Increment();
  isolate->counters()->constructed_objects_runtime()->Increment();

  return *result;
}


RUNTIME_FUNCTION(Runtime_NewObject) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, constructor, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, original_constructor, 1);
  return Runtime_NewObjectHelper(isolate, constructor, original_constructor,
                                 Handle<AllocationSite>::null());
}


RUNTIME_FUNCTION(Runtime_NewObjectWithAllocationSite) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(Object, original_constructor, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, constructor, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, feedback, 0);
  Handle<AllocationSite> site;
  if (feedback->IsAllocationSite()) {
    // The feedback can be an AllocationSite or undefined.
    site = Handle<AllocationSite>::cast(feedback);
  }
  return Runtime_NewObjectHelper(isolate, constructor, original_constructor,
                                 site);
}


RUNTIME_FUNCTION(Runtime_FinalizeInstanceSize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  function->CompleteInobjectSlackTracking();

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GlobalProxy) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, global, 0);
  if (!global->IsJSGlobalObject()) return isolate->heap()->null_value();
  return JSGlobalObject::cast(global)->global_proxy();
}


RUNTIME_FUNCTION(Runtime_LookupAccessor) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_SMI_ARG_CHECKED(flag, 2);
  AccessorComponent component = flag == 0 ? ACCESSOR_GETTER : ACCESSOR_SETTER;
  if (!receiver->IsJSObject()) return isolate->heap()->undefined_value();
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::GetAccessor(Handle<JSObject>::cast(receiver), name, component));
  return *result;
}


RUNTIME_FUNCTION(Runtime_LoadMutableDouble) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, index, 1);
  RUNTIME_ASSERT((index->value() & 1) == 1);
  FieldIndex field_index =
      FieldIndex::ForLoadByFieldIndex(object->map(), index->value());
  if (field_index.is_inobject()) {
    RUNTIME_ASSERT(field_index.property_index() <
                   object->map()->inobject_properties());
  } else {
    RUNTIME_ASSERT(field_index.outobject_array_index() <
                   object->properties()->length());
  }
  return *JSObject::FastPropertyAt(object, Representation::Double(),
                                   field_index);
}


RUNTIME_FUNCTION(Runtime_TryMigrateInstance) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  if (!object->IsJSObject()) return Smi::FromInt(0);
  Handle<JSObject> js_object = Handle<JSObject>::cast(object);
  if (!js_object->map()->is_deprecated()) return Smi::FromInt(0);
  // This call must not cause lazy deopts, because it's called from deferred
  // code where we can't handle lazy deopts for lack of a suitable bailout
  // ID. So we just try migration and signal failure if necessary,
  // which will also trigger a deopt.
  if (!JSObject::TryMigrateInstance(js_object)) return Smi::FromInt(0);
  return *object;
}


RUNTIME_FUNCTION(Runtime_IsJSGlobalProxy) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSGlobalProxy());
}


static bool IsValidAccessor(Handle<Object> obj) {
  return obj->IsUndefined() || obj->IsSpecFunction() || obj->IsNull();
}


// Implements part of 8.12.9 DefineOwnProperty.
// There are 3 cases that lead here:
// Step 4b - define a new accessor property.
// Steps 9c & 12 - replace an existing data property with an accessor property.
// Step 12 - update an existing accessor property with an accessor or generic
//           descriptor.
RUNTIME_FUNCTION(Runtime_DefineAccessorPropertyUnchecked) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  RUNTIME_ASSERT(!obj->IsNull());
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, getter, 2);
  RUNTIME_ASSERT(IsValidAccessor(getter));
  CONVERT_ARG_HANDLE_CHECKED(Object, setter, 3);
  RUNTIME_ASSERT(IsValidAccessor(setter));
  CONVERT_PROPERTY_ATTRIBUTES_CHECKED(attrs, 4);

  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::DefineAccessor(obj, name, getter, setter, attrs));
  return isolate->heap()->undefined_value();
}


// Implements part of 8.12.9 DefineOwnProperty.
// There are 3 cases that lead here:
// Step 4a - define a new data property.
// Steps 9b & 12 - replace an existing accessor property with a data property.
// Step 12 - update an existing data property with a data or generic
//           descriptor.
RUNTIME_FUNCTION(Runtime_DefineDataPropertyUnchecked) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, js_object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, obj_value, 2);
  CONVERT_PROPERTY_ATTRIBUTES_CHECKED(attrs, 3);

  LookupIterator it(js_object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
  if (it.IsFound() && it.state() == LookupIterator::ACCESS_CHECK) {
    if (!isolate->MayAccess(js_object)) {
      return isolate->heap()->undefined_value();
    }
    it.Next();
  }

  // Take special care when attributes are different and there is already
  // a property.
  if (it.state() == LookupIterator::ACCESSOR) {
    // Use IgnoreAttributes version since a readonly property may be
    // overridden and SetProperty does not allow this.
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        JSObject::SetOwnPropertyIgnoreAttributes(
            js_object, name, obj_value, attrs, JSObject::DONT_FORCE_FIELD));
    return *result;
  }

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::DefineObjectProperty(js_object, name, obj_value, attrs));
  return *result;
}


// Return property without being observable by accessors or interceptors.
RUNTIME_FUNCTION(Runtime_GetDataProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);
  return *JSObject::GetDataProperty(object, key);
}


RUNTIME_FUNCTION(Runtime_HasFastPackedElements) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(HeapObject, obj, 0);
  return isolate->heap()->ToBoolean(
      IsFastPackedElementsKind(obj->map()->elements_kind()));
}


RUNTIME_FUNCTION(Runtime_ValueOf) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (!obj->IsJSValue()) return obj;
  return JSValue::cast(obj)->value();
}


RUNTIME_FUNCTION(Runtime_SetValueOf) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  CONVERT_ARG_CHECKED(Object, value, 1);
  if (!obj->IsJSValue()) return value;
  JSValue::cast(obj)->set_value(value);
  return value;
}


RUNTIME_FUNCTION(Runtime_JSValueGetValue) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSValue, obj, 0);
  return JSValue::cast(obj)->value();
}


RUNTIME_FUNCTION(Runtime_HeapObjectGetMap) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(HeapObject, obj, 0);
  return obj->map();
}


RUNTIME_FUNCTION(Runtime_MapGetInstanceType) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Map, map, 0);
  return Smi::FromInt(map->instance_type());
}


RUNTIME_FUNCTION(Runtime_ObjectEquals) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(Object, obj1, 0);
  CONVERT_ARG_CHECKED(Object, obj2, 1);
  return isolate->heap()->ToBoolean(obj1 == obj2);
}


RUNTIME_FUNCTION(Runtime_IsObject) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (!obj->IsHeapObject()) return isolate->heap()->false_value();
  if (obj->IsNull()) return isolate->heap()->true_value();
  if (obj->IsUndetectableObject()) return isolate->heap()->false_value();
  Map* map = HeapObject::cast(obj)->map();
  bool is_non_callable_spec_object =
      map->instance_type() >= FIRST_NONCALLABLE_SPEC_OBJECT_TYPE &&
      map->instance_type() <= LAST_NONCALLABLE_SPEC_OBJECT_TYPE;
  return isolate->heap()->ToBoolean(is_non_callable_spec_object);
}


RUNTIME_FUNCTION(Runtime_IsUndetectableObject) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsUndetectableObject());
}


RUNTIME_FUNCTION(Runtime_IsSpecObject) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsSpecObject());
}


RUNTIME_FUNCTION(Runtime_ClassOf) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (!obj->IsJSReceiver()) return isolate->heap()->null_value();
  return JSReceiver::cast(obj)->class_name();
}


RUNTIME_FUNCTION(Runtime_DefineGetterPropertyUnchecked) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, getter, 2);
  CONVERT_PROPERTY_ATTRIBUTES_CHECKED(attrs, 3);

  RETURN_FAILURE_ON_EXCEPTION(
      isolate,
      JSObject::DefineAccessor(object, name, getter,
                               isolate->factory()->null_value(), attrs));
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DefineSetterPropertyUnchecked) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, setter, 2);
  CONVERT_PROPERTY_ATTRIBUTES_CHECKED(attrs, 3);

  RETURN_FAILURE_ON_EXCEPTION(
      isolate,
      JSObject::DefineAccessor(object, name, isolate->factory()->null_value(),
                               setter, attrs));
  return isolate->heap()->undefined_value();
}
}
}  // namespace v8::internal
