// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-objects.h"

#include "src/api/api-arguments-inl.h"
#include "src/base/optional.h"
#include "src/common/globals.h"
#include "src/date/date.h"
#include "src/execution/arguments.h"
#include "src/execution/frames.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/handles/maybe-handles.h"
#include "src/heap/factory-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/dictionary.h"
#include "src/objects/elements.h"
#include "src/objects/field-type.h"
#include "src/objects/fixed-array.h"
#include "src/objects/heap-number.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/lookup.h"
#include "src/objects/map-updater.h"
#include "src/objects/objects-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-break-iterator.h"
#include "src/objects/js-collator.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-collection.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-date-time-format.h"
#include "src/objects/js-display-names.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-generator-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-list-format.h"
#include "src/objects/js-locale.h"
#include "src/objects/js-number-format.h"
#include "src/objects/js-plural-rules.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-promise.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-regexp-string-iterator.h"
#include "src/objects/js-shadow-realms.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-relative-time-format.h"
#include "src/objects/js-segment-iterator.h"
#include "src/objects/js-segmenter.h"
#include "src/objects/js-segments.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-struct-inl.h"
#include "src/objects/js-temporal-objects-inl.h"
#include "src/objects/js-weak-refs.h"
#include "src/objects/map-inl.h"
#include "src/objects/module.h"
#include "src/objects/oddball.h"
#include "src/objects/property-cell.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/property.h"
#include "src/objects/prototype-info.h"
#include "src/objects/prototype.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/swiss-name-dictionary-inl.h"
#include "src/objects/transitions.h"
#include "src/strings/string-builder-inl.h"
#include "src/strings/string-stream.h"
#include "src/utils/ostreams.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-objects.h"
#include "src/debug/debug-wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

// static
Maybe<bool> JSReceiver::HasProperty(LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return JSProxy::HasProperty(it->isolate(), it->GetHolder<JSProxy>(),
                                    it->GetName());
      case LookupIterator::INTERCEPTOR: {
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithInterceptor(it);
        if (result.IsNothing()) return Nothing<bool>();
        if (result.FromJust() != ABSENT) return Just(true);
        break;
      }
      case LookupIterator::ACCESS_CHECK: {
        if (it->HasAccess()) break;
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithFailedAccessCheck(it);
        if (result.IsNothing()) return Nothing<bool>();
        return Just(result.FromJust() != ABSENT);
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        // TypedArray out-of-bounds access.
        return Just(false);
      case LookupIterator::ACCESSOR:
      case LookupIterator::DATA:
        return Just(true);
    }
  }
  return Just(false);
}

// static
Maybe<bool> JSReceiver::HasOwnProperty(Isolate* isolate,
                                       Handle<JSReceiver> object,
                                       Handle<Name> name) {
  if (object->IsJSModuleNamespace()) {
    PropertyDescriptor desc;
    return JSReceiver::GetOwnPropertyDescriptor(isolate, object, name, &desc);
  }

  if (object->IsJSObject()) {  // Shortcut.
    PropertyKey key(isolate, name);
    LookupIterator it(isolate, object, key, LookupIterator::OWN);
    return HasProperty(&it);
  }

  Maybe<PropertyAttributes> attributes =
      JSReceiver::GetOwnPropertyAttributes(object, name);
  MAYBE_RETURN(attributes, Nothing<bool>());
  return Just(attributes.FromJust() != ABSENT);
}

Handle<Object> JSReceiver::GetDataProperty(LookupIterator* it,
                                           AllocationPolicy allocation_policy) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::INTERCEPTOR:
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::ACCESS_CHECK:
        // Support calling this method without an active context, but refuse
        // access to access-checked objects in that case.
        if (!it->isolate()->context().is_null() && it->HasAccess()) continue;
        V8_FALLTHROUGH;
      case LookupIterator::JSPROXY:
        it->NotFound();
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::ACCESSOR:
        // TODO(verwaest): For now this doesn't call into AccessorInfo, since
        // clients don't need it. Update once relevant.
        it->NotFound();
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::DATA:
        return it->GetDataValue(allocation_policy);
    }
  }
  return it->isolate()->factory()->undefined_value();
}

// static
Maybe<bool> JSReceiver::HasInPrototypeChain(Isolate* isolate,
                                            Handle<JSReceiver> object,
                                            Handle<Object> proto) {
  PrototypeIterator iter(isolate, object, kStartAtReceiver);
  while (true) {
    if (!iter.AdvanceFollowingProxies()) return Nothing<bool>();
    if (iter.IsAtEnd()) return Just(false);
    if (PrototypeIterator::GetCurrent(iter).is_identical_to(proto)) {
      return Just(true);
    }
  }
}

// static
Maybe<bool> JSReceiver::CheckIfCanDefine(Isolate* isolate, LookupIterator* it,
                                         Handle<Object> value,
                                         Maybe<ShouldThrow> should_throw) {
  if (it->IsFound()) {
    Maybe<PropertyAttributes> attributes = GetPropertyAttributes(it);
    MAYBE_RETURN(attributes, Nothing<bool>());
    if ((attributes.FromJust() & DONT_DELETE) != 0) {
      RETURN_FAILURE(
          isolate, GetShouldThrow(isolate, should_throw),
          NewTypeError(MessageTemplate::kRedefineDisallowed, it->GetName()));
    }
  } else if (!JSObject::IsExtensible(
                 Handle<JSObject>::cast(it->GetReceiver()))) {
    RETURN_FAILURE(
        isolate, GetShouldThrow(isolate, should_throw),
        NewTypeError(MessageTemplate::kDefineDisallowed, it->GetName()));
  }
  return Just(true);
}

namespace {

bool HasExcludedProperty(
    const base::ScopedVector<Handle<Object>>* excluded_properties,
    Handle<Object> search_element) {
  // TODO(gsathya): Change this to be a hashtable.
  for (int i = 0; i < excluded_properties->length(); i++) {
    if (search_element->SameValue(*excluded_properties->at(i))) {
      return true;
    }
  }

  return false;
}

V8_WARN_UNUSED_RESULT Maybe<bool> FastAssign(
    Handle<JSReceiver> target, Handle<Object> source,
    PropertiesEnumerationMode mode,
    const base::ScopedVector<Handle<Object>>* excluded_properties,
    bool use_set) {
  // Non-empty strings are the only non-JSReceivers that need to be handled
  // explicitly by Object.assign.
  if (!source->IsJSReceiver()) {
    return Just(!source->IsString() || String::cast(*source).length() == 0);
  }

  Isolate* isolate = target->GetIsolate();

  // If the target is deprecated, the object will be updated on first store. If
  // the source for that store equals the target, this will invalidate the
  // cached representation of the source. Preventively upgrade the target.
  // Do this on each iteration since any property load could cause deprecation.
  if (target->map().is_deprecated()) {
    JSObject::MigrateInstance(isolate, Handle<JSObject>::cast(target));
  }

  Handle<Map> map(JSReceiver::cast(*source).map(), isolate);

  if (!map->IsJSObjectMap()) return Just(false);
  if (!map->OnlyHasSimpleProperties()) return Just(false);

  Handle<JSObject> from = Handle<JSObject>::cast(source);
  if (from->elements() != ReadOnlyRoots(isolate).empty_fixed_array()) {
    return Just(false);
  }

  // We should never try to copy properties from an object itself.
  CHECK_IMPLIES(!use_set, !target.is_identical_to(from));

  Handle<DescriptorArray> descriptors(map->instance_descriptors(isolate),
                                      isolate);

  bool stable = true;

  // Process symbols last and only do that if we found symbols.
  bool has_symbol = false;
  bool process_symbol_only = false;
  while (true) {
    for (InternalIndex i : map->IterateOwnDescriptors()) {
      HandleScope inner_scope(isolate);

      Handle<Name> next_key(descriptors->GetKey(i), isolate);
      if (mode == PropertiesEnumerationMode::kEnumerationOrder) {
        if (next_key->IsSymbol()) {
          has_symbol = true;
          if (!process_symbol_only) continue;
        } else {
          if (process_symbol_only) continue;
        }
      }
      Handle<Object> prop_value;
      // Directly decode from the descriptor array if |from| did not change
      // shape.
      if (stable) {
        DCHECK_EQ(from->map(), *map);
        DCHECK_EQ(*descriptors, map->instance_descriptors(isolate));

        PropertyDetails details = descriptors->GetDetails(i);
        if (!details.IsEnumerable()) continue;
        if (details.kind() == PropertyKind::kData) {
          if (details.location() == PropertyLocation::kDescriptor) {
            prop_value = handle(descriptors->GetStrongValue(i), isolate);
          } else {
            Representation representation = details.representation();
            FieldIndex index = FieldIndex::ForPropertyIndex(
                *map, details.field_index(), representation);
            prop_value =
                JSObject::FastPropertyAt(isolate, from, representation, index);
          }
        } else {
          LookupIterator it(isolate, from, next_key,
                            LookupIterator::OWN_SKIP_INTERCEPTOR);
          ASSIGN_RETURN_ON_EXCEPTION_VALUE(
              isolate, prop_value, Object::GetProperty(&it), Nothing<bool>());
          stable = from->map() == *map;
          descriptors.PatchValue(map->instance_descriptors(isolate));
        }
      } else {
        // If the map did change, do a slower lookup. We are still guaranteed
        // that the object has a simple shape, and that the key is a name.
        LookupIterator it(isolate, from, next_key, from,
                          LookupIterator::OWN_SKIP_INTERCEPTOR);
        if (!it.IsFound()) continue;
        DCHECK(it.state() == LookupIterator::DATA ||
               it.state() == LookupIterator::ACCESSOR);
        if (!it.IsEnumerable()) continue;
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, prop_value, Object::GetProperty(&it), Nothing<bool>());
      }

      if (use_set) {
        // The lookup will walk the prototype chain, so we have to be careful
        // to treat any key correctly for any receiver/holder.
        PropertyKey key(isolate, next_key);
        LookupIterator it(isolate, target, key);
        Maybe<bool> result =
            Object::SetProperty(&it, prop_value, StoreOrigin::kNamed,
                                Just(ShouldThrow::kThrowOnError));
        if (result.IsNothing()) return result;
        if (stable) {
          stable = from->map() == *map;
          descriptors.PatchValue(map->instance_descriptors(isolate));
        }
      } else {
        // No element indexes should get here or the exclusion check may
        // yield false negatives for type mismatch.
        if (excluded_properties != nullptr &&
            HasExcludedProperty(excluded_properties, next_key)) {
          continue;
        }

        // 4a ii 2. Perform ? CreateDataProperty(target, nextKey, propValue).
        // This is an OWN lookup, so constructing a named-mode LookupIterator
        // from {next_key} is safe.
        LookupIterator it(isolate, target, next_key, LookupIterator::OWN);
        CHECK(JSObject::CreateDataProperty(&it, prop_value, Just(kThrowOnError))
                  .FromJust());
      }
    }
    if (mode == PropertiesEnumerationMode::kEnumerationOrder) {
      if (process_symbol_only || !has_symbol) {
        return Just(true);
      }
      if (has_symbol) {
        process_symbol_only = true;
      }
    } else {
      DCHECK_EQ(mode, PropertiesEnumerationMode::kPropertyAdditionOrder);
      return Just(true);
    }
  }
  UNREACHABLE();
}
}  // namespace

// static
Maybe<bool> JSReceiver::SetOrCopyDataProperties(
    Isolate* isolate, Handle<JSReceiver> target, Handle<Object> source,
    PropertiesEnumerationMode mode,
    const base::ScopedVector<Handle<Object>>* excluded_properties,
    bool use_set) {
  Maybe<bool> fast_assign =
      FastAssign(target, source, mode, excluded_properties, use_set);
  if (fast_assign.IsNothing()) return Nothing<bool>();
  if (fast_assign.FromJust()) return Just(true);

  Handle<JSReceiver> from = Object::ToObject(isolate, source).ToHandleChecked();

  // 3b. Let keys be ? from.[[OwnPropertyKeys]]().
  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys,
      KeyAccumulator::GetKeys(from, KeyCollectionMode::kOwnOnly, ALL_PROPERTIES,
                              GetKeysConversion::kKeepNumbers),
      Nothing<bool>());

  if (!from->HasFastProperties() && target->HasFastProperties() &&
      !target->IsJSGlobalProxy()) {
    // JSProxy is always in slow-mode.
    DCHECK(!target->IsJSProxy());
    // Convert to slow properties if we're guaranteed to overflow the number of
    // descriptors.
    int source_length;
    if (from->IsJSGlobalObject()) {
      source_length = JSGlobalObject::cast(*from)
                          .global_dictionary(kAcquireLoad)
                          .NumberOfEnumerableProperties();
    } else if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
      source_length =
          from->property_dictionary_swiss().NumberOfEnumerableProperties();
    } else {
      source_length =
          from->property_dictionary().NumberOfEnumerableProperties();
    }
    if (source_length > kMaxNumberOfDescriptors) {
      JSObject::NormalizeProperties(isolate, Handle<JSObject>::cast(target),
                                    CLEAR_INOBJECT_PROPERTIES, source_length,
                                    "Copying data properties");
    }
  }

  // 4. Repeat for each element nextKey of keys in List order,
  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> next_key(keys->get(i), isolate);
    if (excluded_properties != nullptr &&
        HasExcludedProperty(excluded_properties, next_key)) {
      continue;
    }

    // 4a i. Let desc be ? from.[[GetOwnProperty]](nextKey).
    PropertyDescriptor desc;
    Maybe<bool> found =
        JSReceiver::GetOwnPropertyDescriptor(isolate, from, next_key, &desc);
    if (found.IsNothing()) return Nothing<bool>();
    // 4a ii. If desc is not undefined and desc.[[Enumerable]] is true, then
    if (found.FromJust() && desc.enumerable()) {
      // 4a ii 1. Let propValue be ? Get(from, nextKey).
      Handle<Object> prop_value;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, prop_value,
          Runtime::GetObjectProperty(isolate, from, next_key), Nothing<bool>());

      if (use_set) {
        // 4c ii 2. Let status be ? Set(to, nextKey, propValue, true).
        Handle<Object> status;
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, status,
            Runtime::SetObjectProperty(isolate, target, next_key, prop_value,
                                       StoreOrigin::kMaybeKeyed,
                                       Just(ShouldThrow::kThrowOnError)),
            Nothing<bool>());
      } else {
        // 4a ii 2. Perform ! CreateDataProperty(target, nextKey, propValue).
        PropertyKey key(isolate, next_key);
        LookupIterator it(isolate, target, key, LookupIterator::OWN);
        CHECK(JSObject::CreateDataProperty(&it, prop_value, Just(kThrowOnError))
                  .FromJust());
      }
    }
  }

  return Just(true);
}

String JSReceiver::class_name() {
  ReadOnlyRoots roots = GetReadOnlyRoots();
  if (IsFunction()) return roots.Function_string();
  if (IsJSArgumentsObject()) return roots.Arguments_string();
  if (IsJSArray()) return roots.Array_string();
  if (IsJSArrayBuffer()) {
    if (JSArrayBuffer::cast(*this).is_shared()) {
      return roots.SharedArrayBuffer_string();
    }
    return roots.ArrayBuffer_string();
  }
  if (IsJSArrayIterator()) return roots.ArrayIterator_string();
  if (IsJSDate()) return roots.Date_string();
  if (IsJSError()) return roots.Error_string();
  if (IsJSGeneratorObject()) return roots.Generator_string();
  if (IsJSMap()) return roots.Map_string();
  if (IsJSMapIterator()) return roots.MapIterator_string();
  if (IsJSProxy()) {
    return map().is_callable() ? roots.Function_string()
                               : roots.Object_string();
  }
  if (IsJSRegExp()) return roots.RegExp_string();
  if (IsJSSet()) return roots.Set_string();
  if (IsJSSetIterator()) return roots.SetIterator_string();
  if (IsJSTypedArray()) {
#define SWITCH_KIND(Type, type, TYPE, ctype)      \
  if (map().elements_kind() == TYPE##_ELEMENTS) { \
    return roots.Type##Array_string();            \
  }
    TYPED_ARRAYS(SWITCH_KIND)
#undef SWITCH_KIND
  }
  if (IsJSPrimitiveWrapper()) {
    Object value = JSPrimitiveWrapper::cast(*this).value();
    if (value.IsBoolean()) return roots.Boolean_string();
    if (value.IsString()) return roots.String_string();
    if (value.IsNumber()) return roots.Number_string();
    if (value.IsBigInt()) return roots.BigInt_string();
    if (value.IsSymbol()) return roots.Symbol_string();
    if (value.IsScript()) return roots.Script_string();
    UNREACHABLE();
  }
  if (IsJSWeakMap()) return roots.WeakMap_string();
  if (IsJSWeakSet()) return roots.WeakSet_string();
  if (IsJSGlobalProxy()) return roots.global_string();

  return roots.Object_string();
}

namespace {
std::pair<MaybeHandle<JSFunction>, Handle<String>> GetConstructorHelper(
    Isolate* isolate, Handle<JSReceiver> receiver) {
  // If the object was instantiated simply with base == new.target, the
  // constructor on the map provides the most accurate name.
  // Don't provide the info for prototypes, since their constructors are
  // reclaimed and replaced by Object in OptimizeAsPrototype.
  if (!receiver->IsJSProxy() && receiver->map().new_target_is_base() &&
      !receiver->map().is_prototype_map()) {
    Handle<Object> maybe_constructor(receiver->map().GetConstructor(), isolate);
    if (maybe_constructor->IsJSFunction()) {
      Handle<JSFunction> constructor =
          Handle<JSFunction>::cast(maybe_constructor);
      Handle<String> name =
          SharedFunctionInfo::DebugName(handle(constructor->shared(), isolate));
      if (name->length() != 0 &&
          !name->Equals(ReadOnlyRoots(isolate).Object_string())) {
        return std::make_pair(constructor, name);
      }
    }
  }

  for (PrototypeIterator it(isolate, receiver, kStartAtReceiver); !it.IsAtEnd();
       it.AdvanceIgnoringProxies()) {
    auto current = PrototypeIterator::GetCurrent<JSReceiver>(it);

    LookupIterator it_to_string_tag(
        isolate, receiver, isolate->factory()->to_string_tag_symbol(), current,
        LookupIterator::OWN_SKIP_INTERCEPTOR);
    auto maybe_to_string_tag = JSReceiver::GetDataProperty(
        &it_to_string_tag, AllocationPolicy::kAllocationDisallowed);
    if (maybe_to_string_tag->IsString()) {
      return std::make_pair(MaybeHandle<JSFunction>(),
                            Handle<String>::cast(maybe_to_string_tag));
    }

    // Consider the following example:
    //
    //   function A() {}
    //   function B() {}
    //   B.prototype = new A();
    //   B.prototype.constructor = B;
    //
    // The constructor name for `B.prototype` must yield "A", so we don't take
    // "constructor" into account for the receiver itself, but only starting
    // on the prototype chain.
    if (!receiver.is_identical_to(current)) {
      LookupIterator it_constructor(
          isolate, receiver, isolate->factory()->constructor_string(), current,
          LookupIterator::OWN_SKIP_INTERCEPTOR);
      auto maybe_constructor = JSReceiver::GetDataProperty(
          &it_constructor, AllocationPolicy::kAllocationDisallowed);
      if (maybe_constructor->IsJSFunction()) {
        auto constructor = Handle<JSFunction>::cast(maybe_constructor);
        auto name = SharedFunctionInfo::DebugName(
            handle(constructor->shared(), isolate));

        if (name->length() != 0 &&
            !name->Equals(ReadOnlyRoots(isolate).Object_string())) {
          return std::make_pair(constructor, name);
        }
      }
    }
  }

  return std::make_pair(MaybeHandle<JSFunction>(),
                        handle(receiver->class_name(), isolate));
}
}  // anonymous namespace

// static
MaybeHandle<JSFunction> JSReceiver::GetConstructor(
    Isolate* isolate, Handle<JSReceiver> receiver) {
  return GetConstructorHelper(isolate, receiver).first;
}

// static
Handle<String> JSReceiver::GetConstructorName(Isolate* isolate,
                                              Handle<JSReceiver> receiver) {
  return GetConstructorHelper(isolate, receiver).second;
}

MaybeHandle<NativeContext> JSReceiver::GetCreationContext() {
  JSReceiver receiver = *this;
  // Externals are JSObjects with null as a constructor.
  DCHECK(!receiver.IsJSExternalObject());
  Object constructor = receiver.map().GetConstructor();
  JSFunction function;
  if (constructor.IsJSFunction()) {
    function = JSFunction::cast(constructor);
  } else if (constructor.IsFunctionTemplateInfo()) {
    // Remote objects don't have a creation context.
    return MaybeHandle<NativeContext>();
  } else if (receiver.IsJSGeneratorObject()) {
    function = JSGeneratorObject::cast(receiver).function();
  } else if (receiver.IsJSFunction()) {
    function = JSFunction::cast(receiver);
  } else {
    return MaybeHandle<NativeContext>();
  }

  return function.has_context()
             ? Handle<NativeContext>(function.native_context(),
                                     receiver.GetIsolate())
             : MaybeHandle<NativeContext>();
}

// static
MaybeHandle<NativeContext> JSReceiver::GetFunctionRealm(
    Handle<JSReceiver> receiver) {
  Isolate* isolate = receiver->GetIsolate();
  // This is implemented as a loop because it's possible to construct very
  // long chains of bound functions or proxies where a recursive implementation
  // would run out of stack space.
  DisallowGarbageCollection no_gc;
  JSReceiver current = *receiver;
  do {
    DCHECK(current.map().is_constructor());
    if (current.IsJSProxy()) {
      JSProxy proxy = JSProxy::cast(current);
      if (proxy.IsRevoked()) {
        AllowGarbageCollection allow_allocating_errors;
        THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kProxyRevoked),
                        NativeContext);
      }
      current = JSReceiver::cast(proxy.target());
      continue;
    }
    if (current.IsJSFunction()) {
      JSFunction function = JSFunction::cast(current);
      return handle(function.native_context(), isolate);
    }
    if (current.IsJSBoundFunction()) {
      JSBoundFunction function = JSBoundFunction::cast(current);
      current = function.bound_target_function();
      continue;
    }
    if (current.IsJSWrappedFunction()) {
      JSWrappedFunction function = JSWrappedFunction::cast(current);
      current = function.wrapped_target_function();
      continue;
    }
    JSObject object = JSObject::cast(current);
    DCHECK(!object.IsJSFunction());
    return object.GetCreationContext();
  } while (true);
}

// static
MaybeHandle<NativeContext> JSReceiver::GetContextForMicrotask(
    Handle<JSReceiver> receiver) {
  Isolate* isolate = receiver->GetIsolate();
  while (receiver->IsJSBoundFunction() || receiver->IsJSProxy()) {
    if (receiver->IsJSBoundFunction()) {
      receiver = handle(
          Handle<JSBoundFunction>::cast(receiver)->bound_target_function(),
          isolate);
    } else {
      DCHECK(receiver->IsJSProxy());
      Handle<Object> target(Handle<JSProxy>::cast(receiver)->target(), isolate);
      if (!target->IsJSReceiver()) return MaybeHandle<NativeContext>();
      receiver = Handle<JSReceiver>::cast(target);
    }
  }

  if (!receiver->IsJSFunction()) return MaybeHandle<NativeContext>();
  return handle(Handle<JSFunction>::cast(receiver)->native_context(), isolate);
}

Maybe<PropertyAttributes> JSReceiver::GetPropertyAttributes(
    LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return JSProxy::GetPropertyAttributes(it);
      case LookupIterator::INTERCEPTOR: {
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithInterceptor(it);
        if (result.IsNothing()) return result;
        if (result.FromJust() != ABSENT) return result;
        break;
      }
      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess()) break;
        return JSObject::GetPropertyAttributesWithFailedAccessCheck(it);
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return Just(ABSENT);
      case LookupIterator::ACCESSOR:
        if (it->GetHolder<Object>()->IsJSModuleNamespace()) {
          return JSModuleNamespace::GetPropertyAttributes(it);
        } else {
          return Just(it->property_attributes());
        }
      case LookupIterator::DATA:
        return Just(it->property_attributes());
    }
  }
  return Just(ABSENT);
}

namespace {

Object SetHashAndUpdateProperties(HeapObject properties, int hash) {
  DCHECK_NE(PropertyArray::kNoHashSentinel, hash);
  DCHECK(PropertyArray::HashField::is_valid(hash));

  ReadOnlyRoots roots = properties.GetReadOnlyRoots();
  if (properties == roots.empty_fixed_array() ||
      properties == roots.empty_property_array() ||
      properties == roots.empty_property_dictionary() ||
      properties == roots.empty_swiss_property_dictionary()) {
    return Smi::FromInt(hash);
  }

  if (properties.IsPropertyArray()) {
    PropertyArray::cast(properties).SetHash(hash);
    DCHECK_LT(0, PropertyArray::cast(properties).length());
    return properties;
  }

  if (properties.IsGlobalDictionary()) {
    GlobalDictionary::cast(properties).SetHash(hash);
    return properties;
  }

  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    DCHECK(properties.IsSwissNameDictionary());
    SwissNameDictionary::cast(properties).SetHash(hash);
  } else {
    DCHECK(properties.IsNameDictionary());
    NameDictionary::cast(properties).SetHash(hash);
  }
  return properties;
}

int GetIdentityHashHelper(JSReceiver object) {
  DisallowGarbageCollection no_gc;
  Object properties = object.raw_properties_or_hash();
  if (properties.IsSmi()) {
    return Smi::ToInt(properties);
  }

  if (properties.IsPropertyArray()) {
    return PropertyArray::cast(properties).Hash();
  }
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL &&
      properties.IsSwissNameDictionary()) {
    return SwissNameDictionary::cast(properties).Hash();
  }

  if (properties.IsNameDictionary()) {
    DCHECK(!V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL);
    return NameDictionary::cast(properties).Hash();
  }

  if (properties.IsGlobalDictionary()) {
    return GlobalDictionary::cast(properties).Hash();
  }

#ifdef DEBUG
  ReadOnlyRoots roots = object.GetReadOnlyRoots();
  DCHECK(properties == roots.empty_fixed_array() ||
         properties == roots.empty_property_dictionary() ||
         properties == roots.empty_swiss_property_dictionary());
#endif

  return PropertyArray::kNoHashSentinel;
}
}  // namespace

