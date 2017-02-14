// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

#include "src/code-factory.h"
#include "src/property-descriptor.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 19.1 Object Objects

void Builtins::Generate_ObjectHasOwnProperty(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;

  Node* object = assembler->Parameter(0);
  Node* key = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);

  Label call_runtime(assembler), return_true(assembler),
      return_false(assembler);

  // Smi receivers do not have own properties.
  Label if_objectisnotsmi(assembler);
  assembler->Branch(assembler->TaggedIsSmi(object), &return_false,
                    &if_objectisnotsmi);
  assembler->Bind(&if_objectisnotsmi);

  Node* map = assembler->LoadMap(object);
  Node* instance_type = assembler->LoadMapInstanceType(map);

  Variable var_index(assembler, MachineType::PointerRepresentation());

  Label keyisindex(assembler), if_iskeyunique(assembler);
  assembler->TryToName(key, &keyisindex, &var_index, &if_iskeyunique,
                       &call_runtime);

  assembler->Bind(&if_iskeyunique);
  assembler->TryHasOwnProperty(object, map, instance_type, key, &return_true,
                               &return_false, &call_runtime);

  assembler->Bind(&keyisindex);
  // Handle negative keys in the runtime.
  assembler->GotoIf(assembler->IntPtrLessThan(var_index.value(),
                                              assembler->IntPtrConstant(0)),
                    &call_runtime);
  assembler->TryLookupElement(object, map, instance_type, var_index.value(),
                              &return_true, &return_false, &call_runtime);

  assembler->Bind(&return_true);
  assembler->Return(assembler->BooleanConstant(true));

  assembler->Bind(&return_false);
  assembler->Return(assembler->BooleanConstant(false));

  assembler->Bind(&call_runtime);
  assembler->Return(assembler->CallRuntime(Runtime::kObjectHasOwnProperty,
                                           context, object, key));
}

namespace {

MUST_USE_RESULT Maybe<bool> FastAssign(Handle<JSReceiver> to,
                                       Handle<Object> next_source) {
  // Non-empty strings are the only non-JSReceivers that need to be handled
  // explicitly by Object.assign.
  if (!next_source->IsJSReceiver()) {
    return Just(!next_source->IsString() ||
                String::cast(*next_source)->length() == 0);
  }

  // If the target is deprecated, the object will be updated on first store. If
  // the source for that store equals the target, this will invalidate the
  // cached representation of the source. Preventively upgrade the target.
  // Do this on each iteration since any property load could cause deprecation.
  if (to->map()->is_deprecated()) {
    JSObject::MigrateInstance(Handle<JSObject>::cast(to));
  }

  Isolate* isolate = to->GetIsolate();
  Handle<Map> map(JSReceiver::cast(*next_source)->map(), isolate);

  if (!map->IsJSObjectMap()) return Just(false);
  if (!map->OnlyHasSimpleProperties()) return Just(false);

  Handle<JSObject> from = Handle<JSObject>::cast(next_source);
  if (from->elements() != isolate->heap()->empty_fixed_array()) {
    return Just(false);
  }

  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);
  int length = map->NumberOfOwnDescriptors();

  bool stable = true;

  for (int i = 0; i < length; i++) {
    Handle<Name> next_key(descriptors->GetKey(i), isolate);
    Handle<Object> prop_value;
    // Directly decode from the descriptor array if |from| did not change shape.
    if (stable) {
      PropertyDetails details = descriptors->GetDetails(i);
      if (!details.IsEnumerable()) continue;
      if (details.kind() == kData) {
        if (details.location() == kDescriptor) {
          prop_value = handle(descriptors->GetValue(i), isolate);
        } else {
          Representation representation = details.representation();
          FieldIndex index = FieldIndex::ForDescriptor(*map, i);
          prop_value = JSObject::FastPropertyAt(from, representation, index);
        }
      } else {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, prop_value, JSReceiver::GetProperty(from, next_key),
            Nothing<bool>());
        stable = from->map() == *map;
      }
    } else {
      // If the map did change, do a slower lookup. We are still guaranteed that
      // the object has a simple shape, and that the key is a name.
      LookupIterator it(from, next_key, from,
                        LookupIterator::OWN_SKIP_INTERCEPTOR);
      if (!it.IsFound()) continue;
      DCHECK(it.state() == LookupIterator::DATA ||
             it.state() == LookupIterator::ACCESSOR);
      if (!it.IsEnumerable()) continue;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, prop_value, Object::GetProperty(&it), Nothing<bool>());
    }
    LookupIterator it(to, next_key, to);
    bool call_to_js = it.IsFound() && it.state() != LookupIterator::DATA;
    Maybe<bool> result = Object::SetProperty(
        &it, prop_value, STRICT, Object::CERTAINLY_NOT_STORE_FROM_KEYED);
    if (result.IsNothing()) return result;
    if (stable && call_to_js) stable = from->map() == *map;
  }

  return Just(true);
}

}  // namespace

