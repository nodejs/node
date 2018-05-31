// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/heap/factory-inl.h"
#include "src/objects/property-descriptor-object.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 19.1 Object Objects

typedef compiler::Node Node;
template <class T>
using TNode = CodeStubAssembler::TNode<T>;

class ObjectBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ObjectBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void ReturnToStringFormat(Node* context, Node* string);
  void AddToDictionaryIf(Node* condition, Node* name_dictionary,
                         Handle<Name> name, Node* value, Label* bailout);
  Node* FromPropertyDescriptor(Node* context, Node* desc);
  Node* FromPropertyDetails(Node* context, Node* raw_value, Node* details,
                            Label* if_bailout);
  Node* ConstructAccessorDescriptor(Node* context, Node* getter, Node* setter,
                                    Node* enumerable, Node* configurable);
  Node* ConstructDataDescriptor(Node* context, Node* value, Node* writable,
                                Node* enumerable, Node* configurable);
  Node* GetAccessorOrUndefined(Node* accessor, Label* if_bailout);

  Node* IsSpecialReceiverMap(SloppyTNode<Map> map);
};

class ObjectEntriesValuesBuiltinsAssembler : public ObjectBuiltinsAssembler {
 public:
  explicit ObjectEntriesValuesBuiltinsAssembler(
      compiler::CodeAssemblerState* state)
      : ObjectBuiltinsAssembler(state) {}

 protected:
  enum CollectType { kEntries, kValues };

  TNode<Word32T> IsStringWrapperElementsKind(TNode<Map> map);

  TNode<BoolT> IsPropertyEnumerable(TNode<Uint32T> details);

  TNode<BoolT> IsPropertyKindAccessor(TNode<Uint32T> kind);

  TNode<BoolT> IsPropertyKindData(TNode<Uint32T> kind);

  TNode<Uint32T> HasHiddenPrototype(TNode<Map> map);

  TNode<Uint32T> LoadPropertyKind(TNode<Uint32T> details) {
    return DecodeWord32<PropertyDetails::KindField>(details);
  }

  void GetOwnValuesOrEntries(TNode<Context> context, TNode<Object> maybe_object,
                             CollectType collect_type);

  void GotoIfMapHasSlowProperties(TNode<Map> map, Label* if_slow);

  TNode<JSArray> FastGetOwnValuesOrEntries(
      TNode<Context> context, TNode<JSObject> object,
      Label* if_call_runtime_with_fast_path, Label* if_no_properties,
      CollectType collect_type);

  TNode<JSArray> FinalizeValuesOrEntriesJSArray(
      TNode<Context> context, TNode<FixedArray> values_or_entries,
      TNode<IntPtrT> size, TNode<Map> array_map, Label* if_empty);
};

void ObjectBuiltinsAssembler::ReturnToStringFormat(Node* context,
                                                   Node* string) {
  Node* lhs = StringConstant("[object ");
  Node* rhs = StringConstant("]");

  Callable callable =
      CodeFactory::StringAdd(isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED);

  Return(CallStub(callable, context, CallStub(callable, context, lhs, string),
                  rhs));
}

Node* ObjectBuiltinsAssembler::ConstructAccessorDescriptor(Node* context,
                                                           Node* getter,
                                                           Node* setter,
                                                           Node* enumerable,
                                                           Node* configurable) {
  Node* native_context = LoadNativeContext(context);
  Node* map = LoadContextElement(
      native_context, Context::ACCESSOR_PROPERTY_DESCRIPTOR_MAP_INDEX);
  Node* js_desc = AllocateJSObjectFromMap(map);

  StoreObjectFieldNoWriteBarrier(
      js_desc, JSAccessorPropertyDescriptor::kGetOffset, getter);
  StoreObjectFieldNoWriteBarrier(
      js_desc, JSAccessorPropertyDescriptor::kSetOffset, setter);
  StoreObjectFieldNoWriteBarrier(
      js_desc, JSAccessorPropertyDescriptor::kEnumerableOffset,
      SelectBooleanConstant(enumerable));
  StoreObjectFieldNoWriteBarrier(
      js_desc, JSAccessorPropertyDescriptor::kConfigurableOffset,
      SelectBooleanConstant(configurable));

  return js_desc;
}

Node* ObjectBuiltinsAssembler::ConstructDataDescriptor(Node* context,
                                                       Node* value,
                                                       Node* writable,
                                                       Node* enumerable,
                                                       Node* configurable) {
  Node* native_context = LoadNativeContext(context);
  Node* map = LoadContextElement(native_context,
                                 Context::DATA_PROPERTY_DESCRIPTOR_MAP_INDEX);
  Node* js_desc = AllocateJSObjectFromMap(map);

  StoreObjectFieldNoWriteBarrier(js_desc,
                                 JSDataPropertyDescriptor::kValueOffset, value);
  StoreObjectFieldNoWriteBarrier(js_desc,
                                 JSDataPropertyDescriptor::kWritableOffset,
                                 SelectBooleanConstant(writable));
  StoreObjectFieldNoWriteBarrier(js_desc,
                                 JSDataPropertyDescriptor::kEnumerableOffset,
                                 SelectBooleanConstant(enumerable));
  StoreObjectFieldNoWriteBarrier(js_desc,
                                 JSDataPropertyDescriptor::kConfigurableOffset,
                                 SelectBooleanConstant(configurable));

  return js_desc;
}

Node* ObjectBuiltinsAssembler::IsSpecialReceiverMap(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  TNode<BoolT> is_special =
      IsSpecialReceiverInstanceType(LoadMapInstanceType(map));
  uint32_t mask =
      Map::HasNamedInterceptorBit::kMask | Map::IsAccessCheckNeededBit::kMask;
  USE(mask);
  // Interceptors or access checks imply special receiver.
  CSA_ASSERT(this,
             SelectConstant<BoolT>(IsSetWord32(LoadMapBitField(map), mask),
                                   is_special, Int32TrueConstant()));
  return is_special;
}

TNode<Word32T>
ObjectEntriesValuesBuiltinsAssembler::IsStringWrapperElementsKind(
    TNode<Map> map) {
  Node* kind = LoadMapElementsKind(map);
  return Word32Or(
      Word32Equal(kind, Int32Constant(FAST_STRING_WRAPPER_ELEMENTS)),
      Word32Equal(kind, Int32Constant(SLOW_STRING_WRAPPER_ELEMENTS)));
}

TNode<BoolT> ObjectEntriesValuesBuiltinsAssembler::IsPropertyEnumerable(
    TNode<Uint32T> details) {
  TNode<Uint32T> attributes =
      DecodeWord32<PropertyDetails::AttributesField>(details);
  return IsNotSetWord32(attributes, PropertyAttributes::DONT_ENUM);
}

