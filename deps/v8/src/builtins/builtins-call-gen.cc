// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-call-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/macro-assembler.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/execution/protectors.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/arguments.h"
#include "src/objects/property-cell.h"
#include "src/objects/templates.h"

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

TF_BUILTIN(Call_ReceiverIsNullOrUndefined_Baseline,
           CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kFunction);
  auto argc = UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto context = LoadContextFromBaseline();
  auto feedback_vector = LoadFeedbackVectorFromBaseline();
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);
  CollectCallFeedback(target, context, feedback_vector, slot);
  TailCallBuiltin(Builtins::kCall_ReceiverIsNullOrUndefined, context, target,
                  argc);
}

TF_BUILTIN(Call_ReceiverIsNotNullOrUndefined_Baseline,
           CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kFunction);
  auto argc = UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto context = LoadContextFromBaseline();
  auto feedback_vector = LoadFeedbackVectorFromBaseline();
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);
  CollectCallFeedback(target, context, feedback_vector, slot);
  TailCallBuiltin(Builtins::kCall_ReceiverIsNotNullOrUndefined, context, target,
                  argc);
}

TF_BUILTIN(Call_ReceiverIsAny_Baseline, CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kFunction);
  auto argc = UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto context = LoadContextFromBaseline();
  auto feedback_vector = LoadFeedbackVectorFromBaseline();
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);
  CollectCallFeedback(target, context, feedback_vector, slot);
  TailCallBuiltin(Builtins::kCall_ReceiverIsAny, context, target, argc);
}

TF_BUILTIN(Call_ReceiverIsNullOrUndefined_WithFeedback,
           CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kFunction);
  auto argc = UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto context = Parameter<Context>(Descriptor::kContext);
  auto feedback_vector = Parameter<FeedbackVector>(Descriptor::kFeedbackVector);
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);
  CollectCallFeedback(target, context, feedback_vector, slot);
  TailCallBuiltin(Builtins::kCall_ReceiverIsNullOrUndefined, context, target,
                  argc);
}

TF_BUILTIN(Call_ReceiverIsNotNullOrUndefined_WithFeedback,
           CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kFunction);
  auto argc = UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto context = Parameter<Context>(Descriptor::kContext);
  auto feedback_vector = Parameter<FeedbackVector>(Descriptor::kFeedbackVector);
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);
  CollectCallFeedback(target, context, feedback_vector, slot);
  TailCallBuiltin(Builtins::kCall_ReceiverIsNotNullOrUndefined, context, target,
                  argc);
}

TF_BUILTIN(Call_ReceiverIsAny_WithFeedback, CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kFunction);
  auto argc = UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto context = Parameter<Context>(Descriptor::kContext);
  auto feedback_vector = Parameter<FeedbackVector>(Descriptor::kFeedbackVector);
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);
  CollectCallFeedback(target, context, feedback_vector, slot);
  TailCallBuiltin(Builtins::kCall_ReceiverIsAny, context, target, argc);
}