// ES6 19.1.2.1 Object.assign
BUILTIN(ObjectAssign) {
  HandleScope scope(isolate);
  Handle<Object> target = args.atOrUndefined(isolate, 1);

  // 1. Let to be ? ToObject(target).
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, target,
                                     Object::ToObject(isolate, target));
  Handle<JSReceiver> to = Handle<JSReceiver>::cast(target);
  // 2. If only one argument was passed, return to.
  if (args.length() == 2) return *to;
  // 3. Let sources be the List of argument values starting with the
  //    second argument.
  // 4. For each element nextSource of sources, in ascending index order,
  for (int i = 2; i < args.length(); ++i) {
    Handle<Object> next_source = args.at<Object>(i);
    Maybe<bool> fast_assign = FastAssign(to, next_source);
    if (fast_assign.IsNothing()) return isolate->heap()->exception();
    if (fast_assign.FromJust()) continue;
    // 4a. If nextSource is undefined or null, let keys be an empty List.
    // 4b. Else,
    // 4b i. Let from be ToObject(nextSource).
    // Only non-empty strings and JSReceivers have enumerable properties.
    Handle<JSReceiver> from =
        Object::ToObject(isolate, next_source).ToHandleChecked();
    // 4b ii. Let keys be ? from.[[OwnPropertyKeys]]().
    Handle<FixedArray> keys;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, keys, KeyAccumulator::GetKeys(
                           from, KeyCollectionMode::kOwnOnly, ALL_PROPERTIES,
                           GetKeysConversion::kKeepNumbers));
    // 4c. Repeat for each element nextKey of keys in List order,
    for (int j = 0; j < keys->length(); ++j) {
      Handle<Object> next_key(keys->get(j), isolate);
      // 4c i. Let desc be ? from.[[GetOwnProperty]](nextKey).
      PropertyDescriptor desc;
      Maybe<bool> found =
          JSReceiver::GetOwnPropertyDescriptor(isolate, from, next_key, &desc);
      if (found.IsNothing()) return isolate->heap()->exception();
      // 4c ii. If desc is not undefined and desc.[[Enumerable]] is true, then
      if (found.FromJust() && desc.enumerable()) {
        // 4c ii 1. Let propValue be ? Get(from, nextKey).
        Handle<Object> prop_value;
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
            isolate, prop_value,
            Runtime::GetObjectProperty(isolate, from, next_key));
        // 4c ii 2. Let status be ? Set(to, nextKey, propValue, true).
        Handle<Object> status;
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
            isolate, status, Runtime::SetObjectProperty(isolate, to, next_key,
                                                        prop_value, STRICT));
      }
    }
  }
  // 5. Return to.
  return *to;
}

// ES6 section 19.1.3.4 Object.prototype.propertyIsEnumerable ( V )
BUILTIN(ObjectPrototypePropertyIsEnumerable) {
  HandleScope scope(isolate);
  Handle<JSReceiver> object;
  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, name, Object::ToName(isolate, args.atOrUndefined(isolate, 1)));
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, object, JSReceiver::ToObject(isolate, args.receiver()));
  Maybe<PropertyAttributes> maybe =
      JSReceiver::GetOwnPropertyAttributes(object, name);
  if (!maybe.IsJust()) return isolate->heap()->exception();
  if (maybe.FromJust() == ABSENT) return isolate->heap()->false_value();
  return isolate->heap()->ToBoolean((maybe.FromJust() & DONT_ENUM) == 0);
}

namespace {  // anonymous namespace for ObjectProtoToString()

void IsString(CodeStubAssembler* assembler, compiler::Node* object,
              CodeStubAssembler::Label* if_string,
              CodeStubAssembler::Label* if_notstring) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;

  Label if_notsmi(assembler);
  assembler->Branch(assembler->TaggedIsSmi(object), if_notstring, &if_notsmi);