TNode<BoolT> ObjectEntriesValuesBuiltinsAssembler::IsPropertyKindAccessor(
    TNode<Uint32T> kind) {
  return Word32Equal(kind, Int32Constant(PropertyKind::kAccessor));
}

TNode<BoolT> ObjectEntriesValuesBuiltinsAssembler::IsPropertyKindData(
    TNode<Uint32T> kind) {
  return Word32Equal(kind, Int32Constant(PropertyKind::kData));
}

TNode<Uint32T> ObjectEntriesValuesBuiltinsAssembler::HasHiddenPrototype(
    TNode<Map> map) {
  TNode<Uint32T> bit_field3 = LoadMapBitField3(map);
  return DecodeWord32<Map::HasHiddenPrototypeBit>(bit_field3);
}

void ObjectEntriesValuesBuiltinsAssembler::GetOwnValuesOrEntries(
    TNode<Context> context, TNode<Object> maybe_object,
    CollectType collect_type) {
  TNode<JSReceiver> receiver = ToObject(context, maybe_object);

  Label if_call_runtime_with_fast_path(this, Label::kDeferred),
      if_call_runtime(this, Label::kDeferred),
      if_no_properties(this, Label::kDeferred);

  TNode<Map> map = LoadMap(receiver);
  GotoIfNot(IsJSObjectMap(map), &if_call_runtime);
  GotoIfMapHasSlowProperties(map, &if_call_runtime);

  TNode<JSObject> object = CAST(receiver);
  TNode<FixedArrayBase> elements = LoadElements(object);
  // If the object has elements, we treat it as slow case.
  // So, we go to runtime call.
  GotoIfNot(IsEmptyFixedArray(elements), &if_call_runtime_with_fast_path);

  TNode<JSArray> result = FastGetOwnValuesOrEntries(
      context, object, &if_call_runtime_with_fast_path, &if_no_properties,
      collect_type);
  Return(result);

  BIND(&if_no_properties);
  {
    Node* native_context = LoadNativeContext(context);
    Node* array_map = LoadJSArrayElementsMap(PACKED_ELEMENTS, native_context);
    Node* empty_array = AllocateJSArray(PACKED_ELEMENTS, array_map,
                                        IntPtrConstant(0), SmiConstant(0));
    Return(empty_array);
  }

  BIND(&if_call_runtime_with_fast_path);
  {
    // In slow case, we simply call runtime.
    if (collect_type == CollectType::kEntries) {
      Return(CallRuntime(Runtime::kObjectEntries, context, object));
    } else {
      DCHECK(collect_type == CollectType::kValues);
      Return(CallRuntime(Runtime::kObjectValues, context, object));
    }
  }

  BIND(&if_call_runtime);
  {
    // In slow case, we simply call runtime.
    if (collect_type == CollectType::kEntries) {
      Return(
          CallRuntime(Runtime::kObjectEntriesSkipFastPath, context, receiver));
    } else {
      DCHECK(collect_type == CollectType::kValues);
      Return(
          CallRuntime(Runtime::kObjectValuesSkipFastPath, context, receiver));
    }
  }
}

void ObjectEntriesValuesBuiltinsAssembler::GotoIfMapHasSlowProperties(
    TNode<Map> map, Label* if_slow) {
  GotoIf(IsStringWrapperElementsKind(map), if_slow);
  GotoIf(IsSpecialReceiverMap(map), if_slow);
  GotoIf(HasHiddenPrototype(map), if_slow);
  GotoIf(IsDictionaryMap(map), if_slow);
}

TNode<JSArray> ObjectEntriesValuesBuiltinsAssembler::FastGetOwnValuesOrEntries(
    TNode<Context> context, TNode<JSObject> object,
    Label* if_call_runtime_with_fast_path, Label* if_no_properties,
    CollectType collect_type) {
  Node* native_context = LoadNativeContext(context);
  TNode<Map> array_map =
      LoadJSArrayElementsMap(PACKED_ELEMENTS, native_context);
  TNode<Map> map = LoadMap(object);
  TNode<Uint32T> bit_field3 = LoadMapBitField3(map);

  Label if_has_enum_cache(this), if_not_has_enum_cache(this),
      collect_entries(this);
  Node* object_enum_length =
      DecodeWordFromWord32<Map::EnumLengthBits>(bit_field3);
  Node* has_enum_cache = WordNotEqual(
      object_enum_length, IntPtrConstant(kInvalidEnumCacheSentinel));

  // In case, we found enum_cache in object,
  // we use it as array_length because it has same size for
  // Object.(entries/values) result array object length.
  // So object_enum_length use less memory space than
  // NumberOfOwnDescriptorsBits value.
  // And in case, if enum_cache_not_found,
  // we call runtime and initialize enum_cache for subsequent call of
  // CSA fast path.
  Branch(has_enum_cache, &if_has_enum_cache, if_call_runtime_with_fast_path);

  BIND(&if_has_enum_cache);
  {
    GotoIf(WordEqual(object_enum_length, IntPtrConstant(0)), if_no_properties);
    TNode<FixedArray> values_or_entries = TNode<FixedArray>::UncheckedCast(
        AllocateFixedArray(PACKED_ELEMENTS, object_enum_length,
                           INTPTR_PARAMETERS, kAllowLargeObjectAllocation));

    // If in case we have enum_cache,
    // we can't detect accessor of object until loop through descriptors.
    // So if object might have accessor,
    // we will remain invalid addresses of FixedArray.
    // Because in that case, we need to jump to runtime call.
    // So the array filled by the-hole even if enum_cache exists.
    FillFixedArrayWithValue(PACKED_ELEMENTS, values_or_entries,
                            IntPtrConstant(0), object_enum_length,
                            Heap::kTheHoleValueRootIndex);

    TVARIABLE(IntPtrT, var_result_index, IntPtrConstant(0));
    TVARIABLE(IntPtrT, var_descriptor_number, IntPtrConstant(0));
    Variable* vars[] = {&var_descriptor_number, &var_result_index};
    // Let desc be ? O.[[GetOwnProperty]](key).
    TNode<DescriptorArray> descriptors = LoadMapDescriptors(map);
    Label loop(this, 2, vars), after_loop(this), next_descriptor(this);
    Branch(IntPtrEqual(var_descriptor_number.value(), object_enum_length),
           &after_loop, &loop);

    // We dont use BuildFastLoop.
    // Instead, we use hand-written loop
    // because of we need to use 'continue' functionality.
    BIND(&loop);
    {
      // Currently, we will not invoke getters,
      // so, map will not be changed.
      CSA_ASSERT(this, WordEqual(map, LoadMap(object)));
      TNode<Uint32T> descriptor_index = TNode<Uint32T>::UncheckedCast(
          TruncateIntPtrToInt32(var_descriptor_number.value()));
      Node* next_key = GetKey(descriptors, descriptor_index);

      // Skip Symbols.
      GotoIf(IsSymbol(next_key), &next_descriptor);

      TNode<Uint32T> details = TNode<Uint32T>::UncheckedCast(
          DescriptorArrayGetDetails(descriptors, descriptor_index));
      TNode<Uint32T> kind = LoadPropertyKind(details);

      // If property is accessor, we escape fast path and call runtime.
      GotoIf(IsPropertyKindAccessor(kind), if_call_runtime_with_fast_path);
      CSA_ASSERT(this, IsPropertyKindData(kind));

      // If desc is not undefined and desc.[[Enumerable]] is true, then skip to
      // the next descriptor.
      GotoIfNot(IsPropertyEnumerable(details), &next_descriptor);

      VARIABLE(var_property_value, MachineRepresentation::kTagged,
               UndefinedConstant());
      TNode<IntPtrT> descriptor_name_index = ToKeyIndex<DescriptorArray>(
          Unsigned(TruncateIntPtrToInt32(var_descriptor_number.value())));

      // Let value be ? Get(O, key).
      LoadPropertyFromFastObject(object, map, descriptors,
                                 descriptor_name_index, details,
                                 &var_property_value);

      // If kind is "value", append value to properties.
      Node* value = var_property_value.value();

      if (collect_type == CollectType::kEntries) {
        // Let entry be CreateArrayFromList(« key, value »).
        Node* array = nullptr;
        Node* elements = nullptr;
        std::tie(array, elements) = AllocateUninitializedJSArrayWithElements(
            PACKED_ELEMENTS, array_map, SmiConstant(2), nullptr,
            IntPtrConstant(2));
        StoreFixedArrayElement(elements, 0, next_key, SKIP_WRITE_BARRIER);
        StoreFixedArrayElement(elements, 1, value, SKIP_WRITE_BARRIER);
        value = array;
      }

      StoreFixedArrayElement(values_or_entries, var_result_index.value(),
                             value);
      Increment(&var_result_index, 1);
      Goto(&next_descriptor);

      BIND(&next_descriptor);
      {
        Increment(&var_descriptor_number, 1);
        Branch(IntPtrEqual(var_result_index.value(), object_enum_length),
               &after_loop, &loop);
      }
    }
    BIND(&after_loop);
    return FinalizeValuesOrEntriesJSArray(context, values_or_entries,
                                          var_result_index.value(), array_map,
                                          if_no_properties);
  }
}

