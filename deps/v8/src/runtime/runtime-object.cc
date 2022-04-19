// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/prettyprinter.h"
#include "src/base/macros.h"
#include "src/builtins/builtins.h"
#include "src/common/globals.h"
#include "src/common/message-template.h"
#include "src/debug/debug.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/frames.h"
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
#include "src/objects/property-details.h"
#include "src/objects/swiss-name-dictionary-inl.h"
#include "src/runtime/runtime-utils.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

MaybeHandle<Object> Runtime::GetObjectProperty(
    Isolate* isolate, Handle<Object> lookup_start_object, Handle<Object> key,
    Handle<Object> receiver, bool* is_found) {
  if (receiver.is_null()) {
    receiver = lookup_start_object;
  }
  if (lookup_start_object->IsNullOrUndefined(isolate)) {
    ErrorUtils::ThrowLoadFromNullOrUndefined(isolate, lookup_start_object, key);
    return MaybeHandle<Object>();
  }

  bool success = false;
  PropertyKey lookup_key(isolate, key, &success);
  if (!success) return MaybeHandle<Object>();
  LookupIterator it =
      LookupIterator(isolate, receiver, lookup_key, lookup_start_object);

  MaybeHandle<Object> result = Object::GetProperty(&it);
  if (is_found) *is_found = it.IsFound();

  if (!it.IsFound() && key->IsSymbol() &&
      Symbol::cast(*key).is_private_name()) {
    MessageTemplate message =
        Symbol::cast(*key).IsPrivateBrand()
            ? MessageTemplate::kInvalidPrivateBrandInstance
            : MessageTemplate::kInvalidPrivateMemberRead;
    THROW_NEW_ERROR(isolate, NewTypeError(message, key, lookup_start_object),
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
  Maybe<bool> maybe = JSReceiver::HasProperty(isolate, receiver, name);
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

void GeneralizeAllTransitionsToFieldAsMutable(Isolate* isolate, Handle<Map> map,
                                              Handle<Name> name) {
  InternalIndex descriptor(map->NumberOfOwnDescriptors());

  Handle<Map> target_maps[kPropertyAttributesCombinationsCount];
  int target_maps_count = 0;

  // Collect all outgoing field transitions.
  {
    DisallowGarbageCollection no_gc;
    TransitionsAccessor transitions(isolate, *map);
    transitions.ForEachTransitionTo(
        *name,
        [&](Map target) {
          DCHECK_EQ(descriptor, target.LastAdded());
          DCHECK_EQ(*name, target.GetLastDescriptorName(isolate));
          PropertyDetails details = target.GetLastDescriptorDetails(isolate);
          // Currently, we track constness only for fields.
          if (details.kind() == PropertyKind::kData &&
              details.constness() == PropertyConstness::kConst) {
            target_maps[target_maps_count++] = handle(target, isolate);
          }
          DCHECK_IMPLIES(details.kind() == PropertyKind::kAccessor,
                         details.constness() == PropertyConstness::kConst);
        },
        &no_gc);
    CHECK_LE(target_maps_count, kPropertyAttributesCombinationsCount);
  }

  for (int i = 0; i < target_maps_count; i++) {
    Handle<Map> target = target_maps[i];
    PropertyDetails details =
        target->instance_descriptors(isolate).GetDetails(descriptor);
    Handle<FieldType> field_type(
        target->instance_descriptors(isolate).GetFieldType(descriptor),
        isolate);
    MapUpdater::GeneralizeField(isolate, target, descriptor,
                                PropertyConstness::kMutable,
                                details.representation(), field_type);
    DCHECK_EQ(PropertyConstness::kMutable, target->instance_descriptors(isolate)
                                               .GetDetails(descriptor)
                                               .constness());
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
  DCHECK(receiver_map->IsJSObjectMap());

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

  // Zap the property to avoid keeping objects alive. Zapping is not necessary
  // for properties stored in the descriptor array.
  if (details.location() == PropertyLocation::kField) {
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
        if (!FLAG_enable_third_party_heap) {
          MemoryChunk* chunk = MemoryChunk::FromHeapObject(*receiver);
          chunk->InvalidateRecordedSlots(*receiver);
        }
      }
    }
  }
  // If the {receiver_map} was marked stable before, then there could be
  // optimized code that depends on the assumption that no object that
  // reached this {receiver_map} transitions away from it without triggering
  // the "deoptimize dependent code" mechanism.
  receiver_map->NotifyLeafMapLayoutChange(isolate);
  // Finally, perform the map rollback.
  receiver->set_map(*parent_map, kReleaseStore);
#if VERIFY_HEAP
  receiver->HeapObjectVerify(isolate);
  receiver->property_array().PropertyArrayVerify(isolate);
#endif

  // If the {descriptor} was "const" so far, we need to update the
  // {receiver_map} here, otherwise we could get the constants wrong, i.e.
  //
  //   o.x = 1;
  //   [change o.x's attributes or reconfigure property kind]
  //   delete o.x;
  //   o.x = 2;
  //
  // could trick V8 into thinking that `o.x` is still 1 even after the second
  // assignment.

  // Step 1: Migrate object to an up-to-date shape.
  if (parent_map->is_deprecated()) {
    JSObject::MigrateInstance(isolate, Handle<JSObject>::cast(receiver));
    parent_map = handle(receiver->map(), isolate);
  }

  // Step 2: Mark outgoing transitions from the up-to-date version of the
  // parent_map to same property name of any kind or attributes as mutable.
  // Also migrate object to the up-to-date map to make the object shapes
  // converge sooner.
  GeneralizeAllTransitionsToFieldAsMutable(isolate, parent_map, key);

  return true;
}

}  // namespace

Maybe<bool> Runtime::DeleteObjectProperty(Isolate* isolate,
                                          Handle<JSReceiver> receiver,
                                          Handle<Object> key,
                                          LanguageMode language_mode) {
  if (DeleteObjectPropertyFast(isolate, receiver, key)) return Just(true);

  bool success = false;
  PropertyKey lookup_key(isolate, key, &success);
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
  PropertyKey key(isolate, property, &success);
  if (!success) return ReadOnlyRoots(isolate).exception();

  Handle<Object> object = args.at(0);

  if (object->IsJSModuleNamespace()) {
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
  Handle<Object> object = args.at(0);
  Handle<Object> property = args.at(1);

  bool success;
  PropertyKey key(isolate, property, &success);
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
      PropertyKind::kData, NONE,
      PropertyDetails::kConstIfDictConstnessTracking);
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

RUNTIME_FUNCTION(Runtime_AddPrivateBrand) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 4);
  Handle<JSReceiver> receiver = args.at<JSReceiver>(0);
  Handle<Symbol> brand = args.at<Symbol>(1);
  Handle<Context> context = args.at<Context>(2);
  int depth = args.smi_value_at(3);
  DCHECK(brand->is_private_name());

  LookupIterator it(isolate, receiver, brand, LookupIterator::OWN);

  if (it.IsFound()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kInvalidPrivateBrandReinitialization,
                     brand));
  }

  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

  // Look for the context in |depth| in the context chain to store it
  // in the instance with the brand variable as key, which is needed by
  // the debugger for retrieving names of private methods.
  DCHECK_GE(depth, 0);
  for (; depth > 0; depth--) {
    context =
        handle(Context::cast(context->get(Context::PREVIOUS_INDEX)), isolate);
  }
  DCHECK_EQ(context->scope_info().scope_type(), ScopeType::CLASS_SCOPE);
  CHECK(Object::AddDataProperty(&it, context, attributes, Just(kDontThrow),
                                StoreOrigin::kMaybeKeyed)
            .FromJust());
  return *receiver;
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
    MaybeHandle<String> maybe_property =
        Object::NoSideEffectsToMaybeString(isolate, key);
    Handle<String> property_name;
    if (maybe_property.ToHandle(&property_name)) {
      THROW_NEW_ERROR(
          isolate,
          NewTypeError(MessageTemplate::kNonObjectPropertyStoreWithProperty,
                       object, property_name),
          Object);
    } else {
      THROW_NEW_ERROR(
          isolate,
          NewTypeError(MessageTemplate::kNonObjectPropertyStore, object),
          Object);
    }
  }

  // Check if the given key is an array index.
  bool success = false;
  PropertyKey lookup_key(isolate, key, &success);
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

