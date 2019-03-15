// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include <stdlib.h>
#include <limits>

#include "src/accessors.h"
#include "src/arguments-inl.h"
#include "src/counters.h"
#include "src/debug/debug.h"
#include "src/elements.h"
#include "src/isolate-inl.h"
#include "src/log.h"
#include "src/message-template.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/smi.h"
#include "src/objects/struct-inl.h"
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
  Handle<String> name(constructor->shared()->Name(), isolate);
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

RUNTIME_FUNCTION(Runtime_ThrowSuperNotCalled) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewReferenceError(MessageTemplate::kSuperNotCalled));
}

namespace {

Object ThrowNotSuperConstructor(Isolate* isolate, Handle<Object> constructor,
                                Handle<JSFunction> function) {
  Handle<String> super_name;
  if (constructor->IsJSFunction()) {
    super_name = handle(Handle<JSFunction>::cast(constructor)->shared()->Name(),
                        isolate);
  } else if (constructor->IsOddball()) {
    DCHECK(constructor->IsNull(isolate));
    super_name = isolate->factory()->null_string();
  } else {
    super_name = Object::NoSideEffectsToString(isolate, constructor);
  }
  // null constructor
  if (super_name->length() == 0) {
    super_name = isolate->factory()->null_string();
  }
  Handle<String> function_name(function->shared()->Name(), isolate);
  // anonymous class
  if (function_name->length() == 0) {
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
  return ReadOnlyRoots(isolate).home_object_symbol();
}

namespace {

template <typename Dictionary>
Handle<Name> KeyToName(Isolate* isolate, Handle<Object> key);

template <>
Handle<Name> KeyToName<NameDictionary>(Isolate* isolate, Handle<Object> key) {
  DCHECK(key->IsName());
  return Handle<Name>::cast(key);
}

template <>
Handle<Name> KeyToName<NumberDictionary>(Isolate* isolate, Handle<Object> key) {
  DCHECK(key->IsNumber());
  return isolate->factory()->NumberToString(key);
}

inline void SetHomeObject(Isolate* isolate, JSFunction method,
                          JSObject home_object) {
  if (method->shared()->needs_home_object()) {
    const int kPropertyIndex = JSFunction::kMaybeHomeObjectDescriptorIndex;
    CHECK_EQ(method->map()->instance_descriptors()->GetKey(kPropertyIndex),
             ReadOnlyRoots(isolate).home_object_symbol());

    FieldIndex field_index =
        FieldIndex::ForDescriptor(method->map(), kPropertyIndex);
    method->RawFastPropertyAtPut(field_index, home_object);
  }
}

// Gets |index|'th argument which may be a class constructor object, a class
// prototype object or a class method. In the latter case the following
// post-processing may be required:
// 1) set [[HomeObject]] slot to given |home_object| value if the method's
//    shared function info indicates that the method requires that;
// 2) set method's name to a concatenation of |name_prefix| and |key| if the
//    method's shared function info indicates that method does not have a
//    shared name.
template <typename Dictionary>
MaybeHandle<Object> GetMethodAndSetHomeObjectAndName(
    Isolate* isolate, Arguments& args, Smi index, Handle<JSObject> home_object,
    Handle<String> name_prefix, Handle<Object> key) {
  int int_index = index.value();

  // Class constructor and prototype values do not require post processing.
  if (int_index < ClassBoilerplate::kFirstDynamicArgumentIndex) {
    return args.at<Object>(int_index);
  }

  Handle<JSFunction> method = args.at<JSFunction>(int_index);

  SetHomeObject(isolate, *method, *home_object);

  if (!method->shared()->HasSharedName()) {
    // TODO(ishell): method does not have a shared name at this point only if
    // the key is a computed property name. However, the bytecode generator
    // explicitly generates ToName bytecodes to ensure that the computed
    // property name is properly converted to Name. So, we can actually be smart
    // here and avoid converting Smi keys back to Name.
    Handle<Name> name = KeyToName<Dictionary>(isolate, key);
    if (!JSFunction::SetName(method, name, name_prefix)) {
      return MaybeHandle<Object>();
    }
  }
  return method;
}

// Gets |index|'th argument which may be a class constructor object, a class
// prototype object or a class method. In the latter case the following
// post-processing may be required:
// 1) set [[HomeObject]] slot to given |home_object| value if the method's
//    shared function info indicates that the method requires that;
// This is a simplified version of GetMethodWithSharedNameAndSetHomeObject()
// function above that is used when it's guaranteed that the method has
// shared name.
Object GetMethodWithSharedNameAndSetHomeObject(Isolate* isolate,
                                               Arguments& args, Object index,
                                               JSObject home_object) {
  DisallowHeapAllocation no_gc;
  int int_index = Smi::ToInt(index);

  // Class constructor and prototype values do not require post processing.
  if (int_index < ClassBoilerplate::kFirstDynamicArgumentIndex) {
    return args[int_index];
  }

  Handle<JSFunction> method = args.at<JSFunction>(int_index);

  SetHomeObject(isolate, *method, home_object);

  DCHECK(method->shared()->HasSharedName());
  return *method;
}

template <typename Dictionary>
Handle<Dictionary> ShallowCopyDictionaryTemplate(
    Isolate* isolate, Handle<Dictionary> dictionary_template) {
  Handle<Map> dictionary_map(dictionary_template->map(), isolate);
  Handle<Dictionary> dictionary =
      Handle<Dictionary>::cast(isolate->factory()->CopyFixedArrayWithMap(
          dictionary_template, dictionary_map));
  // Clone all AccessorPairs in the dictionary.
  int capacity = dictionary->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object value = dictionary->ValueAt(i);
    if (value->IsAccessorPair()) {
      Handle<AccessorPair> pair(AccessorPair::cast(value), isolate);
      pair = AccessorPair::Copy(isolate, pair);
      dictionary->ValueAtPut(i, *pair);
    }
  }
  return dictionary;
}

template <typename Dictionary>
bool SubstituteValues(Isolate* isolate, Handle<Dictionary> dictionary,
                      Handle<JSObject> receiver, Arguments& args,
                      bool* install_name_accessor = nullptr) {
  Handle<Name> name_string = isolate->factory()->name_string();

  // Replace all indices with proper methods.
  int capacity = dictionary->Capacity();
  ReadOnlyRoots roots(isolate);
  for (int i = 0; i < capacity; i++) {
    Object maybe_key = dictionary->KeyAt(i);
    if (!Dictionary::IsKey(roots, maybe_key)) continue;
    if (install_name_accessor && *install_name_accessor &&
        (maybe_key == *name_string)) {
      *install_name_accessor = false;
    }
    Handle<Object> key(maybe_key, isolate);
    Handle<Object> value(dictionary->ValueAt(i), isolate);
    if (value->IsAccessorPair()) {
      Handle<AccessorPair> pair = Handle<AccessorPair>::cast(value);
      Object tmp = pair->getter();
      if (tmp->IsSmi()) {
        Handle<Object> result;
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, result,
            GetMethodAndSetHomeObjectAndName<Dictionary>(
                isolate, args, Smi::cast(tmp), receiver,
                isolate->factory()->get_string(), key),
            false);
        pair->set_getter(*result);
      }
      tmp = pair->setter();
      if (tmp->IsSmi()) {
        Handle<Object> result;
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, result,
            GetMethodAndSetHomeObjectAndName<Dictionary>(
                isolate, args, Smi::cast(tmp), receiver,
                isolate->factory()->set_string(), key),
            false);
        pair->set_setter(*result);
      }
    } else if (value->IsSmi()) {
      Handle<Object> result;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, result,
          GetMethodAndSetHomeObjectAndName<Dictionary>(
              isolate, args, Smi::cast(*value), receiver,
              isolate->factory()->empty_string(), key),
          false);
      dictionary->ValueAtPut(i, *result);
    }
  }
  return true;
}

