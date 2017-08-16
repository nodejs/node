// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/keys.h"

#include "src/api-arguments.h"
#include "src/elements.h"
#include "src/factory.h"
#include "src/identity-map.h"
#include "src/isolate-inl.h"
#include "src/objects-inl.h"
#include "src/property-descriptor.h"
#include "src/prototype.h"

namespace v8 {
namespace internal {

KeyAccumulator::~KeyAccumulator() {
}

namespace {

static bool ContainsOnlyValidKeys(Handle<FixedArray> array) {
  int len = array->length();
  for (int i = 0; i < len; i++) {
    Object* e = array->get(i);
    if (!(e->IsName() || e->IsNumber())) return false;
  }
  return true;
}

}  // namespace

// static
MaybeHandle<FixedArray> KeyAccumulator::GetKeys(
    Handle<JSReceiver> object, KeyCollectionMode mode, PropertyFilter filter,
    GetKeysConversion keys_conversion, bool is_for_in) {
  Isolate* isolate = object->GetIsolate();
  FastKeyAccumulator accumulator(isolate, object, mode, filter);
  accumulator.set_is_for_in(is_for_in);
  return accumulator.GetKeys(keys_conversion);
}

Handle<FixedArray> KeyAccumulator::GetKeys(GetKeysConversion convert) {
  if (keys_.is_null()) {
    return isolate_->factory()->empty_fixed_array();
  }
  if (mode_ == KeyCollectionMode::kOwnOnly &&
      keys_->map() == isolate_->heap()->fixed_array_map()) {
    return Handle<FixedArray>::cast(keys_);
  }
  USE(ContainsOnlyValidKeys);
  Handle<FixedArray> result =
      OrderedHashSet::ConvertToKeysArray(keys(), convert);
  DCHECK(ContainsOnlyValidKeys(result));
  return result;
}

void KeyAccumulator::AddKey(Object* key, AddKeyConversion convert) {
  AddKey(handle(key, isolate_), convert);
}

void KeyAccumulator::AddKey(Handle<Object> key, AddKeyConversion convert) {
  if (key->IsSymbol()) {
    if (filter_ & SKIP_SYMBOLS) return;
    if (Handle<Symbol>::cast(key)->is_private()) return;
  } else if (filter_ & SKIP_STRINGS) {
    return;
  }
  if (IsShadowed(key)) return;
  if (keys_.is_null()) {
    keys_ = OrderedHashSet::Allocate(isolate_, 16);
  }
  uint32_t index;
  if (convert == CONVERT_TO_ARRAY_INDEX && key->IsString() &&
      Handle<String>::cast(key)->AsArrayIndex(&index)) {
    key = isolate_->factory()->NewNumberFromUint(index);
  }
  keys_ = OrderedHashSet::Add(keys(), key);
}

void KeyAccumulator::AddKeys(Handle<FixedArray> array,
                             AddKeyConversion convert) {
  int add_length = array->length();
  for (int i = 0; i < add_length; i++) {
    Handle<Object> current(array->get(i), isolate_);
    AddKey(current, convert);
  }
}

void KeyAccumulator::AddKeys(Handle<JSObject> array_like,
                             AddKeyConversion convert) {
  DCHECK(array_like->IsJSArray() || array_like->HasSloppyArgumentsElements());
  ElementsAccessor* accessor = array_like->GetElementsAccessor();
  accessor->AddElementsToKeyAccumulator(array_like, this, convert);
}

MaybeHandle<FixedArray> FilterProxyKeys(KeyAccumulator* accumulator,
                                        Handle<JSProxy> owner,
                                        Handle<FixedArray> keys,
                                        PropertyFilter filter) {
  if (filter == ALL_PROPERTIES) {
    // Nothing to do.
    return keys;
  }
  Isolate* isolate = accumulator->isolate();
  int store_position = 0;
  for (int i = 0; i < keys->length(); ++i) {
    Handle<Name> key(Name::cast(keys->get(i)), isolate);
    if (key->FilterKey(filter)) continue;  // Skip this key.
    if (filter & ONLY_ENUMERABLE) {
      PropertyDescriptor desc;
      Maybe<bool> found =
          JSProxy::GetOwnPropertyDescriptor(isolate, owner, key, &desc);
      MAYBE_RETURN(found, MaybeHandle<FixedArray>());
      if (!found.FromJust()) continue;
      if (!desc.enumerable()) {
        accumulator->AddShadowingKey(key);
        continue;
      }
    }
    // Keep this key.
    if (store_position != i) {
      keys->set(store_position, *key);
    }
    store_position++;
  }
  if (store_position == 0) return isolate->factory()->empty_fixed_array();
  keys->Shrink(store_position);
  return keys;
}

// Returns "nothing" in case of exception, "true" on success.
Maybe<bool> KeyAccumulator::AddKeysFromJSProxy(Handle<JSProxy> proxy,
                                               Handle<FixedArray> keys) {
  // Postpone the enumerable check for for-in to the ForInFilter step.
  if (!is_for_in_) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, keys, FilterProxyKeys(this, proxy, keys, filter_),
        Nothing<bool>());
    if (mode_ == KeyCollectionMode::kOwnOnly) {
      // If we collect only the keys from a JSProxy do not sort or deduplicate.
      keys_ = keys;
      return Just(true);
    }
  }
  AddKeys(keys, is_for_in_ ? CONVERT_TO_ARRAY_INDEX : DO_NOT_CONVERT);
  return Just(true);
}

