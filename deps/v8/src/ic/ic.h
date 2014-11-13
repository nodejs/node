// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_H_
#define V8_IC_H_

#include "src/ic/ic-state.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {


// IC_UTIL_LIST defines all utility functions called from generated
// inline caching code. The argument for the macro, ICU, is the function name.
#define IC_UTIL_LIST(ICU)              \
  ICU(LoadIC_Miss)                     \
  ICU(KeyedLoadIC_Miss)                \
  ICU(CallIC_Miss)                     \
  ICU(CallIC_Customization_Miss)       \
  ICU(StoreIC_Miss)                    \
  ICU(StoreIC_Slow)                    \
  ICU(KeyedStoreIC_Miss)               \
  ICU(KeyedStoreIC_Slow)               \
  /* Utilities for IC stubs. */        \
  ICU(StoreCallbackProperty)           \
  ICU(LoadPropertyWithInterceptorOnly) \
  ICU(LoadPropertyWithInterceptor)     \
  ICU(LoadElementWithInterceptor)      \
  ICU(StorePropertyWithInterceptor)    \
  ICU(CompareIC_Miss)                  \
  ICU(BinaryOpIC_Miss)                 \
  ICU(CompareNilIC_Miss)               \
  ICU(Unreachable)                     \
  ICU(ToBooleanIC_Miss)
//
// IC is the base class for LoadIC, StoreIC, KeyedLoadIC, and KeyedStoreIC.
//
class IC {
 public:
  // The ids for utility called from the generated code.
  enum UtilityId {
#define CONST_NAME(name) k##name,
    IC_UTIL_LIST(CONST_NAME)
#undef CONST_NAME
    kUtilityCount
  };

  // Looks up the address of the named utility.
  static Address AddressFromUtilityId(UtilityId id);

  // Alias the inline cache state type to make the IC code more readable.
  typedef InlineCacheState State;

  // The IC code is either invoked with no extra frames on the stack
  // or with a single extra frame for supporting calls.
  enum FrameDepth { NO_EXTRA_FRAME = 0, EXTRA_CALL_FRAME = 1 };

  // Construct the IC structure with the given number of extra
  // JavaScript frames on the stack.
  IC(FrameDepth depth, Isolate* isolate, FeedbackNexus* nexus = NULL,
     bool for_queries_only = false);
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

  // If the stub contains weak maps then this function adds the stub to
  // the dependent code array of each weak map.
  static void RegisterWeakMapDependency(Handle<Code> stub);

  // This function is called when a weak map in the stub is dying,
  // invalidates the stub by setting maps in it to undefined.
  static void InvalidateMaps(Code* stub);

  // Clear the inline cache to initial state.
  static void Clear(Isolate* isolate, Address address,
                    ConstantPoolArray* constant_pool);

  // Clear the vector-based inline cache to initial state.
  template <class Nexus>
  static void Clear(Isolate* isolate, Code::Kind kind, Code* host,
                    Nexus* nexus);

#ifdef DEBUG
  bool IsLoadStub() const {
    return target()->is_load_stub() || target()->is_keyed_load_stub();
  }

  bool IsStoreStub() const {
    return target()->is_store_stub() || target()->is_keyed_store_stub();
  }

  bool IsCallStub() const { return target()->is_call_stub(); }
#endif

  template <class TypeClass>
  static JSFunction* GetRootConstructor(TypeClass* type,
                                        Context* native_context);
  static inline Handle<Map> GetHandlerCacheHolder(HeapType* type,
                                                  bool receiver_is_holder,
                                                  Isolate* isolate,
                                                  CacheHolderFlag* flag);
  static inline Handle<Map> GetICCacheHolder(HeapType* type, Isolate* isolate,
                                             CacheHolderFlag* flag);

  static bool IsCleared(Code* code) {
    InlineCacheState state = code->ic_state();
    return state == UNINITIALIZED || state == PREMONOMORPHIC;
  }

  static bool IsCleared(FeedbackNexus* nexus) {
    InlineCacheState state = nexus->StateFromFeedback();
    return state == UNINITIALIZED || state == PREMONOMORPHIC;
  }