bool AddDescriptorsByTemplate(
    Isolate* isolate, Handle<Map> map,
    Handle<DescriptorArray> descriptors_template,
    Handle<NumberDictionary> elements_dictionary_template,
    Handle<JSObject> receiver, Arguments& args) {
  int nof_descriptors = descriptors_template->number_of_descriptors();

  Handle<DescriptorArray> descriptors =
      DescriptorArray::Allocate(isolate, nof_descriptors, 0);

  Handle<NumberDictionary> elements_dictionary =
      *elements_dictionary_template ==
              ReadOnlyRoots(isolate).empty_slow_element_dictionary()
          ? elements_dictionary_template
          : ShallowCopyDictionaryTemplate(isolate,
                                          elements_dictionary_template);

  Handle<PropertyArray> property_array =
      isolate->factory()->empty_property_array();
  if (FLAG_track_constant_fields) {
    // If we store constants in instances, count the number of properties
    // that must be in the instance and create the property array to
    // hold the constants.
    int count = 0;
    for (int i = 0; i < nof_descriptors; i++) {
      PropertyDetails details = descriptors_template->GetDetails(i);
      if (details.location() == kDescriptor && details.kind() == kData) {
        count++;
      }
    }
    property_array = isolate->factory()->NewPropertyArray(count);
  }

  // Read values from |descriptors_template| and store possibly post-processed
  // values into "instantiated" |descriptors| array.
  int field_index = 0;
  for (int i = 0; i < nof_descriptors; i++) {
    Object value = descriptors_template->GetStrongValue(i);
    if (value->IsAccessorPair()) {
      Handle<AccessorPair> pair = AccessorPair::Copy(
          isolate, handle(AccessorPair::cast(value), isolate));
      value = *pair;
    }
    DisallowHeapAllocation no_gc;
    Name name = descriptors_template->GetKey(i);
    DCHECK(name->IsUniqueName());
    PropertyDetails details = descriptors_template->GetDetails(i);
    if (details.location() == kDescriptor) {
      if (details.kind() == kData) {
        if (value->IsSmi()) {
          value = GetMethodWithSharedNameAndSetHomeObject(isolate, args, value,
                                                          *receiver);
        }
        details =
            details.CopyWithRepresentation(value->OptimalRepresentation());
      } else {
        DCHECK_EQ(kAccessor, details.kind());
        if (value->IsAccessorPair()) {
          AccessorPair pair = AccessorPair::cast(value);
          Object tmp = pair->getter();
          if (tmp->IsSmi()) {
            pair->set_getter(GetMethodWithSharedNameAndSetHomeObject(
                isolate, args, tmp, *receiver));
          }
          tmp = pair->setter();
          if (tmp->IsSmi()) {
            pair->set_setter(GetMethodWithSharedNameAndSetHomeObject(
                isolate, args, tmp, *receiver));
          }
        }
      }
    } else {
      UNREACHABLE();
    }
    DCHECK(value->FitsRepresentation(details.representation()));
    // With constant field tracking, we store the values in the instance.
    if (FLAG_track_constant_fields && details.location() == kDescriptor &&
        details.kind() == kData) {
      details = PropertyDetails(details.kind(), details.attributes(), kField,
                                PropertyConstness::kConst,
                                details.representation(), field_index)
                    .set_pointer(details.pointer());

      property_array->set(field_index, value);
      field_index++;
      descriptors->Set(i, name, MaybeObject::FromObject(FieldType::Any()),
                       details);
    } else {
      descriptors->Set(i, name, MaybeObject::FromObject(value), details);
    }
  }

  map->InitializeDescriptors(isolate, *descriptors,
                             LayoutDescriptor::FastPointerLayout());
  if (elements_dictionary->NumberOfElements() > 0) {
    if (!SubstituteValues<NumberDictionary>(isolate, elements_dictionary,
                                            receiver, args)) {
      return false;
    }
    map->set_elements_kind(DICTIONARY_ELEMENTS);
  }

  // Atomically commit the changes.
  receiver->synchronized_set_map(*map);
  if (elements_dictionary->NumberOfElements() > 0) {
    receiver->set_elements(*elements_dictionary);
  }
  if (property_array->length() > 0) {
    receiver->SetProperties(*property_array);
  }
  return true;
}

