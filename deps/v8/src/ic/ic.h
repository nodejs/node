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

  bool RecomputeHandlerForName(Handle<Object> name);
  void MarkRecomputeHandler(Handle<Object> name) {
    DCHECK(RecomputeHandlerForName(name));
    old_state_ = state_;
    state_ = RECOMPUTE_HANDLER;
  }

  // Clear the inline cache to initial state.
  static void Clear(Isolate* isolate, Address address, Address constant_pool);

#ifdef DEBUG
  bool IsLoadStub() const {
    return kind_ == Code::LOAD_IC || kind_ == Code::LOAD_GLOBAL_IC ||
           kind_ == Code::KEYED_LOAD_IC;
  }
  bool IsStoreStub() const {
    return kind_ == Code::STORE_IC || kind_ == Code::KEYED_STORE_IC;
  }
  bool IsCallStub() const { return kind_ == Code::CALL_IC; }
#endif

  static inline Handle<Map> GetHandlerCacheHolder(Handle<Map> receiver_map,
                                                  bool receiver_is_holder,
                                                  Isolate* isolate,
                                                  CacheHolderFlag* flag);
  static inline Handle<Map> GetICCacheHolder(Handle<Map> receiver_map,
                                             Isolate* isolate,
                                             CacheHolderFlag* flag);

  static bool IsCleared(FeedbackNexus* nexus) {
    InlineCacheState state = nexus->StateFromFeedback();
    return !FLAG_use_ic || state == UNINITIALIZED || state == PREMONOMORPHIC;
  }

  static bool ICUseVector(Code::Kind kind) {
    return kind == Code::LOAD_IC || kind == Code::LOAD_GLOBAL_IC ||
           kind == Code::KEYED_LOAD_IC || kind == Code::CALL_IC ||
           kind == Code::STORE_IC || kind == Code::KEYED_STORE_IC;
  }

  // The ICs that don't pass slot and vector through the stack have to
  // save/restore them in the dispatcher.
  static bool ShouldPushPopSlotAndVector(Code::Kind kind);

  static InlineCacheState StateFromCode(Code* code);

  static inline bool IsHandler(Object* object);

 protected:
  Address fp() const { return fp_; }
  Address pc() const { return *pc_address_; }
  Isolate* isolate() const { return isolate_; }

  // Get the shared function info of the caller.
  SharedFunctionInfo* GetSharedFunctionInfo() const;
  // Get the code object of the caller.
  Code* GetCode() const;

  inline bool AddressIsDeoptimizedCode() const;
  inline static bool AddressIsDeoptimizedCode(Isolate* isolate,
                                              Address address);

  // Set the call-site target.
  inline void set_target(Code* code);
  bool is_vector_set() { return vector_set_; }

  bool UseVector() const {
    bool use = ICUseVector(kind());
    // If we are supposed to use the nexus, verify the nexus is non-null.
    DCHECK(!use || nexus_ != nullptr);
    return use;
  }

  // Configure for most states.
  void ConfigureVectorState(IC::State new_state, Handle<Object> key);
  // Configure the vector for MONOMORPHIC.
  void ConfigureVectorState(Handle<Name> name, Handle<Map> map,
                            Handle<Object> handler);
  // Configure the vector for POLYMORPHIC.
  void ConfigureVectorState(Handle<Name> name, MapHandleList* maps,
                            List<Handle<Object>>* handlers);
  // Configure the vector for POLYMORPHIC with transitions (only for element
  // keyed stores).
  void ConfigureVectorState(MapHandleList* maps,
                            MapHandleList* transitioned_maps,
                            List<Handle<Object>>* handlers);

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
  // As a vector-based IC, type feedback must be updated differently.
  static void OnTypeFeedbackChanged(Isolate* isolate, Code* host);
  static void PostPatching(Address address, Code* target, Code* old_target);

  void TraceHandlerCacheHitStats(LookupIterator* lookup);

  // Compute the handler either by compiling or by retrieving a cached version.
  Handle<Object> ComputeHandler(LookupIterator* lookup,
                                Handle<Object> value = Handle<Code>::null());
  virtual Handle<Object> GetMapIndependentHandler(LookupIterator* lookup) {
    UNREACHABLE();
    return Handle<Code>::null();
  }
  virtual Handle<Object> CompileHandler(LookupIterator* lookup,
                                        Handle<Object> value,
                                        CacheHolderFlag cache_holder) {
    UNREACHABLE();
    return Handle<Object>::null();
  }

  void UpdateMonomorphicIC(Handle<Object> handler, Handle<Name> name);
  bool UpdatePolymorphicIC(Handle<Name> name, Handle<Object> code);
  void UpdateMegamorphicCache(Map* map, Name* name, Object* code);

  StubCache* stub_cache();

  void CopyICToMegamorphicCache(Handle<Name> name);
  bool IsTransitionOfMonomorphicTarget(Map* source_map, Map* target_map);
  void PatchCache(Handle<Name> name, Handle<Object> code);
  Code::Kind kind() const { return kind_; }
  bool is_keyed() const {
    return kind_ == Code::KEYED_LOAD_IC || kind_ == Code::KEYED_STORE_IC;
  }
  Code::Kind handler_kind() const {
    if (kind_ == Code::KEYED_LOAD_IC) return Code::LOAD_IC;
    DCHECK(kind_ == Code::LOAD_IC || kind_ == Code::STORE_IC ||
           kind_ == Code::KEYED_STORE_IC);
    return kind_;
  }
  bool ShouldRecomputeHandler(Handle<String> name);

  ExtraICState extra_ic_state() const { return extra_ic_state_; }

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

  Handle<FeedbackVector> vector() const { return nexus()->vector_handle(); }
  FeedbackVectorSlot slot() const { return nexus()->slot(); }
  State saved_state() const {
    return state() == RECOMPUTE_HANDLER ? old_state_ : state();
  }

  template <class NexusClass>
  NexusClass* casted_nexus() {
    return static_cast<NexusClass*>(nexus_);
  }
  FeedbackNexus* nexus() const { return nexus_; }

  inline Code* get_host();
  inline Code* target() const;

 private:
  inline Address constant_pool() const;
  inline Address raw_constant_pool() const;

  void FindTargetMaps() {
    if (target_maps_set_) return;
    target_maps_set_ = true;
    DCHECK(UseVector());
    nexus()->ExtractMaps(&target_maps_);
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

  bool vector_set_;
  State old_state_;  // For saving if we marked as prototype failure.
  State state_;
  Code::Kind kind_;
  Handle<Map> receiver_map_;
  MaybeHandle<Object> maybe_handler_;

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

  static void Clear(Isolate* isolate, Code* host, CallICNexus* nexus);
};


