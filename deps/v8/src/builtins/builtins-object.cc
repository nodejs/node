// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/common/message-template.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/logging/counters.h"
#include "src/objects/keys.h"
#include "src/objects/lookup.h"
#include "src/objects/objects-inl.h"
#include "src/objects/property-descriptor.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 19.1 Object Objects

// ES6 section 19.1.3.4 Object.prototype.propertyIsEnumerable ( V )
BUILTIN(ObjectPrototypePropertyIsEnumerable) {
  HandleScope scope(isolate);
  Handle<JSReceiver> object;
  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, name, Object::ToName(isolate, args.atOrUndefined(isolate, 1)));
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, object, Object::ToObject(isolate, args.receiver()));
  Maybe<PropertyAttributes> maybe =
      JSReceiver::GetOwnPropertyAttributes(object, name);
  if (maybe.IsNothing()) return ReadOnlyRoots(isolate).exception();
  if (maybe.FromJust() == ABSENT) return ReadOnlyRoots(isolate).false_value();
  return isolate->heap()->ToBoolean((maybe.FromJust() & DONT_ENUM) == 0);
}

// ES6 section 19.1.2.3 Object.defineProperties
BUILTIN(ObjectDefineProperties) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<Object> target = args.at(1);
  Handle<Object> properties = args.at(2);

  RETURN_RESULT_OR_FAILURE(
      isolate, JSReceiver::DefineProperties(isolate, target, properties));
}

// ES6 section 19.1.2.4 Object.defineProperty
BUILTIN(ObjectDefineProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  Handle<Object> target = args.at(1);
  Handle<Object> key = args.at(2);
  Handle<Object> attributes = args.at(3);

  return JSReceiver::DefineProperty(isolate, target, key, attributes);
}

namespace {

template <AccessorComponent which_accessor>
Object ObjectDefineAccessor(Isolate* isolate, Handle<Object> object,
                            Handle<Object> name, Handle<Object> accessor) {
  // 1. Let O be ? ToObject(this value).
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));
  // 2. If IsCallable(getter) is false, throw a TypeError exception.
  if (!accessor->IsCallable()) {
    MessageTemplate message =
        which_accessor == ACCESSOR_GETTER
            ? MessageTemplate::kObjectGetterExpectingFunction
            : MessageTemplate::kObjectSetterExpectingFunction;
    THROW_NEW_ERROR_RETURN_FAILURE(isolate, NewTypeError(message));
  }
  // 3. Let desc be PropertyDescriptor{[[Get]]: getter, [[Enumerable]]: true,
  //                                   [[Configurable]]: true}.
  PropertyDescriptor desc;
  if (which_accessor == ACCESSOR_GETTER) {
    desc.set_get(accessor);
  } else {
    DCHECK(which_accessor == ACCESSOR_SETTER);
    desc.set_set(accessor);
  }
  desc.set_enumerable(true);
  desc.set_configurable(true);
  // 4. Let key be ? ToPropertyKey(P).
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToPropertyKey(isolate, name));
  // 5. Perform ? DefinePropertyOrThrow(O, key, desc).
  // To preserve legacy behavior, we ignore errors silently rather than
  // throwing an exception.
  Maybe<bool> success = JSReceiver::DefineOwnProperty(
      isolate, receiver, name, &desc, Just(kThrowOnError));
  MAYBE_RETURN(success, ReadOnlyRoots(isolate).exception());
  if (!success.FromJust()) {
    isolate->CountUsage(v8::Isolate::kDefineGetterOrSetterWouldThrow);
  }
  // 6. Return undefined.
  return ReadOnlyRoots(isolate).undefined_value();
}