MaybeHandle<Object> Runtime::DefineObjectOwnProperty(
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
  PropertyKey lookup_key(isolate, key, &success);
  if (!success) return MaybeHandle<Object>();
  LookupIterator it(isolate, object, lookup_key, LookupIterator::OWN);

  if (key->IsSymbol() && Symbol::cast(*key).is_private_name()) {
    Handle<Symbol> private_symbol = Handle<Symbol>::cast(key);
    if (it.IsFound()) {
      Handle<Object> name_string(private_symbol->description(), isolate);
      DCHECK(name_string->IsString());
      MessageTemplate message =
          private_symbol->is_private_brand()
              ? MessageTemplate::kInvalidPrivateBrandReinitialization
              : MessageTemplate::kInvalidPrivateFieldReinitialization;
      THROW_NEW_ERROR(isolate, NewTypeError(message, name_string), Object);
    } else {
      MAYBE_RETURN_NULL(JSReceiver::AddPrivateField(&it, value, should_throw));
    }
  } else {
    MAYBE_RETURN_NULL(JSReceiver::CreateDataProperty(&it, value, should_throw));
  }

  return value;
}

RUNTIME_FUNCTION(Runtime_InternalSetPrototype) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<JSReceiver> obj = args.at<JSReceiver>(0);
  Handle<Object> prototype = args.at(1);
  MAYBE_RETURN(
      JSReceiver::SetPrototype(isolate, obj, prototype, false, kThrowOnError),
      ReadOnlyRoots(isolate).exception());
  return *obj;
}

