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
#ifdef V8_TARGET_ARCH_IA32
  Assembler::SupportsRootRegisterScope supports_root_register(masm);
#endif
  Generate_CallFunction(masm, ConvertReceiverMode::kNullOrUndefined);
}

void Builtins::Generate_CallFunction_ReceiverIsNotNullOrUndefined(
    MacroAssembler* masm) {
#ifdef V8_TARGET_ARCH_IA32
  Assembler::SupportsRootRegisterScope supports_root_register(masm);
#endif
  Generate_CallFunction(masm, ConvertReceiverMode::kNotNullOrUndefined);
}

void Builtins::Generate_CallFunction_ReceiverIsAny(MacroAssembler* masm) {
#ifdef V8_TARGET_ARCH_IA32
  Assembler::SupportsRootRegisterScope supports_root_register(masm);
#endif
  Generate_CallFunction(masm, ConvertReceiverMode::kAny);
}

void Builtins::Generate_CallBoundFunction(MacroAssembler* masm) {
#ifdef V8_TARGET_ARCH_IA32
  Assembler::SupportsRootRegisterScope supports_root_register(masm);
#endif
  Generate_CallBoundFunctionImpl(masm);
}

void Builtins::Generate_Call_ReceiverIsNullOrUndefined(MacroAssembler* masm) {
#ifdef V8_TARGET_ARCH_IA32
  Assembler::SupportsRootRegisterScope supports_root_register(masm);
#endif
  Generate_Call(masm, ConvertReceiverMode::kNullOrUndefined);
}

void Builtins::Generate_Call_ReceiverIsNotNullOrUndefined(
    MacroAssembler* masm) {
#ifdef V8_TARGET_ARCH_IA32
  Assembler::SupportsRootRegisterScope supports_root_register(masm);
#endif
  Generate_Call(masm, ConvertReceiverMode::kNotNullOrUndefined);
}

void Builtins::Generate_Call_ReceiverIsAny(MacroAssembler* masm) {
#ifdef V8_TARGET_ARCH_IA32
  Assembler::SupportsRootRegisterScope supports_root_register(masm);
#endif
  Generate_Call(masm, ConvertReceiverMode::kAny);
}

void Builtins::Generate_CallVarargs(MacroAssembler* masm) {
#ifdef V8_TARGET_ARCH_IA32
  Assembler::SupportsRootRegisterScope supports_root_register(masm);
#endif
  Generate_CallOrConstructVarargs(masm, masm->isolate()->builtins()->Call());
}

void Builtins::Generate_CallForwardVarargs(MacroAssembler* masm) {
#ifdef V8_TARGET_ARCH_IA32
  Assembler::SupportsRootRegisterScope supports_root_register(masm);
#endif
  Generate_CallOrConstructForwardVarargs(masm, CallOrConstructMode::kCall,
                                         masm->isolate()->builtins()->Call());
}

void Builtins::Generate_CallFunctionForwardVarargs(MacroAssembler* masm) {
#ifdef V8_TARGET_ARCH_IA32
  Assembler::SupportsRootRegisterScope supports_root_register(masm);
#endif
  Generate_CallOrConstructForwardVarargs(
      masm, CallOrConstructMode::kCall,
      masm->isolate()->builtins()->CallFunction());
}