  assembler->Bind(&if_notsmi);
  {
    Node* instance_type = assembler->LoadInstanceType(object);

    assembler->Branch(assembler->IsStringInstanceType(instance_type), if_string,
                      if_notstring);
  }
}

void ReturnToStringFormat(CodeStubAssembler* assembler, compiler::Node* context,
                          compiler::Node* string) {
  typedef compiler::Node Node;

  Node* lhs = assembler->HeapConstant(
      assembler->factory()->NewStringFromStaticChars("[object "));
  Node* rhs = assembler->HeapConstant(
      assembler->factory()->NewStringFromStaticChars("]"));

  Callable callable = CodeFactory::StringAdd(
      assembler->isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED);

  assembler->Return(assembler->CallStub(
      callable, context, assembler->CallStub(callable, context, lhs, string),
      rhs));
}

void ReturnIfPrimitive(CodeStubAssembler* assembler,
                       compiler::Node* instance_type,
                       CodeStubAssembler::Label* return_string,
                       CodeStubAssembler::Label* return_boolean,
                       CodeStubAssembler::Label* return_number) {
  assembler->GotoIf(assembler->IsStringInstanceType(instance_type),
                    return_string);

  assembler->GotoIf(assembler->Word32Equal(
                        instance_type, assembler->Int32Constant(ODDBALL_TYPE)),
                    return_boolean);

  assembler->GotoIf(
      assembler->Word32Equal(instance_type,
                             assembler->Int32Constant(HEAP_NUMBER_TYPE)),
      return_number);
}

}  // namespace

