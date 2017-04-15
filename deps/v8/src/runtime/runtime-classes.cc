// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include <stdlib.h>
#include <limits>

#include "src/accessors.h"
#include "src/arguments.h"
#include "src/debug/debug.h"
#include "src/elements.h"
#include "src/frames-inl.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {


RUNTIME_FUNCTION(Runtime_ThrowUnsupportedSuperError) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewReferenceError(MessageTemplate::kUnsupportedSuper));
}


RUNTIME_FUNCTION(Runtime_ThrowConstructorNonCallableError) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, 0);
  Handle<Object> name(constructor->shared()->name(), isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kConstructorNonCallable, name));
}


RUNTIME_FUNCTION(Runtime_ThrowStaticPrototypeError) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kStaticPrototype));
}

RUNTIME_FUNCTION(Runtime_ThrowSuperAlreadyCalledError) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewReferenceError(MessageTemplate::kSuperAlreadyCalled));
}

namespace {

Object* ThrowNotSuperConstructor(Isolate* isolate, Handle<Object> constructor,
                                 Handle<JSFunction> function) {
  Handle<Object> super_name;
  if (constructor->IsJSFunction()) {
    super_name = handle(Handle<JSFunction>::cast(constructor)->shared()->name(),
                        isolate);
  } else if (constructor->IsOddball()) {
    DCHECK(constructor->IsNull(isolate));
    super_name = isolate->factory()->null_string();
  } else {
    super_name = Object::NoSideEffectsToString(isolate, constructor);
  }
  // null constructor
  if (Handle<String>::cast(super_name)->length() == 0) {
    super_name = isolate->factory()->null_string();
  }
  Handle<Object> function_name(function->shared()->name(), isolate);
  // anonymous class
  if (Handle<String>::cast(function_name)->length() == 0) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kNotSuperConstructorAnonymousClass,
                     super_name));
  }
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kNotSuperConstructor, super_name,
                            function_name));
}

}  // namespace

RUNTIME_FUNCTION(Runtime_ThrowNotSuperConstructor) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, constructor, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 1);
  return ThrowNotSuperConstructor(isolate, constructor, function);
}

RUNTIME_FUNCTION(Runtime_HomeObjectSymbol) {
  DCHECK_EQ(0, args.length());
  return isolate->heap()->home_object_symbol();
}

static MaybeHandle<Object> DefineClass(Isolate* isolate,
                                       Handle<Object> super_class,
                                       Handle<JSFunction> constructor,
                                       int start_position, int end_position) {
  Handle<Object> prototype_parent;
  Handle<Object> constructor_parent;

  if (super_class->IsTheHole(isolate)) {
    prototype_parent = isolate->initial_object_prototype();
  } else {
    if (super_class->IsNull(isolate)) {
      prototype_parent = isolate->factory()->null_value();
    } else if (super_class->IsConstructor()) {
      DCHECK(!super_class->IsJSFunction() ||
             !IsResumableFunction(
                 Handle<JSFunction>::cast(super_class)->shared()->kind()));
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, prototype_parent,
          Runtime::GetObjectProperty(isolate, super_class,
                                     isolate->factory()->prototype_string()),
          Object);
      if (!prototype_parent->IsNull(isolate) &&
          !prototype_parent->IsJSReceiver()) {
        THROW_NEW_ERROR(
            isolate, NewTypeError(MessageTemplate::kPrototypeParentNotAnObject,
                                  prototype_parent),
            Object);
      }
      constructor_parent = super_class;
    } else {
      THROW_NEW_ERROR(isolate,
                      NewTypeError(MessageTemplate::kExtendsValueNotConstructor,
                                   super_class),
                      Object);
    }
  }

  Handle<Map> map =
      isolate->factory()->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  map->set_is_prototype_map(true);
  Map::SetPrototype(map, prototype_parent);
  map->SetConstructor(*constructor);
  Handle<JSObject> prototype = isolate->factory()->NewJSObjectFromMap(map);

  if (!super_class->IsTheHole(isolate)) {
    // Derived classes, just like builtins, don't create implicit receivers in
    // [[construct]]. Instead they just set up new.target and call into the
    // constructor. Hence we can reuse the builtins construct stub for derived
    // classes.
    Handle<Code> stub(isolate->builtins()->JSBuiltinsConstructStubForDerived());
    constructor->shared()->SetConstructStub(*stub);
  }

  JSFunction::SetPrototype(constructor, prototype);
  PropertyAttributes attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  RETURN_ON_EXCEPTION(isolate,
                      JSObject::SetOwnPropertyIgnoreAttributes(
                          constructor, isolate->factory()->prototype_string(),
                          prototype, attribs),
                      Object);

  if (!constructor_parent.is_null()) {
    MAYBE_RETURN_NULL(JSObject::SetPrototype(constructor, constructor_parent,
                                             false, Object::THROW_ON_ERROR));
  }

  JSObject::AddProperty(prototype, isolate->factory()->constructor_string(),
                        constructor, DONT_ENUM);

  // Install private properties that are used to construct the FunctionToString.
  RETURN_ON_EXCEPTION(
      isolate,
      Object::SetProperty(
          constructor, isolate->factory()->class_start_position_symbol(),
          handle(Smi::FromInt(start_position), isolate), STRICT),
      Object);
  RETURN_ON_EXCEPTION(
      isolate, Object::SetProperty(
                   constructor, isolate->factory()->class_end_position_symbol(),
                   handle(Smi::FromInt(end_position), isolate), STRICT),
      Object);

  // Caller already has access to constructor, so return the prototype.
  return prototype;
}