TNode<JSArray>
ObjectEntriesValuesBuiltinsAssembler::FinalizeValuesOrEntriesJSArray(
    TNode<Context> context, TNode<FixedArray> result, TNode<IntPtrT> size,
    TNode<Map> array_map, Label* if_empty) {
  CSA_ASSERT(this, IsJSArrayMap(array_map));

  GotoIf(IntPtrEqual(size, IntPtrConstant(0)), if_empty);
  Node* array = AllocateUninitializedJSArrayWithoutElements(
      array_map, SmiTag(size), nullptr);
  StoreObjectField(array, JSArray::kElementsOffset, result);
  return TNode<JSArray>::UncheckedCast(array);
}

TF_BUILTIN(ObjectPrototypeToLocaleString, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));

  Label if_null_or_undefined(this, Label::kDeferred);
  GotoIf(IsNullOrUndefined(receiver), &if_null_or_undefined);

  TNode<Object> method =
      GetProperty(context, receiver, factory()->toString_string());
  Return(CallJS(CodeFactory::Call(isolate()), context, method, receiver));

  BIND(&if_null_or_undefined);
  ThrowTypeError(context, MessageTemplate::kCalledOnNullOrUndefined,
                 "Object.prototype.toLocaleString");
}

TF_BUILTIN(ObjectPrototypeHasOwnProperty, ObjectBuiltinsAssembler) {
  Node* object = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kKey);
  Node* context = Parameter(Descriptor::kContext);

  Label call_runtime(this), return_true(this), return_false(this),
      to_primitive(this);

  // Smi receivers do not have own properties, just perform ToPrimitive on the
  // key.
  Label if_objectisnotsmi(this);
  Branch(TaggedIsSmi(object), &to_primitive, &if_objectisnotsmi);
  BIND(&if_objectisnotsmi);

  Node* map = LoadMap(object);
  Node* instance_type = LoadMapInstanceType(map);

  {
    VARIABLE(var_index, MachineType::PointerRepresentation());
    VARIABLE(var_unique, MachineRepresentation::kTagged);

    Label if_index(this), if_unique_name(this), if_notunique_name(this);
    TryToName(key, &if_index, &var_index, &if_unique_name, &var_unique,
              &call_runtime, &if_notunique_name);

    BIND(&if_unique_name);
    TryHasOwnProperty(object, map, instance_type, var_unique.value(),
                      &return_true, &return_false, &call_runtime);

    BIND(&if_index);
    {
      // Handle negative keys in the runtime.
      GotoIf(IntPtrLessThan(var_index.value(), IntPtrConstant(0)),
             &call_runtime);
      TryLookupElement(object, map, instance_type, var_index.value(),
                       &return_true, &return_false, &return_false,
                       &call_runtime);
    }

    BIND(&if_notunique_name);
    {
      Label not_in_string_table(this);
      TryInternalizeString(key, &if_index, &var_index, &if_unique_name,
                           &var_unique, &not_in_string_table, &call_runtime);

      BIND(&not_in_string_table);
      {
        // If the string was not found in the string table, then no regular
        // object can have a property with that name, so return |false|.
        // "Special API objects" with interceptors must take the slow path.
        Branch(IsSpecialReceiverInstanceType(instance_type), &call_runtime,
               &return_false);
      }
    }
  }
  BIND(&to_primitive);
  GotoIf(IsNumber(key), &return_false);
  Branch(IsName(key), &return_false, &call_runtime);

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());

  BIND(&call_runtime);
  Return(CallRuntime(Runtime::kObjectHasOwnProperty, context, object, key));
}

