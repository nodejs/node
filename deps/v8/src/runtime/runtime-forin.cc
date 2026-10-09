// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate-inl.h"
#include "src/heap/factory.h"
#include "src/objects/keys.h"
#include "src/objects/module.h"
#include "src/objects/objects-inl.h"
#include "src/roots/roots-inl.h"

namespace v8 {
namespace internal {

namespace {

MaybeDirectHandle<ForInEnumeratorHolder> TryCreateEnumeratorHolder(
    Isolate* isolate, DirectHandle<JSReceiver> receiver,
    FastKeyAccumulator* accumulator) {
  if (!v8_flags.forin_enumerator_holder) return {};
  if (!IsJSObject(*receiver)) return {};
  DirectHandle<JSObject> object = Cast<JSObject>(receiver);

  if (!accumulator->has_empty_prototype()) return {};

  ElementsKind elements_kind = object->GetElementsKind();
  if (!IsFastPackedElementsKind(elements_kind)) return {};

  uint32_t elem_len;
  if (IsJSArray(*object)) {
    Tagged<Object> length = Cast<JSArray>(*object)->length();
    if (!IsSmi(length) || Smi::ToInt(length) <= 0) return {};
    elem_len = static_cast<uint32_t>(Smi::ToInt(length));
  } else {
    elem_len = object->elements()->ulength().value();
    if (elem_len == 0 || !Smi::IsValid(elem_len)) return {};
  }

  DirectHandle<Map> map(object->map(), isolate);
  if (!map->OnlyHasSimpleProperties()) return {};
  DCHECK(!map->is_dictionary_map());

  int enum_length = map->EnumLength();
  if (enum_length == kInvalidEnumCacheSentinel) {
    enum_length = map->NumberOfEnumerableProperties();
    if (enum_length > 0) {
      DirectHandle<DescriptorArray> descriptors(map->instance_descriptors(),
                                                isolate);
      DirectHandle<FixedArray> keys(descriptors->enum_cache()->keys(), isolate);
      if (static_cast<uint32_t>(enum_length) <= keys->ulength().value()) {
        map->SetEnumLength(enum_length);
      } else {
        FastKeyAccumulator::InitializeFastPropertyEnumCache(isolate, map,
                                                            enum_length);
      }
    } else {
      map->SetEnumLength(0);
    }
  }

  if (elem_len > static_cast<uint32_t>(FixedArray::kMaxLength - enum_length)) {
    return {};
  }

  DirectHandle<FixedArray> named_keys =
      enum_length > 0
          ? direct_handle(map->instance_descriptors()->enum_cache()->keys(),
                          isolate)
          : isolate->factory()->empty_fixed_array();

  int int_elem_len = static_cast<int>(elem_len);
  return isolate->factory()->NewForInEnumeratorHolder(
      map, named_keys, Smi::FromInt(int_elem_len),
      Smi::FromInt(int_elem_len + enum_length));
}

// Returns either a FixedArray of keys, a ForInEnumeratorHolder (if {receiver}
// has fast packed elements, simple properties, and an empty prototype chain),
// or, if the given {receiver} has an enum cache that contains all enumerable
// properties of the {receiver} and its prototypes have none, the map of the
// {receiver}. This is used to speed up the check for deletions during a
// for-in.
MaybeDirectHandle<HeapObject> Enumerate(Isolate* isolate,
                                        DirectHandle<JSReceiver> receiver) {
  JSObject::MakePrototypesFast(receiver, kStartAtReceiver, isolate);
  FastKeyAccumulator accumulator(isolate, receiver,
                                 KeyCollectionMode::kIncludePrototypes,
                                 ENUMERABLE_STRINGS, true);
  // Test if we have an enum cache for {receiver}.
  if (!accumulator.is_receiver_simple_enum()) {
    if (DirectHandle<ForInEnumeratorHolder> holder;
        TryCreateEnumeratorHolder(isolate, receiver, &accumulator)
            .ToHandle(&holder)) {
      return holder;
    }
    DirectHandle<FixedArray> keys;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, keys,
        accumulator.GetKeys(accumulator.may_have_elements()
                                ? GetKeysConversion::kConvertToString
                                : GetKeysConversion::kNoNumbers));
    // Test again, since cache may have been built by GetKeys() calls above.
    if (!accumulator.is_receiver_simple_enum()) return keys;
  }
  DCHECK(!IsJSModuleNamespace(*receiver));
  return direct_handle(receiver->map(), isolate);
}

// This is a slight modification of JSReceiver::HasProperty, dealing with
// the oddities of JSProxy and JSModuleNamespace in for-in filter.
MaybeDirectHandle<Object> HasEnumerableProperty(
    Isolate* isolate, DirectHandle<JSReceiver> receiver,
    DirectHandle<Object> key) {
  bool success = false;
  Maybe<PropertyAttributes> result = Just(ABSENT);
  PropertyKey lookup_key(isolate, key, &success);
  if (!success) return isolate->factory()->undefined_value();
  LookupIterator it(isolate, receiver, lookup_key);
  for (;; it.Next()) {
    switch (it.state()) {
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY: {
        // For proxies we have to invoke the [[GetOwnProperty]] trap.
        result = JSProxy::GetPropertyAttributes(&it);
        if (result.IsNothing()) return MaybeDirectHandle<Object>();
        if (result.FromJust() == ABSENT) {
          // Continue lookup on the proxy's prototype.
          DirectHandle<JSProxy> proxy = it.GetHolder<JSProxy>();
          DirectHandle<Object> prototype;
          ASSIGN_RETURN_ON_EXCEPTION(isolate, prototype,
                                     JSProxy::GetPrototype(proxy));
          if (IsNull(*prototype)) {
            return isolate->factory()->undefined_value();
          }
          // We already have a stack-check in JSProxy::GetPrototype.
          return HasEnumerableProperty(isolate, Cast<JSReceiver>(prototype),
                                       key);
        } else if (result.FromJust() & DONT_ENUM) {
          return isolate->factory()->undefined_value();
        } else {
          return it.GetName();
        }
      }
      case LookupIterator::MODULE_NAMESPACE: {
        // It triggers evaluation, because this access is like calling
        // [[GetOwnProperty]] on deferred namespace object.
        if (JSDeferredModuleNamespace::TriggersEvaluation(&it)) {
          DirectHandle<JSDeferredModuleNamespace> holder =
              it.GetHolder<JSDeferredModuleNamespace>();
          JSDeferredModuleNamespace::EvaluateModuleSync(isolate, holder);
          RETURN_EXCEPTION_IF_EXCEPTION(isolate);
        }
        continue;
      }
      case LookupIterator::STRING_LOOKUP_START_OBJECT:
        UNREACHABLE();
      case LookupIterator::WASM_OBJECT:
        continue;  // Continue to the prototype, if present.
      case LookupIterator::INTERCEPTOR: {
        result = JSObject::GetPropertyAttributesWithInterceptor(&it);
        if (result.IsNothing()) return MaybeDirectHandle<Object>();
        if (result.FromJust() != ABSENT) return it.GetName();
        continue;
      }
      case LookupIterator::ACCESS_CHECK: {
        if (it.HasAccess()) continue;
        result = JSObject::GetPropertyAttributesWithFailedAccessCheck(&it);
        if (result.IsNothing()) return MaybeDirectHandle<Object>();
        if (result.FromJust() != ABSENT) return it.GetName();
        return isolate->factory()->undefined_value();
      }
      case LookupIterator::TYPED_ARRAY_INDEX_NOT_FOUND:
        // TypedArray out-of-bounds access.
        return isolate->factory()->undefined_value();
      case LookupIterator::ACCESSOR: {
        if (IsJSModuleNamespace(*it.GetHolder<Object>())) {
          result = JSModuleNamespace::GetPropertyAttributes(&it);
          if (result.IsNothing()) return MaybeDirectHandle<Object>();
          DCHECK_EQ(0, result.FromJust() & DONT_ENUM);
        }
        return it.GetName();
      }
      case LookupIterator::DATA:
        return it.GetName();
      case LookupIterator::NOT_FOUND:
        return isolate->factory()->undefined_value();
    }
    UNREACHABLE();
  }
}

}  // namespace

RUNTIME_FUNCTION(Runtime_ForInEnumerate) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  DirectHandle<JSReceiver> receiver = args.at<JSReceiver>(0);
  RETURN_RESULT_OR_FAILURE(isolate, Enumerate(isolate, receiver));
}

RUNTIME_FUNCTION(Runtime_ForInHasProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  DirectHandle<JSReceiver> receiver = args.at<JSReceiver>(0);
  DirectHandle<Object> key = args.at(1);
  DirectHandle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, HasEnumerableProperty(isolate, receiver, key));
  return ReadOnlyRoots(isolate).boolean_value(!IsUndefined(*result));
}

}  // namespace internal
}  // namespace v8
