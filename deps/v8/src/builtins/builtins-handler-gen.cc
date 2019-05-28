// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/ic/ic.h"
#include "src/ic/keyed-store-generic.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

class HandlerBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit HandlerBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void Generate_KeyedStoreIC_SloppyArguments();
  void Generate_KeyedStoreIC_Slow();
  void Generate_StoreInArrayLiteralIC_Slow();

  // Essentially turns runtime elements kinds (TNode<Int32T>) into
  // compile-time types (int) by dispatching over the runtime type and
  // emitting a specialized copy of the given case function for each elements
  // kind. Use with caution. This produces a *lot* of code.
  typedef std::function<void(ElementsKind)> ElementsKindSwitchCase;
  void DispatchByElementsKind(TNode<Int32T> elements_kind,
                              const ElementsKindSwitchCase& case_function);

  // Dispatches over all possible combinations of {from,to} elements kinds.
  typedef std::function<void(ElementsKind, ElementsKind)>
      ElementsKindTransitionSwitchCase;
  void DispatchForElementsKindTransition(
      TNode<Int32T> from_kind, TNode<Int32T> to_kind,
      const ElementsKindTransitionSwitchCase& case_function);

  void Generate_ElementsTransitionAndStore(KeyedAccessStoreMode store_mode);
  void Generate_StoreFastElementIC(KeyedAccessStoreMode store_mode);
};

TF_BUILTIN(LoadIC_StringLength, CodeStubAssembler) {
  Node* string = Parameter(Descriptor::kReceiver);
  Return(LoadStringLengthAsSmi(string));
}

TF_BUILTIN(LoadIC_StringWrapperLength, CodeStubAssembler) {
  Node* value = Parameter(Descriptor::kReceiver);
  Node* string = LoadJSValueValue(value);
  Return(LoadStringLengthAsSmi(string));
}

TF_BUILTIN(KeyedLoadIC_Slow, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kGetProperty, context, receiver, name);
}

void Builtins::Generate_KeyedStoreIC_Megamorphic(
    compiler::CodeAssemblerState* state) {
  KeyedStoreGenericGenerator::Generate(state);
}

void Builtins::Generate_StoreIC_Uninitialized(
    compiler::CodeAssemblerState* state) {
  StoreICUninitializedGenerator::Generate(state);
}

// TODO(mythria): Check if we can remove feedback vector and slot parameters in
// descriptor.
void HandlerBuiltinsAssembler::Generate_KeyedStoreIC_Slow() {
  typedef StoreWithVectorDescriptor Descriptor;
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  TailCallRuntime(Runtime::kKeyedStoreIC_Slow, context, value, receiver, name);
}

TF_BUILTIN(KeyedStoreIC_Slow, HandlerBuiltinsAssembler) {
  Generate_KeyedStoreIC_Slow();
}

TF_BUILTIN(KeyedStoreIC_Slow_Standard, HandlerBuiltinsAssembler) {
  Generate_KeyedStoreIC_Slow();
}

TF_BUILTIN(KeyedStoreIC_Slow_GrowNoTransitionHandleCOW,
           HandlerBuiltinsAssembler) {
  Generate_KeyedStoreIC_Slow();
}

TF_BUILTIN(KeyedStoreIC_Slow_NoTransitionIgnoreOOB, HandlerBuiltinsAssembler) {
  Generate_KeyedStoreIC_Slow();
}

TF_BUILTIN(KeyedStoreIC_Slow_NoTransitionHandleCOW, HandlerBuiltinsAssembler) {
  Generate_KeyedStoreIC_Slow();
}

void HandlerBuiltinsAssembler::Generate_StoreInArrayLiteralIC_Slow() {
  typedef StoreWithVectorDescriptor Descriptor;
  Node* array = Parameter(Descriptor::kReceiver);
  Node* index = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);
  TailCallRuntime(Runtime::kStoreInArrayLiteralIC_Slow, context, value, array,
                  index);
}

