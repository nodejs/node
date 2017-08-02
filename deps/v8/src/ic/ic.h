// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_H_
#define V8_IC_H_

#include "src/factory.h"
#include "src/feedback-vector.h"
#include "src/ic/ic-state.h"
#include "src/macro-assembler.h"
#include "src/messages.h"
#include "src/objects/map.h"

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

  // A polymorphic IC can handle at most 4 distinct maps before transitioning
  // to megamorphic state.
  static constexpr int kMaxPolymorphicMapCount = 4;

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

  bool IsAnyLoad() const {
    return IsLoadIC() || IsLoadGlobalIC() || IsKeyedLoadIC();
  }
  bool IsAnyStore() const {
    return IsStoreIC() || IsStoreOwnIC() || IsStoreGlobalIC() ||
           IsKeyedStoreIC();
  }

  // The ICs that don't pass slot and vector through the stack have to
  // save/restore them in the dispatcher.
  static bool ShouldPushPopSlotAndVector(Code::Kind kind);

  static InlineCacheState StateFromCode(Code* code);

  static inline bool IsHandler(Object* object);

  // Nofity the IC system that a feedback has changed.
  static void OnFeedbackChanged(Isolate* isolate, JSFunction* host_function);

 protected:
  Address fp() const { return fp_; }
  Address pc() const { return *pc_address_; }

  void set_slow_stub_reason(const char* reason) { slow_stub_reason_ = reason; }

  Address GetAbstractPC(int* line, int* column) const;
  Isolate* isolate() const { return isolate_; }

  // Get the caller function object.
  JSFunction* GetHostFunction() const;

  inline bool AddressIsDeoptimizedCode() const;
  inline static bool AddressIsDeoptimizedCode(Isolate* isolate,
                                              Address address);

  // Set the call-site target.
  inline void set_target(Code* code);
  bool is_vector_set() { return vector_set_; }

  // Configure for most states.
  void ConfigureVectorState(IC::State new_state, Handle<Object> key);
  // Configure the vector for MONOMORPHIC.
  void ConfigureVectorState(Handle<Name> name, Handle<Map> map,
                            Handle<Object> handler);
  // Configure the vector for POLYMORPHIC.
  void ConfigureVectorState(Handle<Name> name, MapHandles const& maps,
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
  static void PostPatching(Address address, Code* target, Code* old_target);

  void TraceHandlerCacheHitStats(LookupIterator* lookup);

  // Compute the handler either by compiling or by retrieving a cached version.
  Handle<Object> ComputeHandler(LookupIterator* lookup);
  virtual Handle<Object> GetMapIndependentHandler(LookupIterator* lookup) {
    UNREACHABLE();
    return Handle<Code>::null();
  }
  virtual Handle<Code> CompileHandler(LookupIterator* lookup) {
    UNREACHABLE();
    return Handle<Code>::null();
  }

  void UpdateMonomorphicIC(Handle<Object> handler, Handle<Name> name);
  bool UpdatePolymorphicIC(Handle<Name> name, Handle<Object> code);
  void UpdateMegamorphicCache(Map* map, Name* name, Object* code);

  StubCache* stub_cache();

  void CopyICToMegamorphicCache(Handle<Name> name);
  bool IsTransitionOfMonomorphicTarget(Map* source_map, Map* target_map);
  void PatchCache(Handle<Name> name, Handle<Object> code);
  FeedbackSlotKind kind() const { return kind_; }
  bool IsLoadIC() const { return IsLoadICKind(kind_); }
  bool IsLoadGlobalIC() const { return IsLoadGlobalICKind(kind_); }
  bool IsKeyedLoadIC() const { return IsKeyedLoadICKind(kind_); }
  bool IsStoreGlobalIC() const { return IsStoreGlobalICKind(kind_); }
  bool IsStoreIC() const { return IsStoreICKind(kind_); }
  bool IsStoreOwnIC() const { return IsStoreOwnICKind(kind_); }
  bool IsKeyedStoreIC() const { return IsKeyedStoreICKind(kind_); }
  bool is_keyed() const { return IsKeyedLoadIC() || IsKeyedStoreIC(); }
  Code::Kind handler_kind() const {
    if (IsAnyLoad()) return Code::LOAD_IC;
    DCHECK(IsAnyStore());
    return Code::STORE_IC;
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

  void TargetMaps(MapHandles* list) {
    FindTargetMaps();
    for (Handle<Map> map : target_maps_) {
      list->push_back(map);
    }
  }

  Map* FirstTargetMap() {
    FindTargetMaps();
    return !target_maps_.empty() ? *target_maps_[0] : NULL;
  }

  Handle<FeedbackVector> vector() const { return nexus()->vector_handle(); }
  FeedbackSlot slot() const { return nexus()->slot(); }
  State saved_state() const {
    return state() == RECOMPUTE_HANDLER ? old_state_ : state();
  }

  template <class NexusClass>
  NexusClass* casted_nexus() {
    return static_cast<NexusClass*>(nexus_);
  }
  FeedbackNexus* nexus() const { return nexus_; }

  inline Code* target() const;

 private:
  inline Address constant_pool() const;
  inline Address raw_constant_pool() const;

  void FindTargetMaps() {
    if (target_maps_set_) return;
    target_maps_set_ = true;
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
  FeedbackSlotKind kind_;
  Handle<Map> receiver_map_;
  MaybeHandle<Object> maybe_handler_;

  ExtraICState extra_ic_state_;
  MapHandles target_maps_;
  bool target_maps_set_;

  const char* slow_stub_reason_;

  FeedbackNexus* nexus_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IC);
};


class CallIC : public IC {
 public:
  CallIC(Isolate* isolate, CallICNexus* nexus)
      : IC(EXTRA_CALL_FRAME, isolate, nexus) {
    DCHECK(nexus != NULL);
  }
};


class LoadIC : public IC {
 public:
  LoadIC(Isolate* isolate, FeedbackNexus* nexus)
      : IC(NO_EXTRA_FRAME, isolate, nexus) {
    DCHECK(nexus != NULL);
    DCHECK(IsAnyLoad());
  }

  static bool ShouldThrowReferenceError(FeedbackSlotKind kind) {
    return kind == FeedbackSlotKind::kLoadGlobalNotInsideTypeof;
  }

  bool ShouldThrowReferenceError() const {
    return ShouldThrowReferenceError(kind());
  }

  MUST_USE_RESULT MaybeHandle<Object> Load(Handle<Object> object,
                                           Handle<Name> name);

 protected:
  virtual Handle<Code> slow_stub() const {
    return isolate()->builtins()->LoadIC_Slow();
  }

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupIterator* lookup);

  Handle<Object> GetMapIndependentHandler(LookupIterator* lookup) override;

  Handle<Code> CompileHandler(LookupIterator* lookup) override;

 private:
  // Creates a data handler that represents a load of a field by given index.
  static Handle<Smi> SimpleFieldLoad(Isolate* isolate, FieldIndex index);

  // Creates a data handler that represents a prototype chain check followed
  // by given Smi-handler that encoded a load from the holder.
  // Can be used only if GetPrototypeCheckCount() returns non negative value.
  Handle<Object> LoadFromPrototype(Handle<Map> receiver_map,
                                   Handle<JSObject> holder, Handle<Name> name,
                                   Handle<Smi> smi_handler);

  // Creates a data handler that represents a load of a non-existent property.
  // {holder} is the object from which the property is loaded. If no holder is
  // needed (e.g., for "nonexistent"), null_value() may be passed in.
  Handle<Object> LoadFullChain(Handle<Map> receiver_map, Handle<Object> holder,
                               Handle<Name> name, Handle<Smi> smi_handler);

  friend class IC;
  friend class NamedLoadHandlerCompiler;
};