bool AddDescriptorsByTemplate(
    Isolate* isolate, Handle<Map> map,
    Handle<NameDictionary> properties_dictionary_template,
    Handle<NumberDictionary> elements_dictionary_template,
    Handle<FixedArray> computed_properties, Handle<JSObject> receiver,
    bool install_name_accessor, Arguments& args) {
  int computed_properties_length = computed_properties->length();

  // Shallow-copy properties template.
  Handle<NameDictionary> properties_dictionary =
      ShallowCopyDictionaryTemplate(isolate, properties_dictionary_template);
  Handle<NumberDictionary> elements_dictionary =
      ShallowCopyDictionaryTemplate(isolate, elements_dictionary_template);

  typedef ClassBoilerplate::ValueKind ValueKind;
  typedef ClassBoilerplate::ComputedEntryFlags ComputedEntryFlags;

  // Merge computed properties with properties and elements dictionary
  // templates.
  int i = 0;
  while (i < computed_properties_length) {
    int flags = Smi::ToInt(computed_properties->get(i++));

    ValueKind value_kind = ComputedEntryFlags::ValueKindBits::decode(flags);
    int key_index = ComputedEntryFlags::KeyIndexBits::decode(flags);
    Object value = Smi::FromInt(key_index + 1);  // Value follows name.

    Handle<Object> key = args.at<Object>(key_index);
    DCHECK(key->IsName());
    uint32_t element;
    Handle<Name> name = Handle<Name>::cast(key);
    if (name->AsArrayIndex(&element)) {
      ClassBoilerplate::AddToElementsTemplate(
          isolate, elements_dictionary, element, key_index, value_kind, value);

    } else {
      name = isolate->factory()->InternalizeName(name);
      ClassBoilerplate::AddToPropertiesTemplate(
          isolate, properties_dictionary, name, key_index, value_kind, value);
    }
  }

  // Replace all indices with proper methods.
  if (!SubstituteValues<NameDictionary>(isolate, properties_dictionary,
                                        receiver, args,
                                        &install_name_accessor)) {
    return false;
  }
  if (install_name_accessor) {
    PropertyAttributes attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
    PropertyDetails details(kAccessor, attribs, PropertyCellType::kNoCell);
    Handle<NameDictionary> dict = NameDictionary::Add(
        isolate, properties_dictionary, isolate->factory()->name_string(),
        isolate->factory()->function_name_accessor(), details);
    CHECK_EQ(*dict, *properties_dictionary);
  }

  if (elements_dictionary->NumberOfElements() > 0) {
    if (!SubstituteValues<NumberDictionary>(isolate, elements_dictionary,
                                            receiver, args)) {
      return false;
    }
    map->set_elements_kind(DICTIONARY_ELEMENTS);
  }

  // Atomically commit the changes.
  receiver->synchronized_set_map(*map);
  receiver->set_raw_properties_or_hash(*properties_dictionary);
  if (elements_dictionary->NumberOfElements() > 0) {
    receiver->set_elements(*elements_dictionary);
  }
  return true;
}