  // Utility functions to convert maps to types and back. There are two special
  // cases:
  // - The heap_number_map is used as a marker which includes heap numbers as
  //   well as smis.
  // - The oddball map is only used for booleans.
  static Handle<Map> TypeToMap(HeapType* type, Isolate* isolate);
  template <class T>
  static typename T::TypeHandle MapToType(Handle<Map> map,
                                          typename T::Region* region);

  static Handle<HeapType> CurrentTypeOf(Handle<Object> object,
                                        Isolate* isolate);

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
  // Get the original (non-breakpointed) code object of the caller.
  Code* GetOriginalCode() const;

  // Set the call-site target.
  inline void set_target(Code* code);
  bool is_target_set() { return target_set_; }

  bool UseVector() const {
    bool use = (FLAG_vector_ics &&
                (kind() == Code::LOAD_IC || kind() == Code::KEYED_LOAD_IC)) ||
               kind() == Code::CALL_IC;
    // If we are supposed to use the nexus, verify the nexus is non-null.
    DCHECK(!use || nexus_ != NULL);
    return use;
  }

  char TransitionMarkFromState(IC::State state);
  void TraceIC(const char* type, Handle<Object> name);
  void TraceIC(const char* type, Handle<Object> name, State old_state,
               State new_state);

  MaybeHandle<Object> TypeError(const char* type, Handle<Object> object,
                                Handle<Object> key);
  MaybeHandle<Object> ReferenceError(const char* type, Handle<Name> name);

  // Access the target code for the given IC address.
  static inline Code* GetTargetAtAddress(Address address,
                                         ConstantPoolArray* constant_pool);
  static inline void SetTargetAtAddress(Address address, Code* target,
                                        ConstantPoolArray* constant_pool);
  static void OnTypeFeedbackChanged(Isolate* isolate, Address address,
                                    State old_state, State new_state,
                                    bool target_remains_ic_stub);
  // As a vector-based IC, type feedback must be updated differently.
  static void OnTypeFeedbackChanged(Isolate* isolate, Code* host,
                                    TypeFeedbackVector* vector, State old_state,
                                    State new_state);
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
  void UpdateMegamorphicCache(HeapType* type, Name* name, Code* code);

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

  Handle<HeapType> receiver_type() { return receiver_type_; }
  void update_receiver_type(Handle<Object> receiver) {
    receiver_type_ = CurrentTypeOf(receiver, isolate_);
  }

  void TargetMaps(MapHandleList* list) {
    FindTargetMaps();
    for (int i = 0; i < target_maps_.length(); i++) {
      list->Add(target_maps_.at(i));
    }
  }

  void TargetTypes(TypeHandleList* list) {
    FindTargetMaps();
    for (int i = 0; i < target_maps_.length(); i++) {
      list->Add(MapToType<HeapType>(target_maps_.at(i), isolate_));
    }
  }

  Map* FirstTargetMap() {
    FindTargetMaps();
    return target_maps_.length() > 0 ? *target_maps_.at(0) : NULL;
  }

  inline void UpdateTarget();

  Handle<TypeFeedbackVector> vector() const { return nexus()->vector_handle(); }
  FeedbackVectorICSlot slot() const { return nexus()->slot(); }
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
  inline ConstantPoolArray* constant_pool() const;
  inline ConstantPoolArray* raw_constant_pool() const;

  void FindTargetMaps() {
    if (target_maps_set_) return;
    target_maps_set_ = true;
    if (state_ == MONOMORPHIC) {
      Map* map = target_->FindFirstMap();
      if (map != NULL) target_maps_.Add(handle(map));
    } else if (state_ != UNINITIALIZED && state_ != PREMONOMORPHIC) {
      target_->FindAllMaps(&target_maps_);
    }
  }

  // Frame pointer for the frame that uses (calls) the IC.
  Address fp_;

  // All access to the program counter of an IC structure is indirect
  // to make the code GC safe. This feature is crucial since
  // GetProperty and SetProperty are called and they in turn might
  // invoke the garbage collector.
  Address* pc_address_;

  Isolate* isolate_;

  // The constant pool of the code which originally called the IC (which might
  // be for the breakpointed copy of the original code).
  Handle<ConstantPoolArray> raw_constant_pool_;

  // The original code target that missed.
  Handle<Code> target_;
  bool target_set_;
  State old_state_;  // For saving if we marked as prototype failure.
  State state_;
  Code::Kind kind_;
  Handle<HeapType> receiver_type_;
  MaybeHandle<Code> maybe_handler_;