RUNTIME_FUNCTION(Runtime_OptimizeObjectForAddingMultipleProperties) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<JSObject> object = args.at<JSObject>(0);
  int properties = args.smi_value_at(1);
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

  Handle<JSReceiver> receiver = args.at<JSReceiver>(0);

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

  Handle<JSReceiver> receiver = args.at<JSReceiver>(0);

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

  Handle<JSReceiver> receiver = args.at<JSReceiver>(0);

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

  Handle<JSReceiver> receiver = args.at<JSReceiver>(0);

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
  Handle<Object> object = args.at(0);

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
  Handle<JSReceiver> object = args.at<JSReceiver>(0);

  MAYBE_RETURN(JSReceiver::PreventExtensions(Handle<JSReceiver>::cast(object),
                                             kThrowOnError),
               ReadOnlyRoots(isolate).exception());
  return *object;
}

RUNTIME_FUNCTION(Runtime_JSReceiverPreventExtensionsDontThrow) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSReceiver> object = args.at<JSReceiver>(0);

  Maybe<bool> result = JSReceiver::PreventExtensions(
      Handle<JSReceiver>::cast(object), kDontThrow);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}

RUNTIME_FUNCTION(Runtime_JSReceiverGetPrototypeOf) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSReceiver> receiver = args.at<JSReceiver>(0);

  RETURN_RESULT_OR_FAILURE(isolate,
                           JSReceiver::GetPrototype(isolate, receiver));
}

RUNTIME_FUNCTION(Runtime_JSReceiverSetPrototypeOfThrow) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());
  Handle<JSReceiver> object = args.at<JSReceiver>(0);
  Handle<Object> proto = args.at(1);

  MAYBE_RETURN(
      JSReceiver::SetPrototype(isolate, object, proto, true, kThrowOnError),
      ReadOnlyRoots(isolate).exception());

  return *object;
}

RUNTIME_FUNCTION(Runtime_JSReceiverSetPrototypeOfDontThrow) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());
  Handle<JSReceiver> object = args.at<JSReceiver>(0);
  Handle<Object> proto = args.at(1);

  Maybe<bool> result =
      JSReceiver::SetPrototype(isolate, object, proto, true, kDontThrow);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}