Handle<JSObject> CreateClassPrototype(Isolate* isolate) {
  Factory* factory = isolate->factory();

  const int kInobjectFields = 0;

  Handle<Map> map;
  if (FLAG_track_constant_fields) {
    // For constant tracking we want to avoid tha hassle of handling
    // in-object properties, so create a map with no in-object
    // properties.

    // TODO(ishell) Support caching of zero in-object properties map
    // by ObjectLiteralMapFromCache().
    map = Map::Create(isolate, 0);
  } else {
    // Just use some JSObject map of certain size.
    map = factory->ObjectLiteralMapFromCache(isolate->native_context(),
                                             kInobjectFields);
  }

  return factory->NewJSObjectFromMap(map);
}

bool InitClassPrototype(Isolate* isolate,
                        Handle<ClassBoilerplate> class_boilerplate,
                        Handle<JSObject> prototype,
                        Handle<Object> prototype_parent,
                        Handle<JSFunction> constructor, Arguments& args) {
  Handle<Map> map(prototype->map(), isolate);
  map = Map::CopyDropDescriptors(isolate, map);
  map->set_is_prototype_map(true);
  Map::SetPrototype(isolate, map, prototype_parent);
  constructor->set_prototype_or_initial_map(*prototype);
  map->SetConstructor(*constructor);
  Handle<FixedArray> computed_properties(
      class_boilerplate->instance_computed_properties(), isolate);
  Handle<NumberDictionary> elements_dictionary_template(
      NumberDictionary::cast(class_boilerplate->instance_elements_template()),
      isolate);

  Handle<Object> properties_template(
      class_boilerplate->instance_properties_template(), isolate);
  if (properties_template->IsNameDictionary()) {
    Handle<NameDictionary> properties_dictionary_template =
        Handle<NameDictionary>::cast(properties_template);

    map->set_is_dictionary_map(true);
    map->set_is_migration_target(false);
    map->set_may_have_interesting_symbols(true);
    map->set_construction_counter(Map::kNoSlackTracking);

    // We care about name property only for class constructor.
    const bool install_name_accessor = false;

    return AddDescriptorsByTemplate(
        isolate, map, properties_dictionary_template,
        elements_dictionary_template, computed_properties, prototype,
        install_name_accessor, args);
  } else {
    Handle<DescriptorArray> descriptors_template =
        Handle<DescriptorArray>::cast(properties_template);

    // The size of the prototype object is known at this point.
    // So we can create it now and then add the rest instance methods to the
    // map.
    return AddDescriptorsByTemplate(isolate, map, descriptors_template,
                                    elements_dictionary_template, prototype,
                                    args);
  }
}