RUNTIME_FUNCTION(Runtime_DefineClass) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, super_class, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, 1);
  CONVERT_SMI_ARG_CHECKED(start_position, 2);
  CONVERT_SMI_ARG_CHECKED(end_position, 3);

  RETURN_RESULT_OR_FAILURE(
      isolate, DefineClass(isolate, super_class, constructor, start_position,
                           end_position));
}

namespace {
void InstallClassNameAccessor(Isolate* isolate, Handle<JSObject> object) {
  PropertyAttributes attrs =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
  // Cannot fail since this should only be called when creating an object
  // literal.
  CHECK(!JSObject::SetAccessor(
             object, Accessors::FunctionNameInfo(object->GetIsolate(), attrs))
             .is_null());
}
}  // anonymous namespace

RUNTIME_FUNCTION(Runtime_InstallClassNameAccessor) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  InstallClassNameAccessor(isolate, object);
  return *object;
}

RUNTIME_FUNCTION(Runtime_InstallClassNameAccessorWithCheck) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);

  // If a property named "name" is already defined, exit.
  Handle<Name> key = isolate->factory()->name_string();
  if (JSObject::HasRealNamedProperty(object, key).FromMaybe(false)) {
    return *object;
  }

  // Define the "name" accessor.
  InstallClassNameAccessor(isolate, object);
  return *object;
}

namespace {

enum class SuperMode { kLoad, kStore };

MaybeHandle<JSReceiver> GetSuperHolder(
    Isolate* isolate, Handle<Object> receiver, Handle<JSObject> home_object,
    SuperMode mode, MaybeHandle<Name> maybe_name, uint32_t index) {
  if (home_object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context()), home_object)) {
    isolate->ReportFailedAccessCheck(home_object);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, JSReceiver);
  }

  PrototypeIterator iter(isolate, home_object);
  Handle<Object> proto = PrototypeIterator::GetCurrent(iter);
  if (!proto->IsJSReceiver()) {
    MessageTemplate::Template message =
        mode == SuperMode::kLoad ? MessageTemplate::kNonObjectPropertyLoad
                                 : MessageTemplate::kNonObjectPropertyStore;
    Handle<Name> name;
    if (!maybe_name.ToHandle(&name)) {
      name = isolate->factory()->Uint32ToString(index);
    }
    THROW_NEW_ERROR(isolate, NewTypeError(message, name, proto), JSReceiver);
  }
  return Handle<JSReceiver>::cast(proto);
}

MaybeHandle<Object> LoadFromSuper(Isolate* isolate, Handle<Object> receiver,
                                  Handle<JSObject> home_object,
                                  Handle<Name> name) {
  Handle<JSReceiver> holder;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, holder,
      GetSuperHolder(isolate, receiver, home_object, SuperMode::kLoad, name, 0),
      Object);
  LookupIterator it(receiver, name, holder);
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result, Object::GetProperty(&it), Object);
  return result;
}

MaybeHandle<Object> LoadElementFromSuper(Isolate* isolate,
                                         Handle<Object> receiver,
                                         Handle<JSObject> home_object,
                                         uint32_t index) {
  Handle<JSReceiver> holder;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, holder,
      GetSuperHolder(isolate, receiver, home_object, SuperMode::kLoad,
                     MaybeHandle<Name>(), index),
      Object);
  LookupIterator it(isolate, receiver, index, holder);
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result, Object::GetProperty(&it), Object);
  return result;
}

}  // anonymous namespace

RUNTIME_FUNCTION(Runtime_LoadFromSuper) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 2);

  RETURN_RESULT_OR_FAILURE(isolate,
                           LoadFromSuper(isolate, receiver, home_object, name));
}


RUNTIME_FUNCTION(Runtime_LoadKeyedFromSuper) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 2);

  uint32_t index = 0;

  if (key->ToArrayIndex(&index)) {
    RETURN_RESULT_OR_FAILURE(
        isolate, LoadElementFromSuper(isolate, receiver, home_object, index));
  }

  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));
  // TODO(verwaest): Unify using LookupIterator.
  if (name->AsArrayIndex(&index)) {
    RETURN_RESULT_OR_FAILURE(
        isolate, LoadElementFromSuper(isolate, receiver, home_object, index));
  }
  RETURN_RESULT_OR_FAILURE(isolate,
                           LoadFromSuper(isolate, receiver, home_object, name));
}

