// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/bootstrapper.h"
#include "src/debug/debug.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/property-descriptor.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

MaybeHandle<Object> Runtime::GetObjectProperty(Isolate* isolate,
                                               Handle<Object> object,
                                               Handle<Object> key,
                                               bool* is_found_out) {
  if (object->IsUndefined(isolate) || object->IsNull(isolate)) {
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
  if (receiver_obj->IsJSObject()) {
    if (!receiver_obj->IsJSGlobalProxy() &&
        !receiver_obj->IsAccessCheckNeeded() && key_obj->IsName()) {
      DisallowHeapAllocation no_allocation;
      Handle<JSObject> receiver = Handle<JSObject>::cast(receiver_obj);
      Handle<Name> key = Handle<Name>::cast(key_obj);
      if (receiver->IsJSGlobalObject()) {
        // Attempt dictionary lookup.
        GlobalDictionary* dictionary = receiver->global_dictionary();
        int entry = dictionary->FindEntry(key);
        if (entry != GlobalDictionary::kNotFound) {
          DCHECK(dictionary->ValueAt(entry)->IsPropertyCell());
          PropertyCell* cell = PropertyCell::cast(dictionary->ValueAt(entry));
          if (cell->property_details().type() == DATA) {
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
            (dictionary->DetailsAt(entry).type() == DATA)) {
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
      // become FAST_DOUBLE_ELEMENTS.
      Handle<JSObject> js_object = Handle<JSObject>::cast(receiver_obj);
      ElementsKind elements_kind = js_object->GetElementsKind();
      if (IsFastDoubleElementsKind(elements_kind)) {
        if (Smi::cast(*key_obj)->value() >= js_object->elements()->length()) {
          elements_kind = IsFastHoleyElementsKind(elements_kind)
                              ? FAST_HOLEY_ELEMENTS
                              : FAST_ELEMENTS;
          JSObject::TransitionElementsKind(js_object, elements_kind);
        }
      } else {
        DCHECK(IsFastSmiOrObjectElementsKind(elements_kind) ||
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


Maybe<bool> Runtime::DeleteObjectProperty(Isolate* isolate,
                                          Handle<JSReceiver> receiver,
                                          Handle<Object> key,
                                          LanguageMode language_mode) {
  bool success = false;
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, receiver, key, &success, LookupIterator::OWN);
  if (!success) return Nothing<bool>();

  return JSReceiver::DeleteProperty(&it, language_mode);
}

// ES6 19.1.3.2
RUNTIME_FUNCTION(Runtime_ObjectHasOwnProperty) {
  HandleScope scope(isolate);
  Handle<Object> property = args.at<Object>(1);

  Handle<Name> key;
  uint32_t index;
  bool key_is_array_index = property->ToArrayIndex(&index);

  if (!key_is_array_index) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, key,
                                       Object::ToName(isolate, property));
    key_is_array_index = key->AsArrayIndex(&index);
  }

  Handle<Object> object = args.at<Object>(0);

  if (object->IsJSObject()) {
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
    if (!result.IsJust()) return isolate->heap()->exception();
    return isolate->heap()->ToBoolean(result.FromJust());

  } else if (object->IsString()) {
    return isolate->heap()->ToBoolean(
        key_is_array_index
            ? index < static_cast<uint32_t>(String::cast(*object)->length())
            : key->Equals(isolate->heap()->length_string()));
  } else if (object->IsNull(isolate) || object->IsUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kUndefinedOrNullToObject));
  }

  return isolate->heap()->false_value();
}

MaybeHandle<Object> Runtime::SetObjectProperty(Isolate* isolate,
                                               Handle<Object> object,
                                               Handle<Object> key,
                                               Handle<Object> value,
                                               LanguageMode language_mode) {
  if (object->IsUndefined(isolate) || object->IsNull(isolate)) {
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

  MAYBE_RETURN_NULL(Object::SetProperty(&it, value, language_mode,
                                        Object::MAY_BE_STORE_FROM_KEYED));
  return value;
}


RUNTIME_FUNCTION(Runtime_GetPrototype) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, obj, 0);
  RETURN_RESULT_OR_FAILURE(isolate, JSReceiver::GetPrototype(isolate, obj));
}


RUNTIME_FUNCTION(Runtime_InternalSetPrototype) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  MAYBE_RETURN(
      JSReceiver::SetPrototype(obj, prototype, false, Object::THROW_ON_ERROR),
      isolate->heap()->exception());
  return *obj;
}


RUNTIME_FUNCTION(Runtime_SetPrototype) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  MAYBE_RETURN(
      JSReceiver::SetPrototype(obj, prototype, true, Object::THROW_ON_ERROR),
      isolate->heap()->exception());
  return *obj;
}

RUNTIME_FUNCTION(Runtime_OptimizeObjectForAddingMultipleProperties) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
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


namespace {

Object* StoreGlobalViaContext(Isolate* isolate, int slot, Handle<Object> value,
                              LanguageMode language_mode) {
  // Go up context chain to the script context.
  Handle<Context> script_context(isolate->context()->script_context(), isolate);
  DCHECK(script_context->IsScriptContext());
  DCHECK(script_context->get(slot)->IsPropertyCell());

  // Lookup the named property on the global object.
  Handle<ScopeInfo> scope_info(script_context->scope_info(), isolate);
  Handle<Name> name(scope_info->ContextSlotName(slot), isolate);
  Handle<JSGlobalObject> global_object(script_context->global_object(),
                                       isolate);
  LookupIterator it(global_object, name, global_object, LookupIterator::OWN);

  // Switch to fast mode only if there is a data property and it's not on
  // a hidden prototype.
  if (it.state() == LookupIterator::DATA &&
      it.GetHolder<Object>().is_identical_to(global_object)) {
    // Now update cell in the script context.
    Handle<PropertyCell> cell = it.GetPropertyCell();
    script_context->set(slot, *cell);
  } else {
    // This is not a fast case, so keep this access in a slow mode.
    // Store empty_property_cell here to release the outdated property cell.
    script_context->set(slot, isolate->heap()->empty_property_cell());
  }

  MAYBE_RETURN(Object::SetProperty(&it, value, language_mode,
                                   Object::CERTAINLY_NOT_STORE_FROM_KEYED),
               isolate->heap()->exception());
  return *value;
}

}  // namespace


RUNTIME_FUNCTION(Runtime_StoreGlobalViaContext_Sloppy) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_SMI_ARG_CHECKED(slot, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);

  return StoreGlobalViaContext(isolate, slot, value, SLOPPY);
}


