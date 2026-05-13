// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_INTERCEPTOR_MAP_H_
#define V8_OBJECTS_JS_INTERCEPTOR_MAP_H_

#include "src/objects/map.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

class InterceptorInfo;

// Extended map for interceptor objects, it caches named and indexed
// InterceptorInfo objects in the Map for faster access.
V8_OBJECT class JSInterceptorMap : public ExtendedMap {
 public:
  inline Tagged<InterceptorInfo> named_interceptor() const;
  inline void set_named_interceptor(
      Tagged<InterceptorInfo> interceptor_info,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<InterceptorInfo> indexed_interceptor() const;
  inline void set_indexed_interceptor(
      Tagged<InterceptorInfo> interceptor_info,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline void clear_extended_padding();

 public:
  uint8_t extended_padding_[kTaggedSize - 1];
  TaggedMember<InterceptorInfo> named_interceptor_;
  TaggedMember<InterceptorInfo> indexed_interceptor_;
} V8_OBJECT_END;

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_INTERCEPTOR_MAP_H_