void JSReceiver::SetIdentityHash(int hash) {
  DisallowGarbageCollection no_gc;
  DCHECK_NE(PropertyArray::kNoHashSentinel, hash);
  DCHECK(PropertyArray::HashField::is_valid(hash));

  HeapObject existing_properties = HeapObject::cast(raw_properties_or_hash());
  Object new_properties = SetHashAndUpdateProperties(existing_properties, hash);
  set_raw_properties_or_hash(new_properties, kRelaxedStore);
}

void JSReceiver::SetProperties(HeapObject properties) {
  DCHECK_IMPLIES(properties.IsPropertyArray() &&
                     PropertyArray::cast(properties).length() == 0,
                 properties == GetReadOnlyRoots().empty_property_array());
  DisallowGarbageCollection no_gc;
  int hash = GetIdentityHashHelper(*this);
  Object new_properties = properties;

  // TODO(cbruni): Make GetIdentityHashHelper return a bool so that we
  // don't have to manually compare against kNoHashSentinel.
  if (hash != PropertyArray::kNoHashSentinel) {
    new_properties = SetHashAndUpdateProperties(properties, hash);
  }

  set_raw_properties_or_hash(new_properties, kRelaxedStore);
}

Object JSReceiver::GetIdentityHash() {
  DisallowGarbageCollection no_gc;

  int hash = GetIdentityHashHelper(*this);
  if (hash == PropertyArray::kNoHashSentinel) {
    return GetReadOnlyRoots().undefined_value();
  }

  return Smi::FromInt(hash);
}

// static
Smi JSReceiver::CreateIdentityHash(Isolate* isolate, JSReceiver key) {
  DisallowGarbageCollection no_gc;
  int hash = isolate->GenerateIdentityHash(PropertyArray::HashField::kMax);
  DCHECK_NE(PropertyArray::kNoHashSentinel, hash);

  key.SetIdentityHash(hash);
  return Smi::FromInt(hash);
}

Smi JSReceiver::GetOrCreateIdentityHash(Isolate* isolate) {
  DisallowGarbageCollection no_gc;

  int hash = GetIdentityHashHelper(*this);
  if (hash != PropertyArray::kNoHashSentinel) {
    return Smi::FromInt(hash);
  }

  return JSReceiver::CreateIdentityHash(isolate, *this);
}

void JSReceiver::DeleteNormalizedProperty(Handle<JSReceiver> object,
                                          InternalIndex entry) {
  DCHECK(!object->HasFastProperties());
  Isolate* isolate = object->GetIsolate();
  DCHECK(entry.is_found());

  if (object->IsJSGlobalObject()) {
    // If we have a global object, invalidate the cell and remove it from the
    // global object's dictionary.
    Handle<GlobalDictionary> dictionary(
        JSGlobalObject::cast(*object).global_dictionary(kAcquireLoad), isolate);

    Handle<PropertyCell> cell(dictionary->CellAt(entry), isolate);

    Handle<GlobalDictionary> new_dictionary =
        GlobalDictionary::DeleteEntry(isolate, dictionary, entry);
    JSGlobalObject::cast(*object).set_global_dictionary(*new_dictionary,
                                                        kReleaseStore);

    cell->ClearAndInvalidate(ReadOnlyRoots(isolate));
  } else {
    if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
      Handle<SwissNameDictionary> dictionary(
          object->property_dictionary_swiss(), isolate);

      dictionary = SwissNameDictionary::DeleteEntry(isolate, dictionary, entry);
      object->SetProperties(*dictionary);
    } else {
      Handle<NameDictionary> dictionary(object->property_dictionary(), isolate);

      dictionary = NameDictionary::DeleteEntry(isolate, dictionary, entry);
      object->SetProperties(*dictionary);
    }
  }
  if (object->map().is_prototype_map()) {
    // Invalidate prototype validity cell as this may invalidate transitioning
    // store IC handlers.
    JSObject::InvalidatePrototypeChains(object->map());
  }
}

Maybe<bool> JSReceiver::DeleteProperty(LookupIterator* it,
                                       LanguageMode language_mode) {
  it->UpdateProtector();

  Isolate* isolate = it->isolate();

  if (it->state() == LookupIterator::JSPROXY) {
    return JSProxy::DeletePropertyOrElement(it->GetHolder<JSProxy>(),
                                            it->GetName(), language_mode);
  }

  if (it->GetReceiver()->IsJSProxy()) {
    if (it->state() != LookupIterator::NOT_FOUND) {
      DCHECK_EQ(LookupIterator::DATA, it->state());
      DCHECK(it->name()->IsPrivate());
      it->Delete();
    }
    return Just(true);
  }
  Handle<JSObject> receiver = Handle<JSObject>::cast(it->GetReceiver());

  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::JSPROXY:
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess()) break;
        isolate->ReportFailedAccessCheck(it->GetHolder<JSObject>());
        RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
        return Just(false);
      case LookupIterator::INTERCEPTOR: {
        ShouldThrow should_throw =
            is_sloppy(language_mode) ? kDontThrow : kThrowOnError;
        Maybe<bool> result =
            JSObject::DeletePropertyWithInterceptor(it, should_throw);
        // An exception was thrown in the interceptor. Propagate.
        if (isolate->has_pending_exception()) return Nothing<bool>();
        // Delete with interceptor succeeded. Return result.
        // TODO(neis): In strict mode, we should probably throw if the
        // interceptor returns false.
        if (result.IsJust()) return result;
        break;
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return Just(true);
      case LookupIterator::DATA:
      case LookupIterator::ACCESSOR: {
        Handle<JSObject> holder = it->GetHolder<JSObject>();
        if (!it->IsConfigurable() ||
            (holder->IsJSTypedArray() && it->IsElement(*holder))) {
          // Fail if the property is not configurable if the property is a
          // TypedArray element.
          if (is_strict(language_mode)) {
            isolate->Throw(*isolate->factory()->NewTypeError(
                MessageTemplate::kStrictDeleteProperty, it->GetName(),
                receiver));
            return Nothing<bool>();
          }
          return Just(false);
        }

        it->Delete();

        return Just(true);
      }
    }
  }

  return Just(true);
}

Maybe<bool> JSReceiver::DeleteElement(Handle<JSReceiver> object, uint32_t index,
                                      LanguageMode language_mode) {
  LookupIterator it(object->GetIsolate(), object, index, object,
                    LookupIterator::OWN);
  return DeleteProperty(&it, language_mode);
}

Maybe<bool> JSReceiver::DeleteProperty(Handle<JSReceiver> object,
                                       Handle<Name> name,
                                       LanguageMode language_mode) {
  LookupIterator it(object->GetIsolate(), object, name, object,
                    LookupIterator::OWN);
  return DeleteProperty(&it, language_mode);
}

Maybe<bool> JSReceiver::DeletePropertyOrElement(Handle<JSReceiver> object,
                                                Handle<Name> name,
                                                LanguageMode language_mode) {
  Isolate* isolate = object->GetIsolate();
  PropertyKey key(isolate, name);
  LookupIterator it(isolate, object, key, object, LookupIterator::OWN);
  return DeleteProperty(&it, language_mode);
}

// ES6 19.1.2.4
// static
Object JSReceiver::DefineProperty(Isolate* isolate, Handle<Object> object,
                                  Handle<Object> key,
                                  Handle<Object> attributes) {
  // 1. If Type(O) is not Object, throw a TypeError exception.
  if (!object->IsJSReceiver()) {
    Handle<String> fun_name =
        isolate->factory()->InternalizeUtf8String("Object.defineProperty");
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject, fun_name));
  }
  // 2. Let key be ToPropertyKey(P).
  // 3. ReturnIfAbrupt(key).
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, key,
                                     Object::ToPropertyKey(isolate, key));
  // 4. Let desc be ToPropertyDescriptor(Attributes).
  // 5. ReturnIfAbrupt(desc).
  PropertyDescriptor desc;
  if (!PropertyDescriptor::ToPropertyDescriptor(isolate, attributes, &desc)) {
    return ReadOnlyRoots(isolate).exception();
  }
  // 6. Let success be DefinePropertyOrThrow(O,key, desc).
  Maybe<bool> success =
      DefineOwnProperty(isolate, Handle<JSReceiver>::cast(object), key, &desc,
                        Just(kThrowOnError));
  // 7. ReturnIfAbrupt(success).
  MAYBE_RETURN(success, ReadOnlyRoots(isolate).exception());
  CHECK(success.FromJust());
  // 8. Return O.
  return *object;
}

// ES6 19.1.2.3.1
// static
MaybeHandle<Object> JSReceiver::DefineProperties(Isolate* isolate,
                                                 Handle<Object> object,
                                                 Handle<Object> properties) {
  // 1. If Type(O) is not Object, throw a TypeError exception.
  if (!object->IsJSReceiver()) {
    Handle<String> fun_name =
        isolate->factory()->InternalizeUtf8String("Object.defineProperties");
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCalledOnNonObject, fun_name),
                    Object);
  }
  // 2. Let props be ToObject(Properties).
  // 3. ReturnIfAbrupt(props).
  Handle<JSReceiver> props;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, props,
                             Object::ToObject(isolate, properties), Object);

  // 4. Let keys be props.[[OwnPropertyKeys]]().
  // 5. ReturnIfAbrupt(keys).
  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, keys,
      KeyAccumulator::GetKeys(props, KeyCollectionMode::kOwnOnly,
                              ALL_PROPERTIES),
      Object);
  // 6. Let descriptors be an empty List.
  int capacity = keys->length();
  std::vector<PropertyDescriptor> descriptors(capacity);
  size_t descriptors_index = 0;
  // 7. Repeat for each element nextKey of keys in List order,
  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> next_key(keys->get(i), isolate);
    // 7a. Let propDesc be props.[[GetOwnProperty]](nextKey).
    // 7b. ReturnIfAbrupt(propDesc).
    PropertyKey key(isolate, next_key);
    LookupIterator it(isolate, props, key, LookupIterator::OWN);
    Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
    if (maybe.IsNothing()) return MaybeHandle<Object>();
    PropertyAttributes attrs = maybe.FromJust();
    // 7c. If propDesc is not undefined and propDesc.[[Enumerable]] is true:
    if (attrs == ABSENT) continue;
    if (attrs & DONT_ENUM) continue;
    // 7c i. Let descObj be Get(props, nextKey).
    // 7c ii. ReturnIfAbrupt(descObj).
    Handle<Object> desc_obj;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, desc_obj, Object::GetProperty(&it),
                               Object);
    // 7c iii. Let desc be ToPropertyDescriptor(descObj).
    bool success = PropertyDescriptor::ToPropertyDescriptor(
        isolate, desc_obj, &descriptors[descriptors_index]);
    // 7c iv. ReturnIfAbrupt(desc).
    if (!success) return MaybeHandle<Object>();
    // 7c v. Append the pair (a two element List) consisting of nextKey and
    //       desc to the end of descriptors.
    descriptors[descriptors_index].set_name(next_key);
    descriptors_index++;
  }
  // 8. For each pair from descriptors in list order,
  for (size_t i = 0; i < descriptors_index; ++i) {
    PropertyDescriptor* desc = &descriptors[i];
    // 8a. Let P be the first element of pair.
    // 8b. Let desc be the second element of pair.
    // 8c. Let status be DefinePropertyOrThrow(O, P, desc).
    Maybe<bool> status =
        DefineOwnProperty(isolate, Handle<JSReceiver>::cast(object),
                          desc->name(), desc, Just(kThrowOnError));
    // 8d. ReturnIfAbrupt(status).
    if (status.IsNothing()) return MaybeHandle<Object>();
    CHECK(status.FromJust());
  }
  // 9. Return o.
  return object;
}

// static
Maybe<bool> JSReceiver::DefineOwnProperty(Isolate* isolate,
                                          Handle<JSReceiver> object,
                                          Handle<Object> key,
                                          PropertyDescriptor* desc,
                                          Maybe<ShouldThrow> should_throw) {
  if (object->IsJSArray()) {
    return JSArray::DefineOwnProperty(isolate, Handle<JSArray>::cast(object),
                                      key, desc, should_throw);
  }
  if (object->IsJSProxy()) {
    return JSProxy::DefineOwnProperty(isolate, Handle<JSProxy>::cast(object),
                                      key, desc, should_throw);
  }
  if (object->IsJSTypedArray()) {
    return JSTypedArray::DefineOwnProperty(
        isolate, Handle<JSTypedArray>::cast(object), key, desc, should_throw);
  }
  if (object->IsJSModuleNamespace()) {
    return JSModuleNamespace::DefineOwnProperty(
        isolate, Handle<JSModuleNamespace>::cast(object), key, desc,
        should_throw);
  }

  // OrdinaryDefineOwnProperty, by virtue of calling
  // DefineOwnPropertyIgnoreAttributes, can handle arguments
  // (ES#sec-arguments-exotic-objects-defineownproperty-p-desc).
  return OrdinaryDefineOwnProperty(isolate, Handle<JSObject>::cast(object), key,
                                   desc, should_throw);
}

// static
Maybe<bool> JSReceiver::OrdinaryDefineOwnProperty(
    Isolate* isolate, Handle<JSObject> object, Handle<Object> key,
    PropertyDescriptor* desc, Maybe<ShouldThrow> should_throw) {
  DCHECK(key->IsName() || key->IsNumber());  // |key| is a PropertyKey.
  PropertyKey lookup_key(isolate, key);
  return OrdinaryDefineOwnProperty(isolate, object, lookup_key, desc,
                                   should_throw);
}

Maybe<bool> JSReceiver::OrdinaryDefineOwnProperty(
    Isolate* isolate, Handle<JSObject> object, const PropertyKey& key,
    PropertyDescriptor* desc, Maybe<ShouldThrow> should_throw) {
  LookupIterator it(isolate, object, key, LookupIterator::OWN);

  // Deal with access checks first.
  if (it.state() == LookupIterator::ACCESS_CHECK) {
    if (!it.HasAccess()) {
      isolate->ReportFailedAccessCheck(it.GetHolder<JSObject>());
      RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
      return Just(true);
    }
    it.Next();
  }

  return OrdinaryDefineOwnProperty(&it, desc, should_throw);
}

namespace {

MaybeHandle<Object> GetPropertyWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor, bool* done) {
  *done = false;
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  if (interceptor->getter().IsUndefined(isolate)) {
    return isolate->factory()->undefined_value();
  }

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  Handle<Object> result;
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, receiver, Object::ConvertReceiver(isolate, receiver), Object);
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, Just(kDontThrow));

  if (it->IsElement(*holder)) {
    result = args.CallIndexedGetter(interceptor, it->array_index());
  } else {
    result = args.CallNamedGetter(interceptor, it->name());
  }

  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  if (result.is_null()) return isolate->factory()->undefined_value();
  *done = true;
  // Rebox handle before return
  return handle(*result, isolate);
}

Maybe<PropertyAttributes> GetPropertyAttributesWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc(isolate);
  HandleScope scope(isolate);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  DCHECK_IMPLIES(!it->IsElement(*holder) && it->name()->IsSymbol(),
                 interceptor->can_intercept_symbols());
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<PropertyAttributes>());
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, Just(kDontThrow));
  if (!interceptor->query().IsUndefined(isolate)) {
    Handle<Object> result;
    if (it->IsElement(*holder)) {
      result = args.CallIndexedQuery(interceptor, it->array_index());
    } else {
      result = args.CallNamedQuery(interceptor, it->name());
    }
    if (!result.is_null()) {
      int32_t value;
      CHECK(result->ToInt32(&value));
      DCHECK_IMPLIES((value & ~PropertyAttributes::ALL_ATTRIBUTES_MASK) != 0,
                     value == PropertyAttributes::ABSENT);
      return Just(static_cast<PropertyAttributes>(value));
    }
  } else if (!interceptor->getter().IsUndefined(isolate)) {
    // TODO(verwaest): Use GetPropertyWithInterceptor?
    Handle<Object> result;
    if (it->IsElement(*holder)) {
      result = args.CallIndexedGetter(interceptor, it->array_index());
    } else {
      result = args.CallNamedGetter(interceptor, it->name());
    }
    if (!result.is_null()) return Just(DONT_ENUM);
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<PropertyAttributes>());
  return Just(ABSENT);
}

Maybe<bool> SetPropertyWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor,
    Maybe<ShouldThrow> should_throw, Handle<Object> value) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  if (interceptor->setter().IsUndefined(isolate)) return Just(false);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  bool result;
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, should_throw);

  if (it->IsElement(*holder)) {
    // TODO(neis): In the future, we may want to actually return the
    // interceptor's result, which then should be a boolean.
    result = !args.CallIndexedSetter(interceptor, it->array_index(), value)
                  .is_null();
  } else {
    result = !args.CallNamedSetter(interceptor, it->name(), value).is_null();
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
  return Just(result);
}

Maybe<bool> DefinePropertyWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor,
    Maybe<ShouldThrow> should_throw, PropertyDescriptor* desc) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  if (interceptor->definer().IsUndefined(isolate)) return Just(false);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  bool result;
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, should_throw);

  std::unique_ptr<v8::PropertyDescriptor> descriptor(
      new v8::PropertyDescriptor());
  if (PropertyDescriptor::IsAccessorDescriptor(desc)) {
    descriptor.reset(new v8::PropertyDescriptor(
        v8::Utils::ToLocal(desc->get()), v8::Utils::ToLocal(desc->set())));
  } else if (PropertyDescriptor::IsDataDescriptor(desc)) {
    if (desc->has_writable()) {
      descriptor.reset(new v8::PropertyDescriptor(
          v8::Utils::ToLocal(desc->value()), desc->writable()));
    } else {
      descriptor.reset(
          new v8::PropertyDescriptor(v8::Utils::ToLocal(desc->value())));
    }
  }
  if (desc->has_enumerable()) {
    descriptor->set_enumerable(desc->enumerable());
  }
  if (desc->has_configurable()) {
    descriptor->set_configurable(desc->configurable());
  }

  if (it->IsElement(*holder)) {
    result =
        !args.CallIndexedDefiner(interceptor, it->array_index(), *descriptor)
             .is_null();
  } else {
    result =
        !args.CallNamedDefiner(interceptor, it->name(), *descriptor).is_null();
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
  return Just(result);
}

}  // namespace

// ES6 9.1.6.1
// static
Maybe<bool> JSReceiver::OrdinaryDefineOwnProperty(
    LookupIterator* it, PropertyDescriptor* desc,
    Maybe<ShouldThrow> should_throw) {
  Isolate* isolate = it->isolate();
  // 1. Let current be O.[[GetOwnProperty]](P).
  // 2. ReturnIfAbrupt(current).
  PropertyDescriptor current;
  MAYBE_RETURN(GetOwnPropertyDescriptor(it, &current), Nothing<bool>());

  it->Restart();
  // Handle interceptor
  for (; it->IsFound(); it->Next()) {
    if (it->state() == LookupIterator::INTERCEPTOR) {
      if (it->HolderIsReceiverOrHiddenPrototype()) {
        Maybe<bool> result = DefinePropertyWithInterceptorInternal(
            it, it->GetInterceptor(), should_throw, desc);
        if (result.IsNothing() || result.FromJust()) {
          return result;
        }
      }
    }
  }

  // TODO(jkummerow/verwaest): It would be nice if we didn't have to reset
  // the iterator every time. Currently, the reasons why we need it are:
  // - handle interceptors correctly
  // - handle accessors correctly (which might change the holder's map)
  it->Restart();
  // 3. Let extensible be the value of the [[Extensible]] internal slot of O.
  Handle<JSObject> object = Handle<JSObject>::cast(it->GetReceiver());
  bool extensible = JSObject::IsExtensible(object);

  return ValidateAndApplyPropertyDescriptor(
      isolate, it, extensible, desc, &current, should_throw, Handle<Name>());
}

// ES6 9.1.6.2
// static
Maybe<bool> JSReceiver::IsCompatiblePropertyDescriptor(
    Isolate* isolate, bool extensible, PropertyDescriptor* desc,
    PropertyDescriptor* current, Handle<Name> property_name,
    Maybe<ShouldThrow> should_throw) {
  // 1. Return ValidateAndApplyPropertyDescriptor(undefined, undefined,
  //    Extensible, Desc, Current).
  return ValidateAndApplyPropertyDescriptor(
      isolate, nullptr, extensible, desc, current, should_throw, property_name);
}

// https://tc39.es/ecma262/#sec-validateandapplypropertydescriptor
// static
Maybe<bool> JSReceiver::ValidateAndApplyPropertyDescriptor(
    Isolate* isolate, LookupIterator* it, bool extensible,
    PropertyDescriptor* desc, PropertyDescriptor* current,
    Maybe<ShouldThrow> should_throw, Handle<Name> property_name) {
  // We either need a LookupIterator, or a property name.
  DCHECK((it == nullptr) != property_name.is_null());
  Handle<JSObject> object;
  if (it != nullptr) object = Handle<JSObject>::cast(it->GetReceiver());
  bool desc_is_data_descriptor = PropertyDescriptor::IsDataDescriptor(desc);
  bool desc_is_accessor_descriptor =
      PropertyDescriptor::IsAccessorDescriptor(desc);
  bool desc_is_generic_descriptor =
      PropertyDescriptor::IsGenericDescriptor(desc);
  // 1. (Assert)
  // 2. If current is undefined, then
  if (current->is_empty()) {
    // 2a. If extensible is false, return false.
    if (!extensible) {
      RETURN_FAILURE(
          isolate, GetShouldThrow(isolate, should_throw),
          NewTypeError(MessageTemplate::kDefineDisallowed,
                       it != nullptr ? it->GetName() : property_name));
    }
    // 2c. If IsGenericDescriptor(Desc) or IsDataDescriptor(Desc) is true, then:
    // (This is equivalent to !IsAccessorDescriptor(desc).)
    DCHECK_EQ(desc_is_generic_descriptor || desc_is_data_descriptor,
              !desc_is_accessor_descriptor);
    if (!desc_is_accessor_descriptor) {
      // 2c i. If O is not undefined, create an own data property named P of
      // object O whose [[Value]], [[Writable]], [[Enumerable]] and
      // [[Configurable]] attribute values are described by Desc. If the value
      // of an attribute field of Desc is absent, the attribute of the newly
      // created property is set to its default value.
      if (it != nullptr) {
        if (!desc->has_writable()) desc->set_writable(false);
        if (!desc->has_enumerable()) desc->set_enumerable(false);
        if (!desc->has_configurable()) desc->set_configurable(false);
        Handle<Object> value(
            desc->has_value()
                ? desc->value()
                : Handle<Object>::cast(isolate->factory()->undefined_value()));
        MaybeHandle<Object> result =
            JSObject::DefineOwnPropertyIgnoreAttributes(it, value,
                                                        desc->ToAttributes());
        if (result.is_null()) return Nothing<bool>();
      }
    } else {
      // 2d. Else Desc must be an accessor Property Descriptor,
      DCHECK(desc_is_accessor_descriptor);
      // 2d i. If O is not undefined, create an own accessor property named P
      // of object O whose [[Get]], [[Set]], [[Enumerable]] and
      // [[Configurable]] attribute values are described by Desc. If the value
      // of an attribute field of Desc is absent, the attribute of the newly
      // created property is set to its default value.
      if (it != nullptr) {
        if (!desc->has_enumerable()) desc->set_enumerable(false);
        if (!desc->has_configurable()) desc->set_configurable(false);
        Handle<Object> getter(
            desc->has_get()
                ? desc->get()
                : Handle<Object>::cast(isolate->factory()->null_value()));
        Handle<Object> setter(
            desc->has_set()
                ? desc->set()
                : Handle<Object>::cast(isolate->factory()->null_value()));
        MaybeHandle<Object> result =
            JSObject::DefineAccessor(it, getter, setter, desc->ToAttributes());
        if (result.is_null()) return Nothing<bool>();
      }
    }
    // 2e. Return true.
    return Just(true);
  }
  // 3. If every field in Desc is absent, return true. (This also has a shortcut
  // not in the spec: if every field value matches the current value, return.)
  if ((!desc->has_enumerable() ||
       desc->enumerable() == current->enumerable()) &&
      (!desc->has_configurable() ||
       desc->configurable() == current->configurable()) &&
      (!desc->has_value() ||
       (current->has_value() && current->value()->SameValue(*desc->value()))) &&
      (!desc->has_writable() ||
       (current->has_writable() && current->writable() == desc->writable())) &&
      (!desc->has_get() ||
       (current->has_get() && current->get()->SameValue(*desc->get()))) &&
      (!desc->has_set() ||
       (current->has_set() && current->set()->SameValue(*desc->set())))) {
    return Just(true);
  }
  // 4. If current.[[Configurable]] is false, then
  if (!current->configurable()) {
    // 4a. If Desc.[[Configurable]] is present and its value is true, return
    // false.
    if (desc->has_configurable() && desc->configurable()) {
      RETURN_FAILURE(
          isolate, GetShouldThrow(isolate, should_throw),
          NewTypeError(MessageTemplate::kRedefineDisallowed,
                       it != nullptr ? it->GetName() : property_name));
    }
    // 4b. If Desc.[[Enumerable]] is present and
    // ! SameValue(Desc.[[Enumerable]], current.[[Enumerable]]) is false, return
    // false.
    if (desc->has_enumerable() && desc->enumerable() != current->enumerable()) {
      RETURN_FAILURE(
          isolate, GetShouldThrow(isolate, should_throw),
          NewTypeError(MessageTemplate::kRedefineDisallowed,
                       it != nullptr ? it->GetName() : property_name));
    }
  }

  bool current_is_data_descriptor =
      PropertyDescriptor::IsDataDescriptor(current);
  // 5. If ! IsGenericDescriptor(Desc) is true, no further validation is
  // required.
  if (desc_is_generic_descriptor) {
    // Nothing to see here.

    // 6. Else if ! SameValue(!IsDataDescriptor(current),
    // !IsDataDescriptor(Desc)) is false, the
  } else if (current_is_data_descriptor != desc_is_data_descriptor) {
    // 6a. If current.[[Configurable]] is false, return false.
    if (!current->configurable()) {
      RETURN_FAILURE(
          isolate, GetShouldThrow(isolate, should_throw),
          NewTypeError(MessageTemplate::kRedefineDisallowed,
                       it != nullptr ? it->GetName() : property_name));
    }
    // 6b. If IsDataDescriptor(current) is true, then:
    if (current_is_data_descriptor) {
      // 6b i. If O is not undefined, convert the property named P of object O
      // from a data property to an accessor property. Preserve the existing
      // values of the converted property's [[Configurable]] and [[Enumerable]]
      // attributes and set the rest of the property's attributes to their
      // default values.
      // --> Folded into step 9
    } else {
      // 6c i. If O is not undefined, convert the property named P of object O
      // from an accessor property to a data property. Preserve the existing
      // values of the converted property’s [[Configurable]] and [[Enumerable]]
      // attributes and set the rest of the property’s attributes to their
      // default values.
      // --> Folded into step 9
    }

    // 7. Else if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both
    // true, then:
  } else if (current_is_data_descriptor && desc_is_data_descriptor) {
    // 7a. If current.[[Configurable]] is false and current.[[Writable]] is
    // false, then
    if (!current->configurable() && !current->writable()) {
      // 7a i. If Desc.[[Writable]] is present and Desc.[[Writable]] is true,
      // return false.
      if (desc->has_writable() && desc->writable()) {
        RETURN_FAILURE(
            isolate, GetShouldThrow(isolate, should_throw),
            NewTypeError(MessageTemplate::kRedefineDisallowed,
                         it != nullptr ? it->GetName() : property_name));
      }
      // 7a ii. If Desc.[[Value]] is present and SameValue(Desc.[[Value]],
      // current.[[Value]]) is false, return false.
      if (desc->has_value() && !desc->value()->SameValue(*current->value())) {
        RETURN_FAILURE(
            isolate, GetShouldThrow(isolate, should_throw),
            NewTypeError(MessageTemplate::kRedefineDisallowed,
                         it != nullptr ? it->GetName() : property_name));
      }
    }
  } else {
    // 8. Else,
    // 8a. Assert: ! IsAccessorDescriptor(current) and
    // ! IsAccessorDescriptor(Desc) are both true.
    DCHECK(PropertyDescriptor::IsAccessorDescriptor(current) &&
           desc_is_accessor_descriptor);
    // 8b. If current.[[Configurable]] is false, then:
    if (!current->configurable()) {
      // 8a i. If Desc.[[Set]] is present and SameValue(Desc.[[Set]],
      // current.[[Set]]) is false, return false.
      if (desc->has_set() && !desc->set()->SameValue(*current->set())) {
        RETURN_FAILURE(
            isolate, GetShouldThrow(isolate, should_throw),
            NewTypeError(MessageTemplate::kRedefineDisallowed,
                         it != nullptr ? it->GetName() : property_name));
      }
      // 8a ii. If Desc.[[Get]] is present and SameValue(Desc.[[Get]],
      // current.[[Get]]) is false, return false.
      if (desc->has_get() && !desc->get()->SameValue(*current->get())) {
        RETURN_FAILURE(
            isolate, GetShouldThrow(isolate, should_throw),
            NewTypeError(MessageTemplate::kRedefineDisallowed,
                         it != nullptr ? it->GetName() : property_name));
      }
    }
  }

  // 9. If O is not undefined, then:
  if (it != nullptr) {
    // 9a. For each field of Desc that is present, set the corresponding
    // attribute of the property named P of object O to the value of the field.
    PropertyAttributes attrs = NONE;

    if (desc->has_enumerable()) {
      attrs = static_cast<PropertyAttributes>(
          attrs | (desc->enumerable() ? NONE : DONT_ENUM));
    } else {
      attrs = static_cast<PropertyAttributes>(
          attrs | (current->enumerable() ? NONE : DONT_ENUM));
    }
    if (desc->has_configurable()) {
      attrs = static_cast<PropertyAttributes>(
          attrs | (desc->configurable() ? NONE : DONT_DELETE));
    } else {
      attrs = static_cast<PropertyAttributes>(
          attrs | (current->configurable() ? NONE : DONT_DELETE));
    }
    if (desc_is_data_descriptor ||
        (desc_is_generic_descriptor && current_is_data_descriptor)) {
      if (desc->has_writable()) {
        attrs = static_cast<PropertyAttributes>(
            attrs | (desc->writable() ? NONE : READ_ONLY));
      } else {
        attrs = static_cast<PropertyAttributes>(
            attrs | (current->writable() ? NONE : READ_ONLY));
      }
      Handle<Object> value(
          desc->has_value() ? desc->value()
                            : current->has_value()
                                  ? current->value()
                                  : Handle<Object>::cast(
                                        isolate->factory()->undefined_value()));
      return JSObject::DefineOwnPropertyIgnoreAttributes(it, value, attrs,
                                                         should_throw);
    } else {
      DCHECK(desc_is_accessor_descriptor ||
             (desc_is_generic_descriptor &&
              PropertyDescriptor::IsAccessorDescriptor(current)));
      Handle<Object> getter(
          desc->has_get()
              ? desc->get()
              : current->has_get()
                    ? current->get()
                    : Handle<Object>::cast(isolate->factory()->null_value()));
      Handle<Object> setter(
          desc->has_set()
              ? desc->set()
              : current->has_set()
                    ? current->set()
                    : Handle<Object>::cast(isolate->factory()->null_value()));
      MaybeHandle<Object> result =
          JSObject::DefineAccessor(it, getter, setter, attrs);
      if (result.is_null()) return Nothing<bool>();
    }
  }

  // 10. Return true.
  return Just(true);
}