RUNTIME_FUNCTION(Runtime_StoreGlobalViaContext_Strict) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_SMI_ARG_CHECKED(slot, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);

  return StoreGlobalViaContext(isolate, slot, value, STRICT);
}


RUNTIME_FUNCTION(Runtime_GetProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);

  RETURN_RESULT_OR_FAILURE(isolate,
                           Runtime::GetObjectProperty(isolate, object, key));
}

// KeyedGetProperty is called from KeyedLoadIC::GenerateGeneric.
RUNTIME_FUNCTION(Runtime_KeyedGetProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

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
  if (!maybe.IsJust()) return isolate->heap()->exception();
  CHECK(!it.IsFound());
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
  if (!maybe.IsJust()) return isolate->heap()->exception();
  CHECK(!it.IsFound());

  if (object->IsJSArray()) {
    Handle<JSArray> array = Handle<JSArray>::cast(object);
    CHECK(!JSArray::WouldChangeReadOnlyLength(array, index));
  }
#endif

  RETURN_RESULT_OR_FAILURE(isolate, JSObject::SetOwnElementIgnoreAttributes(
                                        object, index, value, NONE));
}


RUNTIME_FUNCTION(Runtime_AppendElement) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  CHECK(!value->IsTheHole(isolate));

  uint32_t index;
  CHECK(array->length()->ToArrayIndex(&index));

  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::AddDataElement(array, index, value, NONE));
  JSObject::ValidateElements(array);
  return *array;
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


RUNTIME_FUNCTION(Runtime_DeleteProperty_Sloppy) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  return DeleteProperty(isolate, object, key, SLOPPY);
}


RUNTIME_FUNCTION(Runtime_DeleteProperty_Strict) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  return DeleteProperty(isolate, object, key, STRICT);
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
  if (!maybe.IsJust()) return isolate->heap()->exception();
  return isolate->heap()->ToBoolean(maybe.FromJust());
}


RUNTIME_FUNCTION(Runtime_GetOwnPropertyKeys) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
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


RUNTIME_FUNCTION(Runtime_ToFastProperties) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  if (object->IsJSObject() && !object->IsJSGlobalObject()) {
    JSObject::MigrateSlowToFast(Handle<JSObject>::cast(object), 0,
                                "RuntimeToFastProperties");
  }
  return *object;
}


RUNTIME_FUNCTION(Runtime_AllocateHeapNumber) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  return *isolate->factory()->NewHeapNumber(0);
}


RUNTIME_FUNCTION(Runtime_NewObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, new_target, 1);
  RETURN_RESULT_OR_FAILURE(isolate, JSObject::New(target, new_target));
}