TF_BUILTIN(StoreInArrayLiteralIC_Slow, HandlerBuiltinsAssembler) {
  Generate_StoreInArrayLiteralIC_Slow();
}

TF_BUILTIN(StoreInArrayLiteralIC_Slow_Standard, HandlerBuiltinsAssembler) {
  Generate_StoreInArrayLiteralIC_Slow();
}

TF_BUILTIN(StoreInArrayLiteralIC_Slow_GrowNoTransitionHandleCOW,
           HandlerBuiltinsAssembler) {
  Generate_StoreInArrayLiteralIC_Slow();
}

TF_BUILTIN(StoreInArrayLiteralIC_Slow_NoTransitionIgnoreOOB,
           HandlerBuiltinsAssembler) {
  Generate_StoreInArrayLiteralIC_Slow();
}

TF_BUILTIN(StoreInArrayLiteralIC_Slow_NoTransitionHandleCOW,
           HandlerBuiltinsAssembler) {
  Generate_StoreInArrayLiteralIC_Slow();
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

  TNode<Word32T> combined_elements_kind =
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
  typedef StoreTransitionDescriptor Descriptor;
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* map = Parameter(Descriptor::kMap);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

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
                           context);
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
  Generate_ElementsTransitionAndStore(STORE_AND_GROW_NO_TRANSITION_HANDLE_COW);
}

TF_BUILTIN(ElementsTransitionAndStore_NoTransitionIgnoreOOB,
           HandlerBuiltinsAssembler) {
  Generate_ElementsTransitionAndStore(STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS);
}

TF_BUILTIN(ElementsTransitionAndStore_NoTransitionHandleCOW,
           HandlerBuiltinsAssembler) {
  Generate_ElementsTransitionAndStore(STORE_NO_TRANSITION_HANDLE_COW);
}

// All elements kinds handled by EmitElementStore. Specifically, this includes
// fast elements and fixed typed array elements.
#define ELEMENTS_KINDS(V)   \
  V(PACKED_SMI_ELEMENTS)    \
  V(HOLEY_SMI_ELEMENTS)     \
  V(PACKED_ELEMENTS)        \
  V(PACKED_SEALED_ELEMENTS) \
  V(HOLEY_ELEMENTS)         \
  V(PACKED_DOUBLE_ELEMENTS) \
  V(HOLEY_DOUBLE_ELEMENTS)  \
  V(UINT8_ELEMENTS)         \
  V(INT8_ELEMENTS)          \
  V(UINT16_ELEMENTS)        \
  V(INT16_ELEMENTS)         \
  V(UINT32_ELEMENTS)        \
  V(INT32_ELEMENTS)         \
  V(FLOAT32_ELEMENTS)       \
  V(FLOAT64_ELEMENTS)       \
  V(UINT8_CLAMPED_ELEMENTS) \
  V(BIGUINT64_ELEMENTS)     \
  V(BIGINT64_ELEMENTS)