Maybe<bool> KeyAccumulator::CollectKeys(Handle<JSReceiver> receiver,
                                        Handle<JSReceiver> object) {
  // Proxies have no hidden prototype and we should not trigger the
  // [[GetPrototypeOf]] trap on the last iteration when using
  // AdvanceFollowingProxies.
  if (mode_ == KeyCollectionMode::kOwnOnly && object->IsJSProxy()) {
    MAYBE_RETURN(CollectOwnJSProxyKeys(receiver, Handle<JSProxy>::cast(object)),
                 Nothing<bool>());
    return Just(true);
  }

  PrototypeIterator::WhereToEnd end = mode_ == KeyCollectionMode::kOwnOnly
                                          ? PrototypeIterator::END_AT_NON_HIDDEN
                                          : PrototypeIterator::END_AT_NULL;
  for (PrototypeIterator iter(isolate_, object, kStartAtReceiver, end);
       !iter.IsAtEnd();) {
    // Start the shadow checks only after the first prototype has added
    // shadowing keys.
    if (HasShadowingKeys()) skip_shadow_check_ = false;
    Handle<JSReceiver> current =
        PrototypeIterator::GetCurrent<JSReceiver>(iter);
    Maybe<bool> result = Just(false);  // Dummy initialization.
    if (current->IsJSProxy()) {
      result = CollectOwnJSProxyKeys(receiver, Handle<JSProxy>::cast(current));
    } else {
      DCHECK(current->IsJSObject());
      result = CollectOwnKeys(receiver, Handle<JSObject>::cast(current));
    }
    MAYBE_RETURN(result, Nothing<bool>());
    if (!result.FromJust()) break;  // |false| means "stop iterating".
    // Iterate through proxies but ignore access checks for the ALL_CAN_READ
    // case on API objects for OWN_ONLY keys handled in CollectOwnKeys.
    if (!iter.AdvanceFollowingProxiesIgnoringAccessChecks()) {
      return Nothing<bool>();
    }
    if (!last_non_empty_prototype_.is_null() &&
        *last_non_empty_prototype_ == *current) {
      break;
    }
  }
  return Just(true);
}

bool KeyAccumulator::HasShadowingKeys() { return !shadowing_keys_.is_null(); }

bool KeyAccumulator::IsShadowed(Handle<Object> key) {
  if (!HasShadowingKeys() || skip_shadow_check_) return false;
  return shadowing_keys_->Has(isolate_, key);
}

void KeyAccumulator::AddShadowingKey(Object* key) {
  if (mode_ == KeyCollectionMode::kOwnOnly) return;
  AddShadowingKey(handle(key, isolate_));
}
void KeyAccumulator::AddShadowingKey(Handle<Object> key) {
  if (mode_ == KeyCollectionMode::kOwnOnly) return;
  if (shadowing_keys_.is_null()) {
    shadowing_keys_ = ObjectHashSet::New(isolate_, 16);
  }
  shadowing_keys_ = ObjectHashSet::Add(shadowing_keys_, key);
}

namespace {

void TrySettingEmptyEnumCache(JSReceiver* object) {
  Map* map = object->map();
  DCHECK_EQ(kInvalidEnumCacheSentinel, map->EnumLength());
  if (!map->OnlyHasSimpleProperties()) return;
  if (map->IsJSProxyMap()) return;
  if (map->NumberOfOwnDescriptors() > 0) {
    int number_of_enumerable_own_properties =
        map->NumberOfDescribedProperties(OWN_DESCRIPTORS, ENUMERABLE_STRINGS);
    if (number_of_enumerable_own_properties > 0) return;
  }
  DCHECK(object->IsJSObject());
  map->SetEnumLength(0);
}

bool CheckAndInitalizeEmptyEnumCache(JSReceiver* object) {
  if (object->map()->EnumLength() == kInvalidEnumCacheSentinel) {
    TrySettingEmptyEnumCache(object);
  }
  if (object->map()->EnumLength() != 0) return false;
  DCHECK(object->IsJSObject());
  return !JSObject::cast(object)->HasEnumerableElements();
}
}  // namespace

