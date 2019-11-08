// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_ACCESSOR_ASSEMBLER_H_
#define V8_IC_ACCESSOR_ASSEMBLER_H_

#include "src/base/optional.h"
#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

namespace compiler {
class CodeAssemblerState;
}

class ExitPoint;

class V8_EXPORT_PRIVATE AccessorAssembler : public CodeStubAssembler {
 public:
  using Node = compiler::Node;

  explicit AccessorAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  void GenerateLoadIC();
  void GenerateLoadIC_Megamorphic();
  void GenerateLoadIC_Noninlined();
  void GenerateLoadIC_NoFeedback();
  void GenerateLoadICTrampoline();
  void GenerateLoadICTrampoline_Megamorphic();
  void GenerateKeyedLoadIC();
  void GenerateKeyedLoadIC_Megamorphic();
  void GenerateKeyedLoadIC_PolymorphicName();
  void GenerateKeyedLoadICTrampoline();
  void GenerateKeyedLoadICTrampoline_Megamorphic();
  void GenerateStoreIC();
  void GenerateStoreICTrampoline();
  void GenerateStoreGlobalIC();
  void GenerateStoreGlobalICTrampoline();
  void GenerateCloneObjectIC();
  void GenerateCloneObjectIC_Slow();
  void GenerateKeyedHasIC();
  void GenerateKeyedHasIC_Megamorphic();
  void GenerateKeyedHasIC_PolymorphicName();

  void GenerateLoadGlobalIC(TypeofMode typeof_mode);
  void GenerateLoadGlobalICTrampoline(TypeofMode typeof_mode);

  void GenerateKeyedStoreIC();
  void GenerateKeyedStoreICTrampoline();

  void GenerateStoreInArrayLiteralIC();

  void TryProbeStubCache(StubCache* stub_cache, Node* receiver,
                         TNode<Object> name, Label* if_handler,
                         TVariable<MaybeObject>* var_handler, Label* if_miss);

  Node* StubCachePrimaryOffsetForTesting(Node* name, Node* map) {
    return StubCachePrimaryOffset(name, map);
  }
  Node* StubCacheSecondaryOffsetForTesting(Node* name, Node* map) {
    return StubCacheSecondaryOffset(name, map);
  }

  struct LoadICParameters {
    LoadICParameters(TNode<Context> context, Node* receiver, TNode<Object> name,
                     TNode<Smi> slot, Node* vector, Node* holder = nullptr)
        : context_(context),
          receiver_(receiver),
          name_(name),
          slot_(slot),
          vector_(vector),
          holder_(holder ? holder : receiver) {}

    LoadICParameters(const LoadICParameters* p, TNode<Object> unique_name)
        : context_(p->context_),
          receiver_(p->receiver_),
          name_(unique_name),
          slot_(p->slot_),
          vector_(p->vector_),
          holder_(p->holder_) {}

    TNode<Context> context() const { return context_; }
    Node* receiver() const { return receiver_; }
    TNode<Object> name() const { return name_; }
    TNode<Smi> slot() const { return slot_; }
    Node* vector() const { return vector_; }
    Node* holder() const { return holder_; }

   private:
    TNode<Context> context_;
    Node* receiver_;
    TNode<Object> name_;
    TNode<Smi> slot_;
    Node* vector_;
    Node* holder_;
  };

  struct LazyLoadICParameters {
    LazyLoadICParameters(LazyNode<Context> context, Node* receiver,
                         LazyNode<Object> name, LazyNode<Smi> slot,
                         Node* vector, Node* holder = nullptr)
        : context_(context),
          receiver_(receiver),
          name_(name),
          slot_(slot),
          vector_(vector),
          holder_(holder ? holder : receiver) {}

    explicit LazyLoadICParameters(const LoadICParameters* p)
        : receiver_(p->receiver()),
          vector_(p->vector()),
          holder_(p->holder()) {
      slot_ = [=] { return p->slot(); };
      context_ = [=] { return p->context(); };
      name_ = [=] { return p->name(); };
    }

