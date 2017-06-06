// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 19.1 Object Objects

typedef compiler::Node Node;

class ObjectBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ObjectBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void IsString(Node* object, Label* if_string, Label* if_notstring);
  void ReturnToStringFormat(Node* context, Node* string);
};

void ObjectBuiltinsAssembler::IsString(Node* object, Label* if_string,
                                       Label* if_notstring) {
  Label if_notsmi(this);
  Branch(TaggedIsSmi(object), if_notstring, &if_notsmi);

  BIND(&if_notsmi);
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

TF_BUILTIN(ObjectHasOwnProperty, ObjectBuiltinsAssembler) {
  Node* object = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kKey);
  Node* context = Parameter(Descriptor::kContext);

  Label call_runtime(this), return_true(this), return_false(this);

  // Smi receivers do not have own properties.
  Label if_objectisnotsmi(this);
  Branch(TaggedIsSmi(object), &return_false, &if_objectisnotsmi);
  BIND(&if_objectisnotsmi);

  Node* map = LoadMap(object);
  Node* instance_type = LoadMapInstanceType(map);

  {
    VARIABLE(var_index, MachineType::PointerRepresentation());
    VARIABLE(var_unique, MachineRepresentation::kTagged);

    Label keyisindex(this), if_iskeyunique(this);
    TryToName(key, &keyisindex, &var_index, &if_iskeyunique, &var_unique,
              &call_runtime);

    BIND(&if_iskeyunique);
    TryHasOwnProperty(object, map, instance_type, var_unique.value(),
                      &return_true, &return_false, &call_runtime);

    BIND(&keyisindex);
    // Handle negative keys in the runtime.
    GotoIf(IntPtrLessThan(var_index.value(), IntPtrConstant(0)), &call_runtime);
    TryLookupElement(object, map, instance_type, var_index.value(),
                     &return_true, &return_false, &return_false, &call_runtime);
  }
  BIND(&return_true);
  Return(BooleanConstant(true));

  BIND(&return_false);
  Return(BooleanConstant(false));

  BIND(&call_runtime);
  Return(CallRuntime(Runtime::kObjectHasOwnProperty, context, object, key));
}

// ES6 #sec-object.prototype.tostring
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

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* context = Parameter(Descriptor::kContext);

  GotoIf(WordEqual(receiver, UndefinedConstant()), &return_undefined);

  GotoIf(WordEqual(receiver, NullConstant()), &return_null);

  Callable to_object = CodeFactory::ToObject(isolate());
  receiver = CallStub(to_object, context, receiver);

  Node* receiver_instance_type = LoadInstanceType(receiver);

  // for proxies, check IsArray before getting @@toStringTag
  VARIABLE(var_proxy_is_array, MachineRepresentation::kTagged);
  var_proxy_is_array.Bind(BooleanConstant(false));

  Branch(Word32Equal(receiver_instance_type, Int32Constant(JS_PROXY_TYPE)),
         &if_isproxy, &checkstringtag);

  BIND(&if_isproxy);
  {
    // This can throw
    var_proxy_is_array.Bind(
        CallRuntime(Runtime::kArrayIsArray, context, receiver));
    Goto(&checkstringtag);
  }

  BIND(&checkstringtag);
  {
    Node* to_string_tag_symbol =
        HeapConstant(isolate()->factory()->to_string_tag_symbol());

    GetPropertyStub stub(isolate());
    Callable get_property =
        Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
    Node* to_string_tag_value =
        CallStub(get_property, context, receiver, to_string_tag_symbol);

    IsString(to_string_tag_value, &if_tostringtag, &if_notostringtag);

    BIND(&if_tostringtag);
    ReturnToStringFormat(context, to_string_tag_value);
  }
  BIND(&if_notostringtag);
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

    BIND(&return_undefined);
    Return(HeapConstant(isolate()->factory()->undefined_to_string()));

    BIND(&return_null);
    Return(HeapConstant(isolate()->factory()->null_to_string()));

    BIND(&return_arguments);
    Return(HeapConstant(isolate()->factory()->arguments_to_string()));

    BIND(&return_array);
    Return(HeapConstant(isolate()->factory()->array_to_string()));

    BIND(&return_function);
    Return(HeapConstant(isolate()->factory()->function_to_string()));

    BIND(&return_error);
    Return(HeapConstant(isolate()->factory()->error_to_string()));

    BIND(&return_date);
    Return(HeapConstant(isolate()->factory()->date_to_string()));

    BIND(&return_regexp);
    Return(HeapConstant(isolate()->factory()->regexp_to_string()));

    BIND(&return_api);
    {
      Node* class_name = CallRuntime(Runtime::kClassOf, context, receiver);
      ReturnToStringFormat(context, class_name);
    }

    BIND(&return_jsvalue);
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

      BIND(&return_string);
      Return(HeapConstant(isolate()->factory()->string_to_string()));

      BIND(&return_number);
      Return(HeapConstant(isolate()->factory()->number_to_string()));

      BIND(&return_boolean);
      Return(HeapConstant(isolate()->factory()->boolean_to_string()));
    }

    BIND(&return_jsproxy);
    {
      GotoIf(WordEqual(var_proxy_is_array.value(), BooleanConstant(true)),
             &return_array);

      Node* map = LoadMap(receiver);

      // Return object if the proxy {receiver} is not callable.
      Branch(IsCallableMap(map), &return_function, &return_object);
    }

    // Default
    BIND(&return_object);
    Return(HeapConstant(isolate()->factory()->object_to_string()));
  }
}