// static
Maybe<bool> JSReceiver::CreateDataProperty(Isolate* isolate,
                                           Handle<JSReceiver> object,
                                           Handle<Name> key,
                                           Handle<Object> value,
                                           Maybe<ShouldThrow> should_throw) {
  PropertyKey lookup_key(isolate, key);
  LookupIterator it(isolate, object, lookup_key, LookupIterator::OWN);
  return CreateDataProperty(&it, value, should_throw);
}

// static
Maybe<bool> JSReceiver::CreateDataProperty(LookupIterator* it,
                                           Handle<Object> value,
                                           Maybe<ShouldThrow> should_throw) {
  DCHECK(!it->check_prototype_chain());
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());
  Isolate* isolate = receiver->GetIsolate();

  if (receiver->IsJSObject()) {
    return JSObject::CreateDataProperty(it, value, should_throw);  // Shortcut.
  }

  PropertyDescriptor new_desc;
  new_desc.set_value(value);
  new_desc.set_writable(true);
  new_desc.set_enumerable(true);
  new_desc.set_configurable(true);

  return JSReceiver::DefineOwnProperty(isolate, receiver, it->GetName(),
                                       &new_desc, should_throw);
}

// static
Maybe<bool> JSReceiver::AddPrivateField(LookupIterator* it,
                                        Handle<Object> value,
                                        Maybe<ShouldThrow> should_throw) {
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());
  Isolate* isolate = receiver->GetIsolate();
  DCHECK(it->GetName()->IsPrivateName());
  Handle<Symbol> symbol = Handle<Symbol>::cast(it->GetName());

  switch (it->state()) {
    case LookupIterator::JSPROXY: {
      PropertyDescriptor new_desc;
      new_desc.set_value(value);
      new_desc.set_writable(true);
      new_desc.set_enumerable(true);
      new_desc.set_configurable(true);
      return JSProxy::SetPrivateSymbol(isolate, Handle<JSProxy>::cast(receiver),
                                       symbol, &new_desc, should_throw);
    }
    case LookupIterator::DATA:
    case LookupIterator::INTERCEPTOR:
    case LookupIterator::ACCESSOR:
    case LookupIterator::INTEGER_INDEXED_EXOTIC:
      UNREACHABLE();

    case LookupIterator::ACCESS_CHECK: {
      if (!it->HasAccess()) {
        it->isolate()->ReportFailedAccessCheck(it->GetHolder<JSObject>());
        RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
        return Just(true);
      }
      break;
    }

    case LookupIterator::TRANSITION:
    case LookupIterator::NOT_FOUND:
      break;
  }

  return Object::TransitionAndWriteDataProperty(it, value, NONE, should_throw,
                                                StoreOrigin::kMaybeKeyed);
}

// static
Maybe<bool> JSReceiver::GetOwnPropertyDescriptor(Isolate* isolate,
                                                 Handle<JSReceiver> object,
                                                 Handle<Object> key,
                                                 PropertyDescriptor* desc) {
  DCHECK(key->IsName() || key->IsNumber());  // |key| is a PropertyKey.
  PropertyKey lookup_key(isolate, key);
  LookupIterator it(isolate, object, lookup_key, LookupIterator::OWN);
  return GetOwnPropertyDescriptor(&it, desc);
}

namespace {

Maybe<bool> GetPropertyDescriptorWithInterceptor(LookupIterator* it,
                                                 PropertyDescriptor* desc) {
  Handle<InterceptorInfo> interceptor;

  if (it->state() == LookupIterator::ACCESS_CHECK) {
    if (it->HasAccess()) {
      it->Next();
    } else {
      interceptor = it->GetInterceptorForFailedAccessCheck();
      if (interceptor.is_null() &&
          (!JSObject::AllCanRead(it) ||
           it->state() != LookupIterator::INTERCEPTOR)) {
        it->Restart();
        return Just(false);
      }
    }
  }

  if (it->state() == LookupIterator::INTERCEPTOR) {
    interceptor = it->GetInterceptor();
  }
  if (interceptor.is_null()) return Just(false);
  Isolate* isolate = it->isolate();
  if (interceptor->descriptor().IsUndefined(isolate)) return Just(false);

  Handle<Object> result;
  Handle<JSObject> holder = it->GetHolder<JSObject>();

  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }

  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, Just(kDontThrow));
  if (it->IsElement(*holder)) {
    result = args.CallIndexedDescriptor(interceptor, it->array_index());
  } else {
    result = args.CallNamedDescriptor(interceptor, it->name());
  }
  // An exception was thrown in the interceptor. Propagate.
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
  if (!result.is_null()) {
    // Request successfully intercepted, try to set the property
    // descriptor.
    Utils::ApiCheck(
        PropertyDescriptor::ToPropertyDescriptor(isolate, result, desc),
        it->IsElement(*holder) ? "v8::IndexedPropertyDescriptorCallback"
                               : "v8::NamedPropertyDescriptorCallback",
        "Invalid property descriptor.");

    return Just(true);
  }

  it->Next();
  return Just(false);
}
}  // namespace

// ES6 9.1.5.1
// Returns true on success, false if the property didn't exist, nothing if
// an exception was thrown.
// static
Maybe<bool> JSReceiver::GetOwnPropertyDescriptor(LookupIterator* it,
                                                 PropertyDescriptor* desc) {
  Isolate* isolate = it->isolate();
  // "Virtual" dispatch.
  if (it->IsFound() && it->GetHolder<JSReceiver>()->IsJSProxy()) {
    return JSProxy::GetOwnPropertyDescriptor(isolate, it->GetHolder<JSProxy>(),
                                             it->GetName(), desc);
  }

  Maybe<bool> intercepted = GetPropertyDescriptorWithInterceptor(it, desc);
  MAYBE_RETURN(intercepted, Nothing<bool>());
  if (intercepted.FromJust()) {
    return Just(true);
  }

  // Request was not intercepted, continue as normal.
  // 1. (Assert)
  // 2. If O does not have an own property with key P, return undefined.
  Maybe<PropertyAttributes> maybe = JSObject::GetPropertyAttributes(it);
  MAYBE_RETURN(maybe, Nothing<bool>());
  PropertyAttributes attrs = maybe.FromJust();
  if (attrs == ABSENT) return Just(false);
  DCHECK(!isolate->has_pending_exception());

  // 3. Let D be a newly created Property Descriptor with no fields.
  DCHECK(desc->is_empty());
  // 4. Let X be O's own property whose key is P.
  // 5. If X is a data property, then
  bool is_accessor_pair = it->state() == LookupIterator::ACCESSOR &&
                          it->GetAccessors()->IsAccessorPair();
  if (!is_accessor_pair) {
    // 5a. Set D.[[Value]] to the value of X's [[Value]] attribute.
    Handle<Object> value;
    if (!Object::GetProperty(it).ToHandle(&value)) {
      DCHECK(isolate->has_pending_exception());
      return Nothing<bool>();
    }
    desc->set_value(value);
    // 5b. Set D.[[Writable]] to the value of X's [[Writable]] attribute
    desc->set_writable((attrs & READ_ONLY) == 0);
  } else {
    // 6. Else X is an accessor property, so
    Handle<AccessorPair> accessors =
        Handle<AccessorPair>::cast(it->GetAccessors());
    Handle<NativeContext> native_context =
        it->GetHolder<JSReceiver>()->GetCreationContext().ToHandleChecked();
    // 6a. Set D.[[Get]] to the value of X's [[Get]] attribute.
    desc->set_get(AccessorPair::GetComponent(isolate, native_context, accessors,
                                             ACCESSOR_GETTER));
    // 6b. Set D.[[Set]] to the value of X's [[Set]] attribute.
    desc->set_set(AccessorPair::GetComponent(isolate, native_context, accessors,
                                             ACCESSOR_SETTER));
  }

  // 7. Set D.[[Enumerable]] to the value of X's [[Enumerable]] attribute.
  desc->set_enumerable((attrs & DONT_ENUM) == 0);
  // 8. Set D.[[Configurable]] to the value of X's [[Configurable]] attribute.
  desc->set_configurable((attrs & DONT_DELETE) == 0);
  // 9. Return D.
  DCHECK(PropertyDescriptor::IsAccessorDescriptor(desc) !=
         PropertyDescriptor::IsDataDescriptor(desc));
  return Just(true);
}
Maybe<bool> JSReceiver::SetIntegrityLevel(Handle<JSReceiver> receiver,
                                          IntegrityLevel level,
                                          ShouldThrow should_throw) {
  DCHECK(level == SEALED || level == FROZEN);

  if (receiver->IsJSObject()) {
    Handle<JSObject> object = Handle<JSObject>::cast(receiver);

    if (!object->HasSloppyArgumentsElements() &&
        !object->IsJSModuleNamespace()) {  // Fast path.
      // Prevent memory leaks by not adding unnecessary transitions.
      Maybe<bool> test = JSObject::TestIntegrityLevel(object, level);
      MAYBE_RETURN(test, Nothing<bool>());
      if (test.FromJust()) return test;

      if (level == SEALED) {
        return JSObject::PreventExtensionsWithTransition<SEALED>(object,
                                                                 should_throw);
      } else {
        return JSObject::PreventExtensionsWithTransition<FROZEN>(object,
                                                                 should_throw);
      }
    }
  }

  Isolate* isolate = receiver->GetIsolate();

  MAYBE_RETURN(JSReceiver::PreventExtensions(receiver, should_throw),
               Nothing<bool>());

  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys, JSReceiver::OwnPropertyKeys(receiver), Nothing<bool>());

  PropertyDescriptor no_conf;
  no_conf.set_configurable(false);

  PropertyDescriptor no_conf_no_write;
  no_conf_no_write.set_configurable(false);
  no_conf_no_write.set_writable(false);

  if (level == SEALED) {
    for (int i = 0; i < keys->length(); ++i) {
      Handle<Object> key(keys->get(i), isolate);
      MAYBE_RETURN(DefineOwnProperty(isolate, receiver, key, &no_conf,
                                     Just(kThrowOnError)),
                   Nothing<bool>());
    }
    return Just(true);
  }

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> key(keys->get(i), isolate);
    PropertyDescriptor current_desc;
    Maybe<bool> owned = JSReceiver::GetOwnPropertyDescriptor(
        isolate, receiver, key, &current_desc);
    MAYBE_RETURN(owned, Nothing<bool>());
    if (owned.FromJust()) {
      PropertyDescriptor desc =
          PropertyDescriptor::IsAccessorDescriptor(&current_desc)
              ? no_conf
              : no_conf_no_write;
      MAYBE_RETURN(
          DefineOwnProperty(isolate, receiver, key, &desc, Just(kThrowOnError)),
          Nothing<bool>());
    }
  }
  return Just(true);
}

namespace {
Maybe<bool> GenericTestIntegrityLevel(Handle<JSReceiver> receiver,
                                      PropertyAttributes level) {
  DCHECK(level == SEALED || level == FROZEN);

  Maybe<bool> extensible = JSReceiver::IsExtensible(receiver);
  MAYBE_RETURN(extensible, Nothing<bool>());
  if (extensible.FromJust()) return Just(false);

  Isolate* isolate = receiver->GetIsolate();

  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys, JSReceiver::OwnPropertyKeys(receiver), Nothing<bool>());

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> key(keys->get(i), isolate);
    PropertyDescriptor current_desc;
    Maybe<bool> owned = JSReceiver::GetOwnPropertyDescriptor(
        isolate, receiver, key, &current_desc);
    MAYBE_RETURN(owned, Nothing<bool>());
    if (owned.FromJust()) {
      if (current_desc.configurable()) return Just(false);
      if (level == FROZEN &&
          PropertyDescriptor::IsDataDescriptor(&current_desc) &&
          current_desc.writable()) {
        return Just(false);
      }
    }
  }
  return Just(true);
}

}  // namespace

Maybe<bool> JSReceiver::TestIntegrityLevel(Handle<JSReceiver> receiver,
                                           IntegrityLevel level) {
  if (!receiver->map().IsCustomElementsReceiverMap()) {
    return JSObject::TestIntegrityLevel(Handle<JSObject>::cast(receiver),
                                        level);
  }
  return GenericTestIntegrityLevel(receiver, level);
}

Maybe<bool> JSReceiver::PreventExtensions(Handle<JSReceiver> object,
                                          ShouldThrow should_throw) {
  if (object->IsJSProxy()) {
    return JSProxy::PreventExtensions(Handle<JSProxy>::cast(object),
                                      should_throw);
  }
  DCHECK(object->IsJSObject());
  return JSObject::PreventExtensions(Handle<JSObject>::cast(object),
                                     should_throw);
}

Maybe<bool> JSReceiver::IsExtensible(Handle<JSReceiver> object) {
  if (object->IsJSProxy()) {
    return JSProxy::IsExtensible(Handle<JSProxy>::cast(object));
  }
  return Just(JSObject::IsExtensible(Handle<JSObject>::cast(object)));
}

// static
MaybeHandle<Object> JSReceiver::ToPrimitive(Isolate* isolate,
                                            Handle<JSReceiver> receiver,
                                            ToPrimitiveHint hint) {
  Handle<Object> exotic_to_prim;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, exotic_to_prim,
      Object::GetMethod(receiver, isolate->factory()->to_primitive_symbol()),
      Object);
  if (!exotic_to_prim->IsUndefined(isolate)) {
    Handle<Object> hint_string =
        isolate->factory()->ToPrimitiveHintString(hint);
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, exotic_to_prim, receiver, 1, &hint_string),
        Object);
    if (result->IsPrimitive()) return result;
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCannotConvertToPrimitive),
                    Object);
  }
  return OrdinaryToPrimitive(isolate, receiver,
                             (hint == ToPrimitiveHint::kString)
                                 ? OrdinaryToPrimitiveHint::kString
                                 : OrdinaryToPrimitiveHint::kNumber);
}

// static
MaybeHandle<Object> JSReceiver::OrdinaryToPrimitive(
    Isolate* isolate, Handle<JSReceiver> receiver,
    OrdinaryToPrimitiveHint hint) {
  Handle<String> method_names[2];
  switch (hint) {
    case OrdinaryToPrimitiveHint::kNumber:
      method_names[0] = isolate->factory()->valueOf_string();
      method_names[1] = isolate->factory()->toString_string();
      break;
    case OrdinaryToPrimitiveHint::kString:
      method_names[0] = isolate->factory()->toString_string();
      method_names[1] = isolate->factory()->valueOf_string();
      break;
  }
  for (Handle<String> name : method_names) {
    Handle<Object> method;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, method,
                               JSReceiver::GetProperty(isolate, receiver, name),
                               Object);
    if (method->IsCallable()) {
      Handle<Object> result;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, result,
          Execution::Call(isolate, method, receiver, 0, nullptr), Object);
      if (result->IsPrimitive()) return result;
    }
  }
  THROW_NEW_ERROR(isolate,
                  NewTypeError(MessageTemplate::kCannotConvertToPrimitive),
                  Object);
}

V8_WARN_UNUSED_RESULT Maybe<bool> FastGetOwnValuesOrEntries(
    Isolate* isolate, Handle<JSReceiver> receiver, bool get_entries,
    Handle<FixedArray>* result) {
  Handle<Map> map(JSReceiver::cast(*receiver).map(), isolate);

  if (!map->IsJSObjectMap()) return Just(false);
  if (!map->OnlyHasSimpleProperties()) return Just(false);

  Handle<JSObject> object(JSObject::cast(*receiver), isolate);
  Handle<DescriptorArray> descriptors(map->instance_descriptors(isolate),
                                      isolate);

  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  size_t number_of_own_elements =
      object->GetElementsAccessor()->GetCapacity(*object, object->elements());

  if (number_of_own_elements >
      static_cast<size_t>(FixedArray::kMaxLength - number_of_own_descriptors)) {
    isolate->Throw(*isolate->factory()->NewRangeError(
        MessageTemplate::kInvalidArrayLength));
    return Nothing<bool>();
  }
  // The static cast is safe after the range check right above.
  Handle<FixedArray> values_or_entries = isolate->factory()->NewFixedArray(
      static_cast<int>(number_of_own_descriptors + number_of_own_elements));
  int count = 0;

  if (object->elements() != ReadOnlyRoots(isolate).empty_fixed_array()) {
    MAYBE_RETURN(object->GetElementsAccessor()->CollectValuesOrEntries(
                     isolate, object, values_or_entries, get_entries, &count,
                     ENUMERABLE_STRINGS),
                 Nothing<bool>());
  }

  // We may have already lost stability, if CollectValuesOrEntries had
  // side-effects.
  bool stable = *map == object->map();
  if (stable) {
    descriptors.PatchValue(map->instance_descriptors(isolate));
  }

  for (InternalIndex index : InternalIndex::Range(number_of_own_descriptors)) {
    HandleScope inner_scope(isolate);

    Handle<Name> next_key(descriptors->GetKey(index), isolate);
    if (!next_key->IsString()) continue;
    Handle<Object> prop_value;

    // Directly decode from the descriptor array if |from| did not change shape.
    if (stable) {
      DCHECK_EQ(object->map(), *map);
      DCHECK_EQ(*descriptors, map->instance_descriptors(isolate));

      PropertyDetails details = descriptors->GetDetails(index);
      if (!details.IsEnumerable()) continue;
      if (details.kind() == PropertyKind::kData) {
        if (details.location() == PropertyLocation::kDescriptor) {
          prop_value = handle(descriptors->GetStrongValue(index), isolate);
        } else {
          Representation representation = details.representation();
          FieldIndex field_index = FieldIndex::ForPropertyIndex(
              *map, details.field_index(), representation);
          prop_value = JSObject::FastPropertyAt(isolate, object, representation,
                                                field_index);
        }
      } else {
        LookupIterator it(isolate, object, next_key,
                          LookupIterator::OWN_SKIP_INTERCEPTOR);
        DCHECK_EQ(LookupIterator::ACCESSOR, it.state());
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, prop_value, Object::GetProperty(&it), Nothing<bool>());
        stable = object->map() == *map;
        descriptors.PatchValue(map->instance_descriptors(isolate));
      }
    } else {
      // If the map did change, do a slower lookup. We are still guaranteed that
      // the object has a simple shape, and that the key is a name.
      LookupIterator it(isolate, object, next_key,
                        LookupIterator::OWN_SKIP_INTERCEPTOR);
      if (!it.IsFound()) continue;
      DCHECK(it.state() == LookupIterator::DATA ||
             it.state() == LookupIterator::ACCESSOR);
      if (!it.IsEnumerable()) continue;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, prop_value, Object::GetProperty(&it), Nothing<bool>());
    }

    if (get_entries) {
      prop_value = MakeEntryPair(isolate, next_key, prop_value);
    }

    values_or_entries->set(count, *prop_value);
    count++;
  }

  DCHECK_LE(count, values_or_entries->length());
  *result = FixedArray::ShrinkOrEmpty(isolate, values_or_entries, count);
  return Just(true);
}

MaybeHandle<FixedArray> GetOwnValuesOrEntries(Isolate* isolate,
                                              Handle<JSReceiver> object,
                                              PropertyFilter filter,
                                              bool try_fast_path,
                                              bool get_entries) {
  Handle<FixedArray> values_or_entries;
  if (try_fast_path && filter == ENUMERABLE_STRINGS) {
    Maybe<bool> fast_values_or_entries = FastGetOwnValuesOrEntries(
        isolate, object, get_entries, &values_or_entries);
    if (fast_values_or_entries.IsNothing()) return MaybeHandle<FixedArray>();
    if (fast_values_or_entries.FromJust()) return values_or_entries;
  }

  PropertyFilter key_filter =
      static_cast<PropertyFilter>(filter & ~ONLY_ENUMERABLE);

  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys,
      KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly, key_filter,
                              GetKeysConversion::kConvertToString),
      MaybeHandle<FixedArray>());

  values_or_entries = isolate->factory()->NewFixedArray(keys->length());
  int length = 0;

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Name> key =
        Handle<Name>::cast(handle(keys->get(isolate, i), isolate));

    if (filter & ONLY_ENUMERABLE) {
      PropertyDescriptor descriptor;
      Maybe<bool> did_get_descriptor = JSReceiver::GetOwnPropertyDescriptor(
          isolate, object, key, &descriptor);
      MAYBE_RETURN(did_get_descriptor, MaybeHandle<FixedArray>());
      if (!did_get_descriptor.FromJust() || !descriptor.enumerable()) continue;
    }

    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, value, Object::GetPropertyOrElement(isolate, object, key),
        MaybeHandle<FixedArray>());

    if (get_entries) {
      Handle<FixedArray> entry_storage = isolate->factory()->NewFixedArray(2);
      entry_storage->set(0, *key);
      entry_storage->set(1, *value);
      value = isolate->factory()->NewJSArrayWithElements(entry_storage,
                                                         PACKED_ELEMENTS, 2);
    }

    values_or_entries->set(length, *value);
    length++;
  }
  DCHECK_LE(length, values_or_entries->length());
  return FixedArray::ShrinkOrEmpty(isolate, values_or_entries, length);
}

MaybeHandle<FixedArray> JSReceiver::GetOwnValues(Handle<JSReceiver> object,
                                                 PropertyFilter filter,
                                                 bool try_fast_path) {
  return GetOwnValuesOrEntries(object->GetIsolate(), object, filter,
                               try_fast_path, false);
}

MaybeHandle<FixedArray> JSReceiver::GetOwnEntries(Handle<JSReceiver> object,
                                                  PropertyFilter filter,
                                                  bool try_fast_path) {
  return GetOwnValuesOrEntries(object->GetIsolate(), object, filter,
                               try_fast_path, true);
}