void FastKeyAccumulator::Prepare() {
  DisallowHeapAllocation no_gc;
  // Directly go for the fast path for OWN_ONLY keys.
  if (mode_ == KeyCollectionMode::kOwnOnly) return;
  // Fully walk the prototype chain and find the last prototype with keys.
  is_receiver_simple_enum_ = false;
  has_empty_prototype_ = true;
  JSReceiver* last_prototype = nullptr;
  for (PrototypeIterator iter(isolate_, *receiver_); !iter.IsAtEnd();
       iter.Advance()) {
    JSReceiver* current = iter.GetCurrent<JSReceiver>();
    bool has_no_properties = CheckAndInitalizeEmptyEnumCache(current);
    if (has_no_properties) continue;
    last_prototype = current;
    has_empty_prototype_ = false;
  }
  if (has_empty_prototype_) {
    is_receiver_simple_enum_ =
        receiver_->map()->EnumLength() != kInvalidEnumCacheSentinel &&
        !JSObject::cast(*receiver_)->HasEnumerableElements();
  } else if (last_prototype != nullptr) {
    last_non_empty_prototype_ = handle(last_prototype, isolate_);
  }
}

namespace {
static Handle<FixedArray> ReduceFixedArrayTo(Isolate* isolate,
                                             Handle<FixedArray> array,
                                             int length) {
  DCHECK_LE(length, array->length());
  if (array->length() == length) return array;
  return isolate->factory()->CopyFixedArrayUpTo(array, length);
}

// Initializes and directly returns the enume cache. Users of this function
// have to make sure to never directly leak the enum cache.
Handle<FixedArray> GetFastEnumPropertyKeys(Isolate* isolate,
                                           Handle<JSObject> object) {
  Handle<Map> map(object->map());
  bool cache_enum_length = map->OnlyHasSimpleProperties();

  Handle<DescriptorArray> descs =
      Handle<DescriptorArray>(map->instance_descriptors(), isolate);
  int own_property_count = map->EnumLength();
  // If the enum length of the given map is set to kInvalidEnumCache, this
  // means that the map itself has never used the present enum cache. The
  // first step to using the cache is to set the enum length of the map by
  // counting the number of own descriptors that are ENUMERABLE_STRINGS.
  if (own_property_count == kInvalidEnumCacheSentinel) {
    own_property_count =
        map->NumberOfDescribedProperties(OWN_DESCRIPTORS, ENUMERABLE_STRINGS);
  } else {
    DCHECK(
        own_property_count ==
        map->NumberOfDescribedProperties(OWN_DESCRIPTORS, ENUMERABLE_STRINGS));
  }

  if (descs->HasEnumCache()) {
    Handle<FixedArray> keys(descs->GetEnumCache(), isolate);
    // In case the number of properties required in the enum are actually
    // present, we can reuse the enum cache. Otherwise, this means that the
    // enum cache was generated for a previous (smaller) version of the
    // Descriptor Array. In that case we regenerate the enum cache.
    if (own_property_count <= keys->length()) {
      isolate->counters()->enum_cache_hits()->Increment();
      if (cache_enum_length) map->SetEnumLength(own_property_count);
      return ReduceFixedArrayTo(isolate, keys, own_property_count);
    }
  }

  if (descs->IsEmpty()) {
    isolate->counters()->enum_cache_hits()->Increment();
    if (cache_enum_length) map->SetEnumLength(0);
    return isolate->factory()->empty_fixed_array();
  }

  isolate->counters()->enum_cache_misses()->Increment();

  Handle<FixedArray> storage =
      isolate->factory()->NewFixedArray(own_property_count);
  Handle<FixedArray> indices =
      isolate->factory()->NewFixedArray(own_property_count);

  int size = map->NumberOfOwnDescriptors();
  int index = 0;

  for (int i = 0; i < size; i++) {
    PropertyDetails details = descs->GetDetails(i);
    if (details.IsDontEnum()) continue;
    Object* key = descs->GetKey(i);
    if (key->IsSymbol()) continue;
    storage->set(index, key);
    if (!indices.is_null()) {
      if (details.location() == kField) {
        DCHECK_EQ(kData, details.kind());
        FieldIndex field_index = FieldIndex::ForDescriptor(*map, i);
        int load_by_field_index = field_index.GetLoadByFieldIndex();
        indices->set(index, Smi::FromInt(load_by_field_index));
      } else {
        indices = Handle<FixedArray>();
      }
    }
    index++;
  }
  DCHECK(index == storage->length());

  DescriptorArray::SetEnumCache(descs, isolate, storage, indices);
  if (cache_enum_length) {
    map->SetEnumLength(own_property_count);
  }
  return storage;
}

template <bool fast_properties>
MaybeHandle<FixedArray> GetOwnKeysWithElements(Isolate* isolate,
                                               Handle<JSObject> object,
                                               GetKeysConversion convert) {
  Handle<FixedArray> keys;
  ElementsAccessor* accessor = object->GetElementsAccessor();
  if (fast_properties) {
    keys = GetFastEnumPropertyKeys(isolate, object);
  } else {
    // TODO(cbruni): preallocate big enough array to also hold elements.
    keys = KeyAccumulator::GetOwnEnumPropertyKeys(isolate, object);
  }
  MaybeHandle<FixedArray> result =
      accessor->PrependElementIndices(object, keys, convert, ONLY_ENUMERABLE);

  if (FLAG_trace_for_in_enumerate) {
    PrintF("| strings=%d symbols=0 elements=%u || prototypes>=1 ||\n",
           keys->length(), result.ToHandleChecked()->length() - keys->length());
  }
  return result;
}

bool OnlyHasSimpleProperties(Map* map) {
  return map->instance_type() > LAST_CUSTOM_ELEMENTS_RECEIVER;
}

}  // namespace