void CallOrConstructBuiltinsAssembler::CallOrConstructWithArrayLike(
    TNode<Object> target, SloppyTNode<Object> new_target,
    TNode<Object> arguments_list, TNode<Context> context) {
  Label if_done(this), if_arguments(this), if_array(this),
      if_holey_array(this, Label::kDeferred),
      if_runtime(this, Label::kDeferred);

  // Perform appropriate checks on {target} (and {new_target} first).
  if (new_target == nullptr) {
    // Check that {target} is Callable.
    Label if_target_callable(this),
        if_target_not_callable(this, Label::kDeferred);
    GotoIf(TaggedIsSmi(target), &if_target_not_callable);
    Branch(IsCallable(CAST(target)), &if_target_callable,
           &if_target_not_callable);
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
    Branch(IsConstructor(CAST(target)), &if_target_constructor,
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
    Branch(IsConstructor(CAST(new_target)), &if_new_target_constructor,
           &if_new_target_not_constructor);
    BIND(&if_new_target_not_constructor);
    {
      CallRuntime(Runtime::kThrowNotConstructor, context, new_target);
      Unreachable();
    }
    BIND(&if_new_target_constructor);
  }

  GotoIf(TaggedIsSmi(arguments_list), &if_runtime);

  TNode<Map> arguments_list_map = LoadMap(CAST(arguments_list));
  TNode<Context> native_context = LoadNativeContext(context);

  // Check if {arguments_list} is an (unmodified) arguments object.
  TNode<Map> sloppy_arguments_map = CAST(
      LoadContextElement(native_context, Context::SLOPPY_ARGUMENTS_MAP_INDEX));
  GotoIf(WordEqual(arguments_list_map, sloppy_arguments_map), &if_arguments);
  TNode<Map> strict_arguments_map = CAST(
      LoadContextElement(native_context, Context::STRICT_ARGUMENTS_MAP_INDEX));
  GotoIf(WordEqual(arguments_list_map, strict_arguments_map), &if_arguments);

  // Check if {arguments_list} is a fast JSArray.
  Branch(IsJSArrayMap(arguments_list_map), &if_array, &if_runtime);

  TVARIABLE(FixedArrayBase, var_elements);
  TVARIABLE(Int32T, var_length);
  BIND(&if_array);
  {
    // Try to extract the elements from a JSArray object.
    var_elements = LoadElements(CAST(arguments_list));
    var_length =
        LoadAndUntagToWord32ObjectField(arguments_list, JSArray::kLengthOffset);

    // Holey arrays and double backing stores need special treatment.
    STATIC_ASSERT(PACKED_SMI_ELEMENTS == 0);
    STATIC_ASSERT(HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(PACKED_ELEMENTS == 2);
    STATIC_ASSERT(HOLEY_ELEMENTS == 3);
    STATIC_ASSERT(PACKED_DOUBLE_ELEMENTS == 4);
    STATIC_ASSERT(HOLEY_DOUBLE_ELEMENTS == 5);
    STATIC_ASSERT(LAST_FAST_ELEMENTS_KIND == HOLEY_DOUBLE_ELEMENTS);

    TNode<Int32T> kind = LoadMapElementsKind(arguments_list_map);

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
    TNode<JSArgumentsObject> js_arguments = CAST(arguments_list);
    // Try to extract the elements from an JSArgumentsObjectWithLength.
    TNode<Object> length = LoadObjectField(
        js_arguments, JSArgumentsObjectWithLength::kLengthOffset);
    TNode<FixedArrayBase> elements = LoadElements(js_arguments);
    TNode<Smi> elements_length = LoadFixedArrayBaseLength(elements);
    GotoIfNot(WordEqual(length, elements_length), &if_runtime);
    var_elements = elements;
    var_length = SmiToInt32(CAST(length));
    Goto(&if_done);
  }

  BIND(&if_runtime);
  {
    // Ask the runtime to create the list (actually a FixedArray).
    var_elements = CAST(CallRuntime(Runtime::kCreateListFromArrayLike, context,
                                    arguments_list));
    var_length = LoadAndUntagToWord32ObjectField(var_elements.value(),
                                                 FixedArray::kLengthOffset);
    Goto(&if_done);
  }

  // Tail call to the appropriate builtin (depending on whether we have
  // a {new_target} passed).
  BIND(&if_done);
  {
    Label if_not_double(this), if_double(this);
    TNode<Int32T> args_count = Int32Constant(0);  // args already on the stack

    TNode<Int32T> length = var_length.value();
    {
      Label normalize_done(this);
      GotoIfNot(Word32Equal(length, Int32Constant(0)), &normalize_done);
      // Make sure we don't accidentally pass along the
      // empty_fixed_double_array since the tailed-called stubs cannot handle
      // the normalization yet.
      var_elements = EmptyFixedArrayConstant();
      Goto(&normalize_done);

      BIND(&normalize_done);
    }

    TNode<FixedArrayBase> elements = var_elements.value();
    Branch(IsFixedDoubleArray(elements), &if_double, &if_not_double);

    BIND(&if_not_double);
    {
      if (new_target == nullptr) {
        Callable callable = CodeFactory::CallVarargs(isolate());
        TailCallStub(callable, context, target, args_count, length, elements);
      } else {
        Callable callable = CodeFactory::ConstructVarargs(isolate());
        TailCallStub(callable, context, target, new_target, args_count, length,
                     elements);
      }
    }

    BIND(&if_double);
    {
      // Kind is hardcoded here because CreateListFromArrayLike will only
      // produce holey double arrays.
      CallOrConstructDoubleVarargs(target, new_target, CAST(elements), length,
                                   args_count, context,
                                   Int32Constant(HOLEY_DOUBLE_ELEMENTS));
    }
  }
}

// Takes a FixedArray of doubles and creates a new FixedArray with those doubles
// boxed as HeapNumbers, then tail calls CallVarargs/ConstructVarargs depending
// on whether {new_target} was passed.
void CallOrConstructBuiltinsAssembler::CallOrConstructDoubleVarargs(
    TNode<Object> target, SloppyTNode<Object> new_target,
    TNode<FixedDoubleArray> elements, TNode<Int32T> length,
    TNode<Int32T> args_count, TNode<Context> context, TNode<Int32T> kind) {
  Label if_done(this);

  const ElementsKind new_kind = PACKED_ELEMENTS;
  const WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER;
  TNode<IntPtrT> intptr_length = ChangeInt32ToIntPtr(length);
  CSA_ASSERT(this, WordNotEqual(intptr_length, IntPtrConstant(0)));

  // Allocate a new FixedArray of Objects.
  TNode<FixedArray> new_elements = CAST(AllocateFixedArray(
      new_kind, intptr_length, CodeStubAssembler::kAllowLargeObjectAllocation));
  Branch(Word32Equal(kind, Int32Constant(HOLEY_DOUBLE_ELEMENTS)),
         [&] {
           // Fill the FixedArray with pointers to HeapObjects.
           CopyFixedArrayElements(HOLEY_DOUBLE_ELEMENTS, elements, new_kind,
                                  new_elements, intptr_length, intptr_length,
                                  barrier_mode);
           Goto(&if_done);
         },
         [&] {
           CopyFixedArrayElements(PACKED_DOUBLE_ELEMENTS, elements, new_kind,
                                  new_elements, intptr_length, intptr_length,
                                  barrier_mode);
           Goto(&if_done);
         });

  BIND(&if_done);
  {
    if (new_target == nullptr) {
      Callable callable = CodeFactory::CallVarargs(isolate());
      TailCallStub(callable, context, target, args_count, length, new_elements);
    } else {
      Callable callable = CodeFactory::ConstructVarargs(isolate());
      TailCallStub(callable, context, target, new_target, args_count, length,
                   new_elements);
    }
  }
}

void CallOrConstructBuiltinsAssembler::CallOrConstructWithSpread(
    TNode<Object> target, TNode<Object> new_target, TNode<Object> spread,
    TNode<Int32T> args_count, TNode<Context> context) {
  Label if_smiorobject(this), if_double(this),
      if_generic(this, Label::kDeferred);

  TVARIABLE(Int32T, var_length);
  TVARIABLE(FixedArrayBase, var_elements);
  TVARIABLE(Int32T, var_elements_kind);

  GotoIf(TaggedIsSmi(spread), &if_generic);
  TNode<Map> spread_map = LoadMap(CAST(spread));
  GotoIfNot(IsJSArrayMap(spread_map), &if_generic);
  TNode<JSArray> spread_array = CAST(spread);

  // Check that we have the original Array.prototype.
  GotoIfNot(IsPrototypeInitialArrayPrototype(context, spread_map), &if_generic);

  // Check that there are no elements on the Array.prototype chain.
  GotoIf(IsNoElementsProtectorCellInvalid(), &if_generic);

  // Check that the Array.prototype hasn't been modified in a way that would
  // affect iteration.
  TNode<PropertyCell> protector_cell =
      CAST(LoadRoot(RootIndex::kArrayIteratorProtector));
  GotoIf(WordEqual(LoadObjectField(protector_cell, PropertyCell::kValueOffset),
                   SmiConstant(Isolate::kProtectorInvalid)),
         &if_generic);
  {
    // The fast-path accesses the {spread} elements directly.
    TNode<Int32T> spread_kind = LoadMapElementsKind(spread_map);
    var_elements_kind = spread_kind;
    var_length =
        LoadAndUntagToWord32ObjectField(spread_array, JSArray::kLengthOffset);
    var_elements = LoadElements(spread_array);

    // Check elements kind of {spread}.
    GotoIf(Int32LessThan(spread_kind, Int32Constant(PACKED_DOUBLE_ELEMENTS)),
           &if_smiorobject);
    Branch(
        Int32GreaterThan(spread_kind, Int32Constant(LAST_FAST_ELEMENTS_KIND)),
        &if_generic, &if_double);
  }

  BIND(&if_generic);
  {
    Label if_iterator_fn_not_callable(this, Label::kDeferred);
    TNode<Object> iterator_fn =
        GetProperty(context, spread, IteratorSymbolConstant());
    GotoIfNot(TaggedIsCallable(iterator_fn), &if_iterator_fn_not_callable);
    TNode<JSArray> list =
        CAST(CallBuiltin(Builtins::kIterableToListMayPreserveHoles, context,
                         spread, iterator_fn));
    var_length = LoadAndUntagToWord32ObjectField(list, JSArray::kLengthOffset);

    var_elements = LoadElements(list);
    var_elements_kind = LoadElementsKind(list);
    Branch(Int32LessThan(var_elements_kind.value(),
                         Int32Constant(PACKED_DOUBLE_ELEMENTS)),
           &if_smiorobject, &if_double);

    BIND(&if_iterator_fn_not_callable);
    ThrowTypeError(context, MessageTemplate::kIteratorSymbolNonCallable);
  }

  BIND(&if_smiorobject);
  {
    TNode<FixedArrayBase> elements = var_elements.value();
    TNode<Int32T> length = var_length.value();

    if (new_target == nullptr) {
      Callable callable = CodeFactory::CallVarargs(isolate());
      TailCallStub(callable, context, target, args_count, length, elements);
    } else {
      Callable callable = CodeFactory::ConstructVarargs(isolate());
      TailCallStub(callable, context, target, new_target, args_count, length,
                   elements);
    }
  }

  BIND(&if_double);
  {
    GotoIf(Word32Equal(var_length.value(), Int32Constant(0)), &if_smiorobject);
    CallOrConstructDoubleVarargs(target, new_target, CAST(var_elements.value()),
                                 var_length.value(), args_count, context,
                                 var_elements_kind.value());
  }
}

TF_BUILTIN(CallWithArrayLike, CallOrConstructBuiltinsAssembler) {
  TNode<Object> target = CAST(Parameter(Descriptor::kTarget));
  SloppyTNode<Object> new_target = nullptr;
  TNode<Object> arguments_list = CAST(Parameter(Descriptor::kArgumentsList));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  CallOrConstructWithArrayLike(target, new_target, arguments_list, context);
}

TF_BUILTIN(CallWithSpread, CallOrConstructBuiltinsAssembler) {
  TNode<Object> target = CAST(Parameter(Descriptor::kTarget));
  SloppyTNode<Object> new_target = nullptr;
  TNode<Object> spread = CAST(Parameter(Descriptor::kSpread));
  TNode<Int32T> args_count =
      UncheckedCast<Int32T>(Parameter(Descriptor::kArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  CallOrConstructWithSpread(target, new_target, spread, args_count, context);
}

}  // namespace internal
}  // namespace v8
