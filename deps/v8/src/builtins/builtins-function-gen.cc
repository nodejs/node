// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

TF_BUILTIN(FastFunctionPrototypeBind, CodeStubAssembler) {
  Label slow(this);

  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  Node* argc = Parameter(BuiltinDescriptor::kArgumentsCount);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);

  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));

  // Check that receiver has instance type of JS_FUNCTION_TYPE
  Node* receiver = args.GetReceiver();
  GotoIf(TaggedIsSmi(receiver), &slow);

  Node* receiver_map = LoadMap(receiver);
  Node* instance_type = LoadMapInstanceType(receiver_map);
  GotoIf(Word32NotEqual(instance_type, Int32Constant(JS_FUNCTION_TYPE)), &slow);

  // Disallow binding of slow-mode functions. We need to figure out whether the
  // length and name property are in the original state.
  Comment("Disallow binding of slow-mode functions");
  GotoIf(IsDictionaryMap(receiver_map), &slow);

  // Check whether the length and name properties are still present as
  // AccessorInfo objects. In that case, their value can be recomputed even if
  // the actual value on the object changes.
  Comment("Check descriptor array length");
  Node* descriptors = LoadMapDescriptors(receiver_map);
  Node* descriptors_length = LoadFixedArrayBaseLength(descriptors);
  GotoIf(SmiLessThanOrEqual(descriptors_length, SmiConstant(1)), &slow);

  // Check whether the length and name properties are still present as
  // AccessorInfo objects. In that case, their value can be recomputed even if
  // the actual value on the object changes.
  Comment("Check name and length properties");
  const int length_index = JSFunction::kLengthDescriptorIndex;
  Node* maybe_length = LoadFixedArrayElement(
      descriptors, DescriptorArray::ToKeyIndex(length_index));
  GotoIf(WordNotEqual(maybe_length, LoadRoot(Heap::klength_stringRootIndex)),
         &slow);

  Node* maybe_length_accessor = LoadFixedArrayElement(
      descriptors, DescriptorArray::ToValueIndex(length_index));
  GotoIf(TaggedIsSmi(maybe_length_accessor), &slow);
  Node* length_value_map = LoadMap(maybe_length_accessor);
  GotoIfNot(IsAccessorInfoMap(length_value_map), &slow);

  const int name_index = JSFunction::kNameDescriptorIndex;
  Node* maybe_name = LoadFixedArrayElement(
      descriptors, DescriptorArray::ToKeyIndex(name_index));
  GotoIf(WordNotEqual(maybe_name, LoadRoot(Heap::kname_stringRootIndex)),
         &slow);

  Node* maybe_name_accessor = LoadFixedArrayElement(
      descriptors, DescriptorArray::ToValueIndex(name_index));
  GotoIf(TaggedIsSmi(maybe_name_accessor), &slow);
  Node* name_value_map = LoadMap(maybe_name_accessor);
  GotoIfNot(IsAccessorInfoMap(name_value_map), &slow);

  // Choose the right bound function map based on whether the target is
  // constructable.
  Comment("Choose the right bound function map");
  VARIABLE(bound_function_map, MachineRepresentation::kTagged);
  Label with_constructor(this);
  VariableList vars({&bound_function_map}, zone());
  Node* native_context = LoadNativeContext(context);

  Label map_done(this, vars);
  Node* bit_field = LoadMapBitField(receiver_map);
  int mask = static_cast<int>(1 << Map::kIsConstructor);
  GotoIf(IsSetWord32(bit_field, mask), &with_constructor);

  bound_function_map.Bind(LoadContextElement(
      native_context, Context::BOUND_FUNCTION_WITHOUT_CONSTRUCTOR_MAP_INDEX));
  Goto(&map_done);

  BIND(&with_constructor);
  bound_function_map.Bind(LoadContextElement(
      native_context, Context::BOUND_FUNCTION_WITH_CONSTRUCTOR_MAP_INDEX));
  Goto(&map_done);

  BIND(&map_done);

  // Verify that __proto__ matches that of a the target bound function.
  Comment("Verify that __proto__ matches target bound function");
  Node* prototype = LoadMapPrototype(receiver_map);
  Node* expected_prototype = LoadMapPrototype(bound_function_map.value());
  GotoIf(WordNotEqual(prototype, expected_prototype), &slow);

  // Allocate the arguments array.
  Comment("Allocate the arguments array");
  VARIABLE(argument_array, MachineRepresentation::kTagged);
  Label empty_arguments(this);
  Label arguments_done(this, &argument_array);
  GotoIf(Uint32LessThanOrEqual(argc, Int32Constant(1)), &empty_arguments);
  Node* elements_length = ChangeUint32ToWord(Int32Sub(argc, Int32Constant(1)));
  Node* elements = AllocateFixedArray(FAST_ELEMENTS, elements_length);
  VARIABLE(index, MachineType::PointerRepresentation());
  index.Bind(IntPtrConstant(0));
  VariableList foreach_vars({&index}, zone());
  args.ForEach(foreach_vars,
               [this, elements, &index](Node* arg) {
                 StoreFixedArrayElement(elements, index.value(), arg);
                 Increment(index);
               },
               IntPtrConstant(1));
  argument_array.Bind(elements);
  Goto(&arguments_done);

  BIND(&empty_arguments);
  argument_array.Bind(EmptyFixedArrayConstant());
  Goto(&arguments_done);

  BIND(&arguments_done);

  // Determine bound receiver.
  Comment("Determine bound receiver");
  VARIABLE(bound_receiver, MachineRepresentation::kTagged);
  Label has_receiver(this);
  Label receiver_done(this, &bound_receiver);
  GotoIf(Word32NotEqual(argc, Int32Constant(0)), &has_receiver);
  bound_receiver.Bind(UndefinedConstant());
  Goto(&receiver_done);

  BIND(&has_receiver);
  bound_receiver.Bind(args.AtIndex(0));
  Goto(&receiver_done);

  BIND(&receiver_done);

  // Allocate the resulting bound function.
  Comment("Allocate the resulting bound function");
  Node* bound_function = Allocate(JSBoundFunction::kSize);
  StoreMapNoWriteBarrier(bound_function, bound_function_map.value());
  StoreObjectFieldNoWriteBarrier(
      bound_function, JSBoundFunction::kBoundTargetFunctionOffset, receiver);
  StoreObjectFieldNoWriteBarrier(bound_function,
                                 JSBoundFunction::kBoundThisOffset,
                                 bound_receiver.value());
  StoreObjectFieldNoWriteBarrier(bound_function,
                                 JSBoundFunction::kBoundArgumentsOffset,
                                 argument_array.value());
  Node* empty_fixed_array = EmptyFixedArrayConstant();
  StoreObjectFieldNoWriteBarrier(bound_function, JSObject::kPropertiesOffset,
                                 empty_fixed_array);
  StoreObjectFieldNoWriteBarrier(bound_function, JSObject::kElementsOffset,
                                 empty_fixed_array);

  args.PopAndReturn(bound_function);
  BIND(&slow);

  Node* target = LoadFromFrame(StandardFrameConstants::kFunctionOffset,
                               MachineType::TaggedPointer());
  TailCallStub(CodeFactory::FunctionPrototypeBind(isolate()), context, target,
               new_target, argc);
}

// ES6 #sec-function.prototype-@@hasinstance
TF_BUILTIN(FunctionPrototypeHasInstance, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* f = Parameter(Descriptor::kReceiver);
  Node* v = Parameter(Descriptor::kV);
  Node* result = OrdinaryHasInstance(context, f, v);
  Return(result);
}

}  // namespace internal
}  // namespace v8
