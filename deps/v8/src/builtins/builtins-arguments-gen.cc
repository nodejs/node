// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-arguments-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/codegen/interface-descriptors.h"
#include "src/execution/arguments.h"
#include "src/execution/frame-constants.h"
#include "src/objects/arguments.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

ArgumentsBuiltinsAssembler::ArgumentsAllocationResult
ArgumentsBuiltinsAssembler::AllocateArgumentsObject(
    TNode<Map> map, TNode<BInt> arguments_count,
    TNode<BInt> parameter_map_count, int base_size) {
  // Allocate the parameter object (either a Rest parameter object, a strict
  // argument object or a sloppy arguments object) and the elements/mapped
  // arguments together.
  int elements_offset = base_size;
  TNode<BInt> element_count = arguments_count;
  if (parameter_map_count != nullptr) {
    base_size += FixedArray::kHeaderSize;
    element_count = IntPtrOrSmiAdd(element_count, parameter_map_count);
  }
  bool empty = IsIntPtrOrSmiConstantZero(arguments_count);
  DCHECK_IMPLIES(empty, parameter_map_count == nullptr);
  TNode<IntPtrT> size =
      empty ? IntPtrConstant(base_size)
            : ElementOffsetFromIndex(element_count, PACKED_ELEMENTS,
                                     base_size + FixedArray::kHeaderSize);
  TNode<HeapObject> result = Allocate(size);
  Comment("Initialize arguments object");
  StoreMapNoWriteBarrier(result, map);
  TNode<FixedArray> empty_fixed_array = EmptyFixedArrayConstant();
  StoreObjectField(result, JSArray::kPropertiesOrHashOffset, empty_fixed_array);
  TNode<Smi> smi_arguments_count = BIntToSmi(arguments_count);
  StoreObjectFieldNoWriteBarrier(result, JSArray::kLengthOffset,
                                 smi_arguments_count);
  TNode<HeapObject> arguments;
  if (!empty) {
    arguments = InnerAllocate(result, elements_offset);
    StoreObjectFieldNoWriteBarrier(arguments, FixedArray::kLengthOffset,
                                   smi_arguments_count);
    TNode<Map> fixed_array_map = FixedArrayMapConstant();
    StoreMapNoWriteBarrier(arguments, fixed_array_map);
  }
  TNode<HeapObject> parameter_map;
  if (!parameter_map_count.is_null()) {
    TNode<IntPtrT> parameter_map_offset = ElementOffsetFromIndex(
        arguments_count, PACKED_ELEMENTS, FixedArray::kHeaderSize);
    parameter_map = InnerAllocate(arguments, parameter_map_offset);
    StoreObjectFieldNoWriteBarrier(result, JSArray::kElementsOffset,
                                   parameter_map);
    TNode<Map> sloppy_elements_map = SloppyArgumentsElementsMapConstant();
    StoreMapNoWriteBarrier(parameter_map, sloppy_elements_map);
    StoreObjectFieldNoWriteBarrier(parameter_map, FixedArray::kLengthOffset,
                                   BIntToSmi(parameter_map_count));
  } else {
    if (empty) {
      StoreObjectFieldNoWriteBarrier(result, JSArray::kElementsOffset,
                                     empty_fixed_array);
    } else {
      StoreObjectFieldNoWriteBarrier(result, JSArray::kElementsOffset,
                                     arguments);
    }
  }
  return {CAST(result), UncheckedCast<FixedArray>(arguments),
          UncheckedCast<FixedArray>(parameter_map)};
}