  ExtraICState extra_ic_state_;
  MapHandleList target_maps_;
  bool target_maps_set_;

  FeedbackNexus* nexus_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IC);
};


// An IC_Utility encapsulates IC::UtilityId. It exists mainly because you
// cannot make forward declarations to an enum.
class IC_Utility {
 public:
  explicit IC_Utility(IC::UtilityId id)
      : address_(IC::AddressFromUtilityId(id)), id_(id) {}

  Address address() const { return address_; }

  IC::UtilityId id() const { return id_; }

 private:
  Address address_;
  IC::UtilityId id_;
};


class CallIC : public IC {
 public:
  CallIC(Isolate* isolate, CallICNexus* nexus)
      : IC(EXTRA_CALL_FRAME, isolate, nexus) {
    DCHECK(nexus != NULL);
  }

  void PatchMegamorphic(Handle<Object> function);

  void HandleMiss(Handle<Object> receiver, Handle<Object> function);

  // Returns true if a custom handler was installed.
  bool DoCustomHandler(Handle<Object> receiver, Handle<Object> function,
                       const CallICState& callic_state);

  // Code generator routines.
  static Handle<Code> initialize_stub(Isolate* isolate, int argc,
                                      CallICState::CallType call_type);

  static void Clear(Isolate* isolate, Code* host, CallICNexus* nexus);
};


class LoadIC : public IC {
 public:
  static ExtraICState ComputeExtraICState(ContextualMode contextual_mode) {
    return LoadICState(contextual_mode).GetExtraICState();
  }

  ContextualMode contextual_mode() const {
    return LoadICState::GetContextualMode(extra_ic_state());
  }

  explicit LoadIC(FrameDepth depth, Isolate* isolate) : IC(depth, isolate) {
    DCHECK(IsLoadStub());
  }

  // Returns if this IC is for contextual (no explicit receiver)
  // access to properties.
  bool IsUndeclaredGlobal(Handle<Object> receiver) {
    if (receiver->IsGlobalObject()) {
      return contextual_mode() == CONTEXTUAL;
    } else {
      DCHECK(contextual_mode() != CONTEXTUAL);
      return false;
    }
  }

  // Code generator routines.
  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm);
  }
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateNormal(MacroAssembler* masm);
  static void GenerateRuntimeGetProperty(MacroAssembler* masm);

  static Handle<Code> initialize_stub(Isolate* isolate,
                                      ExtraICState extra_state);
  static Handle<Code> initialize_stub_in_optimized_code(
      Isolate* isolate, ExtraICState extra_state);

  MUST_USE_RESULT MaybeHandle<Object> Load(Handle<Object> object,
                                           Handle<Name> name);

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

  virtual Handle<Code> megamorphic_stub() OVERRIDE;

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupIterator* lookup);

  virtual Handle<Code> CompileHandler(LookupIterator* lookup,
                                      Handle<Object> unused,
                                      CacheHolderFlag cache_holder) OVERRIDE;

 private:
  virtual Handle<Code> pre_monomorphic_stub() const;
  static Handle<Code> pre_monomorphic_stub(Isolate* isolate,
                                           ExtraICState extra_state);

  Handle<Code> SimpleFieldLoad(FieldIndex index);

  static void Clear(Isolate* isolate, Address address, Code* target,
                    ConstantPoolArray* constant_pool);

  friend class IC;
};


class KeyedLoadIC : public LoadIC {
 public:
  explicit KeyedLoadIC(FrameDepth depth, Isolate* isolate)
      : LoadIC(depth, isolate) {
    DCHECK(target()->is_keyed_load_stub());
  }

  MUST_USE_RESULT MaybeHandle<Object> Load(Handle<Object> object,
                                           Handle<Object> key);