Maybe<bool> JSReceiver::SetPrototype(Isolate* isolate,
                                     Handle<JSReceiver> object,
                                     Handle<Object> value, bool from_javascript,
                                     ShouldThrow should_throw) {
  if (object->IsJSProxy()) {
    return JSProxy::SetPrototype(isolate, Handle<JSProxy>::cast(object), value,
                                 from_javascript, should_throw);
  }
  return JSObject::SetPrototype(isolate, Handle<JSObject>::cast(object), value,
                                from_javascript, should_throw);
}

bool JSReceiver::HasProxyInPrototype(Isolate* isolate) {
  for (PrototypeIterator iter(isolate, *this, kStartAtReceiver,
                              PrototypeIterator::END_AT_NULL);
       !iter.IsAtEnd(); iter.AdvanceIgnoringProxies()) {
    if (iter.GetCurrent().IsJSProxy()) return true;
  }
  return false;
}

bool JSReceiver::IsCodeLike(Isolate* isolate) const {
  DisallowGarbageCollection no_gc;
  Object maybe_constructor = map().GetConstructor();
  if (!maybe_constructor.IsJSFunction()) return false;
  if (!JSFunction::cast(maybe_constructor).shared().IsApiFunction()) {
    return false;
  }
  Object instance_template = JSFunction::cast(maybe_constructor)
                                 .shared()
                                 .get_api_func_data()
                                 .GetInstanceTemplate();
  if (instance_template.IsUndefined(isolate)) return false;
  return ObjectTemplateInfo::cast(instance_template).code_like();
}

// static
MaybeHandle<JSObject> JSObject::New(Handle<JSFunction> constructor,
                                    Handle<JSReceiver> new_target,
                                    Handle<AllocationSite> site) {
  // If called through new, new.target can be:
  // - a subclass of constructor,
  // - a proxy wrapper around constructor, or
  // - the constructor itself.
  // If called through Reflect.construct, it's guaranteed to be a constructor.
  Isolate* const isolate = constructor->GetIsolate();
  DCHECK(constructor->IsConstructor());
  DCHECK(new_target->IsConstructor());
  DCHECK(!constructor->has_initial_map() ||
         !InstanceTypeChecker::IsJSFunction(
             constructor->initial_map().instance_type()));

  Handle<Map> initial_map;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, initial_map,
      JSFunction::GetDerivedMap(isolate, constructor, new_target), JSObject);
  int initial_capacity = V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL
                             ? SwissNameDictionary::kInitialCapacity
                             : NameDictionary::kInitialCapacity;
  Handle<JSObject> result = isolate->factory()->NewFastOrSlowJSObjectFromMap(
      initial_map, initial_capacity, AllocationType::kYoung, site);
  isolate->counters()->constructed_objects()->Increment();
  isolate->counters()->constructed_objects_runtime()->Increment();
  return result;
}

// 9.1.12 ObjectCreate ( proto [ , internalSlotsList ] )
// Notice: This is NOT 19.1.2.2 Object.create ( O, Properties )
MaybeHandle<JSObject> JSObject::ObjectCreate(Isolate* isolate,
                                             Handle<Object> prototype) {
  // Generate the map with the specified {prototype} based on the Object
  // function's initial map from the current native context.
  // TODO(bmeurer): Use a dedicated cache for Object.create; think about
  // slack tracking for Object.create.
  Handle<Map> map =
      Map::GetObjectCreateMap(isolate, Handle<HeapObject>::cast(prototype));

  // Actually allocate the object.
  return isolate->factory()->NewFastOrSlowJSObjectFromMap(map);
}

void JSObject::EnsureWritableFastElements(Handle<JSObject> object) {
  DCHECK(object->HasSmiOrObjectElements() ||
         object->HasFastStringWrapperElements() ||
         object->HasAnyNonextensibleElements());
  FixedArray raw_elems = FixedArray::cast(object->elements());
  Isolate* isolate = object->GetIsolate();
  if (raw_elems.map() != ReadOnlyRoots(isolate).fixed_cow_array_map()) return;
  Handle<FixedArray> elems(raw_elems, isolate);
  Handle<FixedArray> writable_elems = isolate->factory()->CopyFixedArrayWithMap(
      elems, isolate->factory()->fixed_array_map());
  object->set_elements(*writable_elems);
  isolate->counters()->cow_arrays_converted()->Increment();
}

int JSObject::GetHeaderSize(InstanceType type,
                            bool function_has_prototype_slot) {
  switch (type) {
    case JS_API_OBJECT_TYPE:
    case JS_ITERATOR_PROTOTYPE_TYPE:
    case JS_MAP_ITERATOR_PROTOTYPE_TYPE:
    case JS_OBJECT_PROTOTYPE_TYPE:
    case JS_OBJECT_TYPE:
    case JS_PROMISE_PROTOTYPE_TYPE:
    case JS_REG_EXP_PROTOTYPE_TYPE:
    case JS_SET_ITERATOR_PROTOTYPE_TYPE:
    case JS_SET_PROTOTYPE_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_STRING_ITERATOR_PROTOTYPE_TYPE:
    case JS_ARRAY_ITERATOR_PROTOTYPE_TYPE:
    case JS_TYPED_ARRAY_PROTOTYPE_TYPE:
      return JSObject::kHeaderSize;
    case JS_GENERATOR_OBJECT_TYPE:
      return JSGeneratorObject::kHeaderSize;
    case JS_ASYNC_FUNCTION_OBJECT_TYPE:
      return JSAsyncFunctionObject::kHeaderSize;
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
      return JSAsyncGeneratorObject::kHeaderSize;
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
      return JSAsyncFromSyncIterator::kHeaderSize;
    case JS_GLOBAL_PROXY_TYPE:
      return JSGlobalProxy::kHeaderSize;
    case JS_GLOBAL_OBJECT_TYPE:
      return JSGlobalObject::kHeaderSize;
    case JS_BOUND_FUNCTION_TYPE:
      return JSBoundFunction::kHeaderSize;
    case JS_FUNCTION_TYPE:
    case JS_CLASS_CONSTRUCTOR_TYPE:
    case JS_PROMISE_CONSTRUCTOR_TYPE:
    case JS_REG_EXP_CONSTRUCTOR_TYPE:
    case JS_ARRAY_CONSTRUCTOR_TYPE:
#define TYPED_ARRAY_CONSTRUCTORS_SWITCH(Type, type, TYPE, Ctype) \
  case TYPE##_TYPED_ARRAY_CONSTRUCTOR_TYPE:
      TYPED_ARRAYS(TYPED_ARRAY_CONSTRUCTORS_SWITCH)
#undef TYPED_ARRAY_CONSTRUCTORS_SWITCH
      return JSFunction::GetHeaderSize(function_has_prototype_slot);
    case JS_PRIMITIVE_WRAPPER_TYPE:
      return JSPrimitiveWrapper::kHeaderSize;
    case JS_DATE_TYPE:
      return JSDate::kHeaderSize;
    case JS_ARRAY_TYPE:
      return JSArray::kHeaderSize;
    case JS_ARRAY_BUFFER_TYPE:
      return JSArrayBuffer::kHeaderSize;
    case JS_ARRAY_ITERATOR_TYPE:
      return JSArrayIterator::kHeaderSize;
    case JS_TYPED_ARRAY_TYPE:
      return JSTypedArray::kHeaderSize;
    case JS_DATA_VIEW_TYPE:
      return JSDataView::kHeaderSize;
    case JS_SET_TYPE:
      return JSSet::kHeaderSize;
    case JS_MAP_TYPE:
      return JSMap::kHeaderSize;
    case JS_SET_KEY_VALUE_ITERATOR_TYPE:
    case JS_SET_VALUE_ITERATOR_TYPE:
      return JSSetIterator::kHeaderSize;
    case JS_MAP_KEY_ITERATOR_TYPE:
    case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case JS_MAP_VALUE_ITERATOR_TYPE:
      return JSMapIterator::kHeaderSize;
    case JS_WEAK_REF_TYPE:
      return JSWeakRef::kHeaderSize;
    case JS_FINALIZATION_REGISTRY_TYPE:
      return JSFinalizationRegistry::kHeaderSize;
    case JS_WEAK_MAP_TYPE:
      return JSWeakMap::kHeaderSize;
    case JS_WEAK_SET_TYPE:
      return JSWeakSet::kHeaderSize;
    case JS_PROMISE_TYPE:
      return JSPromise::kHeaderSize;
    case JS_REG_EXP_TYPE:
      return JSRegExp::kHeaderSize;
    case JS_REG_EXP_STRING_ITERATOR_TYPE:
      return JSRegExpStringIterator::kHeaderSize;
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
      return JSObject::kHeaderSize;
    case JS_MESSAGE_OBJECT_TYPE:
      return JSMessageObject::kHeaderSize;
    case JS_ARGUMENTS_OBJECT_TYPE:
      return JSObject::kHeaderSize;
    case JS_ERROR_TYPE:
      return JSObject::kHeaderSize;
    case JS_EXTERNAL_OBJECT_TYPE:
      return JSExternalObject::kHeaderSize;
    case JS_SHADOW_REALM_TYPE:
      return JSShadowRealm::kHeaderSize;
    case JS_STRING_ITERATOR_TYPE:
      return JSStringIterator::kHeaderSize;
    case JS_MODULE_NAMESPACE_TYPE:
      return JSModuleNamespace::kHeaderSize;
    case JS_SHARED_STRUCT_TYPE:
      return JSSharedStruct::kHeaderSize;
    case JS_TEMPORAL_CALENDAR_TYPE:
      return JSTemporalCalendar::kHeaderSize;
    case JS_TEMPORAL_DURATION_TYPE:
      return JSTemporalDuration::kHeaderSize;
    case JS_TEMPORAL_INSTANT_TYPE:
      return JSTemporalInstant::kHeaderSize;
    case JS_TEMPORAL_PLAIN_DATE_TYPE:
      return JSTemporalPlainDate::kHeaderSize;
    case JS_TEMPORAL_PLAIN_DATE_TIME_TYPE:
      return JSTemporalPlainDateTime::kHeaderSize;
    case JS_TEMPORAL_PLAIN_MONTH_DAY_TYPE:
      return JSTemporalPlainMonthDay::kHeaderSize;
    case JS_TEMPORAL_PLAIN_TIME_TYPE:
      return JSTemporalPlainTime::kHeaderSize;
    case JS_TEMPORAL_PLAIN_YEAR_MONTH_TYPE:
      return JSTemporalPlainYearMonth::kHeaderSize;
    case JS_TEMPORAL_TIME_ZONE_TYPE:
      return JSTemporalTimeZone::kHeaderSize;
    case JS_TEMPORAL_ZONED_DATE_TIME_TYPE:
      return JSTemporalZonedDateTime::kHeaderSize;
    case JS_WRAPPED_FUNCTION_TYPE:
      return JSWrappedFunction::kHeaderSize;
#ifdef V8_INTL_SUPPORT
    case JS_V8_BREAK_ITERATOR_TYPE:
      return JSV8BreakIterator::kHeaderSize;
    case JS_COLLATOR_TYPE:
      return JSCollator::kHeaderSize;
    case JS_DATE_TIME_FORMAT_TYPE:
      return JSDateTimeFormat::kHeaderSize;
    case JS_DISPLAY_NAMES_TYPE:
      return JSDisplayNames::kHeaderSize;
    case JS_LIST_FORMAT_TYPE:
      return JSListFormat::kHeaderSize;
    case JS_LOCALE_TYPE:
      return JSLocale::kHeaderSize;
    case JS_NUMBER_FORMAT_TYPE:
      return JSNumberFormat::kHeaderSize;
    case JS_PLURAL_RULES_TYPE:
      return JSPluralRules::kHeaderSize;
    case JS_RELATIVE_TIME_FORMAT_TYPE:
      return JSRelativeTimeFormat::kHeaderSize;
    case JS_SEGMENT_ITERATOR_TYPE:
      return JSSegmentIterator::kHeaderSize;
    case JS_SEGMENTER_TYPE:
      return JSSegmenter::kHeaderSize;
    case JS_SEGMENTS_TYPE:
      return JSSegments::kHeaderSize;
#endif  // V8_INTL_SUPPORT
#if V8_ENABLE_WEBASSEMBLY
    case WASM_GLOBAL_OBJECT_TYPE:
      return WasmGlobalObject::kHeaderSize;
    case WASM_INSTANCE_OBJECT_TYPE:
      return WasmInstanceObject::kHeaderSize;
    case WASM_MEMORY_OBJECT_TYPE:
      return WasmMemoryObject::kHeaderSize;
    case WASM_MODULE_OBJECT_TYPE:
      return WasmModuleObject::kHeaderSize;
    case WASM_SUSPENDER_OBJECT_TYPE:
      return WasmSuspenderObject::kHeaderSize;
    case WASM_TABLE_OBJECT_TYPE:
      return WasmTableObject::kHeaderSize;
    case WASM_VALUE_OBJECT_TYPE:
      return WasmValueObject::kHeaderSize;
    case WASM_TAG_OBJECT_TYPE:
      return WasmTagObject::kHeaderSize;
#endif  // V8_ENABLE_WEBASSEMBLY
    default: {
      // Special type check for API Objects because they are in a large variable
      // instance type range.
      if (InstanceTypeChecker::IsJSApiObject(type)) {
        return JSObject::kHeaderSize;
      }
      std::stringstream ss;
      ss << type;
      FATAL("unexpected instance type: %s\n", ss.str().c_str());
    }
  }
}

// static
bool JSObject::AllCanRead(LookupIterator* it) {
  // Skip current iteration, it's in state ACCESS_CHECK or INTERCEPTOR, both of
  // which have already been checked.
  DCHECK(it->state() == LookupIterator::ACCESS_CHECK ||
         it->state() == LookupIterator::INTERCEPTOR);
  for (it->Next(); it->IsFound(); it->Next()) {
    if (it->state() == LookupIterator::ACCESSOR) {
      auto accessors = it->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        if (AccessorInfo::cast(*accessors).all_can_read()) return true;
      }
    } else if (it->state() == LookupIterator::INTERCEPTOR) {
      if (it->GetInterceptor()->all_can_read()) return true;
    } else if (it->state() == LookupIterator::JSPROXY) {
      // Stop lookupiterating. And no, AllCanNotRead.
      return false;
    }
  }
  return false;
}

MaybeHandle<Object> JSObject::GetPropertyWithFailedAccessCheck(
    LookupIterator* it) {
  Isolate* isolate = it->isolate();
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor =
      it->GetInterceptorForFailedAccessCheck();
  if (interceptor.is_null()) {
    while (AllCanRead(it)) {
      if (it->state() == LookupIterator::ACCESSOR) {
        return Object::GetPropertyWithAccessor(it);
      }
      DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
      bool done;
      Handle<Object> result;
      ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                                 GetPropertyWithInterceptor(it, &done), Object);
      if (done) return result;
    }

  } else {
    Handle<Object> result;
    bool done;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        GetPropertyWithInterceptorInternal(it, interceptor, &done), Object);
    if (done) return result;
  }

  // Cross-Origin [[Get]] of Well-Known Symbols does not throw, and returns
  // undefined.
  Handle<Name> name = it->GetName();
  if (name->IsSymbol() && Symbol::cast(*name).is_well_known_symbol()) {
    return it->factory()->undefined_value();
  }

  isolate->ReportFailedAccessCheck(checked);
  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  return it->factory()->undefined_value();
}

Maybe<PropertyAttributes> JSObject::GetPropertyAttributesWithFailedAccessCheck(
    LookupIterator* it) {
  Isolate* isolate = it->isolate();
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor =
      it->GetInterceptorForFailedAccessCheck();
  if (interceptor.is_null()) {
    while (AllCanRead(it)) {
      if (it->state() == LookupIterator::ACCESSOR) {
        return Just(it->property_attributes());
      }
      DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
      auto result = GetPropertyAttributesWithInterceptor(it);
      if (isolate->has_scheduled_exception()) break;
      if (result.IsJust() && result.FromJust() != ABSENT) return result;
    }
  } else {
    Maybe<PropertyAttributes> result =
        GetPropertyAttributesWithInterceptorInternal(it, interceptor);
    if (isolate->has_pending_exception()) return Nothing<PropertyAttributes>();
    if (result.FromMaybe(ABSENT) != ABSENT) return result;
  }
  isolate->ReportFailedAccessCheck(checked);
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<PropertyAttributes>());
  return Just(ABSENT);
}

// static
bool JSObject::AllCanWrite(LookupIterator* it) {
  for (; it->IsFound() && it->state() != LookupIterator::JSPROXY; it->Next()) {
    if (it->state() == LookupIterator::ACCESSOR) {
      Handle<Object> accessors = it->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        if (AccessorInfo::cast(*accessors).all_can_write()) return true;
      }
    }
  }
  return false;
}

Maybe<bool> JSObject::SetPropertyWithFailedAccessCheck(
    LookupIterator* it, Handle<Object> value, Maybe<ShouldThrow> should_throw) {
  Isolate* isolate = it->isolate();
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor =
      it->GetInterceptorForFailedAccessCheck();
  if (interceptor.is_null()) {
    if (AllCanWrite(it)) {
      return Object::SetPropertyWithAccessor(it, value, should_throw);
    }
  } else {
    Maybe<bool> result = SetPropertyWithInterceptorInternal(
        it, interceptor, should_throw, value);
    if (isolate->has_pending_exception()) return Nothing<bool>();
    if (result.IsJust()) return result;
  }
  isolate->ReportFailedAccessCheck(checked);
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
  return Just(true);
}

void JSObject::SetNormalizedProperty(Handle<JSObject> object, Handle<Name> name,
                                     Handle<Object> value,
                                     PropertyDetails details) {
  DCHECK(!object->HasFastProperties());
  DCHECK(name->IsUniqueName());
  Isolate* isolate = object->GetIsolate();

  uint32_t hash = name->hash();

  if (object->IsJSGlobalObject()) {
    Handle<JSGlobalObject> global_obj = Handle<JSGlobalObject>::cast(object);
    Handle<GlobalDictionary> dictionary(
        global_obj->global_dictionary(kAcquireLoad), isolate);
    ReadOnlyRoots roots(isolate);
    InternalIndex entry = dictionary->FindEntry(isolate, roots, name, hash);

    if (entry.is_not_found()) {
      DCHECK_IMPLIES(global_obj->map().is_prototype_map(),
                     Map::IsPrototypeChainInvalidated(global_obj->map()));
      auto cell_type = value->IsUndefined(roots) ? PropertyCellType::kUndefined
                                                 : PropertyCellType::kConstant;
      details = details.set_cell_type(cell_type);
      auto cell = isolate->factory()->NewPropertyCell(name, details, value);
      dictionary =
          GlobalDictionary::Add(isolate, dictionary, name, cell, details);
      global_obj->set_global_dictionary(*dictionary, kReleaseStore);
    } else {
      PropertyCell::PrepareForAndSetValue(isolate, dictionary, entry, value,
                                          details);
      DCHECK_EQ(dictionary->CellAt(entry).value(), *value);
    }
  } else {
    if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
      Handle<SwissNameDictionary> dictionary(
          object->property_dictionary_swiss(), isolate);
      InternalIndex entry = dictionary->FindEntry(isolate, *name);
      if (entry.is_not_found()) {
        DCHECK_IMPLIES(object->map().is_prototype_map(),
                       Map::IsPrototypeChainInvalidated(object->map()));
        dictionary =
            SwissNameDictionary::Add(isolate, dictionary, name, value, details);
        object->SetProperties(*dictionary);
      } else {
        dictionary->ValueAtPut(entry, *value);
        dictionary->DetailsAtPut(entry, details);
      }
    } else {
      Handle<NameDictionary> dictionary(object->property_dictionary(), isolate);
      InternalIndex entry = dictionary->FindEntry(isolate, name);
      if (entry.is_not_found()) {
        DCHECK_IMPLIES(object->map().is_prototype_map(),
                       Map::IsPrototypeChainInvalidated(object->map()));
        dictionary =
            NameDictionary::Add(isolate, dictionary, name, value, details);
        object->SetProperties(*dictionary);
      } else {
        PropertyDetails original_details = dictionary->DetailsAt(entry);
        int enumeration_index = original_details.dictionary_index();
        DCHECK_GT(enumeration_index, 0);
        details = details.set_index(enumeration_index);
        dictionary->SetEntry(entry, *name, *value, details);
      }
    }
  }
}

void JSObject::SetNormalizedElement(Handle<JSObject> object, uint32_t index,
                                    Handle<Object> value,
                                    PropertyDetails details) {
  DCHECK_EQ(object->GetElementsKind(), DICTIONARY_ELEMENTS);

  Isolate* isolate = object->GetIsolate();

  Handle<NumberDictionary> dictionary =
      handle(NumberDictionary::cast(object->elements()), isolate);
  dictionary =
      NumberDictionary::Set(isolate, dictionary, index, value, object, details);
  object->set_elements(*dictionary);
}

void JSObject::JSObjectShortPrint(StringStream* accumulator) {
  switch (map().instance_type()) {
    case JS_ARRAY_TYPE: {
      double length = JSArray::cast(*this).length().IsUndefined()
                          ? 0
                          : JSArray::cast(*this).length().Number();
      accumulator->Add("<JSArray[%u]>", static_cast<uint32_t>(length));
      break;
    }
    case JS_BOUND_FUNCTION_TYPE: {
      JSBoundFunction bound_function = JSBoundFunction::cast(*this);
      accumulator->Add("<JSBoundFunction");
      accumulator->Add(" (BoundTargetFunction %p)>",
                       reinterpret_cast<void*>(
                           bound_function.bound_target_function().ptr()));
      break;
    }
    case JS_WEAK_MAP_TYPE: {
      accumulator->Add("<JSWeakMap>");
      break;
    }
    case JS_WEAK_SET_TYPE: {
      accumulator->Add("<JSWeakSet>");
      break;
    }
    case JS_REG_EXP_TYPE: {
      accumulator->Add("<JSRegExp");
      JSRegExp regexp = JSRegExp::cast(*this);
      if (regexp.source().IsString()) {
        accumulator->Add(" ");
        String::cast(regexp.source()).StringShortPrint(accumulator);
      }
      accumulator->Add(">");

      break;
    }
    case JS_PROMISE_CONSTRUCTOR_TYPE:
    case JS_REG_EXP_CONSTRUCTOR_TYPE:
    case JS_ARRAY_CONSTRUCTOR_TYPE:
#define TYPED_ARRAY_CONSTRUCTORS_SWITCH(Type, type, TYPE, Ctype) \
  case TYPE##_TYPED_ARRAY_CONSTRUCTOR_TYPE:
      TYPED_ARRAYS(TYPED_ARRAY_CONSTRUCTORS_SWITCH)
#undef TYPED_ARRAY_CONSTRUCTORS_SWITCH
    case JS_CLASS_CONSTRUCTOR_TYPE:
    case JS_FUNCTION_TYPE: {
      JSFunction function = JSFunction::cast(*this);
      std::unique_ptr<char[]> fun_name = function.shared().DebugNameCStr();
      if (fun_name[0] != '\0') {
        accumulator->Add("<JSFunction ");
        accumulator->Add(fun_name.get());
      } else {
        accumulator->Add("<JSFunction");
      }
      if (FLAG_trace_file_names) {
        Object source_name = Script::cast(function.shared().script()).name();
        if (source_name.IsString()) {
          String str = String::cast(source_name);
          if (str.length() > 0) {
            accumulator->Add(" <");
            accumulator->Put(str);
            accumulator->Add(">");
          }
        }
      }
      accumulator->Add(" (sfi = %p)",
                       reinterpret_cast<void*>(function.shared().ptr()));
      accumulator->Put('>');
      break;
    }
    case JS_GENERATOR_OBJECT_TYPE: {
      accumulator->Add("<JSGenerator>");
      break;
    }
    case JS_ASYNC_FUNCTION_OBJECT_TYPE: {
      accumulator->Add("<JSAsyncFunctionObject>");
      break;
    }
    case JS_ASYNC_GENERATOR_OBJECT_TYPE: {
      accumulator->Add("<JS AsyncGenerator>");
      break;
    }

    // All other JSObjects are rather similar to each other (JSObject,
    // JSGlobalProxy, JSGlobalObject, JSUndetectable, JSPrimitiveWrapper).
    default: {
      Map map_of_this = map();
      Heap* heap = GetHeap();
      Object constructor = map_of_this.GetConstructor();
      bool printed = false;
      if (constructor.IsHeapObject() &&
          !heap->Contains(HeapObject::cast(constructor))) {
        accumulator->Add("!!!INVALID CONSTRUCTOR!!!");
      } else {
        bool is_global_proxy = IsJSGlobalProxy();
        if (constructor.IsJSFunction()) {
          if (!heap->Contains(JSFunction::cast(constructor).shared())) {
            accumulator->Add("!!!INVALID SHARED ON CONSTRUCTOR!!!");
          } else {
            String constructor_name =
                JSFunction::cast(constructor).shared().Name();
            if (constructor_name.length() > 0) {
              accumulator->Add(is_global_proxy ? "<GlobalObject " : "<");
              accumulator->Put(constructor_name);
              accumulator->Add(" %smap = %p",
                               map_of_this.is_deprecated() ? "deprecated-" : "",
                               map_of_this);
              printed = true;
            }
          }
        } else if (constructor.IsFunctionTemplateInfo()) {
          accumulator->Add("<RemoteObject>");
          printed = true;
        }
        if (!printed) {
          accumulator->Add("<JS");
          if (is_global_proxy) {
            accumulator->Add("GlobalProxy");
          } else if (IsJSGlobalObject()) {
            accumulator->Add("GlobalObject");
          } else {
            accumulator->Add("Object");
          }
        }
      }
      if (IsJSPrimitiveWrapper()) {
        accumulator->Add(" value = ");
        JSPrimitiveWrapper::cast(*this).value().ShortPrint(accumulator);
      }
      accumulator->Put('>');
      break;
    }
  }
}

void JSObject::PrintElementsTransition(FILE* file, Handle<JSObject> object,
                                       ElementsKind from_kind,
                                       Handle<FixedArrayBase> from_elements,
                                       ElementsKind to_kind,
                                       Handle<FixedArrayBase> to_elements) {
  if (from_kind != to_kind) {
    OFStream os(file);
    os << "elements transition [" << ElementsKindToString(from_kind) << " -> "
       << ElementsKindToString(to_kind) << "] in ";
    JavaScriptFrame::PrintTop(object->GetIsolate(), file, false, true);
    PrintF(file, " for ");
    object->ShortPrint(file);
    PrintF(file, " from ");
    from_elements->ShortPrint(file);
    PrintF(file, " to ");
    to_elements->ShortPrint(file);
    PrintF(file, "\n");
  }
}