// ES6 section 19.1.3.6 Object.prototype.toString
void Builtins::Generate_ObjectProtoToString(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;

  Label return_undefined(assembler, Label::kDeferred),
      return_null(assembler, Label::kDeferred),
      return_arguments(assembler, Label::kDeferred), return_array(assembler),
      return_api(assembler, Label::kDeferred), return_object(assembler),
      return_regexp(assembler), return_function(assembler),
      return_error(assembler), return_date(assembler), return_string(assembler),
      return_boolean(assembler), return_jsvalue(assembler),
      return_jsproxy(assembler, Label::kDeferred), return_number(assembler);

  Label if_isproxy(assembler, Label::kDeferred);

  Label checkstringtag(assembler);
  Label if_tostringtag(assembler), if_notostringtag(assembler);

  Node* receiver = assembler->Parameter(0);
  Node* context = assembler->Parameter(3);

  assembler->GotoIf(
      assembler->WordEqual(receiver, assembler->UndefinedConstant()),
      &return_undefined);

  assembler->GotoIf(assembler->WordEqual(receiver, assembler->NullConstant()),
                    &return_null);

  assembler->GotoIf(assembler->TaggedIsSmi(receiver), &return_number);

  Node* receiver_instance_type = assembler->LoadInstanceType(receiver);
  ReturnIfPrimitive(assembler, receiver_instance_type, &return_string,
                    &return_boolean, &return_number);

  // for proxies, check IsArray before getting @@toStringTag
  Variable var_proxy_is_array(assembler, MachineRepresentation::kTagged);
  var_proxy_is_array.Bind(assembler->BooleanConstant(false));

  assembler->Branch(
      assembler->Word32Equal(receiver_instance_type,
                             assembler->Int32Constant(JS_PROXY_TYPE)),
      &if_isproxy, &checkstringtag);

  assembler->Bind(&if_isproxy);
  {
    // This can throw
    var_proxy_is_array.Bind(
        assembler->CallRuntime(Runtime::kArrayIsArray, context, receiver));
    assembler->Goto(&checkstringtag);
  }

  assembler->Bind(&checkstringtag);
  {
    Node* to_string_tag_symbol = assembler->HeapConstant(
        assembler->isolate()->factory()->to_string_tag_symbol());

    GetPropertyStub stub(assembler->isolate());
    Callable get_property =
        Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
    Node* to_string_tag_value = assembler->CallStub(
        get_property, context, receiver, to_string_tag_symbol);

    IsString(assembler, to_string_tag_value, &if_tostringtag,
             &if_notostringtag);

    assembler->Bind(&if_tostringtag);
    ReturnToStringFormat(assembler, context, to_string_tag_value);
  }
  assembler->Bind(&if_notostringtag);
  {
    size_t const kNumCases = 11;
    Label* case_labels[kNumCases];
    int32_t case_values[kNumCases];
    case_labels[0] = &return_api;
    case_values[0] = JS_API_OBJECT_TYPE;
    case_labels[1] = &return_api;
    case_values[1] = JS_SPECIAL_API_OBJECT_TYPE;
    case_labels[2] = &return_arguments;
    case_values[2] = JS_ARGUMENTS_TYPE;
    case_labels[3] = &return_array;
    case_values[3] = JS_ARRAY_TYPE;
    case_labels[4] = &return_function;
    case_values[4] = JS_BOUND_FUNCTION_TYPE;
    case_labels[5] = &return_function;
    case_values[5] = JS_FUNCTION_TYPE;
    case_labels[6] = &return_error;
    case_values[6] = JS_ERROR_TYPE;
    case_labels[7] = &return_date;
    case_values[7] = JS_DATE_TYPE;
    case_labels[8] = &return_regexp;
    case_values[8] = JS_REGEXP_TYPE;
    case_labels[9] = &return_jsvalue;
    case_values[9] = JS_VALUE_TYPE;
    case_labels[10] = &return_jsproxy;
    case_values[10] = JS_PROXY_TYPE;

    assembler->Switch(receiver_instance_type, &return_object, case_values,
                      case_labels, arraysize(case_values));

    assembler->Bind(&return_undefined);
    assembler->Return(assembler->HeapConstant(
        assembler->isolate()->factory()->undefined_to_string()));

    assembler->Bind(&return_null);
    assembler->Return(assembler->HeapConstant(
        assembler->isolate()->factory()->null_to_string()));

    assembler->Bind(&return_number);
    assembler->Return(assembler->HeapConstant(
        assembler->isolate()->factory()->number_to_string()));

    assembler->Bind(&return_string);
    assembler->Return(assembler->HeapConstant(
        assembler->isolate()->factory()->string_to_string()));

    assembler->Bind(&return_boolean);
    assembler->Return(assembler->HeapConstant(
        assembler->isolate()->factory()->boolean_to_string()));

    assembler->Bind(&return_arguments);
    assembler->Return(assembler->HeapConstant(
        assembler->isolate()->factory()->arguments_to_string()));

    assembler->Bind(&return_array);
    assembler->Return(assembler->HeapConstant(
        assembler->isolate()->factory()->array_to_string()));

    assembler->Bind(&return_function);
    assembler->Return(assembler->HeapConstant(
        assembler->isolate()->factory()->function_to_string()));

    assembler->Bind(&return_error);
    assembler->Return(assembler->HeapConstant(
        assembler->isolate()->factory()->error_to_string()));

    assembler->Bind(&return_date);
    assembler->Return(assembler->HeapConstant(
        assembler->isolate()->factory()->date_to_string()));

    assembler->Bind(&return_regexp);
    assembler->Return(assembler->HeapConstant(
        assembler->isolate()->factory()->regexp_to_string()));

    assembler->Bind(&return_api);
    {
      Node* class_name =
          assembler->CallRuntime(Runtime::kClassOf, context, receiver);
      ReturnToStringFormat(assembler, context, class_name);
    }

    assembler->Bind(&return_jsvalue);
    {
      Node* value = assembler->LoadJSValueValue(receiver);
      assembler->GotoIf(assembler->TaggedIsSmi(value), &return_number);

      ReturnIfPrimitive(assembler, assembler->LoadInstanceType(value),
                        &return_string, &return_boolean, &return_number);
      assembler->Goto(&return_object);
    }

    assembler->Bind(&return_jsproxy);
    {
      assembler->GotoIf(assembler->WordEqual(var_proxy_is_array.value(),
                                             assembler->BooleanConstant(true)),
                        &return_array);

      Node* map = assembler->LoadMap(receiver);

      // Return object if the proxy {receiver} is not callable.
      assembler->Branch(assembler->IsCallableMap(map), &return_function,
                        &return_object);
    }

    // Default
    assembler->Bind(&return_object);
    assembler->Return(assembler->HeapConstant(
        assembler->isolate()->factory()->object_to_string()));
  }
}

