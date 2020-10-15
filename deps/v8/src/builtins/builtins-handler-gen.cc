// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/optional.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/ic/ic.h"
#include "src/ic/keyed-store-generic.h"
#include "src/objects/objects-inl.h"
#include "torque-generated/exported-macros-assembler-tq.h"

namespace v8 {
namespace internal {

class HandlerBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit HandlerBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void Generate_KeyedStoreIC_SloppyArguments();

  // Essentially turns runtime elements kinds (TNode<Int32T>) into
  // compile-time types (int) by dispatching over the runtime type and
  // emitting a specialized copy of the given case function for each elements
  // kind. Use with caution. This produces a *lot* of code.
  using ElementsKindSwitchCase = std::function<void(ElementsKind)>;
  void DispatchByElementsKind(TNode<Int32T> elements_kind,
                              const ElementsKindSwitchCase& case_function,
                              bool handle_typed_elements_kind);

  // Dispatches over all possible combinations of {from,to} elements kinds.
  using ElementsKindTransitionSwitchCase =
      std::function<void(ElementsKind, ElementsKind)>;
  void DispatchForElementsKindTransition(
      TNode<Int32T> from_kind, TNode<Int32T> to_kind,
      const ElementsKindTransitionSwitchCase& case_function);

  void Generate_ElementsTransitionAndStore(KeyedAccessStoreMode store_mode);
  void Generate_StoreFastElementIC(KeyedAccessStoreMode store_mode);

  enum class ArgumentsAccessMode { kLoad, kStore, kHas };

  // Emits keyed sloppy arguments has. Returns whether the key is in the
  // arguments.
  TNode<Object> HasKeyedSloppyArguments(TNode<JSObject> receiver,
                                        TNode<Object> key, Label* bailout) {
    return EmitKeyedSloppyArguments(receiver, key, base::nullopt, bailout,
                                    ArgumentsAccessMode::kHas);
  }

  // Emits keyed sloppy arguments load. Returns either the loaded value.
  TNode<Object> LoadKeyedSloppyArguments(TNode<JSObject> receiver,
                                         TNode<Object> key, Label* bailout) {
    return EmitKeyedSloppyArguments(receiver, key, base::nullopt, bailout,
                                    ArgumentsAccessMode::kLoad);
  }

  // Emits keyed sloppy arguments store.
  void StoreKeyedSloppyArguments(TNode<JSObject> receiver, TNode<Object> key,
                                 TNode<Object> value, Label* bailout) {
    EmitKeyedSloppyArguments(receiver, key, value, bailout,
                             ArgumentsAccessMode::kStore);
  }