void JSObject::PrintInstanceMigration(FILE* file, Map original_map,
                                      Map new_map) {
  if (new_map.is_dictionary_map()) {
    PrintF(file, "[migrating to slow]\n");
    return;
  }
  PrintF(file, "[migrating]");
  Isolate* isolate = GetIsolate();
  DescriptorArray o = original_map.instance_descriptors(isolate);
  DescriptorArray n = new_map.instance_descriptors(isolate);
  for (InternalIndex i : original_map.IterateOwnDescriptors()) {
    Representation o_r = o.GetDetails(i).representation();
    Representation n_r = n.GetDetails(i).representation();
    if (!o_r.Equals(n_r)) {
      String::cast(o.GetKey(i)).PrintOn(file);
      PrintF(file, ":%s->%s ", o_r.Mnemonic(), n_r.Mnemonic());
    } else if (o.GetDetails(i).location() == PropertyLocation::kDescriptor &&
               n.GetDetails(i).location() == PropertyLocation::kField) {
      Name name = o.GetKey(i);
      if (name.IsString()) {
        String::cast(name).PrintOn(file);
      } else {
        PrintF(file, "{symbol %p}", reinterpret_cast<void*>(name.ptr()));
      }
      PrintF(file, " ");
    }
  }
  if (original_map.elements_kind() != new_map.elements_kind()) {
    PrintF(file, "elements_kind[%i->%i]", original_map.elements_kind(),
           new_map.elements_kind());
  }
  PrintF(file, "\n");
}

bool JSObject::IsUnmodifiedApiObject(FullObjectSlot o) {
  Object object = *o;
  if (object.IsSmi()) return false;
  HeapObject heap_object = HeapObject::cast(object);
  if (!object.IsJSObject()) return false;
  JSObject js_object = JSObject::cast(object);
  if (!js_object.IsDroppableApiObject()) return false;
  Object maybe_constructor = js_object.map().GetConstructor();
  if (!maybe_constructor.IsJSFunction()) return false;
  JSFunction constructor = JSFunction::cast(maybe_constructor);
  if (js_object.elements().length() != 0) return false;
  // Check that the object is not a key in a WeakMap (over-approximation).
  if (!js_object.GetIdentityHash().IsUndefined()) return false;

  return constructor.initial_map() == heap_object.map();
}

// static
void JSObject::UpdatePrototypeUserRegistration(Handle<Map> old_map,
                                               Handle<Map> new_map,
                                               Isolate* isolate) {
  DCHECK(old_map->is_prototype_map());
  DCHECK(new_map->is_prototype_map());
  bool was_registered = JSObject::UnregisterPrototypeUser(old_map, isolate);
  new_map->set_prototype_info(old_map->prototype_info(), kReleaseStore);
  old_map->set_prototype_info(Smi::zero(), kReleaseStore);
  if (FLAG_trace_prototype_users) {
    PrintF("Moving prototype_info %p from map %p to map %p.\n",
           reinterpret_cast<void*>(new_map->prototype_info().ptr()),
           reinterpret_cast<void*>(old_map->ptr()),
           reinterpret_cast<void*>(new_map->ptr()));
  }
  if (was_registered) {
    if (new_map->prototype_info().IsPrototypeInfo()) {
      // The new map isn't registered with its prototype yet; reflect this fact
      // in the PrototypeInfo it just inherited from the old map.
      PrototypeInfo::cast(new_map->prototype_info())
          .set_registry_slot(PrototypeInfo::UNREGISTERED);
    }
    JSObject::LazyRegisterPrototypeUser(new_map, isolate);
  }
}

// static
void JSObject::NotifyMapChange(Handle<Map> old_map, Handle<Map> new_map,
                               Isolate* isolate) {
  if (!old_map->is_prototype_map()) return;

  InvalidatePrototypeChains(*old_map);

  // If the map was registered with its prototype before, ensure that it
  // registers with its new prototype now. This preserves the invariant that
  // when a map on a prototype chain is registered with its prototype, then
  // all prototypes further up the chain are also registered with their
  // respective prototypes.
  UpdatePrototypeUserRegistration(old_map, new_map, isolate);
}

namespace {

// To migrate a fast instance to a fast map:
// - First check whether the instance needs to be rewritten. If not, simply
//   change the map.
// - Otherwise, allocate a fixed array large enough to hold all fields, in
//   addition to unused space.
// - Copy all existing properties in, in the following order: backing store
//   properties, unused fields, inobject properties.
// - If all allocation succeeded, commit the state atomically:
//   * Copy inobject properties from the backing store back into the object.
//   * Trim the difference in instance size of the object. This also cleanly
//     frees inobject properties that moved to the backing store.
//   * If there are properties left in the backing store, trim of the space used
//     to temporarily store the inobject properties.
//   * If there are properties left in the backing store, install the backing
//     store.
void MigrateFastToFast(Isolate* isolate, Handle<JSObject> object,
                       Handle<Map> new_map) {
  Handle<Map> old_map(object->map(), isolate);
  // In case of a regular transition.
  if (new_map->GetBackPointer(isolate) == *old_map) {
    // If the map does not add named properties, simply set the map.
    if (old_map->NumberOfOwnDescriptors() ==
        new_map->NumberOfOwnDescriptors()) {
      object->set_map(*new_map, kReleaseStore);
      return;
    }

    // If the map adds a new kDescriptor property, simply set the map.
    PropertyDetails details = new_map->GetLastDescriptorDetails(isolate);
    if (details.location() == PropertyLocation::kDescriptor) {
      object->set_map(*new_map, kReleaseStore);
      return;
    }

    // Check if we still have space in the {object}, in which case we
    // can also simply set the map (modulo a special case for mutable
    // double boxes).
    FieldIndex index =
        FieldIndex::ForDescriptor(isolate, *new_map, new_map->LastAdded());
    if (index.is_inobject() || index.outobject_array_index() <
                                   object->property_array(isolate).length()) {
      // Allocate HeapNumbers for double fields.
      if (index.is_double()) {
        auto value = isolate->factory()->NewHeapNumberWithHoleNaN();
        object->FastPropertyAtPut(index, *value);
      }
      object->set_map(*new_map, kReleaseStore);
      return;
    }

    // This migration is a transition from a map that has run out of property
    // space. Extend the backing store.
    int grow_by = new_map->UnusedPropertyFields() + 1;
    Handle<PropertyArray> old_storage(object->property_array(isolate), isolate);
    Handle<PropertyArray> new_storage =
        isolate->factory()->CopyPropertyArrayAndGrow(old_storage, grow_by);

    // Properly initialize newly added property.
    Handle<Object> value;
    if (details.representation().IsDouble()) {
      value = isolate->factory()->NewHeapNumberWithHoleNaN();
    } else {
      value = isolate->factory()->uninitialized_value();
    }
    DCHECK_EQ(PropertyLocation::kField, details.location());
    DCHECK_EQ(PropertyKind::kData, details.kind());
    DCHECK(!index.is_inobject());  // Must be a backing store index.
    new_storage->set(index.outobject_array_index(), *value);

    // From here on we cannot fail and we shouldn't GC anymore.
    DisallowGarbageCollection no_gc;

    // Set the new property value and do the map transition.
    object->SetProperties(*new_storage);
    object->set_map(*new_map, kReleaseStore);
    return;
  }

  int old_number_of_fields;
  int number_of_fields = new_map->NumberOfFields(ConcurrencyMode::kSynchronous);
  int inobject = new_map->GetInObjectProperties();
  int unused = new_map->UnusedPropertyFields();

  // Nothing to do if no functions were converted to fields and no smis were
  // converted to doubles.
  if (!old_map->InstancesNeedRewriting(*new_map, number_of_fields, inobject,
                                       unused, &old_number_of_fields,
                                       ConcurrencyMode::kSynchronous)) {
    object->set_map(*new_map, kReleaseStore);
    return;
  }

  int total_size = number_of_fields + unused;
  int external = total_size - inobject;
  Handle<PropertyArray> array = isolate->factory()->NewPropertyArray(external);

  // We use this array to temporarily store the inobject properties.
  Handle<FixedArray> inobject_props =
      isolate->factory()->NewFixedArray(inobject);

  Handle<DescriptorArray> old_descriptors(
      old_map->instance_descriptors(isolate), isolate);
  Handle<DescriptorArray> new_descriptors(
      new_map->instance_descriptors(isolate), isolate);
  int old_nof = old_map->NumberOfOwnDescriptors();
  int new_nof = new_map->NumberOfOwnDescriptors();

  // This method only supports generalizing instances to at least the same
  // number of properties.
  DCHECK(old_nof <= new_nof);

  for (InternalIndex i : InternalIndex::Range(old_nof)) {
    PropertyDetails details = new_descriptors->GetDetails(i);
    if (details.location() != PropertyLocation::kField) continue;
    DCHECK_EQ(PropertyKind::kData, details.kind());
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    Representation old_representation = old_details.representation();
    Representation representation = details.representation();
    Handle<Object> value;
    if (old_details.location() == PropertyLocation::kDescriptor) {
      if (old_details.kind() == PropertyKind::kAccessor) {
        // In case of kAccessor -> kData property reconfiguration, the property
        // must already be prepared for data of certain type.
        DCHECK(!details.representation().IsNone());
        if (details.representation().IsDouble()) {
          value = isolate->factory()->NewHeapNumberWithHoleNaN();
        } else {
          value = isolate->factory()->uninitialized_value();
        }
      } else {
        DCHECK_EQ(PropertyKind::kData, old_details.kind());
        value = handle(old_descriptors->GetStrongValue(isolate, i), isolate);
        DCHECK(!old_representation.IsDouble() && !representation.IsDouble());
      }
    } else {
      DCHECK_EQ(PropertyLocation::kField, old_details.location());
      FieldIndex index = FieldIndex::ForDescriptor(isolate, *old_map, i);
      value = handle(object->RawFastPropertyAt(isolate, index), isolate);
      if (!old_representation.IsDouble() && representation.IsDouble()) {
        DCHECK_IMPLIES(old_representation.IsNone(),
                       value->IsUninitialized(isolate));
        value = Object::NewStorageFor(isolate, value, representation);
      } else if (old_representation.IsDouble() && !representation.IsDouble()) {
        value = Object::WrapForRead(isolate, value, old_representation);
      }
    }
    DCHECK(!(representation.IsDouble() && value->IsSmi()));
    int target_index = new_descriptors->GetFieldIndex(i);
    if (target_index < inobject) {
      inobject_props->set(target_index, *value);
    } else {
      array->set(target_index - inobject, *value);
    }
  }

  for (InternalIndex i : InternalIndex::Range(old_nof, new_nof)) {
    PropertyDetails details = new_descriptors->GetDetails(i);
    if (details.location() != PropertyLocation::kField) continue;
    DCHECK_EQ(PropertyKind::kData, details.kind());
    Handle<Object> value;
    if (details.representation().IsDouble()) {
      value = isolate->factory()->NewHeapNumberWithHoleNaN();
    } else {
      value = isolate->factory()->uninitialized_value();
    }
    int target_index = new_descriptors->GetFieldIndex(i);
    if (target_index < inobject) {
      inobject_props->set(target_index, *value);
    } else {
      array->set(target_index - inobject, *value);
    }
  }

  // From here on we cannot fail and we shouldn't GC anymore.
  DisallowGarbageCollection no_gc;

  Heap* heap = isolate->heap();

  // Copy (real) inobject properties. If necessary, stop at number_of_fields to
  // avoid overwriting |one_pointer_filler_map|.
  int limit = std::min(inobject, number_of_fields);
  for (int i = 0; i < limit; i++) {
    FieldIndex index = FieldIndex::ForPropertyIndex(*new_map, i);
    Object value = inobject_props->get(isolate, i);
    object->FastPropertyAtPut(index, value);
  }

  object->SetProperties(*array);

  // Create filler object past the new instance size.
  int old_instance_size = old_map->instance_size();
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = old_instance_size - new_instance_size;
  DCHECK_GE(instance_size_delta, 0);

  if (instance_size_delta > 0) {
    Address address = object->address();
    heap->CreateFillerObjectAt(address + new_instance_size, instance_size_delta,
                               ClearRecordedSlots::kYes);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  object->set_map(*new_map, kReleaseStore);
}

void MigrateFastToSlow(Isolate* isolate, Handle<JSObject> object,
                       Handle<Map> new_map,
                       int expected_additional_properties) {
  // The global object is always normalized.
  DCHECK(!object->IsJSGlobalObject(isolate));
  // JSGlobalProxy must never be normalized
  DCHECK(!object->IsJSGlobalProxy(isolate));

  DCHECK_IMPLIES(new_map->is_prototype_map(),
                 Map::IsPrototypeChainInvalidated(*new_map));

  HandleScope scope(isolate);
  Handle<Map> map(object->map(isolate), isolate);

  // Allocate new content.
  int real_size = map->NumberOfOwnDescriptors();
  int property_count = real_size;
  if (expected_additional_properties > 0) {
    property_count += expected_additional_properties;
  } else {
    // Make space for two more properties.
    int initial_capacity = V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL
                               ? SwissNameDictionary::kInitialCapacity
                               : NameDictionary::kInitialCapacity;
    property_count += initial_capacity;
  }

  Handle<NameDictionary> dictionary;
  Handle<SwissNameDictionary> ord_dictionary;
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    ord_dictionary = isolate->factory()->NewSwissNameDictionary(property_count);
  } else {
    dictionary = isolate->factory()->NewNameDictionary(property_count);
  }

  Handle<DescriptorArray> descs(map->instance_descriptors(isolate), isolate);
  for (InternalIndex i : InternalIndex::Range(real_size)) {
    PropertyDetails details = descs->GetDetails(i);
    Handle<Name> key(descs->GetKey(isolate, i), isolate);
    Handle<Object> value;
    if (details.location() == PropertyLocation::kField) {
      FieldIndex index = FieldIndex::ForDescriptor(isolate, *map, i);
      if (details.kind() == PropertyKind::kData) {
        value = handle(object->RawFastPropertyAt(isolate, index), isolate);
        if (details.representation().IsDouble()) {
          DCHECK(value->IsHeapNumber(isolate));
          double old_value = Handle<HeapNumber>::cast(value)->value();
          value = isolate->factory()->NewHeapNumber(old_value);
        }
      } else {
        DCHECK_EQ(PropertyKind::kAccessor, details.kind());
        value = handle(object->RawFastPropertyAt(isolate, index), isolate);
      }

    } else {
      DCHECK_EQ(PropertyLocation::kDescriptor, details.location());
      value = handle(descs->GetStrongValue(isolate, i), isolate);
    }
    DCHECK(!value.is_null());
    PropertyConstness constness = V8_DICT_PROPERTY_CONST_TRACKING_BOOL
                                      ? details.constness()
                                      : PropertyConstness::kMutable;
    PropertyDetails d(details.kind(), details.attributes(), constness);

    if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
      ord_dictionary =
          SwissNameDictionary::Add(isolate, ord_dictionary, key, value, d);
    } else {
      dictionary = NameDictionary::Add(isolate, dictionary, key, value, d);
    }
  }

  if (!V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    // Copy the next enumeration index from instance descriptor.
    dictionary->set_next_enumeration_index(real_size + 1);
  }

  // From here on we cannot fail and we shouldn't GC anymore.
  DisallowGarbageCollection no_gc;

  Heap* heap = isolate->heap();

  // Resize the object in the heap if necessary.
  int old_instance_size = map->instance_size();
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = old_instance_size - new_instance_size;
  DCHECK_GE(instance_size_delta, 0);

  if (instance_size_delta > 0) {
    heap->CreateFillerObjectAt(object->address() + new_instance_size,
                               instance_size_delta, ClearRecordedSlots::kYes);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  object->set_map(*new_map, kReleaseStore);

  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    object->SetProperties(*ord_dictionary);
  } else {
    object->SetProperties(*dictionary);
  }

  // Ensure that in-object space of slow-mode object does not contain random
  // garbage.
  int inobject_properties = new_map->GetInObjectProperties();
  if (inobject_properties) {
    for (int i = 0; i < inobject_properties; i++) {
      FieldIndex index = FieldIndex::ForPropertyIndex(*new_map, i);
      object->FastPropertyAtPut(index, Smi::zero());
    }
  }

  isolate->counters()->props_to_dictionary()->Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    StdoutStream os;
    os << "Object properties have been normalized:\n";
    object->Print(os);
  }
#endif
}

}  // namespace

void JSObject::MigrateToMap(Isolate* isolate, Handle<JSObject> object,
                            Handle<Map> new_map,
                            int expected_additional_properties) {
  if (object->map(isolate) == *new_map) return;
  Handle<Map> old_map(object->map(isolate), isolate);
  NotifyMapChange(old_map, new_map, isolate);

  if (old_map->is_dictionary_map()) {
    // For slow-to-fast migrations JSObject::MigrateSlowToFast()
    // must be used instead.
    CHECK(new_map->is_dictionary_map());

    // Slow-to-slow migration is trivial.
    object->set_map(*new_map, kReleaseStore);
  } else if (!new_map->is_dictionary_map()) {
    MigrateFastToFast(isolate, object, new_map);
    if (old_map->is_prototype_map()) {
      DCHECK(!old_map->is_stable());
      DCHECK(new_map->is_stable());
      DCHECK(new_map->owns_descriptors());
      DCHECK(old_map->owns_descriptors());
      // Transfer ownership to the new map. Keep the descriptor pointer of the
      // old map intact because the concurrent marker might be iterating the
      // object with the old map.
      old_map->set_owns_descriptors(false);
      DCHECK(old_map->is_abandoned_prototype_map());
      // Ensure that no transition was inserted for prototype migrations.
      DCHECK_EQ(0,
                TransitionsAccessor(isolate, *old_map).NumberOfTransitions());
      DCHECK(new_map->GetBackPointer(isolate).IsUndefined(isolate));
      DCHECK(object->map(isolate) != *old_map);
    }
  } else {
    MigrateFastToSlow(isolate, object, new_map, expected_additional_properties);
  }

  // Careful: Don't allocate here!
  // For some callers of this method, |object| might be in an inconsistent
  // state now: the new map might have a new elements_kind, but the object's
  // elements pointer hasn't been updated yet. Callers will fix this, but in
  // the meantime, (indirectly) calling JSObjectVerify() must be avoided.
  // When adding code here, add a DisallowGarbageCollection too.
}

void JSObject::ForceSetPrototype(Isolate* isolate, Handle<JSObject> object,
                                 Handle<HeapObject> proto) {
  // object.__proto__ = proto;
  Handle<Map> old_map = Handle<Map>(object->map(), isolate);
  Handle<Map> new_map = Map::Copy(isolate, old_map, "ForceSetPrototype");
  Map::SetPrototype(isolate, new_map, proto);
  JSObject::MigrateToMap(isolate, object, new_map);
}

Maybe<bool> JSObject::SetPropertyWithInterceptor(
    LookupIterator* it, Maybe<ShouldThrow> should_throw, Handle<Object> value) {
  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  return SetPropertyWithInterceptorInternal(it, it->GetInterceptor(),
                                            should_throw, value);
}

Handle<Map> JSObject::GetElementsTransitionMap(Handle<JSObject> object,
                                               ElementsKind to_kind) {
  Handle<Map> map(object->map(), object->GetIsolate());
  return Map::TransitionElementsTo(object->GetIsolate(), map, to_kind);
}

void JSObject::AllocateStorageForMap(Handle<JSObject> object, Handle<Map> map) {
  DCHECK(object->map().GetInObjectProperties() == map->GetInObjectProperties());
  ElementsKind obj_kind = object->map().elements_kind();
  ElementsKind map_kind = map->elements_kind();
  Isolate* isolate = object->GetIsolate();
  if (map_kind != obj_kind) {
    ElementsKind to_kind = GetMoreGeneralElementsKind(map_kind, obj_kind);
    if (IsDictionaryElementsKind(obj_kind)) {
      to_kind = obj_kind;
    }
    if (IsDictionaryElementsKind(to_kind)) {
      NormalizeElements(object);
    } else {
      TransitionElementsKind(object, to_kind);
    }
    map = MapUpdater{isolate, map}.ReconfigureElementsKind(to_kind);
  }
  int number_of_fields = map->NumberOfFields(ConcurrencyMode::kSynchronous);
  int inobject = map->GetInObjectProperties();
  int unused = map->UnusedPropertyFields();
  int total_size = number_of_fields + unused;
  int external = total_size - inobject;
  // Allocate mutable double boxes if necessary. It is always necessary if we
  // have external properties, but is also necessary if we only have inobject
  // properties but don't unbox double fields.

  Handle<DescriptorArray> descriptors(map->instance_descriptors(isolate),
                                      isolate);
  Handle<FixedArray> storage = isolate->factory()->NewFixedArray(inobject);

  Handle<PropertyArray> array = isolate->factory()->NewPropertyArray(external);

  for (InternalIndex i : map->IterateOwnDescriptors()) {
    PropertyDetails details = descriptors->GetDetails(i);
    Representation representation = details.representation();
    if (!representation.IsDouble()) continue;
    FieldIndex index = FieldIndex::ForDescriptor(*map, i);
    auto box = isolate->factory()->NewHeapNumberWithHoleNaN();
    if (index.is_inobject()) {
      storage->set(index.property_index(), *box);
    } else {
      array->set(index.outobject_array_index(), *box);
    }
  }

  object->SetProperties(*array);
  for (int i = 0; i < inobject; i++) {
    FieldIndex index = FieldIndex::ForPropertyIndex(*map, i);
    Object value = storage->get(i);
    object->FastPropertyAtPut(index, value);
  }
  object->set_map(*map, kReleaseStore);
}

void JSObject::MigrateInstance(Isolate* isolate, Handle<JSObject> object) {
  Handle<Map> original_map(object->map(), isolate);
  Handle<Map> map = Map::Update(isolate, original_map);
  map->set_is_migration_target(true);
  JSObject::MigrateToMap(isolate, object, map);
  if (FLAG_trace_migration) {
    object->PrintInstanceMigration(stdout, *original_map, *map);
  }
#if VERIFY_HEAP
  if (FLAG_verify_heap) {
    object->JSObjectVerify(isolate);
  }
#endif
}

// static
bool JSObject::TryMigrateInstance(Isolate* isolate, Handle<JSObject> object) {
  DisallowDeoptimization no_deoptimization(isolate);
  Handle<Map> original_map(object->map(), isolate);
  Handle<Map> new_map;
  if (!Map::TryUpdate(isolate, original_map).ToHandle(&new_map)) {
    return false;
  }
  JSObject::MigrateToMap(isolate, object, new_map);
  if (FLAG_trace_migration && *original_map != object->map()) {
    object->PrintInstanceMigration(stdout, *original_map, object->map());
  }
#if VERIFY_HEAP
  if (FLAG_verify_heap) {
    object->JSObjectVerify(isolate);
  }
#endif
  return true;
}

void JSObject::AddProperty(Isolate* isolate, Handle<JSObject> object,
                           Handle<Name> name, Handle<Object> value,
                           PropertyAttributes attributes) {
  LookupIterator it(isolate, object, name, object,
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
#ifdef DEBUG
  uint32_t index;
  DCHECK(!object->IsJSProxy());
  DCHECK(!name->AsArrayIndex(&index));
  Maybe<PropertyAttributes> maybe = GetPropertyAttributes(&it);
  DCHECK(maybe.IsJust());
  DCHECK(!it.IsFound());
  DCHECK(object->map().is_extensible() || name->IsPrivate());
#endif
  CHECK(Object::AddDataProperty(&it, value, attributes,
                                Just(ShouldThrow::kThrowOnError),
                                StoreOrigin::kNamed)
            .IsJust());
}

void JSObject::AddProperty(Isolate* isolate, Handle<JSObject> object,
                           const char* name, Handle<Object> value,
                           PropertyAttributes attributes) {
  JSObject::AddProperty(isolate, object,
                        isolate->factory()->InternalizeUtf8String(name), value,
                        attributes);
}

// Reconfigures a property to a data property with attributes, even if it is not
// reconfigurable.
// Requires a LookupIterator that does not look at the prototype chain beyond
// hidden prototypes.
MaybeHandle<Object> JSObject::DefineOwnPropertyIgnoreAttributes(
    LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
    AccessorInfoHandling handling, EnforceDefineSemantics semantics) {
  MAYBE_RETURN_NULL(DefineOwnPropertyIgnoreAttributes(
      it, value, attributes, Just(ShouldThrow::kThrowOnError), handling,
      semantics));
  return value;
}

Maybe<bool> JSObject::DefineOwnPropertyIgnoreAttributes(
    LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
    Maybe<ShouldThrow> should_throw, AccessorInfoHandling handling,
    EnforceDefineSemantics semantics, StoreOrigin store_origin) {
  it->UpdateProtector();

  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::JSPROXY:
      case LookupIterator::TRANSITION:
      case LookupIterator::NOT_FOUND:
        UNREACHABLE();

      case LookupIterator::ACCESS_CHECK:
        if (!it->HasAccess()) {
          it->isolate()->ReportFailedAccessCheck(it->GetHolder<JSObject>());
          RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
          return Just(true);
        }
        break;

      // If there's an interceptor, try to store the property with the
      // interceptor.
      // In case of success, the attributes will have been reset to the default
      // attributes of the interceptor, rather than the incoming attributes.
      //
      // TODO(verwaest): JSProxy afterwards verify the attributes that the
      // JSProxy claims it has, and verifies that they are compatible. If not,
      // they throw. Here we should do the same.
      case LookupIterator::INTERCEPTOR: {
        Maybe<bool> result = Just(false);
        if (semantics == EnforceDefineSemantics::kDefine) {
          PropertyDescriptor descriptor;
          descriptor.set_configurable((attributes & DONT_DELETE) != 0);
          descriptor.set_enumerable((attributes & DONT_ENUM) != 0);
          descriptor.set_writable((attributes & READ_ONLY) != 0);
          descriptor.set_value(value);
          result = DefinePropertyWithInterceptorInternal(
              it, it->GetInterceptor(), should_throw, &descriptor);
        } else {
          DCHECK_EQ(semantics, EnforceDefineSemantics::kSet);
          if (handling == DONT_FORCE_FIELD) {
            result =
                JSObject::SetPropertyWithInterceptor(it, should_throw, value);
          }
        }
        if (result.IsNothing() || result.FromJust()) return result;

        if (semantics == EnforceDefineSemantics::kDefine) {
          it->Restart();
          Maybe<bool> can_define = JSReceiver::CheckIfCanDefine(
              it->isolate(), it, value, should_throw);
          if (can_define.IsNothing() || !can_define.FromJust()) {
            return can_define;
          }
        }

        // The interceptor declined to handle the operation, so proceed defining
        // own property without the interceptor.
        Isolate* isolate = it->isolate();
        Handle<Object> receiver = it->GetReceiver();
        LookupIterator::Configuration c = LookupIterator::OWN_SKIP_INTERCEPTOR;
        LookupIterator own_lookup =
            it->IsElement() ? LookupIterator(isolate, receiver, it->index(), c)
                            : LookupIterator(isolate, receiver, it->name(), c);
        return JSObject::DefineOwnPropertyIgnoreAttributes(
            &own_lookup, value, attributes, should_throw, handling, semantics,
            store_origin);
      }

      case LookupIterator::ACCESSOR: {
        Handle<Object> accessors = it->GetAccessors();

        // Special handling for AccessorInfo, which behaves like a data
        // property.
        if (accessors->IsAccessorInfo() && handling == DONT_FORCE_FIELD) {
          PropertyAttributes current_attributes = it->property_attributes();
          // Ensure the context isn't changed after calling into accessors.
          AssertNoContextChange ncc(it->isolate());

          // Update the attributes before calling the setter. The setter may
          // later change the shape of the property.
          if (current_attributes != attributes) {
            it->TransitionToAccessorPair(accessors, attributes);
          }

          return Object::SetPropertyWithAccessor(it, value, should_throw);
        }

        it->ReconfigureDataProperty(value, attributes);
        return Just(true);
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return Object::RedefineIncompatibleProperty(
            it->isolate(), it->GetName(), value, should_throw);

      case LookupIterator::DATA: {
        // Regular property update if the attributes match.
        if (it->property_attributes() == attributes) {
          return Object::SetDataProperty(it, value);
        }

        // The non-matching attribute case for JSTypedArrays has already been
        // handled by JSTypedArray::DefineOwnProperty.
        DCHECK(!it->IsElement() ||
               !Handle<JSObject>::cast(it->GetReceiver())
                    ->HasTypedArrayOrRabGsabTypedArrayElements());
        // Reconfigure the data property if the attributes mismatch.
        it->ReconfigureDataProperty(value, attributes);

        return Just(true);
      }
    }
  }

  return Object::AddDataProperty(it, value, attributes, should_throw,
                                 store_origin, semantics);
}