  // Code generator routines.
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateRuntimeGetProperty(MacroAssembler* masm);
  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm);
  }
  static void GenerateGeneric(MacroAssembler* masm);

  // Bit mask to be tested against bit field for the cases when
  // generic stub should go into slow case.
  // Access check is necessary explicitly since generic stub does not perform
  // map checks.
  static const int kSlowCaseBitFieldMask =
      (1 << Map::kIsAccessCheckNeeded) | (1 << Map::kHasIndexedInterceptor);

  static Handle<Code> initialize_stub(Isolate* isolate);
  static Handle<Code> initialize_stub_in_optimized_code(Isolate* isolate);
  static Handle<Code> generic_stub(Isolate* isolate);
  static Handle<Code> pre_monomorphic_stub(Isolate* isolate);

 protected:
  // receiver is HeapObject because it could be a String or a JSObject
  Handle<Code> LoadElementStub(Handle<HeapObject> receiver);
  virtual Handle<Code> pre_monomorphic_stub() const {
    return pre_monomorphic_stub(isolate());
  }

 private:
  Handle<Code> generic_stub() const { return generic_stub(isolate()); }

  static void Clear(Isolate* isolate, Address address, Code* target,
                    ConstantPoolArray* constant_pool);

  friend class IC;
};


class StoreIC : public IC {
 public:
  class StrictModeState : public BitField<StrictMode, 1, 1> {};
  static ExtraICState ComputeExtraICState(StrictMode flag) {
    return StrictModeState::encode(flag);
  }
  static StrictMode GetStrictMode(ExtraICState state) {
    return StrictModeState::decode(state);
  }

  // For convenience, a statically declared encoding of strict mode extra
  // IC state.
  static const ExtraICState kStrictModeState = 1 << StrictModeState::kShift;

  StoreIC(FrameDepth depth, Isolate* isolate) : IC(depth, isolate) {
    DCHECK(IsStoreStub());
  }

  StrictMode strict_mode() const {
    return StrictModeState::decode(extra_ic_state());
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
                                         StrictMode strict_mode);

  static Handle<Code> initialize_stub(Isolate* isolate, StrictMode strict_mode);

  MUST_USE_RESULT MaybeHandle<Object> Store(
      Handle<Object> object, Handle<Name> name, Handle<Object> value,
      JSReceiver::StoreFromKeyed store_mode =
          JSReceiver::CERTAINLY_NOT_STORE_FROM_KEYED);

  bool LookupForWrite(LookupIterator* it, Handle<Object> value,
                      JSReceiver::StoreFromKeyed store_mode);

 protected:
  virtual Handle<Code> megamorphic_stub() OVERRIDE;

  // Stub accessors.
  Handle<Code> generic_stub() const;

  Handle<Code> slow_stub() const;

  virtual Handle<Code> pre_monomorphic_stub() const {
    return pre_monomorphic_stub(isolate(), strict_mode());
  }

  static Handle<Code> pre_monomorphic_stub(Isolate* isolate,
                                           StrictMode strict_mode);

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupIterator* lookup, Handle<Object> value,
                    JSReceiver::StoreFromKeyed store_mode);
  virtual Handle<Code> CompileHandler(LookupIterator* lookup,
                                      Handle<Object> value,
                                      CacheHolderFlag cache_holder) OVERRIDE;

 private:
  inline void set_target(Code* code);

  static void Clear(Isolate* isolate, Address address, Code* target,
                    ConstantPoolArray* constant_pool);

  friend class IC;
};


enum KeyedStoreCheckMap { kDontCheckMap, kCheckMap };


enum KeyedStoreIncrementLength { kDontIncrementLength, kIncrementLength };


class KeyedStoreIC : public StoreIC {
 public:
  // ExtraICState bits (building on IC)
  // ExtraICState bits
  class ExtraICStateKeyedAccessStoreMode
      : public BitField<KeyedAccessStoreMode, 2, 4> {};  // NOLINT

  class IcCheckTypeField : public BitField<IcCheckType, 6, 1> {};

  static ExtraICState ComputeExtraICState(StrictMode flag,
                                          KeyedAccessStoreMode mode) {
    return StrictModeState::encode(flag) |
           ExtraICStateKeyedAccessStoreMode::encode(mode) |
           IcCheckTypeField::encode(ELEMENT);
  }

  static KeyedAccessStoreMode GetKeyedAccessStoreMode(
      ExtraICState extra_state) {
    return ExtraICStateKeyedAccessStoreMode::decode(extra_state);
  }

  static IcCheckType GetKeyType(ExtraICState extra_state) {
    return IcCheckTypeField::decode(extra_state);
  }