// ES #sec-object.keys
TF_BUILTIN(ObjectKeys, ObjectBuiltinsAssembler) {
  Node* object = Parameter(Descriptor::kObject);
  Node* context = Parameter(Descriptor::kContext);

  VARIABLE(var_length, MachineRepresentation::kTagged);
  VARIABLE(var_elements, MachineRepresentation::kTagged);
  Label if_empty(this, Label::kDeferred), if_empty_elements(this),
      if_fast(this), if_slow(this, Label::kDeferred), if_join(this);

  // Check if the {object} has a usable enum cache.
  GotoIf(TaggedIsSmi(object), &if_slow);
  Node* object_map = LoadMap(object);
  Node* object_bit_field3 = LoadMapBitField3(object_map);
  Node* object_enum_length =
      DecodeWordFromWord32<Map::EnumLengthBits>(object_bit_field3);
  GotoIf(
      WordEqual(object_enum_length, IntPtrConstant(kInvalidEnumCacheSentinel)),
      &if_slow);

  // Ensure that the {object} doesn't have any elements.
  CSA_ASSERT(this, IsJSObjectMap(object_map));
  Node* object_elements = LoadElements(object);
  GotoIf(IsEmptyFixedArray(object_elements), &if_empty_elements);
  Branch(IsEmptySlowElementDictionary(object_elements), &if_empty_elements,
         &if_slow);

  // Check whether there are enumerable properties.
  BIND(&if_empty_elements);
  Branch(WordEqual(object_enum_length, IntPtrConstant(0)), &if_empty, &if_fast);

  BIND(&if_fast);
  {
    // The {object} has a usable enum cache, use that.
    Node* object_descriptors = LoadMapDescriptors(object_map);
    Node* object_enum_cache =
        LoadObjectField(object_descriptors, DescriptorArray::kEnumCacheOffset);
    Node* object_enum_keys =
        LoadObjectField(object_enum_cache, EnumCache::kKeysOffset);

    // Allocate a JSArray and copy the elements from the {object_enum_keys}.
    Node* array = nullptr;
    Node* elements = nullptr;
    Node* native_context = LoadNativeContext(context);
    Node* array_map = LoadJSArrayElementsMap(PACKED_ELEMENTS, native_context);
    Node* array_length = SmiTag(object_enum_length);
    std::tie(array, elements) = AllocateUninitializedJSArrayWithElements(
        PACKED_ELEMENTS, array_map, array_length, nullptr, object_enum_length,
        INTPTR_PARAMETERS);
    CopyFixedArrayElements(PACKED_ELEMENTS, object_enum_keys, elements,
                           object_enum_length, SKIP_WRITE_BARRIER);
    Return(array);
  }

  BIND(&if_empty);
  {
    // The {object} doesn't have any enumerable keys.
    var_length.Bind(SmiConstant(0));
    var_elements.Bind(EmptyFixedArrayConstant());
    Goto(&if_join);
  }

  BIND(&if_slow);
  {
    // Let the runtime compute the elements.
    Node* elements = CallRuntime(Runtime::kObjectKeys, context, object);
    var_length.Bind(LoadObjectField(elements, FixedArray::kLengthOffset));
    var_elements.Bind(elements);
    Goto(&if_join);
  }

  BIND(&if_join);
  {
    // Wrap the elements into a proper JSArray and return that.
    Node* native_context = LoadNativeContext(context);
    Node* array_map = LoadJSArrayElementsMap(PACKED_ELEMENTS, native_context);
    Node* array = AllocateUninitializedJSArrayWithoutElements(
        array_map, var_length.value(), nullptr);
    StoreObjectFieldNoWriteBarrier(array, JSArray::kElementsOffset,
                                   var_elements.value());
    Return(array);
  }
}

TF_BUILTIN(ObjectValues, ObjectEntriesValuesBuiltinsAssembler) {
  TNode<JSObject> object =
      TNode<JSObject>::UncheckedCast(Parameter(Descriptor::kObject));
  TNode<Context> context =
      TNode<Context>::UncheckedCast(Parameter(Descriptor::kContext));
  GetOwnValuesOrEntries(context, object, CollectType::kValues);
}

TF_BUILTIN(ObjectEntries, ObjectEntriesValuesBuiltinsAssembler) {
  TNode<JSObject> object =
      TNode<JSObject>::UncheckedCast(Parameter(Descriptor::kObject));
  TNode<Context> context =
      TNode<Context>::UncheckedCast(Parameter(Descriptor::kContext));
  GetOwnValuesOrEntries(context, object, CollectType::kEntries);
}

// ES #sec-object.prototype.isprototypeof
TF_BUILTIN(ObjectPrototypeIsPrototypeOf, ObjectBuiltinsAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);
  Label if_receiverisnullorundefined(this, Label::kDeferred),
      if_valueisnotreceiver(this, Label::kDeferred);

  // We only check whether {value} is a Smi here, so that the
  // prototype chain walk below can safely access the {value}s
  // map. We don't rule out Primitive {value}s, since all of
  // them have null as their prototype, so the chain walk below
  // immediately aborts and returns false anyways.
  GotoIf(TaggedIsSmi(value), &if_valueisnotreceiver);

  // Check if {receiver} is either null or undefined and in that case,
  // invoke the ToObject builtin, which raises the appropriate error.
  // Otherwise we don't need to invoke ToObject, since {receiver} is
  // either already a JSReceiver, in which case ToObject is a no-op,
  // or it's a Primitive and ToObject would allocate a fresh JSValue
  // wrapper, which wouldn't be identical to any existing JSReceiver
  // found in the prototype chain of {value}, hence it will return
  // false no matter if we search for the Primitive {receiver} or
  // a newly allocated JSValue wrapper for {receiver}.
  GotoIf(IsNull(receiver), &if_receiverisnullorundefined);
  GotoIf(IsUndefined(receiver), &if_receiverisnullorundefined);

  // Loop through the prototype chain looking for the {receiver}.
  Return(HasInPrototypeChain(context, value, receiver));

  BIND(&if_receiverisnullorundefined);
  {
    // If {value} is a primitive HeapObject, we need to return
    // false instead of throwing an exception per order of the
    // steps in the specification, so check that first here.
    GotoIfNot(IsJSReceiver(value), &if_valueisnotreceiver);

    // Simulate the ToObject invocation on {receiver}.
    ToObject(context, receiver);
    Unreachable();
  }

  BIND(&if_valueisnotreceiver);
  Return(FalseConstant());
}