MaybeHandle<FixedArray> FastKeyAccumulator::GetKeys(
    GetKeysConversion keys_conversion) {
  if (filter_ == ENUMERABLE_STRINGS) {
    Handle<FixedArray> keys;
    if (GetKeysFast(keys_conversion).ToHandle(&keys)) {
      return keys;
    }
    if (isolate_->has_pending_exception()) return MaybeHandle<FixedArray>();
  }

  return GetKeysSlow(keys_conversion);
}

MaybeHandle<FixedArray> FastKeyAccumulator::GetKeysFast(
    GetKeysConversion keys_conversion) {
  bool own_only = has_empty_prototype_ || mode_ == KeyCollectionMode::kOwnOnly;
  Map* map = receiver_->map();
  if (!own_only || !OnlyHasSimpleProperties(map)) {
    return MaybeHandle<FixedArray>();
  }

  // From this point on we are certiain to only collect own keys.
  DCHECK(receiver_->IsJSObject());
  Handle<JSObject> object = Handle<JSObject>::cast(receiver_);

  // Do not try to use the enum-cache for dict-mode objects.
  if (map->is_dictionary_map()) {
    return GetOwnKeysWithElements<false>(isolate_, object, keys_conversion);
  }
  int enum_length = receiver_->map()->EnumLength();
  if (enum_length == kInvalidEnumCacheSentinel) {
    Handle<FixedArray> keys;
    // Try initializing the enum cache and return own properties.
    if (GetOwnKeysWithUninitializedEnumCache().ToHandle(&keys)) {
      if (FLAG_trace_for_in_enumerate) {
        PrintF("| strings=%d symbols=0 elements=0 || prototypes>=1 ||\n",
               keys->length());
      }
      is_receiver_simple_enum_ =
          object->map()->EnumLength() != kInvalidEnumCacheSentinel;
      return keys;
    }
  }
  // The properties-only case failed because there were probably elements on the
  // receiver.
  return GetOwnKeysWithElements<true>(isolate_, object, keys_conversion);
}

MaybeHandle<FixedArray>
FastKeyAccumulator::GetOwnKeysWithUninitializedEnumCache() {
  Handle<JSObject> object = Handle<JSObject>::cast(receiver_);
  // Uninitalized enum cache
  Map* map = object->map();
  if (object->elements()->length() != 0) {
    // Assume that there are elements.
    return MaybeHandle<FixedArray>();
  }
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  if (number_of_own_descriptors == 0) {
    map->SetEnumLength(0);
    return isolate_->factory()->empty_fixed_array();
  }
  // We have no elements but possibly enumerable property keys, hence we can
  // directly initialize the enum cache.
  Handle<FixedArray> keys = GetFastEnumPropertyKeys(isolate_, object);
  if (is_for_in_) return keys;
  // Do not leak the enum cache as it might end up as an elements backing store.
  return isolate_->factory()->CopyFixedArray(keys);
}

MaybeHandle<FixedArray> FastKeyAccumulator::GetKeysSlow(
    GetKeysConversion keys_conversion) {
  KeyAccumulator accumulator(isolate_, mode_, filter_);
  accumulator.set_is_for_in(is_for_in_);
  accumulator.set_last_non_empty_prototype(last_non_empty_prototype_);

  MAYBE_RETURN(accumulator.CollectKeys(receiver_, receiver_),
               MaybeHandle<FixedArray>());
  return accumulator.GetKeys(keys_conversion);
}