Object ObjectLookupAccessor(Isolate* isolate, Handle<Object> object,
                            Handle<Object> key, AccessorComponent component) {
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, object,
                                     Object::ToObject(isolate, object));
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, key,
                                     Object::ToPropertyKey(isolate, key));
  bool success = false;
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, key, &success,
      LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  DCHECK(success);

  for (; it.IsFound(); it.Next()) {
    switch (it.state()) {
      case LookupIterator::INTERCEPTOR:
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();

      case LookupIterator::ACCESS_CHECK:
        if (it.HasAccess()) continue;
        isolate->ReportFailedAccessCheck(it.GetHolder<JSObject>());
        RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
        return ReadOnlyRoots(isolate).undefined_value();

      case LookupIterator::JSPROXY: {
        PropertyDescriptor desc;
        Maybe<bool> found = JSProxy::GetOwnPropertyDescriptor(
            isolate, it.GetHolder<JSProxy>(), it.GetName(), &desc);
        MAYBE_RETURN(found, ReadOnlyRoots(isolate).exception());
        if (found.FromJust()) {
          if (component == ACCESSOR_GETTER && desc.has_get()) {
            return *desc.get();
          }
          if (component == ACCESSOR_SETTER && desc.has_set()) {
            return *desc.set();
          }
          return ReadOnlyRoots(isolate).undefined_value();
        }
        Handle<Object> prototype;
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
            isolate, prototype, JSProxy::GetPrototype(it.GetHolder<JSProxy>()));
        if (prototype->IsNull(isolate)) {
          return ReadOnlyRoots(isolate).undefined_value();
        }
        return ObjectLookupAccessor(isolate, prototype, key, component);
      }

      case LookupIterator::INTEGER_INDEXED_EXOTIC:
      case LookupIterator::DATA:
        return ReadOnlyRoots(isolate).undefined_value();

      case LookupIterator::ACCESSOR: {
        Handle<Object> maybe_pair = it.GetAccessors();
        if (maybe_pair->IsAccessorPair()) {
          return *AccessorPair::GetComponent(
              isolate, Handle<AccessorPair>::cast(maybe_pair), component);
        }
      }
    }
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

}  // namespace

// ES6 B.2.2.2 a.k.a.
// https://tc39.github.io/ecma262/#sec-object.prototype.__defineGetter__
BUILTIN(ObjectDefineGetter) {
  HandleScope scope(isolate);
  Handle<Object> object = args.at(0);  // Receiver.
  Handle<Object> name = args.at(1);
  Handle<Object> getter = args.at(2);
  return ObjectDefineAccessor<ACCESSOR_GETTER>(isolate, object, name, getter);
}

// ES6 B.2.2.3 a.k.a.
// https://tc39.github.io/ecma262/#sec-object.prototype.__defineSetter__
BUILTIN(ObjectDefineSetter) {
  HandleScope scope(isolate);
  Handle<Object> object = args.at(0);  // Receiver.
  Handle<Object> name = args.at(1);
  Handle<Object> setter = args.at(2);
  return ObjectDefineAccessor<ACCESSOR_SETTER>(isolate, object, name, setter);
}

// ES6 B.2.2.4 a.k.a.
// https://tc39.github.io/ecma262/#sec-object.prototype.__lookupGetter__
BUILTIN(ObjectLookupGetter) {
  HandleScope scope(isolate);
  Handle<Object> object = args.at(0);
  Handle<Object> name = args.at(1);
  return ObjectLookupAccessor(isolate, object, name, ACCESSOR_GETTER);
}

// ES6 B.2.2.5 a.k.a.
// https://tc39.github.io/ecma262/#sec-object.prototype.__lookupSetter__
BUILTIN(ObjectLookupSetter) {
  HandleScope scope(isolate);
  Handle<Object> object = args.at(0);
  Handle<Object> name = args.at(1);
  return ObjectLookupAccessor(isolate, object, name, ACCESSOR_SETTER);
}

// ES6 section 19.1.2.5 Object.freeze ( O )
BUILTIN(ObjectFreeze) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  if (object->IsJSReceiver()) {
    MAYBE_RETURN(JSReceiver::SetIntegrityLevel(Handle<JSReceiver>::cast(object),
                                               FROZEN, kThrowOnError),
                 ReadOnlyRoots(isolate).exception());
  }
  return *object;
}

// ES6 section B.2.2.1.1 get Object.prototype.__proto__
BUILTIN(ObjectPrototypeGetProto) {
  HandleScope scope(isolate);
  // 1. Let O be ? ToObject(this value).
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, receiver, Object::ToObject(isolate, args.receiver()));

  // 2. Return ? O.[[GetPrototypeOf]]().
  RETURN_RESULT_OR_FAILURE(isolate,
                           JSReceiver::GetPrototype(isolate, receiver));
}

