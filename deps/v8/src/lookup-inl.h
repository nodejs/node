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

  if (!check_derived() &&
      !(check_hidden() && next->map()->is_hidden_prototype())) {
    return NULL;
  }

  return next;
}


LookupIterator::State LookupIterator::LookupInHolder(Map* map) {
  DisallowHeapAllocation no_gc;
  switch (state_) {
    case NOT_FOUND:
      if (map->IsJSProxyMap()) {
        return JSPROXY;
      }
      if (check_access_check() && map->is_access_check_needed()) {
        return ACCESS_CHECK;
      }
    // Fall through.
    case ACCESS_CHECK:
      if (check_interceptor() && map->has_named_interceptor()) {
        return INTERCEPTOR;
      }
    // Fall through.
    case INTERCEPTOR:
      if (map->is_dictionary_map()) {
        property_encoding_ = DICTIONARY;
      } else {
        DescriptorArray* descriptors = map->instance_descriptors();
        number_ = descriptors->SearchWithCache(*name_, map);
        if (number_ == DescriptorArray::kNotFound) return NOT_FOUND;
        property_encoding_ = DESCRIPTOR;
      }
      return PROPERTY;
    case PROPERTY:
      return NOT_FOUND;
    case JSPROXY:
      UNREACHABLE();
  }
  UNREACHABLE();
  return state_;
}
}
}  // namespace v8::internal

#endif  // V8_LOOKUP_INL_H_
