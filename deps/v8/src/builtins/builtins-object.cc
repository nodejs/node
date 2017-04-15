// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/property-descriptor.h"

namespace v8 {
namespace internal {

class ObjectBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ObjectBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void IsString(Node* object, Label* if_string, Label* if_notstring);
  void ReturnToStringFormat(Node* context, Node* string);
};

// -----------------------------------------------------------------------------
// ES6 section 19.1 Object Objects

TF_BUILTIN(ObjectHasOwnProperty, ObjectBuiltinsAssembler) {
  Node* object = Parameter(0);
  Node* key = Parameter(1);
  Node* context = Parameter(4);

  Label call_runtime(this), return_true(this), return_false(this);

  // Smi receivers do not have own properties.
  Label if_objectisnotsmi(this);
  Branch(TaggedIsSmi(object), &return_false, &if_objectisnotsmi);
  Bind(&if_objectisnotsmi);

  Node* map = LoadMap(object);
  Node* instance_type = LoadMapInstanceType(map);

  Variable var_index(this, MachineType::PointerRepresentation());

  Label keyisindex(this), if_iskeyunique(this);
  TryToName(key, &keyisindex, &var_index, &if_iskeyunique, &call_runtime);

  Bind(&if_iskeyunique);
  TryHasOwnProperty(object, map, instance_type, key, &return_true,
                    &return_false, &call_runtime);

  Bind(&keyisindex);
  // Handle negative keys in the runtime.
  GotoIf(IntPtrLessThan(var_index.value(), IntPtrConstant(0)), &call_runtime);
  TryLookupElement(object, map, instance_type, var_index.value(), &return_true,
                   &return_false, &call_runtime);

  Bind(&return_true);
  Return(BooleanConstant(true));

  Bind(&return_false);
  Return(BooleanConstant(false));

  Bind(&call_runtime);
  Return(CallRuntime(Runtime::kObjectHasOwnProperty, context, object, key));
}

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
    Handle<Object> next_source = args.at(i);
    MAYBE_RETURN(
        JSReceiver::SetOrCopyDataProperties(isolate, to, next_source, true),
        isolate->heap()->exception());
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

void ObjectBuiltinsAssembler::IsString(Node* object, Label* if_string,
                                       Label* if_notstring) {
  Label if_notsmi(this);
  Branch(TaggedIsSmi(object), if_notstring, &if_notsmi);

  Bind(&if_notsmi);
  {
    Node* instance_type = LoadInstanceType(object);

    Branch(IsStringInstanceType(instance_type), if_string, if_notstring);
  }
}

void ObjectBuiltinsAssembler::ReturnToStringFormat(Node* context,
                                                   Node* string) {
  Node* lhs = HeapConstant(factory()->NewStringFromStaticChars("[object "));
  Node* rhs = HeapConstant(factory()->NewStringFromStaticChars("]"));

  Callable callable =
      CodeFactory::StringAdd(isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED);

  Return(CallStub(callable, context, CallStub(callable, context, lhs, string),
                  rhs));
}

// ES6 section 19.1.3.6 Object.prototype.toString
TF_BUILTIN(ObjectProtoToString, ObjectBuiltinsAssembler) {
  Label return_undefined(this, Label::kDeferred),
      return_null(this, Label::kDeferred),
      return_arguments(this, Label::kDeferred), return_array(this),
      return_api(this, Label::kDeferred), return_object(this),
      return_regexp(this), return_function(this), return_error(this),
      return_date(this), return_jsvalue(this),
      return_jsproxy(this, Label::kDeferred);

  Label if_isproxy(this, Label::kDeferred);

  Label checkstringtag(this);
  Label if_tostringtag(this), if_notostringtag(this);

  Node* receiver = Parameter(0);
  Node* context = Parameter(3);

  GotoIf(WordEqual(receiver, UndefinedConstant()), &return_undefined);

  GotoIf(WordEqual(receiver, NullConstant()), &return_null);

  Callable to_object = CodeFactory::ToObject(isolate());
  receiver = CallStub(to_object, context, receiver);

  Node* receiver_instance_type = LoadInstanceType(receiver);

  // for proxies, check IsArray before getting @@toStringTag
  Variable var_proxy_is_array(this, MachineRepresentation::kTagged);
  var_proxy_is_array.Bind(BooleanConstant(false));

  Branch(Word32Equal(receiver_instance_type, Int32Constant(JS_PROXY_TYPE)),
         &if_isproxy, &checkstringtag);

  Bind(&if_isproxy);
  {
    // This can throw
    var_proxy_is_array.Bind(
        CallRuntime(Runtime::kArrayIsArray, context, receiver));
    Goto(&checkstringtag);
  }

  Bind(&checkstringtag);
  {
    Node* to_string_tag_symbol =
        HeapConstant(isolate()->factory()->to_string_tag_symbol());

    GetPropertyStub stub(isolate());
    Callable get_property =
        Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
    Node* to_string_tag_value =
        CallStub(get_property, context, receiver, to_string_tag_symbol);

    IsString(to_string_tag_value, &if_tostringtag, &if_notostringtag);

    Bind(&if_tostringtag);
    ReturnToStringFormat(context, to_string_tag_value);
  }
  Bind(&if_notostringtag);
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

    Switch(receiver_instance_type, &return_object, case_values, case_labels,
           arraysize(case_values));

    Bind(&return_undefined);
    Return(HeapConstant(isolate()->factory()->undefined_to_string()));

    Bind(&return_null);
    Return(HeapConstant(isolate()->factory()->null_to_string()));

    Bind(&return_arguments);
    Return(HeapConstant(isolate()->factory()->arguments_to_string()));

    Bind(&return_array);
    Return(HeapConstant(isolate()->factory()->array_to_string()));

    Bind(&return_function);
    Return(HeapConstant(isolate()->factory()->function_to_string()));

    Bind(&return_error);
    Return(HeapConstant(isolate()->factory()->error_to_string()));

    Bind(&return_date);
    Return(HeapConstant(isolate()->factory()->date_to_string()));

    Bind(&return_regexp);
    Return(HeapConstant(isolate()->factory()->regexp_to_string()));

    Bind(&return_api);
    {
      Node* class_name = CallRuntime(Runtime::kClassOf, context, receiver);
      ReturnToStringFormat(context, class_name);
    }

    Bind(&return_jsvalue);
    {
      Label return_boolean(this), return_number(this), return_string(this);

      Node* value = LoadJSValueValue(receiver);
      GotoIf(TaggedIsSmi(value), &return_number);
      Node* instance_type = LoadInstanceType(value);

      GotoIf(IsStringInstanceType(instance_type), &return_string);
      GotoIf(Word32Equal(instance_type, Int32Constant(HEAP_NUMBER_TYPE)),
             &return_number);
      GotoIf(Word32Equal(instance_type, Int32Constant(ODDBALL_TYPE)),
             &return_boolean);

      CSA_ASSERT(this, Word32Equal(instance_type, Int32Constant(SYMBOL_TYPE)));
      Goto(&return_object);

      Bind(&return_string);
      Return(HeapConstant(isolate()->factory()->string_to_string()));

      Bind(&return_number);
      Return(HeapConstant(isolate()->factory()->number_to_string()));

      Bind(&return_boolean);
      Return(HeapConstant(isolate()->factory()->boolean_to_string()));
    }

    Bind(&return_jsproxy);
    {
      GotoIf(WordEqual(var_proxy_is_array.value(), BooleanConstant(true)),
             &return_array);

      Node* map = LoadMap(receiver);

      // Return object if the proxy {receiver} is not callable.
      Branch(IsCallableMap(map), &return_function, &return_object);
    }

    // Default
    Bind(&return_object);
    Return(HeapConstant(isolate()->factory()->object_to_string()));
  }
}