 private:
  // Emits keyed sloppy arguments load if the |value| is nullopt or store
  // otherwise. Returns either the loaded value or |value|.
  TNode<Object> EmitKeyedSloppyArguments(TNode<JSObject> receiver,
                                         TNode<Object> key,
                                         base::Optional<TNode<Object>> value,
                                         Label* bailout,
                                         ArgumentsAccessMode access_mode);
};

TNode<Object> HandlerBuiltinsAssembler::EmitKeyedSloppyArguments(
    TNode<JSObject> receiver, TNode<Object> tagged_key,
    base::Optional<TNode<Object>> value, Label* bailout,
    ArgumentsAccessMode access_mode) {
  GotoIfNot(TaggedIsSmi(tagged_key), bailout);
  TNode<IntPtrT> key = SmiUntag(CAST(tagged_key));
  GotoIf(IntPtrLessThan(key, IntPtrConstant(0)), bailout);

  TNode<SloppyArgumentsElements> elements = CAST(LoadElements(receiver));
  TNode<IntPtrT> elements_length = LoadAndUntagFixedArrayBaseLength(elements);

  TVARIABLE(Object, var_result);
  if (access_mode == ArgumentsAccessMode::kStore) {
    var_result = *value;
  } else {
    DCHECK(access_mode == ArgumentsAccessMode::kLoad ||
           access_mode == ArgumentsAccessMode::kHas);
  }
  Label if_mapped(this), if_unmapped(this), end(this, &var_result);

  GotoIf(UintPtrGreaterThanOrEqual(key, elements_length), &if_unmapped);

  TNode<Object> mapped_index =
      LoadSloppyArgumentsElementsMappedEntries(elements, key);
  Branch(TaggedEqual(mapped_index, TheHoleConstant()), &if_unmapped,
         &if_mapped);

  BIND(&if_mapped);
  {
    TNode<IntPtrT> mapped_index_intptr = SmiUntag(CAST(mapped_index));
    TNode<Context> the_context = LoadSloppyArgumentsElementsContext(elements);
    if (access_mode == ArgumentsAccessMode::kLoad) {
      TNode<Object> result =
          LoadContextElement(the_context, mapped_index_intptr);
      CSA_ASSERT(this, TaggedNotEqual(result, TheHoleConstant()));
      var_result = result;
    } else if (access_mode == ArgumentsAccessMode::kHas) {
      CSA_ASSERT(this, Word32BinaryNot(IsTheHole(LoadContextElement(
                           the_context, mapped_index_intptr))));
      var_result = TrueConstant();
    } else {
      StoreContextElement(the_context, mapped_index_intptr, *value);
    }
    Goto(&end);
  }

  BIND(&if_unmapped);
  {
    TNode<HeapObject> backing_store_ho =
        LoadSloppyArgumentsElementsArguments(elements);
    GotoIf(TaggedNotEqual(LoadMap(backing_store_ho), FixedArrayMapConstant()),
           bailout);
    TNode<FixedArray> backing_store = CAST(backing_store_ho);

    TNode<IntPtrT> backing_store_length =
        LoadAndUntagFixedArrayBaseLength(backing_store);

    // Out-of-bounds access may involve prototype chain walk and is handled
    // in runtime.
    GotoIf(UintPtrGreaterThanOrEqual(key, backing_store_length), bailout);

    // The key falls into unmapped range.
    if (access_mode == ArgumentsAccessMode::kStore) {
      StoreFixedArrayElement(backing_store, key, *value);
    } else {
      TNode<Object> value = LoadFixedArrayElement(backing_store, key);
      GotoIf(TaggedEqual(value, TheHoleConstant()), bailout);

      if (access_mode == ArgumentsAccessMode::kHas) {
        var_result = TrueConstant();
      } else {
        DCHECK_EQ(access_mode, ArgumentsAccessMode::kLoad);
        var_result = value;
      }
    }
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TF_BUILTIN(LoadIC_StringLength, CodeStubAssembler) {
  TNode<String> string = CAST(Parameter(Descriptor::kReceiver));
  Return(LoadStringLengthAsSmi(string));
}

TF_BUILTIN(LoadIC_StringWrapperLength, CodeStubAssembler) {
  TNode<JSPrimitiveWrapper> value = CAST(Parameter(Descriptor::kReceiver));
  TNode<String> string = CAST(LoadJSPrimitiveWrapperValue(value));
  Return(LoadStringLengthAsSmi(string));
}

void Builtins::Generate_KeyedStoreIC_Megamorphic(
    compiler::CodeAssemblerState* state) {
  KeyedStoreGenericGenerator::Generate(state);
}

void Builtins::Generate_StoreIC_NoFeedback(
    compiler::CodeAssemblerState* state) {
  StoreICNoFeedbackGenerator::Generate(state);
}

// All possible fast-to-fast transitions. Transitions to dictionary mode are not
// handled by ElementsTransitionAndStore.
#define ELEMENTS_KIND_TRANSITIONS(V)               \
  V(PACKED_SMI_ELEMENTS, HOLEY_SMI_ELEMENTS)       \
  V(PACKED_SMI_ELEMENTS, PACKED_DOUBLE_ELEMENTS)   \
  V(PACKED_SMI_ELEMENTS, HOLEY_DOUBLE_ELEMENTS)    \
  V(PACKED_SMI_ELEMENTS, PACKED_ELEMENTS)          \
  V(PACKED_SMI_ELEMENTS, HOLEY_ELEMENTS)           \
  V(HOLEY_SMI_ELEMENTS, HOLEY_DOUBLE_ELEMENTS)     \
  V(HOLEY_SMI_ELEMENTS, HOLEY_ELEMENTS)            \
  V(PACKED_DOUBLE_ELEMENTS, HOLEY_DOUBLE_ELEMENTS) \
  V(PACKED_DOUBLE_ELEMENTS, PACKED_ELEMENTS)       \
  V(PACKED_DOUBLE_ELEMENTS, HOLEY_ELEMENTS)        \
  V(HOLEY_DOUBLE_ELEMENTS, HOLEY_ELEMENTS)         \
  V(PACKED_ELEMENTS, HOLEY_ELEMENTS)

void HandlerBuiltinsAssembler::DispatchForElementsKindTransition(
    TNode<Int32T> from_kind, TNode<Int32T> to_kind,
    const ElementsKindTransitionSwitchCase& case_function) {
  STATIC_ASSERT(sizeof(ElementsKind) == sizeof(uint8_t));

  Label next(this), if_unknown_type(this, Label::kDeferred);

  int32_t combined_elements_kinds[] = {
#define ELEMENTS_KINDS_CASE(FROM, TO) (FROM << kBitsPerByte) | TO,
      ELEMENTS_KIND_TRANSITIONS(ELEMENTS_KINDS_CASE)
#undef ELEMENTS_KINDS_CASE
  };

#define ELEMENTS_KINDS_CASE(FROM, TO) Label if_##FROM##_##TO(this);
  ELEMENTS_KIND_TRANSITIONS(ELEMENTS_KINDS_CASE)
#undef ELEMENTS_KINDS_CASE

  Label* elements_kind_labels[] = {
#define ELEMENTS_KINDS_CASE(FROM, TO) &if_##FROM##_##TO,
      ELEMENTS_KIND_TRANSITIONS(ELEMENTS_KINDS_CASE)
#undef ELEMENTS_KINDS_CASE
  };
  STATIC_ASSERT(arraysize(combined_elements_kinds) ==
                arraysize(elements_kind_labels));

  TNode<Int32T> combined_elements_kind =
      Word32Or(Word32Shl(from_kind, Int32Constant(kBitsPerByte)), to_kind);

  Switch(combined_elements_kind, &if_unknown_type, combined_elements_kinds,
         elements_kind_labels, arraysize(combined_elements_kinds));

#define ELEMENTS_KINDS_CASE(FROM, TO) \
  BIND(&if_##FROM##_##TO);            \
  {                                   \
    case_function(FROM, TO);          \
    Goto(&next);                      \
  }
  ELEMENTS_KIND_TRANSITIONS(ELEMENTS_KINDS_CASE)
#undef ELEMENTS_KINDS_CASE

  BIND(&if_unknown_type);
  Unreachable();

  BIND(&next);
}

#undef ELEMENTS_KIND_TRANSITIONS

void HandlerBuiltinsAssembler::Generate_ElementsTransitionAndStore(
    KeyedAccessStoreMode store_mode) {
  using Descriptor = StoreTransitionDescriptor;
  TNode<JSObject> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> key = CAST(Parameter(Descriptor::kName));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Map> map = CAST(Parameter(Descriptor::kMap));
  TNode<Smi> slot = CAST(Parameter(Descriptor::kSlot));
  TNode<FeedbackVector> vector = CAST(Parameter(Descriptor::kVector));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Comment("ElementsTransitionAndStore: store_mode=", store_mode);

  Label miss(this);

  if (FLAG_trace_elements_transitions) {
    // Tracing elements transitions is the job of the runtime.
    Goto(&miss);
  } else {
    // TODO(v8:8481): Pass from_kind and to_kind in feedback vector slots.
    DispatchForElementsKindTransition(
        LoadElementsKind(receiver), LoadMapElementsKind(map),
        [=, &miss](ElementsKind from_kind, ElementsKind to_kind) {
          TransitionElementsKind(receiver, map, from_kind, to_kind, &miss);
          EmitElementStore(receiver, key, value, to_kind, store_mode, &miss,
                           context, nullptr);
        });
    Return(value);
  }

  BIND(&miss);
  TailCallRuntime(Runtime::kElementsTransitionAndStoreIC_Miss, context,
                  receiver, key, value, map, slot, vector);
}

TF_BUILTIN(ElementsTransitionAndStore_Standard, HandlerBuiltinsAssembler) {
  Generate_ElementsTransitionAndStore(STANDARD_STORE);
}

TF_BUILTIN(ElementsTransitionAndStore_GrowNoTransitionHandleCOW,
           HandlerBuiltinsAssembler) {
  Generate_ElementsTransitionAndStore(STORE_AND_GROW_HANDLE_COW);
}

TF_BUILTIN(ElementsTransitionAndStore_NoTransitionIgnoreOOB,
           HandlerBuiltinsAssembler) {
  Generate_ElementsTransitionAndStore(STORE_IGNORE_OUT_OF_BOUNDS);
}

TF_BUILTIN(ElementsTransitionAndStore_NoTransitionHandleCOW,
           HandlerBuiltinsAssembler) {
  Generate_ElementsTransitionAndStore(STORE_HANDLE_COW);
}

// All elements kinds handled by EmitElementStore. Specifically, this includes
// fast elements and fixed typed array elements.
#define ELEMENTS_KINDS(V)          \
  V(PACKED_SMI_ELEMENTS)           \
  V(HOLEY_SMI_ELEMENTS)            \
  V(PACKED_ELEMENTS)               \
  V(PACKED_NONEXTENSIBLE_ELEMENTS) \
  V(PACKED_SEALED_ELEMENTS)        \
  V(HOLEY_ELEMENTS)                \
  V(HOLEY_NONEXTENSIBLE_ELEMENTS)  \
  V(HOLEY_SEALED_ELEMENTS)         \
  V(PACKED_DOUBLE_ELEMENTS)        \
  V(HOLEY_DOUBLE_ELEMENTS)         \
  V(UINT8_ELEMENTS)                \
  V(INT8_ELEMENTS)                 \
  V(UINT16_ELEMENTS)               \
  V(INT16_ELEMENTS)                \
  V(UINT32_ELEMENTS)               \
  V(INT32_ELEMENTS)                \
  V(FLOAT32_ELEMENTS)              \
  V(FLOAT64_ELEMENTS)              \
  V(UINT8_CLAMPED_ELEMENTS)        \
  V(BIGUINT64_ELEMENTS)            \
  V(BIGINT64_ELEMENTS)

void HandlerBuiltinsAssembler::DispatchByElementsKind(
    TNode<Int32T> elements_kind, const ElementsKindSwitchCase& case_function,
    bool handle_typed_elements_kind) {
  Label next(this), if_unknown_type(this, Label::kDeferred);

  int32_t elements_kinds[] = {
#define ELEMENTS_KINDS_CASE(KIND) KIND,
      ELEMENTS_KINDS(ELEMENTS_KINDS_CASE)
#undef ELEMENTS_KINDS_CASE
  };

#define ELEMENTS_KINDS_CASE(KIND) Label if_##KIND(this);
  ELEMENTS_KINDS(ELEMENTS_KINDS_CASE)
#undef ELEMENTS_KINDS_CASE

  Label* elements_kind_labels[] = {
#define ELEMENTS_KINDS_CASE(KIND) &if_##KIND,
      ELEMENTS_KINDS(ELEMENTS_KINDS_CASE)
#undef ELEMENTS_KINDS_CASE
  };
  STATIC_ASSERT(arraysize(elements_kinds) == arraysize(elements_kind_labels));

  // TODO(mythria): Do not emit cases for typed elements kind when
  // handle_typed_elements is false to decrease the size of the jump table.
  Switch(elements_kind, &if_unknown_type, elements_kinds, elements_kind_labels,
         arraysize(elements_kinds));

#define ELEMENTS_KINDS_CASE(KIND)                                \
  BIND(&if_##KIND);                                              \
  {                                                              \
    if (!FLAG_enable_sealed_frozen_elements_kind &&              \
        IsAnyNonextensibleElementsKindUnchecked(KIND)) {         \
      /* Disable support for frozen or sealed elements kinds. */ \
      Unreachable();                                             \
    } else if (!handle_typed_elements_kind &&                    \
               IsTypedArrayElementsKind(KIND)) {                 \
      Unreachable();                                             \
    } else {                                                     \
      case_function(KIND);                                       \
      Goto(&next);                                               \
    }                                                            \
  }
  ELEMENTS_KINDS(ELEMENTS_KINDS_CASE)
#undef ELEMENTS_KINDS_CASE

  BIND(&if_unknown_type);
  Unreachable();

  BIND(&next);
}

#undef ELEMENTS_KINDS

void HandlerBuiltinsAssembler::Generate_StoreFastElementIC(
    KeyedAccessStoreMode store_mode) {
  using Descriptor = StoreWithVectorDescriptor;
  TNode<JSObject> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> key = CAST(Parameter(Descriptor::kName));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Smi> slot = CAST(Parameter(Descriptor::kSlot));
  TNode<HeapObject> vector = CAST(Parameter(Descriptor::kVector));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Comment("StoreFastElementStub: store_mode=", store_mode);

  Label miss(this);

  bool handle_typed_elements_kind =
      store_mode == STANDARD_STORE || store_mode == STORE_IGNORE_OUT_OF_BOUNDS;
  // For typed arrays maybe_converted_value contains the value obtained after
  // calling ToNumber. We should pass the converted value to the runtime to
  // avoid doing the user visible conversion again.
  TVARIABLE(Object, maybe_converted_value, value);
  // TODO(v8:8481): Pass elements_kind in feedback vector slots.
  DispatchByElementsKind(
      LoadElementsKind(receiver),
      [=, &miss, &maybe_converted_value](ElementsKind elements_kind) {
        EmitElementStore(receiver, key, value, elements_kind, store_mode, &miss,
                         context, &maybe_converted_value);
      },
      handle_typed_elements_kind);
  Return(value);

  BIND(&miss);
  TailCallRuntime(Runtime::kKeyedStoreIC_Miss, context,
                  maybe_converted_value.value(), slot, vector, receiver, key);
}

TF_BUILTIN(StoreFastElementIC_Standard, HandlerBuiltinsAssembler) {
  Generate_StoreFastElementIC(STANDARD_STORE);
}

TF_BUILTIN(StoreFastElementIC_GrowNoTransitionHandleCOW,
           HandlerBuiltinsAssembler) {
  Generate_StoreFastElementIC(STORE_AND_GROW_HANDLE_COW);
}

TF_BUILTIN(StoreFastElementIC_NoTransitionIgnoreOOB, HandlerBuiltinsAssembler) {
  Generate_StoreFastElementIC(STORE_IGNORE_OUT_OF_BOUNDS);
}

TF_BUILTIN(StoreFastElementIC_NoTransitionHandleCOW, HandlerBuiltinsAssembler) {
  Generate_StoreFastElementIC(STORE_HANDLE_COW);
}

TF_BUILTIN(LoadIC_FunctionPrototype, CodeStubAssembler) {
  TNode<JSFunction> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Name> name = CAST(Parameter(Descriptor::kName));
  TNode<Smi> slot = CAST(Parameter(Descriptor::kSlot));
  TNode<FeedbackVector> vector = CAST(Parameter(Descriptor::kVector));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Label miss(this, Label::kDeferred);
  Return(LoadJSFunctionPrototype(receiver, &miss));

  BIND(&miss);
  TailCallRuntime(Runtime::kLoadIC_Miss, context, receiver, name, slot, vector);
}

TF_BUILTIN(StoreGlobalIC_Slow, CodeStubAssembler) {
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Name> name = CAST(Parameter(Descriptor::kName));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Smi> slot = CAST(Parameter(Descriptor::kSlot));
  TNode<FeedbackVector> vector = CAST(Parameter(Descriptor::kVector));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  TailCallRuntime(Runtime::kStoreGlobalIC_Slow, context, value, slot, vector,
                  receiver, name);
}

TF_BUILTIN(KeyedLoadIC_SloppyArguments, HandlerBuiltinsAssembler) {
  TNode<JSObject> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> key = CAST(Parameter(Descriptor::kName));
  TNode<Smi> slot = CAST(Parameter(Descriptor::kSlot));
  TNode<HeapObject> vector = CAST(Parameter(Descriptor::kVector));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Label miss(this);

  TNode<Object> result = LoadKeyedSloppyArguments(receiver, key, &miss);
  Return(result);

  BIND(&miss);
  {
    Comment("Miss");
    TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver, key, slot,
                    vector);
  }
}

void HandlerBuiltinsAssembler::Generate_KeyedStoreIC_SloppyArguments() {
  using Descriptor = StoreWithVectorDescriptor;
  TNode<JSObject> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> key = CAST(Parameter(Descriptor::kName));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Smi> slot = CAST(Parameter(Descriptor::kSlot));
  TNode<HeapObject> vector = CAST(Parameter(Descriptor::kVector));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Label miss(this);

  StoreKeyedSloppyArguments(receiver, key, value, &miss);
  Return(value);

  BIND(&miss);
  TailCallRuntime(Runtime::kKeyedStoreIC_Miss, context, value, slot, vector,
                  receiver, key);
}

TF_BUILTIN(KeyedStoreIC_SloppyArguments_Standard, HandlerBuiltinsAssembler) {
  Generate_KeyedStoreIC_SloppyArguments();
}

TF_BUILTIN(KeyedStoreIC_SloppyArguments_GrowNoTransitionHandleCOW,
           HandlerBuiltinsAssembler) {
  Generate_KeyedStoreIC_SloppyArguments();
}

TF_BUILTIN(KeyedStoreIC_SloppyArguments_NoTransitionIgnoreOOB,
           HandlerBuiltinsAssembler) {
  Generate_KeyedStoreIC_SloppyArguments();
}

TF_BUILTIN(KeyedStoreIC_SloppyArguments_NoTransitionHandleCOW,
           HandlerBuiltinsAssembler) {
  Generate_KeyedStoreIC_SloppyArguments();
}

TF_BUILTIN(LoadIndexedInterceptorIC, CodeStubAssembler) {
  TNode<JSObject> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> key = CAST(Parameter(Descriptor::kName));
  TNode<Smi> slot = CAST(Parameter(Descriptor::kSlot));
  TNode<HeapObject> vector = CAST(Parameter(Descriptor::kVector));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Label if_keyispositivesmi(this), if_keyisinvalid(this);
  Branch(TaggedIsPositiveSmi(key), &if_keyispositivesmi, &if_keyisinvalid);
  BIND(&if_keyispositivesmi);
  TailCallRuntime(Runtime::kLoadElementWithInterceptor, context, receiver, key);

  BIND(&if_keyisinvalid);
  TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver, key, slot,
                  vector);
}

TF_BUILTIN(KeyedHasIC_SloppyArguments, HandlerBuiltinsAssembler) {
  TNode<JSObject> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> key = CAST(Parameter(Descriptor::kName));
  TNode<Smi> slot = CAST(Parameter(Descriptor::kSlot));
  TNode<HeapObject> vector = CAST(Parameter(Descriptor::kVector));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Label miss(this);

  TNode<Object> result = HasKeyedSloppyArguments(receiver, key, &miss);
  Return(result);

  BIND(&miss);
  {
    Comment("Miss");
    TailCallRuntime(Runtime::kKeyedHasIC_Miss, context, receiver, key, slot,
                    vector);
  }
}

TF_BUILTIN(HasIndexedInterceptorIC, CodeStubAssembler) {
  TNode<JSObject> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> key = CAST(Parameter(Descriptor::kName));
  TNode<Smi> slot = CAST(Parameter(Descriptor::kSlot));
  TNode<HeapObject> vector = CAST(Parameter(Descriptor::kVector));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Label if_keyispositivesmi(this), if_keyisinvalid(this);
  Branch(TaggedIsPositiveSmi(key), &if_keyispositivesmi, &if_keyisinvalid);
  BIND(&if_keyispositivesmi);
  TailCallRuntime(Runtime::kHasElementWithInterceptor, context, receiver, key);

  BIND(&if_keyisinvalid);
  TailCallRuntime(Runtime::kKeyedHasIC_Miss, context, receiver, key, slot,
                  vector);
}

}  // namespace internal
}  // namespace v8