RUNTIME_FUNCTION(Runtime_FinalizeInstanceSize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(Map, initial_map, 0);
  initial_map->CompleteInobjectSlackTracking();

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_LoadMutableDouble) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, index, 1);
  CHECK((index->value() & 1) == 1);
  FieldIndex field_index =
      FieldIndex::ForLoadByFieldIndex(object->map(), index->value());
  if (field_index.is_inobject()) {
    CHECK(field_index.property_index() <
          object->map()->GetInObjectProperties());
  } else {
    CHECK(field_index.outobject_array_index() < object->properties()->length());
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

static bool IsValidAccessor(Isolate* isolate, Handle<Object> obj) {
  return obj->IsUndefined(isolate) || obj->IsCallable() || obj->IsNull(isolate);
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
  DCHECK(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_PROPERTY_ATTRIBUTES_CHECKED(attrs, 3);
  CONVERT_SMI_ARG_CHECKED(set_function_name, 4);

  if (set_function_name) {
    DCHECK(value->IsJSFunction());
    JSFunction::SetName(Handle<JSFunction>::cast(value), name,
                        isolate->factory()->empty_string());
  }

  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, object, LookupIterator::OWN);
  // Cannot fail since this should only be called when
  // creating an object literal.
  CHECK(JSObject::DefineOwnPropertyIgnoreAttributes(&it, value, attrs,
                                                    Object::DONT_THROW)
            .IsJust());
  return *object;
}

RUNTIME_FUNCTION(Runtime_DefineDataProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_PROPERTY_ATTRIBUTES_CHECKED(attrs, 3);
  CONVERT_SMI_ARG_CHECKED(set_function_name, 4);

  if (set_function_name) {
    DCHECK(value->IsJSFunction());
    JSFunction::SetName(Handle<JSFunction>::cast(value), name,
                        isolate->factory()->empty_string());
  }

  PropertyDescriptor desc;
  desc.set_writable(!(attrs & ReadOnly));
  desc.set_enumerable(!(attrs & DontEnum));
  desc.set_configurable(!(attrs & DontDelete));
  desc.set_value(value);

  Maybe<bool> result = JSReceiver::DefineOwnProperty(isolate, receiver, name,
                                                     &desc, Object::DONT_THROW);
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  if (result.IsNothing()) {
    DCHECK(isolate->has_pending_exception());
    return isolate->heap()->exception();
  }

  return *receiver;
}

// Return property without being observable by accessors or interceptors.
RUNTIME_FUNCTION(Runtime_GetDataProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  return *JSReceiver::GetDataProperty(object, name);
}

RUNTIME_FUNCTION(Runtime_GetConstructorName) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);

  CHECK(!object->IsUndefined(isolate) && !object->IsNull(isolate));
  Handle<JSReceiver> recv = Object::ToObject(isolate, object).ToHandleChecked();
  return *JSReceiver::GetConstructorName(recv);
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


RUNTIME_FUNCTION(Runtime_IsJSReceiver) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSReceiver());
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

  if (String::cast(getter->shared()->name())->length() == 0) {
    JSFunction::SetName(getter, name, isolate->factory()->get_string());
  }

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

  if (String::cast(setter->shared()->name())->length() == 0) {
    JSFunction::SetName(setter, name, isolate->factory()->set_string());
  }

  RETURN_FAILURE_ON_EXCEPTION(
      isolate,
      JSObject::DefineAccessor(object, name, isolate->factory()->null_value(),
                               setter, attrs));
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_ToObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToObject(isolate, object));
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


// TODO(bmeurer): Kill this special wrapper and use TF compatible LessThan,
// GreaterThan, etc. which return true or false.
RUNTIME_FUNCTION(Runtime_Compare) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, y, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, ncr, 2);
  Maybe<ComparisonResult> result = Object::Compare(x, y);
  if (result.IsJust()) {
    switch (result.FromJust()) {
      case ComparisonResult::kLessThan:
        return Smi::FromInt(LESS);
      case ComparisonResult::kEqual:
        return Smi::FromInt(EQUAL);
      case ComparisonResult::kGreaterThan:
        return Smi::FromInt(GREATER);
      case ComparisonResult::kUndefined:
        return *ncr;
    }
    UNREACHABLE();
  }
  return isolate->heap()->exception();
}

RUNTIME_FUNCTION(Runtime_HasInPrototypeChain) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  Maybe<bool> result =
      JSReceiver::HasInPrototypeChain(isolate, object, prototype);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}


// ES6 section 7.4.7 CreateIterResultObject ( value, done )
RUNTIME_FUNCTION(Runtime_CreateIterResultObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, done, 1);
  Handle<JSObject> result =
      isolate->factory()->NewJSObjectFromMap(isolate->iterator_result_map());
  result->InObjectPropertyAtPut(JSIteratorResult::kValueIndex, *value);
  result->InObjectPropertyAtPut(JSIteratorResult::kDoneIndex, *done);
  return *result;
}


RUNTIME_FUNCTION(Runtime_IsAccessCheckNeeded) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, object, 0);
  return isolate->heap()->ToBoolean(object->IsAccessCheckNeeded());
}


RUNTIME_FUNCTION(Runtime_CreateDataProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, o, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  bool success;
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, o, key, &success, LookupIterator::OWN);
  if (!success) return isolate->heap()->exception();
  MAYBE_RETURN(
      JSReceiver::CreateDataProperty(&it, value, Object::THROW_ON_ERROR),
      isolate->heap()->exception());
  return *value;
}

RUNTIME_FUNCTION(Runtime_LoadModuleExport) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  Handle<Module> module(isolate->context()->module());
  return *Module::LoadExport(module, name);
}

RUNTIME_FUNCTION(Runtime_LoadModuleImport) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, module_request, 1);
  Handle<Module> module(isolate->context()->module());
  return *Module::LoadImport(module, name, module_request->value());
}

RUNTIME_FUNCTION(Runtime_StoreModuleExport) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  Handle<Module> module(isolate->context()->module());
  Module::StoreExport(module, name, value);
  return isolate->heap()->undefined_value();
}

}  // namespace internal
}  // namespace v8
