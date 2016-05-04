// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_H_
#define V8_IC_H_

#include "src/ic/ic-state.h"
#include "src/macro-assembler.h"
#include "src/messages.h"

namespace v8 {
namespace internal {

//
// IC is the base class for LoadIC, StoreIC, KeyedLoadIC, and KeyedStoreIC.
//
class IC {
 public:
  // Alias the inline cache state type to make the IC code more readable.
  typedef InlineCacheState State;

  // The IC code is either invoked with no extra frames on the stack
  // or with a single extra frame for supporting calls.
  enum FrameDepth { NO_EXTRA_FRAME = 0, EXTRA_CALL_FRAME = 1 };

  // Construct the IC structure with the given number of extra
  // JavaScript frames on the stack.
  IC(FrameDepth depth, Isolate* isolate, FeedbackNexus* nexus = NULL);
  virtual ~IC() {}

  State state() const { return state_; }
  inline Address address() const;

  // Compute the current IC state based on the target stub, receiver and name.
  void UpdateState(Handle<Object> receiver, Handle<Object> name);

  bool IsNameCompatibleWithPrototypeFailure(Handle<Object> name);
  void MarkPrototypeFailure(Handle<Object> name) {
    DCHECK(IsNameCompatibleWithPrototypeFailure(name));
    old_state_ = state_;
    state_ = PROTOTYPE_FAILURE;
  }

  // Clear the inline cache to initial state.
  static void Clear(Isolate* isolate, Address address, Address constant_pool);

#ifdef DEBUG
  bool IsLoadStub() const {
    return target()->is_load_stub() || target()->is_keyed_load_stub();
  }

  bool IsStoreStub() const {
    return target()->is_store_stub() || target()->is_keyed_store_stub();
  }

  bool IsCallStub() const { return target()->is_call_stub(); }
#endif

  static inline Handle<Map> GetHandlerCacheHolder(Handle<Map> receiver_map,
                                                  bool receiver_is_holder,
                                                  Isolate* isolate,
                                                  CacheHolderFlag* flag);
  static inline Handle<Map> GetICCacheHolder(Handle<Map> receiver_map,
                                             Isolate* isolate,
                                             CacheHolderFlag* flag);

  static bool IsCleared(Code* code) {
    InlineCacheState state = code->ic_state();
    return !FLAG_use_ic || state == UNINITIALIZED || state == PREMONOMORPHIC;
  }

  static bool IsCleared(FeedbackNexus* nexus) {
    InlineCacheState state = nexus->StateFromFeedback();
    return !FLAG_use_ic || state == UNINITIALIZED || state == PREMONOMORPHIC;
  }

  static bool ICUseVector(Code::Kind kind) {
    return kind == Code::LOAD_IC || kind == Code::KEYED_LOAD_IC ||
           kind == Code::CALL_IC || kind == Code::STORE_IC ||
           kind == Code::KEYED_STORE_IC;
  }

 protected:
  // Get the call-site target; used for determining the state.
  Handle<Code> target() const { return target_; }

  Address fp() const { return fp_; }
  Address pc() const { return *pc_address_; }
  Isolate* isolate() const { return isolate_; }

  // Get the shared function info of the caller.
  SharedFunctionInfo* GetSharedFunctionInfo() const;
  // Get the code object of the caller.
  Code* GetCode() const;

  bool AddressIsOptimizedCode() const;
  inline bool AddressIsDeoptimizedCode() const;
  inline static bool AddressIsDeoptimizedCode(Isolate* isolate,
                                              Address address);

  // Set the call-site target.
  inline void set_target(Code* code);
  bool is_target_set() { return target_set_; }
  bool is_vector_set() { return vector_set_; }

  bool UseVector() const {
    bool use = ICUseVector(kind());
    // If we are supposed to use the nexus, verify the nexus is non-null.
    DCHECK(!use || nexus_ != NULL);
    return use;
  }

  // Configure for most states.
  void ConfigureVectorState(IC::State new_state, Handle<Object> key);
  // Configure the vector for MONOMORPHIC.
  void ConfigureVectorState(Handle<Name> name, Handle<Map> map,
                            Handle<Code> handler);
  // Configure the vector for POLYMORPHIC.
  void ConfigureVectorState(Handle<Name> name, MapHandleList* maps,
                            CodeHandleList* handlers);
  // Configure the vector for POLYMORPHIC with transitions (only for element
  // keyed stores).
  void ConfigureVectorState(MapHandleList* maps,
                            MapHandleList* transitioned_maps,
                            CodeHandleList* handlers);