namespace {

enum IndexedOrNamed { kIndexed, kNamed };

// Returns |true| on success, |nothing| on exception.
template <class Callback, IndexedOrNamed type>
Maybe<bool> CollectInterceptorKeysInternal(Handle<JSReceiver> receiver,
                                           Handle<JSObject> object,
                                           Handle<InterceptorInfo> interceptor,
                                           KeyAccumulator* accumulator) {
  Isolate* isolate = accumulator->isolate();
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *object, Object::DONT_THROW);
  Handle<JSObject> result;
  if (!interceptor->enumerator()->IsUndefined(isolate)) {
    Callback enum_fun = v8::ToCData<Callback>(interceptor->enumerator());
    const char* log_tag = type == kIndexed ? "interceptor-indexed-enum"
                                           : "interceptor-named-enum";
    LOG(isolate, ApiObjectAccess(log_tag, *object));
    result = args.Call(enum_fun);
  }
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
  if (result.is_null()) return Just(true);
  accumulator->AddKeys(
      result, type == kIndexed ? CONVERT_TO_ARRAY_INDEX : DO_NOT_CONVERT);
  return Just(true);
}

template <class Callback, IndexedOrNamed type>
Maybe<bool> CollectInterceptorKeys(Handle<JSReceiver> receiver,
                                   Handle<JSObject> object,
                                   KeyAccumulator* accumulator) {
  Isolate* isolate = accumulator->isolate();
  if (type == kIndexed) {
    if (!object->HasIndexedInterceptor()) return Just(true);
  } else {
    if (!object->HasNamedInterceptor()) return Just(true);
  }
  Handle<InterceptorInfo> interceptor(type == kIndexed
                                          ? object->GetIndexedInterceptor()
                                          : object->GetNamedInterceptor(),
                                      isolate);
  if ((accumulator->filter() & ONLY_ALL_CAN_READ) &&
      !interceptor->all_can_read()) {
    return Just(true);
  }
  return CollectInterceptorKeysInternal<Callback, type>(
      receiver, object, interceptor, accumulator);
}

}  // namespace

Maybe<bool> KeyAccumulator::CollectOwnElementIndices(
    Handle<JSReceiver> receiver, Handle<JSObject> object) {
  if (filter_ & SKIP_STRINGS || skip_indices_) return Just(true);

  ElementsAccessor* accessor = object->GetElementsAccessor();
  accessor->CollectElementIndices(object, this);

  return CollectInterceptorKeys<v8::IndexedPropertyEnumeratorCallback,
                                kIndexed>(receiver, object, this);
}

namespace {

template <bool skip_symbols>
int CollectOwnPropertyNamesInternal(Handle<JSObject> object,
                                    KeyAccumulator* keys,
                                    Handle<DescriptorArray> descs,
                                    int start_index, int limit) {
  int first_skipped = -1;
  PropertyFilter filter = keys->filter();
  KeyCollectionMode mode = keys->mode();
  for (int i = start_index; i < limit; i++) {
    bool is_shadowing_key = false;
    PropertyDetails details = descs->GetDetails(i);

    if ((details.attributes() & filter) != 0) {
      if (mode == KeyCollectionMode::kIncludePrototypes) {
        is_shadowing_key = true;
      } else {
        continue;
      }
    }

    if (filter & ONLY_ALL_CAN_READ) {
      if (details.kind() != kAccessor) continue;
      Object* accessors = descs->GetValue(i);
      if (!accessors->IsAccessorInfo()) continue;
      if (!AccessorInfo::cast(accessors)->all_can_read()) continue;
    }

    Name* key = descs->GetKey(i);
    if (skip_symbols == key->IsSymbol()) {
      if (first_skipped == -1) first_skipped = i;
      continue;
    }
    if (key->FilterKey(keys->filter())) continue;

    if (is_shadowing_key) {
      keys->AddShadowingKey(key);
    } else {
      keys->AddKey(key, DO_NOT_CONVERT);
    }
  }
  return first_skipped;
}

template <class T>
Handle<FixedArray> GetOwnEnumPropertyDictionaryKeys(Isolate* isolate,
                                                    KeyCollectionMode mode,
                                                    KeyAccumulator* accumulator,
                                                    Handle<JSObject> object,
                                                    T* raw_dictionary) {
  Handle<T> dictionary(raw_dictionary, isolate);
  int length = dictionary->NumberOfEnumElements();
  if (length == 0) {
    return isolate->factory()->empty_fixed_array();
  }
  Handle<FixedArray> storage = isolate->factory()->NewFixedArray(length);
  T::CopyEnumKeysTo(dictionary, storage, mode, accumulator);
  return storage;
}
}  // namespace

