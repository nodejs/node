// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/execution/frame-constants.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/descriptor-array.h"

namespace v8 {
namespace internal {

TF_BUILTIN(FastFunctionPrototypeBind, CodeStubAssembler) {
  Label slow(this);

  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kJSNewTarget));

  CodeStubArguments args(this, argc);

  // Check that receiver has instance type of JS_FUNCTION_TYPE
  TNode<Object> receiver = args.GetReceiver();
  GotoIf(TaggedIsSmi(receiver), &slow);

  TNode<Map> receiver_map = LoadMap(CAST(receiver));
  {
    TNode<Uint16T> instance_type = LoadMapInstanceType(receiver_map);
    GotoIfNot(
        Word32Or(InstanceTypeEqual(instance_type, JS_FUNCTION_TYPE),
                 InstanceTypeEqual(instance_type, JS_BOUND_FUNCTION_TYPE)),
        &slow);
  }

  // Disallow binding of slow-mode functions. We need to figure out whether the
  // length and name property are in the original state.
  Comment("Disallow binding of slow-mode functions");
  GotoIf(IsDictionaryMap(receiver_map), &slow);

  // Check whether the length and name properties are still present as
  // AccessorInfo objects. In that case, their value can be recomputed even if
  // the actual value on the object changes.
  Comment("Check descriptor array length");
  // Minimum descriptor array length required for fast path.
  const int min_nof_descriptors = i::Max(JSFunction::kLengthDescriptorIndex,
                                         JSFunction::kNameDescriptorIndex) +
                                  1;
  TNode<Int32T> nof_descriptors = LoadNumberOfOwnDescriptors(receiver_map);
  GotoIf(Int32LessThan(nof_descriptors, Int32Constant(min_nof_descriptors)),
         &slow);

  // Check whether the length and name properties are still present as
  // AccessorInfo objects. In that case, their value can be recomputed even if
  // the actual value on the object changes.
  Comment("Check name and length properties");
  {
    TNode<DescriptorArray> descriptors = LoadMapDescriptors(receiver_map);
    const int length_index = JSFunction::kLengthDescriptorIndex;
    TNode<Name> maybe_length =
        LoadKeyByDescriptorEntry(descriptors, length_index);
    GotoIf(TaggedNotEqual(maybe_length, LengthStringConstant()), &slow);

    TNode<Object> maybe_length_accessor =
        LoadValueByDescriptorEntry(descriptors, length_index);
    GotoIf(TaggedIsSmi(maybe_length_accessor), &slow);
    TNode<Map> length_value_map = LoadMap(CAST(maybe_length_accessor));
    GotoIfNot(IsAccessorInfoMap(length_value_map), &slow);

    const int name_index = JSFunction::kNameDescriptorIndex;
    TNode<Name> maybe_name = LoadKeyByDescriptorEntry(descriptors, name_index);
    GotoIf(TaggedNotEqual(maybe_name, NameStringConstant()), &slow);

    TNode<Object> maybe_name_accessor =
        LoadValueByDescriptorEntry(descriptors, name_index);
    GotoIf(TaggedIsSmi(maybe_name_accessor), &slow);
    TNode<Map> name_value_map = LoadMap(CAST(maybe_name_accessor));
    GotoIfNot(IsAccessorInfoMap(name_value_map), &slow);
  }

  // Choose the right bound function map based on whether the target is
  // constructable.
  Comment("Choose the right bound function map");
  TVARIABLE(Map, bound_function_map);
  {
    Label with_constructor(this);
    TNode<NativeContext> native_context = LoadNativeContext(context);

    Label map_done(this, &bound_function_map);
    GotoIf(IsConstructorMap(receiver_map), &with_constructor);

    bound_function_map = CAST(LoadContextElement(
        native_context, Context::BOUND_FUNCTION_WITHOUT_CONSTRUCTOR_MAP_INDEX));
    Goto(&map_done);

    BIND(&with_constructor);
    bound_function_map = CAST(LoadContextElement(
        native_context, Context::BOUND_FUNCTION_WITH_CONSTRUCTOR_MAP_INDEX));
    Goto(&map_done);

    BIND(&map_done);
  }