void Builtins::Generate_ObjectCreate(CodeStubAssembler* a) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;

  Node* prototype = a->Parameter(1);
  Node* properties = a->Parameter(2);
  Node* context = a->Parameter(3 + 2);

  Label call_runtime(a, Label::kDeferred), prototype_valid(a), no_properties(a);
  {
    a->Comment("Argument 1 check: prototype");
    a->GotoIf(a->WordEqual(prototype, a->NullConstant()), &prototype_valid);
    a->BranchIfJSReceiver(prototype, &prototype_valid, &call_runtime);
  }

  a->Bind(&prototype_valid);
  {
    a->Comment("Argument 2 check: properties");
    // Check that we have a simple object
    a->GotoIf(a->TaggedIsSmi(properties), &call_runtime);
    // Undefined implies no properties.
    a->GotoIf(a->WordEqual(properties, a->UndefinedConstant()), &no_properties);
    Node* properties_map = a->LoadMap(properties);
    a->GotoIf(a->IsSpecialReceiverMap(properties_map), &call_runtime);
    // Stay on the fast path only if there are no elements.
    a->GotoUnless(a->WordEqual(a->LoadElements(properties),
                               a->LoadRoot(Heap::kEmptyFixedArrayRootIndex)),
                  &call_runtime);
    // Handle dictionary objects or fast objects with properties in runtime.
    Node* bit_field3 = a->LoadMapBitField3(properties_map);
    a->GotoIf(a->IsSetWord32<Map::DictionaryMap>(bit_field3), &call_runtime);
    a->Branch(a->IsSetWord32<Map::NumberOfOwnDescriptorsBits>(bit_field3),
              &call_runtime, &no_properties);
  }

  // Create a new object with the given prototype.
  a->Bind(&no_properties);
  {
    Variable map(a, MachineRepresentation::kTagged);
    Variable properties(a, MachineRepresentation::kTagged);
    Label non_null_proto(a), instantiate_map(a), good(a);

    a->Branch(a->WordEqual(prototype, a->NullConstant()), &good,
              &non_null_proto);

    a->Bind(&good);
    {
      map.Bind(a->LoadContextElement(
          context, Context::SLOW_OBJECT_WITH_NULL_PROTOTYPE_MAP));
      properties.Bind(
          a->AllocateNameDictionary(NameDictionary::kInitialCapacity));
      a->Goto(&instantiate_map);
    }

    a->Bind(&non_null_proto);
    {
      properties.Bind(a->EmptyFixedArrayConstant());
      Node* object_function =
          a->LoadContextElement(context, Context::OBJECT_FUNCTION_INDEX);
      Node* object_function_map = a->LoadObjectField(
          object_function, JSFunction::kPrototypeOrInitialMapOffset);
      map.Bind(object_function_map);
      a->GotoIf(a->WordEqual(prototype, a->LoadMapPrototype(map.value())),
                &instantiate_map);
      // Try loading the prototype info.
      Node* prototype_info =
          a->LoadMapPrototypeInfo(a->LoadMap(prototype), &call_runtime);
      a->Comment("Load ObjectCreateMap from PrototypeInfo");
      Node* weak_cell =
          a->LoadObjectField(prototype_info, PrototypeInfo::kObjectCreateMap);
      a->GotoIf(a->WordEqual(weak_cell, a->UndefinedConstant()), &call_runtime);
      map.Bind(a->LoadWeakCellValue(weak_cell, &call_runtime));
      a->Goto(&instantiate_map);
    }

    a->Bind(&instantiate_map);
    {
      Node* instance =
          a->AllocateJSObjectFromMap(map.value(), properties.value());
      a->Return(instance);
    }
  }

  a->Bind(&call_runtime);
  {
    a->Return(
        a->CallRuntime(Runtime::kObjectCreate, context, prototype, properties));
  }
}

// ES6 section 19.1.2.3 Object.defineProperties
BUILTIN(ObjectDefineProperties) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<Object> target = args.at<Object>(1);
  Handle<Object> properties = args.at<Object>(2);

  RETURN_RESULT_OR_FAILURE(
      isolate, JSReceiver::DefineProperties(isolate, target, properties));
}

// ES6 section 19.1.2.4 Object.defineProperty
BUILTIN(ObjectDefineProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  Handle<Object> target = args.at<Object>(1);
  Handle<Object> key = args.at<Object>(2);
  Handle<Object> attributes = args.at<Object>(3);

  return JSReceiver::DefineProperty(isolate, target, key, attributes);
}

