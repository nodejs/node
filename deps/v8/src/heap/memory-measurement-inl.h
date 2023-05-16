// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MEMORY_MEASUREMENT_INL_H_
#define V8_HEAP_MEMORY_MEASUREMENT_INL_H_

#include "src/heap/memory-measurement.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/contexts.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/map-inl.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

bool NativeContextInferrer::Infer(Isolate* isolate, Map map, HeapObject object,
                                  Address* native_context) {
  switch (map.visitor_id()) {
    case kVisitContext:
      return InferForContext(isolate, Context::cast(object), native_context);
    case kVisitNativeContext:
      *native_context = object.ptr();
      return true;
    case kVisitJSFunction:
      return InferForJSFunction(isolate, JSFunction::cast(object),
                                native_context);
    case kVisitJSApiObject:
    case kVisitJSArrayBuffer:
    case kVisitJSFinalizationRegistry:
    case kVisitJSObject:
    case kVisitJSObjectFast:
    case kVisitJSTypedArray:
    case kVisitJSWeakCollection:
      return InferForJSObject(isolate, map, JSObject::cast(object),
                              native_context);
    default:
      return false;
  }
}

V8_INLINE bool NativeContextStats::HasExternalBytes(Map map) {
  InstanceType instance_type = map.instance_type();
  return (instance_type == JS_ARRAY_BUFFER_TYPE ||
          InstanceTypeChecker::IsExternalString(instance_type));
}

V8_INLINE void NativeContextStats::IncrementSize(Address context, Map map,
                                                 HeapObject object,
                                                 size_t size) {
  size_by_context_[context] += size;
  if (HasExternalBytes(map)) {
    IncrementExternalSize(context, map, object);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MEMORY_MEASUREMENT_INL_H_
