// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include <stdlib.h>
#include <limits>

#include "src/arguments.h"
#include "src/debug/debug.h"
#include "src/frames-inl.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {


RUNTIME_FUNCTION(Runtime_ThrowNonMethodError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewReferenceError(MessageTemplate::kNonMethod));
}


RUNTIME_FUNCTION(Runtime_ThrowUnsupportedSuperError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewReferenceError(MessageTemplate::kUnsupportedSuper));
}


RUNTIME_FUNCTION(Runtime_ThrowConstructorNonCallableError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, 0);
  Handle<Object> name(constructor->shared()->name(), isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kConstructorNonCallable, name));
}


RUNTIME_FUNCTION(Runtime_ThrowArrayNotSubclassableError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kArrayNotSubclassable));
}


static Object* ThrowStaticPrototypeError(Isolate* isolate) {
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kStaticPrototype));
}


RUNTIME_FUNCTION(Runtime_ThrowStaticPrototypeError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  return ThrowStaticPrototypeError(isolate);
}


RUNTIME_FUNCTION(Runtime_ThrowIfStaticPrototype) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 0);
  if (Name::Equals(name, isolate->factory()->prototype_string())) {
    return ThrowStaticPrototypeError(isolate);
  }
  return *name;
}


RUNTIME_FUNCTION(Runtime_HomeObjectSymbol) {
  DCHECK(args.length() == 0);
  return isolate->heap()->home_object_symbol();
}


static MaybeHandle<Object> DefineClass(Isolate* isolate, Handle<Object> name,
                                       Handle<Object> super_class,
                                       Handle<JSFunction> constructor,
                                       int start_position, int end_position) {
  Handle<Object> prototype_parent;
  Handle<Object> constructor_parent;

  if (super_class->IsTheHole()) {
    prototype_parent = isolate->initial_object_prototype();
  } else {
    if (super_class->IsNull()) {
      prototype_parent = isolate->factory()->null_value();
    } else if (super_class->IsConstructor()) {
      if (super_class->IsJSFunction() &&
          Handle<JSFunction>::cast(super_class)->shared()->is_generator()) {
        THROW_NEW_ERROR(
            isolate,
            NewTypeError(MessageTemplate::kExtendsValueGenerator, super_class),
            Object);
      }
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, prototype_parent,
          Runtime::GetObjectProperty(isolate, super_class,
                                     isolate->factory()->prototype_string(),
                                     SLOPPY),
          Object);
      if (!prototype_parent->IsNull() && !prototype_parent->IsJSReceiver()) {
        THROW_NEW_ERROR(
            isolate, NewTypeError(MessageTemplate::kPrototypeParentNotAnObject,
                                  prototype_parent),
            Object);
      }
      constructor_parent = super_class;
    } else {
      THROW_NEW_ERROR(
          isolate,
          NewTypeError(MessageTemplate::kExtendsValueNotFunction, super_class),
          Object);
    }
  }

  Handle<Map> map =
      isolate->factory()->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  map->set_is_prototype_map(true);
  if (constructor->map()->is_strong()) {
    map->set_is_strong();
    if (super_class->IsNull()) {
      // Strong class is not permitted to extend null.
      THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kStrongExtendNull),
                      Object);
    }
  }
  Map::SetPrototype(map, prototype_parent);
  map->SetConstructor(*constructor);
  Handle<JSObject> prototype = isolate->factory()->NewJSObjectFromMap(map);

  Handle<String> name_string = name->IsString()
                                   ? Handle<String>::cast(name)
                                   : isolate->factory()->empty_string();
  constructor->shared()->set_name(*name_string);

  if (!super_class->IsTheHole()) {
    // Derived classes, just like builtins, don't create implicit receivers in
    // [[construct]]. Instead they just set up new.target and call into the
    // constructor. Hence we can reuse the builtins construct stub for derived
    // classes.
    Handle<Code> stub(isolate->builtins()->JSBuiltinsConstructStub());
    constructor->shared()->set_construct_stub(*stub);
  }

  JSFunction::SetPrototype(constructor, prototype);
  PropertyAttributes attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  RETURN_ON_EXCEPTION(isolate,
                      JSObject::SetOwnPropertyIgnoreAttributes(
                          constructor, isolate->factory()->prototype_string(),
                          prototype, attribs),
                      Object);

  // TODO(arv): Only do this conditionally.
  Handle<Symbol> home_object_symbol(isolate->heap()->home_object_symbol());
  RETURN_ON_EXCEPTION(
      isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                   constructor, home_object_symbol, prototype, DONT_ENUM),
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

  return constructor;
}