bool InitClassConstructor(Isolate* isolate,
                          Handle<ClassBoilerplate> class_boilerplate,
                          Handle<Object> constructor_parent,
                          Handle<JSFunction> constructor, Arguments& args) {
  Handle<Map> map(constructor->map(), isolate);
  map = Map::CopyDropDescriptors(isolate, map);
  DCHECK(map->is_prototype_map());

  if (!constructor_parent.is_null()) {
    // Set map's prototype without enabling prototype setup mode for superclass
    // because it does not make sense.
    Map::SetPrototype(isolate, map, constructor_parent, false);
  }

  Handle<NumberDictionary> elements_dictionary_template(
      NumberDictionary::cast(class_boilerplate->static_elements_template()),
      isolate);
  Handle<FixedArray> computed_properties(
      class_boilerplate->static_computed_properties(), isolate);

  Handle<Object> properties_template(
      class_boilerplate->static_properties_template(), isolate);

  if (properties_template->IsNameDictionary()) {
    Handle<NameDictionary> properties_dictionary_template =
        Handle<NameDictionary>::cast(properties_template);

    map->set_is_dictionary_map(true);
    map->InitializeDescriptors(isolate,
                               ReadOnlyRoots(isolate).empty_descriptor_array(),
                               LayoutDescriptor::FastPointerLayout());
    map->set_is_migration_target(false);
    map->set_may_have_interesting_symbols(true);
    map->set_construction_counter(Map::kNoSlackTracking);

    bool install_name_accessor =
        class_boilerplate->install_class_name_accessor() != 0;

    return AddDescriptorsByTemplate(
        isolate, map, properties_dictionary_template,
        elements_dictionary_template, computed_properties, constructor,
        install_name_accessor, args);
  } else {
    Handle<DescriptorArray> descriptors_template =
        Handle<DescriptorArray>::cast(properties_template);

    return AddDescriptorsByTemplate(isolate, map, descriptors_template,
                                    elements_dictionary_template, constructor,
                                    args);
  }
}

MaybeHandle<Object> DefineClass(Isolate* isolate,
                                Handle<ClassBoilerplate> class_boilerplate,
                                Handle<Object> super_class,
                                Handle<JSFunction> constructor,
                                Arguments& args) {
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
      // Create new handle to avoid |constructor_parent| corruption because of
      // |super_class| handle value overwriting via storing to
      // args[ClassBoilerplate::kPrototypeArgumentIndex] below.
      constructor_parent = handle(*super_class, isolate);
    } else {
      THROW_NEW_ERROR(isolate,
                      NewTypeError(MessageTemplate::kExtendsValueNotConstructor,
                                   super_class),
                      Object);
    }
  }

  Handle<JSObject> prototype = CreateClassPrototype(isolate);
  DCHECK_EQ(*constructor, args[ClassBoilerplate::kConstructorArgumentIndex]);
  args.set_at(ClassBoilerplate::kPrototypeArgumentIndex, *prototype);

  if (!InitClassConstructor(isolate, class_boilerplate, constructor_parent,
                            constructor, args) ||
      !InitClassPrototype(isolate, class_boilerplate, prototype,
                          prototype_parent, constructor, args)) {
    DCHECK(isolate->has_pending_exception());
    return MaybeHandle<Object>();
  }
  if (FLAG_trace_maps) {
    LOG(isolate,
        MapEvent("InitialMap", Map(), constructor->map(),
                 "init class constructor", constructor->shared()->DebugName()));
    LOG(isolate, MapEvent("InitialMap", Map(), prototype->map(),
                          "init class prototype"));
  }

  return prototype;
}

}  // namespace

