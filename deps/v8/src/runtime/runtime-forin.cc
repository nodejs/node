// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments-inl.h"
#include "src/counters.h"
#include "src/elements.h"
#include "src/heap/factory.h"
#include "src/isolate-inl.h"
#include "src/keys.h"
#include "src/objects-inl.h"
#include "src/objects/module.h"

namespace v8 {
namespace internal {

namespace {

// Returns either a FixedArray or, if the given {receiver} has an enum cache
// that contains all enumerable properties of the {receiver} and its prototypes
// have none, the map of the {receiver}. This is used to speed up the check for
// deletions during a for-in.
MaybeHandle<HeapObject> Enumerate(Isolate* isolate,
                                  Handle<JSReceiver> receiver) {
  JSObject::MakePrototypesFast(receiver, kStartAtReceiver, isolate);
  FastKeyAccumulator accumulator(isolate, receiver,
                                 KeyCollectionMode::kIncludePrototypes,
                                 ENUMERABLE_STRINGS, true);
  // Test if we have an enum cache for {receiver}.
  if (!accumulator.is_receiver_simple_enum()) {
    Handle<FixedArray> keys;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, keys, accumulator.GetKeys(GetKeysConversion::kConvertToString),
        HeapObject);
    // Test again, since cache may have been built by GetKeys() calls above.
    if (!accumulator.is_receiver_simple_enum()) return keys;
  }
  DCHECK(!receiver->IsJSModuleNamespace());
  return handle(receiver->map(), isolate);
}

// This is a slight modifcation of JSReceiver::HasProperty, dealing with
// the oddities of JSProxy and JSModuleNamespace in for-in filter.
MaybeHandle<Object> HasEnumerableProperty(Isolate* isolate,
                                          Handle<JSReceiver> receiver,
                                          Handle<Object> key) {
  bool success = false;
  Maybe<PropertyAttributes> result = Just(ABSENT);
  LookupIterator it =
      LookupIterator::PropertyOrElement(isolate, receiver, key, &success);
  if (!success) return isolate->factory()->undefined_value();
  for (; it.IsFound(); it.Next()) {
    switch (it.state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY: {
        // For proxies we have to invoke the [[GetOwnProperty]] trap.
        result = JSProxy::GetPropertyAttributes(&it);
        if (result.IsNothing()) return MaybeHandle<Object>();
        if (result.FromJust() == ABSENT) {
          // Continue lookup on the proxy's prototype.
          Handle<JSProxy> proxy = it.GetHolder<JSProxy>();
          Handle<Object> prototype;
          ASSIGN_RETURN_ON_EXCEPTION(isolate, prototype,
                                     JSProxy::GetPrototype(proxy), Object);
          if (prototype->IsNull(isolate)) {
            return isolate->factory()->undefined_value();
          }
          // We already have a stack-check in JSProxy::GetPrototype.
          return HasEnumerableProperty(
              isolate, Handle<JSReceiver>::cast(prototype), key);
        } else if (result.FromJust() & DONT_ENUM) {
          return isolate->factory()->undefined_value();
        } else {
          return it.GetName();
        }
      }
      case LookupIterator::INTERCEPTOR: {
        result = JSObject::GetPropertyAttributesWithInterceptor(&it);
        if (result.IsNothing()) return MaybeHandle<Object>();
        if (result.FromJust() != ABSENT) return it.GetName();
        continue;
      }
      case LookupIterator::ACCESS_CHECK: {
        if (it.HasAccess()) continue;
        result = JSObject::GetPropertyAttributesWithFailedAccessCheck(&it);
        if (result.IsNothing()) return MaybeHandle<Object>();
        if (result.FromJust() != ABSENT) return it.GetName();
        return isolate->factory()->undefined_value();
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        // TypedArray out-of-bounds access.
        return isolate->factory()->undefined_value();
      case LookupIterator::ACCESSOR: {
        if (it.GetHolder<Object>()->IsJSModuleNamespace()) {
          result = JSModuleNamespace::GetPropertyAttributes(&it);
          if (result.IsNothing()) return MaybeHandle<Object>();
          DCHECK_EQ(0, result.FromJust() & DONT_ENUM);
        }
        return it.GetName();
      }
      case LookupIterator::DATA:
        return it.GetName();
    }
  }
  return isolate->factory()->undefined_value();
}

}  // namespace


RUNTIME_FUNCTION(Runtime_ForInEnumerate) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  RETURN_RESULT_OR_FAILURE(isolate, Enumerate(isolate, receiver));
}


RUNTIME_FUNCTION(Runtime_ForInHasProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, HasEnumerableProperty(isolate, receiver, key));
  return isolate->heap()->ToBoolean(!result->IsUndefined(isolate));
}

}  // namespace internal
}  // namespace v8
