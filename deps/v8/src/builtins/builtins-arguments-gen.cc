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

using Node = compiler::Node;

std::tuple<Node*, Node*, Node*>
ArgumentsBuiltinsAssembler::AllocateArgumentsObject(Node* map,
                                                    Node* arguments_count,
                                                    Node* parameter_map_count,
                                                    ParameterMode mode,
                                                    int base_size) {
  // Allocate the parameter object (either a Rest parameter object, a strict
  // argument object or a sloppy arguments object) and the elements/mapped
  // arguments together.
  int elements_offset = base_size;
  Node* element_count = arguments_count;
  if (parameter_map_count != nullptr) {
    base_size += FixedArray::kHeaderSize;
    element_count = IntPtrOrSmiAdd(element_count, parameter_map_count, mode);
  }
  bool empty = IsIntPtrOrSmiConstantZero(arguments_count, mode);
  DCHECK_IMPLIES(empty, parameter_map_count == nullptr);
  TNode<IntPtrT> size =
      empty ? IntPtrConstant(base_size)
            : ElementOffsetFromIndex(element_count, PACKED_ELEMENTS, mode,
                                     base_size + FixedArray::kHeaderSize);
  TNode<Object> result = Allocate(size);
  Comment("Initialize arguments object");
  StoreMapNoWriteBarrier(result, map);
  Node* empty_fixed_array = LoadRoot(RootIndex::kEmptyFixedArray);
  StoreObjectField(result, JSArray::kPropertiesOrHashOffset, empty_fixed_array);
  Node* smi_arguments_count = ParameterToTagged(arguments_count, mode);
  StoreObjectFieldNoWriteBarrier(result, JSArray::kLengthOffset,
                                 smi_arguments_count);
  Node* arguments = nullptr;
  if (!empty) {
    arguments = InnerAllocate(CAST(result), elements_offset);
    StoreObjectFieldNoWriteBarrier(arguments, FixedArray::kLengthOffset,
                                   smi_arguments_count);
    Node* fixed_array_map = LoadRoot(RootIndex::kFixedArrayMap);
    StoreMapNoWriteBarrier(arguments, fixed_array_map);
  }
  Node* parameter_map = nullptr;
  if (parameter_map_count != nullptr) {
    TNode<IntPtrT> parameter_map_offset = ElementOffsetFromIndex(
        arguments_count, PACKED_ELEMENTS, mode, FixedArray::kHeaderSize);
    parameter_map = InnerAllocate(CAST(arguments), parameter_map_offset);
    StoreObjectFieldNoWriteBarrier(result, JSArray::kElementsOffset,
                                   parameter_map);
    Node* sloppy_elements_map =
        LoadRoot(RootIndex::kSloppyArgumentsElementsMap);
    StoreMapNoWriteBarrier(parameter_map, sloppy_elements_map);
    parameter_map_count = ParameterToTagged(parameter_map_count, mode);
    StoreObjectFieldNoWriteBarrier(parameter_map, FixedArray::kLengthOffset,
                                   parameter_map_count);
  } else {
    if (empty) {
      StoreObjectFieldNoWriteBarrier(result, JSArray::kElementsOffset,
                                     empty_fixed_array);
    } else {
      StoreObjectFieldNoWriteBarrier(result, JSArray::kElementsOffset,
                                     arguments);
    }
  }
  return std::tuple<Node*, Node*, Node*>(result, arguments, parameter_map);
}

Node* ArgumentsBuiltinsAssembler::ConstructParametersObjectFromArgs(
    Node* map, Node* frame_ptr, Node* arg_count, Node* first_arg,
    Node* rest_count, ParameterMode param_mode, int base_size) {
  // Allocate the parameter object (either a Rest parameter object, a strict
  // argument object or a sloppy arguments object) and the elements together and
  // fill in the contents with the arguments above |formal_parameter_count|.
  Node* result;
  Node* elements;
  Node* unused;
  std::tie(result, elements, unused) =
      AllocateArgumentsObject(map, rest_count, nullptr, param_mode, base_size);
  DCHECK_NULL(unused);
  CodeStubArguments arguments(this, arg_count, frame_ptr, param_mode);
  VARIABLE(offset, MachineType::PointerRepresentation());
  offset.Bind(IntPtrConstant(FixedArrayBase::kHeaderSize - kHeapObjectTag));
  VariableList list({&offset}, zone());
  arguments.ForEach(list,
                    [this, elements, &offset](Node* arg) {
                      StoreNoWriteBarrier(MachineRepresentation::kTagged,
                                          elements, offset.value(), arg);
                      Increment(&offset, kTaggedSize);
                    },
                    first_arg, nullptr, param_mode);
  return result;
}