RUNTIME_FUNCTION(Runtime_GetProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3 || args.length() == 2);
  Handle<Object> lookup_start_obj = args.at(0);
  Handle<Object> key_obj = args.at(1);
  Handle<Object> receiver_obj = lookup_start_obj;
  if (args.length() == 3) {
    receiver_obj = args.at<Object>(2);
  }

  // Fast cases for getting named properties of the lookup_start_obj JSObject
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
  if (lookup_start_obj->IsJSObject()) {
    Handle<JSObject> lookup_start_object =
        Handle<JSObject>::cast(lookup_start_obj);
    if (!lookup_start_object->IsJSGlobalProxy() &&
        !lookup_start_object->IsAccessCheckNeeded() && key_obj->IsName()) {
      Handle<Name> key = Handle<Name>::cast(key_obj);
      key_obj = key = isolate->factory()->InternalizeName(key);

      DisallowGarbageCollection no_gc;
      if (lookup_start_object->IsJSGlobalObject()) {
        // Attempt dictionary lookup.
        GlobalDictionary dictionary = JSGlobalObject::cast(*lookup_start_object)
                                          .global_dictionary(kAcquireLoad);
        InternalIndex entry = dictionary.FindEntry(isolate, key);
        if (entry.is_found()) {
          PropertyCell cell = dictionary.CellAt(entry);
          if (cell.property_details().kind() == PropertyKind::kData) {
            Object value = cell.value();
            if (!value.IsTheHole(isolate)) return value;
            // If value is the hole (meaning, absent) do the general lookup.
          }
        }
      } else if (!lookup_start_object->HasFastProperties()) {
        // Attempt dictionary lookup.
        if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
          SwissNameDictionary dictionary =
              lookup_start_object->property_dictionary_swiss();
          InternalIndex entry = dictionary.FindEntry(isolate, *key);
          if (entry.is_found() &&
              (dictionary.DetailsAt(entry).kind() == PropertyKind::kData)) {
            return dictionary.ValueAt(entry);
          }
        } else {
          NameDictionary dictionary =
              lookup_start_object->property_dictionary();
          InternalIndex entry = dictionary.FindEntry(isolate, key);
          if ((entry.is_found()) &&
              (dictionary.DetailsAt(entry).kind() == PropertyKind::kData)) {
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
      ElementsKind elements_kind = lookup_start_object->GetElementsKind();
      if (IsDoubleElementsKind(elements_kind)) {
        if (Smi::ToInt(*key_obj) >= lookup_start_object->elements().length()) {
          elements_kind = IsHoleyElementsKind(elements_kind) ? HOLEY_ELEMENTS
                                                             : PACKED_ELEMENTS;
          JSObject::TransitionElementsKind(lookup_start_object, elements_kind);
        }
      } else {
        DCHECK(IsSmiOrObjectElementsKind(elements_kind) ||
               !IsFastElementsKind(elements_kind));
      }
    }
  } else if (lookup_start_obj->IsString() && key_obj->IsSmi()) {
    // Fast case for string indexing using [] with a smi index.
    Handle<String> str = Handle<String>::cast(lookup_start_obj);
    int smi_index = Handle<Smi>::cast(key_obj)->value();
    if (smi_index >= 0 && smi_index < str->length()) {
      Factory* factory = isolate->factory();
      return *factory->LookupSingleCharacterStringFromCode(
          String::Flatten(isolate, str)->Get(smi_index));
    }
  }

  // Fall back to GetObjectProperty.
  RETURN_RESULT_OR_FAILURE(
      isolate, Runtime::GetObjectProperty(isolate, lookup_start_obj, key_obj,
                                          receiver_obj));
}

RUNTIME_FUNCTION(Runtime_SetKeyedProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  Handle<Object> object = args.at(0);
  Handle<Object> key = args.at(1);
  Handle<Object> value = args.at(2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Runtime::SetObjectProperty(isolate, object, key, value,
                                          StoreOrigin::kMaybeKeyed));
}

RUNTIME_FUNCTION(Runtime_DefineObjectOwnProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  Handle<Object> object = args.at(0);
  Handle<Object> key = args.at(1);
  Handle<Object> value = args.at(2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Runtime::DefineObjectOwnProperty(isolate, object, key, value,
                                                StoreOrigin::kMaybeKeyed));
}

RUNTIME_FUNCTION(Runtime_SetNamedProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  Handle<Object> object = args.at(0);
  Handle<Object> key = args.at(1);
  Handle<Object> value = args.at(2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Runtime::SetObjectProperty(isolate, object, key, value,
                                          StoreOrigin::kNamed));
}

// Similar to DefineKeyedOwnPropertyInLiteral, but does not update feedback, and
// and does not have a flags parameter for performing SetFunctionName().
//
// Currently, this is used for ObjectLiteral spread properties in CloneObjectIC
// and for array literal creations in StoreInArrayLiteralIC.
// TODO(v8:12548): merge this into DefineKeyedOwnPropertyInLiteral.
RUNTIME_FUNCTION(Runtime_DefineKeyedOwnPropertyInLiteral_Simple) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  Handle<JSReceiver> object = args.at<JSReceiver>(0);
  Handle<Object> key = args.at(1);
  Handle<Object> value = args.at(2);

  PropertyKey lookup_key(isolate, key);
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
  Handle<Object> object = args.at(0);
  Handle<Object> key = args.at(1);
  int language_mode = args.smi_value_at(2);
  return DeleteProperty(isolate, object, key,
                        static_cast<LanguageMode>(language_mode));
}

