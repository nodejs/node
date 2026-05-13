// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_INTERCEPTOR_MAP_INL_H_
#define V8_OBJECTS_JS_INTERCEPTOR_MAP_INL_H_

#include "src/objects/js-interceptor-map.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/api-callbacks-inl.h"
#include "src/objects/map-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

Tagged<InterceptorInfo> JSInterceptorMap::named_interceptor() const {
  return named_interceptor_.load();
}
void JSInterceptorMap::set_named_interceptor(
    Tagged<InterceptorInfo> interceptor_info, WriteBarrierMode mode) {
  DCHECK(interceptor_info->is_named());
  named_interceptor_.store(this, interceptor_info, mode);
}

Tagged<InterceptorInfo> JSInterceptorMap::indexed_interceptor() const {
  return indexed_interceptor_.load();
}
void JSInterceptorMap::set_indexed_interceptor(
    Tagged<InterceptorInfo> interceptor_info, WriteBarrierMode mode) {
  DCHECK(!interceptor_info->is_named());
  indexed_interceptor_.store(this, interceptor_info, mode);
}

void JSInterceptorMap::clear_extended_padding() {
  memset(extended_padding_, 0, sizeof(extended_padding_));
}

Tagged<InterceptorInfo> Map::GetNamedInterceptor() const {
  DCHECK(has_named_interceptor());
  Tagged<InterceptorInfo> interceptor_info =
      Cast<JSInterceptorMap>(Tagged<Map>(this))->named_interceptor();
  DCHECK_EQ(GetFunctionTemplateInfo()->GetNamedPropertyHandler(),
            interceptor_info);
  return interceptor_info;
}

Tagged<InterceptorInfo> Map::GetIndexedInterceptor() const {
  DCHECK(has_indexed_interceptor());
  Tagged<InterceptorInfo> interceptor_info =
      Cast<JSInterceptorMap>(Tagged<Map>(this))->indexed_interceptor();
  DCHECK_EQ(GetFunctionTemplateInfo()->GetIndexedPropertyHandler(),
            interceptor_info);
  return interceptor_info;
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_INTERCEPTOR_MAP_INL_H_
