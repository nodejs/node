// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-call-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/globals.h"
#include "src/heap/heap-inl.h"
#include "src/isolate.h"
#include "src/macro-assembler.h"
#include "src/objects/arguments.h"

namespace v8 {
namespace internal {

void Builtins::Generate_CallFunction_ReceiverIsNullOrUndefined(
    MacroAssembler* masm) {
  Generate_CallFunction(masm, ConvertReceiverMode::kNullOrUndefined);
}

void Builtins::Generate_CallFunction_ReceiverIsNotNullOrUndefined(
    MacroAssembler* masm) {
  Generate_CallFunction(masm, ConvertReceiverMode::kNotNullOrUndefined);
}

void Builtins::Generate_CallFunction_ReceiverIsAny(MacroAssembler* masm) {
  Generate_CallFunction(masm, ConvertReceiverMode::kAny);
}

void Builtins::Generate_CallBoundFunction(MacroAssembler* masm) {
  Generate_CallBoundFunctionImpl(masm);
}

void Builtins::Generate_Call_ReceiverIsNullOrUndefined(MacroAssembler* masm) {
  Generate_Call(masm, ConvertReceiverMode::kNullOrUndefined);
}

void Builtins::Generate_Call_ReceiverIsNotNullOrUndefined(
    MacroAssembler* masm) {
  Generate_Call(masm, ConvertReceiverMode::kNotNullOrUndefined);
}

void Builtins::Generate_Call_ReceiverIsAny(MacroAssembler* masm) {
  Generate_Call(masm, ConvertReceiverMode::kAny);
}

void Builtins::Generate_CallVarargs(MacroAssembler* masm) {
  Generate_CallOrConstructVarargs(masm, masm->isolate()->builtins()->Call());
}

void Builtins::Generate_CallForwardVarargs(MacroAssembler* masm) {
  Generate_CallOrConstructForwardVarargs(masm, CallOrConstructMode::kCall,
                                         masm->isolate()->builtins()->Call());
}

void Builtins::Generate_CallFunctionForwardVarargs(MacroAssembler* masm) {
  Generate_CallOrConstructForwardVarargs(
      masm, CallOrConstructMode::kCall,
      masm->isolate()->builtins()->CallFunction());
}

void CallOrConstructBuiltinsAssembler::CallOrConstructWithArrayLike(
    Node* target, Node* new_target, Node* arguments_list, Node* context) {
  VARIABLE(var_elements, MachineRepresentation::kTagged);
  VARIABLE(var_length, MachineRepresentation::kWord32);
  Label if_done(this), if_arguments(this), if_array(this),
      if_holey_array(this, Label::kDeferred),
      if_runtime(this, Label::kDeferred);

  // Perform appropriate checks on {target} (and {new_target} first).
  if (new_target == nullptr) {
    // Check that {target} is Callable.
    Label if_target_callable(this),
        if_target_not_callable(this, Label::kDeferred);
    GotoIf(TaggedIsSmi(target), &if_target_not_callable);
    Branch(IsCallable(target), &if_target_callable, &if_target_not_callable);
    BIND(&if_target_not_callable);
    {
      CallRuntime(Runtime::kThrowApplyNonFunction, context, target);
      Unreachable();
    }
    BIND(&if_target_callable);
  } else {
    // Check that {target} is a Constructor.
    Label if_target_constructor(this),
        if_target_not_constructor(this, Label::kDeferred);
    GotoIf(TaggedIsSmi(target), &if_target_not_constructor);
    Branch(IsConstructor(target), &if_target_constructor,
           &if_target_not_constructor);
    BIND(&if_target_not_constructor);
    {
      CallRuntime(Runtime::kThrowNotConstructor, context, target);
      Unreachable();
    }
    BIND(&if_target_constructor);

    // Check that {new_target} is a Constructor.
    Label if_new_target_constructor(this),
        if_new_target_not_constructor(this, Label::kDeferred);
    GotoIf(TaggedIsSmi(new_target), &if_new_target_not_constructor);
    Branch(IsConstructor(new_target), &if_new_target_constructor,
           &if_new_target_not_constructor);
    BIND(&if_new_target_not_constructor);
    {
      CallRuntime(Runtime::kThrowNotConstructor, context, new_target);
      Unreachable();
    }
    BIND(&if_new_target_constructor);
  }

  GotoIf(TaggedIsSmi(arguments_list), &if_runtime);
  Node* arguments_list_map = LoadMap(arguments_list);
  Node* native_context = LoadNativeContext(context);

  // Check if {arguments_list} is an (unmodified) arguments object.
  Node* sloppy_arguments_map =
      LoadContextElement(native_context, Context::SLOPPY_ARGUMENTS_MAP_INDEX);
  GotoIf(WordEqual(arguments_list_map, sloppy_arguments_map), &if_arguments);
  Node* strict_arguments_map =
      LoadContextElement(native_context, Context::STRICT_ARGUMENTS_MAP_INDEX);
  GotoIf(WordEqual(arguments_list_map, strict_arguments_map), &if_arguments);

  // Check if {arguments_list} is a fast JSArray.
  Branch(IsJSArrayMap(arguments_list_map), &if_array, &if_runtime);

  BIND(&if_array);
  {
    // Try to extract the elements from a JSArray object.
    var_elements.Bind(
        LoadObjectField(arguments_list, JSArray::kElementsOffset));
    var_length.Bind(LoadAndUntagToWord32ObjectField(arguments_list,
                                                    JSArray::kLengthOffset));

    // Holey arrays and double backing stores need special treatment.
    STATIC_ASSERT(PACKED_SMI_ELEMENTS == 0);
    STATIC_ASSERT(HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(PACKED_ELEMENTS == 2);
    STATIC_ASSERT(HOLEY_ELEMENTS == 3);
    STATIC_ASSERT(PACKED_DOUBLE_ELEMENTS == 4);
    STATIC_ASSERT(HOLEY_DOUBLE_ELEMENTS == 5);
    STATIC_ASSERT(LAST_FAST_ELEMENTS_KIND == HOLEY_DOUBLE_ELEMENTS);

    Node* kind = LoadMapElementsKind(arguments_list_map);

    GotoIf(Int32GreaterThan(kind, Int32Constant(LAST_FAST_ELEMENTS_KIND)),
           &if_runtime);
    Branch(Word32And(kind, Int32Constant(1)), &if_holey_array, &if_done);
  }

  BIND(&if_holey_array);
  {
    // For holey JSArrays we need to check that the array prototype chain
    // protector is intact and our prototype is the Array.prototype actually.
    GotoIfNot(IsPrototypeInitialArrayPrototype(context, arguments_list_map),
              &if_runtime);
    Branch(IsNoElementsProtectorCellInvalid(), &if_runtime, &if_done);
  }

  BIND(&if_arguments);
  {
    // Try to extract the elements from an JSArgumentsObject.
    Node* length =
        LoadObjectField(arguments_list, JSArgumentsObject::kLengthOffset);
    Node* elements =
        LoadObjectField(arguments_list, JSArgumentsObject::kElementsOffset);
    Node* elements_length = LoadFixedArrayBaseLength(elements);
    GotoIfNot(WordEqual(length, elements_length), &if_runtime);
    var_elements.Bind(elements);
    var_length.Bind(SmiToInt32(length));
    Goto(&if_done);
  }

  BIND(&if_runtime);
  {
    // Ask the runtime to create the list (actually a FixedArray).
    Node* elements =
        CallRuntime(Runtime::kCreateListFromArrayLike, context, arguments_list);
    var_elements.Bind(elements);
    var_length.Bind(
        LoadAndUntagToWord32ObjectField(elements, FixedArray::kLengthOffset));
    Goto(&if_done);
  }

  // Tail call to the appropriate builtin (depending on whether we have
  // a {new_target} passed).
  BIND(&if_done);
  {
    Label if_not_double(this), if_double(this);
    Node* elements = var_elements.value();
    Node* length = var_length.value();
    Node* args_count = Int32Constant(0);  // args already on the stack

    Branch(IsFixedDoubleArray(elements), &if_double, &if_not_double);

    BIND(&if_not_double);
    if (new_target == nullptr) {
      Callable callable = CodeFactory::CallVarargs(isolate());
      TailCallStub(callable, context, target, args_count, elements, length);
    } else {
      Callable callable = CodeFactory::ConstructVarargs(isolate());
      TailCallStub(callable, context, target, new_target, args_count, elements,
                   length);
    }

    BIND(&if_double);
    {
      // Kind is hardcoded here because CreateListFromArrayLike will only
      // produce holey double arrays.
      CallOrConstructDoubleVarargs(target, new_target, elements, length,
                                   args_count, context,
                                   Int32Constant(HOLEY_DOUBLE_ELEMENTS));
    }
  }
}

// Takes a FixedArray of doubles and creates a new FixedArray with those doubles
// boxed as HeapNumbers, then tail calls CallVarargs/ConstructVarargs depending
// on whether {new_target} was passed.
void CallOrConstructBuiltinsAssembler::CallOrConstructDoubleVarargs(
    Node* target, Node* new_target, Node* elements, Node* length,
    Node* args_count, Node* context, Node* kind) {
  Label if_holey_double(this), if_packed_double(this), if_done(this);

  const ElementsKind new_kind = PACKED_ELEMENTS;
  const ParameterMode mode = INTPTR_PARAMETERS;
  const WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER;
  Node* intptr_length = ChangeInt32ToIntPtr(length);

  // Allocate a new FixedArray of Objects.
  Node* new_elements =
      AllocateFixedArray(new_kind, intptr_length, mode,
                         CodeStubAssembler::kAllowLargeObjectAllocation);
  Branch(Word32Equal(kind, Int32Constant(HOLEY_DOUBLE_ELEMENTS)),
         &if_holey_double, &if_packed_double);

  BIND(&if_holey_double);
  {
    // Fill the FixedArray with pointers to HeapObjects.
    CopyFixedArrayElements(HOLEY_DOUBLE_ELEMENTS, elements, new_kind,
                           new_elements, intptr_length, intptr_length,
                           barrier_mode);
    Goto(&if_done);
  }

  BIND(&if_packed_double);
  {
    CopyFixedArrayElements(PACKED_DOUBLE_ELEMENTS, elements, new_kind,
                           new_elements, intptr_length, intptr_length,
                           barrier_mode);
    Goto(&if_done);
  }

  BIND(&if_done);
  {
    if (new_target == nullptr) {
      Callable callable = CodeFactory::CallVarargs(isolate());
      TailCallStub(callable, context, target, args_count, new_elements, length);
    } else {
      Callable callable = CodeFactory::ConstructVarargs(isolate());
      TailCallStub(callable, context, target, new_target, args_count,
                   new_elements, length);
    }
  }
}

void CallOrConstructBuiltinsAssembler::CallOrConstructWithSpread(
    Node* target, Node* new_target, Node* spread, Node* args_count,
    Node* context) {
  Label if_smiorobject(this), if_double(this),
      if_generic(this, Label::kDeferred);

  VARIABLE(var_length, MachineRepresentation::kWord32);
  VARIABLE(var_elements, MachineRepresentation::kTagged);
  VARIABLE(var_elements_kind, MachineRepresentation::kWord32);

  GotoIf(TaggedIsSmi(spread), &if_generic);
  Node* spread_map = LoadMap(spread);
  GotoIfNot(IsJSArrayMap(spread_map), &if_generic);

  // Check that we have the original Array.prototype.
  GotoIfNot(IsPrototypeInitialArrayPrototype(context, spread_map), &if_generic);

  // Check that there are no elements on the Array.prototype chain.
  GotoIf(IsNoElementsProtectorCellInvalid(), &if_generic);

  // Check that the Array.prototype hasn't been modified in a way that would
  // affect iteration.
  Node* protector_cell = LoadRoot(Heap::kArrayIteratorProtectorRootIndex);
  DCHECK(isolate()->heap()->array_iterator_protector()->IsPropertyCell());
  GotoIf(WordEqual(LoadObjectField(protector_cell, PropertyCell::kValueOffset),
                   SmiConstant(Isolate::kProtectorInvalid)),
         &if_generic);

  // The fast-path accesses the {spread} elements directly.
  Node* spread_kind = LoadMapElementsKind(spread_map);
  var_elements_kind.Bind(spread_kind);
  var_length.Bind(
      LoadAndUntagToWord32ObjectField(spread, JSArray::kLengthOffset));
  var_elements.Bind(LoadObjectField(spread, JSArray::kElementsOffset));

  // Check elements kind of {spread}.
  GotoIf(Int32LessThan(spread_kind, Int32Constant(PACKED_DOUBLE_ELEMENTS)),
         &if_smiorobject);
  Branch(Int32GreaterThan(spread_kind, Int32Constant(LAST_FAST_ELEMENTS_KIND)),
         &if_generic, &if_double);

  BIND(&if_generic);
  {
    Label if_iterator_fn_not_callable(this, Label::kDeferred);
    Node* iterator_fn = GetProperty(context, spread, IteratorSymbolConstant());
    GotoIf(TaggedIsSmi(iterator_fn), &if_iterator_fn_not_callable);
    GotoIfNot(IsCallable(iterator_fn), &if_iterator_fn_not_callable);
    Node* list =
        CallBuiltin(Builtins::kIterableToList, context, spread, iterator_fn);
    CSA_ASSERT(this, IsJSArray(list));
    Node* list_kind = LoadMapElementsKind(LoadMap(list));
    var_length.Bind(
        LoadAndUntagToWord32ObjectField(list, JSArray::kLengthOffset));
    var_elements.Bind(LoadObjectField(list, JSArray::kElementsOffset));
    var_elements_kind.Bind(list_kind);
    Branch(Int32LessThan(list_kind, Int32Constant(PACKED_DOUBLE_ELEMENTS)),
           &if_smiorobject, &if_double);

    BIND(&if_iterator_fn_not_callable);
    ThrowTypeError(context, MessageTemplate::kIteratorSymbolNonCallable);
  }

  BIND(&if_smiorobject);
  {
    Node* const elements = var_elements.value();
    Node* const length = var_length.value();

    if (new_target == nullptr) {
      Callable callable = CodeFactory::CallVarargs(isolate());
      TailCallStub(callable, context, target, args_count, elements, length);
    } else {
      Callable callable = CodeFactory::ConstructVarargs(isolate());
      TailCallStub(callable, context, target, new_target, args_count, elements,
                   length);
    }
  }

  BIND(&if_double);
  {
    Node* const elements_kind = var_elements_kind.value();
    Node* const elements = var_elements.value();
    Node* const length = var_length.value();

    CallOrConstructDoubleVarargs(target, new_target, elements, length,
                                 args_count, context, elements_kind);
  }
}

TF_BUILTIN(CallWithArrayLike, CallOrConstructBuiltinsAssembler) {
  Node* target = Parameter(CallWithArrayLikeDescriptor::kTarget);
  Node* new_target = nullptr;
  Node* arguments_list = Parameter(CallWithArrayLikeDescriptor::kArgumentsList);
  Node* context = Parameter(CallWithArrayLikeDescriptor::kContext);
  CallOrConstructWithArrayLike(target, new_target, arguments_list, context);
}

TF_BUILTIN(CallWithSpread, CallOrConstructBuiltinsAssembler) {
  Node* target = Parameter(CallWithSpreadDescriptor::kTarget);
  Node* new_target = nullptr;
  Node* spread = Parameter(CallWithSpreadDescriptor::kSpread);
  Node* args_count = Parameter(CallWithSpreadDescriptor::kArgumentsCount);
  Node* context = Parameter(CallWithSpreadDescriptor::kContext);
  CallOrConstructWithSpread(target, new_target, spread, args_count, context);
}

}  // namespace internal
}  // namespace v8
