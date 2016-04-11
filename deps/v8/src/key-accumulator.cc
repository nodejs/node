// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/key-accumulator.h"

#include "src/elements.h"
#include "src/factory.h"
#include "src/isolate-inl.h"
#include "src/objects-inl.h"
#include "src/property-descriptor.h"


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
    if (num_string_properties < 0) {
      // If the |num_string_properties| is negative, the current level contains
      // properties from a proxy, hence we skip the integer keys in |elements_|
      // since proxies define the complete ordering.
      num_string_properties = -num_string_properties;
    } else if (level < elements_.size()) {
      // Add the element indices for this prototype level.
      std::vector<uint32_t>* elements = elements_[level];
      int num_elements = static_cast<int>(elements->size());
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
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate_, keys, FilterProxyKeys(isolate_, proxy, keys, filter_),
      Nothing<bool>());
  // Proxies define a complete list of keys with no distinction of
  // elements and properties, which breaks the normal assumption for the
  // KeyAccumulator.
  AddKeys(keys, PROXY_MAGIC);
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


}  // namespace internal
}  // namespace v8