void CallOrConstructBuiltinsAssembler::CallOrConstructWithArrayLike(
    TNode<Object> target, base::Optional<TNode<Object>> new_target,
    TNode<Object> arguments_list, TNode<Context> context) {
  Label if_done(this), if_arguments(this), if_array(this),
      if_holey_array(this, Label::kDeferred),
      if_runtime(this, Label::kDeferred);

  // Perform appropriate checks on {target} (and {new_target} first).
  if (!new_target) {
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
    GotoIf(TaggedIsSmi(*new_target), &if_new_target_not_constructor);
    Branch(IsConstructor(CAST(*new_target)), &if_new_target_constructor,
           &if_new_target_not_constructor);
    BIND(&if_new_target_not_constructor);
    {
      CallRuntime(Runtime::kThrowNotConstructor, context, *new_target);
      Unreachable();
    }
    BIND(&if_new_target_constructor);
  }

  GotoIf(TaggedIsSmi(arguments_list), &if_runtime);

  TNode<Map> arguments_list_map = LoadMap(CAST(arguments_list));
  TNode<NativeContext> native_context = LoadNativeContext(context);

  // Check if {arguments_list} is an (unmodified) arguments object.
  TNode<Map> sloppy_arguments_map = CAST(
      LoadContextElement(native_context, Context::SLOPPY_ARGUMENTS_MAP_INDEX));
  GotoIf(TaggedEqual(arguments_list_map, sloppy_arguments_map), &if_arguments);
  TNode<Map> strict_arguments_map = CAST(
      LoadContextElement(native_context, Context::STRICT_ARGUMENTS_MAP_INDEX));
  GotoIf(TaggedEqual(arguments_list_map, strict_arguments_map), &if_arguments);

  // Check if {arguments_list} is a fast JSArray.
  Branch(IsJSArrayMap(arguments_list_map), &if_array, &if_runtime);

  TVARIABLE(FixedArrayBase, var_elements);
  TVARIABLE(Int32T, var_length);
  BIND(&if_array);
  {
    TNode<Int32T> kind = LoadMapElementsKind(arguments_list_map);
    GotoIf(
        IsElementsKindGreaterThan(kind, LAST_ANY_NONEXTENSIBLE_ELEMENTS_KIND),
        &if_runtime);

    TNode<JSObject> js_object = CAST(arguments_list);
    // Try to extract the elements from a JSArray object.
    var_elements = LoadElements(js_object);
    var_length =
        LoadAndUntagToWord32ObjectField(js_object, JSArray::kLengthOffset);

    // Holey arrays and double backing stores need special treatment.
    STATIC_ASSERT(PACKED_SMI_ELEMENTS == 0);
    STATIC_ASSERT(HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(PACKED_ELEMENTS == 2);
    STATIC_ASSERT(HOLEY_ELEMENTS == 3);
    STATIC_ASSERT(PACKED_DOUBLE_ELEMENTS == 4);
    STATIC_ASSERT(HOLEY_DOUBLE_ELEMENTS == 5);
    STATIC_ASSERT(LAST_FAST_ELEMENTS_KIND == HOLEY_DOUBLE_ELEMENTS);

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
    // Try to extract the elements from a JSArgumentsObject with standard map.
    TNode<Object> length = LoadJSArgumentsObjectLength(context, js_arguments);
    TNode<FixedArrayBase> elements = LoadElements(js_arguments);
    TNode<Smi> elements_length = LoadFixedArrayBaseLength(elements);
    GotoIfNot(TaggedEqual(length, elements_length), &if_runtime);
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
      CSA_ASSERT(this, Int32LessThanOrEqual(
                           length, Int32Constant(FixedArray::kMaxLength)));
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
      if (!new_target) {
        Callable callable = CodeFactory::CallVarargs(isolate());
        TailCallStub(callable, context, target, args_count, length, elements);
      } else {
        Callable callable = CodeFactory::ConstructVarargs(isolate());
        TailCallStub(callable, context, target, *new_target, args_count, length,
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
    TNode<Object> target, base::Optional<TNode<Object>> new_target,
    TNode<FixedDoubleArray> elements, TNode<Int32T> length,
    TNode<Int32T> args_count, TNode<Context> context, TNode<Int32T> kind) {
  const ElementsKind new_kind = PACKED_ELEMENTS;
  const WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER;
  CSA_ASSERT(this, Int32LessThanOrEqual(length,
                                        Int32Constant(FixedArray::kMaxLength)));
  TNode<IntPtrT> intptr_length = ChangeInt32ToIntPtr(length);
  CSA_ASSERT(this, WordNotEqual(intptr_length, IntPtrConstant(0)));

  // Allocate a new FixedArray of Objects.
  TNode<FixedArray> new_elements = CAST(AllocateFixedArray(
      new_kind, intptr_length, CodeStubAssembler::kAllowLargeObjectAllocation));
  // CopyFixedArrayElements does not distinguish between holey and packed for
  // its first argument, so we don't need to dispatch on {kind} here.
  CopyFixedArrayElements(PACKED_DOUBLE_ELEMENTS, elements, new_kind,
                         new_elements, intptr_length, intptr_length,
                         barrier_mode);
  if (!new_target) {
    Callable callable = CodeFactory::CallVarargs(isolate());
    TailCallStub(callable, context, target, args_count, length, new_elements);
  } else {
    Callable callable = CodeFactory::ConstructVarargs(isolate());
    TailCallStub(callable, context, target, *new_target, args_count, length,
                 new_elements);
  }
}

void CallOrConstructBuiltinsAssembler::CallOrConstructWithSpread(
    TNode<Object> target, base::Optional<TNode<Object>> new_target,
    TNode<Object> spread, TNode<Int32T> args_count, TNode<Context> context) {
  Label if_smiorobject(this), if_double(this),
      if_generic(this, Label::kDeferred);

  TVARIABLE(JSArray, var_js_array);
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
  TNode<PropertyCell> protector_cell = ArrayIteratorProtectorConstant();
  GotoIf(
      TaggedEqual(LoadObjectField(protector_cell, PropertyCell::kValueOffset),
                  SmiConstant(Protectors::kProtectorInvalid)),
      &if_generic);
  {
    // The fast-path accesses the {spread} elements directly.
    TNode<Int32T> spread_kind = LoadMapElementsKind(spread_map);
    var_js_array = spread_array;
    var_elements_kind = spread_kind;
    var_elements = LoadElements(spread_array);

    // Check elements kind of {spread}.
    GotoIf(IsElementsKindLessThanOrEqual(spread_kind, HOLEY_ELEMENTS),
           &if_smiorobject);
    GotoIf(IsElementsKindLessThanOrEqual(spread_kind, LAST_FAST_ELEMENTS_KIND),
           &if_double);
    Branch(IsElementsKindLessThanOrEqual(spread_kind,
                                         LAST_ANY_NONEXTENSIBLE_ELEMENTS_KIND),
           &if_smiorobject, &if_generic);
  }

  BIND(&if_generic);
  {
    Label if_iterator_fn_not_callable(this, Label::kDeferred),
        if_iterator_is_null_or_undefined(this, Label::kDeferred),
        throw_spread_error(this, Label::kDeferred);
    TVARIABLE(Smi, message_id);

    GotoIf(IsNullOrUndefined(spread), &if_iterator_is_null_or_undefined);

    TNode<Object> iterator_fn =
        GetProperty(context, spread, IteratorSymbolConstant());
    GotoIfNot(TaggedIsCallable(iterator_fn), &if_iterator_fn_not_callable);
    TNode<JSArray> list =
        CAST(CallBuiltin(Builtins::kIterableToListMayPreserveHoles, context,
                         spread, iterator_fn));

    var_js_array = list;
    var_elements = LoadElements(list);
    var_elements_kind = LoadElementsKind(list);
    Branch(Int32LessThan(var_elements_kind.value(),
                         Int32Constant(PACKED_DOUBLE_ELEMENTS)),
           &if_smiorobject, &if_double);

    BIND(&if_iterator_fn_not_callable);
    message_id = SmiConstant(
        static_cast<int>(MessageTemplate::kIteratorSymbolNonCallable)),
    Goto(&throw_spread_error);

    BIND(&if_iterator_is_null_or_undefined);
    message_id = SmiConstant(
        static_cast<int>(MessageTemplate::kNotIterableNoSymbolLoad));
    Goto(&throw_spread_error);

    BIND(&throw_spread_error);
    CallRuntime(Runtime::kThrowSpreadArgError, context, message_id.value(),
                spread);
    Unreachable();
  }

  BIND(&if_smiorobject);
  {
    TNode<Int32T> length = LoadAndUntagToWord32ObjectField(
        var_js_array.value(), JSArray::kLengthOffset);
    TNode<FixedArrayBase> elements = var_elements.value();
    CSA_ASSERT(this, Int32LessThanOrEqual(
                         length, Int32Constant(FixedArray::kMaxLength)));

    if (!new_target) {
      Callable callable = CodeFactory::CallVarargs(isolate());
      TailCallStub(callable, context, target, args_count, length, elements);
    } else {
      Callable callable = CodeFactory::ConstructVarargs(isolate());
      TailCallStub(callable, context, target, *new_target, args_count, length,
                   elements);
    }
  }

  BIND(&if_double);
  {
    TNode<Int32T> length = LoadAndUntagToWord32ObjectField(
        var_js_array.value(), JSArray::kLengthOffset);
    GotoIf(Word32Equal(length, Int32Constant(0)), &if_smiorobject);
    CallOrConstructDoubleVarargs(target, new_target, CAST(var_elements.value()),
                                 length, args_count, context,
                                 var_elements_kind.value());
  }
}

TF_BUILTIN(CallWithArrayLike, CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kTarget);
  base::Optional<TNode<Object>> new_target = base::nullopt;
  auto arguments_list = Parameter<Object>(Descriptor::kArgumentsList);
  auto context = Parameter<Context>(Descriptor::kContext);
  CallOrConstructWithArrayLike(target, new_target, arguments_list, context);
}

TF_BUILTIN(CallWithArrayLike_WithFeedback, CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kTarget);
  base::Optional<TNode<Object>> new_target = base::nullopt;
  auto arguments_list = Parameter<Object>(Descriptor::kArgumentsList);
  auto context = Parameter<Context>(Descriptor::kContext);
  auto feedback_vector = Parameter<FeedbackVector>(Descriptor::kFeedbackVector);
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);
  CollectCallFeedback(target, context, feedback_vector, slot);
  CallOrConstructWithArrayLike(target, new_target, arguments_list, context);
}

TF_BUILTIN(CallWithSpread, CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kTarget);
  base::Optional<TNode<Object>> new_target = base::nullopt;
  auto spread = Parameter<Object>(Descriptor::kSpread);
  auto args_count = UncheckedParameter<Int32T>(Descriptor::kArgumentsCount);
  auto context = Parameter<Context>(Descriptor::kContext);
  CallOrConstructWithSpread(target, new_target, spread, args_count, context);
}