namespace {

template <AccessorComponent which_accessor>
Object* ObjectDefineAccessor(Isolate* isolate, Handle<Object> object,
                             Handle<Object> name, Handle<Object> accessor) {
  // 1. Let O be ? ToObject(this value).
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ConvertReceiver(isolate, object));
  // 2. If IsCallable(getter) is false, throw a TypeError exception.
  if (!accessor->IsCallable()) {
    MessageTemplate::Template message =
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
      isolate, receiver, name, &desc, Object::DONT_THROW);
  MAYBE_RETURN(success, isolate->heap()->exception());
  if (!success.FromJust()) {
    isolate->CountUsage(v8::Isolate::kDefineGetterOrSetterWouldThrow);
  }
  // 6. Return undefined.
  return isolate->heap()->undefined_value();
}

Object* ObjectLookupAccessor(Isolate* isolate, Handle<Object> object,
                             Handle<Object> key, AccessorComponent component) {
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, object,
                                     Object::ConvertReceiver(isolate, object));
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
        return isolate->heap()->undefined_value();

      case LookupIterator::JSPROXY:
        return isolate->heap()->undefined_value();

      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return isolate->heap()->undefined_value();
      case LookupIterator::DATA:
        continue;
      case LookupIterator::ACCESSOR: {
        Handle<Object> maybe_pair = it.GetAccessors();
        if (maybe_pair->IsAccessorPair()) {
          return *AccessorPair::GetComponent(
              Handle<AccessorPair>::cast(maybe_pair), component);
        }
      }
    }
  }

  return isolate->heap()->undefined_value();
}

}  // namespace

// ES6 B.2.2.2 a.k.a.
// https://tc39.github.io/ecma262/#sec-object.prototype.__defineGetter__
BUILTIN(ObjectDefineGetter) {
  HandleScope scope(isolate);
  Handle<Object> object = args.at<Object>(0);  // Receiver.
  Handle<Object> name = args.at<Object>(1);
  Handle<Object> getter = args.at<Object>(2);
  return ObjectDefineAccessor<ACCESSOR_GETTER>(isolate, object, name, getter);
}

// ES6 B.2.2.3 a.k.a.
// https://tc39.github.io/ecma262/#sec-object.prototype.__defineSetter__
BUILTIN(ObjectDefineSetter) {
  HandleScope scope(isolate);
  Handle<Object> object = args.at<Object>(0);  // Receiver.
  Handle<Object> name = args.at<Object>(1);
  Handle<Object> setter = args.at<Object>(2);
  return ObjectDefineAccessor<ACCESSOR_SETTER>(isolate, object, name, setter);
}

// ES6 B.2.2.4 a.k.a.
// https://tc39.github.io/ecma262/#sec-object.prototype.__lookupGetter__
BUILTIN(ObjectLookupGetter) {
  HandleScope scope(isolate);
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> name = args.at<Object>(1);
  return ObjectLookupAccessor(isolate, object, name, ACCESSOR_GETTER);
}

// ES6 B.2.2.5 a.k.a.
// https://tc39.github.io/ecma262/#sec-object.prototype.__lookupSetter__
BUILTIN(ObjectLookupSetter) {
  HandleScope scope(isolate);
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> name = args.at<Object>(1);
  return ObjectLookupAccessor(isolate, object, name, ACCESSOR_SETTER);
}

// ES6 section 19.1.2.5 Object.freeze ( O )
BUILTIN(ObjectFreeze) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  if (object->IsJSReceiver()) {
    MAYBE_RETURN(JSReceiver::SetIntegrityLevel(Handle<JSReceiver>::cast(object),
                                               FROZEN, Object::THROW_ON_ERROR),
                 isolate->heap()->exception());
  }
  return *object;
}

// ES section 19.1.2.9 Object.getPrototypeOf ( O )
BUILTIN(ObjectGetPrototypeOf) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);

  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));

  RETURN_RESULT_OR_FAILURE(isolate,
                           JSReceiver::GetPrototype(isolate, receiver));
}