RUNTIME_FUNCTION(Runtime_DefineClass) {
  HandleScope scope(isolate);
  DCHECK_LE(ClassBoilerplate::kFirstDynamicArgumentIndex, args.length());
  CONVERT_ARG_HANDLE_CHECKED(ClassBoilerplate, class_boilerplate, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, super_class, 2);
  DCHECK_EQ(class_boilerplate->arguments_count(), args.length());

  RETURN_RESULT_OR_FAILURE(
      isolate,
      DefineClass(isolate, class_boilerplate, super_class, constructor, args));
}

namespace {

enum class SuperMode { kLoad, kStore };

MaybeHandle<JSReceiver> GetSuperHolder(
    Isolate* isolate, Handle<Object> receiver, Handle<JSObject> home_object,
    SuperMode mode, MaybeHandle<Name> maybe_name, uint32_t index) {
  if (home_object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context(), isolate), home_object)) {
    isolate->ReportFailedAccessCheck(home_object);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, JSReceiver);
  }

  PrototypeIterator iter(isolate, home_object);
  Handle<Object> proto = PrototypeIterator::GetCurrent(iter);
  if (!proto->IsJSReceiver()) {
    MessageTemplate message = mode == SuperMode::kLoad
                                  ? MessageTemplate::kNonObjectPropertyLoad
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
                                 Handle<Object> value) {
  Handle<JSReceiver> holder;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, holder,
                             GetSuperHolder(isolate, receiver, home_object,
                                            SuperMode::kStore, name, 0),
                             Object);
  LookupIterator it(receiver, name, holder);
  MAYBE_RETURN(Object::SetSuperProperty(&it, value, StoreOrigin::kNamed),
               MaybeHandle<Object>());
  return value;
}

MaybeHandle<Object> StoreElementToSuper(Isolate* isolate,
                                        Handle<JSObject> home_object,
                                        Handle<Object> receiver, uint32_t index,
                                        Handle<Object> value) {
  Handle<JSReceiver> holder;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, holder,
      GetSuperHolder(isolate, receiver, home_object, SuperMode::kStore,
                     MaybeHandle<Name>(), index),
      Object);
  LookupIterator it(isolate, receiver, index, holder);
  MAYBE_RETURN(Object::SetSuperProperty(&it, value, StoreOrigin::kMaybeKeyed),
               MaybeHandle<Object>());
  return value;
}

}  // anonymous namespace

RUNTIME_FUNCTION(Runtime_StoreToSuper) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 3);

  RETURN_RESULT_OR_FAILURE(
      isolate, StoreToSuper(isolate, home_object, receiver, name, value));
}

static MaybeHandle<Object> StoreKeyedToSuper(Isolate* isolate,
                                             Handle<JSObject> home_object,
                                             Handle<Object> receiver,
                                             Handle<Object> key,
                                             Handle<Object> value) {
  uint32_t index = 0;

  if (key->ToArrayIndex(&index)) {
    return StoreElementToSuper(isolate, home_object, receiver, index, value);
  }
  Handle<Name> name;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, name, Object::ToName(isolate, key),
                             Object);
  // TODO(verwaest): Unify using LookupIterator.
  if (name->AsArrayIndex(&index)) {
    return StoreElementToSuper(isolate, home_object, receiver, index, value);
  }
  return StoreToSuper(isolate, home_object, receiver, name, value);
}

RUNTIME_FUNCTION(Runtime_StoreKeyedToSuper) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 3);

  RETURN_RESULT_OR_FAILURE(
      isolate, StoreKeyedToSuper(isolate, home_object, receiver, key, value));
}

}  // namespace internal
}  // namespace v8