TF_BUILTIN(CallWithSpread_Baseline, CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kTarget);
  base::Optional<TNode<Object>> new_target = base::nullopt;
  auto spread = Parameter<Object>(Descriptor::kSpread);
  auto args_count = UncheckedParameter<Int32T>(Descriptor::kArgumentsCount);
  auto context = LoadContextFromBaseline();
  auto feedback_vector = LoadFeedbackVectorFromBaseline();
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);
  CollectCallFeedback(target, context, feedback_vector, slot);
  CallOrConstructWithSpread(target, new_target, spread, args_count, context);
}

TF_BUILTIN(CallWithSpread_WithFeedback, CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kTarget);
  base::Optional<TNode<Object>> new_target = base::nullopt;
  auto spread = Parameter<Object>(Descriptor::kSpread);
  auto args_count = UncheckedParameter<Int32T>(Descriptor::kArgumentsCount);
  auto context = Parameter<Context>(Descriptor::kContext);
  auto feedback_vector = Parameter<FeedbackVector>(Descriptor::kFeedbackVector);
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);
  CollectCallFeedback(target, context, feedback_vector, slot);
  CallOrConstructWithSpread(target, new_target, spread, args_count, context);
}

TNode<JSReceiver> CallOrConstructBuiltinsAssembler::GetCompatibleReceiver(
    TNode<JSReceiver> receiver, TNode<HeapObject> signature,
    TNode<Context> context) {
  // Walk up the hidden prototype chain to find the compatible holder
  // for the {signature}, starting with the {receiver} itself.
  //
  // Be careful, these loops are hand-tuned for (close to) ideal CSA
  // code generation. Especially the sharing of the {var_template}
  // below is intentional (even though it reads a bit funny in the
  // first loop).
  TVARIABLE(HeapObject, var_holder, receiver);
  Label holder_loop(this, &var_holder), holder_found(this, &var_holder),
      holder_next(this, Label::kDeferred);
  Goto(&holder_loop);
  BIND(&holder_loop);
  {
    // Find the template to compare against the {signature}. We don't
    // bother checking that the template is a FunctionTemplateInfo here,
    // but instead do that as part of the template loop below. The only
    // thing we care about is that the template is actually a HeapObject.
    TNode<HeapObject> holder = var_holder.value();
    TVARIABLE(HeapObject, var_template, LoadMap(holder));
    Label template_map_loop(this, &var_template),
        template_loop(this, &var_template),
        template_from_closure(this, &var_template);
    Goto(&template_map_loop);
    BIND(&template_map_loop);
    {
      // Load the constructor field from the current map (in the
      // {var_template} variable), and see if that is a HeapObject.
      // If it's a Smi then it is non-instance prototype on some
      // initial map, which cannot be the case for API instances.
      TNode<Object> constructor =
          LoadObjectField(var_template.value(),
                          Map::kConstructorOrBackPointerOrNativeContextOffset);
      GotoIf(TaggedIsSmi(constructor), &holder_next);

      // Now there are three cases for {constructor} that we care
      // about here:
      //
      //  1. {constructor} is a JSFunction, and we can load the template
      //     from its SharedFunctionInfo::function_data field (which
      //     may not actually be a FunctionTemplateInfo).
      //  2. {constructor} is a Map, in which case it's not a constructor
      //     but a back-pointer and we follow that.
      //  3. {constructor} is a FunctionTemplateInfo (or some other
      //     HeapObject), in which case we can directly use that for
      //     the template loop below (non-FunctionTemplateInfo objects
      //     will be ruled out there).
      //
      var_template = CAST(constructor);
      TNode<Uint16T> template_type = LoadInstanceType(var_template.value());
      GotoIf(IsJSFunctionInstanceType(template_type), &template_from_closure);
      Branch(InstanceTypeEqual(template_type, MAP_TYPE), &template_map_loop,
             &template_loop);
    }

    BIND(&template_from_closure);
    {
      // The first case from above, where we load the template from the
      // SharedFunctionInfo of the closure. We only check that the
      // SharedFunctionInfo::function_data is a HeapObject and blindly
      // use that as a template, since a non-FunctionTemplateInfo objects
      // will be ruled out automatically by the template loop below.
      TNode<SharedFunctionInfo> template_shared =
          LoadObjectField<SharedFunctionInfo>(
              var_template.value(), JSFunction::kSharedFunctionInfoOffset);
      TNode<Object> template_data = LoadObjectField(
          template_shared, SharedFunctionInfo::kFunctionDataOffset);
      GotoIf(TaggedIsSmi(template_data), &holder_next);
      var_template = CAST(template_data);
      Goto(&template_loop);
    }

    BIND(&template_loop);
    {
      // This loop compares the template to the expected {signature},
      // following the chain of parent templates until it hits the
      // end, in which case we continue with the next holder (the
      // hidden prototype) if there's any.
      TNode<HeapObject> current = var_template.value();
      GotoIf(TaggedEqual(current, signature), &holder_found);

      GotoIfNot(IsFunctionTemplateInfoMap(LoadMap(current)), &holder_next);

      TNode<HeapObject> current_rare = LoadObjectField<HeapObject>(
          current, FunctionTemplateInfo::kRareDataOffset);
      GotoIf(IsUndefined(current_rare), &holder_next);
      var_template = LoadObjectField<HeapObject>(
          current_rare, FunctionTemplateRareData::kParentTemplateOffset);
      Goto(&template_loop);
    }

    BIND(&holder_next);
    {
      // Continue with the hidden prototype of the {holder} if it is a
      // JSGlobalProxy (the hidden prototype can either be null or a
      // JSObject in that case), or throw an illegal invocation exception,
      // since the receiver did not pass the {signature} check.
      TNode<Map> holder_map = LoadMap(holder);
      var_holder = LoadMapPrototype(holder_map);
      GotoIf(IsJSGlobalProxyMap(holder_map), &holder_loop);
      ThrowTypeError(context, MessageTemplate::kIllegalInvocation);
    }
  }

  BIND(&holder_found);
  return CAST(var_holder.value());
}