// ES6 section 19.1.2.21 Object.setPrototypeOf ( O, proto )
BUILTIN(ObjectSetPrototypeOf) {
  HandleScope scope(isolate);

  // 1. Let O be ? RequireObjectCoercible(O).
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  if (object->IsNull(isolate) || object->IsUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Object.setPrototypeOf")));
  }

  // 2. If Type(proto) is neither Object nor Null, throw a TypeError exception.
  Handle<Object> proto = args.atOrUndefined(isolate, 2);
  if (!proto->IsNull(isolate) && !proto->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kProtoObjectOrNull, proto));
  }

  // 3. If Type(O) is not Object, return O.
  if (!object->IsJSReceiver()) return *object;
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(object);

  // 4. Let status be ? O.[[SetPrototypeOf]](proto).
  // 5. If status is false, throw a TypeError exception.
  MAYBE_RETURN(
      JSReceiver::SetPrototype(receiver, proto, true, Object::THROW_ON_ERROR),
      isolate->heap()->exception());

  // 6. Return O.
  return *receiver;
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
  if (object->IsNull(isolate) || object->IsUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "set Object.prototype.__proto__")));
  }

  // 2. If Type(proto) is neither Object nor Null, return undefined.
  Handle<Object> proto = args.at<Object>(1);
  if (!proto->IsNull(isolate) && !proto->IsJSReceiver()) {
    return isolate->heap()->undefined_value();
  }

  // 3. If Type(O) is not Object, return undefined.
  if (!object->IsJSReceiver()) return isolate->heap()->undefined_value();
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(object);

  // 4. Let status be ? O.[[SetPrototypeOf]](proto).
  // 5. If status is false, throw a TypeError exception.
  MAYBE_RETURN(
      JSReceiver::SetPrototype(receiver, proto, true, Object::THROW_ON_ERROR),
      isolate->heap()->exception());

  // Return undefined.
  return isolate->heap()->undefined_value();
}

// ES6 section 19.1.2.6 Object.getOwnPropertyDescriptor ( O, P )
BUILTIN(ObjectGetOwnPropertyDescriptor) {
  HandleScope scope(isolate);
  // 1. Let obj be ? ToObject(O).
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));
  // 2. Let key be ? ToPropertyKey(P).
  Handle<Object> property = args.atOrUndefined(isolate, 2);
  Handle<Name> key;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, key,
                                     Object::ToName(isolate, property));
  // 3. Let desc be ? obj.[[GetOwnProperty]](key).
  PropertyDescriptor desc;
  Maybe<bool> found =
      JSReceiver::GetOwnPropertyDescriptor(isolate, receiver, key, &desc);
  MAYBE_RETURN(found, isolate->heap()->exception());
  // 4. Return FromPropertyDescriptor(desc).
  if (!found.FromJust()) return isolate->heap()->undefined_value();
  return *desc.ToObject(isolate);
}

namespace {

Object* GetOwnPropertyKeys(Isolate* isolate, BuiltinArguments args,
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

// ES6 section 19.1.2.7 Object.getOwnPropertyNames ( O )
BUILTIN(ObjectGetOwnPropertyNames) {
  return GetOwnPropertyKeys(isolate, args, SKIP_SYMBOLS);
}

// ES6 section 19.1.2.8 Object.getOwnPropertySymbols ( O )
BUILTIN(ObjectGetOwnPropertySymbols) {
  return GetOwnPropertyKeys(isolate, args, SKIP_STRINGS);
}

// ES#sec-object.is Object.is ( value1, value2 )
BUILTIN(ObjectIs) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(3, args.length());
  Handle<Object> value1 = args.at<Object>(1);
  Handle<Object> value2 = args.at<Object>(2);
  return isolate->heap()->ToBoolean(value1->SameValue(*value2));
}

// ES6 section 19.1.2.11 Object.isExtensible ( O )
BUILTIN(ObjectIsExtensible) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Maybe<bool> result =
      object->IsJSReceiver()
          ? JSReceiver::IsExtensible(Handle<JSReceiver>::cast(object))
          : Just(false);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}

// ES6 section 19.1.2.12 Object.isFrozen ( O )
BUILTIN(ObjectIsFrozen) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Maybe<bool> result = object->IsJSReceiver()
                           ? JSReceiver::TestIntegrityLevel(
                                 Handle<JSReceiver>::cast(object), FROZEN)
                           : Just(true);
  MAYBE_RETURN(result, isolate->heap()->exception());
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
  MAYBE_RETURN(result, isolate->heap()->exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}