RUNTIME_FUNCTION(Runtime_DefineClass) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, super_class, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, 2);
  CONVERT_SMI_ARG_CHECKED(start_position, 3);
  CONVERT_SMI_ARG_CHECKED(end_position, 4);

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, DefineClass(isolate, name, super_class, constructor,
                                   start_position, end_position));
  return *result;
}


RUNTIME_FUNCTION(Runtime_DefineClassMethod) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 2);

  RETURN_FAILURE_ON_EXCEPTION(isolate,
                              JSObject::DefinePropertyOrElementIgnoreAttributes(
                                  object, name, function, DONT_ENUM));
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FinalizeClassDefinition) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, constructor, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, prototype, 1);

  JSObject::MigrateSlowToFast(constructor, 0, "RuntimeToFastProperties");

  if (constructor->map()->is_strong()) {
    DCHECK(prototype->map()->is_strong());
    MAYBE_RETURN(JSReceiver::SetIntegrityLevel(prototype, FROZEN,
                                               Object::THROW_ON_ERROR),
                 isolate->heap()->exception());
    MAYBE_RETURN(JSReceiver::SetIntegrityLevel(constructor, FROZEN,
                                               Object::THROW_ON_ERROR),
                 isolate->heap()->exception());
  }
  return *constructor;
}


static MaybeHandle<Object> LoadFromSuper(Isolate* isolate,
                                         Handle<Object> receiver,
                                         Handle<JSObject> home_object,
                                         Handle<Name> name,
                                         LanguageMode language_mode) {
  if (home_object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context()), home_object)) {
    isolate->ReportFailedAccessCheck(home_object);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  }

  PrototypeIterator iter(isolate, home_object);
  Handle<Object> proto = PrototypeIterator::GetCurrent(iter);
  if (!proto->IsJSReceiver()) {
    return Object::ReadAbsentProperty(isolate, proto, name, language_mode);
  }

  LookupIterator it(receiver, name, Handle<JSReceiver>::cast(proto));
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                             Object::GetProperty(&it, language_mode), Object);
  return result;
}


static MaybeHandle<Object> LoadElementFromSuper(Isolate* isolate,
                                                Handle<Object> receiver,
                                                Handle<JSObject> home_object,
                                                uint32_t index,
                                                LanguageMode language_mode) {
  if (home_object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context()), home_object)) {
    isolate->ReportFailedAccessCheck(home_object);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  }

  PrototypeIterator iter(isolate, home_object);
  Handle<Object> proto = PrototypeIterator::GetCurrent(iter);
  if (!proto->IsJSReceiver()) {
    Handle<Object> name = isolate->factory()->NewNumberFromUint(index);
    return Object::ReadAbsentProperty(isolate, proto, name, language_mode);
  }

  LookupIterator it(isolate, receiver, index, Handle<JSReceiver>::cast(proto));
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                             Object::GetProperty(&it, language_mode), Object);
  return result;
}


// TODO(conradw): It would be more efficient to have a separate runtime function
// for strong mode.
RUNTIME_FUNCTION(Runtime_LoadFromSuper) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 2);
  CONVERT_LANGUAGE_MODE_ARG_CHECKED(language_mode, 3);

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      LoadFromSuper(isolate, receiver, home_object, name, language_mode));
  return *result;
}