void HandlerBuiltinsAssembler::DispatchByElementsKind(
    TNode<Int32T> elements_kind, const ElementsKindSwitchCase& case_function) {
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

  Switch(elements_kind, &if_unknown_type, elements_kinds, elements_kind_labels,
         arraysize(elements_kinds));

#define ELEMENTS_KINDS_CASE(KIND) \
  BIND(&if_##KIND);               \
  {                               \
    case_function(KIND);          \
    Goto(&next);                  \
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
  typedef StoreWithVectorDescriptor Descriptor;
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Comment("StoreFastElementStub: store_mode=", store_mode);

  Label miss(this);

  // TODO(v8:8481): Pass elements_kind in feedback vector slots.
  DispatchByElementsKind(LoadElementsKind(receiver),
                         [=, &miss](ElementsKind elements_kind) {
                           EmitElementStore(receiver, key, value, elements_kind,
                                            store_mode, &miss, context);
                         });
  Return(value);

  BIND(&miss);
  TailCallRuntime(Runtime::kKeyedStoreIC_Miss, context, value, slot, vector,
                  receiver, key);
}

TF_BUILTIN(StoreFastElementIC_Standard, HandlerBuiltinsAssembler) {
  Generate_StoreFastElementIC(STANDARD_STORE);
}

TF_BUILTIN(StoreFastElementIC_GrowNoTransitionHandleCOW,
           HandlerBuiltinsAssembler) {
  Generate_StoreFastElementIC(STORE_AND_GROW_NO_TRANSITION_HANDLE_COW);
}

TF_BUILTIN(StoreFastElementIC_NoTransitionIgnoreOOB, HandlerBuiltinsAssembler) {
  Generate_StoreFastElementIC(STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS);
}

TF_BUILTIN(StoreFastElementIC_NoTransitionHandleCOW, HandlerBuiltinsAssembler) {
  Generate_StoreFastElementIC(STORE_NO_TRANSITION_HANDLE_COW);
}

TF_BUILTIN(LoadGlobalIC_Slow, CodeStubAssembler) {
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kLoadGlobalIC_Slow, context, name, slot, vector);
}

TF_BUILTIN(LoadIC_FunctionPrototype, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label miss(this, Label::kDeferred);
  Return(LoadJSFunctionPrototype(receiver, &miss));

  BIND(&miss);
  TailCallRuntime(Runtime::kLoadIC_Miss, context, receiver, name, slot, vector);
}

TF_BUILTIN(LoadIC_Slow, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kGetProperty, context, receiver, name);
}

TF_BUILTIN(StoreGlobalIC_Slow, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  TailCallRuntime(Runtime::kStoreGlobalIC_Slow, context, value, slot, vector,
                  receiver, name);
}

TF_BUILTIN(KeyedLoadIC_SloppyArguments, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label miss(this);

  Node* result = LoadKeyedSloppyArguments(receiver, key, &miss);
  Return(result);

  BIND(&miss);
  {
    Comment("Miss");
    TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver, key, slot,
                    vector);
  }
}

void HandlerBuiltinsAssembler::Generate_KeyedStoreIC_SloppyArguments() {
  typedef StoreWithVectorDescriptor Descriptor;
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

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

TF_BUILTIN(StoreInterceptorIC, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);
  TailCallRuntime(Runtime::kStorePropertyWithInterceptor, context, value, slot,
                  vector, receiver, name);
}

TF_BUILTIN(LoadIndexedInterceptorIC, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label if_keyispositivesmi(this), if_keyisinvalid(this);
  Branch(TaggedIsPositiveSmi(key), &if_keyispositivesmi, &if_keyisinvalid);
  BIND(&if_keyispositivesmi);
  TailCallRuntime(Runtime::kLoadElementWithInterceptor, context, receiver, key);

  BIND(&if_keyisinvalid);
  TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver, key, slot,
                  vector);
}

TF_BUILTIN(KeyedHasIC_SloppyArguments, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label miss(this);

  Node* result = HasKeyedSloppyArguments(receiver, key, &miss);
  Return(result);

  BIND(&miss);
  {
    Comment("Miss");
    TailCallRuntime(Runtime::kKeyedHasIC_Miss, context, receiver, key, slot,
                    vector);
  }
}

TF_BUILTIN(HasIndexedInterceptorIC, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label if_keyispositivesmi(this), if_keyisinvalid(this);
  Branch(TaggedIsPositiveSmi(key), &if_keyispositivesmi, &if_keyisinvalid);
  BIND(&if_keyispositivesmi);
  TailCallRuntime(Runtime::kHasElementWithInterceptor, context, receiver, key);

  BIND(&if_keyisinvalid);
  TailCallRuntime(Runtime::kKeyedHasIC_Miss, context, receiver, key, slot,
                  vector);
}

TF_BUILTIN(HasIC_Slow, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kHasProperty, context, receiver, name);
}

}  // namespace internal
}  // namespace v8