TF_BUILTIN(ObjectCreate, ObjectBuiltinsAssembler) {
  Node* prototype = Parameter(1);
  Node* properties = Parameter(2);
  Node* context = Parameter(3 + 2);

  Label call_runtime(this, Label::kDeferred), prototype_valid(this),
      no_properties(this);
  {
    Comment("Argument 1 check: prototype");
    GotoIf(WordEqual(prototype, NullConstant()), &prototype_valid);
    BranchIfJSReceiver(prototype, &prototype_valid, &call_runtime);
  }

  Bind(&prototype_valid);
  {
    Comment("Argument 2 check: properties");
    // Check that we have a simple object
    GotoIf(TaggedIsSmi(properties), &call_runtime);
    // Undefined implies no properties.
    GotoIf(WordEqual(properties, UndefinedConstant()), &no_properties);
    Node* properties_map = LoadMap(properties);
    GotoIf(IsSpecialReceiverMap(properties_map), &call_runtime);
    // Stay on the fast path only if there are no elements.
    GotoUnless(WordEqual(LoadElements(properties),
                         LoadRoot(Heap::kEmptyFixedArrayRootIndex)),
               &call_runtime);
    // Handle dictionary objects or fast objects with properties in runtime.
    Node* bit_field3 = LoadMapBitField3(properties_map);
    GotoIf(IsSetWord32<Map::DictionaryMap>(bit_field3), &call_runtime);
    Branch(IsSetWord32<Map::NumberOfOwnDescriptorsBits>(bit_field3),
           &call_runtime, &no_properties);
  }

  // Create a new object with the given prototype.
  Bind(&no_properties);
  {
    Variable map(this, MachineRepresentation::kTagged);
    Variable properties(this, MachineRepresentation::kTagged);
    Label non_null_proto(this), instantiate_map(this), good(this);

    Branch(WordEqual(prototype, NullConstant()), &good, &non_null_proto);

    Bind(&good);
    {
      map.Bind(LoadContextElement(
          context, Context::SLOW_OBJECT_WITH_NULL_PROTOTYPE_MAP));
      properties.Bind(AllocateNameDictionary(NameDictionary::kInitialCapacity));
      Goto(&instantiate_map);
    }

    Bind(&non_null_proto);
    {
      properties.Bind(EmptyFixedArrayConstant());
      Node* object_function =
          LoadContextElement(context, Context::OBJECT_FUNCTION_INDEX);
      Node* object_function_map = LoadObjectField(
          object_function, JSFunction::kPrototypeOrInitialMapOffset);
      map.Bind(object_function_map);
      GotoIf(WordEqual(prototype, LoadMapPrototype(map.value())),
             &instantiate_map);
      // Try loading the prototype info.
      Node* prototype_info =
          LoadMapPrototypeInfo(LoadMap(prototype), &call_runtime);
      Comment("Load ObjectCreateMap from PrototypeInfo");
      Node* weak_cell =
          LoadObjectField(prototype_info, PrototypeInfo::kObjectCreateMap);
      GotoIf(WordEqual(weak_cell, UndefinedConstant()), &call_runtime);
      map.Bind(LoadWeakCellValue(weak_cell, &call_runtime));
      Goto(&instantiate_map);
    }

    Bind(&instantiate_map);
    {
      Node* instance = AllocateJSObjectFromMap(map.value(), properties.value());
      Return(instance);
    }
  }

  Bind(&call_runtime);
  {
    Return(CallRuntime(Runtime::kObjectCreate, context, prototype, properties));
  }
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

      case LookupIterator::JSPROXY: {
        PropertyDescriptor desc;
        Maybe<bool> found = JSProxy::GetOwnPropertyDescriptor(
            isolate, it.GetHolder<JSProxy>(), it.GetName(), &desc);
        MAYBE_RETURN(found, isolate->heap()->exception());
        if (found.FromJust()) {
          if (component == ACCESSOR_GETTER && desc.has_get()) {
            return *desc.get();
          }
          if (component == ACCESSOR_SETTER && desc.has_set()) {
            return *desc.set();
          }
          return isolate->heap()->undefined_value();
        }
        Handle<Object> prototype;
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
            isolate, prototype, JSProxy::GetPrototype(it.GetHolder<JSProxy>()));
        if (prototype->IsNull(isolate)) {
          return isolate->heap()->undefined_value();
        }
        return ObjectLookupAccessor(isolate, prototype, key, component);
      }

      case LookupIterator::INTEGER_INDEXED_EXOTIC:
      case LookupIterator::DATA:
        return isolate->heap()->undefined_value();

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
  if (object->IsNullOrUndefined(isolate)) {
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
  if (object->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "set Object.prototype.__proto__")));
  }

  // 2. If Type(proto) is neither Object nor Null, return undefined.
  Handle<Object> proto = args.at(1);
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
  Handle<Object> value1 = args.at(1);
  Handle<Object> value2 = args.at(2);
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