class LoadGlobalIC : public LoadIC {
 public:
  LoadGlobalIC(Isolate* isolate, FeedbackNexus* nexus)
      : LoadIC(isolate, nexus) {}

  MUST_USE_RESULT MaybeHandle<Object> Load(Handle<Name> name);

 protected:
  Handle<Code> slow_stub() const override {
    return isolate()->builtins()->LoadGlobalIC_Slow();
  }
};

class KeyedLoadIC : public LoadIC {
 public:
  KeyedLoadIC(Isolate* isolate, KeyedLoadICNexus* nexus)
      : LoadIC(isolate, nexus) {
    DCHECK(nexus != NULL);
  }

  MUST_USE_RESULT MaybeHandle<Object> Load(Handle<Object> object,
                                           Handle<Object> key);

 protected:
  // receiver is HeapObject because it could be a String or a JSObject
  void UpdateLoadElement(Handle<HeapObject> receiver);

 private:
  friend class IC;

  Handle<Object> LoadElementHandler(Handle<Map> receiver_map);

  void LoadElementPolymorphicHandlers(MapHandles* receiver_maps,
                                      List<Handle<Object>>* handlers);
};


class StoreIC : public IC {
 public:
  StoreIC(Isolate* isolate, FeedbackNexus* nexus)
      : IC(NO_EXTRA_FRAME, isolate, nexus) {
    DCHECK(IsAnyStore());
  }