// ES #sec-object.prototype.tostring
TF_BUILTIN(ObjectPrototypeToString, ObjectBuiltinsAssembler) {
  Label checkstringtag(this), if_apiobject(this, Label::kDeferred),
      if_arguments(this), if_array(this), if_boolean(this), if_date(this),
      if_error(this), if_function(this), if_number(this, Label::kDeferred),
      if_object(this), if_primitive(this), if_proxy(this, Label::kDeferred),
      if_regexp(this), if_string(this), if_symbol(this, Label::kDeferred),
      if_value(this), if_bigint(this, Label::kDeferred);

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* context = Parameter(Descriptor::kContext);

  // This is arranged to check the likely cases first.
  VARIABLE(var_default, MachineRepresentation::kTagged);
  VARIABLE(var_holder, MachineRepresentation::kTagged, receiver);
  GotoIf(TaggedIsSmi(receiver), &if_number);
  Node* receiver_map = LoadMap(receiver);
  Node* receiver_instance_type = LoadMapInstanceType(receiver_map);
  GotoIf(IsPrimitiveInstanceType(receiver_instance_type), &if_primitive);
  const struct {
    InstanceType value;
    Label* label;
  } kJumpTable[] = {{JS_OBJECT_TYPE, &if_object},
                    {JS_ARRAY_TYPE, &if_array},
                    {JS_FUNCTION_TYPE, &if_function},
                    {JS_REGEXP_TYPE, &if_regexp},
                    {JS_ARGUMENTS_TYPE, &if_arguments},
                    {JS_DATE_TYPE, &if_date},
                    {JS_BOUND_FUNCTION_TYPE, &if_function},
                    {JS_API_OBJECT_TYPE, &if_apiobject},
                    {JS_SPECIAL_API_OBJECT_TYPE, &if_apiobject},
                    {JS_PROXY_TYPE, &if_proxy},
                    {JS_ERROR_TYPE, &if_error},
                    {JS_VALUE_TYPE, &if_value}};
  size_t const kNumCases = arraysize(kJumpTable);
  Label* case_labels[kNumCases];
  int32_t case_values[kNumCases];
  for (size_t i = 0; i < kNumCases; ++i) {
    case_labels[i] = kJumpTable[i].label;
    case_values[i] = kJumpTable[i].value;
  }
  Switch(receiver_instance_type, &if_object, case_values, case_labels,
         arraysize(case_values));

  BIND(&if_apiobject);
  {
    // Lookup the @@toStringTag property on the {receiver}.
    VARIABLE(var_tag, MachineRepresentation::kTagged,
             GetProperty(context, receiver,
                         isolate()->factory()->to_string_tag_symbol()));
    Label if_tagisnotstring(this), if_tagisstring(this);
    GotoIf(TaggedIsSmi(var_tag.value()), &if_tagisnotstring);
    Branch(IsString(var_tag.value()), &if_tagisstring, &if_tagisnotstring);
    BIND(&if_tagisnotstring);
    {
      var_tag.Bind(CallRuntime(Runtime::kClassOf, context, receiver));
      Goto(&if_tagisstring);
    }
    BIND(&if_tagisstring);
    ReturnToStringFormat(context, var_tag.value());
  }

  BIND(&if_arguments);
  {
    var_default.Bind(LoadRoot(Heap::karguments_to_stringRootIndex));
    Goto(&checkstringtag);
  }

  BIND(&if_array);
  {
    var_default.Bind(LoadRoot(Heap::karray_to_stringRootIndex));
    Goto(&checkstringtag);
  }

  BIND(&if_boolean);
  {
    Node* native_context = LoadNativeContext(context);
    Node* boolean_constructor =
        LoadContextElement(native_context, Context::BOOLEAN_FUNCTION_INDEX);
    Node* boolean_initial_map = LoadObjectField(
        boolean_constructor, JSFunction::kPrototypeOrInitialMapOffset);
    Node* boolean_prototype =
        LoadObjectField(boolean_initial_map, Map::kPrototypeOffset);
    var_default.Bind(LoadRoot(Heap::kboolean_to_stringRootIndex));
    var_holder.Bind(boolean_prototype);
    Goto(&checkstringtag);
  }

  BIND(&if_date);
  {
    var_default.Bind(LoadRoot(Heap::kdate_to_stringRootIndex));
    Goto(&checkstringtag);
  }

  BIND(&if_error);
  {
    var_default.Bind(LoadRoot(Heap::kerror_to_stringRootIndex));
    Goto(&checkstringtag);
  }

  BIND(&if_function);
  {
    var_default.Bind(LoadRoot(Heap::kfunction_to_stringRootIndex));
    Goto(&checkstringtag);
  }

  BIND(&if_number);
  {
    Node* native_context = LoadNativeContext(context);
    Node* number_constructor =
        LoadContextElement(native_context, Context::NUMBER_FUNCTION_INDEX);
    Node* number_initial_map = LoadObjectField(
        number_constructor, JSFunction::kPrototypeOrInitialMapOffset);
    Node* number_prototype =
        LoadObjectField(number_initial_map, Map::kPrototypeOffset);
    var_default.Bind(LoadRoot(Heap::knumber_to_stringRootIndex));
    var_holder.Bind(number_prototype);
    Goto(&checkstringtag);
  }

  BIND(&if_object);
  {
    CSA_ASSERT(this, IsJSReceiver(receiver));
    var_default.Bind(LoadRoot(Heap::kobject_to_stringRootIndex));
    Goto(&checkstringtag);
  }

  BIND(&if_primitive);
  {
    Label return_undefined(this);

    GotoIf(IsStringInstanceType(receiver_instance_type), &if_string);
    GotoIf(IsBigIntInstanceType(receiver_instance_type), &if_bigint);
    GotoIf(IsBooleanMap(receiver_map), &if_boolean);
    GotoIf(IsHeapNumberMap(receiver_map), &if_number);
    GotoIf(IsSymbolMap(receiver_map), &if_symbol);
    GotoIf(IsUndefined(receiver), &return_undefined);
    CSA_ASSERT(this, IsNull(receiver));
    Return(LoadRoot(Heap::knull_to_stringRootIndex));

    BIND(&return_undefined);
    Return(LoadRoot(Heap::kundefined_to_stringRootIndex));
  }

  BIND(&if_proxy);
  {
    // If {receiver} is a proxy for a JSArray, we default to "[object Array]",
    // otherwise we default to "[object Object]" or "[object Function]" here,
    // depending on whether the {receiver} is callable. The order matters here,
    // i.e. we need to execute the %ArrayIsArray check before the [[Get]] below,
    // as the exception is observable.
    Node* receiver_is_array =
        CallRuntime(Runtime::kArrayIsArray, context, receiver);
    TNode<String> builtin_tag = Select<String>(
        IsTrue(receiver_is_array),
        [=] { return CAST(LoadRoot(Heap::kArray_stringRootIndex)); },
        [=] {
          return Select<String>(
              IsCallableMap(receiver_map),
              [=] { return CAST(LoadRoot(Heap::kFunction_stringRootIndex)); },
              [=] { return CAST(LoadRoot(Heap::kObject_stringRootIndex)); });
        });

    // Lookup the @@toStringTag property on the {receiver}.
    VARIABLE(var_tag, MachineRepresentation::kTagged,
             GetProperty(context, receiver,
                         isolate()->factory()->to_string_tag_symbol()));
    Label if_tagisnotstring(this), if_tagisstring(this);
    GotoIf(TaggedIsSmi(var_tag.value()), &if_tagisnotstring);
    Branch(IsString(var_tag.value()), &if_tagisstring, &if_tagisnotstring);
    BIND(&if_tagisnotstring);
    {
      var_tag.Bind(builtin_tag);
      Goto(&if_tagisstring);
    }
    BIND(&if_tagisstring);
    ReturnToStringFormat(context, var_tag.value());
  }

  BIND(&if_regexp);
  {
    var_default.Bind(LoadRoot(Heap::kregexp_to_stringRootIndex));
    Goto(&checkstringtag);
  }

  BIND(&if_string);
  {
    Node* native_context = LoadNativeContext(context);
    Node* string_constructor =
        LoadContextElement(native_context, Context::STRING_FUNCTION_INDEX);
    Node* string_initial_map = LoadObjectField(
        string_constructor, JSFunction::kPrototypeOrInitialMapOffset);
    Node* string_prototype =
        LoadObjectField(string_initial_map, Map::kPrototypeOffset);
    var_default.Bind(LoadRoot(Heap::kstring_to_stringRootIndex));
    var_holder.Bind(string_prototype);
    Goto(&checkstringtag);
  }

  BIND(&if_symbol);
  {
    Node* native_context = LoadNativeContext(context);
    Node* symbol_constructor =
        LoadContextElement(native_context, Context::SYMBOL_FUNCTION_INDEX);
    Node* symbol_initial_map = LoadObjectField(
        symbol_constructor, JSFunction::kPrototypeOrInitialMapOffset);
    Node* symbol_prototype =
        LoadObjectField(symbol_initial_map, Map::kPrototypeOffset);
    var_default.Bind(LoadRoot(Heap::kobject_to_stringRootIndex));
    var_holder.Bind(symbol_prototype);
    Goto(&checkstringtag);
  }

  BIND(&if_bigint);
  {
    Node* native_context = LoadNativeContext(context);
    Node* bigint_constructor =
        LoadContextElement(native_context, Context::BIGINT_FUNCTION_INDEX);
    Node* bigint_initial_map = LoadObjectField(
        bigint_constructor, JSFunction::kPrototypeOrInitialMapOffset);
    Node* bigint_prototype =
        LoadObjectField(bigint_initial_map, Map::kPrototypeOffset);
    var_default.Bind(LoadRoot(Heap::kobject_to_stringRootIndex));
    var_holder.Bind(bigint_prototype);
    Goto(&checkstringtag);
  }

  BIND(&if_value);
  {
    Node* receiver_value = LoadJSValueValue(receiver);
    GotoIf(TaggedIsSmi(receiver_value), &if_number);
    Node* receiver_value_map = LoadMap(receiver_value);
    GotoIf(IsHeapNumberMap(receiver_value_map), &if_number);
    GotoIf(IsBooleanMap(receiver_value_map), &if_boolean);
    GotoIf(IsSymbolMap(receiver_value_map), &if_symbol);
    Node* receiver_value_instance_type =
        LoadMapInstanceType(receiver_value_map);
    GotoIf(IsBigIntInstanceType(receiver_value_instance_type), &if_bigint);
    CSA_ASSERT(this, IsStringInstanceType(receiver_value_instance_type));
    Goto(&if_string);
  }

  BIND(&checkstringtag);
  {
    // Check if all relevant maps (including the prototype maps) don't
    // have any interesting symbols (i.e. that none of them have the
    // @@toStringTag property).
    Label loop(this, &var_holder), return_default(this),
        return_generic(this, Label::kDeferred);
    Goto(&loop);
    BIND(&loop);
    {
      Node* holder = var_holder.value();
      GotoIf(IsNull(holder), &return_default);
      Node* holder_map = LoadMap(holder);
      Node* holder_bit_field3 = LoadMapBitField3(holder_map);
      GotoIf(IsSetWord32<Map::MayHaveInterestingSymbolsBit>(holder_bit_field3),
             &return_generic);
      var_holder.Bind(LoadMapPrototype(holder_map));
      Goto(&loop);
    }

    BIND(&return_generic);
    {
      Node* tag = GetProperty(context, ToObject(context, receiver),
                              LoadRoot(Heap::kto_string_tag_symbolRootIndex));
      GotoIf(TaggedIsSmi(tag), &return_default);
      GotoIfNot(IsString(tag), &return_default);
      ReturnToStringFormat(context, tag);
    }

    BIND(&return_default);
    Return(var_default.value());
  }
}