RUNTIME_FUNCTION(Runtime_ShrinkNameDictionary) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<NameDictionary> dictionary = args.at<NameDictionary>(0);

  return *NameDictionary::Shrink(isolate, dictionary);
}

RUNTIME_FUNCTION(Runtime_ShrinkSwissNameDictionary) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<SwissNameDictionary> dictionary = args.at<SwissNameDictionary>(0);

  return *SwissNameDictionary::Shrink(isolate, dictionary);
}

// ES6 section 12.9.3, operator in.
RUNTIME_FUNCTION(Runtime_HasProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<Object> object = args.at(0);
  Handle<Object> key = args.at(1);

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
  Maybe<bool> maybe = JSReceiver::HasProperty(isolate, receiver, name);
  if (maybe.IsNothing()) return ReadOnlyRoots(isolate).exception();
  return isolate->heap()->ToBoolean(maybe.FromJust());
}

RUNTIME_FUNCTION(Runtime_GetOwnPropertyKeys) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<JSReceiver> object = args.at<JSReceiver>(0);
  int filter_value = args.smi_value_at(1);
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
  Handle<Object> object = args.at(0);
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
  Handle<JSFunction> target = args.at<JSFunction>(0);
  Handle<JSReceiver> new_target = args.at<JSReceiver>(1);
  RETURN_RESULT_OR_FAILURE(
      isolate,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
}

RUNTIME_FUNCTION(Runtime_GetDerivedMap) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<JSFunction> target = args.at<JSFunction>(0);
  Handle<JSReceiver> new_target = args.at<JSReceiver>(1);
  Handle<Object> rab_gsab = args.at(2);
  if (rab_gsab->IsTrue()) {
    return *JSFunction::GetDerivedRabGsabMap(isolate, target, new_target);
  } else {
    RETURN_RESULT_OR_FAILURE(
        isolate, JSFunction::GetDerivedMap(isolate, target, new_target));
  }
}