MaybeHandle<Object> JSObject::SetOwnPropertyIgnoreAttributes(
    Handle<JSObject> object, Handle<Name> name, Handle<Object> value,
    PropertyAttributes attributes) {
  DCHECK(!value->IsTheHole());
  LookupIterator it(object->GetIsolate(), object, name, object,
                    LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

MaybeHandle<Object> JSObject::SetOwnElementIgnoreAttributes(
    Handle<JSObject> object, size_t index, Handle<Object> value,
    PropertyAttributes attributes) {
  DCHECK(!object->IsJSTypedArray());
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, object, LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

MaybeHandle<Object> JSObject::DefinePropertyOrElementIgnoreAttributes(
    Handle<JSObject> object, Handle<Name> name, Handle<Object> value,
    PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();
  PropertyKey key(isolate, name);
  LookupIterator it(isolate, object, key, object, LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

Maybe<PropertyAttributes> JSObject::GetPropertyAttributesWithInterceptor(
    LookupIterator* it) {
  return GetPropertyAttributesWithInterceptorInternal(it, it->GetInterceptor());
}

void JSObject::NormalizeProperties(Isolate* isolate, Handle<JSObject> object,
                                   PropertyNormalizationMode mode,
                                   int expected_additional_properties,
                                   const char* reason) {
  if (!object->HasFastProperties()) return;

  Handle<Map> map(object->map(), isolate);
  Handle<Map> new_map =
      Map::Normalize(isolate, map, map->elements_kind(), mode, reason);

  JSObject::MigrateToMap(isolate, object, new_map,
                         expected_additional_properties);
}

void JSObject::MigrateSlowToFast(Handle<JSObject> object,
                                 int unused_property_fields,
                                 const char* reason) {
  if (object->HasFastProperties()) return;
  DCHECK(!object->IsJSGlobalObject());
  Isolate* isolate = object->GetIsolate();
  Factory* factory = isolate->factory();

  Handle<NameDictionary> dictionary;
  Handle<SwissNameDictionary> swiss_dictionary;
  int number_of_elements;
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    swiss_dictionary = handle(object->property_dictionary_swiss(), isolate);
    number_of_elements = swiss_dictionary->NumberOfElements();
  } else {
    dictionary = handle(object->property_dictionary(), isolate);
    number_of_elements = dictionary->NumberOfElements();
  }

  // Make sure we preserve dictionary representation if there are too many
  // descriptors.
  if (number_of_elements > kMaxNumberOfDescriptors) return;

  Handle<FixedArray> iteration_order;
  int iteration_length;
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    // |iteration_order| remains empty handle, we don't need it.
    iteration_length = swiss_dictionary->UsedCapacity();
  } else {
    iteration_order = NameDictionary::IterationIndices(isolate, dictionary);
    iteration_length = dictionary->NumberOfElements();
  }

  int number_of_fields = 0;

  // Compute the length of the instance descriptor.
  ReadOnlyRoots roots(isolate);
  for (int i = 0; i < iteration_length; i++) {
    PropertyKind kind;
    if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
      InternalIndex index(swiss_dictionary->EntryForEnumerationIndex(i));
      Object key = swiss_dictionary->KeyAt(index);
      if (!SwissNameDictionary::IsKey(roots, key)) {
        // Ignore deleted entries.
        continue;
      }
      kind = swiss_dictionary->DetailsAt(index).kind();
    } else {
      InternalIndex index(Smi::ToInt(iteration_order->get(i)));
      DCHECK(dictionary->IsKey(roots, dictionary->KeyAt(isolate, index)));
      kind = dictionary->DetailsAt(index).kind();
    }

    if (kind == PropertyKind::kData) {
      number_of_fields += 1;
    }
  }

  Handle<Map> old_map(object->map(), isolate);

  int inobject_props = old_map->GetInObjectProperties();

  // Allocate new map.
  Handle<Map> new_map = Map::CopyDropDescriptors(isolate, old_map);
  // We should not only set this bit if we need to. We should not retain the
  // old bit because turning a map into dictionary always sets this bit.
  new_map->set_may_have_interesting_symbols(new_map->has_named_interceptor() ||
                                            new_map->is_access_check_needed());
  new_map->set_is_dictionary_map(false);

  NotifyMapChange(old_map, new_map, isolate);

  if (number_of_elements == 0) {
    DisallowGarbageCollection no_gc;
    DCHECK_LE(unused_property_fields, inobject_props);
    // Transform the object.
    new_map->SetInObjectUnusedPropertyFields(inobject_props);
    object->set_map(*new_map, kReleaseStore);
    object->SetProperties(ReadOnlyRoots(isolate).empty_fixed_array());
    // Check that it really works.
    DCHECK(object->HasFastProperties());
    if (FLAG_log_maps) {
      LOG(isolate, MapEvent("SlowToFast", old_map, new_map, reason));
    }
    return;
  }

  // Allocate the instance descriptor.
  Handle<DescriptorArray> descriptors =
      DescriptorArray::Allocate(isolate, number_of_elements, 0);

  int number_of_allocated_fields =
      number_of_fields + unused_property_fields - inobject_props;
  if (number_of_allocated_fields < 0) {
    // There is enough inobject space for all fields (including unused).
    number_of_allocated_fields = 0;
    unused_property_fields = inobject_props - number_of_fields;
  }

  // Allocate the property array for the fields.
  Handle<PropertyArray> fields =
      factory->NewPropertyArray(number_of_allocated_fields);

  bool is_transitionable_elements_kind =
      IsTransitionableFastElementsKind(old_map->elements_kind());

  // Fill in the instance descriptor and the fields.
  int current_offset = 0;
  int descriptor_index = 0;
  for (int i = 0; i < iteration_length; i++) {
    Name k;
    Object value;
    PropertyDetails details = PropertyDetails::Empty();

    if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
      InternalIndex index(swiss_dictionary->EntryForEnumerationIndex(i));
      Object key_obj = swiss_dictionary->KeyAt(index);
      if (!SwissNameDictionary::IsKey(roots, key_obj)) {
        continue;
      }
      k = Name::cast(key_obj);

      value = swiss_dictionary->ValueAt(index);
      details = swiss_dictionary->DetailsAt(index);
    } else {
      InternalIndex index(Smi::ToInt(iteration_order->get(i)));
      k = dictionary->NameAt(index);

      value = dictionary->ValueAt(index);
      details = dictionary->DetailsAt(index);
    }

    // Dictionary keys are internalized upon insertion.
    // TODO(jkummerow): Turn this into a DCHECK if it's not hit in the wild.
    CHECK(k.IsUniqueName());
    Handle<Name> key(k, isolate);

    // Properly mark the {new_map} if the {key} is an "interesting symbol".
    if (key->IsInterestingSymbol()) {
      new_map->set_may_have_interesting_symbols(true);
    }

    DCHECK_EQ(PropertyLocation::kField, details.location());
    DCHECK_IMPLIES(!V8_DICT_PROPERTY_CONST_TRACKING_BOOL,
                   details.constness() == PropertyConstness::kMutable);

    Descriptor d;
    if (details.kind() == PropertyKind::kData) {
      // Ensure that we make constant field only when elements kind is not
      // transitionable.
      PropertyConstness constness = is_transitionable_elements_kind
                                        ? PropertyConstness::kMutable
                                        : PropertyConstness::kConst;
      // TODO(v8:11248): Consider always setting constness to kMutable
      // once all prototypes stay in dictionary mode and we are not interested
      // in tracking constness for fast mode properties anymore.

      d = Descriptor::DataField(
          key, current_offset, details.attributes(), constness,
          // TODO(verwaest): value->OptimalRepresentation();
          Representation::Tagged(), MaybeObjectHandle(FieldType::Any(isolate)));
    } else {
      DCHECK_EQ(PropertyKind::kAccessor, details.kind());
      d = Descriptor::AccessorConstant(key, handle(value, isolate),
                                       details.attributes());
    }
    details = d.GetDetails();
    if (details.location() == PropertyLocation::kField) {
      if (current_offset < inobject_props) {
        object->InObjectPropertyAtPut(current_offset, value,
                                      UPDATE_WRITE_BARRIER);
      } else {
        int offset = current_offset - inobject_props;
        fields->set(offset, value);
      }
      current_offset += details.field_width_in_words();
    }
    descriptors->Set(InternalIndex(descriptor_index++), &d);
  }
  DCHECK_EQ(current_offset, number_of_fields);
  DCHECK_EQ(descriptor_index, number_of_elements);

  descriptors->Sort();

  DisallowGarbageCollection no_gc;
  new_map->InitializeDescriptors(isolate, *descriptors);
  if (number_of_allocated_fields == 0) {
    new_map->SetInObjectUnusedPropertyFields(unused_property_fields);
  } else {
    new_map->SetOutOfObjectUnusedPropertyFields(unused_property_fields);
  }

  if (FLAG_log_maps) {
    LOG(isolate, MapEvent("SlowToFast", old_map, new_map, reason));
  }
  // Transform the object.
  object->set_map(*new_map, kReleaseStore);

  object->SetProperties(*fields);
  DCHECK(object->IsJSObject());

  // Check that it really works.
  DCHECK(object->HasFastProperties());
}

void JSObject::RequireSlowElements(NumberDictionary dictionary) {
  DCHECK_NE(dictionary,
            ReadOnlyRoots(GetIsolate()).empty_slow_element_dictionary());
  if (dictionary.requires_slow_elements()) return;
  dictionary.set_requires_slow_elements();
  if (map().is_prototype_map()) {
    // If this object is a prototype (the callee will check), invalidate any
    // prototype chains involving it.
    InvalidatePrototypeChains(map());
  }
}

Handle<NumberDictionary> JSObject::NormalizeElements(Handle<JSObject> object) {
  DCHECK(!object->HasTypedArrayOrRabGsabTypedArrayElements());
  Isolate* isolate = object->GetIsolate();
  bool is_sloppy_arguments = object->HasSloppyArgumentsElements();
  {
    DisallowGarbageCollection no_gc;
    FixedArrayBase elements = object->elements();

    if (is_sloppy_arguments) {
      elements = SloppyArgumentsElements::cast(elements).arguments();
    }

    if (elements.IsNumberDictionary()) {
      return handle(NumberDictionary::cast(elements), isolate);
    }
  }

  DCHECK(object->HasSmiOrObjectElements() || object->HasDoubleElements() ||
         object->HasFastArgumentsElements() ||
         object->HasFastStringWrapperElements() ||
         object->HasSealedElements() || object->HasNonextensibleElements());

  Handle<NumberDictionary> dictionary =
      object->GetElementsAccessor()->Normalize(object);

  // Switch to using the dictionary as the backing storage for elements.
  ElementsKind target_kind = is_sloppy_arguments
                                 ? SLOW_SLOPPY_ARGUMENTS_ELEMENTS
                                 : object->HasFastStringWrapperElements()
                                       ? SLOW_STRING_WRAPPER_ELEMENTS
                                       : DICTIONARY_ELEMENTS;
  Handle<Map> new_map = JSObject::GetElementsTransitionMap(object, target_kind);
  // Set the new map first to satify the elements type assert in set_elements().
  JSObject::MigrateToMap(isolate, object, new_map);

  if (is_sloppy_arguments) {
    SloppyArgumentsElements::cast(object->elements())
        .set_arguments(*dictionary);
  } else {
    object->set_elements(*dictionary);
  }

  isolate->counters()->elements_to_dictionary()->Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    StdoutStream os;
    os << "Object elements have been normalized:\n";
    object->Print(os);
  }
#endif

  DCHECK(object->HasDictionaryElements() ||
         object->HasSlowArgumentsElements() ||
         object->HasSlowStringWrapperElements());
  return dictionary;
}

Maybe<bool> JSObject::DeletePropertyWithInterceptor(LookupIterator* it,
                                                    ShouldThrow should_throw) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  Handle<InterceptorInfo> interceptor(it->GetInterceptor());
  if (interceptor->deleter().IsUndefined(isolate)) return Nothing<bool>();

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }

  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, Just(should_throw));
  Handle<Object> result;
  if (it->IsElement(*holder)) {
    result = args.CallIndexedDeleter(interceptor, it->array_index());
  } else {
    result = args.CallNamedDeleter(interceptor, it->name());
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
  if (result.is_null()) return Nothing<bool>();

  DCHECK(result->IsBoolean());
  // Rebox CustomArguments::kReturnValueOffset before returning.
  return Just(result->IsTrue(isolate));
}

Maybe<bool> JSObject::CreateDataProperty(LookupIterator* it,
                                         Handle<Object> value,
                                         Maybe<ShouldThrow> should_throw) {
  DCHECK(it->GetReceiver()->IsJSObject());
  MAYBE_RETURN(JSReceiver::GetPropertyAttributes(it), Nothing<bool>());
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());
  Isolate* isolate = receiver->GetIsolate();

  Maybe<bool> can_define =
      JSReceiver::CheckIfCanDefine(isolate, it, value, should_throw);
  if (can_define.IsNothing() || !can_define.FromJust()) {
    return can_define;
  }

  RETURN_ON_EXCEPTION_VALUE(it->isolate(),
                            DefineOwnPropertyIgnoreAttributes(it, value, NONE),
                            Nothing<bool>());

  return Just(true);
}

namespace {

template <typename Dictionary>
bool TestDictionaryPropertiesIntegrityLevel(Dictionary dict,
                                            ReadOnlyRoots roots,
                                            PropertyAttributes level) {
  DCHECK(level == SEALED || level == FROZEN);

  for (InternalIndex i : dict.IterateEntries()) {
    Object key;
    if (!dict.ToKey(roots, i, &key)) continue;
    if (key.FilterKey(ALL_PROPERTIES)) continue;
    PropertyDetails details = dict.DetailsAt(i);
    if (details.IsConfigurable()) return false;
    if (level == FROZEN && details.kind() == PropertyKind::kData &&
        !details.IsReadOnly()) {
      return false;
    }
  }
  return true;
}

bool TestFastPropertiesIntegrityLevel(Map map, PropertyAttributes level) {
  DCHECK(level == SEALED || level == FROZEN);
  DCHECK(!map.IsCustomElementsReceiverMap());
  DCHECK(!map.is_dictionary_map());

  DescriptorArray descriptors = map.instance_descriptors();
  for (InternalIndex i : map.IterateOwnDescriptors()) {
    if (descriptors.GetKey(i).IsPrivate()) continue;
    PropertyDetails details = descriptors.GetDetails(i);
    if (details.IsConfigurable()) return false;
    if (level == FROZEN && details.kind() == PropertyKind::kData &&
        !details.IsReadOnly()) {
      return false;
    }
  }
  return true;
}

bool TestPropertiesIntegrityLevel(JSObject object, PropertyAttributes level) {
  DCHECK(!object.map().IsCustomElementsReceiverMap());

  if (object.HasFastProperties()) {
    return TestFastPropertiesIntegrityLevel(object.map(), level);
  }

  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    return TestDictionaryPropertiesIntegrityLevel(
        object.property_dictionary_swiss(), object.GetReadOnlyRoots(), level);
  } else {
    return TestDictionaryPropertiesIntegrityLevel(
        object.property_dictionary(), object.GetReadOnlyRoots(), level);
  }
}

bool TestElementsIntegrityLevel(JSObject object, PropertyAttributes level) {
  DCHECK(!object.HasSloppyArgumentsElements());

  ElementsKind kind = object.GetElementsKind();

  if (IsDictionaryElementsKind(kind)) {
    return TestDictionaryPropertiesIntegrityLevel(
        NumberDictionary::cast(object.elements()), object.GetReadOnlyRoots(),
        level);
  }
  if (IsTypedArrayElementsKind(kind)) {
    if (level == FROZEN && JSArrayBufferView::cast(object).byte_length() > 0) {
      return false;  // TypedArrays with elements can't be frozen.
    }
    return TestPropertiesIntegrityLevel(object, level);
  }
  if (IsFrozenElementsKind(kind)) return true;
  if (IsSealedElementsKind(kind) && level != FROZEN) return true;
  if (IsNonextensibleElementsKind(kind) && level == NONE) return true;

  ElementsAccessor* accessor = ElementsAccessor::ForKind(kind);
  // Only DICTIONARY_ELEMENTS and SLOW_SLOPPY_ARGUMENTS_ELEMENTS have
  // PropertyAttributes so just test if empty
  return accessor->NumberOfElements(object) == 0;
}

bool FastTestIntegrityLevel(JSObject object, PropertyAttributes level) {
  DCHECK(!object.map().IsCustomElementsReceiverMap());

  return !object.map().is_extensible() &&
         TestElementsIntegrityLevel(object, level) &&
         TestPropertiesIntegrityLevel(object, level);
}

}  // namespace

Maybe<bool> JSObject::TestIntegrityLevel(Handle<JSObject> object,
                                         IntegrityLevel level) {
  if (!object->map().IsCustomElementsReceiverMap() &&
      !object->HasSloppyArgumentsElements()) {
    return Just(FastTestIntegrityLevel(*object, level));
  }
  return GenericTestIntegrityLevel(Handle<JSReceiver>::cast(object), level);
}

Maybe<bool> JSObject::PreventExtensions(Handle<JSObject> object,
                                        ShouldThrow should_throw) {
  Isolate* isolate = object->GetIsolate();

  if (!object->HasSloppyArgumentsElements()) {
    return PreventExtensionsWithTransition<NONE>(object, should_throw);
  }

  if (object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context(), isolate), object)) {
    isolate->ReportFailedAccessCheck(object);
    RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kNoAccess));
  }

  if (!object->map().is_extensible()) return Just(true);

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return Just(true);
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return PreventExtensions(PrototypeIterator::GetCurrent<JSObject>(iter),
                             should_throw);
  }

  if (object->map().has_named_interceptor() ||
      object->map().has_indexed_interceptor()) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kCannotPreventExt));
  }

  DCHECK(!object->HasTypedArrayOrRabGsabTypedArrayElements());

  // Normalize fast elements.
  Handle<NumberDictionary> dictionary = NormalizeElements(object);
  DCHECK(object->HasDictionaryElements() || object->HasSlowArgumentsElements());

  // Make sure that we never go back to fast case.
  if (*dictionary != ReadOnlyRoots(isolate).empty_slow_element_dictionary()) {
    object->RequireSlowElements(*dictionary);
  }

  // Do a map transition, other objects with this map may still
  // be extensible.
  // TODO(adamk): Extend the NormalizedMapCache to handle non-extensible maps.
  Handle<Map> new_map =
      Map::Copy(isolate, handle(object->map(), isolate), "PreventExtensions");

  new_map->set_is_extensible(false);
  JSObject::MigrateToMap(isolate, object, new_map);
  DCHECK(!object->map().is_extensible());

  return Just(true);
}

bool JSObject::IsExtensible(Handle<JSObject> object) {
  Isolate* isolate = object->GetIsolate();
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context(), isolate), object)) {
    return true;
  }
  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, *object);
    if (iter.IsAtEnd()) return false;
    DCHECK(iter.GetCurrent().IsJSGlobalObject());
    return iter.GetCurrent<JSObject>().map().is_extensible();
  }
  return object->map().is_extensible();
}

// static
MaybeHandle<Object> JSObject::ReadFromOptionsBag(Handle<Object> options,
                                                 Handle<String> option_name,
                                                 Isolate* isolate) {
  if (options->IsJSReceiver()) {
    Handle<JSReceiver> js_options = Handle<JSReceiver>::cast(options);
    return JSObject::GetProperty(isolate, js_options, option_name);
  }
  return MaybeHandle<Object>(isolate->factory()->undefined_value());
}

template <typename Dictionary>
void JSObject::ApplyAttributesToDictionary(
    Isolate* isolate, ReadOnlyRoots roots, Handle<Dictionary> dictionary,
    const PropertyAttributes attributes) {
  for (InternalIndex i : dictionary->IterateEntries()) {
    Object k;
    if (!dictionary->ToKey(roots, i, &k)) continue;
    if (k.FilterKey(ALL_PROPERTIES)) continue;
    PropertyDetails details = dictionary->DetailsAt(i);
    int attrs = attributes;
    // READ_ONLY is an invalid attribute for JS setters/getters.
    if ((attributes & READ_ONLY) && details.kind() == PropertyKind::kAccessor) {
      Object v = dictionary->ValueAt(i);
      if (v.IsAccessorPair()) attrs &= ~READ_ONLY;
    }
    details = details.CopyAddAttributes(PropertyAttributesFromInt(attrs));
    dictionary->DetailsAtPut(i, details);
  }
}

template void JSObject::ApplyAttributesToDictionary(
    Isolate* isolate, ReadOnlyRoots roots, Handle<NumberDictionary> dictionary,
    const PropertyAttributes attributes);

Handle<NumberDictionary> CreateElementDictionary(Isolate* isolate,
                                                 Handle<JSObject> object) {
  Handle<NumberDictionary> new_element_dictionary;
  if (!object->HasTypedArrayOrRabGsabTypedArrayElements() &&
      !object->HasDictionaryElements() &&
      !object->HasSlowStringWrapperElements()) {
    int length = object->IsJSArray()
                     ? Smi::ToInt(Handle<JSArray>::cast(object)->length())
                     : object->elements().length();
    new_element_dictionary =
        length == 0 ? isolate->factory()->empty_slow_element_dictionary()
                    : object->GetElementsAccessor()->Normalize(object);
  }
  return new_element_dictionary;
}