// ES6 #sec-object.prototype.valueof
TF_BUILTIN(ObjectPrototypeValueOf, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* context = Parameter(Descriptor::kContext);

  Callable to_object = CodeFactory::ToObject(isolate());
  receiver = CallStub(to_object, context, receiver);

  Return(receiver);
}

// ES #sec-object.create
TF_BUILTIN(ObjectCreate, ObjectBuiltinsAssembler) {
  Node* prototype = Parameter(Descriptor::kPrototype);
  Node* properties = Parameter(Descriptor::kProperties);
  Node* context = Parameter(Descriptor::kContext);

  Label call_runtime(this, Label::kDeferred), prototype_valid(this),
      no_properties(this);
  {
    Comment("Argument 1 check: prototype");
    GotoIf(WordEqual(prototype, NullConstant()), &prototype_valid);
    BranchIfJSReceiver(prototype, &prototype_valid, &call_runtime);
  }

  BIND(&prototype_valid);
  {
    Comment("Argument 2 check: properties");
    // Check that we have a simple object
    GotoIf(TaggedIsSmi(properties), &call_runtime);
    // Undefined implies no properties.
    GotoIf(WordEqual(properties, UndefinedConstant()), &no_properties);
    Node* properties_map = LoadMap(properties);
    GotoIf(IsSpecialReceiverMap(properties_map), &call_runtime);
    // Stay on the fast path only if there are no elements.
    GotoIfNot(WordEqual(LoadElements(properties),
                        LoadRoot(Heap::kEmptyFixedArrayRootIndex)),
              &call_runtime);
    // Handle dictionary objects or fast objects with properties in runtime.
    Node* bit_field3 = LoadMapBitField3(properties_map);
    GotoIf(IsSetWord32<Map::DictionaryMap>(bit_field3), &call_runtime);
    Branch(IsSetWord32<Map::NumberOfOwnDescriptorsBits>(bit_field3),
           &call_runtime, &no_properties);
  }

  // Create a new object with the given prototype.
  BIND(&no_properties);
  {
    VARIABLE(map, MachineRepresentation::kTagged);
    VARIABLE(properties, MachineRepresentation::kTagged);
    Label non_null_proto(this), instantiate_map(this), good(this);

    Branch(WordEqual(prototype, NullConstant()), &good, &non_null_proto);

    BIND(&good);
    {
      map.Bind(LoadContextElement(
          context, Context::SLOW_OBJECT_WITH_NULL_PROTOTYPE_MAP));
      properties.Bind(AllocateNameDictionary(NameDictionary::kInitialCapacity));
      Goto(&instantiate_map);
    }

    BIND(&non_null_proto);
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

    BIND(&instantiate_map);
    {
      Node* instance = AllocateJSObjectFromMap(map.value(), properties.value());
      Return(instance);
    }
  }

  BIND(&call_runtime);
  {
    Return(CallRuntime(Runtime::kObjectCreate, context, prototype, properties));
  }
}

TF_BUILTIN(CreateIterResultObject, ObjectBuiltinsAssembler) {
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
  Node* key = Parameter(Descriptor::kKey);
  Node* object = Parameter(Descriptor::kObject);
  Node* context = Parameter(Descriptor::kContext);

  Return(HasProperty(object, key, context, Runtime::kHasProperty));
}

TF_BUILTIN(InstanceOf, ObjectBuiltinsAssembler) {
  Node* object = Parameter(Descriptor::kLeft);
  Node* callable = Parameter(Descriptor::kRight);
  Node* context = Parameter(Descriptor::kContext);

  Return(InstanceOf(object, callable, context));
}

// ES6 section 7.3.19 OrdinaryHasInstance ( C, O )
TF_BUILTIN(OrdinaryHasInstance, ObjectBuiltinsAssembler) {
  Node* constructor = Parameter(Descriptor::kLeft);
  Node* object = Parameter(Descriptor::kRight);
  Node* context = Parameter(Descriptor::kContext);

  Return(OrdinaryHasInstance(context, constructor, object));
}

TF_BUILTIN(GetSuperConstructor, ObjectBuiltinsAssembler) {
  Node* object = Parameter(Descriptor::kObject);
  Node* context = Parameter(Descriptor::kContext);

  Return(GetSuperConstructor(object, context));
}

}  // namespace internal
}  // namespace v8