// This calls an API callback by passing a {FunctionTemplateInfo},
// does appropriate access and compatible receiver checks.
void CallOrConstructBuiltinsAssembler::CallFunctionTemplate(
    CallFunctionTemplateMode mode,
    TNode<FunctionTemplateInfo> function_template_info, TNode<IntPtrT> argc,
    TNode<Context> context) {
  CodeStubArguments args(this, argc);
  Label throw_illegal_invocation(this, Label::kDeferred);

  // For API callbacks the receiver is always a JSReceiver (since
  // they are treated like sloppy mode functions). We might need
  // to perform access checks in the current {context}, depending
  // on whether the "needs access check" bit is set on the receiver
  // _and_ the {function_template_info} doesn't have the "accepts
  // any receiver" bit set.
  TNode<JSReceiver> receiver = CAST(args.GetReceiver());
  if (mode == CallFunctionTemplateMode::kCheckAccess ||
      mode == CallFunctionTemplateMode::kCheckAccessAndCompatibleReceiver) {
    TNode<Map> receiver_map = LoadMap(receiver);
    Label receiver_needs_access_check(this, Label::kDeferred),
        receiver_done(this);
    GotoIfNot(IsSetWord32<Map::Bits1::IsAccessCheckNeededBit>(
                  LoadMapBitField(receiver_map)),
              &receiver_done);
    TNode<IntPtrT> function_template_info_flags = LoadAndUntagObjectField(
        function_template_info, FunctionTemplateInfo::kFlagOffset);
    Branch(IsSetWord(function_template_info_flags,
                     1 << FunctionTemplateInfo::AcceptAnyReceiverBit::kShift),
           &receiver_done, &receiver_needs_access_check);

    BIND(&receiver_needs_access_check);
    {
      CallRuntime(Runtime::kAccessCheck, context, receiver);
      Goto(&receiver_done);
    }

    BIND(&receiver_done);
  }

  // Figure out the API holder for the {receiver} depending on the
  // {mode} and the signature on the {function_template_info}.
  TNode<JSReceiver> holder;
  if (mode == CallFunctionTemplateMode::kCheckAccess) {
    // We did the access check (including the ToObject) above, so
    // {receiver} is a JSReceiver at this point, and we don't need
    // to perform any "compatible receiver check", so {holder} is
    // actually the {receiver}.
    holder = receiver;
  } else {
    // If the {function_template_info} doesn't specify any signature, we
    // just use the receiver as the holder for the API callback, otherwise
    // we need to look for a compatible holder in the receiver's hidden
    // prototype chain.
    TNode<HeapObject> signature = LoadObjectField<HeapObject>(
        function_template_info, FunctionTemplateInfo::kSignatureOffset);
    holder = Select<JSReceiver>(
        IsUndefined(signature),  // --
        [&]() { return receiver; },
        [&]() { return GetCompatibleReceiver(receiver, signature, context); });
  }

  // Perform the actual API callback invocation via CallApiCallback.
  TNode<CallHandlerInfo> call_handler_info = LoadObjectField<CallHandlerInfo>(
      function_template_info, FunctionTemplateInfo::kCallCodeOffset);
  TNode<Foreign> foreign = LoadObjectField<Foreign>(
      call_handler_info, CallHandlerInfo::kJsCallbackOffset);
  TNode<RawPtrT> callback = LoadForeignForeignAddressPtr(foreign);
  TNode<Object> call_data =
      LoadObjectField<Object>(call_handler_info, CallHandlerInfo::kDataOffset);
  TailCallStub(CodeFactory::CallApiCallback(isolate()), context, callback, argc,
               call_data, holder);
}