  char TransitionMarkFromState(IC::State state);
  void TraceIC(const char* type, Handle<Object> name);
  void TraceIC(const char* type, Handle<Object> name, State old_state,
               State new_state);

  MaybeHandle<Object> TypeError(MessageTemplate::Template,
                                Handle<Object> object, Handle<Object> key);
  MaybeHandle<Object> ReferenceError(Handle<Name> name);

  // Access the target code for the given IC address.
  static inline Code* GetTargetAtAddress(Address address,
                                         Address constant_pool);
  static inline void SetTargetAtAddress(Address address, Code* target,
                                        Address constant_pool);
  static void OnTypeFeedbackChanged(Isolate* isolate, Address address,
                                    State old_state, State new_state,
                                    bool target_remains_ic_stub);
  // As a vector-based IC, type feedback must be updated differently.
  static void OnTypeFeedbackChanged(Isolate* isolate, Code* host);
  static void PostPatching(Address address, Code* target, Code* old_target);

  // Compute the handler either by compiling or by retrieving a cached version.
  Handle<Code> ComputeHandler(LookupIterator* lookup,
                              Handle<Object> value = Handle<Code>::null());
  virtual Handle<Code> CompileHandler(LookupIterator* lookup,
                                      Handle<Object> value,
                                      CacheHolderFlag cache_holder) {
    UNREACHABLE();
    return Handle<Code>::null();
  }

  void UpdateMonomorphicIC(Handle<Code> handler, Handle<Name> name);
  bool UpdatePolymorphicIC(Handle<Name> name, Handle<Code> code);
  void UpdateMegamorphicCache(Map* map, Name* name, Code* code);

  void CopyICToMegamorphicCache(Handle<Name> name);
  bool IsTransitionOfMonomorphicTarget(Map* source_map, Map* target_map);
  void PatchCache(Handle<Name> name, Handle<Code> code);
  Code::Kind kind() const { return kind_; }
  Code::Kind handler_kind() const {
    if (kind_ == Code::KEYED_LOAD_IC) return Code::LOAD_IC;
    DCHECK(kind_ == Code::LOAD_IC || kind_ == Code::STORE_IC ||
           kind_ == Code::KEYED_STORE_IC);
    return kind_;
  }
  virtual Handle<Code> megamorphic_stub() {
    UNREACHABLE();
    return Handle<Code>::null();
  }

  bool TryRemoveInvalidPrototypeDependentStub(Handle<Object> receiver,
                                              Handle<String> name);

  ExtraICState extra_ic_state() const { return extra_ic_state_; }
  void set_extra_ic_state(ExtraICState state) { extra_ic_state_ = state; }

  Handle<Map> receiver_map() { return receiver_map_; }
  void update_receiver_map(Handle<Object> receiver) {
    if (receiver->IsSmi()) {
      receiver_map_ = isolate_->factory()->heap_number_map();
    } else {
      receiver_map_ = handle(HeapObject::cast(*receiver)->map());
    }
  }

  void TargetMaps(MapHandleList* list) {
    FindTargetMaps();
    for (int i = 0; i < target_maps_.length(); i++) {
      list->Add(target_maps_.at(i));
    }
  }

  Map* FirstTargetMap() {
    FindTargetMaps();
    return target_maps_.length() > 0 ? *target_maps_.at(0) : NULL;
  }

  inline void UpdateTarget();

  Handle<TypeFeedbackVector> vector() const { return nexus()->vector_handle(); }
  FeedbackVectorSlot slot() const { return nexus()->slot(); }
  State saved_state() const {
    return state() == PROTOTYPE_FAILURE ? old_state_ : state();
  }

  template <class NexusClass>
  NexusClass* casted_nexus() {
    return static_cast<NexusClass*>(nexus_);
  }
  FeedbackNexus* nexus() const { return nexus_; }

  inline Code* get_host();

 private:
  inline Code* raw_target() const;
  inline Address constant_pool() const;
  inline Address raw_constant_pool() const;

  void FindTargetMaps() {
    if (target_maps_set_) return;
    target_maps_set_ = true;
    if (UseVector()) {
      nexus()->ExtractMaps(&target_maps_);
    } else {
      if (state_ == MONOMORPHIC) {
        Map* map = target_->FindFirstMap();
        if (map != NULL) target_maps_.Add(handle(map));
      } else if (state_ != UNINITIALIZED && state_ != PREMONOMORPHIC) {
        target_->FindAllMaps(&target_maps_);
      }
    }
  }