    TNode<Context> context() const { return context_(); }
    Node* receiver() const { return receiver_; }
    TNode<Object> name() const { return name_(); }
    TNode<Smi> slot() const { return slot_(); }
    Node* vector() const { return vector_; }
    Node* holder() const { return holder_; }

   private:
    LazyNode<Context> context_;
    Node* receiver_;
    LazyNode<Object> name_;
    LazyNode<Smi> slot_;
    Node* vector_;
    Node* holder_;
  };

  void LoadGlobalIC(TNode<HeapObject> maybe_feedback_vector,
                    const LazyNode<Smi>& lazy_smi_slot,
                    const LazyNode<UintPtrT>& lazy_slot,
                    const LazyNode<Context>& lazy_context,
                    const LazyNode<Name>& lazy_name, TypeofMode typeof_mode,
                    ExitPoint* exit_point);

  // Specialized LoadIC for inlined bytecode handler, hand-tuned to omit frame
  // construction on common paths.
  void LoadIC_BytecodeHandler(const LazyLoadICParameters* p,
                              ExitPoint* exit_point);

  // Loads dataX field from the DataHandler object.
  TNode<MaybeObject> LoadHandlerDataField(SloppyTNode<DataHandler> handler,
                                          int data_index);

 protected:
  struct StoreICParameters : public LoadICParameters {
    StoreICParameters(TNode<Context> context, Node* receiver,
                      TNode<Object> name, SloppyTNode<Object> value,
                      TNode<Smi> slot, Node* vector)
        : LoadICParameters(context, receiver, name, slot, vector),
          value_(value) {}

    SloppyTNode<Object> value() const { return value_; }

   private:
    SloppyTNode<Object> value_;
  };

  enum class LoadAccessMode { kLoad, kHas };
  enum class ICMode { kNonGlobalIC, kGlobalIC };
  enum ElementSupport { kOnlyProperties, kSupportElements };
  void HandleStoreICHandlerCase(
      const StoreICParameters* p, TNode<MaybeObject> handler, Label* miss,
      ICMode ic_mode, ElementSupport support_elements = kOnlyProperties);
  enum StoreTransitionMapFlags {
    kCheckPrototypeValidity = 1 << 0,
    kValidateTransitionHandler = 1 << 1,
    kStoreTransitionMapFlagsMask =
        kCheckPrototypeValidity | kValidateTransitionHandler,
  };
  void HandleStoreICTransitionMapHandlerCase(const StoreICParameters* p,
                                             TNode<Map> transition_map,
                                             Label* miss,
                                             StoreTransitionMapFlags flags);

  void JumpIfDataProperty(TNode<Uint32T> details, Label* writable,
                          Label* readonly);

  void InvalidateValidityCellIfPrototype(Node* map, Node* bitfield3 = nullptr);

  void OverwriteExistingFastDataProperty(SloppyTNode<HeapObject> object,
                                         TNode<Map> object_map,
                                         TNode<DescriptorArray> descriptors,
                                         TNode<IntPtrT> descriptor_name_index,
                                         TNode<Uint32T> details,
                                         TNode<Object> value, Label* slow,
                                         bool do_transitioning_store);

  void CheckFieldType(TNode<DescriptorArray> descriptors,
                      TNode<IntPtrT> name_index, TNode<Word32T> representation,
                      Node* value, Label* bailout);

 private:
  // Stub generation entry points.

  // LoadIC contains the full LoadIC logic, while LoadIC_Noninlined contains
  // logic not inlined into Ignition bytecode handlers.
  void LoadIC(const LoadICParameters* p);
  void LoadIC_Noninlined(const LoadICParameters* p, TNode<Map> receiver_map,
                         TNode<HeapObject> feedback,
                         TVariable<MaybeObject>* var_handler, Label* if_handler,
                         Label* miss, ExitPoint* exit_point);

  TNode<Object> LoadDescriptorValue(TNode<Map> map,
                                    TNode<IntPtrT> descriptor_entry);
  TNode<MaybeObject> LoadDescriptorValueOrFieldType(
      TNode<Map> map, TNode<IntPtrT> descriptor_entry);