TF_BUILTIN(CallFunctionTemplate_CheckAccess, CallOrConstructBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto function_template_info = UncheckedParameter<FunctionTemplateInfo>(
      Descriptor::kFunctionTemplateInfo);
  auto argc = UncheckedParameter<IntPtrT>(Descriptor::kArgumentsCount);
  CallFunctionTemplate(CallFunctionTemplateMode::kCheckAccess,
                       function_template_info, argc, context);
}

TF_BUILTIN(CallFunctionTemplate_CheckCompatibleReceiver,
           CallOrConstructBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto function_template_info = UncheckedParameter<FunctionTemplateInfo>(
      Descriptor::kFunctionTemplateInfo);
  auto argc = UncheckedParameter<IntPtrT>(Descriptor::kArgumentsCount);
  CallFunctionTemplate(CallFunctionTemplateMode::kCheckCompatibleReceiver,
                       function_template_info, argc, context);
}

TF_BUILTIN(CallFunctionTemplate_CheckAccessAndCompatibleReceiver,
           CallOrConstructBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto function_template_info = UncheckedParameter<FunctionTemplateInfo>(
      Descriptor::kFunctionTemplateInfo);
  auto argc = UncheckedParameter<IntPtrT>(Descriptor::kArgumentsCount);
  CallFunctionTemplate(
      CallFunctionTemplateMode::kCheckAccessAndCompatibleReceiver,
      function_template_info, argc, context);
}

}  // namespace internal
}  // namespace v8
