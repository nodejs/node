// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOOKUP_INL_H_
#define V8_LOOKUP_INL_H_

#include "src/lookup.h"

namespace v8 {
namespace internal {


JSReceiver* LookupIterator::NextHolder(Map* map) {
  DisallowHeapAllocation no_gc;
  if (map->prototype()->IsNull()) return NULL;

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


LookupIterator::State LookupIterator::LookupInHolder(Map* map,
                                                     JSReceiver* holder) {
  STATIC_ASSERT(INTERCEPTOR == BEFORE_PROPERTY);
  DisallowHeapAllocation no_gc;
  switch (state_) {
    case NOT_FOUND:
      if (map->IsJSProxyMap()) return JSPROXY;
      if (map->is_access_check_needed()) return ACCESS_CHECK;
    // Fall through.
    case ACCESS_CHECK:
      if (check_interceptor() && map->has_named_interceptor()) {
        return INTERCEPTOR;
      }
    // Fall through.
    case INTERCEPTOR:
      if (map->is_dictionary_map()) {
        NameDictionary* dict = JSObject::cast(holder)->property_dictionary();
        number_ = dict->FindEntry(name_);
        if (number_ == NameDictionary::kNotFound) return NOT_FOUND;
        property_details_ = dict->DetailsAt(number_);
        if (holder->IsGlobalObject()) {
          if (property_details_.IsDeleted()) return NOT_FOUND;
          PropertyCell* cell = PropertyCell::cast(dict->ValueAt(number_));
          if (cell->value()->IsTheHole()) return NOT_FOUND;
        }
      } else {
        DescriptorArray* descriptors = map->instance_descriptors();
        number_ = descriptors->SearchWithCache(*name_, map);
        if (number_ == DescriptorArray::kNotFound) return NOT_FOUND;
        property_details_ = descriptors->GetDetails(number_);
      }
      has_property_ = true;
      switch (property_details_.type()) {
        case v8::internal::CONSTANT:
        case v8::internal::FIELD:
        case v8::internal::NORMAL:
          return DATA;
        case v8::internal::CALLBACKS:
          return ACCESSOR;
      }
    case ACCESSOR:
    case DATA:
      return NOT_FOUND;
    case JSPROXY:
    case TRANSITION:
      UNREACHABLE();
  }
  UNREACHABLE();
  return state_;
}
}
}  // namespace v8::internal

#endif  // V8_LOOKUP_INL_H_