TNode<JSObject> ArgumentsBuiltinsAssembler::ConstructParametersObjectFromArgs(
    TNode<Map> map, TNode<RawPtrT> frame_ptr, TNode<BInt> arg_count,
    TNode<BInt> first_arg, TNode<BInt> rest_count, int base_size) {
  // Allocate the parameter object (either a Rest parameter object, a strict
  // argument object or a sloppy arguments object) and the elements together and
  // fill in the contents with the arguments above |formal_parameter_count|.
  ArgumentsAllocationResult alloc_result =
      AllocateArgumentsObject(map, rest_count, {}, base_size);
  DCHECK(alloc_result.parameter_map.is_null());
  CodeStubArguments arguments(this, arg_count, frame_ptr);
  TVARIABLE(IntPtrT, offset,
            IntPtrConstant(FixedArrayBase::kHeaderSize - kHeapObjectTag));
  VariableList list({&offset}, zone());
  arguments.ForEach(
      list,
      [&](TNode<Object> arg) {
        StoreNoWriteBarrier(MachineRepresentation::kTagged,
                            alloc_result.elements, offset.value(), arg);
        Increment(&offset, kTaggedSize);
      },
      first_arg);
  return alloc_result.arguments_object;
}

TNode<JSObject> ArgumentsBuiltinsAssembler::EmitFastNewRestParameter(
    TNode<Context> context, TNode<JSFunction> function) {
  ParameterMode mode = OptimalParameterMode();
  TNode<BInt> zero = BIntConstant(0);

  TorqueStructArgumentsInfo info = GetArgumentsFrameAndCount(context, function);

  TVARIABLE(JSObject, result);
  Label no_rest_parameters(this), runtime(this, Label::kDeferred),
      done(this, &result);

  TNode<BInt> rest_count =
      IntPtrOrSmiSub(info.argument_count, info.formal_parameter_count);
  const TNode<NativeContext> native_context = LoadNativeContext(context);
  const TNode<Map> array_map =
      LoadJSArrayElementsMap(PACKED_ELEMENTS, native_context);
  GotoIf(IntPtrOrSmiLessThanOrEqual(rest_count, zero), &no_rest_parameters);

  GotoIfFixedArraySizeDoesntFitInNewSpace(
      rest_count, &runtime, JSArray::kHeaderSize + FixedArray::kHeaderSize,
      mode);

  // Allocate the Rest JSArray and the elements together and fill in the
  // contents with the arguments above |formal_parameter_count|.
  result = ConstructParametersObjectFromArgs(
      array_map, info.frame, info.argument_count, info.formal_parameter_count,
      rest_count, JSArray::kHeaderSize);
  Goto(&done);

  BIND(&no_rest_parameters);
  {
    ArgumentsAllocationResult alloc_result =
        AllocateArgumentsObject(array_map, zero, {}, JSArray::kHeaderSize);
    result = alloc_result.arguments_object;
    Goto(&done);
  }

  BIND(&runtime);
  {
    result = CAST(CallRuntime(Runtime::kNewRestParameter, context, function));
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

TNode<JSObject> ArgumentsBuiltinsAssembler::EmitFastNewStrictArguments(
    TNode<Context> context, TNode<JSFunction> function) {
  TVARIABLE(JSObject, result);
  Label done(this, &result), empty(this), runtime(this, Label::kDeferred);

  ParameterMode mode = OptimalParameterMode();
  TNode<BInt> zero = BIntConstant(0);

  TorqueStructArgumentsInfo info = GetArgumentsFrameAndCount(context, function);

  GotoIfFixedArraySizeDoesntFitInNewSpace(
      info.argument_count, &runtime,
      JSStrictArgumentsObject::kSize + FixedArray::kHeaderSize, mode);

  const TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<Map> map = CAST(
      LoadContextElement(native_context, Context::STRICT_ARGUMENTS_MAP_INDEX));
  GotoIf(BIntEqual(info.argument_count, zero), &empty);

  result = ConstructParametersObjectFromArgs(
      map, info.frame, info.argument_count, zero, info.argument_count,
      JSStrictArgumentsObject::kSize);
  Goto(&done);

  BIND(&empty);
  {
    ArgumentsAllocationResult alloc_result =
        AllocateArgumentsObject(map, zero, {}, JSStrictArgumentsObject::kSize);
    result = alloc_result.arguments_object;
    Goto(&done);
  }

  BIND(&runtime);
  {
    result = CAST(CallRuntime(Runtime::kNewStrictArguments, context, function));
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

TNode<JSObject> ArgumentsBuiltinsAssembler::EmitFastNewSloppyArguments(
    TNode<Context> context, TNode<JSFunction> function) {
  TVARIABLE(JSObject, result);

  ParameterMode mode = OptimalParameterMode();
  TNode<BInt> zero = BIntConstant(0);

  Label done(this, &result), empty(this), no_parameters(this),
      runtime(this, Label::kDeferred);

  TorqueStructArgumentsInfo info = GetArgumentsFrameAndCount(context, function);

  GotoIf(BIntEqual(info.argument_count, zero), &empty);

  GotoIf(BIntEqual(info.formal_parameter_count, zero), &no_parameters);

  {
    Comment("Mapped parameter JSSloppyArgumentsObject");

    TNode<BInt> mapped_count =
        IntPtrOrSmiMin(info.argument_count, info.formal_parameter_count);

    TNode<BInt> parameter_map_size =
        IntPtrOrSmiAdd(mapped_count, BIntConstant(2));

    // Verify that the overall allocation will fit in new space.
    TNode<BInt> elements_allocated =
        IntPtrOrSmiAdd(info.argument_count, parameter_map_size);
    GotoIfFixedArraySizeDoesntFitInNewSpace(
        elements_allocated, &runtime,
        JSSloppyArgumentsObject::kSize + FixedArray::kHeaderSize * 2, mode);

    const TNode<NativeContext> native_context = LoadNativeContext(context);
    const TNode<Map> map = CAST(LoadContextElement(
        native_context, Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX));
    ArgumentsAllocationResult alloc_result =
        AllocateArgumentsObject(map, info.argument_count, parameter_map_size,
                                JSSloppyArgumentsObject::kSize);
    StoreObjectFieldNoWriteBarrier(alloc_result.arguments_object,
                                   JSSloppyArgumentsObject::kCalleeOffset,
                                   function);
    StoreFixedArrayElement(alloc_result.parameter_map, 0, context,
                           SKIP_WRITE_BARRIER);
    StoreFixedArrayElement(alloc_result.parameter_map, 1, alloc_result.elements,
                           SKIP_WRITE_BARRIER);

    Comment("Fill in non-mapped parameters");
    TNode<IntPtrT> argument_offset =
        ElementOffsetFromIndex(info.argument_count, PACKED_ELEMENTS,
                               FixedArray::kHeaderSize - kHeapObjectTag);
    TNode<IntPtrT> mapped_offset =
        ElementOffsetFromIndex(mapped_count, PACKED_ELEMENTS,
                               FixedArray::kHeaderSize - kHeapObjectTag);
    CodeStubArguments arguments(this, info.argument_count, info.frame);
    TVARIABLE(RawPtrT, current_argument,
              arguments.AtIndexPtr(info.argument_count));
    VariableList var_list1({&current_argument}, zone());
    mapped_offset = BuildFastLoop<IntPtrT>(
        var_list1, argument_offset, mapped_offset,
        [&](TNode<IntPtrT> offset) {
          Increment(&current_argument, kSystemPointerSize);
          TNode<Object> arg = LoadBufferObject(
              ReinterpretCast<RawPtrT>(current_argument.value()), 0);
          StoreNoWriteBarrier(MachineRepresentation::kTagged,
                              alloc_result.elements, offset, arg);
          return;
        },
        -kTaggedSize);

    // Copy the parameter slots and the holes in the arguments.
    // We need to fill in mapped_count slots. They index the context,
    // where parameters are stored in reverse order, at
    //   context_header_size .. context_header_size+argument_count-1
    // The mapped parameter thus need to get indices
    //   context_header_size+parameter_count-1 ..
    //       context_header_size+argument_count-mapped_count
    // We loop from right to left.
    Comment("Fill in mapped parameters");
    STATIC_ASSERT(Context::MIN_CONTEXT_EXTENDED_SLOTS ==
                  Context::MIN_CONTEXT_SLOTS + 1);
    TNode<IntPtrT> flags = LoadAndUntagObjectField(LoadScopeInfo(context),
                                                   ScopeInfo::kFlagsOffset);
    TNode<BInt> context_header_size = IntPtrOrSmiAdd(
        IntPtrToBInt(
            Signed(DecodeWord<ScopeInfo::HasContextExtensionSlotField>(flags))),
        BIntConstant(Context::MIN_CONTEXT_SLOTS));
    TVARIABLE(BInt, context_index,
              IntPtrOrSmiSub(IntPtrOrSmiAdd(context_header_size,
                                            info.formal_parameter_count),
                             mapped_count));
    TNode<Oddball> the_hole = TheHoleConstant();
    VariableList var_list2({&context_index}, zone());
    const int kParameterMapHeaderSize = FixedArray::OffsetOfElementAt(2);
    TNode<IntPtrT> adjusted_map_array = IntPtrAdd(
        BitcastTaggedToWord(alloc_result.parameter_map),
        IntPtrConstant(kParameterMapHeaderSize - FixedArray::kHeaderSize));
    TNode<IntPtrT> zero_offset = ElementOffsetFromIndex(
        zero, PACKED_ELEMENTS, mode, FixedArray::kHeaderSize - kHeapObjectTag);
    BuildFastLoop<IntPtrT>(
        var_list2, mapped_offset, zero_offset,
        [&](TNode<IntPtrT> offset) {
          StoreNoWriteBarrier(MachineRepresentation::kTagged,
                              alloc_result.elements, offset, the_hole);
          StoreNoWriteBarrier(MachineRepresentation::kTagged,
                              adjusted_map_array, offset,
                              BIntToSmi(context_index.value()));
          Increment(&context_index);
        },
        -kTaggedSize);

    result = alloc_result.arguments_object;
    Goto(&done);
  }

  BIND(&no_parameters);
  {
    Comment("No parameters JSSloppyArgumentsObject");
    GotoIfFixedArraySizeDoesntFitInNewSpace(
        info.argument_count, &runtime,
        JSSloppyArgumentsObject::kSize + FixedArray::kHeaderSize, mode);
    const TNode<NativeContext> native_context = LoadNativeContext(context);
    TNode<Map> map = CAST(LoadContextElement(
        native_context, Context::SLOPPY_ARGUMENTS_MAP_INDEX));
    result = ConstructParametersObjectFromArgs(
        map, info.frame, info.argument_count, zero, info.argument_count,
        JSSloppyArgumentsObject::kSize);
    StoreObjectFieldNoWriteBarrier(
        result.value(), JSSloppyArgumentsObject::kCalleeOffset, function);
    Goto(&done);
  }

  BIND(&empty);
  {
    Comment("Empty JSSloppyArgumentsObject");
    const TNode<NativeContext> native_context = LoadNativeContext(context);
    const TNode<Map> map = CAST(LoadContextElement(
        native_context, Context::SLOPPY_ARGUMENTS_MAP_INDEX));
    ArgumentsAllocationResult alloc_result =
        AllocateArgumentsObject(map, zero, {}, JSSloppyArgumentsObject::kSize);
    result = alloc_result.arguments_object;
    StoreObjectFieldNoWriteBarrier(
        result.value(), JSSloppyArgumentsObject::kCalleeOffset, function);
    Goto(&done);
  }

  BIND(&runtime);
  {
    result = CAST(CallRuntime(Runtime::kNewSloppyArguments, context, function));
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

}  // namespace internal
}  // namespace v8