namespace {

MaybeHandle<Object> StoreToSuper(Isolate* isolate, Handle<JSObject> home_object,
                                 Handle<Object> receiver, Handle<Name> name,
                                 Handle<Object> value,
                                 LanguageMode language_mode) {
  Handle<JSReceiver> holder;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, holder,
                             GetSuperHolder(isolate, receiver, home_object,
                                            SuperMode::kStore, name, 0),
                             Object);
  LookupIterator it(receiver, name, holder);
  MAYBE_RETURN(Object::SetSuperProperty(&it, value, language_mode,
                                        Object::CERTAINLY_NOT_STORE_FROM_KEYED),
               MaybeHandle<Object>());
  return value;
}

MaybeHandle<Object> StoreElementToSuper(Isolate* isolate,
                                        Handle<JSObject> home_object,
                                        Handle<Object> receiver, uint32_t index,
                                        Handle<Object> value,
                                        LanguageMode language_mode) {
  Handle<JSReceiver> holder;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, holder,
      GetSuperHolder(isolate, receiver, home_object, SuperMode::kStore,
                     MaybeHandle<Name>(), index),
      Object);
  LookupIterator it(isolate, receiver, index, holder);
  MAYBE_RETURN(Object::SetSuperProperty(&it, value, language_mode,
                                        Object::MAY_BE_STORE_FROM_KEYED),
               MaybeHandle<Object>());
  return value;
}

}  // anonymous namespace

RUNTIME_FUNCTION(Runtime_StoreToSuper_Strict) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 3);

  RETURN_RESULT_OR_FAILURE(isolate, StoreToSuper(isolate, home_object, receiver,
                                                 name, value, STRICT));
}


RUNTIME_FUNCTION(Runtime_StoreToSuper_Sloppy) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 3);

  RETURN_RESULT_OR_FAILURE(isolate, StoreToSuper(isolate, home_object, receiver,
                                                 name, value, SLOPPY));
}

static MaybeHandle<Object> StoreKeyedToSuper(
    Isolate* isolate, Handle<JSObject> home_object, Handle<Object> receiver,
    Handle<Object> key, Handle<Object> value, LanguageMode language_mode) {
  uint32_t index = 0;

  if (key->ToArrayIndex(&index)) {
    return StoreElementToSuper(isolate, home_object, receiver, index, value,
                               language_mode);
  }
  Handle<Name> name;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, name, Object::ToName(isolate, key),
                             Object);
  // TODO(verwaest): Unify using LookupIterator.
  if (name->AsArrayIndex(&index)) {
    return StoreElementToSuper(isolate, home_object, receiver, index, value,
                               language_mode);
  }
  return StoreToSuper(isolate, home_object, receiver, name, value,
                      language_mode);
}


RUNTIME_FUNCTION(Runtime_StoreKeyedToSuper_Strict) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 3);

  RETURN_RESULT_OR_FAILURE(
      isolate,
      StoreKeyedToSuper(isolate, home_object, receiver, key, value, STRICT));
}


RUNTIME_FUNCTION(Runtime_StoreKeyedToSuper_Sloppy) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 3);

  RETURN_RESULT_OR_FAILURE(
      isolate,
      StoreKeyedToSuper(isolate, home_object, receiver, key, value, SLOPPY));
}


RUNTIME_FUNCTION(Runtime_GetSuperConstructor) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSFunction, active_function, 0);
  Object* prototype = active_function->map()->prototype();
  if (!prototype->IsConstructor()) {
    HandleScope scope(isolate);
    return ThrowNotSuperConstructor(isolate, handle(prototype, isolate),
                                    handle(active_function, isolate));
  }
  return prototype;
}

RUNTIME_FUNCTION(Runtime_NewWithSpread) {
  HandleScope scope(isolate);
  DCHECK_LE(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, constructor, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, new_target, 1);

  int constructor_argc = args.length() - 2;
  CONVERT_ARG_HANDLE_CHECKED(Object, spread, args.length() - 1);

  // Iterate over the spread if we need to.
  if (spread->IterationHasObservableEffects()) {
    Handle<JSFunction> spread_iterable_function = isolate->spread_iterable();
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, spread,
        Execution::Call(isolate, spread_iterable_function,
                        isolate->factory()->undefined_value(), 1, &spread));
  }

  uint32_t spread_length;
  Handle<JSArray> spread_array = Handle<JSArray>::cast(spread);
  CHECK(spread_array->length()->ToArrayIndex(&spread_length));
  int result_length = constructor_argc - 1 + spread_length;
  ScopedVector<Handle<Object>> construct_args(result_length);

  // Append each of the individual args to the result.
  for (int i = 0; i < constructor_argc - 1; i++) {
    construct_args[i] = args.at<Object>(2 + i);
  }

  // Append element of the spread to the result.
  ElementsAccessor* accessor = spread_array->GetElementsAccessor();
  for (uint32_t i = 0; i < spread_length; i++) {
    DCHECK(accessor->HasElement(spread_array, i));
    Handle<Object> element = accessor->Get(spread_array, i);
    construct_args[constructor_argc - 1 + i] = element;
  }

  // Call the constructor.
  RETURN_RESULT_OR_FAILURE(
      isolate, Execution::New(isolate, constructor, new_target, result_length,
                              construct_args.start()));
}

}  // namespace internal
}  // namespace v8
