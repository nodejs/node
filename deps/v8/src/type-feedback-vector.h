// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPE_FEEDBACK_VECTOR_H_
#define V8_TYPE_FEEDBACK_VECTOR_H_

#include "src/checks.h"
#include "src/elements-kind.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

// The shape of the TypeFeedbackVector is an array with:
// 0: first_ic_slot_index (== length() if no ic slots are present)
// 1: ics_with_types
// 2: ics_with_generic_info
// 3: type information for ic slots, if any
// ...
// N: first feedback slot (N >= 3)
// ...
// [<first_ic_slot_index>: feedback slot]
// ...to length() - 1
//
class TypeFeedbackVector : public FixedArray {
 public:
  // Casting.
  static TypeFeedbackVector* cast(Object* obj) {
    DCHECK(obj->IsTypeFeedbackVector());
    return reinterpret_cast<TypeFeedbackVector*>(obj);
  }

  static const int kReservedIndexCount = 3;
  static const int kFirstICSlotIndex = 0;
  static const int kWithTypesIndex = 1;
  static const int kGenericCountIndex = 2;

  int first_ic_slot_index() const {
    DCHECK(length() >= kReservedIndexCount);
    return Smi::cast(get(kFirstICSlotIndex))->value();
  }

  int ic_with_type_info_count() {
    return length() > 0 ? Smi::cast(get(kWithTypesIndex))->value() : 0;
  }

  void change_ic_with_type_info_count(int delta) {
    if (delta == 0) return;
    int value = ic_with_type_info_count() + delta;
    // Could go negative because of the debugger.
    if (value >= 0) {
      set(kWithTypesIndex, Smi::FromInt(value));
    }
  }

  int ic_generic_count() {
    return length() > 0 ? Smi::cast(get(kGenericCountIndex))->value() : 0;
  }

  void change_ic_generic_count(int delta) {
    if (delta == 0) return;
    int value = ic_generic_count() + delta;
    if (value >= 0) {
      set(kGenericCountIndex, Smi::FromInt(value));
    }
  }

  inline int ic_metadata_length() const;

  int Slots() const {
    if (length() == 0) return 0;
    return Max(
        0, first_ic_slot_index() - ic_metadata_length() - kReservedIndexCount);
  }

  int ICSlots() const {
    if (length() == 0) return 0;
    return length() - first_ic_slot_index();
  }

  // Conversion from a slot or ic slot to an integer index to the underlying
  // array.
  int GetIndex(FeedbackVectorSlot slot) const {
    return kReservedIndexCount + ic_metadata_length() + slot.ToInt();
  }

  int GetIndex(FeedbackVectorICSlot slot) const {
    int first_ic_slot = first_ic_slot_index();
    DCHECK(slot.ToInt() < ICSlots());
    return first_ic_slot + slot.ToInt();
  }

  // Conversion from an integer index to either a slot or an ic slot. The caller
  // should know what kind she expects.
  FeedbackVectorSlot ToSlot(int index) const {
    DCHECK(index >= kReservedIndexCount && index < first_ic_slot_index());
    return FeedbackVectorSlot(index - ic_metadata_length() -
                              kReservedIndexCount);
  }

  FeedbackVectorICSlot ToICSlot(int index) const {
    DCHECK(index >= first_ic_slot_index() && index < length());
    return FeedbackVectorICSlot(index - first_ic_slot_index());
  }

  Object* Get(FeedbackVectorSlot slot) const { return get(GetIndex(slot)); }
  void Set(FeedbackVectorSlot slot, Object* value,
           WriteBarrierMode mode = UPDATE_WRITE_BARRIER) {
    set(GetIndex(slot), value, mode);
  }

  Object* Get(FeedbackVectorICSlot slot) const { return get(GetIndex(slot)); }
  void Set(FeedbackVectorICSlot slot, Object* value,
           WriteBarrierMode mode = UPDATE_WRITE_BARRIER) {
    set(GetIndex(slot), value, mode);
  }

  // IC slots need metadata to recognize the type of IC. Set a Kind for every
  // slot. If GetKind() returns Code::NUMBER_OF_KINDS, then there is
  // no kind associated with this slot. This may happen in the current design
  // if a decision is made at compile time not to emit an IC that was planned
  // for at parse time. This can be eliminated if we encode kind at parse
  // time.
  Code::Kind GetKind(FeedbackVectorICSlot slot) const;
  void SetKind(FeedbackVectorICSlot slot, Code::Kind kind);

  static Handle<TypeFeedbackVector> Allocate(Isolate* isolate, int slot_count,
                                             int ic_slot_count);

  static Handle<TypeFeedbackVector> Copy(Isolate* isolate,
                                         Handle<TypeFeedbackVector> vector);

  // Clears the vector slots and the vector ic slots.
  void ClearSlots(SharedFunctionInfo* shared);

  // The object that indicates an uninitialized cache.
  static inline Handle<Object> UninitializedSentinel(Isolate* isolate);

  // The object that indicates a megamorphic state.
  static inline Handle<Object> MegamorphicSentinel(Isolate* isolate);