  void LoadIC_NoFeedback(const LoadICParameters* p);

  void KeyedLoadIC(const LoadICParameters* p, LoadAccessMode access_mode);
  void KeyedLoadICGeneric(const LoadICParameters* p);
  void KeyedLoadICPolymorphicName(const LoadICParameters* p,
                                  LoadAccessMode access_mode);
  void StoreIC(const StoreICParameters* p);
  void StoreGlobalIC(const StoreICParameters* p);
  void StoreGlobalIC_PropertyCellCase(Node* property_cell, TNode<Object> value,
                                      ExitPoint* exit_point, Label* miss);
  void KeyedStoreIC(const StoreICParameters* p);
  void StoreInArrayLiteralIC(const StoreICParameters* p);

  // IC dispatcher behavior.

  // Checks monomorphic case. Returns {feedback} entry of the vector.
  TNode<MaybeObject> TryMonomorphicCase(
      TNode<Smi> slot, TNode<FeedbackVector> vector, TNode<Map> receiver_map,
      Label* if_handler, TVariable<MaybeObject>* var_handler, Label* if_miss);
  void HandlePolymorphicCase(TNode<Map> receiver_map,
                             TNode<WeakFixedArray> feedback, Label* if_handler,
                             TVariable<MaybeObject>* var_handler,
                             Label* if_miss);

  // LoadIC implementation.
  void HandleLoadICHandlerCase(
      const LazyLoadICParameters* p, TNode<Object> handler, Label* miss,
      ExitPoint* exit_point, ICMode ic_mode = ICMode::kNonGlobalIC,
      OnNonExistent on_nonexistent = OnNonExistent::kReturnUndefined,
      ElementSupport support_elements = kOnlyProperties,
      LoadAccessMode access_mode = LoadAccessMode::kLoad);

  void HandleLoadICSmiHandlerCase(
      const LazyLoadICParameters* p, SloppyTNode<HeapObject> holder,
      SloppyTNode<Smi> smi_handler, SloppyTNode<Object> handler, Label* miss,
      ExitPoint* exit_point, ICMode ic_mode, OnNonExistent on_nonexistent,
      ElementSupport support_elements, LoadAccessMode access_mode);

  void HandleLoadICProtoHandler(const LazyLoadICParameters* p,
                                TNode<DataHandler> handler,
                                Variable* var_holder, Variable* var_smi_handler,
                                Label* if_smi_handler, Label* miss,
                                ExitPoint* exit_point, ICMode ic_mode,
                                LoadAccessMode access_mode);

  void HandleLoadCallbackProperty(const LazyLoadICParameters* p,
                                  TNode<JSObject> holder,
                                  TNode<WordT> handler_word,
                                  ExitPoint* exit_point);

  void HandleLoadAccessor(const LazyLoadICParameters* p,
                          TNode<CallHandlerInfo> call_handler_info,
                          TNode<WordT> handler_word, TNode<DataHandler> handler,
                          TNode<IntPtrT> handler_kind, ExitPoint* exit_point);

  void HandleLoadField(SloppyTNode<JSObject> holder, TNode<WordT> handler_word,
                       Variable* var_double_value, Label* rebox_double,
                       Label* miss, ExitPoint* exit_point);

  void EmitAccessCheck(TNode<Context> expected_native_context,
                       TNode<Context> context, TNode<Object> receiver,
                       Label* can_access, Label* miss);

  void HandleLoadICSmiHandlerLoadNamedCase(
      const LazyLoadICParameters* p, TNode<HeapObject> holder,
      TNode<IntPtrT> handler_kind, TNode<WordT> handler_word,
      Label* rebox_double, Variable* var_double_value,
      SloppyTNode<Object> handler, Label* miss, ExitPoint* exit_point,
      ICMode ic_mode, OnNonExistent on_nonexistent,
      ElementSupport support_elements);

