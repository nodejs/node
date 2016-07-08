// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/keys.h"

#include "src/elements.h"
#include "src/factory.h"
#include "src/isolate-inl.h"
#include "src/objects-inl.h"
#include "src/property-descriptor.h"
#include "src/prototype.h"

namespace v8 {
namespace internal {

KeyAccumulator::~KeyAccumulator() {
  for (size_t i = 0; i < elements_.size(); i++) {
    delete elements_[i];
  }
}

Handle<FixedArray> KeyAccumulator::GetKeys(GetKeysConversion convert) {
  if (length_ == 0) {
    return isolate_->factory()->empty_fixed_array();
  }
  // Make sure we have all the lengths collected.
  NextPrototype();

  if (type_ == OWN_ONLY && !ownProxyKeys_.is_null()) {
    return ownProxyKeys_;
  }
  // Assemble the result array by first adding the element keys and then the
  // property keys. We use the total number of String + Symbol keys per level in
  // |level_lengths_| and the available element keys in the corresponding bucket
  // in |elements_| to deduce the number of keys to take from the
  // |string_properties_| and |symbol_properties_| set.
  Handle<FixedArray> result = isolate_->factory()->NewFixedArray(length_);
  int insertion_index = 0;
  int string_properties_index = 0;
  int symbol_properties_index = 0;
  // String and Symbol lengths always come in pairs:
  size_t max_level = level_lengths_.size() / 2;
  for (size_t level = 0; level < max_level; level++) {
    int num_string_properties = level_lengths_[level * 2];
    int num_symbol_properties = level_lengths_[level * 2 + 1];
    int num_elements = 0;
    if (num_string_properties < 0) {
      // If the |num_string_properties| is negative, the current level contains
      // properties from a proxy, hence we skip the integer keys in |elements_|
      // since proxies define the complete ordering.
      num_string_properties = -num_string_properties;
    } else if (level < elements_.size()) {
      // Add the element indices for this prototype level.
      std::vector<uint32_t>* elements = elements_[level];
      num_elements = static_cast<int>(elements->size());
      for (int i = 0; i < num_elements; i++) {
        Handle<Object> key;
        if (convert == KEEP_NUMBERS) {
          key = isolate_->factory()->NewNumberFromUint(elements->at(i));
        } else {
          key = isolate_->factory()->Uint32ToString(elements->at(i));
        }
        result->set(insertion_index, *key);
        insertion_index++;
      }
    }
    // Add the string property keys for this prototype level.
    for (int i = 0; i < num_string_properties; i++) {
      Object* key = string_properties_->KeyAt(string_properties_index);
      result->set(insertion_index, key);
      insertion_index++;
      string_properties_index++;
    }
    // Add the symbol property keys for this prototype level.
    for (int i = 0; i < num_symbol_properties; i++) {
      Object* key = symbol_properties_->KeyAt(symbol_properties_index);
      result->set(insertion_index, key);
      insertion_index++;
      symbol_properties_index++;
    }
    if (FLAG_trace_for_in_enumerate) {
      PrintF("| strings=%d symbols=%d elements=%i ", num_string_properties,
             num_symbol_properties, num_elements);
    }
  }
  if (FLAG_trace_for_in_enumerate) {
    PrintF("|| prototypes=%zu ||\n", max_level);
  }

  DCHECK_EQ(insertion_index, length_);
  return result;
}

namespace {

bool AccumulatorHasKey(std::vector<uint32_t>* sub_elements, uint32_t key) {
  return std::binary_search(sub_elements->begin(), sub_elements->end(), key);
}

}  // namespace

bool KeyAccumulator::AddKey(Object* key, AddKeyConversion convert) {
  return AddKey(handle(key, isolate_), convert);
}

bool KeyAccumulator::AddKey(Handle<Object> key, AddKeyConversion convert) {
  if (key->IsSymbol()) {
    if (filter_ & SKIP_SYMBOLS) return false;
    if (Handle<Symbol>::cast(key)->is_private()) return false;
    return AddSymbolKey(key);
  }
  if (filter_ & SKIP_STRINGS) return false;
  // Make sure we do not add keys to a proxy-level (see AddKeysFromProxy).
  DCHECK_LE(0, level_string_length_);
  // In some cases (e.g. proxies) we might get in String-converted ints which
  // should be added to the elements list instead of the properties. For
  // proxies we have to convert as well but also respect the original order.
  // Therefore we add a converted key to both sides
  if (convert == CONVERT_TO_ARRAY_INDEX || convert == PROXY_MAGIC) {
    uint32_t index = 0;
    int prev_length = length_;
    int prev_proto = level_string_length_;
    if ((key->IsString() && Handle<String>::cast(key)->AsArrayIndex(&index)) ||
        key->ToArrayIndex(&index)) {
      bool key_was_added = AddIntegerKey(index);
      if (convert == CONVERT_TO_ARRAY_INDEX) return key_was_added;
      if (convert == PROXY_MAGIC) {
        // If we had an array index (number) and it wasn't added, the key
        // already existed before, hence we cannot add it to the properties
        // keys as it would lead to duplicate entries.
        if (!key_was_added) {
          return false;
        }
        length_ = prev_length;
        level_string_length_ = prev_proto;
      }
    }
  }
  return AddStringKey(key, convert);
}

bool KeyAccumulator::AddKey(uint32_t key) { return AddIntegerKey(key); }

bool KeyAccumulator::AddIntegerKey(uint32_t key) {
  // Make sure we do not add keys to a proxy-level (see AddKeysFromProxy).
  // We mark proxy-levels with a negative length
  DCHECK_LE(0, level_string_length_);
  // Binary search over all but the last level. The last one might not be
  // sorted yet.
  for (size_t i = 1; i < elements_.size(); i++) {
    if (AccumulatorHasKey(elements_[i - 1], key)) return false;
  }
  elements_.back()->push_back(key);
  length_++;
  return true;
}

bool KeyAccumulator::AddStringKey(Handle<Object> key,
                                  AddKeyConversion convert) {
  if (string_properties_.is_null()) {
    string_properties_ = OrderedHashSet::Allocate(isolate_, 16);
  }
  // TODO(cbruni): remove this conversion once we throw the correct TypeError
  // for non-string/symbol elements returned by proxies
  if (convert == PROXY_MAGIC && key->IsNumber()) {
    key = isolate_->factory()->NumberToString(key);
  }
  int prev_size = string_properties_->NumberOfElements();
  string_properties_ = OrderedHashSet::Add(string_properties_, key);
  if (prev_size < string_properties_->NumberOfElements()) {
    length_++;
    level_string_length_++;
    return true;
  } else {
    return false;
  }
}

bool KeyAccumulator::AddSymbolKey(Handle<Object> key) {
  if (symbol_properties_.is_null()) {
    symbol_properties_ = OrderedHashSet::Allocate(isolate_, 16);
  }
  int prev_size = symbol_properties_->NumberOfElements();
  symbol_properties_ = OrderedHashSet::Add(symbol_properties_, key);
  if (prev_size < symbol_properties_->NumberOfElements()) {
    length_++;
    level_symbol_length_++;
    return true;
  } else {
    return false;
  }
}

void KeyAccumulator::AddKeys(Handle<FixedArray> array,
                             AddKeyConversion convert) {
  int add_length = array->length();
  if (add_length == 0) return;
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

void KeyAccumulator::AddKeysFromProxy(Handle<JSObject> array_like) {
  // Proxies define a complete list of keys with no distinction of
  // elements and properties, which breaks the normal assumption for the
  // KeyAccumulator.
  AddKeys(array_like, PROXY_MAGIC);
  // Invert the current length to indicate a present proxy, so we can ignore
  // element keys for this level. Otherwise we would not fully respect the order
  // given by the proxy.
  level_string_length_ = -level_string_length_;
}

MaybeHandle<FixedArray> FilterProxyKeys(Isolate* isolate, Handle<JSProxy> owner,
                                        Handle<FixedArray> keys,
                                        PropertyFilter filter) {
  if (filter == ALL_PROPERTIES) {
    // Nothing to do.
    return keys;
  }
  int store_position = 0;
  for (int i = 0; i < keys->length(); ++i) {
    Handle<Name> key(Name::cast(keys->get(i)), isolate);
    if (key->FilterKey(filter)) continue;  // Skip this key.
    if (filter & ONLY_ENUMERABLE) {
      PropertyDescriptor desc;
      Maybe<bool> found =
          JSProxy::GetOwnPropertyDescriptor(isolate, owner, key, &desc);
      MAYBE_RETURN(found, MaybeHandle<FixedArray>());
      if (!found.FromJust() || !desc.enumerable()) continue;  // Skip this key.
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
Maybe<bool> KeyAccumulator::AddKeysFromProxy(Handle<JSProxy> proxy,
                                             Handle<FixedArray> keys) {
  if (filter_proxy_keys_) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, keys, FilterProxyKeys(isolate_, proxy, keys, filter_),
        Nothing<bool>());
  }
  // Proxies define a complete list of keys with no distinction of
  // elements and properties, which breaks the normal assumption for the
  // KeyAccumulator.
  if (type_ == OWN_ONLY) {
    ownProxyKeys_ = keys;
    level_string_length_ = keys->length();
    length_ = level_string_length_;
  } else {
    AddKeys(keys, PROXY_MAGIC);
  }
  // Invert the current length to indicate a present proxy, so we can ignore
  // element keys for this level. Otherwise we would not fully respect the order
  // given by the proxy.
  level_string_length_ = -level_string_length_;
  return Just(true);
}

void KeyAccumulator::AddElementKeysFromInterceptor(
    Handle<JSObject> array_like) {
  AddKeys(array_like, CONVERT_TO_ARRAY_INDEX);
  // The interceptor might introduce duplicates for the current level, since
  // these keys get added after the objects's normal element keys.
  SortCurrentElementsListRemoveDuplicates();
}

void KeyAccumulator::SortCurrentElementsListRemoveDuplicates() {
  // Sort and remove duplicates from the current elements level and adjust.
  // the lengths accordingly.
  auto last_level = elements_.back();
  size_t nof_removed_keys = last_level->size();
  std::sort(last_level->begin(), last_level->end());
  last_level->erase(std::unique(last_level->begin(), last_level->end()),
                    last_level->end());
  // Adjust total length by the number of removed duplicates.
  nof_removed_keys -= last_level->size();
  length_ -= static_cast<int>(nof_removed_keys);
}

void KeyAccumulator::SortCurrentElementsList() {
  if (elements_.empty()) return;
  auto element_keys = elements_.back();
  std::sort(element_keys->begin(), element_keys->end());
}

void KeyAccumulator::NextPrototype() {
  // Store the protoLength on the first call of this method.
  if (!elements_.empty()) {
    level_lengths_.push_back(level_string_length_);
    level_lengths_.push_back(level_symbol_length_);
  }
  elements_.push_back(new std::vector<uint32_t>());
  level_string_length_ = 0;
  level_symbol_length_ = 0;
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

bool CheckAndInitalizeSimpleEnumCache(JSReceiver* object) {
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
  if (type_ == OWN_ONLY) return;
  // Fully walk the prototype chain and find the last prototype with keys.
  is_receiver_simple_enum_ = false;
  has_empty_prototype_ = true;
  JSReceiver* first_non_empty_prototype;
  for (PrototypeIterator iter(isolate_, *receiver_); !iter.IsAtEnd();
       iter.Advance()) {
    JSReceiver* current = iter.GetCurrent<JSReceiver>();
    if (CheckAndInitalizeSimpleEnumCache(current)) continue;
    has_empty_prototype_ = false;
    first_non_empty_prototype = current;
    // TODO(cbruni): use the first non-empty prototype.
    USE(first_non_empty_prototype);
    return;
  }
  DCHECK(has_empty_prototype_);
  is_receiver_simple_enum_ =
      receiver_->map()->EnumLength() != kInvalidEnumCacheSentinel &&
      !JSObject::cast(*receiver_)->HasEnumerableElements();
}

namespace {

template <bool fast_properties>
Handle<FixedArray> GetOwnKeysWithElements(Isolate* isolate,
                                          Handle<JSObject> object,
                                          GetKeysConversion convert) {
  Handle<FixedArray> keys;
  ElementsAccessor* accessor = object->GetElementsAccessor();
  if (fast_properties) {
    keys = JSObject::GetFastEnumPropertyKeys(isolate, object);
  } else {
    // TODO(cbruni): preallocate big enough array to also hold elements.
    keys = JSObject::GetEnumPropertyKeys(object);
  }
  Handle<FixedArray> result =
      accessor->PrependElementIndices(object, keys, convert, ONLY_ENUMERABLE);

  if (FLAG_trace_for_in_enumerate) {
    PrintF("| strings=%d symbols=0 elements=%u || prototypes>=1 ||\n",
           keys->length(), result->length() - keys->length());
  }
  return result;
}

MaybeHandle<FixedArray> GetOwnKeysWithUninitializedEnumCache(
    Isolate* isolate, Handle<JSObject> object) {
  // Uninitalized enum cache
  Map* map = object->map();
  if (object->elements() != isolate->heap()->empty_fixed_array() ||
      object->elements() != isolate->heap()->empty_slow_element_dictionary()) {
    // Assume that there are elements.
    return MaybeHandle<FixedArray>();
  }
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  if (number_of_own_descriptors == 0) {
    map->SetEnumLength(0);
    return isolate->factory()->empty_fixed_array();
  }
  // We have no elements but possibly enumerable property keys, hence we can
  // directly initialize the enum cache.
  return JSObject::GetFastEnumPropertyKeys(isolate, object);
}

bool OnlyHasSimpleProperties(Map* map) {
  return map->instance_type() > LAST_CUSTOM_ELEMENTS_RECEIVER &&
    map->instance_type() != JS_GLOBAL_PROXY_TYPE;
}

}  // namespace

MaybeHandle<FixedArray> FastKeyAccumulator::GetKeys(GetKeysConversion convert) {
  Handle<FixedArray> keys;
  if (GetKeysFast(convert).ToHandle(&keys)) {
    return keys;
  }
  return GetKeysSlow(convert);
}

MaybeHandle<FixedArray> FastKeyAccumulator::GetKeysFast(
    GetKeysConversion convert) {
  bool own_only = has_empty_prototype_ || type_ == OWN_ONLY;
  Map* map = receiver_->map();
  if (!own_only || !OnlyHasSimpleProperties(map)) {
    return MaybeHandle<FixedArray>();
  }

  // From this point on we are certiain to only collect own keys.
  DCHECK(receiver_->IsJSObject());
  Handle<JSObject> object = Handle<JSObject>::cast(receiver_);

  // Do not try to use the enum-cache for dict-mode objects.
  if (map->is_dictionary_map()) {
    return GetOwnKeysWithElements<false>(isolate_, object, convert);
  }
  int enum_length = receiver_->map()->EnumLength();
  if (enum_length == kInvalidEnumCacheSentinel) {
    Handle<FixedArray> keys;
    // Try initializing the enum cache and return own properties.
    if (GetOwnKeysWithUninitializedEnumCache(isolate_, object)
            .ToHandle(&keys)) {
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
  return GetOwnKeysWithElements<true>(isolate_, object, convert);
}

MaybeHandle<FixedArray> FastKeyAccumulator::GetKeysSlow(
    GetKeysConversion convert) {
  return JSReceiver::GetKeys(receiver_, type_, ENUMERABLE_STRINGS, KEEP_NUMBERS,
                             filter_proxy_keys_);
}

}  // namespace internal
}  // namespace v8