class LoadIC : public IC {
 public:
  LoadIC(FrameDepth depth, Isolate* isolate, FeedbackNexus* nexus = NULL)
      : IC(depth, isolate, nexus) {
    DCHECK(nexus != NULL);
    DCHECK(IsLoadStub());
  }

  bool ShouldThrowReferenceError() const {
    return kind() == Code::LOAD_GLOBAL_IC &&
           LoadGlobalICState::GetTypeofMode(extra_ic_state()) ==
               NOT_INSIDE_TYPEOF;
  }

  MUST_USE_RESULT MaybeHandle<Object> Load(Handle<Object> object,
                                           Handle<Name> name);

  static void Clear(Isolate* isolate, Code* host, LoadICNexus* nexus);

 protected:
  virtual Handle<Code> slow_stub() const {
    return isolate()->builtins()->LoadIC_Slow();
  }

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupIterator* lookup);

  Handle<Object> GetMapIndependentHandler(LookupIterator* lookup) override;

  Handle<Object> CompileHandler(LookupIterator* lookup, Handle<Object> unused,
                                CacheHolderFlag cache_holder) override;

 private:
  // Creates a data handler that represents a load of a field by given index.
  static Handle<Object> SimpleFieldLoad(Isolate* isolate, FieldIndex index);

  // Creates a data handler that represents a prototype chain check followed
  // by given Smi-handler that encoded a load from the holder.
  // Can be used only if GetPrototypeCheckCount() returns non negative value.
  Handle<Object> LoadFromPrototype(Handle<Map> receiver_map,
                                   Handle<JSObject> holder, Handle<Name> name,
                                   Handle<Object> smi_handler);

  // Creates a data handler that represents a load of a non-existent property.
  Handle<Object> LoadNonExistent(Handle<Map> receiver_map, Handle<Name> name);

  friend class IC;
  friend class NamedLoadHandlerCompiler;
};