RUNTIME_FUNCTION(Runtime_CompleteInobjectSlackTrackingForMap) {
  DisallowGarbageCollection no_gc;
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  Handle<Map> initial_map = args.at<Map>(0);
  MapUpdater::CompleteInobjectSlackTracking(isolate, *initial_map);

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_TryMigrateInstance) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSObject> js_object = args.at<JSObject>(0);
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
  Handle<JSObject> obj = args.at<JSObject>(0);
  CHECK(!obj->IsNull(isolate));
  Handle<Name> name = args.at<Name>(1);
  Handle<Object> getter = args.at(2);
  CHECK(IsValidAccessor(isolate, getter));
  Handle<Object> setter = args.at(3);
  CHECK(IsValidAccessor(isolate, setter));
  auto attrs = PropertyAttributesFromInt(args.smi_value_at(4));

  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::DefineAccessor(obj, name, getter, setter, attrs));
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_DefineKeyedOwnPropertyInLiteral) {
  HandleScope scope(isolate);
  DCHECK_EQ(6, args.length());
  Handle<JSObject> object = args.at<JSObject>(0);
  Handle<Name> name = args.at<Name>(1);
  Handle<Object> value = args.at(2);
  int flag = args.smi_value_at(3);
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(4);
  int index = args.tagged_index_value_at(5);

  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    Handle<FeedbackVector> vector = Handle<FeedbackVector>::cast(maybe_vector);
    FeedbackNexus nexus(vector, FeedbackVector::ToSlot(index));
    if (nexus.ic_state() == InlineCacheState::UNINITIALIZED) {
      if (name->IsUniqueName()) {
        nexus.ConfigureMonomorphic(name, handle(object->map(), isolate),
                                   MaybeObjectHandle());
      } else {
        nexus.ConfigureMegamorphic(IcCheckType::kProperty);
      }
    } else if (nexus.ic_state() == InlineCacheState::MONOMORPHIC) {
      if (nexus.GetFirstMap() != object->map() || nexus.GetName() != *name) {
        nexus.ConfigureMegamorphic(IcCheckType::kProperty);
      }
    }
  }

  DefineKeyedOwnPropertyInLiteralFlags flags(flag);
  PropertyAttributes attrs =
      (flags & DefineKeyedOwnPropertyInLiteralFlag::kDontEnum)
          ? PropertyAttributes::DONT_ENUM
          : PropertyAttributes::NONE;

  if (flags & DefineKeyedOwnPropertyInLiteralFlag::kSetFunctionName) {
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

  PropertyKey key(isolate, name);
  LookupIterator it(isolate, object, key, object, LookupIterator::OWN);
  // Cannot fail since this should only be called when
  // creating an object literal.
  CHECK(JSObject::DefineOwnPropertyIgnoreAttributes(&it, value, attrs,
                                                    Just(kDontThrow))
            .IsJust());

  // Return the value so that
  // BaselineCompiler::VisitDefineKeyedOwnPropertyInLiteral doesn't have to
  // save the accumulator.
  return *value;
}

RUNTIME_FUNCTION(Runtime_CollectTypeProfile) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  int position = args.smi_value_at(0);
  Handle<Object> value = args.at(1);
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(2);

  if (maybe_vector->IsUndefined()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(2);

  Handle<String> type = Object::TypeOf(isolate, value);
  if (value->IsJSReceiver()) {
    Handle<JSReceiver> object = Handle<JSReceiver>::cast(value);
    type = JSReceiver::GetConstructorName(isolate, object);
  } else if (value->IsNull(isolate)) {
    // typeof(null) is object. But it's more user-friendly to annotate
    // null as type "null".
    type = Handle<String>(ReadOnlyRoots(isolate).null_string(), isolate);
  }

  DCHECK(vector->metadata().HasTypeProfileSlot());
  FeedbackNexus nexus(vector, vector->GetTypeProfileSlot());
  nexus.Collect(type, position);

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_HasFastPackedElements) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  auto obj = HeapObject::cast(args[0]);
  return isolate->heap()->ToBoolean(
      IsFastPackedElementsKind(obj.map().elements_kind()));
}

RUNTIME_FUNCTION(Runtime_IsJSReceiver) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  Object obj = args[0];
  return isolate->heap()->ToBoolean(obj.IsJSReceiver());
}

RUNTIME_FUNCTION(Runtime_GetFunctionName) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSFunction> function = args.at<JSFunction>(0);
  return *JSFunction::GetName(isolate, function);
}

RUNTIME_FUNCTION(Runtime_DefineGetterPropertyUnchecked) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  Handle<JSObject> object = args.at<JSObject>(0);
  Handle<Name> name = args.at<Name>(1);
  Handle<JSFunction> getter = args.at<JSFunction>(2);
  auto attrs = PropertyAttributesFromInt(args.smi_value_at(3));

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
  Handle<JSReceiver> target = args.at<JSReceiver>(0);
  Handle<Object> source = args.at(1);

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
  Handle<JSObject> target = args.at<JSObject>(0);
  Handle<Object> source = args.at(1);

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

namespace {

// Check that the excluded properties are within the stack range of the top of
// the stack, and the start of the JS frame.
void CheckExcludedPropertiesAreOnCallerStack(Isolate* isolate, Address base,
                                             int count) {
#ifdef DEBUG
  StackFrameIterator it(isolate);

  // Don't need to check when there's no excluded properties.
  if (count == 0) return;

  DCHECK(!it.done());

  // Properties are pass in order on the stack, which means that their addresses
  // are in reverse order in memory (because stacks grow backwards). So, we
  // need to check if the _last_ property address is before the stack end...
  Address last_property = base - (count - 1) * kSystemPointerSize;
  DCHECK_GE(last_property, it.frame()->sp());

  // ... and for the first JS frame, make sure the _first_ property address is
  // after that stack frame's start.
  for (; !it.done(); it.Advance()) {
    if (it.frame()->is_java_script()) {
      DCHECK_LT(base, it.frame()->fp());
      return;
    }
  }

  // We should always find a JS frame.
  UNREACHABLE();
#endif
}

}  // namespace