// ES6 section B.2.2.1.2 set Object.prototype.__proto__
BUILTIN(ObjectPrototypeSetProto) {
  HandleScope scope(isolate);
  // 1. Let O be ? RequireObjectCoercible(this value).
  Handle<Object> object = args.receiver();
  if (object->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "set Object.prototype.__proto__")));
  }

  // 2. If Type(proto) is neither Object nor Null, return undefined.
  Handle<Object> proto = args.at(1);
  if (!proto->IsNull(isolate) && !proto->IsJSReceiver()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // 3. If Type(O) is not Object, return undefined.
  if (!object->IsJSReceiver()) return ReadOnlyRoots(isolate).undefined_value();
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(object);

  // 4. Let status be ? O.[[SetPrototypeOf]](proto).
  // 5. If status is false, throw a TypeError exception.
  MAYBE_RETURN(JSReceiver::SetPrototype(receiver, proto, true, kThrowOnError),
               ReadOnlyRoots(isolate).exception());

  // Return undefined.
  return ReadOnlyRoots(isolate).undefined_value();
}

namespace {

Object GetOwnPropertyKeys(Isolate* isolate, BuiltinArguments args,
                          PropertyFilter filter) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));
  Handle<FixedArray> keys;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, keys,
      KeyAccumulator::GetKeys(receiver, KeyCollectionMode::kOwnOnly, filter,
                              GetKeysConversion::kConvertToString));
  return *isolate->factory()->NewJSArrayWithElements(keys);
}

}  // namespace

// ES6 section 19.1.2.8 Object.getOwnPropertySymbols ( O )
BUILTIN(ObjectGetOwnPropertySymbols) {
  return GetOwnPropertyKeys(isolate, args, SKIP_STRINGS);
}

// ES6 section 19.1.2.12 Object.isFrozen ( O )
BUILTIN(ObjectIsFrozen) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Maybe<bool> result = object->IsJSReceiver()
                           ? JSReceiver::TestIntegrityLevel(
                                 Handle<JSReceiver>::cast(object), FROZEN)
                           : Just(true);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}

// ES6 section 19.1.2.13 Object.isSealed ( O )
BUILTIN(ObjectIsSealed) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Maybe<bool> result = object->IsJSReceiver()
                           ? JSReceiver::TestIntegrityLevel(
                                 Handle<JSReceiver>::cast(object), SEALED)
                           : Just(true);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}

BUILTIN(ObjectGetOwnPropertyDescriptors) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);

  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));

  Handle<FixedArray> keys;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, keys, KeyAccumulator::GetKeys(
                         receiver, KeyCollectionMode::kOwnOnly, ALL_PROPERTIES,
                         GetKeysConversion::kConvertToString));

  Handle<JSObject> descriptors =
      isolate->factory()->NewJSObject(isolate->object_function());

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Name> key = Handle<Name>::cast(FixedArray::get(*keys, i, isolate));
    PropertyDescriptor descriptor;
    Maybe<bool> did_get_descriptor = JSReceiver::GetOwnPropertyDescriptor(
        isolate, receiver, key, &descriptor);
    MAYBE_RETURN(did_get_descriptor, ReadOnlyRoots(isolate).exception());

    if (!did_get_descriptor.FromJust()) continue;
    Handle<Object> from_descriptor = descriptor.ToObject(isolate);

    Maybe<bool> success = JSReceiver::CreateDataProperty(
        isolate, descriptors, key, from_descriptor, Just(kDontThrow));
    CHECK(success.FromJust());
  }

  return *descriptors;
}

// ES6 section 19.1.2.17 Object.seal ( O )
BUILTIN(ObjectSeal) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  if (object->IsJSReceiver()) {
    MAYBE_RETURN(JSReceiver::SetIntegrityLevel(Handle<JSReceiver>::cast(object),
                                               SEALED, kThrowOnError),
                 ReadOnlyRoots(isolate).exception());
  }
  return *object;
}

}  // namespace internal
}  // namespace v8