  // Frame pointer for the frame that uses (calls) the IC.
  Address fp_;

  // All access to the program counter and constant pool of an IC structure is
  // indirect to make the code GC safe. This feature is crucial since
  // GetProperty and SetProperty are called and they in turn might
  // invoke the garbage collector.
  Address* pc_address_;

  // The constant pool of the code which originally called the IC (which might
  // be for the breakpointed copy of the original code).
  Address* constant_pool_address_;

  Isolate* isolate_;

  // The original code target that missed.
  Handle<Code> target_;
  bool target_set_;
  bool vector_set_;
  State old_state_;  // For saving if we marked as prototype failure.
  State state_;
  Code::Kind kind_;
  Handle<Map> receiver_map_;
  MaybeHandle<Code> maybe_handler_;

  ExtraICState extra_ic_state_;
  MapHandleList target_maps_;
  bool target_maps_set_;

  FeedbackNexus* nexus_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IC);
};


class CallIC : public IC {
 public:
  CallIC(Isolate* isolate, CallICNexus* nexus)
      : IC(EXTRA_CALL_FRAME, isolate, nexus) {
    DCHECK(nexus != NULL);
  }

  void HandleMiss(Handle<Object> function);

  // Code generator routines.
  static Handle<Code> initialize_stub(Isolate* isolate, int argc,
                                      ConvertReceiverMode mode,
                                      TailCallMode tail_call_mode);
  static Handle<Code> initialize_stub_in_optimized_code(
      Isolate* isolate, int argc, ConvertReceiverMode mode,
      TailCallMode tail_call_mode);

  static void Clear(Isolate* isolate, Code* host, CallICNexus* nexus);
};


class LoadIC : public IC {
 public:
  TypeofMode typeof_mode() const {
    return LoadICState::GetTypeofMode(extra_ic_state());
  }

  LoadIC(FrameDepth depth, Isolate* isolate, FeedbackNexus* nexus = NULL)
      : IC(depth, isolate, nexus) {
    DCHECK(nexus != NULL);
    DCHECK(IsLoadStub());
  }

  bool ShouldThrowReferenceError(Handle<Object> receiver) {
    return receiver->IsJSGlobalObject() && typeof_mode() == NOT_INSIDE_TYPEOF;
  }

  // Code generator routines.

  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateRuntimeGetProperty(MacroAssembler* masm);
  static void GenerateNormal(MacroAssembler* masm);

  static Handle<Code> initialize_stub(Isolate* isolate,
                                      ExtraICState extra_state);
  static Handle<Code> initialize_stub_in_optimized_code(
      Isolate* isolate, ExtraICState extra_state, State initialization_state);

  MUST_USE_RESULT MaybeHandle<Object> Load(Handle<Object> object,
                                           Handle<Name> name);

  static void Clear(Isolate* isolate, Code* host, LoadICNexus* nexus);

 protected:
  inline void set_target(Code* code);

  Handle<Code> slow_stub() const {
    if (kind() == Code::LOAD_IC) {
      return isolate()->builtins()->LoadIC_Slow();
    } else {
      DCHECK_EQ(Code::KEYED_LOAD_IC, kind());
      return isolate()->builtins()->KeyedLoadIC_Slow();
    }
  }

  Handle<Code> megamorphic_stub() override;

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupIterator* lookup);

  Handle<Code> CompileHandler(LookupIterator* lookup, Handle<Object> unused,
                              CacheHolderFlag cache_holder) override;

 private:
  Handle<Code> SimpleFieldLoad(FieldIndex index);

  static void Clear(Isolate* isolate, Address address, Code* target,
                    Address constant_pool);

  friend class IC;
};


class KeyedLoadIC : public LoadIC {
 public:
  KeyedLoadIC(FrameDepth depth, Isolate* isolate,
              KeyedLoadICNexus* nexus = NULL)
      : LoadIC(depth, isolate, nexus) {
    DCHECK(nexus != NULL);
    DCHECK(target()->is_keyed_load_stub());
  }

  MUST_USE_RESULT MaybeHandle<Object> Load(Handle<Object> object,
                                           Handle<Object> key);

  // Code generator routines.
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateRuntimeGetProperty(MacroAssembler* masm);
  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GenerateMegamorphic(MacroAssembler* masm);