// ES6 #sec-object.prototype.valueof
TF_BUILTIN(ObjectPrototypeValueOf, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* context = Parameter(Descriptor::kContext);

  Return(ToObject(context, receiver));
}

// ES #sec-object.create
TF_BUILTIN(ObjectCreate, ObjectBuiltinsAssembler) {
  int const kPrototypeArg = 0;
  int const kPropertiesArg = 1;

  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);

  Node* prototype = args.GetOptionalArgumentValue(kPrototypeArg);
  Node* properties = args.GetOptionalArgumentValue(kPropertiesArg);
  Node* context = Parameter(BuiltinDescriptor::kContext);

  Label call_runtime(this, Label::kDeferred), prototype_valid(this),
      no_properties(this);
  {
    Comment("Argument 1 check: prototype");
    GotoIf(IsNull(prototype), &prototype_valid);
    BranchIfJSReceiver(prototype, &prototype_valid, &call_runtime);
  }

  BIND(&prototype_valid);
  {
    Comment("Argument 2 check: properties");
    // Check that we have a simple object
    GotoIf(TaggedIsSmi(properties), &call_runtime);
    // Undefined implies no properties.
    GotoIf(IsUndefined(properties), &no_properties);
    Node* properties_map = LoadMap(properties);
    GotoIf(IsSpecialReceiverMap(properties_map), &call_runtime);
    // Stay on the fast path only if there are no elements.
    GotoIfNot(WordEqual(LoadElements(properties),
                        LoadRoot(Heap::kEmptyFixedArrayRootIndex)),
              &call_runtime);
    // Handle dictionary objects or fast objects with properties in runtime.
    Node* bit_field3 = LoadMapBitField3(properties_map);
    GotoIf(IsSetWord32<Map::IsDictionaryMapBit>(bit_field3), &call_runtime);
    Branch(IsSetWord32<Map::NumberOfOwnDescriptorsBits>(bit_field3),
           &call_runtime, &no_properties);
  }

  // Create a new object with the given prototype.
  BIND(&no_properties);
  {
    VARIABLE(map, MachineRepresentation::kTagged);
    VARIABLE(properties, MachineRepresentation::kTagged);
    Label non_null_proto(this), instantiate_map(this), good(this);

    Branch(IsNull(prototype), &good, &non_null_proto);

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
      GotoIf(IsUndefined(weak_cell), &call_runtime);
      map.Bind(LoadWeakCellValue(weak_cell, &call_runtime));
      Goto(&instantiate_map);
    }

    BIND(&instantiate_map);
    {
      Node* instance = AllocateJSObjectFromMap(map.value(), properties.value());
      args.PopAndReturn(instance);
    }
  }

  BIND(&call_runtime);
  {
    Node* result =
        CallRuntime(Runtime::kObjectCreate, context, prototype, properties);
    args.PopAndReturn(result);
  }
}