  KeyedStoreIC(FrameDepth depth, Isolate* isolate) : StoreIC(depth, isolate) {
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
  static void GenerateMegamorphic(MacroAssembler* masm, StrictMode strict_mode);
  static void GenerateGeneric(MacroAssembler* masm, StrictMode strict_mode);
  static void GenerateSloppyArguments(MacroAssembler* masm);

 protected:
  virtual Handle<Code> pre_monomorphic_stub() const {
    return pre_monomorphic_stub(isolate(), strict_mode());
  }
  static Handle<Code> pre_monomorphic_stub(Isolate* isolate,
                                           StrictMode strict_mode) {
    if (strict_mode == STRICT) {
      return isolate->builtins()->KeyedStoreIC_PreMonomorphic_Strict();
    } else {
      return isolate->builtins()->KeyedStoreIC_PreMonomorphic();
    }
  }

  Handle<Code> StoreElementStub(Handle<JSObject> receiver,
                                KeyedAccessStoreMode store_mode);

 private:
  inline void set_target(Code* code);

  // Stub accessors.
  Handle<Code> sloppy_arguments_stub() {
    return isolate()->builtins()->KeyedStoreIC_SloppyArguments();
  }

  static void Clear(Isolate* isolate, Address address, Code* target,
                    ConstantPoolArray* constant_pool);

  KeyedAccessStoreMode GetStoreMode(Handle<JSObject> receiver,
                                    Handle<Object> key, Handle<Object> value);

  Handle<Map> ComputeTransitionedMap(Handle<Map> map,
                                     KeyedAccessStoreMode store_mode);

  friend class IC;
};


// Type Recording BinaryOpIC, that records the types of the inputs and outputs.
class BinaryOpIC : public IC {
 public:
  explicit BinaryOpIC(Isolate* isolate) : IC(EXTRA_CALL_FRAME, isolate) {}

  static Builtins::JavaScript TokenToJSBuiltin(Token::Value op);

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
                    ConstantPoolArray* constant_pool);

  Token::Value op_;

  friend class IC;
};


class CompareNilIC : public IC {
 public:
  explicit CompareNilIC(Isolate* isolate) : IC(EXTRA_CALL_FRAME, isolate) {}

  Handle<Object> CompareNil(Handle<Object> object);

  static Handle<Code> GetUninitialized();

  static void Clear(Address address, Code* target,
                    ConstantPoolArray* constant_pool);

  static Handle<Object> DoCompareNilSlow(Isolate* isolate, NilValue nil,
                                         Handle<Object> object);
};


class ToBooleanIC : public IC {
 public:
  explicit ToBooleanIC(Isolate* isolate) : IC(EXTRA_CALL_FRAME, isolate) {}

  Handle<Object> ToBoolean(Handle<Object> object);
};


// Helper for BinaryOpIC and CompareIC.
enum InlinedSmiCheck { ENABLE_INLINED_SMI_CHECK, DISABLE_INLINED_SMI_CHECK };
void PatchInlinedSmiCode(Address address, InlinedSmiCheck check);

DECLARE_RUNTIME_FUNCTION(KeyedLoadIC_MissFromStubFailure);
DECLARE_RUNTIME_FUNCTION(KeyedStoreIC_MissFromStubFailure);
DECLARE_RUNTIME_FUNCTION(UnaryOpIC_Miss);
DECLARE_RUNTIME_FUNCTION(StoreIC_MissFromStubFailure);
DECLARE_RUNTIME_FUNCTION(ElementsTransitionAndStoreIC_Miss);
DECLARE_RUNTIME_FUNCTION(BinaryOpIC_Miss);
DECLARE_RUNTIME_FUNCTION(BinaryOpIC_MissWithAllocationSite);
DECLARE_RUNTIME_FUNCTION(CompareNilIC_Miss);
DECLARE_RUNTIME_FUNCTION(ToBooleanIC_Miss);
DECLARE_RUNTIME_FUNCTION(LoadIC_MissFromStubFailure);

// Support functions for callbacks handlers.
DECLARE_RUNTIME_FUNCTION(StoreCallbackProperty);

// Support functions for interceptor handlers.
DECLARE_RUNTIME_FUNCTION(LoadPropertyWithInterceptorOnly);
DECLARE_RUNTIME_FUNCTION(LoadPropertyWithInterceptor);
DECLARE_RUNTIME_FUNCTION(LoadElementWithInterceptor);
DECLARE_RUNTIME_FUNCTION(StorePropertyWithInterceptor);
}
}  // namespace v8::internal

#endif  // V8_IC_H_