RUNTIME_FUNCTION(Runtime_CopyDataPropertiesWithExcludedPropertiesOnStack) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<Object> source = args.at(0);
  int excluded_property_count = args.smi_value_at(1);
  // The excluded_property_base is passed as a raw stack pointer. This is safe
  // because the stack pointer is aligned, so it looks like a Smi to the GC.
  Address* excluded_property_base = reinterpret_cast<Address*>(args[2].ptr());
  DCHECK(HAS_SMI_TAG(reinterpret_cast<intptr_t>(excluded_property_base)));
  // Also make sure that the given base pointer points to to on-stack values.
  CheckExcludedPropertiesAreOnCallerStack(
      isolate, reinterpret_cast<Address>(excluded_property_base),
      excluded_property_count);

  // If source is undefined or null, throw a non-coercible error.
  if (source->IsNullOrUndefined(isolate)) {
    return ErrorUtils::ThrowLoadFromNullOrUndefined(isolate, source,
                                                    MaybeHandle<Object>());
  }

  base::ScopedVector<Handle<Object>> excluded_properties(
      excluded_property_count);
  for (int i = 0; i < excluded_property_count; i++) {
    // Because the excluded properties on stack is from high address
    // to low address, so we need to use sub
    Handle<Object> property(excluded_property_base - i);
    uint32_t property_num;
    // We convert string to number if possible, in cases of computed
    // properties resolving to numbers, which would've been strings
    // instead because of our call to %ToName() in the desugaring for
    // computed properties.
    if (property->IsString() &&
        String::cast(*property).AsArrayIndex(&property_num)) {
      property = isolate->factory()->NewNumberFromUint(property_num);
    }

    excluded_properties[i] = property;
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
  Handle<JSObject> object = args.at<JSObject>(0);
  Handle<Name> name = args.at<Name>(1);
  Handle<JSFunction> setter = args.at<JSFunction>(2);
  auto attrs = PropertyAttributesFromInt(args.smi_value_at(3));

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
  Handle<Object> input = args.at(0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToNumber(isolate, input));
}

RUNTIME_FUNCTION(Runtime_ToNumeric) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<Object> input = args.at(0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToNumeric(isolate, input));
}

RUNTIME_FUNCTION(Runtime_ToLength) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<Object> input = args.at(0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToLength(isolate, input));
}

RUNTIME_FUNCTION(Runtime_ToString) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<Object> input = args.at(0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToString(isolate, input));
}

RUNTIME_FUNCTION(Runtime_ToName) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<Object> input = args.at(0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::ToName(isolate, input));
}

RUNTIME_FUNCTION(Runtime_HasInPrototypeChain) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<Object> object = args.at(0);
  Handle<Object> prototype = args.at(1);
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
  Handle<Object> value = args.at(0);
  Handle<Object> done = args.at(1);
  return *isolate->factory()->NewJSIteratorResult(value,
                                                  done->BooleanValue(isolate));
}

RUNTIME_FUNCTION(Runtime_CreateDataProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<JSReceiver> o = args.at<JSReceiver>(0);
  Handle<Object> key = args.at(1);
  Handle<Object> value = args.at(2);
  bool success;
  PropertyKey lookup_key(isolate, key, &success);
  if (!success) return ReadOnlyRoots(isolate).exception();
  LookupIterator it(isolate, o, lookup_key, LookupIterator::OWN);
  MAYBE_RETURN(JSReceiver::CreateDataProperty(&it, value, Just(kThrowOnError)),
               ReadOnlyRoots(isolate).exception());
  return *value;
}