// ES #sec-object.is
TF_BUILTIN(ObjectIs, ObjectBuiltinsAssembler) {
  Node* const left = Parameter(Descriptor::kLeft);
  Node* const right = Parameter(Descriptor::kRight);

  Label return_true(this), return_false(this);
  BranchIfSameValue(left, right, &return_true, &return_false);

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
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

  Return(HasProperty(object, key, context, kHasProperty));
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

  Return(GetSuperConstructor(context, object));
}

TF_BUILTIN(CreateGeneratorObject, ObjectBuiltinsAssembler) {
  Node* closure = Parameter(Descriptor::kClosure);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* context = Parameter(Descriptor::kContext);

  // Get the initial map from the function, jumping to the runtime if we don't
  // have one.
  Label done(this), runtime(this);
  GotoIfNot(IsFunctionWithPrototypeSlotMap(LoadMap(closure)), &runtime);
  Node* maybe_map =
      LoadObjectField(closure, JSFunction::kPrototypeOrInitialMapOffset);
  GotoIf(DoesntHaveInstanceType(maybe_map, MAP_TYPE), &runtime);

  Node* shared =
      LoadObjectField(closure, JSFunction::kSharedFunctionInfoOffset);
  Node* bytecode_array = LoadSharedFunctionInfoBytecodeArray(shared);

  Node* frame_size = ChangeInt32ToIntPtr(LoadObjectField(
      bytecode_array, BytecodeArray::kFrameSizeOffset, MachineType::Int32()));
  Node* size = WordSar(frame_size, IntPtrConstant(kPointerSizeLog2));
  Node* register_file = AllocateFixedArray(HOLEY_ELEMENTS, size);
  FillFixedArrayWithValue(HOLEY_ELEMENTS, register_file, IntPtrConstant(0),
                          size, Heap::kUndefinedValueRootIndex);
  // TODO(cbruni): support start_offset to avoid double initialization.
  Node* result = AllocateJSObjectFromMap(maybe_map, nullptr, nullptr, kNone,
                                         kWithSlackTracking);
  StoreObjectFieldNoWriteBarrier(result, JSGeneratorObject::kFunctionOffset,
                                 closure);
  StoreObjectFieldNoWriteBarrier(result, JSGeneratorObject::kContextOffset,
                                 context);
  StoreObjectFieldNoWriteBarrier(result, JSGeneratorObject::kReceiverOffset,
                                 receiver);
  StoreObjectFieldNoWriteBarrier(result, JSGeneratorObject::kRegisterFileOffset,
                                 register_file);
  Node* executing = SmiConstant(JSGeneratorObject::kGeneratorExecuting);
  StoreObjectFieldNoWriteBarrier(result, JSGeneratorObject::kContinuationOffset,
                                 executing);
  GotoIfNot(HasInstanceType(maybe_map, JS_ASYNC_GENERATOR_OBJECT_TYPE), &done);
  StoreObjectFieldNoWriteBarrier(
      result, JSAsyncGeneratorObject::kIsAwaitingOffset, SmiConstant(0));
  Goto(&done);

  BIND(&done);
  { Return(result); }

  BIND(&runtime);
  {
    Return(CallRuntime(Runtime::kCreateJSGeneratorObject, context, closure,
                       receiver));
  }
}

// ES6 section 19.1.2.7 Object.getOwnPropertyDescriptor ( O, P )
TF_BUILTIN(ObjectGetOwnPropertyDescriptor, ObjectBuiltinsAssembler) {
  Node* argc = Parameter(BuiltinDescriptor::kArgumentsCount);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  CSA_ASSERT(this, IsUndefined(Parameter(BuiltinDescriptor::kNewTarget)));

  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));
  Node* object = args.GetOptionalArgumentValue(0);
  Node* key = args.GetOptionalArgumentValue(1);

  // 1. Let obj be ? ToObject(O).
  object = ToObject(context, object);

  // 2. Let key be ? ToPropertyKey(P).
  key = ToName(context, key);

  // 3. Let desc be ? obj.[[GetOwnProperty]](key).
  Label if_keyisindex(this), if_iskeyunique(this),
      call_runtime(this, Label::kDeferred),
      return_undefined(this, Label::kDeferred), if_notunique_name(this);
  Node* map = LoadMap(object);
  Node* instance_type = LoadMapInstanceType(map);
  GotoIf(IsSpecialReceiverInstanceType(instance_type), &call_runtime);
  {
    VARIABLE(var_index, MachineType::PointerRepresentation(),
             IntPtrConstant(0));
    VARIABLE(var_name, MachineRepresentation::kTagged);

    TryToName(key, &if_keyisindex, &var_index, &if_iskeyunique, &var_name,
              &call_runtime, &if_notunique_name);

    BIND(&if_notunique_name);
    {
      Label not_in_string_table(this);
      TryInternalizeString(key, &if_keyisindex, &var_index, &if_iskeyunique,
                           &var_name, &not_in_string_table, &call_runtime);

      BIND(&not_in_string_table);
      {
        // If the string was not found in the string table, then no regular
        // object can have a property with that name, so return |undefined|.
        Goto(&return_undefined);
      }
    }

    BIND(&if_iskeyunique);
    {
      Label if_found_value(this), return_empty(this), if_not_found(this);

      VARIABLE(var_value, MachineRepresentation::kTagged);
      VARIABLE(var_details, MachineRepresentation::kWord32);
      VARIABLE(var_raw_value, MachineRepresentation::kTagged);

      TryGetOwnProperty(context, object, object, map, instance_type,
                        var_name.value(), &if_found_value, &var_value,
                        &var_details, &var_raw_value, &return_empty,
                        &if_not_found, kReturnAccessorPair);

      BIND(&if_found_value);
      // 4. Return FromPropertyDescriptor(desc).
      Node* js_desc = FromPropertyDetails(context, var_value.value(),
                                          var_details.value(), &call_runtime);
      args.PopAndReturn(js_desc);

      BIND(&return_empty);
      var_value.Bind(UndefinedConstant());
      args.PopAndReturn(UndefinedConstant());

      BIND(&if_not_found);
      Goto(&call_runtime);
    }
  }

  BIND(&if_keyisindex);
  Goto(&call_runtime);

  BIND(&call_runtime);
  {
    Node* desc =
        CallRuntime(Runtime::kGetOwnPropertyDescriptor, context, object, key);

    GotoIf(IsUndefined(desc), &return_undefined);

    CSA_ASSERT(this, IsFixedArray(desc));

    // 4. Return FromPropertyDescriptor(desc).
    Node* js_desc = FromPropertyDescriptor(context, desc);
    args.PopAndReturn(js_desc);
  }
  BIND(&return_undefined);
  args.PopAndReturn(UndefinedConstant());
}