  // Verify that __proto__ matches that of a the target bound function.
  Comment("Verify that __proto__ matches target bound function");
  TNode<HeapObject> prototype = LoadMapPrototype(receiver_map);
  TNode<HeapObject> expected_prototype =
      LoadMapPrototype(bound_function_map.value());
  GotoIf(TaggedNotEqual(prototype, expected_prototype), &slow);

  // Allocate the arguments array.
  Comment("Allocate the arguments array");
  TVARIABLE(FixedArray, argument_array);
  {
    Label empty_arguments(this);
    Label arguments_done(this, &argument_array);
    GotoIf(Uint32LessThanOrEqual(argc, Int32Constant(1)), &empty_arguments);
    TNode<IntPtrT> elements_length =
        Signed(ChangeUint32ToWord(Unsigned(Int32Sub(argc, Int32Constant(1)))));
    argument_array = CAST(AllocateFixedArray(PACKED_ELEMENTS, elements_length,
                                             kAllowLargeObjectAllocation));
    TVARIABLE(IntPtrT, index, IntPtrConstant(0));
    VariableList foreach_vars({&index}, zone());
    args.ForEach(
        foreach_vars,
        [&](TNode<Object> arg) {
          StoreFixedArrayElement(argument_array.value(), index.value(), arg);
          Increment(&index);
        },
        IntPtrConstant(1));
    Goto(&arguments_done);

    BIND(&empty_arguments);
    argument_array = EmptyFixedArrayConstant();
    Goto(&arguments_done);

    BIND(&arguments_done);
  }

  // Determine bound receiver.
  Comment("Determine bound receiver");
  TVARIABLE(Object, bound_receiver);
  {
    Label has_receiver(this);
    Label receiver_done(this, &bound_receiver);
    GotoIf(Word32NotEqual(argc, Int32Constant(0)), &has_receiver);
    bound_receiver = UndefinedConstant();
    Goto(&receiver_done);

    BIND(&has_receiver);
    bound_receiver = args.AtIndex(0);
    Goto(&receiver_done);

    BIND(&receiver_done);
  }

  // Allocate the resulting bound function.
  Comment("Allocate the resulting bound function");
  {
    TNode<HeapObject> bound_function = Allocate(JSBoundFunction::kHeaderSize);
    StoreMapNoWriteBarrier(bound_function, bound_function_map.value());
    StoreObjectFieldNoWriteBarrier(
        bound_function, JSBoundFunction::kBoundTargetFunctionOffset, receiver);
    StoreObjectFieldNoWriteBarrier(bound_function,
                                   JSBoundFunction::kBoundThisOffset,
                                   bound_receiver.value());
    StoreObjectFieldNoWriteBarrier(bound_function,
                                   JSBoundFunction::kBoundArgumentsOffset,
                                   argument_array.value());
    TNode<FixedArray> empty_fixed_array = EmptyFixedArrayConstant();
    StoreObjectFieldNoWriteBarrier(
        bound_function, JSObject::kPropertiesOrHashOffset, empty_fixed_array);
    StoreObjectFieldNoWriteBarrier(bound_function, JSObject::kElementsOffset,
                                   empty_fixed_array);

    args.PopAndReturn(bound_function);
  }

  BIND(&slow);
  {
    // We are not using Parameter(Descriptor::kJSTarget) and loading the value
    // from the current frame here in order to reduce register pressure on the
    // fast path.
    TNode<JSFunction> target = LoadTargetFromFrame();
    TailCallBuiltin(Builtins::kFunctionPrototypeBind, context, target,
                    new_target, argc);
  }
}

// ES6 #sec-function.prototype-@@hasinstance
TF_BUILTIN(FunctionPrototypeHasInstance, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> f = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> v = CAST(Parameter(Descriptor::kV));
  TNode<Oddball> result = OrdinaryHasInstance(context, f, v);
  Return(result);
}

}  // namespace internal
}  // namespace v8