RUNTIME_FUNCTION(Runtime_SetOwnPropertyIgnoreAttributes) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  Handle<JSObject> o = args.at<JSObject>(0);
  Handle<String> key = args.at<String>(1);
  Handle<Object> value = args.at(2);
  int attributes = args.smi_value_at(3);

  RETURN_RESULT_OR_FAILURE(isolate,
                           JSObject::SetOwnPropertyIgnoreAttributes(
                               o, key, value, PropertyAttributes(attributes)));
}

RUNTIME_FUNCTION(Runtime_GetOwnPropertyDescriptor) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());
  Handle<JSReceiver> object = args.at<JSReceiver>(0);
  Handle<Name> name = args.at<Name>(1);

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
  Handle<AccessorPair> pair = args.at<AccessorPair>(0);
  DCHECK(pair->setter().IsJSFunction());
  return pair->setter();
}

RUNTIME_FUNCTION(Runtime_LoadPrivateGetter) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  Handle<AccessorPair> pair = args.at<AccessorPair>(0);
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

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableAllocate) {
  HandleScope scope(isolate);
  int at_least_space_for = args.smi_value_at(0);

  return *isolate->factory()->NewSwissNameDictionary(at_least_space_for,
                                                     AllocationType::kYoung);
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableAdd) {
  HandleScope scope(isolate);
  Handle<SwissNameDictionary> table = args.at<SwissNameDictionary>(0);
  Handle<Name> key = args.at<Name>(1);
  Handle<Object> value = args.at(2);
  PropertyDetails details(Smi::cast(args[3]));

  DCHECK(key->IsUniqueName());

  return *SwissNameDictionary::Add(isolate, table, key, value, details);
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableFindEntry) {
  HandleScope scope(isolate);
  DisallowGarbageCollection no_gc;
  auto table = SwissNameDictionary::cast(args[0]);
  Name key = Name::cast(args[1]);
  InternalIndex index = table.FindEntry(isolate, key);
  return Smi::FromInt(index.is_found()
                          ? index.as_int()
                          : SwissNameDictionary::kNotFoundSentinel);
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableUpdate) {
  HandleScope scope(isolate);
  DisallowGarbageCollection no_gc;
  auto table = SwissNameDictionary::cast(args[0]);
  InternalIndex index(args.smi_value_at(1));
  Object value = args[2];
  table.ValueAtPut(index, value);

  PropertyDetails details(Smi::cast(args[3]));
  table.DetailsAtPut(index, details);

  return ReadOnlyRoots(isolate).undefined_value();
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableDelete) {
  HandleScope scope(isolate);
  Handle<SwissNameDictionary> table = args.at<SwissNameDictionary>(0);
  InternalIndex index(args.smi_value_at(1));

  return *SwissNameDictionary::DeleteEntry(isolate, table, index);
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableEquals) {
  HandleScope scope(isolate);
  DisallowGarbageCollection no_gc;
  auto table = SwissNameDictionary::cast(args[0]);
  auto other = SwissNameDictionary::cast(args[0]);
  return Smi::FromInt(table.EqualsForTesting(other));
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableElementsCount) {
  HandleScope scope(isolate);
  DisallowGarbageCollection no_gc;
  auto table = SwissNameDictionary::cast(args[0]);
  return Smi::FromInt(table.NumberOfElements());
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableKeyAt) {
  HandleScope scope(isolate);
  DisallowGarbageCollection no_gc;
  auto table = SwissNameDictionary::cast(args[0]);
  InternalIndex index(args.smi_value_at(1));
  return table.KeyAt(index);
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableValueAt) {
  HandleScope scope(isolate);
  DisallowGarbageCollection no_gc;
  auto table = SwissNameDictionary::cast(args[0]);
  InternalIndex index(args.smi_value_at(1));
  return table.ValueAt(index);
}

// TODO(v8:11330) This is only here while the CSA/Torque implementaton of
// SwissNameDictionary is work in progress.
RUNTIME_FUNCTION(Runtime_SwissTableDetailsAt) {
  HandleScope scope(isolate);
  DisallowGarbageCollection no_gc;
  auto table = SwissNameDictionary::cast(args[0]);
  InternalIndex index(args.smi_value_at(1));
  PropertyDetails d = table.DetailsAt(index);
  return d.AsSmi();
}

}  // namespace internal
}  // namespace v8