  // Bit mask to be tested against bit field for the cases when
  // generic stub should go into slow case.
  // Access check is necessary explicitly since generic stub does not perform
  // map checks.
  static const int kSlowCaseBitFieldMask =
      (1 << Map::kIsAccessCheckNeeded) | (1 << Map::kHasIndexedInterceptor);

  static Handle<Code> initialize_stub(Isolate* isolate,
                                      ExtraICState extra_state);
  static Handle<Code> initialize_stub_in_optimized_code(
      Isolate* isolate, State initialization_state, ExtraICState extra_state);
  static Handle<Code> ChooseMegamorphicStub(Isolate* isolate,
                                            ExtraICState extra_state);

  static void Clear(Isolate* isolate, Code* host, KeyedLoadICNexus* nexus);

 protected:
  // receiver is HeapObject because it could be a String or a JSObject
  Handle<Code> LoadElementStub(Handle<HeapObject> receiver);

 private:
  static void Clear(Isolate* isolate, Address address, Code* target,
                    Address constant_pool);

  friend class IC;
};


class StoreIC : public IC {
 public:
  static ExtraICState ComputeExtraICState(LanguageMode flag) {
    return StoreICState(flag).GetExtraICState();
  }

  StoreIC(FrameDepth depth, Isolate* isolate, FeedbackNexus* nexus = NULL)
      : IC(depth, isolate, nexus) {
    DCHECK(IsStoreStub());
  }

  LanguageMode language_mode() const {
    return StoreICState::GetLanguageMode(extra_ic_state());
  }

  // Code generators for stub routines. Only called once at startup.
  static void GenerateSlow(MacroAssembler* masm);
  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm);
  }
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateMegamorphic(MacroAssembler* masm);
  static void GenerateNormal(MacroAssembler* masm);
  static void GenerateRuntimeSetProperty(MacroAssembler* masm,
                                         LanguageMode language_mode);

  static Handle<Code> initialize_stub(Isolate* isolate,
                                      LanguageMode language_mode,
                                      State initialization_state);
  static Handle<Code> initialize_stub_in_optimized_code(
      Isolate* isolate, LanguageMode language_mode, State initialization_state);

  MUST_USE_RESULT MaybeHandle<Object> Store(
      Handle<Object> object, Handle<Name> name, Handle<Object> value,
      JSReceiver::StoreFromKeyed store_mode =
          JSReceiver::CERTAINLY_NOT_STORE_FROM_KEYED);

  bool LookupForWrite(LookupIterator* it, Handle<Object> value,
                      JSReceiver::StoreFromKeyed store_mode);

  static void Clear(Isolate* isolate, Code* host, StoreICNexus* nexus);

 protected:
  // Stub accessors.
  Handle<Code> megamorphic_stub() override;
  Handle<Code> slow_stub() const;

  virtual Handle<Code> pre_monomorphic_stub() const {
    return pre_monomorphic_stub(isolate(), language_mode());
  }

  static Handle<Code> pre_monomorphic_stub(Isolate* isolate,
                                           LanguageMode language_mode);

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupIterator* lookup, Handle<Object> value,
                    JSReceiver::StoreFromKeyed store_mode);
  Handle<Code> CompileHandler(LookupIterator* lookup, Handle<Object> value,
                              CacheHolderFlag cache_holder) override;

 private:
  inline void set_target(Code* code);

  static void Clear(Isolate* isolate, Address address, Code* target,
                    Address constant_pool);

  friend class IC;
};


enum KeyedStoreCheckMap { kDontCheckMap, kCheckMap };


enum KeyedStoreIncrementLength { kDontIncrementLength, kIncrementLength };


class KeyedStoreIC : public StoreIC {
 public:
  // ExtraICState bits (building on IC)
  // ExtraICState bits
  // When more language modes are added, these BitFields need to move too.
  STATIC_ASSERT(i::LANGUAGE_END == 3);
  class ExtraICStateKeyedAccessStoreMode
      : public BitField<KeyedAccessStoreMode, 3, 3> {};  // NOLINT

  class IcCheckTypeField : public BitField<IcCheckType, 6, 1> {};

  static ExtraICState ComputeExtraICState(LanguageMode flag,
                                          KeyedAccessStoreMode mode) {
    return StoreICState(flag).GetExtraICState() |
           ExtraICStateKeyedAccessStoreMode::encode(mode) |
           IcCheckTypeField::encode(ELEMENT);
  }