  void HandleLoadICSmiHandlerHasNamedCase(const LazyLoadICParameters* p,
                                          TNode<HeapObject> holder,
                                          TNode<IntPtrT> handler_kind,
                                          Label* miss, ExitPoint* exit_point,
                                          ICMode ic_mode);

  // LoadGlobalIC implementation.

  void LoadGlobalIC_TryPropertyCellCase(TNode<FeedbackVector> vector,
                                        TNode<UintPtrT> slot,
                                        const LazyNode<Context>& lazy_context,
                                        ExitPoint* exit_point,
                                        Label* try_handler, Label* miss);

  void LoadGlobalIC_TryHandlerCase(TNode<FeedbackVector> vector,
                                   TNode<UintPtrT> slot,
                                   const LazyNode<Smi>& lazy_smi_slot,
                                   const LazyNode<Context>& lazy_context,
                                   const LazyNode<Name>& lazy_name,
                                   TypeofMode typeof_mode,
                                   ExitPoint* exit_point, Label* miss);

  // StoreIC implementation.

  void HandleStoreICProtoHandler(const StoreICParameters* p,
                                 TNode<StoreHandler> handler, Label* miss,
                                 ICMode ic_mode,
                                 ElementSupport support_elements);
  void HandleStoreICSmiHandlerCase(SloppyTNode<Word32T> handler_word,
                                   SloppyTNode<JSObject> holder,
                                   SloppyTNode<Object> value, Label* miss);
  void HandleStoreFieldAndReturn(TNode<Word32T> handler_word,
                                 TNode<JSObject> holder, TNode<Object> value,
                                 base::Optional<TNode<Float64T>> double_value,
                                 Representation representation, Label* miss);

  void CheckPrototypeValidityCell(TNode<Object> maybe_validity_cell,
                                  Label* miss);
  void HandleStoreICNativeDataProperty(const StoreICParameters* p,
                                       SloppyTNode<HeapObject> holder,
                                       TNode<Word32T> handler_word);

  void HandleStoreToProxy(const StoreICParameters* p, Node* proxy, Label* miss,
                          ElementSupport support_elements);

  void HandleStoreAccessor(const StoreICParameters* p,
                           SloppyTNode<HeapObject> holder,
                           TNode<Word32T> handler_word);

  // KeyedLoadIC_Generic implementation.

  void GenericElementLoad(Node* receiver, TNode<Map> receiver_map,
                          SloppyTNode<Int32T> instance_type, Node* index,
                          Label* slow);

  enum UseStubCache { kUseStubCache, kDontUseStubCache };
  void GenericPropertyLoad(Node* receiver, TNode<Map> receiver_map,
                           SloppyTNode<Int32T> instance_type,
                           const LoadICParameters* p, Label* slow,
                           UseStubCache use_stub_cache = kUseStubCache);

  // Low-level helpers.

  using OnCodeHandler = std::function<void(TNode<Code> code_handler)>;
  using OnFoundOnReceiver = std::function<void(TNode<NameDictionary> properties,
                                               TNode<IntPtrT> name_index)>;

  template <typename ICHandler, typename ICParameters>
  TNode<Object> HandleProtoHandler(
      const ICParameters* p, TNode<DataHandler> handler,
      const OnCodeHandler& on_code_handler,
      const OnFoundOnReceiver& on_found_on_receiver, Label* miss,
      ICMode ic_mode);

  void CheckHeapObjectTypeMatchesDescriptor(TNode<Word32T> handler_word,
                                            TNode<JSObject> holder,
                                            TNode<Object> value,
                                            Label* bailout);
  // Double fields store double values in a mutable box, where stores are
  // writes into this box rather than HeapNumber assignment.
  void CheckDescriptorConsidersNumbersMutable(TNode<Word32T> handler_word,
                                              TNode<JSObject> holder,
                                              Label* bailout);

  // Extends properties backing store by JSObject::kFieldsAdded elements,
  // returns updated properties backing store.
  Node* ExtendPropertiesBackingStore(Node* object, Node* index);

