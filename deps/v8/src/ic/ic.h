// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_IC_H_
#define V8_IC_IC_H_

#include <vector>

#include "src/common/message-template.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/ic/stub-cache.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/map.h"
#include "src/objects/maybe-object.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

enum class NamedPropertyType : bool { kNotOwn, kOwn };

//
// IC is the base class for LoadIC, StoreIC, KeyedLoadIC, and KeyedStoreIC.
//
class IC {
 public:
  // Alias the inline cache state type to make the IC code more readable.
  using State = InlineCacheState;

  // Construct the IC structure with the given number of extra
  // JavaScript frames on the stack.
  IC(Isolate* isolate, Handle<FeedbackVector> vector, FeedbackSlot slot,
     FeedbackSlotKind kind);
  virtual ~IC() = default;

  State state() const { return state_; }

  // Compute the current IC state based on the target stub, lookup_start_object
  // and name.
  void UpdateState(DirectHandle<Object> lookup_start_object,
                   DirectHandle<Object> name);

  bool RecomputeHandlerForName(DirectHandle<Object> name);
  void MarkRecomputeHandler(DirectHandle<Object> name) {
    DCHECK(RecomputeHandlerForName(name));
    old_state_ = state_;
    state_ = InlineCacheState::RECOMPUTE_HANDLER;
  }

  bool IsAnyHas() const { return IsKeyedHasIC(); }
  bool IsAnyLoad() const {
    return IsLoadIC() || IsLoadGlobalIC() || IsKeyedLoadIC();
  }
  bool IsAnyStore() const {
    return IsSetNamedIC() || IsDefineNamedOwnIC() || IsStoreGlobalIC() ||
           IsKeyedStoreIC() || IsStoreInArrayLiteralICKind(kind()) ||
           IsDefineKeyedOwnIC();
  }
  bool IsAnyDefineOwn() const {
    return IsDefineNamedOwnIC() || IsDefineKeyedOwnIC();
  }

  static inline bool IsHandler(Tagged<MaybeObject> object);

  // Notify the IC system that a feedback has changed.
  static void OnFeedbackChanged(Isolate* isolate, Tagged<FeedbackVector> vector,
                                FeedbackSlot slot, const char* reason);

  void OnFeedbackChanged(const char* reason);

 protected:
  void set_slow_stub_reason(const char* reason) { slow_stub_reason_ = reason; }
  void set_accessor(Handle<Object> accessor) { accessor_ = accessor; }
  MaybeHandle<Object> accessor() const { return accessor_; }

  Isolate* isolate() const { return isolate_; }

  bool is_vector_set() { return vector_set_; }
  inline bool vector_needs_update();

  // Configure for most states.
  bool ConfigureVectorState(IC::State new_state, DirectHandle<Object> key);
  // Configure the vector for MONOMORPHIC.
  void ConfigureVectorState(DirectHandle<Name> name, DirectHandle<Map> map,
                            DirectHandle<Object> handler);
  void ConfigureVectorState(DirectHandle<Name> name, DirectHandle<Map> map,
                            const MaybeObjectDirectHandle& handler);
  // Configure the vector for POLYMORPHIC.
  void ConfigureVectorState(DirectHandle<Name> name, MapHandlesSpan maps,
                            MaybeObjectHandles* handlers);
  void ConfigureVectorState(DirectHandle<Name> name,
                            MapsAndHandlers const& maps_and_handlers);

  char TransitionMarkFromState(IC::State state);
  void TraceIC(const char* type, DirectHandle<Object> name);
  void TraceIC(const char* type, DirectHandle<Object> name, State old_state,
               State new_state);

  MaybeDirectHandle<Object> TypeError(MessageTemplate, Handle<Object> object,
                                      Handle<Object> key);
  MaybeDirectHandle<Object> ReferenceError(Handle<Name> name);

  void UpdateMonomorphicIC(const MaybeObjectDirectHandle& handler,
                           DirectHandle<Name> name);
  bool UpdateMegaDOMIC(const MaybeObjectDirectHandle& handler,
                       DirectHandle<Name> name);
  bool UpdatePolymorphicIC(DirectHandle<Name> name,
                           const MaybeObjectHandle& handler);
  void UpdateMegamorphicCache(DirectHandle<Map> map, DirectHandle<Name> name,
                              const MaybeObjectDirectHandle& handler);

  StubCache* stub_cache();