Node* ArgumentsBuiltinsAssembler::EmitFastNewRestParameter(Node* context,
                                                           Node* function) {
  ParameterMode mode = OptimalParameterMode();
  Node* zero = IntPtrOrSmiConstant(0, mode);

  TorqueStructArgumentsInfo info = GetArgumentsFrameAndCount(
      CAST(context), UncheckedCast<JSFunction>(function));

  VARIABLE(result, MachineRepresentation::kTagged);
  Label no_rest_parameters(this), runtime(this, Label::kDeferred),
      done(this, &result);

  Node* rest_count =
      IntPtrOrSmiSub(info.argument_count, info.formal_parameter_count, mode);
  Node* const native_context = LoadNativeContext(context);
  Node* const array_map =
      LoadJSArrayElementsMap(PACKED_ELEMENTS, native_context);
  GotoIf(IntPtrOrSmiLessThanOrEqual(rest_count, zero, mode),
         &no_rest_parameters);

  GotoIfFixedArraySizeDoesntFitInNewSpace(
      rest_count, &runtime, JSArray::kSize + FixedArray::kHeaderSize, mode);

  // Allocate the Rest JSArray and the elements together and fill in the
  // contents with the arguments above |formal_parameter_count|.
  result.Bind(ConstructParametersObjectFromArgs(
      array_map, info.frame, info.argument_count, info.formal_parameter_count,
      rest_count, mode, JSArray::kSize));
  Goto(&done);

  BIND(&no_rest_parameters);
  {
    Node* arguments;
    Node* elements;
    Node* unused;
    std::tie(arguments, elements, unused) =
        AllocateArgumentsObject(array_map, zero, nullptr, mode, JSArray::kSize);
    result.Bind(arguments);
    Goto(&done);
  }

  BIND(&runtime);
  {
    result.Bind(CallRuntime(Runtime::kNewRestParameter, context, function));
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

Node* ArgumentsBuiltinsAssembler::EmitFastNewStrictArguments(Node* context,
                                                             Node* function) {
  VARIABLE(result, MachineRepresentation::kTagged);
  Label done(this, &result), empty(this), runtime(this, Label::kDeferred);

  ParameterMode mode = OptimalParameterMode();
  Node* zero = IntPtrOrSmiConstant(0, mode);

  TorqueStructArgumentsInfo info = GetArgumentsFrameAndCount(
      CAST(context), UncheckedCast<JSFunction>(function));

  GotoIfFixedArraySizeDoesntFitInNewSpace(
      info.argument_count, &runtime,
      JSStrictArgumentsObject::kSize + FixedArray::kHeaderSize, mode);

  Node* const native_context = LoadNativeContext(context);
  Node* const map =
      LoadContextElement(native_context, Context::STRICT_ARGUMENTS_MAP_INDEX);
  GotoIf(WordEqual(info.argument_count, zero), &empty);

  result.Bind(ConstructParametersObjectFromArgs(
      map, info.frame, info.argument_count, zero, info.argument_count, mode,
      JSStrictArgumentsObject::kSize));
  Goto(&done);

  BIND(&empty);
  {
    Node* arguments;
    Node* elements;
    Node* unused;
    std::tie(arguments, elements, unused) = AllocateArgumentsObject(
        map, zero, nullptr, mode, JSStrictArgumentsObject::kSize);
    result.Bind(arguments);
    Goto(&done);
  }

  BIND(&runtime);
  {
    result.Bind(CallRuntime(Runtime::kNewStrictArguments, context, function));
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

Node* ArgumentsBuiltinsAssembler::EmitFastNewSloppyArguments(Node* context,
                                                             Node* function) {
  VARIABLE(result, MachineRepresentation::kTagged);

  ParameterMode mode = OptimalParameterMode();
  Node* zero = IntPtrOrSmiConstant(0, mode);

  Label done(this, &result), empty(this), no_parameters(this),
      runtime(this, Label::kDeferred);

  TorqueStructArgumentsInfo info = GetArgumentsFrameAndCount(
      CAST(context), UncheckedCast<JSFunction>(function));

  GotoIf(WordEqual(info.argument_count, zero), &empty);

  GotoIf(WordEqual(info.formal_parameter_count, zero), &no_parameters);

  {
    Comment("Mapped parameter JSSloppyArgumentsObject");

    Node* mapped_count =
        IntPtrOrSmiMin(info.argument_count, info.formal_parameter_count, mode);

    Node* parameter_map_size =
        IntPtrOrSmiAdd(mapped_count, IntPtrOrSmiConstant(2, mode), mode);

    // Verify that the overall allocation will fit in new space.
    Node* elements_allocated =
        IntPtrOrSmiAdd(info.argument_count, parameter_map_size, mode);
    GotoIfFixedArraySizeDoesntFitInNewSpace(
        elements_allocated, &runtime,
        JSSloppyArgumentsObject::kSize + FixedArray::kHeaderSize * 2, mode);

    Node* const native_context = LoadNativeContext(context);
    Node* const map = LoadContextElement(
        native_context, Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX);
    Node* argument_object;
    Node* elements;
    Node* map_array;
    std::tie(argument_object, elements, map_array) =
        AllocateArgumentsObject(map, info.argument_count, parameter_map_size,
                                mode, JSSloppyArgumentsObject::kSize);
    StoreObjectFieldNoWriteBarrier(
        argument_object, JSSloppyArgumentsObject::kCalleeOffset, function);
    StoreFixedArrayElement(CAST(map_array), 0, context, SKIP_WRITE_BARRIER);
    StoreFixedArrayElement(CAST(map_array), 1, elements, SKIP_WRITE_BARRIER);

    Comment("Fill in non-mapped parameters");
    Node* argument_offset =
        ElementOffsetFromIndex(info.argument_count, PACKED_ELEMENTS, mode,
                               FixedArray::kHeaderSize - kHeapObjectTag);
    Node* mapped_offset =
        ElementOffsetFromIndex(mapped_count, PACKED_ELEMENTS, mode,
                               FixedArray::kHeaderSize - kHeapObjectTag);
    CodeStubArguments arguments(this, info.argument_count, info.frame, mode);
    VARIABLE(current_argument, MachineType::PointerRepresentation());
    current_argument.Bind(arguments.AtIndexPtr(info.argument_count, mode));
    VariableList var_list1({&current_argument}, zone());
    mapped_offset = BuildFastLoop(
        var_list1, argument_offset, mapped_offset,
        [this, elements, &current_argument](Node* offset) {
          Increment(&current_argument, kSystemPointerSize);
          Node* arg = LoadBufferObject(current_argument.value(), 0);
          StoreNoWriteBarrier(MachineRepresentation::kTagged, elements, offset,
                              arg);
        },
        -kTaggedSize, INTPTR_PARAMETERS);

    // Copy the parameter slots and the holes in the arguments.
    // We need to fill in mapped_count slots. They index the context,
    // where parameters are stored in reverse order, at
    //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+argument_count-1
    // The mapped parameter thus need to get indices
    //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
    //       MIN_CONTEXT_SLOTS+argument_count-mapped_count
    // We loop from right to left.
    Comment("Fill in mapped parameters");
    VARIABLE(context_index, OptimalParameterRepresentation());
    context_index.Bind(IntPtrOrSmiSub(
        IntPtrOrSmiAdd(IntPtrOrSmiConstant(Context::MIN_CONTEXT_SLOTS, mode),
                       info.formal_parameter_count, mode),
        mapped_count, mode));
    Node* the_hole = TheHoleConstant();
    VariableList var_list2({&context_index}, zone());
    const int kParameterMapHeaderSize = FixedArray::OffsetOfElementAt(2);
    Node* adjusted_map_array = IntPtrAdd(
        BitcastTaggedToWord(map_array),
        IntPtrConstant(kParameterMapHeaderSize - FixedArray::kHeaderSize));
    Node* zero_offset = ElementOffsetFromIndex(
        zero, PACKED_ELEMENTS, mode, FixedArray::kHeaderSize - kHeapObjectTag);
    BuildFastLoop(
        var_list2, mapped_offset, zero_offset,
        [=, &context_index](Node* offset) {
          StoreNoWriteBarrier(MachineRepresentation::kTagged, elements, offset,
                              the_hole);
          StoreNoWriteBarrier(MachineRepresentation::kTagged,
                              adjusted_map_array, offset,
                              ParameterToTagged(context_index.value(), mode));
          Increment(&context_index, 1, mode);
        },
        -kTaggedSize, INTPTR_PARAMETERS);

    result.Bind(argument_object);
    Goto(&done);
  }

  BIND(&no_parameters);
  {
    Comment("No parameters JSSloppyArgumentsObject");
    GotoIfFixedArraySizeDoesntFitInNewSpace(
        info.argument_count, &runtime,
        JSSloppyArgumentsObject::kSize + FixedArray::kHeaderSize, mode);
    Node* const native_context = LoadNativeContext(context);
    Node* const map =
        LoadContextElement(native_context, Context::SLOPPY_ARGUMENTS_MAP_INDEX);
    result.Bind(ConstructParametersObjectFromArgs(
        map, info.frame, info.argument_count, zero, info.argument_count, mode,
        JSSloppyArgumentsObject::kSize));
    StoreObjectFieldNoWriteBarrier(
        result.value(), JSSloppyArgumentsObject::kCalleeOffset, function);
    Goto(&done);
  }

  BIND(&empty);
  {
    Comment("Empty JSSloppyArgumentsObject");
    Node* const native_context = LoadNativeContext(context);
    Node* const map =
        LoadContextElement(native_context, Context::SLOPPY_ARGUMENTS_MAP_INDEX);
    Node* arguments;
    Node* elements;
    Node* unused;
    std::tie(arguments, elements, unused) = AllocateArgumentsObject(
        map, zero, nullptr, mode, JSSloppyArgumentsObject::kSize);
    result.Bind(arguments);
    StoreObjectFieldNoWriteBarrier(
        result.value(), JSSloppyArgumentsObject::kCalleeOffset, function);
    Goto(&done);
  }

  BIND(&runtime);
  {
    result.Bind(CallRuntime(Runtime::kNewSloppyArguments, context, function));
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

}  // namespace internal
}  // namespace v8