template <PropertyAttributes attrs>
Maybe<bool> JSObject::PreventExtensionsWithTransition(
    Handle<JSObject> object, ShouldThrow should_throw) {
  STATIC_ASSERT(attrs == NONE || attrs == SEALED || attrs == FROZEN);

  // Sealing/freezing sloppy arguments or namespace objects should be handled
  // elsewhere.
  DCHECK(!object->HasSloppyArgumentsElements());
  DCHECK_IMPLIES(object->IsJSModuleNamespace(), attrs == NONE);

  Isolate* isolate = object->GetIsolate();
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context(), isolate), object)) {
    isolate->ReportFailedAccessCheck(object);
    RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kNoAccess));
  }

  if (attrs == NONE && !object->map().is_extensible()) {
    return Just(true);
  }

  {
    ElementsKind old_elements_kind = object->map().elements_kind();
    if (IsFrozenElementsKind(old_elements_kind)) return Just(true);
    if (attrs != FROZEN && IsSealedElementsKind(old_elements_kind)) {
      return Just(true);
    }
  }

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return Just(true);
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return PreventExtensionsWithTransition<attrs>(
        PrototypeIterator::GetCurrent<JSObject>(iter), should_throw);
  }

  if (object->map().has_named_interceptor() ||
      object->map().has_indexed_interceptor()) {
    MessageTemplate message = MessageTemplate::kNone;
    switch (attrs) {
      case NONE:
        message = MessageTemplate::kCannotPreventExt;
        break;

      case SEALED:
        message = MessageTemplate::kCannotSeal;
        break;

      case FROZEN:
        message = MessageTemplate::kCannotFreeze;
        break;
    }
    RETURN_FAILURE(isolate, should_throw, NewTypeError(message));
  }

  Handle<Symbol> transition_marker;
  if (attrs == NONE) {
    transition_marker = isolate->factory()->nonextensible_symbol();
  } else if (attrs == SEALED) {
    transition_marker = isolate->factory()->sealed_symbol();
  } else {
    DCHECK(attrs == FROZEN);
    transition_marker = isolate->factory()->frozen_symbol();
  }

  // Currently, there are only have sealed/frozen Object element kinds and
  // Map::MigrateToMap doesn't handle properties' attributes reconfiguring and
  // elements kind change in one go. If seal or freeze with Smi or Double
  // elements kind, we will transition to Object elements kind first to make
  // sure of valid element access.
  if (FLAG_enable_sealed_frozen_elements_kind) {
    switch (object->map().elements_kind()) {
      case PACKED_SMI_ELEMENTS:
      case PACKED_DOUBLE_ELEMENTS:
        JSObject::TransitionElementsKind(object, PACKED_ELEMENTS);
        break;
      case HOLEY_SMI_ELEMENTS:
      case HOLEY_DOUBLE_ELEMENTS:
        JSObject::TransitionElementsKind(object, HOLEY_ELEMENTS);
        break;
      default:
        break;
    }
  }

  // Make sure we only use this element dictionary in case we can't transition
  // to sealed, frozen elements kind.
  Handle<NumberDictionary> new_element_dictionary;

  Handle<Map> old_map(object->map(), isolate);
  old_map = Map::Update(isolate, old_map);
  Handle<Map> transition_map;
  MaybeHandle<Map> maybe_transition_map =
      TransitionsAccessor::SearchSpecial(isolate, old_map, *transition_marker);
  if (maybe_transition_map.ToHandle(&transition_map)) {
    DCHECK(transition_map->has_dictionary_elements() ||
           transition_map->has_typed_array_or_rab_gsab_typed_array_elements() ||
           transition_map->elements_kind() == SLOW_STRING_WRAPPER_ELEMENTS ||
           transition_map->has_any_nonextensible_elements());
    DCHECK(!transition_map->is_extensible());
    if (!transition_map->has_any_nonextensible_elements()) {
      new_element_dictionary = CreateElementDictionary(isolate, object);
    }
    JSObject::MigrateToMap(isolate, object, transition_map);
  } else if (TransitionsAccessor::CanHaveMoreTransitions(isolate, old_map)) {
    // Create a new descriptor array with the appropriate property attributes
    Handle<Map> new_map = Map::CopyForPreventExtensions(
        isolate, old_map, attrs, transition_marker, "CopyForPreventExtensions");
    if (!new_map->has_any_nonextensible_elements()) {
      new_element_dictionary = CreateElementDictionary(isolate, object);
    }
    JSObject::MigrateToMap(isolate, object, new_map);
  } else {
    DCHECK(old_map->is_dictionary_map() || !old_map->is_prototype_map());
    // Slow path: need to normalize properties for safety
    NormalizeProperties(isolate, object, CLEAR_INOBJECT_PROPERTIES, 0,
                        "SlowPreventExtensions");

    // Create a new map, since other objects with this map may be extensible.
    // TODO(adamk): Extend the NormalizedMapCache to handle non-extensible maps.
    Handle<Map> new_map = Map::Copy(isolate, handle(object->map(), isolate),
                                    "SlowCopyForPreventExtensions");
    new_map->set_is_extensible(false);
    new_element_dictionary = CreateElementDictionary(isolate, object);
    if (!new_element_dictionary.is_null()) {
      ElementsKind new_kind =
          IsStringWrapperElementsKind(old_map->elements_kind())
              ? SLOW_STRING_WRAPPER_ELEMENTS
              : DICTIONARY_ELEMENTS;
      new_map->set_elements_kind(new_kind);
    }
    JSObject::MigrateToMap(isolate, object, new_map);

    if (attrs != NONE) {
      ReadOnlyRoots roots(isolate);
      if (object->IsJSGlobalObject()) {
        Handle<GlobalDictionary> dictionary(
            JSGlobalObject::cast(*object).global_dictionary(kAcquireLoad),
            isolate);
        JSObject::ApplyAttributesToDictionary(isolate, roots, dictionary,
                                              attrs);
      } else if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
        Handle<SwissNameDictionary> dictionary(
            object->property_dictionary_swiss(), isolate);
        JSObject::ApplyAttributesToDictionary(isolate, roots, dictionary,
                                              attrs);
      } else {
        Handle<NameDictionary> dictionary(object->property_dictionary(),
                                          isolate);
        JSObject::ApplyAttributesToDictionary(isolate, roots, dictionary,
                                              attrs);
      }
    }
  }

  if (object->map().has_any_nonextensible_elements()) {
    DCHECK(new_element_dictionary.is_null());
    return Just(true);
  }

  // Both seal and preventExtensions always go through without modifications to
  // typed array elements. Freeze works only if there are no actual elements.
  if (object->HasTypedArrayOrRabGsabTypedArrayElements()) {
    DCHECK(new_element_dictionary.is_null());
    if (attrs == FROZEN && JSTypedArray::cast(*object).GetLength() > 0) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kCannotFreezeArrayBufferView));
      return Nothing<bool>();
    }
    return Just(true);
  }

  DCHECK(object->map().has_dictionary_elements() ||
         object->map().elements_kind() == SLOW_STRING_WRAPPER_ELEMENTS);
  if (!new_element_dictionary.is_null()) {
    object->set_elements(*new_element_dictionary);
  }

  if (object->elements() !=
      ReadOnlyRoots(isolate).empty_slow_element_dictionary()) {
    Handle<NumberDictionary> dictionary(object->element_dictionary(), isolate);
    // Make sure we never go back to the fast case
    object->RequireSlowElements(*dictionary);
    if (attrs != NONE) {
      JSObject::ApplyAttributesToDictionary(isolate, ReadOnlyRoots(isolate),
                                            dictionary, attrs);
    }
  }

  return Just(true);
}

Handle<Object> JSObject::FastPropertyAt(Isolate* isolate,
                                        Handle<JSObject> object,
                                        Representation representation,
                                        FieldIndex index) {
  Handle<Object> raw_value(object->RawFastPropertyAt(index), isolate);
  return Object::WrapForRead(isolate, raw_value, representation);
}

Handle<Object> JSObject::FastPropertyAt(Isolate* isolate,
                                        Handle<JSObject> object,
                                        Representation representation,
                                        FieldIndex index, SeqCstAccessTag tag) {
  Handle<Object> raw_value(object->RawFastPropertyAt(index, tag), isolate);
  return Object::WrapForRead(isolate, raw_value, representation);
}

// static
Handle<Object> JSObject::DictionaryPropertyAt(Isolate* isolate,
                                              Handle<JSObject> object,
                                              InternalIndex dict_index) {
  DCHECK_EQ(ThreadId::Current(), isolate->thread_id());
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    SwissNameDictionary dict = object->property_dictionary_swiss();
    return handle(dict.ValueAt(dict_index), isolate);
  } else {
    NameDictionary dict = object->property_dictionary();
    return handle(dict.ValueAt(dict_index), isolate);
  }
}

// static
base::Optional<Object> JSObject::DictionaryPropertyAt(Handle<JSObject> object,
                                                      InternalIndex dict_index,
                                                      Heap* heap) {
  Object backing_store = object->raw_properties_or_hash(kRelaxedLoad);
  if (!backing_store.IsHeapObject()) return {};
  if (heap->IsPendingAllocation(HeapObject::cast(backing_store))) return {};

  base::Optional<Object> maybe_obj;
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    if (!backing_store.IsSwissNameDictionary()) return {};
    maybe_obj = SwissNameDictionary::cast(backing_store).TryValueAt(dict_index);
  } else {
    if (!backing_store.IsNameDictionary()) return {};
    maybe_obj = NameDictionary::cast(backing_store).TryValueAt(dict_index);
  }

  if (!maybe_obj) return {};
  return maybe_obj.value();
}

// TODO(cbruni/jkummerow): Consider moving this into elements.cc.
bool JSObject::HasEnumerableElements() {
  // TODO(cbruni): cleanup
  JSObject object = *this;
  switch (object.GetElementsKind()) {
    case PACKED_SMI_ELEMENTS:
    case PACKED_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case PACKED_NONEXTENSIBLE_ELEMENTS:
    case PACKED_DOUBLE_ELEMENTS: {
      int length = object.IsJSArray()
                       ? Smi::ToInt(JSArray::cast(object).length())
                       : object.elements().length();
      return length > 0;
    }
    case HOLEY_SMI_ELEMENTS:
    case HOLEY_FROZEN_ELEMENTS:
    case HOLEY_SEALED_ELEMENTS:
    case HOLEY_NONEXTENSIBLE_ELEMENTS:
    case HOLEY_ELEMENTS: {
      FixedArray elements = FixedArray::cast(object.elements());
      int length = object.IsJSArray()
                       ? Smi::ToInt(JSArray::cast(object).length())
                       : elements.length();
      Isolate* isolate = GetIsolate();
      for (int i = 0; i < length; i++) {
        if (!elements.is_the_hole(isolate, i)) return true;
      }
      return false;
    }
    case HOLEY_DOUBLE_ELEMENTS: {
      int length = object.IsJSArray()
                       ? Smi::ToInt(JSArray::cast(object).length())
                       : object.elements().length();
      // Zero-length arrays would use the empty FixedArray...
      if (length == 0) return false;
      // ...so only cast to FixedDoubleArray otherwise.
      FixedDoubleArray elements = FixedDoubleArray::cast(object.elements());
      for (int i = 0; i < length; i++) {
        if (!elements.is_the_hole(i)) return true;
      }
      return false;
    }
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE) {
        size_t length = JSTypedArray::cast(object).length();
        return length > 0;
      }

      RAB_GSAB_TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      {
        size_t length = JSTypedArray::cast(object).GetLength();
        return length > 0;
      }
    case DICTIONARY_ELEMENTS: {
      NumberDictionary elements = NumberDictionary::cast(object.elements());
      return elements.NumberOfEnumerableProperties() > 0;
    }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
      // We're approximating non-empty arguments objects here.
      return true;
    case FAST_STRING_WRAPPER_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
      if (String::cast(JSPrimitiveWrapper::cast(object).value()).length() > 0) {
        return true;
      }
      return object.elements().length() > 0;
    case WASM_ARRAY_ELEMENTS:
      UNIMPLEMENTED();

    case NO_ELEMENTS:
      return false;
  }
  UNREACHABLE();
}

MaybeHandle<Object> JSObject::DefineAccessor(Handle<JSObject> object,
                                             Handle<Name> name,
                                             Handle<Object> getter,
                                             Handle<Object> setter,
                                             PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();

  PropertyKey key(isolate, name);
  LookupIterator it(isolate, object, key, LookupIterator::OWN_SKIP_INTERCEPTOR);
  return DefineAccessor(&it, getter, setter, attributes);
}

MaybeHandle<Object> JSObject::DefineAccessor(LookupIterator* it,
                                             Handle<Object> getter,
                                             Handle<Object> setter,
                                             PropertyAttributes attributes) {
  Isolate* isolate = it->isolate();

  it->UpdateProtector();

  if (it->state() == LookupIterator::ACCESS_CHECK) {
    if (!it->HasAccess()) {
      isolate->ReportFailedAccessCheck(it->GetHolder<JSObject>());
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
      return isolate->factory()->undefined_value();
    }
    it->Next();
  }

  Handle<JSObject> object = Handle<JSObject>::cast(it->GetReceiver());
  // Ignore accessors on typed arrays.
  if (it->IsElement() && object->HasTypedArrayOrRabGsabTypedArrayElements()) {
    return it->factory()->undefined_value();
  }

  DCHECK(getter->IsCallable() || getter->IsUndefined(isolate) ||
         getter->IsNull(isolate) || getter->IsFunctionTemplateInfo());
  DCHECK(setter->IsCallable() || setter->IsUndefined(isolate) ||
         setter->IsNull(isolate) || setter->IsFunctionTemplateInfo());
  it->TransitionToAccessorProperty(getter, setter, attributes);

  return isolate->factory()->undefined_value();
}

MaybeHandle<Object> JSObject::SetAccessor(Handle<JSObject> object,
                                          Handle<Name> name,
                                          Handle<AccessorInfo> info,
                                          PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();

  PropertyKey key(isolate, name);
  LookupIterator it(isolate, object, key, LookupIterator::OWN_SKIP_INTERCEPTOR);

  // Duplicate ACCESS_CHECK outside of GetPropertyAttributes for the case that
  // the FailedAccessCheckCallbackFunction doesn't throw an exception.
  //
  // TODO(verwaest): Force throw an exception if the callback doesn't, so we can
  // remove reliance on default return values.
  if (it.state() == LookupIterator::ACCESS_CHECK) {
    if (!it.HasAccess()) {
      isolate->ReportFailedAccessCheck(object);
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
      return it.factory()->undefined_value();
    }
    it.Next();
  }

  // Ignore accessors on typed arrays.
  if (it.IsElement() && object->HasTypedArrayOrRabGsabTypedArrayElements()) {
    return it.factory()->undefined_value();
  }

  CHECK(GetPropertyAttributes(&it).IsJust());

  // ES5 forbids turning a property into an accessor if it's not
  // configurable. See 8.6.1 (Table 5).
  if (it.IsFound() && !it.IsConfigurable()) {
    return it.factory()->undefined_value();
  }

  it.TransitionToAccessorPair(info, attributes);

  return object;
}

Object JSObject::SlowReverseLookup(Object value) {
  if (HasFastProperties()) {
    DescriptorArray descs = map().instance_descriptors();
    bool value_is_number = value.IsNumber();
    for (InternalIndex i : map().IterateOwnDescriptors()) {
      PropertyDetails details = descs.GetDetails(i);
      if (details.location() == PropertyLocation::kField) {
        DCHECK_EQ(PropertyKind::kData, details.kind());
        FieldIndex field_index = FieldIndex::ForDescriptor(map(), i);
        Object property = RawFastPropertyAt(field_index);
        if (field_index.is_double()) {
          DCHECK(property.IsHeapNumber());
          if (value_is_number && property.Number() == value.Number()) {
            return descs.GetKey(i);
          }
        } else if (property == value) {
          return descs.GetKey(i);
        }
      } else {
        DCHECK_EQ(PropertyLocation::kDescriptor, details.location());
        if (details.kind() == PropertyKind::kData) {
          if (descs.GetStrongValue(i) == value) {
            return descs.GetKey(i);
          }
        }
      }
    }
    return GetReadOnlyRoots().undefined_value();
  } else if (IsJSGlobalObject()) {
    return JSGlobalObject::cast(*this)
        .global_dictionary(kAcquireLoad)
        .SlowReverseLookup(value);
  } else if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    return property_dictionary_swiss().SlowReverseLookup(GetIsolate(), value);
  } else {
    return property_dictionary().SlowReverseLookup(value);
  }
}

void JSObject::PrototypeRegistryCompactionCallback(HeapObject value,
                                                   int old_index,
                                                   int new_index) {
  DCHECK(value.IsMap() && Map::cast(value).is_prototype_map());
  Map map = Map::cast(value);
  DCHECK(map.prototype_info().IsPrototypeInfo());
  PrototypeInfo proto_info = PrototypeInfo::cast(map.prototype_info());
  DCHECK_EQ(old_index, proto_info.registry_slot());
  proto_info.set_registry_slot(new_index);
}

// static
void JSObject::MakePrototypesFast(Handle<Object> receiver,
                                  WhereToStart where_to_start,
                                  Isolate* isolate) {
  if (!receiver->IsJSReceiver()) return;
  for (PrototypeIterator iter(isolate, Handle<JSReceiver>::cast(receiver),
                              where_to_start);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<Object> current = PrototypeIterator::GetCurrent(iter);
    if (!current->IsJSObject()) return;
    Handle<JSObject> current_obj = Handle<JSObject>::cast(current);
    Map current_map = current_obj->map();
    if (current_map.is_prototype_map()) {
      // If the map is already marked as should be fast, we're done. Its
      // prototypes will have been marked already as well.
      if (current_map.should_be_fast_prototype_map()) return;
      Handle<Map> map(current_map, isolate);
      Map::SetShouldBeFastPrototypeMap(map, true, isolate);
      JSObject::OptimizeAsPrototype(current_obj);
    }
  }
}

static bool PrototypeBenefitsFromNormalization(Handle<JSObject> object) {
  DisallowGarbageCollection no_gc;
  if (!object->HasFastProperties()) return false;
  if (object->IsJSGlobalProxy()) return false;
  // TODO(v8:11248) make bootstrapper create dict mode prototypes, too?
  if (object->GetIsolate()->bootstrapper()->IsActive()) return false;
  if (V8_DICT_PROPERTY_CONST_TRACKING_BOOL) return true;
  return !object->map().is_prototype_map() ||
         !object->map().should_be_fast_prototype_map();
}

// static
void JSObject::OptimizeAsPrototype(Handle<JSObject> object,
                                   bool enable_setup_mode) {
  Isolate* isolate = object->GetIsolate();
  if (object->IsJSGlobalObject()) return;
  if (enable_setup_mode && PrototypeBenefitsFromNormalization(object)) {
    // First normalize to ensure all JSFunctions are DATA_CONSTANT.
    JSObject::NormalizeProperties(isolate, object, KEEP_INOBJECT_PROPERTIES, 0,
                                  "NormalizeAsPrototype");
  }
  if (object->map().is_prototype_map()) {
    if (!V8_DICT_PROPERTY_CONST_TRACKING_BOOL &&
        object->map().should_be_fast_prototype_map() &&
        !object->HasFastProperties()) {
      JSObject::MigrateSlowToFast(object, 0, "OptimizeAsPrototype");
    }
  } else {
    Handle<Map> new_map =
        Map::Copy(isolate, handle(object->map(), isolate), "CopyAsPrototype");
    new_map->set_is_prototype_map(true);

    // Replace the pointer to the exact constructor with the Object function
    // from the same context if undetectable from JS. This is to avoid keeping
    // memory alive unnecessarily.
    Object maybe_constructor = new_map->GetConstructor();
    if (maybe_constructor.IsJSFunction()) {
      JSFunction constructor = JSFunction::cast(maybe_constructor);
      if (!constructor.shared().IsApiFunction()) {
        Context context = constructor.native_context();
        JSFunction object_function = context.object_function();
        new_map->SetConstructor(object_function);
      }
    }
    JSObject::MigrateToMap(isolate, object, new_map);

    if (V8_DICT_PROPERTY_CONST_TRACKING_BOOL && !object->HasFastProperties()) {
      ReadOnlyRoots roots(isolate);
      DisallowHeapAllocation no_gc;

      auto make_constant = [&](auto dict) {
        for (InternalIndex index : dict.IterateEntries()) {
          Object k;
          if (!dict.ToKey(roots, index, &k)) continue;

          PropertyDetails details = dict.DetailsAt(index);
          details = details.CopyWithConstness(PropertyConstness::kConst);
          dict.DetailsAtPut(index, details);
        }
      };
      if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
        make_constant(object->property_dictionary_swiss());
      } else {
        make_constant(object->property_dictionary());
      }
    }
  }
#ifdef DEBUG
  bool should_be_dictionary = V8_DICT_PROPERTY_CONST_TRACKING_BOOL &&
                              enable_setup_mode && !object->IsJSGlobalProxy() &&
                              !object->GetIsolate()->bootstrapper()->IsActive();
  DCHECK_IMPLIES(should_be_dictionary, object->map().is_dictionary_map());
#endif
}

// static
void JSObject::ReoptimizeIfPrototype(Handle<JSObject> object) {
  if (!object->map().is_prototype_map()) return;
  if (!object->map().should_be_fast_prototype_map()) return;
  OptimizeAsPrototype(object);
}

// static
void JSObject::LazyRegisterPrototypeUser(Handle<Map> user, Isolate* isolate) {
  // Contract: In line with InvalidatePrototypeChains()'s requirements,
  // leaf maps don't need to register as users, only prototypes do.
  DCHECK(user->is_prototype_map());

  Handle<Map> current_user = user;
  Handle<PrototypeInfo> current_user_info =
      Map::GetOrCreatePrototypeInfo(user, isolate);
  for (PrototypeIterator iter(isolate, user); !iter.IsAtEnd(); iter.Advance()) {
    // Walk up the prototype chain as far as links haven't been registered yet.
    if (current_user_info->registry_slot() != PrototypeInfo::UNREGISTERED) {
      break;
    }
    Handle<Object> maybe_proto = PrototypeIterator::GetCurrent(iter);
    // Proxies on the prototype chain are not supported. They make it
    // impossible to make any assumptions about the prototype chain anyway.
    if (maybe_proto->IsJSProxy()) return;
    Handle<JSObject> proto = Handle<JSObject>::cast(maybe_proto);
    Handle<PrototypeInfo> proto_info =
        Map::GetOrCreatePrototypeInfo(proto, isolate);
    Handle<Object> maybe_registry(proto_info->prototype_users(), isolate);
    Handle<WeakArrayList> registry =
        maybe_registry->IsSmi()
            ? handle(ReadOnlyRoots(isolate->heap()).empty_weak_array_list(),
                     isolate)
            : Handle<WeakArrayList>::cast(maybe_registry);
    int slot = 0;
    Handle<WeakArrayList> new_array =
        PrototypeUsers::Add(isolate, registry, current_user, &slot);
    current_user_info->set_registry_slot(slot);
    if (!maybe_registry.is_identical_to(new_array)) {
      proto_info->set_prototype_users(*new_array);
    }
    if (FLAG_trace_prototype_users) {
      PrintF("Registering %p as a user of prototype %p (map=%p).\n",
             reinterpret_cast<void*>(current_user->ptr()),
             reinterpret_cast<void*>(proto->ptr()),
             reinterpret_cast<void*>(proto->map().ptr()));
    }

    current_user = handle(proto->map(), isolate);
    current_user_info = proto_info;
  }
}

// Can be called regardless of whether |user| was actually registered with
// |prototype|. Returns true when there was a registration.
// static
bool JSObject::UnregisterPrototypeUser(Handle<Map> user, Isolate* isolate) {
  DCHECK(user->is_prototype_map());
  // If it doesn't have a PrototypeInfo, it was never registered.
  if (!user->prototype_info().IsPrototypeInfo()) return false;
  // If it had no prototype before, see if it had users that might expect
  // registration.
  if (!user->prototype().IsJSObject()) {
    Object users =
        PrototypeInfo::cast(user->prototype_info()).prototype_users();
    return users.IsWeakArrayList();
  }
  Handle<JSObject> prototype(JSObject::cast(user->prototype()), isolate);
  Handle<PrototypeInfo> user_info =
      Map::GetOrCreatePrototypeInfo(user, isolate);
  int slot = user_info->registry_slot();
  if (slot == PrototypeInfo::UNREGISTERED) return false;
  DCHECK(prototype->map().is_prototype_map());
  Object maybe_proto_info = prototype->map().prototype_info();
  // User knows its registry slot, prototype info and user registry must exist.
  DCHECK(maybe_proto_info.IsPrototypeInfo());
  Handle<PrototypeInfo> proto_info(PrototypeInfo::cast(maybe_proto_info),
                                   isolate);
  Handle<WeakArrayList> prototype_users(
      WeakArrayList::cast(proto_info->prototype_users()), isolate);
  DCHECK_EQ(prototype_users->Get(slot), HeapObjectReference::Weak(*user));
  PrototypeUsers::MarkSlotEmpty(*prototype_users, slot);
  if (FLAG_trace_prototype_users) {
    PrintF("Unregistering %p as a user of prototype %p.\n",
           reinterpret_cast<void*>(user->ptr()),
           reinterpret_cast<void*>(prototype->ptr()));
  }
  return true;
}

namespace {

// This function must be kept in sync with
// AccessorAssembler::InvalidateValidityCellIfPrototype() which does pre-checks
// before jumping here.
void InvalidateOnePrototypeValidityCellInternal(Map map) {
  DCHECK(map.is_prototype_map());
  if (FLAG_trace_prototype_users) {
    PrintF("Invalidating prototype map %p 's cell\n",
           reinterpret_cast<void*>(map.ptr()));
  }
  Object maybe_cell = map.prototype_validity_cell();
  if (maybe_cell.IsCell()) {
    // Just set the value; the cell will be replaced lazily.
    Cell cell = Cell::cast(maybe_cell);
    cell.set_value(Smi::FromInt(Map::kPrototypeChainInvalid));
  }
  Object maybe_prototype_info = map.prototype_info();
  if (maybe_prototype_info.IsPrototypeInfo()) {
    PrototypeInfo prototype_info = PrototypeInfo::cast(maybe_prototype_info);
    prototype_info.set_prototype_chain_enum_cache(Object());
  }

  // We may inline accesses to constants stored in dictionary mode protoypes in
  // optimized code. When doing so, we install depenendies of group
  // |kPrototypeCheckGroup| on each prototype between the receiver's immediate
  // prototype and the holder of the constant property. This dependency is used
  // both to detect changes to the constant value itself, and other changes to
  // the prototype chain that invalidate the access to the given property from
  // the given receiver (like adding the property to another prototype between
  // the receiver and the (previous) holder). This works by de-opting this group
  // whenever the validity cell would be invalidated. However, the actual value
  // of the validity cell is not used. Therefore, we always trigger the de-opt
  // here, even if the cell was already invalid.
  if (V8_DICT_PROPERTY_CONST_TRACKING_BOOL && map.is_dictionary_map()) {
    // TODO(11527): pass Isolate as an argument.
    Isolate* isolate = GetIsolateFromWritableObject(map);
    map.dependent_code().DeoptimizeDependentCodeGroup(
        isolate, DependentCode::kPrototypeCheckGroup);
  }
}

void InvalidatePrototypeChainsInternal(Map map) {
  // We handle linear prototype chains by looping, and multiple children
  // by recursion, in order to reduce the likelihood of running into stack
  // overflows. So, conceptually, the outer loop iterates the depth of the
  // prototype tree, and the inner loop iterates the breadth of a node.
  Map next_map;
  for (; !map.is_null(); map = next_map, next_map = Map()) {
    InvalidateOnePrototypeValidityCellInternal(map);

    Object maybe_proto_info = map.prototype_info();
    if (!maybe_proto_info.IsPrototypeInfo()) return;
    PrototypeInfo proto_info = PrototypeInfo::cast(maybe_proto_info);
    if (!proto_info.prototype_users().IsWeakArrayList()) {
      return;
    }
    WeakArrayList prototype_users =
        WeakArrayList::cast(proto_info.prototype_users());
    // For now, only maps register themselves as users.
    for (int i = PrototypeUsers::kFirstIndex; i < prototype_users.length();
         ++i) {
      HeapObject heap_object;
      if (prototype_users.Get(i)->GetHeapObjectIfWeak(&heap_object) &&
          heap_object.IsMap()) {
        // Walk the prototype chain (backwards, towards leaf objects) if
        // necessary.
        if (next_map.is_null()) {
          next_map = Map::cast(heap_object);
        } else {
          InvalidatePrototypeChainsInternal(Map::cast(heap_object));
        }
      }
    }
  }
}

}  // namespace

// static
Map JSObject::InvalidatePrototypeChains(Map map) {
  DisallowGarbageCollection no_gc;
  InvalidatePrototypeChainsInternal(map);
  return map;
}

// We also invalidate global objects validity cell when a new lexical
// environment variable is added. This is necessary to ensure that
// Load/StoreGlobalIC handlers that load/store from global object's prototype
// get properly invalidated.
// Note, that the normal Load/StoreICs that load/store through the global object
// in the prototype chain are not affected by appearance of a new lexical
// variable and therefore we don't propagate invalidation down.
// static
void JSObject::InvalidatePrototypeValidityCell(JSGlobalObject global) {
  DisallowGarbageCollection no_gc;
  InvalidateOnePrototypeValidityCellInternal(global.map());
}