RUNTIME_FUNCTION(Runtime_LoadKeyedFromSuper) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 2);
  CONVERT_LANGUAGE_MODE_ARG_CHECKED(language_mode, 3);

  uint32_t index = 0;
  Handle<Object> result;

  if (key->ToArrayIndex(&index)) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, LoadElementFromSuper(isolate, receiver, home_object,
                                              index, language_mode));
    return *result;
  }

  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));
  // TODO(verwaest): Unify using LookupIterator.
  if (name->AsArrayIndex(&index)) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, LoadElementFromSuper(isolate, receiver, home_object,
                                              index, language_mode));
    return *result;
  }
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      LoadFromSuper(isolate, receiver, home_object, name, language_mode));
  return *result;
}


static Object* StoreToSuper(Isolate* isolate, Handle<JSObject> home_object,
                            Handle<Object> receiver, Handle<Name> name,
                            Handle<Object> value, LanguageMode language_mode) {
  if (home_object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context()), home_object)) {
    isolate->ReportFailedAccessCheck(home_object);
    RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  }

  PrototypeIterator iter(isolate, home_object);
  Handle<Object> proto = PrototypeIterator::GetCurrent(iter);
  if (!proto->IsJSReceiver()) return isolate->heap()->undefined_value();

  LookupIterator it(receiver, name, Handle<JSReceiver>::cast(proto));
  MAYBE_RETURN(Object::SetSuperProperty(&it, value, language_mode,
                                        Object::CERTAINLY_NOT_STORE_FROM_KEYED),
               isolate->heap()->exception());
  return *value;
}


static Object* StoreElementToSuper(Isolate* isolate,
                                   Handle<JSObject> home_object,
                                   Handle<Object> receiver, uint32_t index,
                                   Handle<Object> value,
                                   LanguageMode language_mode) {
  if (home_object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context()), home_object)) {
    isolate->ReportFailedAccessCheck(home_object);
    RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  }

  PrototypeIterator iter(isolate, home_object);
  Handle<Object> proto = PrototypeIterator::GetCurrent(iter);
  if (!proto->IsJSReceiver()) return isolate->heap()->undefined_value();

  LookupIterator it(isolate, receiver, index, Handle<JSReceiver>::cast(proto));
  MAYBE_RETURN(Object::SetSuperProperty(&it, value, language_mode,
                                        Object::MAY_BE_STORE_FROM_KEYED),
               isolate->heap()->exception());
  return *value;
}


RUNTIME_FUNCTION(Runtime_StoreToSuper_Strict) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 3);

  return StoreToSuper(isolate, home_object, receiver, name, value, STRICT);
}


RUNTIME_FUNCTION(Runtime_StoreToSuper_Sloppy) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 3);

  return StoreToSuper(isolate, home_object, receiver, name, value, SLOPPY);
}


static Object* StoreKeyedToSuper(Isolate* isolate, Handle<JSObject> home_object,
                                 Handle<Object> receiver, Handle<Object> key,
                                 Handle<Object> value,
                                 LanguageMode language_mode) {
  uint32_t index = 0;

  if (key->ToArrayIndex(&index)) {
    return StoreElementToSuper(isolate, home_object, receiver, index, value,
                               language_mode);
  }
  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));
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
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 3);

  return StoreKeyedToSuper(isolate, home_object, receiver, key, value, STRICT);
}


RUNTIME_FUNCTION(Runtime_StoreKeyedToSuper_Sloppy) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 3);

  return StoreKeyedToSuper(isolate, home_object, receiver, key, value, SLOPPY);
}


RUNTIME_FUNCTION(Runtime_GetSuperConstructor) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSFunction, active_function, 0);
  return active_function->map()->prototype();
}

}  // namespace internal
}  // namespace v8