  void EmitFastElementsBoundsCheck(Node* object, Node* elements,
                                   Node* intptr_index,
                                   Node* is_jsarray_condition, Label* miss);
  void EmitElementLoad(Node* object, TNode<Word32T> elements_kind,
                       SloppyTNode<IntPtrT> key, Node* is_jsarray_condition,
                       Label* if_hole, Label* rebox_double,
                       Variable* var_double_value,
                       Label* unimplemented_elements_kind, Label* out_of_bounds,
                       Label* miss, ExitPoint* exit_point,
                       LoadAccessMode access_mode = LoadAccessMode::kLoad);
  void NameDictionaryNegativeLookup(Node* object, SloppyTNode<Name> name,
                                    Label* miss);
  TNode<BoolT> IsPropertyDetailsConst(TNode<Uint32T> details);

  // Stub cache access helpers.

  // This enum is used here as a replacement for StubCache::Table to avoid
  // including stub cache header.
  enum StubCacheTable : int;

  Node* StubCachePrimaryOffset(Node* name, Node* map);
  Node* StubCacheSecondaryOffset(Node* name, Node* seed);

  void TryProbeStubCacheTable(StubCache* stub_cache, StubCacheTable table_id,
                              Node* entry_offset, TNode<Object> name,
                              TNode<Map> map, Label* if_handler,
                              TVariable<MaybeObject>* var_handler,
                              Label* if_miss);

  void BranchIfPrototypesHaveNoElements(TNode<Map> receiver_map,
                                        Label* definitely_no_elements,
                                        Label* possibly_elements);
};

// Abstraction over direct and indirect exit points. Direct exits correspond to
// tailcalls and Return, while indirect exits store the result in a variable
// and then jump to an exit label.
class ExitPoint {
 private:
  using Node = compiler::Node;
  using CodeAssemblerLabel = compiler::CodeAssemblerLabel;
  using CodeAssemblerVariable = compiler::CodeAssemblerVariable;

 public:
  using IndirectReturnHandler = std::function<void(Node* result)>;

  explicit ExitPoint(CodeStubAssembler* assembler)
      : ExitPoint(assembler, nullptr) {}

  ExitPoint(CodeStubAssembler* assembler,
            const IndirectReturnHandler& indirect_return_handler)
      : asm_(assembler), indirect_return_handler_(indirect_return_handler) {}

  ExitPoint(CodeStubAssembler* assembler, CodeAssemblerLabel* out,
            CodeAssemblerVariable* var_result)
      : ExitPoint(assembler, [=](Node* result) {
          var_result->Bind(result);
          assembler->Goto(out);
        }) {
    DCHECK_EQ(out != nullptr, var_result != nullptr);
  }

  template <class... TArgs>
  void ReturnCallRuntime(Runtime::FunctionId function, Node* context,
                         TArgs... args) {
    if (IsDirect()) {
      asm_->TailCallRuntime(function, context, args...);
    } else {
      indirect_return_handler_(asm_->CallRuntime(function, context, args...));
    }
  }

  template <class... TArgs>
  void ReturnCallStub(Callable const& callable, Node* context, TArgs... args) {
    if (IsDirect()) {
      asm_->TailCallStub(callable, context, args...);
    } else {
      indirect_return_handler_(asm_->CallStub(callable, context, args...));
    }
  }

  template <class... TArgs>
  void ReturnCallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                      Node* context, TArgs... args) {
    if (IsDirect()) {
      asm_->TailCallStub(descriptor, target, context, args...);
    } else {
      indirect_return_handler_(
          asm_->CallStub(descriptor, target, context, args...));
    }
  }

  void Return(Node* const result) {
    if (IsDirect()) {
      asm_->Return(result);
    } else {
      indirect_return_handler_(result);
    }
  }

  bool IsDirect() const { return !indirect_return_handler_; }

 private:
  CodeStubAssembler* const asm_;
  IndirectReturnHandler indirect_return_handler_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_ACCESSOR_ASSEMBLER_H_