  void CopyICToMegamorphicCache(DirectHandle<Name> name);
  bool IsTransitionOfMonomorphicTarget(Tagged<Map> source_map,
                                       Tagged<Map> target_map);
  void SetCache(DirectHandle<Name> name, Handle<Object> handler);
  void SetCache(DirectHandle<Name> name, const MaybeObjectHandle& handler);
  FeedbackSlotKind kind() const { return kind_; }
  bool IsGlobalIC() const { return IsLoadGlobalIC() || IsStoreGlobalIC(); }
  bool IsLoadIC() const { return IsLoadICKind(kind_); }
  bool IsLoadGlobalIC() const { return IsLoadGlobalICKind(kind_); }
  bool IsKeyedLoadIC() const { return IsKeyedLoadICKind(kind_); }
  bool IsStoreGlobalIC() const { return IsStoreGlobalICKind(kind_); }
  bool IsSetNamedIC() const { return IsSetNamedICKind(kind_); }
  bool IsDefineNamedOwnIC() const { return IsDefineNamedOwnICKind(kind_); }
  bool IsStoreInArrayLiteralIC() const {
    return IsStoreInArrayLiteralICKind(kind_);
  }
  bool IsKeyedStoreIC() const { return IsKeyedStoreICKind(kind_); }
  bool IsKeyedHasIC() const { return IsKeyedHasICKind(kind_); }
  bool IsDefineKeyedOwnIC() const { return IsDefineKeyedOwnICKind(kind_); }
  bool is_keyed() const {
    return IsKeyedLoadIC() || IsKeyedStoreIC() || IsStoreInArrayLiteralIC() ||
           IsKeyedHasIC() || IsDefineKeyedOwnIC();
  }
  bool ShouldRecomputeHandler(DirectHandle<String> name);

  Handle<Map> lookup_start_object_map() { return lookup_start_object_map_; }
  inline void update_lookup_start_object_map(DirectHandle<Object> object);

  void TargetMaps(MapHandles* list) {
    FindTargetMaps();
    for (Handle<Map> map : target_maps_) {
      list->push_back(map);
    }
  }

  Tagged<Map> FirstTargetMap() {
    FindTargetMaps();
    return !target_maps_.empty() ? *target_maps_[0] : Tagged<Map>();
  }

  const FeedbackNexus* nexus() const { return &nexus_; }
  FeedbackNexus* nexus() { return &nexus_; }

 private:
  void FindTargetMaps() {
    if (target_maps_set_) return;
    target_maps_set_ = true;
    nexus()->ExtractMaps(&target_maps_);
  }

  Isolate* isolate_;

  bool vector_set_;
  State old_state_;  // For saving if we marked as prototype failure.
  State state_;
  FeedbackSlotKind kind_;
  Handle<Map> lookup_start_object_map_;
  MaybeHandle<Object> accessor_;
  MapHandles target_maps_;
  bool target_maps_set_;

  const char* slow_stub_reason_;

  FeedbackNexus nexus_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IC);
};

class LoadIC : public IC {
 public:
  LoadIC(Isolate* isolate, Handle<FeedbackVector> vector, FeedbackSlot slot,
         FeedbackSlotKind kind)
      : IC(isolate, vector, slot, kind) {
    DCHECK(IsAnyLoad() || IsAnyHas());
  }

  static bool ShouldThrowReferenceError(FeedbackSlotKind kind) {
    return kind == FeedbackSlotKind::kLoadGlobalNotInsideTypeof;
  }

  bool ShouldThrowReferenceError() const {
    return ShouldThrowReferenceError(kind());
  }

  // If receiver is empty, use object as the receiver.
  V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object> Load(
      Handle<JSAny> object, Handle<Name> name, bool update_feedback = true,
      DirectHandle<JSAny> receiver = DirectHandle<JSAny>());

 protected:
  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupIterator* lookup);

 private:
  MaybeObjectHandle ComputeHandler(LookupIterator* lookup);

  friend class IC;
  friend class NamedLoadHandlerCompiler;
};

class LoadGlobalIC : public LoadIC {
 public:
  LoadGlobalIC(Isolate* isolate, Handle<FeedbackVector> vector,
               FeedbackSlot slot, FeedbackSlotKind kind)
      : LoadIC(isolate, vector, slot, kind) {}

  V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object> Load(
      Handle<Name> name, bool update_feedback = true);
};

class KeyedLoadIC : public LoadIC {
 public:
  KeyedLoadIC(Isolate* isolate, Handle<FeedbackVector> vector,
              FeedbackSlot slot, FeedbackSlotKind kind)
      : LoadIC(isolate, vector, slot, kind) {}