// ES6 section 19.1.2.14 Object.keys ( O )
BUILTIN(ObjectKeys) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));

  Handle<FixedArray> keys;
  int enum_length = receiver->map()->EnumLength();
  if (enum_length != kInvalidEnumCacheSentinel &&
      JSObject::cast(*receiver)->elements() ==
          isolate->heap()->empty_fixed_array()) {
    DCHECK(receiver->IsJSObject());
    DCHECK(!JSObject::cast(*receiver)->HasNamedInterceptor());
    DCHECK(!JSObject::cast(*receiver)->IsAccessCheckNeeded());
    DCHECK(!receiver->map()->has_hidden_prototype());
    DCHECK(JSObject::cast(*receiver)->HasFastProperties());
    if (enum_length == 0) {
      keys = isolate->factory()->empty_fixed_array();
    } else {
      Handle<FixedArray> cache(
          receiver->map()->instance_descriptors()->GetEnumCache());
      keys = isolate->factory()->CopyFixedArrayUpTo(cache, enum_length);
    }
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, keys,
        KeyAccumulator::GetKeys(receiver, KeyCollectionMode::kOwnOnly,
                                ENUMERABLE_STRINGS,
                                GetKeysConversion::kConvertToString));
  }
  return *isolate->factory()->NewJSArrayWithElements(keys, FAST_ELEMENTS);
}

BUILTIN(ObjectValues) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));
  Handle<FixedArray> values;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, values, JSReceiver::GetOwnValues(receiver, ENUMERABLE_STRINGS));
  return *isolate->factory()->NewJSArrayWithElements(values);
}

BUILTIN(ObjectEntries) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));
  Handle<FixedArray> entries;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, entries,
      JSReceiver::GetOwnEntries(receiver, ENUMERABLE_STRINGS));
  return *isolate->factory()->NewJSArrayWithElements(entries);
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
    MAYBE_RETURN(did_get_descriptor, isolate->heap()->exception());

    if (!did_get_descriptor.FromJust()) continue;
    Handle<Object> from_descriptor = descriptor.ToObject(isolate);

    LookupIterator it = LookupIterator::PropertyOrElement(
        isolate, descriptors, key, descriptors, LookupIterator::OWN);
    Maybe<bool> success = JSReceiver::CreateDataProperty(&it, from_descriptor,
                                                         Object::DONT_THROW);
    CHECK(success.FromJust());
  }

  return *descriptors;
}

// ES6 section 19.1.2.15 Object.preventExtensions ( O )
BUILTIN(ObjectPreventExtensions) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  if (object->IsJSReceiver()) {
    MAYBE_RETURN(JSReceiver::PreventExtensions(Handle<JSReceiver>::cast(object),
                                               Object::THROW_ON_ERROR),
                 isolate->heap()->exception());
  }
  return *object;
}

// ES6 section 19.1.2.17 Object.seal ( O )
BUILTIN(ObjectSeal) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  if (object->IsJSReceiver()) {
    MAYBE_RETURN(JSReceiver::SetIntegrityLevel(Handle<JSReceiver>::cast(object),
                                               SEALED, Object::THROW_ON_ERROR),
                 isolate->heap()->exception());
  }
  return *object;
}

void Builtins::Generate_HasProperty(CodeStubAssembler* assembler) {
  typedef HasPropertyDescriptor Descriptor;
  typedef compiler::Node Node;

  Node* key = assembler->Parameter(Descriptor::kKey);
  Node* object = assembler->Parameter(Descriptor::kObject);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->Return(
      assembler->HasProperty(object, key, context, Runtime::kHasProperty));
}

void Builtins::Generate_ForInFilter(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef ForInFilterDescriptor Descriptor;

  Node* key = assembler->Parameter(Descriptor::kKey);
  Node* object = assembler->Parameter(Descriptor::kObject);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->Return(assembler->ForInFilter(key, object, context));
}

void Builtins::Generate_InstanceOf(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef CompareDescriptor Descriptor;
  Node* object = assembler->Parameter(Descriptor::kLeft);
  Node* callable = assembler->Parameter(Descriptor::kRight);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->Return(assembler->InstanceOf(object, callable, context));
}

// ES6 section 7.3.19 OrdinaryHasInstance ( C, O )
void Builtins::Generate_OrdinaryHasInstance(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef CompareDescriptor Descriptor;

  Node* constructor = assembler->Parameter(Descriptor::kLeft);
  Node* object = assembler->Parameter(Descriptor::kRight);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->Return(
      assembler->OrdinaryHasInstance(context, constructor, object));
}

}  // namespace internal
}  // namespace v8