Maybe<bool> KeyAccumulator::CollectOwnPropertyNames(Handle<JSReceiver> receiver,
                                                    Handle<JSObject> object) {
  if (filter_ == ENUMERABLE_STRINGS) {
    Handle<FixedArray> enum_keys;
    if (object->HasFastProperties()) {
      enum_keys = KeyAccumulator::GetOwnEnumPropertyKeys(isolate_, object);
      // If the number of properties equals the length of enumerable properties
      // we do not have to filter out non-enumerable ones
      Map* map = object->map();
      int nof_descriptors = map->NumberOfOwnDescriptors();
      if (enum_keys->length() != nof_descriptors) {
        Handle<DescriptorArray> descs =
            Handle<DescriptorArray>(map->instance_descriptors(), isolate_);
        for (int i = 0; i < nof_descriptors; i++) {
          PropertyDetails details = descs->GetDetails(i);
          if (!details.IsDontEnum()) continue;
          Object* key = descs->GetKey(i);
          this->AddShadowingKey(key);
        }
      }
    } else if (object->IsJSGlobalObject()) {
      enum_keys = GetOwnEnumPropertyDictionaryKeys(
          isolate_, mode_, this, object, object->global_dictionary());
    } else {
      enum_keys = GetOwnEnumPropertyDictionaryKeys(
          isolate_, mode_, this, object, object->property_dictionary());
    }
    AddKeys(enum_keys, DO_NOT_CONVERT);
  } else {
    if (object->HasFastProperties()) {
      int limit = object->map()->NumberOfOwnDescriptors();
      Handle<DescriptorArray> descs(object->map()->instance_descriptors(),
                                    isolate_);
      // First collect the strings,
      int first_symbol =
          CollectOwnPropertyNamesInternal<true>(object, this, descs, 0, limit);
      // then the symbols.
      if (first_symbol != -1) {
        CollectOwnPropertyNamesInternal<false>(object, this, descs,
                                               first_symbol, limit);
      }
    } else if (object->IsJSGlobalObject()) {
      GlobalDictionary::CollectKeysTo(
          handle(object->global_dictionary(), isolate_), this);
    } else {
      NameDictionary::CollectKeysTo(
          handle(object->property_dictionary(), isolate_), this);
    }
  }
  // Add the property keys from the interceptor.
  return CollectInterceptorKeys<v8::GenericNamedPropertyEnumeratorCallback,
                                kNamed>(receiver, object, this);
}

Maybe<bool> KeyAccumulator::CollectAccessCheckInterceptorKeys(
    Handle<AccessCheckInfo> access_check_info, Handle<JSReceiver> receiver,
    Handle<JSObject> object) {
  MAYBE_RETURN(
      (CollectInterceptorKeysInternal<v8::IndexedPropertyEnumeratorCallback,
                                      kIndexed>(
          receiver, object,
          handle(
              InterceptorInfo::cast(access_check_info->indexed_interceptor()),
              isolate_),
          this)),
      Nothing<bool>());
  MAYBE_RETURN(
      (CollectInterceptorKeysInternal<
          v8::GenericNamedPropertyEnumeratorCallback, kNamed>(
          receiver, object,
          handle(InterceptorInfo::cast(access_check_info->named_interceptor()),
                 isolate_),
          this)),
      Nothing<bool>());
  return Just(true);
}

// Returns |true| on success, |false| if prototype walking should be stopped,
// |nothing| if an exception was thrown.
Maybe<bool> KeyAccumulator::CollectOwnKeys(Handle<JSReceiver> receiver,
                                           Handle<JSObject> object) {
  // Check access rights if required.
  if (object->IsAccessCheckNeeded() &&
      !isolate_->MayAccess(handle(isolate_->context()), object)) {
    // The cross-origin spec says that [[Enumerate]] shall return an empty
    // iterator when it doesn't have access...
    if (mode_ == KeyCollectionMode::kIncludePrototypes) {
      return Just(false);
    }
    // ...whereas [[OwnPropertyKeys]] shall return whitelisted properties.
    DCHECK(KeyCollectionMode::kOwnOnly == mode_);
    Handle<AccessCheckInfo> access_check_info;
    {
      DisallowHeapAllocation no_gc;
      AccessCheckInfo* maybe_info = AccessCheckInfo::Get(isolate_, object);
      if (maybe_info) access_check_info = handle(maybe_info, isolate_);
    }
    // We always have both kinds of interceptors or none.
    if (!access_check_info.is_null() &&
        access_check_info->named_interceptor()) {
      MAYBE_RETURN(CollectAccessCheckInterceptorKeys(access_check_info,
                                                     receiver, object),
                   Nothing<bool>());
      return Just(false);
    }
    filter_ = static_cast<PropertyFilter>(filter_ | ONLY_ALL_CAN_READ);
  }
  MAYBE_RETURN(CollectOwnElementIndices(receiver, object), Nothing<bool>());
  MAYBE_RETURN(CollectOwnPropertyNames(receiver, object), Nothing<bool>());
  return Just(true);
}