  V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object> Load(Handle<JSAny> object,
                                                       Handle<Object> key);

 protected:
  V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object> RuntimeLoad(
      DirectHandle<JSAny> object, DirectHandle<Object> key,
      bool* is_found = nullptr);

  V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object> LoadName(
      Handle<JSAny> object, DirectHandle<Object> key, Handle<Name> name);

  // receiver is HeapObject because it could be a String or a JSObject
  void UpdateLoadElement(DirectHandle<HeapObject> receiver,
                         KeyedAccessLoadMode new_load_mode);

 private:
  friend class IC;

  Handle<Object> LoadElementHandler(DirectHandle<Map> receiver_map,
                                    KeyedAccessLoadMode new_load_mode);

  void LoadElementPolymorphicHandlers(MapHandles* receiver_maps,
                                      MaybeObjectHandles* handlers,
                                      KeyedAccessLoadMode new_load_mode);

  KeyedAccessLoadMode GetKeyedAccessLoadModeFor(
      DirectHandle<Map> receiver_map) const;
};

class StoreIC : public IC {
 public:
  StoreIC(Isolate* isolate, Handle<FeedbackVector> vector, FeedbackSlot slot,
          FeedbackSlotKind kind)
      : IC(isolate, vector, slot, kind) {
    DCHECK(IsAnyStore());
  }

  V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object> Store(
      Handle<JSAny> object, Handle<Name> name, DirectHandle<Object> value,
      StoreOrigin store_origin = StoreOrigin::kNamed);

  bool LookupForWrite(LookupIterator* it, DirectHandle<Object> value,
                      StoreOrigin store_origin);

 protected:
  // Stub accessors.
  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupIterator* lookup, DirectHandle<Object> value,
                    StoreOrigin store_origin);

 private:
  MaybeObjectHandle ComputeHandler(LookupIterator* lookup);

  friend class IC;
};

class StoreGlobalIC : public StoreIC {
 public:
  StoreGlobalIC(Isolate* isolate, Handle<FeedbackVector> vector,
                FeedbackSlot slot, FeedbackSlotKind kind)
      : StoreIC(isolate, vector, slot, kind) {}

  V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object> Store(
      Handle<Name> name, DirectHandle<Object> value);
};

enum KeyedStoreCheckMap { kDontCheckMap, kCheckMap };

enum KeyedStoreIncrementLength { kDontIncrementLength, kIncrementLength };

enum class TransitionMode {
  kNoTransition,
  kTransitionToDouble,
  kTransitionToObject
};

class KeyedStoreIC : public StoreIC {
 public:
  KeyedAccessStoreMode GetKeyedAccessStoreMode() {
    return nexus()->GetKeyedAccessStoreMode();
  }

  KeyedStoreIC(Isolate* isolate, Handle<FeedbackVector> vector,
               FeedbackSlot slot, FeedbackSlotKind kind)
      : StoreIC(isolate, vector, slot, kind) {}

  V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object> Store(Handle<JSAny> object,
                                                        Handle<Object> name,
                                                        Handle<Object> value);

 protected:
  void UpdateStoreElement(Handle<Map> receiver_map,
                          KeyedAccessStoreMode store_mode,
                          Handle<Map> new_receiver_map);

 private:
  DirectHandle<Map> ComputeTransitionedMap(Handle<Map> map,
                                           TransitionMode transition_mode);

  Handle<Object> StoreElementHandler(DirectHandle<Map> receiver_map,
                                     KeyedAccessStoreMode store_mode,
                                     MaybeDirectHandle<UnionOf<Smi, Cell>>
                                         prev_validity_cell = kNullMaybeHandle);

  void StoreElementPolymorphicHandlers(
      MapsAndHandlers* receiver_maps_and_handlers,
      KeyedAccessStoreMode store_mode);

  friend class IC;
};

class StoreInArrayLiteralIC : public KeyedStoreIC {
 public:
  StoreInArrayLiteralIC(Isolate* isolate, Handle<FeedbackVector> vector,
                        FeedbackSlot slot)
      : KeyedStoreIC(isolate, vector, slot,
                     FeedbackSlotKind::kStoreInArrayLiteral) {
    DCHECK(IsStoreInArrayLiteralICKind(kind()));
  }

  MaybeDirectHandle<Object> Store(DirectHandle<JSArray> array,
                                  Handle<Object> index,
                                  DirectHandle<Object> value);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_IC_H_