Maybe<bool> JSObject::SetPrototype(Isolate* isolate, Handle<JSObject> object,
                                   Handle<Object> value, bool from_javascript,
                                   ShouldThrow should_throw) {
#ifdef DEBUG
  int size = object->Size();
#endif

  if (from_javascript) {
    if (object->IsAccessCheckNeeded() &&
        !isolate->MayAccess(handle(isolate->context(), isolate), object)) {
      isolate->ReportFailedAccessCheck(object);
      RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
      RETURN_FAILURE(isolate, should_throw,
                     NewTypeError(MessageTemplate::kNoAccess));
    }
  } else {
    DCHECK(!object->IsAccessCheckNeeded());
  }

  // Silently ignore the change if value is not a JSObject or null.
  // SpiderMonkey behaves this way.
  if (!value->IsJSReceiver() && !value->IsNull(isolate)) return Just(true);

  bool all_extensible = object->map().is_extensible();
  Handle<JSObject> real_receiver = object;
  if (from_javascript) {
    // Find the first object in the chain whose prototype object is not
    // hidden.
    PrototypeIterator iter(isolate, real_receiver, kStartAtPrototype,
                           PrototypeIterator::END_AT_NON_HIDDEN);
    while (!iter.IsAtEnd()) {
      // Casting to JSObject is fine because hidden prototypes are never
      // JSProxies.
      real_receiver = PrototypeIterator::GetCurrent<JSObject>(iter);
      iter.Advance();
      all_extensible = all_extensible && real_receiver->map().is_extensible();
    }
  }
  Handle<Map> map(real_receiver->map(), isolate);

  // Nothing to do if prototype is already set.
  if (map->prototype() == *value) return Just(true);

  bool immutable_proto = map->is_immutable_proto();
  if (immutable_proto) {
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kImmutablePrototypeSet, object));
  }

  // From 6.1.7.3 Invariants of the Essential Internal Methods
  //
  // [[SetPrototypeOf]] ( V )
  // * ...
  // * If target is non-extensible, [[SetPrototypeOf]] must return false,
  //   unless V is the SameValue as the target's observed [[GetPrototypeOf]]
  //   value.
  if (!all_extensible) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kNonExtensibleProto, object));
  }

  // Before we can set the prototype we need to be sure prototype cycles are
  // prevented.  It is sufficient to validate that the receiver is not in the
  // new prototype chain.
  if (value->IsJSReceiver()) {
    for (PrototypeIterator iter(isolate, JSReceiver::cast(*value),
                                kStartAtReceiver);
         !iter.IsAtEnd(); iter.Advance()) {
      if (iter.GetCurrent<JSReceiver>() == *object) {
        // Cycle detected.
        RETURN_FAILURE(isolate, should_throw,
                       NewTypeError(MessageTemplate::kCyclicProto));
      }
    }
  }

  // Set the new prototype of the object.

  isolate->UpdateNoElementsProtectorOnSetPrototype(real_receiver);

  Handle<Map> new_map =
      Map::TransitionToPrototype(isolate, map, Handle<HeapObject>::cast(value));
  DCHECK(new_map->prototype() == *value);
  JSObject::MigrateToMap(isolate, real_receiver, new_map);

  DCHECK(size == object->Size());
  return Just(true);
}

// static
void JSObject::SetImmutableProto(Handle<JSObject> object) {
  Handle<Map> map(object->map(), object->GetIsolate());

  // Nothing to do if prototype is already set.
  if (map->is_immutable_proto()) return;

  Handle<Map> new_map =
      Map::TransitionToImmutableProto(object->GetIsolate(), map);
  object->set_map(*new_map, kReleaseStore);
}

void JSObject::EnsureCanContainElements(Handle<JSObject> object,
                                        JavaScriptArguments* args,
                                        uint32_t arg_count,
                                        EnsureElementsMode mode) {
  return EnsureCanContainElements(object, args->first_slot(), arg_count, mode);
}

void JSObject::ValidateElements(JSObject object) {
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    object.GetElementsAccessor()->Validate(object);
  }
#endif
}

bool JSObject::WouldConvertToSlowElements(uint32_t index) {
  if (!HasFastElements()) return false;
  uint32_t capacity = static_cast<uint32_t>(elements().length());
  uint32_t new_capacity;
  return ShouldConvertToSlowElements(*this, capacity, index, &new_capacity);
}

static bool ShouldConvertToFastElements(JSObject object,
                                        NumberDictionary dictionary,
                                        uint32_t index,
                                        uint32_t* new_capacity) {
  // If properties with non-standard attributes or accessors were added, we
  // cannot go back to fast elements.
  if (dictionary.requires_slow_elements()) return false;

  // Adding a property with this index will require slow elements.
  if (index >= static_cast<uint32_t>(Smi::kMaxValue)) return false;

  if (object.IsJSArray()) {
    Object length = JSArray::cast(object).length();
    if (!length.IsSmi()) return false;
    *new_capacity = static_cast<uint32_t>(Smi::ToInt(length));
  } else if (object.IsJSArgumentsObject()) {
    return false;
  } else {
    *new_capacity = dictionary.max_number_key() + 1;
  }
  *new_capacity = std::max(index + 1, *new_capacity);

  uint32_t dictionary_size = static_cast<uint32_t>(dictionary.Capacity()) *
                             NumberDictionary::kEntrySize;

  // Turn fast if the dictionary only saves 50% space.
  return 2 * dictionary_size >= *new_capacity;
}

static ElementsKind BestFittingFastElementsKind(JSObject object) {
  if (!object.map().CanHaveFastTransitionableElementsKind()) {
    return HOLEY_ELEMENTS;
  }
  if (object.HasSloppyArgumentsElements()) {
    return FAST_SLOPPY_ARGUMENTS_ELEMENTS;
  }
  if (object.HasStringWrapperElements()) {
    return FAST_STRING_WRAPPER_ELEMENTS;
  }
  DCHECK(object.HasDictionaryElements());
  NumberDictionary dictionary = object.element_dictionary();
  ElementsKind kind = HOLEY_SMI_ELEMENTS;
  for (InternalIndex i : dictionary.IterateEntries()) {
    Object key = dictionary.KeyAt(i);
    if (key.IsNumber()) {
      Object value = dictionary.ValueAt(i);
      if (!value.IsNumber()) return HOLEY_ELEMENTS;
      if (!value.IsSmi()) {
        if (!FLAG_unbox_double_arrays) return HOLEY_ELEMENTS;
        kind = HOLEY_DOUBLE_ELEMENTS;
      }
    }
  }
  return kind;
}

// static
Maybe<bool> JSObject::AddDataElement(Handle<JSObject> object, uint32_t index,
                                     Handle<Object> value,
                                     PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();

  DCHECK(object->map(isolate).is_extensible());

  uint32_t old_length = 0;
  uint32_t new_capacity = 0;

  if (object->IsJSArray(isolate)) {
    CHECK(JSArray::cast(*object).length().ToArrayLength(&old_length));
  }

  ElementsKind kind = object->GetElementsKind(isolate);
  FixedArrayBase elements = object->elements(isolate);
  ElementsKind dictionary_kind = DICTIONARY_ELEMENTS;
  if (IsSloppyArgumentsElementsKind(kind)) {
    elements = SloppyArgumentsElements::cast(elements).arguments(isolate);
    dictionary_kind = SLOW_SLOPPY_ARGUMENTS_ELEMENTS;
  } else if (IsStringWrapperElementsKind(kind)) {
    dictionary_kind = SLOW_STRING_WRAPPER_ELEMENTS;
  }

  if (attributes != NONE) {
    kind = dictionary_kind;
  } else if (elements.IsNumberDictionary(isolate)) {
    kind = ShouldConvertToFastElements(
               *object, NumberDictionary::cast(elements), index, &new_capacity)
               ? BestFittingFastElementsKind(*object)
               : dictionary_kind;
  } else if (ShouldConvertToSlowElements(
                 *object, static_cast<uint32_t>(elements.length()), index,
                 &new_capacity)) {
    kind = dictionary_kind;
  }

  ElementsKind to = value->OptimalElementsKind(isolate);
  if (IsHoleyElementsKind(kind) || !object->IsJSArray(isolate) ||
      index > old_length) {
    to = GetHoleyElementsKind(to);
    kind = GetHoleyElementsKind(kind);
  }
  to = GetMoreGeneralElementsKind(kind, to);
  ElementsAccessor* accessor = ElementsAccessor::ForKind(to);
  MAYBE_RETURN(accessor->Add(object, index, value, attributes, new_capacity),
               Nothing<bool>());

  if (object->IsJSArray(isolate) && index >= old_length) {
    Handle<Object> new_length =
        isolate->factory()->NewNumberFromUint(index + 1);
    JSArray::cast(*object).set_length(*new_length);
  }
  return Just(true);
}

template <AllocationSiteUpdateMode update_or_check>
bool JSObject::UpdateAllocationSite(Handle<JSObject> object,
                                    ElementsKind to_kind) {
  if (!object->IsJSArray()) return false;

  if (!Heap::InYoungGeneration(*object)) return false;

  if (Heap::IsLargeObject(*object)) return false;

  Handle<AllocationSite> site;
  {
    DisallowGarbageCollection no_gc;

    Heap* heap = object->GetHeap();
    AllocationMemento memento =
        heap->FindAllocationMemento<Heap::kForRuntime>(object->map(), *object);
    if (memento.is_null()) return false;

    // Walk through to the Allocation Site
    site = handle(memento.GetAllocationSite(), heap->isolate());
  }
  return AllocationSite::DigestTransitionFeedback<update_or_check>(site,
                                                                   to_kind);
}

template bool
JSObject::UpdateAllocationSite<AllocationSiteUpdateMode::kCheckOnly>(
    Handle<JSObject> object, ElementsKind to_kind);

template bool JSObject::UpdateAllocationSite<AllocationSiteUpdateMode::kUpdate>(
    Handle<JSObject> object, ElementsKind to_kind);

void JSObject::TransitionElementsKind(Handle<JSObject> object,
                                      ElementsKind to_kind) {
  ElementsKind from_kind = object->GetElementsKind();

  if (IsHoleyElementsKind(from_kind)) {
    to_kind = GetHoleyElementsKind(to_kind);
  }

  if (from_kind == to_kind) return;

  // This method should never be called for any other case.
  DCHECK(IsFastElementsKind(from_kind) ||
         IsNonextensibleElementsKind(from_kind));
  DCHECK(IsFastElementsKind(to_kind) || IsNonextensibleElementsKind(to_kind));
  DCHECK_NE(TERMINAL_FAST_ELEMENTS_KIND, from_kind);

  UpdateAllocationSite(object, to_kind);
  Isolate* isolate = object->GetIsolate();
  if (object->elements() == ReadOnlyRoots(isolate).empty_fixed_array() ||
      IsDoubleElementsKind(from_kind) == IsDoubleElementsKind(to_kind)) {
    // No change is needed to the elements() buffer, the transition
    // only requires a map change.
    Handle<Map> new_map = GetElementsTransitionMap(object, to_kind);
    JSObject::MigrateToMap(isolate, object, new_map);
    if (FLAG_trace_elements_transitions) {
      Handle<FixedArrayBase> elms(object->elements(), isolate);
      PrintElementsTransition(stdout, object, from_kind, elms, to_kind, elms);
    }
  } else {
    DCHECK((IsSmiElementsKind(from_kind) && IsDoubleElementsKind(to_kind)) ||
           (IsDoubleElementsKind(from_kind) && IsObjectElementsKind(to_kind)));
    uint32_t c = static_cast<uint32_t>(object->elements().length());
    if (ElementsAccessor::ForKind(to_kind)
            ->GrowCapacityAndConvert(object, c)
            .IsNothing()) {
      // TODO(victorgomes): Temporarily forcing a fatal error here in case of
      // overflow, until all users of TransitionElementsKind can handle
      // exceptions.
      FATAL(
          "Fatal JavaScript invalid size error when transitioning elements "
          "kind");
      UNREACHABLE();
    }
  }
}

template <typename BackingStore>
static int HoleyElementsUsage(JSObject object, BackingStore store) {
  Isolate* isolate = object.GetIsolate();
  int limit = object.IsJSArray() ? Smi::ToInt(JSArray::cast(object).length())
                                 : store.length();
  int used = 0;
  for (int i = 0; i < limit; ++i) {
    if (!store.is_the_hole(isolate, i)) ++used;
  }
  return used;
}

int JSObject::GetFastElementsUsage() {
  FixedArrayBase store = elements();
  switch (GetElementsKind()) {
    case PACKED_SMI_ELEMENTS:
    case PACKED_DOUBLE_ELEMENTS:
    case PACKED_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case PACKED_NONEXTENSIBLE_ELEMENTS:
      return IsJSArray() ? Smi::ToInt(JSArray::cast(*this).length())
                         : store.length();
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
      store = SloppyArgumentsElements::cast(store).arguments();
      V8_FALLTHROUGH;
    case HOLEY_SMI_ELEMENTS:
    case HOLEY_ELEMENTS:
    case HOLEY_FROZEN_ELEMENTS:
    case HOLEY_SEALED_ELEMENTS:
    case HOLEY_NONEXTENSIBLE_ELEMENTS:
    case FAST_STRING_WRAPPER_ELEMENTS:
      return HoleyElementsUsage(*this, FixedArray::cast(store));
    case HOLEY_DOUBLE_ELEMENTS:
      if (elements().length() == 0) return 0;
      return HoleyElementsUsage(*this, FixedDoubleArray::cast(store));

    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
    case DICTIONARY_ELEMENTS:
    case WASM_ARRAY_ELEMENTS:
    case NO_ELEMENTS:
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
      RAB_GSAB_TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      UNREACHABLE();
  }
  return 0;
}

MaybeHandle<Object> JSObject::GetPropertyWithInterceptor(LookupIterator* it,
                                                         bool* done) {
  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  return GetPropertyWithInterceptorInternal(it, it->GetInterceptor(), done);
}

Maybe<bool> JSObject::HasRealNamedProperty(Isolate* isolate,
                                           Handle<JSObject> object,
                                           Handle<Name> name) {
  PropertyKey key(isolate, name);
  LookupIterator it(isolate, object, key, LookupIterator::OWN_SKIP_INTERCEPTOR);
  return HasProperty(&it);
}

Maybe<bool> JSObject::HasRealElementProperty(Isolate* isolate,
                                             Handle<JSObject> object,
                                             uint32_t index) {
  LookupIterator it(isolate, object, index, object,
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  return HasProperty(&it);
}

Maybe<bool> JSObject::HasRealNamedCallbackProperty(Isolate* isolate,
                                                   Handle<JSObject> object,
                                                   Handle<Name> name) {
  PropertyKey key(isolate, name);
  LookupIterator it(isolate, object, key, LookupIterator::OWN_SKIP_INTERCEPTOR);
  Maybe<PropertyAttributes> maybe_result = GetPropertyAttributes(&it);
  return maybe_result.IsJust() ? Just(it.state() == LookupIterator::ACCESSOR)
                               : Nothing<bool>();
}

bool JSGlobalProxy::IsDetached() const {
  return native_context().IsNull(GetIsolate());
}

void JSGlobalObject::InvalidatePropertyCell(Handle<JSGlobalObject> global,
                                            Handle<Name> name) {
  Isolate* isolate = global->GetIsolate();
  // Regardless of whether the property is there or not invalidate
  // Load/StoreGlobalICs that load/store through global object's prototype.
  JSObject::InvalidatePrototypeValidityCell(*global);
  DCHECK(!global->HasFastProperties());
  auto dictionary = handle(global->global_dictionary(kAcquireLoad), isolate);
  InternalIndex entry = dictionary->FindEntry(isolate, name);
  if (entry.is_not_found()) return;

  Handle<PropertyCell> cell(dictionary->CellAt(entry), isolate);
  Handle<Object> value(cell->value(), isolate);
  PropertyDetails details = cell->property_details();
  details = details.set_cell_type(PropertyCellType::kMutable);
  PropertyCell::InvalidateAndReplaceEntry(isolate, dictionary, entry, details,
                                          value);
}

// static
MaybeHandle<JSDate> JSDate::New(Handle<JSFunction> constructor,
                                Handle<JSReceiver> new_target, double tv) {
  Isolate* const isolate = constructor->GetIsolate();
  Handle<JSObject> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      JSObject::New(constructor, new_target, Handle<AllocationSite>::null()),
      JSDate);
  if (-DateCache::kMaxTimeInMs <= tv && tv <= DateCache::kMaxTimeInMs) {
    tv = DoubleToInteger(tv) + 0.0;
  } else {
    tv = std::numeric_limits<double>::quiet_NaN();
  }
  Handle<Object> value = isolate->factory()->NewNumber(tv);
  Handle<JSDate>::cast(result)->SetValue(*value, std::isnan(tv));
  return Handle<JSDate>::cast(result);
}

// static
double JSDate::CurrentTimeValue(Isolate* isolate) {
  if (FLAG_log_internal_timer_events) LOG(isolate, CurrentTimeEvent());
  if (FLAG_correctness_fuzzer_suppressions) return 4.2;

  // According to ECMA-262, section 15.9.1, page 117, the precision of
  // the number in a Date object representing a particular instant in
  // time is milliseconds. Therefore, we floor the result of getting
  // the OS time.
  return std::floor(V8::GetCurrentPlatform()->CurrentClockTimeMillis());
}

// static
Address JSDate::GetField(Isolate* isolate, Address raw_object,
                         Address smi_index) {
  // Called through CallCFunction.
  DisallowGarbageCollection no_gc;
  DisallowHandleAllocation no_handles;
  DisallowJavascriptExecution no_js(isolate);

  Object object(raw_object);
  Smi index(smi_index);
  return JSDate::cast(object)
      .DoGetField(isolate, static_cast<FieldIndex>(index.value()))
      .ptr();
}

Object JSDate::DoGetField(Isolate* isolate, FieldIndex index) {
  DCHECK_NE(index, kDateValue);

  DateCache* date_cache = isolate->date_cache();

  if (index < kFirstUncachedField) {
    Object stamp = cache_stamp();
    if (stamp != date_cache->stamp() && stamp.IsSmi()) {
      // Since the stamp is not NaN, the value is also not NaN.
      int64_t local_time_ms =
          date_cache->ToLocal(static_cast<int64_t>(value().Number()));
      SetCachedFields(local_time_ms, date_cache);
    }
    switch (index) {
      case kYear:
        return year();
      case kMonth:
        return month();
      case kDay:
        return day();
      case kWeekday:
        return weekday();
      case kHour:
        return hour();
      case kMinute:
        return min();
      case kSecond:
        return sec();
      default:
        UNREACHABLE();
    }
  }

  if (index >= kFirstUTCField) {
    return GetUTCField(index, value().Number(), date_cache);
  }

  double time = value().Number();
  if (std::isnan(time)) return GetReadOnlyRoots().nan_value();

  int64_t local_time_ms = date_cache->ToLocal(static_cast<int64_t>(time));
  int days = DateCache::DaysFromTime(local_time_ms);

  if (index == kDays) return Smi::FromInt(days);

  int time_in_day_ms = DateCache::TimeInDay(local_time_ms, days);
  if (index == kMillisecond) return Smi::FromInt(time_in_day_ms % 1000);
  DCHECK_EQ(index, kTimeInDay);
  return Smi::FromInt(time_in_day_ms);
}

Object JSDate::GetUTCField(FieldIndex index, double value,
                           DateCache* date_cache) {
  DCHECK_GE(index, kFirstUTCField);

  if (std::isnan(value)) return GetReadOnlyRoots().nan_value();

  int64_t time_ms = static_cast<int64_t>(value);

  if (index == kTimezoneOffset) {
    return Smi::FromInt(date_cache->TimezoneOffset(time_ms));
  }

  int days = DateCache::DaysFromTime(time_ms);

  if (index == kWeekdayUTC) return Smi::FromInt(date_cache->Weekday(days));

  if (index <= kDayUTC) {
    int year, month, day;
    date_cache->YearMonthDayFromDays(days, &year, &month, &day);
    if (index == kYearUTC) return Smi::FromInt(year);
    if (index == kMonthUTC) return Smi::FromInt(month);
    DCHECK_EQ(index, kDayUTC);
    return Smi::FromInt(day);
  }

  int time_in_day_ms = DateCache::TimeInDay(time_ms, days);
  switch (index) {
    case kHourUTC:
      return Smi::FromInt(time_in_day_ms / (60 * 60 * 1000));
    case kMinuteUTC:
      return Smi::FromInt((time_in_day_ms / (60 * 1000)) % 60);
    case kSecondUTC:
      return Smi::FromInt((time_in_day_ms / 1000) % 60);
    case kMillisecondUTC:
      return Smi::FromInt(time_in_day_ms % 1000);
    case kDaysUTC:
      return Smi::FromInt(days);
    case kTimeInDayUTC:
      return Smi::FromInt(time_in_day_ms);
    default:
      UNREACHABLE();
  }

  UNREACHABLE();
}

// static
Handle<Object> JSDate::SetValue(Handle<JSDate> date, double v) {
  Isolate* const isolate = date->GetIsolate();
  Handle<Object> value = isolate->factory()->NewNumber(v);
  bool value_is_nan = std::isnan(v);
  date->SetValue(*value, value_is_nan);
  return value;
}

void JSDate::SetValue(Object value, bool is_value_nan) {
  set_value(value);
  if (is_value_nan) {
    HeapNumber nan = GetReadOnlyRoots().nan_value();
    set_cache_stamp(nan, SKIP_WRITE_BARRIER);
    set_year(nan, SKIP_WRITE_BARRIER);
    set_month(nan, SKIP_WRITE_BARRIER);
    set_day(nan, SKIP_WRITE_BARRIER);
    set_hour(nan, SKIP_WRITE_BARRIER);
    set_min(nan, SKIP_WRITE_BARRIER);
    set_sec(nan, SKIP_WRITE_BARRIER);
    set_weekday(nan, SKIP_WRITE_BARRIER);
  } else {
    set_cache_stamp(Smi::FromInt(DateCache::kInvalidStamp), SKIP_WRITE_BARRIER);
  }
}

void JSDate::SetCachedFields(int64_t local_time_ms, DateCache* date_cache) {
  int days = DateCache::DaysFromTime(local_time_ms);
  int time_in_day_ms = DateCache::TimeInDay(local_time_ms, days);
  int year, month, day;
  date_cache->YearMonthDayFromDays(days, &year, &month, &day);
  int weekday = date_cache->Weekday(days);
  int hour = time_in_day_ms / (60 * 60 * 1000);
  int min = (time_in_day_ms / (60 * 1000)) % 60;
  int sec = (time_in_day_ms / 1000) % 60;
  set_cache_stamp(date_cache->stamp());
  set_year(Smi::FromInt(year), SKIP_WRITE_BARRIER);
  set_month(Smi::FromInt(month), SKIP_WRITE_BARRIER);
  set_day(Smi::FromInt(day), SKIP_WRITE_BARRIER);
  set_weekday(Smi::FromInt(weekday), SKIP_WRITE_BARRIER);
  set_hour(Smi::FromInt(hour), SKIP_WRITE_BARRIER);
  set_min(Smi::FromInt(min), SKIP_WRITE_BARRIER);
  set_sec(Smi::FromInt(sec), SKIP_WRITE_BARRIER);
}

// static
void JSMessageObject::EnsureSourcePositionsAvailable(
    Isolate* isolate, Handle<JSMessageObject> message) {
  if (!message->DidEnsureSourcePositionsAvailable()) {
    DCHECK_EQ(message->start_position(), -1);
    DCHECK_GE(message->bytecode_offset().value(), kFunctionEntryBytecodeOffset);
    Handle<SharedFunctionInfo> shared_info(
        SharedFunctionInfo::cast(message->shared_info()), isolate);
    IsCompiledScope is_compiled_scope;
    SharedFunctionInfo::EnsureBytecodeArrayAvailable(
        isolate, shared_info, &is_compiled_scope, CreateSourcePositions::kYes);
    SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate, shared_info);
    DCHECK(shared_info->HasBytecodeArray());
    int position = shared_info->abstract_code(isolate).SourcePosition(
        message->bytecode_offset().value());
    DCHECK_GE(position, 0);
    message->set_start_position(position);
    message->set_end_position(position + 1);
    message->set_shared_info(ReadOnlyRoots(isolate).undefined_value());
  }
}

int JSMessageObject::GetLineNumber() const {
  DCHECK(DidEnsureSourcePositionsAvailable());
  if (start_position() == -1) return Message::kNoLineNumberInfo;

  Handle<Script> the_script(script(), GetIsolate());

  Script::PositionInfo info;
  const Script::OffsetFlag offset_flag = Script::WITH_OFFSET;
  if (!Script::GetPositionInfo(the_script, start_position(), &info,
                               offset_flag)) {
    return Message::kNoLineNumberInfo;
  }

  return info.line + 1;
}

int JSMessageObject::GetColumnNumber() const {
  DCHECK(DidEnsureSourcePositionsAvailable());
  if (start_position() == -1) return -1;

  Handle<Script> the_script(script(), GetIsolate());

  Script::PositionInfo info;
  const Script::OffsetFlag offset_flag = Script::WITH_OFFSET;
  if (!Script::GetPositionInfo(the_script, start_position(), &info,
                               offset_flag)) {
    return -1;
  }

  return info.column;  // Note: No '+1' in contrast to GetLineNumber.
}

String JSMessageObject::GetSource() const {
  Script script_object = script();
  if (script_object.HasValidSource()) {
    Object source = script_object.source();
    if (source.IsString()) return String::cast(source);
  }
  return ReadOnlyRoots(GetIsolate()).empty_string();
}

Handle<String> JSMessageObject::GetSourceLine() const {
  Isolate* isolate = GetIsolate();
  Handle<Script> the_script(script(), isolate);

#if V8_ENABLE_WEBASSEMBLY
  if (the_script->type() == Script::TYPE_WASM) {
    return isolate->factory()->empty_string();
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  Script::PositionInfo info;
  const Script::OffsetFlag offset_flag = Script::WITH_OFFSET;
  DCHECK(DidEnsureSourcePositionsAvailable());
  if (!Script::GetPositionInfo(the_script, start_position(), &info,
                               offset_flag)) {
    return isolate->factory()->empty_string();
  }

  Handle<String> src = handle(String::cast(the_script->source()), isolate);
  return isolate->factory()->NewSubString(src, info.line_start, info.line_end);
}

}  // namespace internal
}  // namespace v8