// static
Handle<FixedArray> KeyAccumulator::GetOwnEnumPropertyKeys(
    Isolate* isolate, Handle<JSObject> object) {
  if (object->HasFastProperties()) {
    return GetFastEnumPropertyKeys(isolate, object);
  } else if (object->IsJSGlobalObject()) {
    return GetOwnEnumPropertyDictionaryKeys(
        isolate, KeyCollectionMode::kOwnOnly, nullptr, object,
        object->global_dictionary());
  } else {
    return GetOwnEnumPropertyDictionaryKeys(
        isolate, KeyCollectionMode::kOwnOnly, nullptr, object,
        object->property_dictionary());
  }
}

namespace {

struct NameComparator {
  bool operator()(uint32_t hash1, uint32_t hash2, const Handle<Name>& key1,
                  const Handle<Name>& key2) const {
    return Name::Equals(key1, key2);
  }
};

}  // namespace

// ES6 9.5.12
// Returns |true| on success, |nothing| in case of exception.
Maybe<bool> KeyAccumulator::CollectOwnJSProxyKeys(Handle<JSReceiver> receiver,
                                                  Handle<JSProxy> proxy) {
  STACK_CHECK(isolate_, Nothing<bool>());
  // 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate_);
  // 2. If handler is null, throw a TypeError exception.
  // 3. Assert: Type(handler) is Object.
  if (proxy->IsRevoked()) {
    isolate_->Throw(*isolate_->factory()->NewTypeError(
        MessageTemplate::kProxyRevoked, isolate_->factory()->ownKeys_string()));
    return Nothing<bool>();
  }
  // 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Handle<JSReceiver> target(proxy->target(), isolate_);
  // 5. Let trap be ? GetMethod(handler, "ownKeys").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate_, trap, Object::GetMethod(Handle<JSReceiver>::cast(handler),
                                        isolate_->factory()->ownKeys_string()),
      Nothing<bool>());
  // 6. If trap is undefined, then
  if (trap->IsUndefined(isolate_)) {
    // 6a. Return target.[[OwnPropertyKeys]]().
    return CollectOwnJSProxyTargetKeys(proxy, target);
  }
  // 7. Let trapResultArray be Call(trap, handler, «target»).
  Handle<Object> trap_result_array;
  Handle<Object> args[] = {target};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate_, trap_result_array,
      Execution::Call(isolate_, trap, handler, arraysize(args), args),
      Nothing<bool>());
  // 8. Let trapResult be ? CreateListFromArrayLike(trapResultArray,
  //    «String, Symbol»).
  Handle<FixedArray> trap_result;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate_, trap_result,
      Object::CreateListFromArrayLike(isolate_, trap_result_array,
                                      ElementTypes::kStringAndSymbol),
      Nothing<bool>());
  // 9. Let extensibleTarget be ? IsExtensible(target).
  Maybe<bool> maybe_extensible = JSReceiver::IsExtensible(target);
  MAYBE_RETURN(maybe_extensible, Nothing<bool>());
  bool extensible_target = maybe_extensible.FromJust();
  // 10. Let targetKeys be ? target.[[OwnPropertyKeys]]().
  Handle<FixedArray> target_keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate_, target_keys,
                                   JSReceiver::OwnPropertyKeys(target),
                                   Nothing<bool>());
  // 11. (Assert)
  // 12. Let targetConfigurableKeys be an empty List.
  // To save memory, we're re-using target_keys and will modify it in-place.
  Handle<FixedArray> target_configurable_keys = target_keys;
  // 13. Let targetNonconfigurableKeys be an empty List.
  Handle<FixedArray> target_nonconfigurable_keys =
      isolate_->factory()->NewFixedArray(target_keys->length());
  int nonconfigurable_keys_length = 0;
  // 14. Repeat, for each element key of targetKeys:
  for (int i = 0; i < target_keys->length(); ++i) {
    // 14a. Let desc be ? target.[[GetOwnProperty]](key).
    PropertyDescriptor desc;
    Maybe<bool> found = JSReceiver::GetOwnPropertyDescriptor(
        isolate_, target, handle(target_keys->get(i), isolate_), &desc);
    MAYBE_RETURN(found, Nothing<bool>());
    // 14b. If desc is not undefined and desc.[[Configurable]] is false, then
    if (found.FromJust() && !desc.configurable()) {
      // 14b i. Append key as an element of targetNonconfigurableKeys.
      target_nonconfigurable_keys->set(nonconfigurable_keys_length,
                                       target_keys->get(i));
      nonconfigurable_keys_length++;
      // The key was moved, null it out in the original list.
      target_keys->set(i, Smi::kZero);
    } else {
      // 14c. Else,
      // 14c i. Append key as an element of targetConfigurableKeys.
      // (No-op, just keep it in |target_keys|.)
    }
  }
  // 15. If extensibleTarget is true and targetNonconfigurableKeys is empty,
  //     then:
  if (extensible_target && nonconfigurable_keys_length == 0) {
    // 15a. Return trapResult.
    return AddKeysFromJSProxy(proxy, trap_result);
  }
  // 16. Let uncheckedResultKeys be a new List which is a copy of trapResult.
  Zone set_zone(isolate_->allocator(), ZONE_NAME);
  ZoneAllocationPolicy alloc(&set_zone);
  const int kPresent = 1;
  const int kGone = 0;
  base::TemplateHashMapImpl<Handle<Name>, int, NameComparator,
                            ZoneAllocationPolicy>
      unchecked_result_keys(ZoneHashMap::kDefaultHashMapCapacity,
                            NameComparator(), alloc);
  int unchecked_result_keys_size = 0;
  for (int i = 0; i < trap_result->length(); ++i) {
    Handle<Name> key(Name::cast(trap_result->get(i)), isolate_);
    auto entry = unchecked_result_keys.LookupOrInsert(key, key->Hash(), alloc);
    if (entry->value != kPresent) {
      entry->value = kPresent;
      unchecked_result_keys_size++;
    }
  }
  // 17. Repeat, for each key that is an element of targetNonconfigurableKeys:
  for (int i = 0; i < nonconfigurable_keys_length; ++i) {
    Object* raw_key = target_nonconfigurable_keys->get(i);
    Handle<Name> key(Name::cast(raw_key), isolate_);
    // 17a. If key is not an element of uncheckedResultKeys, throw a
    //      TypeError exception.
    auto found = unchecked_result_keys.Lookup(key, key->Hash());
    if (found == nullptr || found->value == kGone) {
      isolate_->Throw(*isolate_->factory()->NewTypeError(
          MessageTemplate::kProxyOwnKeysMissing, key));
      return Nothing<bool>();
    }
    // 17b. Remove key from uncheckedResultKeys.
    found->value = kGone;
    unchecked_result_keys_size--;
  }
  // 18. If extensibleTarget is true, return trapResult.
  if (extensible_target) {
    return AddKeysFromJSProxy(proxy, trap_result);
  }
  // 19. Repeat, for each key that is an element of targetConfigurableKeys:
  for (int i = 0; i < target_configurable_keys->length(); ++i) {
    Object* raw_key = target_configurable_keys->get(i);
    if (raw_key->IsSmi()) continue;  // Zapped entry, was nonconfigurable.
    Handle<Name> key(Name::cast(raw_key), isolate_);
    // 19a. If key is not an element of uncheckedResultKeys, throw a
    //      TypeError exception.
    auto found = unchecked_result_keys.Lookup(key, key->Hash());
    if (found == nullptr || found->value == kGone) {
      isolate_->Throw(*isolate_->factory()->NewTypeError(
          MessageTemplate::kProxyOwnKeysMissing, key));
      return Nothing<bool>();
    }
    // 19b. Remove key from uncheckedResultKeys.
    found->value = kGone;
    unchecked_result_keys_size--;
  }
  // 20. If uncheckedResultKeys is not empty, throw a TypeError exception.
  if (unchecked_result_keys_size != 0) {
    DCHECK_GT(unchecked_result_keys_size, 0);
    isolate_->Throw(*isolate_->factory()->NewTypeError(
        MessageTemplate::kProxyOwnKeysNonExtensible));
    return Nothing<bool>();
  }
  // 21. Return trapResult.
  return AddKeysFromJSProxy(proxy, trap_result);
}

Maybe<bool> KeyAccumulator::CollectOwnJSProxyTargetKeys(
    Handle<JSProxy> proxy, Handle<JSReceiver> target) {
  // TODO(cbruni): avoid creating another KeyAccumulator
  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate_, keys,
      KeyAccumulator::GetKeys(target, KeyCollectionMode::kOwnOnly, filter_,
                              GetKeysConversion::kConvertToString, is_for_in_),
      Nothing<bool>());
  Maybe<bool> result = AddKeysFromJSProxy(proxy, keys);
  return result;
}

}  // namespace internal
}  // namespace v8
