// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_ACCESSOR_ASSEMBLER_H_
#define V8_IC_ACCESSOR_ASSEMBLER_H_

#include "src/base/optional.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/compiler/code-assembler.h"

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
  void GenerateLoadGlobalIC_NoFeedback();
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

  void TryProbeStubCache(StubCache* stub_cache, TNode<Object> receiver,
                         TNode<Name> name, Label* if_handler,
                         TVariable<MaybeObject>* var_handler, Label* if_miss);

  TNode<IntPtrT> StubCachePrimaryOffsetForTesting(TNode<Name> name,
                                                  TNode<Map> map) {
    return StubCachePrimaryOffset(name, map);
  }
  TNode<IntPtrT> StubCacheSecondaryOffsetForTesting(TNode<Name> name,
                                                    TNode<IntPtrT> seed) {
    return StubCacheSecondaryOffset(name, seed);
  }

  struct LoadICParameters {
    LoadICParameters(TNode<Context> context,
                     base::Optional<TNode<Object>> receiver, TNode<Object> name,
                     TNode<Smi> slot, TNode<HeapObject> vector,
                     base::Optional<TNode<Object>> holder = base::nullopt)
        : context_(context),
          receiver_(receiver),
          name_(name),
          slot_(slot),
          vector_(vector),
          holder_(holder ? holder.value() : receiver) {}

    LoadICParameters(const LoadICParameters* p, TNode<Object> unique_name)
        : context_(p->context_),
          receiver_(p->receiver_),
          name_(unique_name),
          slot_(p->slot_),
          vector_(p->vector_),
          holder_(p->holder_) {}

    TNode<Context> context() const { return context_; }
    TNode<Object> receiver() const { return receiver_.value(); }
    TNode<Object> name() const { return name_; }
    TNode<Smi> slot() const { return slot_; }
    TNode<HeapObject> vector() const { return vector_; }
    TNode<Object> holder() const { return holder_.value(); }
    bool receiver_is_null() const { return !receiver_.has_value(); }

   private:
    TNode<Context> context_;
    base::Optional<TNode<Object>> receiver_;
    TNode<Object> name_;
    TNode<Smi> slot_;
    TNode<HeapObject> vector_;
    base::Optional<TNode<Object>> holder_;
  };

  void LoadGlobalIC(TNode<HeapObject> maybe_feedback_vector,
                    TNode<Smi> smi_slot, TNode<UintPtrT> slot,
                    TNode<Context> context, TNode<Name> name,
                    TypeofMode typeof_mode, ExitPoint* exit_point);

  // Specialized LoadIC for inlined bytecode handler, hand-tuned to omit frame
  // construction on common paths.
  void LoadIC_BytecodeHandler(const LoadICParameters* p, ExitPoint* exit_point);

  // Loads dataX field from the DataHandler object.
  TNode<MaybeObject> LoadHandlerDataField(TNode<DataHandler> handler,
                                          int data_index);

 protected:
  struct StoreICParameters : public LoadICParameters {
    StoreICParameters(TNode<Context> context,
                      base::Optional<TNode<Object>> receiver,
                      TNode<Object> name, TNode<Object> value, TNode<Smi> slot,
                      TNode<HeapObject> vector)
        : LoadICParameters(context, receiver, name, slot, vector),
          value_(value) {}

    TNode<Object> value() const { return value_; }

   private:
    TNode<Object> value_;
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

  void InvalidateValidityCellIfPrototype(
      TNode<Map> map, base::Optional<TNode<Uint32T>> bitfield3 = base::nullopt);

  void OverwriteExistingFastDataProperty(TNode<HeapObject> object,
                                         TNode<Map> object_map,
                                         TNode<DescriptorArray> descriptors,
                                         TNode<IntPtrT> descriptor_name_index,
                                         TNode<Uint32T> details,
                                         TNode<Object> value, Label* slow,
                                         bool do_transitioning_store);

  void CheckFieldType(TNode<DescriptorArray> descriptors,
                      TNode<IntPtrT> name_index, TNode<Word32T> representation,
                      TNode<Object> value, Label* bailout);

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

  void LoadIC_NoFeedback(const LoadICParameters* p, TNode<Smi> smi_typeof_mode);
  void LoadGlobalIC_NoFeedback(TNode<Context> context, TNode<Object> name,
                               TNode<Smi> smi_typeof_mode);

  void KeyedLoadIC(const LoadICParameters* p, LoadAccessMode access_mode);
  void KeyedLoadICGeneric(const LoadICParameters* p);
  void KeyedLoadICPolymorphicName(const LoadICParameters* p,
                                  LoadAccessMode access_mode);
  void StoreIC(const StoreICParameters* p);
  void StoreGlobalIC(const StoreICParameters* p);
  void StoreGlobalIC_PropertyCellCase(TNode<PropertyCell> property_cell,
                                      TNode<Object> value,
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
      const LoadICParameters* p, TNode<Object> handler, Label* miss,
      ExitPoint* exit_point, ICMode ic_mode = ICMode::kNonGlobalIC,
      OnNonExistent on_nonexistent = OnNonExistent::kReturnUndefined,
      ElementSupport support_elements = kOnlyProperties,
      LoadAccessMode access_mode = LoadAccessMode::kLoad);

  void HandleLoadICSmiHandlerCase(const LoadICParameters* p,
                                  TNode<Object> holder, TNode<Smi> smi_handler,
                                  TNode<Object> handler, Label* miss,
                                  ExitPoint* exit_point, ICMode ic_mode,
                                  OnNonExistent on_nonexistent,
                                  ElementSupport support_elements,
                                  LoadAccessMode access_mode);

  void HandleLoadICProtoHandler(const LoadICParameters* p,
                                TNode<DataHandler> handler,
                                TVariable<Object>* var_holder,
                                TVariable<Object>* var_smi_handler,
                                Label* if_smi_handler, Label* miss,
                                ExitPoint* exit_point, ICMode ic_mode,
                                LoadAccessMode access_mode);

  void HandleLoadCallbackProperty(const LoadICParameters* p,
                                  TNode<JSObject> holder,
                                  TNode<WordT> handler_word,
                                  ExitPoint* exit_point);

  void HandleLoadAccessor(const LoadICParameters* p,
                          TNode<CallHandlerInfo> call_handler_info,
                          TNode<WordT> handler_word, TNode<DataHandler> handler,
                          TNode<IntPtrT> handler_kind, ExitPoint* exit_point);

  void HandleLoadField(TNode<JSObject> holder, TNode<WordT> handler_word,
                       TVariable<Float64T>* var_double_value,
                       Label* rebox_double, Label* miss, ExitPoint* exit_point);

  void EmitAccessCheck(TNode<Context> expected_native_context,
                       TNode<Context> context, TNode<Object> receiver,
                       Label* can_access, Label* miss);

  void HandleLoadICSmiHandlerLoadNamedCase(
      const LoadICParameters* p, TNode<Object> holder,
      TNode<IntPtrT> handler_kind, TNode<WordT> handler_word,
      Label* rebox_double, TVariable<Float64T>* var_double_value,
      TNode<Object> handler, Label* miss, ExitPoint* exit_point, ICMode ic_mode,
      OnNonExistent on_nonexistent, ElementSupport support_elements);

  void HandleLoadICSmiHandlerHasNamedCase(const LoadICParameters* p,
                                          TNode<Object> holder,
                                          TNode<IntPtrT> handler_kind,
                                          Label* miss, ExitPoint* exit_point,
                                          ICMode ic_mode);

  // LoadGlobalIC implementation.

  void LoadGlobalIC_TryPropertyCellCase(TNode<FeedbackVector> vector,
                                        TNode<UintPtrT> slot,
                                        TNode<Context> context,
                                        ExitPoint* exit_point,
                                        Label* try_handler, Label* miss);

  void LoadGlobalIC_TryHandlerCase(TNode<FeedbackVector> vector,
                                   TNode<UintPtrT> slot, TNode<Smi> smi_slot,
                                   TNode<Context> context, TNode<Name> name,
                                   TypeofMode typeof_mode,
                                   ExitPoint* exit_point, Label* miss);

  // This is a copy of ScriptContextTable::Lookup. They should be kept in sync.
  void ScriptContextTableLookup(TNode<Name> name,
                                TNode<NativeContext> native_context,
                                Label* found_hole, Label* not_found);

  // StoreIC implementation.

  void HandleStoreICProtoHandler(const StoreICParameters* p,
                                 TNode<StoreHandler> handler, Label* miss,
                                 ICMode ic_mode,
                                 ElementSupport support_elements);
  void HandleStoreICSmiHandlerCase(TNode<Word32T> handler_word,
                                   TNode<JSObject> holder, TNode<Object> value,
                                   Label* miss);
  void HandleStoreFieldAndReturn(TNode<Word32T> handler_word,
                                 TNode<JSObject> holder, TNode<Object> value,
                                 base::Optional<TNode<Float64T>> double_value,
                                 Representation representation, Label* miss);

  void CheckPrototypeValidityCell(TNode<Object> maybe_validity_cell,
                                  Label* miss);
  void HandleStoreICNativeDataProperty(const StoreICParameters* p,
                                       TNode<HeapObject> holder,
                                       TNode<Word32T> handler_word);

  void HandleStoreToProxy(const StoreICParameters* p, TNode<JSProxy> proxy,
                          Label* miss, ElementSupport support_elements);

  void HandleStoreAccessor(const StoreICParameters* p, TNode<HeapObject> holder,
                           TNode<Word32T> handler_word);

  // KeyedLoadIC_Generic implementation.

  void GenericElementLoad(TNode<HeapObject> receiver, TNode<Map> receiver_map,
                          TNode<Int32T> instance_type, TNode<IntPtrT> index,
                          Label* slow);

  enum UseStubCache { kUseStubCache, kDontUseStubCache };
  void GenericPropertyLoad(TNode<HeapObject> receiver, TNode<Map> receiver_map,
                           TNode<Int32T> instance_type,
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
  TNode<PropertyArray> ExtendPropertiesBackingStore(TNode<HeapObject> object,
                                                    TNode<IntPtrT> index);

  void EmitFastElementsBoundsCheck(TNode<JSObject> object,
                                   TNode<FixedArrayBase> elements,
                                   TNode<IntPtrT> intptr_index,
                                   TNode<BoolT> is_jsarray_condition,
                                   Label* miss);
  void EmitElementLoad(TNode<HeapObject> object, TNode<Word32T> elements_kind,
                       TNode<IntPtrT> key, TNode<BoolT> is_jsarray_condition,
                       Label* if_hole, Label* rebox_double,
                       TVariable<Float64T>* var_double_value,
                       Label* unimplemented_elements_kind, Label* out_of_bounds,
                       Label* miss, ExitPoint* exit_point,
                       LoadAccessMode access_mode = LoadAccessMode::kLoad);
  TNode<BoolT> IsPropertyDetailsConst(TNode<Uint32T> details);

  // Stub cache access helpers.

  // This enum is used here as a replacement for StubCache::Table to avoid
  // including stub cache header.
  enum StubCacheTable : int;

  TNode<IntPtrT> StubCachePrimaryOffset(TNode<Name> name, TNode<Map> map);
  TNode<IntPtrT> StubCacheSecondaryOffset(TNode<Name> name,
                                          TNode<IntPtrT> seed);

  void TryProbeStubCacheTable(StubCache* stub_cache, StubCacheTable table_id,
                              TNode<IntPtrT> entry_offset, TNode<Object> name,
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
  using IndirectReturnHandler = std::function<void(TNode<Object> result)>;

  explicit ExitPoint(CodeStubAssembler* assembler)
      : ExitPoint(assembler, nullptr) {}

  ExitPoint(CodeStubAssembler* assembler,
            const IndirectReturnHandler& indirect_return_handler)
      : asm_(assembler), indirect_return_handler_(indirect_return_handler) {}

  ExitPoint(CodeStubAssembler* assembler, CodeAssemblerLabel* out,
            compiler::CodeAssembler::TVariable<Object>* var_result)
      : ExitPoint(assembler, [=](TNode<Object> result) {
          *var_result = result;
          assembler->Goto(out);
        }) {
    DCHECK_EQ(out != nullptr, var_result != nullptr);
  }

  template <class... TArgs>
  void ReturnCallRuntime(Runtime::FunctionId function, TNode<Context> context,
                         TArgs... args) {
    if (IsDirect()) {
      asm_->TailCallRuntime(function, context, args...);
    } else {
      indirect_return_handler_(asm_->CallRuntime(function, context, args...));
    }
  }

  template <class... TArgs>
  void ReturnCallStub(Callable const& callable, TNode<Context> context,
                      TArgs... args) {
    if (IsDirect()) {
      asm_->TailCallStub(callable, context, args...);
    } else {
      indirect_return_handler_(asm_->CallStub(callable, context, args...));
    }
  }

  template <class... TArgs>
  void ReturnCallStub(const CallInterfaceDescriptor& descriptor,
                      TNode<Code> target, TNode<Context> context,
                      TArgs... args) {
    if (IsDirect()) {
      asm_->TailCallStub(descriptor, target, context, args...);
    } else {
      indirect_return_handler_(
          asm_->CallStub(descriptor, target, context, args...));
    }
  }

  void Return(const TNode<Object> result) {
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