TF_BUILTIN(CreateIterResultObject, ObjectBuiltinsAssembler) {
  typedef CreateIterResultObjectDescriptor Descriptor;

  Node* const value = Parameter(Descriptor::kValue);
  Node* const done = Parameter(Descriptor::kDone);
  Node* const context = Parameter(Descriptor::kContext);

  Node* const native_context = LoadNativeContext(context);
  Node* const map =
      LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);

  Node* const result = AllocateJSObjectFromMap(map);

  StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kValueOffset, value);
  StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kDoneOffset, done);

  Return(result);
}

TF_BUILTIN(HasProperty, ObjectBuiltinsAssembler) {
  typedef HasPropertyDescriptor Descriptor;

  Node* key = Parameter(Descriptor::kKey);
  Node* object = Parameter(Descriptor::kObject);
  Node* context = Parameter(Descriptor::kContext);

  Return(HasProperty(object, key, context, Runtime::kHasProperty));
}

TF_BUILTIN(ForInFilter, ObjectBuiltinsAssembler) {
  typedef ForInFilterDescriptor Descriptor;

  Node* key = Parameter(Descriptor::kKey);
  Node* object = Parameter(Descriptor::kObject);
  Node* context = Parameter(Descriptor::kContext);

  Return(ForInFilter(key, object, context));
}

TF_BUILTIN(InstanceOf, ObjectBuiltinsAssembler) {
  typedef CompareDescriptor Descriptor;

  Node* object = Parameter(Descriptor::kLeft);
  Node* callable = Parameter(Descriptor::kRight);
  Node* context = Parameter(Descriptor::kContext);

  Return(InstanceOf(object, callable, context));
}

// ES6 section 7.3.19 OrdinaryHasInstance ( C, O )
TF_BUILTIN(OrdinaryHasInstance, ObjectBuiltinsAssembler) {
  typedef CompareDescriptor Descriptor;

  Node* constructor = Parameter(Descriptor::kLeft);
  Node* object = Parameter(Descriptor::kRight);
  Node* context = Parameter(Descriptor::kContext);

  Return(OrdinaryHasInstance(context, constructor, object));
}

TF_BUILTIN(GetSuperConstructor, ObjectBuiltinsAssembler) {
  typedef TypeofDescriptor Descriptor;

  Node* object = Parameter(Descriptor::kObject);
  Node* context = Parameter(Descriptor::kContext);

  Return(GetSuperConstructor(object, context));
}

}  // namespace internal
}  // namespace v8
