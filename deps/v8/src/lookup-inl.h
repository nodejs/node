// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOOKUP_INL_H_
#define V8_LOOKUP_INL_H_

#include "src/lookup.h"

#include "src/elements.h"

namespace v8 {
namespace internal {


JSReceiver* LookupIterator::NextHolder(Map* map) {
  DisallowHeapAllocation no_gc;
  if (!map->prototype()->IsJSReceiver()) return NULL;

  JSReceiver* next = JSReceiver::cast(map->prototype());
  DCHECK(!next->map()->IsGlobalObjectMap() ||
         next->map()->is_hidden_prototype());

  if (!check_prototype_chain() &&
      !(check_hidden() && next->map()->is_hidden_prototype()) &&
      // Always lookup behind the JSGlobalProxy into the JSGlobalObject, even
      // when not checking other hidden prototypes.
      !map->IsJSGlobalProxyMap()) {
    return NULL;
  }

  return next;
}


LookupIterator::State LookupIterator::LookupInHolder(Map* const map,
                                                     JSReceiver* const holder) {
  STATIC_ASSERT(INTERCEPTOR == BEFORE_PROPERTY);
  DisallowHeapAllocation no_gc;
  if (interceptor_state_ == InterceptorState::kProcessNonMasking) {
    return LookupNonMaskingInterceptorInHolder(map, holder);
  }
  switch (state_) {
    case NOT_FOUND:
      if (map->IsJSProxyMap()) return JSPROXY;
      if (map->is_access_check_needed() &&
          (IsElement() || !isolate_->IsInternallyUsedPropertyName(name_))) {
        return ACCESS_CHECK;
      }
    // Fall through.
    case ACCESS_CHECK:
      if (exotic_index_state_ != ExoticIndexState::kNotExotic &&
          IsIntegerIndexedExotic(holder)) {
        return INTEGER_INDEXED_EXOTIC;
      }
      if (check_interceptor() && HasInterceptor(map) &&
          !SkipInterceptor(JSObject::cast(holder))) {
        return INTERCEPTOR;
      }
    // Fall through.
    case INTERCEPTOR:
      if (IsElement()) {
        // TODO(verwaest): Optimize.
        if (holder->IsStringObjectWithCharacterAt(index_)) {
          PropertyAttributes attributes =
              static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE);
          property_details_ = PropertyDetails(attributes, v8::internal::DATA, 0,
                                              PropertyCellType::kNoCell);
        } else {
          JSObject* js_object = JSObject::cast(holder);
          if (js_object->elements() == isolate()->heap()->empty_fixed_array()) {
            return NOT_FOUND;
          }

          ElementsAccessor* accessor = js_object->GetElementsAccessor();
          FixedArrayBase* backing_store = js_object->elements();
          number_ =
              accessor->GetEntryForIndex(js_object, backing_store, index_);
          if (number_ == kMaxUInt32) return NOT_FOUND;
          property_details_ = accessor->GetDetails(backing_store, number_);
        }
      } else if (!map->is_dictionary_map()) {
        DescriptorArray* descriptors = map->instance_descriptors();
        int number = descriptors->SearchWithCache(*name_, map);
        if (number == DescriptorArray::kNotFound) return NOT_FOUND;
        number_ = static_cast<uint32_t>(number);
        property_details_ = descriptors->GetDetails(number_);
      } else if (map->IsGlobalObjectMap()) {
        GlobalDictionary* dict = JSObject::cast(holder)->global_dictionary();
        int number = dict->FindEntry(name_);
        if (number == GlobalDictionary::kNotFound) return NOT_FOUND;
        number_ = static_cast<uint32_t>(number);
        DCHECK(dict->ValueAt(number_)->IsPropertyCell());
        PropertyCell* cell = PropertyCell::cast(dict->ValueAt(number_));
        if (cell->value()->IsTheHole()) return NOT_FOUND;
        property_details_ = cell->property_details();
      } else {
        NameDictionary* dict = JSObject::cast(holder)->property_dictionary();
        int number = dict->FindEntry(name_);
        if (number == NameDictionary::kNotFound) return NOT_FOUND;
        number_ = static_cast<uint32_t>(number);
        property_details_ = dict->DetailsAt(number_);
      }
      has_property_ = true;
      switch (property_details_.kind()) {
        case v8::internal::kData:
          return DATA;
        case v8::internal::kAccessor:
          return ACCESSOR;
      }
    case ACCESSOR:
    case DATA:
      return NOT_FOUND;
    case INTEGER_INDEXED_EXOTIC:
    case JSPROXY:
    case TRANSITION:
      UNREACHABLE();
  }
  UNREACHABLE();
  return state_;
}


LookupIterator::State LookupIterator::LookupNonMaskingInterceptorInHolder(
    Map* const map, JSReceiver* const holder) {
  switch (state_) {
    case NOT_FOUND:
      if (check_interceptor() && HasInterceptor(map) &&
          !SkipInterceptor(JSObject::cast(holder))) {
        return INTERCEPTOR;
      }
    // Fall through.
    default:
      return NOT_FOUND;
  }
  UNREACHABLE();
  return state_;
}
}
}  // namespace v8::internal

#endif  // V8_LOOKUP_INL_H_