void ObjectBuiltinsAssembler::AddToDictionaryIf(Node* condition,
                                                Node* name_dictionary,
                                                Handle<Name> name, Node* value,
                                                Label* bailout) {
  Label done(this);
  GotoIfNot(condition, &done);

  Add<NameDictionary>(name_dictionary, HeapConstant(name), value, bailout);
  Goto(&done);

  BIND(&done);
}

Node* ObjectBuiltinsAssembler::FromPropertyDescriptor(Node* context,
                                                      Node* desc) {
  VARIABLE(js_descriptor, MachineRepresentation::kTagged);

  Node* flags = LoadAndUntagToWord32ObjectField(
      desc, PropertyDescriptorObject::kFlagsOffset);

  Node* has_flags =
      Word32And(flags, Int32Constant(PropertyDescriptorObject::kHasMask));

  Label if_accessor_desc(this), if_data_desc(this), if_generic_desc(this),
      return_desc(this);
  GotoIf(
      Word32Equal(has_flags,
                  Int32Constant(
                      PropertyDescriptorObject::kRegularAccessorPropertyBits)),
      &if_accessor_desc);
  GotoIf(Word32Equal(
             has_flags,
             Int32Constant(PropertyDescriptorObject::kRegularDataPropertyBits)),
         &if_data_desc);
  Goto(&if_generic_desc);

  BIND(&if_accessor_desc);
  {
    js_descriptor.Bind(ConstructAccessorDescriptor(
        context, LoadObjectField(desc, PropertyDescriptorObject::kGetOffset),
        LoadObjectField(desc, PropertyDescriptorObject::kSetOffset),
        IsSetWord32<PropertyDescriptorObject::IsEnumerableBit>(flags),
        IsSetWord32<PropertyDescriptorObject::IsConfigurableBit>(flags)));
    Goto(&return_desc);
  }

  BIND(&if_data_desc);
  {
    js_descriptor.Bind(ConstructDataDescriptor(
        context, LoadObjectField(desc, PropertyDescriptorObject::kValueOffset),
        IsSetWord32<PropertyDescriptorObject::IsWritableBit>(flags),
        IsSetWord32<PropertyDescriptorObject::IsEnumerableBit>(flags),
        IsSetWord32<PropertyDescriptorObject::IsConfigurableBit>(flags)));
    Goto(&return_desc);
  }

  BIND(&if_generic_desc);
  {
    Node* native_context = LoadNativeContext(context);
    Node* map = LoadContextElement(
        native_context, Context::SLOW_OBJECT_WITH_OBJECT_PROTOTYPE_MAP);
    // We want to preallocate the slots for value, writable, get, set,
    // enumerable and configurable - a total of 6
    Node* properties = AllocateNameDictionary(6);
    Node* js_desc = AllocateJSObjectFromMap(map, properties);

    Label bailout(this, Label::kDeferred);

    Factory* factory = isolate()->factory();
    Node* value = LoadObjectField(desc, PropertyDescriptorObject::kValueOffset);
    AddToDictionaryIf(IsNotTheHole(value), properties, factory->value_string(),
                      value, &bailout);
    AddToDictionaryIf(
        IsSetWord32<PropertyDescriptorObject::HasWritableBit>(flags),
        properties, factory->writable_string(),
        SelectBooleanConstant(
            IsSetWord32<PropertyDescriptorObject::IsWritableBit>(flags)),
        &bailout);

    Node* get = LoadObjectField(desc, PropertyDescriptorObject::kGetOffset);
    AddToDictionaryIf(IsNotTheHole(get), properties, factory->get_string(), get,
                      &bailout);
    Node* set = LoadObjectField(desc, PropertyDescriptorObject::kSetOffset);
    AddToDictionaryIf(IsNotTheHole(set), properties, factory->set_string(), set,
                      &bailout);

    AddToDictionaryIf(
        IsSetWord32<PropertyDescriptorObject::HasEnumerableBit>(flags),
        properties, factory->enumerable_string(),
        SelectBooleanConstant(
            IsSetWord32<PropertyDescriptorObject::IsEnumerableBit>(flags)),
        &bailout);
    AddToDictionaryIf(
        IsSetWord32<PropertyDescriptorObject::HasConfigurableBit>(flags),
        properties, factory->configurable_string(),
        SelectBooleanConstant(
            IsSetWord32<PropertyDescriptorObject::IsConfigurableBit>(flags)),
        &bailout);

    js_descriptor.Bind(js_desc);
    Goto(&return_desc);

    BIND(&bailout);
    CSA_ASSERT(this, Int32Constant(0));
    Unreachable();
  }

  BIND(&return_desc);
  return js_descriptor.value();
}

Node* ObjectBuiltinsAssembler::FromPropertyDetails(Node* context,
                                                   Node* raw_value,
                                                   Node* details,
                                                   Label* if_bailout) {
  VARIABLE(js_descriptor, MachineRepresentation::kTagged);

  Label if_accessor_desc(this), if_data_desc(this), return_desc(this);
  BranchIfAccessorPair(raw_value, &if_accessor_desc, &if_data_desc);

  BIND(&if_accessor_desc);
  {
    Node* getter = LoadObjectField(raw_value, AccessorPair::kGetterOffset);
    Node* setter = LoadObjectField(raw_value, AccessorPair::kSetterOffset);
    js_descriptor.Bind(ConstructAccessorDescriptor(
        context, GetAccessorOrUndefined(getter, if_bailout),
        GetAccessorOrUndefined(setter, if_bailout),
        IsNotSetWord32(details, PropertyDetails::kAttributesDontEnumMask),
        IsNotSetWord32(details, PropertyDetails::kAttributesDontDeleteMask)));
    Goto(&return_desc);
  }

  BIND(&if_data_desc);
  {
    js_descriptor.Bind(ConstructDataDescriptor(
        context, raw_value,
        IsNotSetWord32(details, PropertyDetails::kAttributesReadOnlyMask),
        IsNotSetWord32(details, PropertyDetails::kAttributesDontEnumMask),
        IsNotSetWord32(details, PropertyDetails::kAttributesDontDeleteMask)));
    Goto(&return_desc);
  }

  BIND(&return_desc);
  return js_descriptor.value();
}

Node* ObjectBuiltinsAssembler::GetAccessorOrUndefined(Node* accessor,
                                                      Label* if_bailout) {
  Label bind_undefined(this, Label::kDeferred), return_result(this);
  VARIABLE(result, MachineRepresentation::kTagged);

  GotoIf(IsNull(accessor), &bind_undefined);
  result.Bind(accessor);
  Node* map = LoadMap(accessor);
  // TODO(ishell): probe template instantiations cache.
  GotoIf(IsFunctionTemplateInfoMap(map), if_bailout);
  Goto(&return_result);

  BIND(&bind_undefined);
  result.Bind(UndefinedConstant());
  Goto(&return_result);

  BIND(&return_result);
  return result.value();
}
}  // namespace internal
}  // namespace v8