  KeyedAccessStoreMode GetKeyedAccessStoreMode() {
    return casted_nexus<KeyedStoreICNexus>()->GetKeyedAccessStoreMode();
  }

  KeyedStoreIC(FrameDepth depth, Isolate* isolate,
               KeyedStoreICNexus* nexus = NULL)
      : StoreIC(depth, isolate, nexus) {
    DCHECK(target()->is_keyed_store_stub());
  }

  MUST_USE_RESULT MaybeHandle<Object> Store(Handle<Object> object,
                                            Handle<Object> name,
                                            Handle<Object> value);

  // Code generators for stub routines.  Only called once at startup.
  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm);
  }
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateSlow(MacroAssembler* masm);
  static void GenerateMegamorphic(MacroAssembler* masm,
                                  LanguageMode language_mode);

  static Handle<Code> initialize_stub(Isolate* isolate,
                                      LanguageMode language_mode,
                                      State initialization_state);

  static Handle<Code> initialize_stub_in_optimized_code(
      Isolate* isolate, LanguageMode language_mode, State initialization_state);
  static Handle<Code> ChooseMegamorphicStub(Isolate* isolate,
                                            ExtraICState extra_state);

  static void Clear(Isolate* isolate, Code* host, KeyedStoreICNexus* nexus);

 protected:
  virtual Handle<Code> pre_monomorphic_stub() const {
    return pre_monomorphic_stub(isolate(), language_mode());
  }
  static Handle<Code> pre_monomorphic_stub(Isolate* isolate,
                                           LanguageMode language_mode) {
    if (is_strict(language_mode)) {
      return isolate->builtins()->KeyedStoreIC_PreMonomorphic_Strict();
    } else {
      return isolate->builtins()->KeyedStoreIC_PreMonomorphic();
    }
  }

  Handle<Code> StoreElementStub(Handle<Map> receiver_map,
                                KeyedAccessStoreMode store_mode);

 private:
  inline void set_target(Code* code);

  static void Clear(Isolate* isolate, Address address, Code* target,
                    Address constant_pool);

  Handle<Map> ComputeTransitionedMap(Handle<Map> map,
                                     KeyedAccessStoreMode store_mode);

  friend class IC;
};


// Type Recording BinaryOpIC, that records the types of the inputs and outputs.
class BinaryOpIC : public IC {
 public:
  explicit BinaryOpIC(Isolate* isolate) : IC(EXTRA_CALL_FRAME, isolate) {}

  MaybeHandle<Object> Transition(Handle<AllocationSite> allocation_site,
                                 Handle<Object> left,
                                 Handle<Object> right) WARN_UNUSED_RESULT;
};


class CompareIC : public IC {
 public:
  CompareIC(Isolate* isolate, Token::Value op)
      : IC(EXTRA_CALL_FRAME, isolate), op_(op) {}

  // Update the inline cache for the given operands.
  Code* UpdateCaches(Handle<Object> x, Handle<Object> y);

  // Helper function for computing the condition for a compare operation.
  static Condition ComputeCondition(Token::Value op);

  // Factory method for getting an uninitialized compare stub.
  static Handle<Code> GetUninitialized(Isolate* isolate, Token::Value op);

 private:
  static bool HasInlinedSmiCode(Address address);

  bool strict() const { return op_ == Token::EQ_STRICT; }
  Condition GetCondition() const { return ComputeCondition(op_); }

  static Code* GetRawUninitialized(Isolate* isolate, Token::Value op);

  static void Clear(Isolate* isolate, Address address, Code* target,
                    Address constant_pool);

  Token::Value op_;

  friend class IC;
};


class CompareNilIC : public IC {
 public:
  explicit CompareNilIC(Isolate* isolate) : IC(EXTRA_CALL_FRAME, isolate) {}

  Handle<Object> CompareNil(Handle<Object> object);

  static Handle<Code> GetUninitialized();

  static void Clear(Address address, Code* target, Address constant_pool);
};


class ToBooleanIC : public IC {
 public:
  explicit ToBooleanIC(Isolate* isolate) : IC(EXTRA_CALL_FRAME, isolate) {}

  Handle<Object> ToBoolean(Handle<Object> object);
};


// Helper for BinaryOpIC and CompareIC.
enum InlinedSmiCheck { ENABLE_INLINED_SMI_CHECK, DISABLE_INLINED_SMI_CHECK };
void PatchInlinedSmiCode(Isolate* isolate, Address address,
                         InlinedSmiCheck check);

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_H_