  // The object that indicates a premonomorphic state.
  static inline Handle<Object> PremonomorphicSentinel(Isolate* isolate);

  // The object that indicates a generic state.
  static inline Handle<Object> GenericSentinel(Isolate* isolate);

  // The object that indicates a monomorphic state of Array with
  // ElementsKind
  static inline Handle<Object> MonomorphicArraySentinel(
      Isolate* isolate, ElementsKind elements_kind);

  // A raw version of the uninitialized sentinel that's safe to read during
  // garbage collection (e.g., for patching the cache).
  static inline Object* RawUninitializedSentinel(Heap* heap);

 private:
  enum VectorICKind {
    KindUnused = 0x0,
    KindCallIC = 0x1,
    KindLoadIC = 0x2,
    KindKeyedLoadIC = 0x3
  };

  static const int kVectorICKindBits = 2;
  static VectorICKind FromCodeKind(Code::Kind kind);
  static Code::Kind FromVectorICKind(VectorICKind kind);
  typedef BitSetComputer<VectorICKind, kVectorICKindBits, kSmiValueSize,
                         uint32_t> VectorICComputer;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TypeFeedbackVector);
};


// A FeedbackNexus is the combination of a TypeFeedbackVector and a slot.
// Derived classes customize the update and retrieval of feedback.
class FeedbackNexus {
 public:
  FeedbackNexus(Handle<TypeFeedbackVector> vector, FeedbackVectorICSlot slot)
      : vector_handle_(vector), vector_(NULL), slot_(slot) {}
  FeedbackNexus(TypeFeedbackVector* vector, FeedbackVectorICSlot slot)
      : vector_(vector), slot_(slot) {}
  virtual ~FeedbackNexus() {}

  Handle<TypeFeedbackVector> vector_handle() const {
    DCHECK(vector_ == NULL);
    return vector_handle_;
  }
  TypeFeedbackVector* vector() const {
    return vector_handle_.is_null() ? vector_ : *vector_handle_;
  }
  FeedbackVectorICSlot slot() const { return slot_; }

  InlineCacheState ic_state() const { return StateFromFeedback(); }
  Map* FindFirstMap() const {
    MapHandleList maps;
    ExtractMaps(&maps);
    if (maps.length() > 0) return *maps.at(0);
    return NULL;
  }

  virtual InlineCacheState StateFromFeedback() const = 0;
  virtual int ExtractMaps(MapHandleList* maps) const = 0;
  virtual MaybeHandle<Code> FindHandlerForMap(Handle<Map> map) const = 0;
  virtual bool FindHandlers(CodeHandleList* code_list, int length = -1) const {
    return length == 0;
  }
  virtual Name* FindFirstName() const { return NULL; }

  Object* GetFeedback() const { return vector()->Get(slot()); }

 protected:
  Isolate* GetIsolate() const { return vector()->GetIsolate(); }

  void SetFeedback(Object* feedback,
                   WriteBarrierMode mode = UPDATE_WRITE_BARRIER) {
    vector()->Set(slot(), feedback, mode);
  }

  Handle<FixedArray> EnsureArrayOfSize(int length);
  void InstallHandlers(int start_index, TypeHandleList* types,
                       CodeHandleList* handlers);
  int ExtractMaps(int start_index, MapHandleList* maps) const;
  MaybeHandle<Code> FindHandlerForMap(int start_index, Handle<Map> map) const;
  bool FindHandlers(int start_index, CodeHandleList* code_list,
                    int length) const;

 private:
  // The reason for having a vector handle and a raw pointer is that we can and
  // should use handles during IC miss, but not during GC when we clear ICs. If
  // you have a handle to the vector that is better because more operations can
  // be done, like allocation.
  Handle<TypeFeedbackVector> vector_handle_;
  TypeFeedbackVector* vector_;
  FeedbackVectorICSlot slot_;
};


class CallICNexus : public FeedbackNexus {
 public:
  CallICNexus(Handle<TypeFeedbackVector> vector, FeedbackVectorICSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->GetKind(slot) == Code::CALL_IC);
  }
  CallICNexus(TypeFeedbackVector* vector, FeedbackVectorICSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->GetKind(slot) == Code::CALL_IC);
  }

  void ConfigureUninitialized();
  void ConfigureGeneric();
  void ConfigureMonomorphicArray();
  void ConfigureMonomorphic(Handle<JSFunction> function);

  virtual InlineCacheState StateFromFeedback() const OVERRIDE;

  virtual int ExtractMaps(MapHandleList* maps) const OVERRIDE {
    // CallICs don't record map feedback.
    return 0;
  }
  virtual MaybeHandle<Code> FindHandlerForMap(Handle<Map> map) const OVERRIDE {
    return MaybeHandle<Code>();
  }
  virtual bool FindHandlers(CodeHandleList* code_list,
                            int length = -1) const OVERRIDE {
    return length == 0;
  }
};
}
}  // namespace v8::internal

#endif  // V8_TRANSITIONS_H_