class LoadGlobalIC : public LoadIC {
 public:
  LoadGlobalIC(FrameDepth depth, Isolate* isolate, FeedbackNexus* nexus = NULL)
      : LoadIC(depth, isolate, nexus) {}

  MUST_USE_RESULT MaybeHandle<Object> Load(Handle<Name> name);

  static void Clear(Isolate* isolate, Code* host, LoadGlobalICNexus* nexus);

 protected:
  Handle<Code> slow_stub() const override {
    return isolate()->builtins()->LoadGlobalIC_Slow();
  }
};

class KeyedLoadIC : public LoadIC {
 public:
  KeyedLoadIC(FrameDepth depth, Isolate* isolate,
              KeyedLoadICNexus* nexus = NULL)
      : LoadIC(depth, isolate, nexus) {
    DCHECK(nexus != NULL);
  }

  MUST_USE_RESULT MaybeHandle<Object> Load(Handle<Object> object,
                                           Handle<Object> key);

  static void Clear(Isolate* isolate, Code* host, KeyedLoadICNexus* nexus);

 protected:
  // receiver is HeapObject because it could be a String or a JSObject
  void UpdateLoadElement(Handle<HeapObject> receiver);

 private:
  friend class IC;
};


class StoreIC : public IC {
 public:
  StoreIC(FrameDepth depth, Isolate* isolate, FeedbackNexus* nexus = NULL)
      : IC(depth, isolate, nexus) {
    DCHECK(IsStoreStub());
  }

  LanguageMode language_mode() const {
    return StoreICState::GetLanguageMode(extra_ic_state());
  }

  MUST_USE_RESULT MaybeHandle<Object> Store(
      Handle<Object> object, Handle<Name> name, Handle<Object> value,
      JSReceiver::StoreFromKeyed store_mode =
          JSReceiver::CERTAINLY_NOT_STORE_FROM_KEYED);

  bool LookupForWrite(LookupIterator* it, Handle<Object> value,
                      JSReceiver::StoreFromKeyed store_mode);

  static void Clear(Isolate* isolate, Code* host, StoreICNexus* nexus);

 protected:
  // Stub accessors.
  Handle<Code> slow_stub() const {
    switch (language_mode()) {
      case SLOPPY:
        return isolate()->builtins()->StoreIC_SlowSloppy();
      case STRICT:
        return isolate()->builtins()->StoreIC_SlowStrict();
      default:
        UNREACHABLE();
        return Handle<Code>();
    }
  }

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupIterator* lookup, Handle<Object> value,
                    JSReceiver::StoreFromKeyed store_mode);
  Handle<Object> GetMapIndependentHandler(LookupIterator* lookup) override;
  Handle<Object> CompileHandler(LookupIterator* lookup, Handle<Object> value,
                                CacheHolderFlag cache_holder) override;

 private:
  Handle<Object> StoreTransition(Handle<Map> receiver_map,
                                 Handle<JSObject> holder,
                                 Handle<Map> transition, Handle<Name> name);

  friend class IC;
};


enum KeyedStoreCheckMap { kDontCheckMap, kCheckMap };


enum KeyedStoreIncrementLength { kDontIncrementLength, kIncrementLength };


class KeyedStoreIC : public StoreIC {
 public:
  KeyedAccessStoreMode GetKeyedAccessStoreMode() {
    return casted_nexus<KeyedStoreICNexus>()->GetKeyedAccessStoreMode();
  }

  KeyedStoreIC(FrameDepth depth, Isolate* isolate,
               KeyedStoreICNexus* nexus = NULL)
      : StoreIC(depth, isolate, nexus) {}

  MUST_USE_RESULT MaybeHandle<Object> Store(Handle<Object> object,
                                            Handle<Object> name,
                                            Handle<Object> value);

  // Code generators for stub routines.  Only called once at startup.
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateSlow(MacroAssembler* masm);
  static void GenerateMegamorphic(MacroAssembler* masm,
                                  LanguageMode language_mode);

  static void Clear(Isolate* isolate, Code* host, KeyedStoreICNexus* nexus);

 protected:
  void UpdateStoreElement(Handle<Map> receiver_map,
                          KeyedAccessStoreMode store_mode);

 private:
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