  LanguageMode language_mode() const {
    return nexus()->vector()->GetLanguageMode(nexus()->slot());
  }

  MUST_USE_RESULT MaybeHandle<Object> Store(
      Handle<Object> object, Handle<Name> name, Handle<Object> value,
      JSReceiver::StoreFromKeyed store_mode =
          JSReceiver::CERTAINLY_NOT_STORE_FROM_KEYED);

  bool LookupForWrite(LookupIterator* it, Handle<Object> value,
                      JSReceiver::StoreFromKeyed store_mode);

 protected:
  // Stub accessors.
  Handle<Code> slow_stub() const {
    // All StoreICs share the same slow stub.
    return isolate()->builtins()->KeyedStoreIC_Slow();
  }

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupIterator* lookup, Handle<Object> value,
                    JSReceiver::StoreFromKeyed store_mode);
  Handle<Object> GetMapIndependentHandler(LookupIterator* lookup) override;
  Handle<Code> CompileHandler(LookupIterator* lookup) override;

 private:
  Handle<Object> StoreTransition(Handle<Map> receiver_map,
                                 Handle<JSObject> holder,
                                 Handle<Map> transition, Handle<Name> name);

  friend class IC;
};

class StoreGlobalIC : public StoreIC {
 public:
  StoreGlobalIC(Isolate* isolate, FeedbackNexus* nexus)
      : StoreIC(isolate, nexus) {}

  MUST_USE_RESULT MaybeHandle<Object> Store(Handle<Object> object,
                                            Handle<Name> name,
                                            Handle<Object> value);
};

enum KeyedStoreCheckMap { kDontCheckMap, kCheckMap };


enum KeyedStoreIncrementLength { kDontIncrementLength, kIncrementLength };


class KeyedStoreIC : public StoreIC {
 public:
  KeyedAccessStoreMode GetKeyedAccessStoreMode() {
    return casted_nexus<KeyedStoreICNexus>()->GetKeyedAccessStoreMode();
  }

  KeyedStoreIC(Isolate* isolate, KeyedStoreICNexus* nexus)
      : StoreIC(isolate, nexus) {}

  MUST_USE_RESULT MaybeHandle<Object> Store(Handle<Object> object,
                                            Handle<Object> name,
                                            Handle<Object> value);

 protected:
  void UpdateStoreElement(Handle<Map> receiver_map,
                          KeyedAccessStoreMode store_mode);

 private:
  Handle<Map> ComputeTransitionedMap(Handle<Map> map,
                                     KeyedAccessStoreMode store_mode);

  Handle<Object> StoreElementHandler(Handle<Map> receiver_map,
                                     KeyedAccessStoreMode store_mode);

  void StoreElementPolymorphicHandlers(MapHandles* receiver_maps,
                                       List<Handle<Object>>* handlers,
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
