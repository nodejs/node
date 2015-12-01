// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPE_FEEDBACK_VECTOR_H_
#define V8_TYPE_FEEDBACK_VECTOR_H_

#include <vector>

#include "src/checks.h"
#include "src/elements-kind.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {


enum class FeedbackVectorSlotKind {
  UNUSED,
  CALL_IC,
  LOAD_IC,
  KEYED_LOAD_IC,
  STORE_IC,
  KEYED_STORE_IC,

  KINDS_NUMBER  // Last value indicating number of kinds.
};


std::ostream& operator<<(std::ostream& os, FeedbackVectorSlotKind kind);


class StaticFeedbackVectorSpec {
 public:
  StaticFeedbackVectorSpec() : slots_(0), ic_slots_(0), ic_kinds_(NULL) {}
  StaticFeedbackVectorSpec(int slots, int ic_slots,
                           FeedbackVectorSlotKind* ic_slot_kinds)
      : slots_(slots), ic_slots_(ic_slots), ic_kinds_(ic_slot_kinds) {}

  int slots() const { return slots_; }

  int ic_slots() const { return ic_slots_; }

  FeedbackVectorSlotKind GetKind(int ic_slot) const {
    DCHECK(ic_slots_ > 0 && ic_slot < ic_slots_);
    return ic_kinds_[ic_slot];
  }

 private:
  int slots_;
  int ic_slots_;
  FeedbackVectorSlotKind* ic_kinds_;
};


class FeedbackVectorSpec {
 public:
  explicit FeedbackVectorSpec(Zone* zone)
      : slots_(0), ic_slots_(0), ic_slot_kinds_(zone) {}

  int slots() const { return slots_; }
  void increase_slots(int count) {
    DCHECK_LT(0, count);
    slots_ += count;
  }

  int ic_slots() const { return ic_slots_; }
  void increase_ic_slots(int count) {
    DCHECK_LT(0, count);
    ic_slots_ += count;
    ic_slot_kinds_.resize(ic_slots_);
  }

  FeedbackVectorICSlot AddSlot(FeedbackVectorSlotKind kind) {
    int slot = ic_slots_;
    increase_ic_slots(1);
    ic_slot_kinds_[slot] = static_cast<unsigned char>(kind);
    return FeedbackVectorICSlot(slot);
  }

  FeedbackVectorICSlot AddSlots(FeedbackVectorSlotKind kind, int count) {
    int slot = ic_slots_;
    increase_ic_slots(count);
    for (int i = 0; i < count; i++) {
      ic_slot_kinds_[slot + i] = static_cast<unsigned char>(kind);
    }
    return FeedbackVectorICSlot(slot);
  }

  FeedbackVectorICSlot AddCallICSlot() {
    return AddSlot(FeedbackVectorSlotKind::CALL_IC);
  }

  FeedbackVectorICSlot AddLoadICSlot() {
    return AddSlot(FeedbackVectorSlotKind::LOAD_IC);
  }

  FeedbackVectorICSlot AddLoadICSlots(int count) {
    return AddSlots(FeedbackVectorSlotKind::LOAD_IC, count);
  }

  FeedbackVectorICSlot AddKeyedLoadICSlot() {
    return AddSlot(FeedbackVectorSlotKind::KEYED_LOAD_IC);
  }

  FeedbackVectorICSlot AddStoreICSlot() {
    return AddSlot(FeedbackVectorSlotKind::STORE_IC);
  }

  FeedbackVectorSlot AddStubSlot() {
    int slot = slots_;
    increase_slots(1);
    return FeedbackVectorSlot(slot);
  }

  FeedbackVectorSlot AddStubSlots(int count) {
    int slot = slots_;
    increase_slots(count);
    return FeedbackVectorSlot(slot);
  }

  FeedbackVectorSlotKind GetKind(int ic_slot) const {
    return static_cast<FeedbackVectorSlotKind>(ic_slot_kinds_.at(ic_slot));
  }

 private:
  int slots_;
  int ic_slots_;
  ZoneVector<unsigned char> ic_slot_kinds_;
};


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
  static inline TypeFeedbackVector* cast(Object* obj);

  static const int kReservedIndexCount = 3;
  static const int kFirstICSlotIndex = 0;
  static const int kWithTypesIndex = 1;
  static const int kGenericCountIndex = 2;

  static int elements_per_ic_slot() { return 2; }

  inline int first_ic_slot_index() const;
  inline int ic_with_type_info_count();
  inline void change_ic_with_type_info_count(int delta);
  inline int ic_generic_count();
  inline void change_ic_generic_count(int delta);
  inline int ic_metadata_length() const;

  bool SpecDiffersFrom(const FeedbackVectorSpec* other_spec) const;

  inline int Slots() const;
  inline int ICSlots() const;

  // Conversion from a slot or ic slot to an integer index to the underlying
  // array.
  inline int GetIndex(FeedbackVectorSlot slot) const;
  inline int GetIndex(FeedbackVectorICSlot slot) const;

  template <typename Spec>
  static int GetIndexFromSpec(const Spec* spec, FeedbackVectorSlot slot);
  template <typename Spec>
  static int GetIndexFromSpec(const Spec* spec, FeedbackVectorICSlot slot);

  // Conversion from an integer index to either a slot or an ic slot. The caller
  // should know what kind she expects.
  inline FeedbackVectorSlot ToSlot(int index) const;
  inline FeedbackVectorICSlot ToICSlot(int index) const;
  inline Object* Get(FeedbackVectorSlot slot) const;
  inline void Set(FeedbackVectorSlot slot, Object* value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Object* Get(FeedbackVectorICSlot slot) const;
  inline void Set(FeedbackVectorICSlot slot, Object* value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // IC slots need metadata to recognize the type of IC.
  FeedbackVectorSlotKind GetKind(FeedbackVectorICSlot slot) const;

  template <typename Spec>
  static Handle<TypeFeedbackVector> Allocate(Isolate* isolate,
                                             const Spec* spec);

  static Handle<TypeFeedbackVector> Copy(Isolate* isolate,
                                         Handle<TypeFeedbackVector> vector);

#ifdef OBJECT_PRINT
  // For gdb debugging.
  void Print();
#endif  // OBJECT_PRINT

  DECLARE_PRINTER(TypeFeedbackVector)

  // Clears the vector slots and the vector ic slots.
  void ClearSlots(SharedFunctionInfo* shared) { ClearSlotsImpl(shared, true); }
  void ClearSlotsAtGCTime(SharedFunctionInfo* shared) {
    ClearSlotsImpl(shared, false);
  }

  void ClearICSlots(SharedFunctionInfo* shared) {
    ClearICSlotsImpl(shared, true);
  }
  void ClearICSlotsAtGCTime(SharedFunctionInfo* shared) {
    ClearICSlotsImpl(shared, false);
  }

  static void ClearAllKeyedStoreICs(Isolate* isolate);
  void ClearKeyedStoreICs(SharedFunctionInfo* shared);

  // The object that indicates an uninitialized cache.
  static inline Handle<Object> UninitializedSentinel(Isolate* isolate);

  // The object that indicates a megamorphic state.
  static inline Handle<Object> MegamorphicSentinel(Isolate* isolate);

  // The object that indicates a premonomorphic state.
  static inline Handle<Object> PremonomorphicSentinel(Isolate* isolate);

  // A raw version of the uninitialized sentinel that's safe to read during
  // garbage collection (e.g., for patching the cache).
  static inline Object* RawUninitializedSentinel(Heap* heap);

  static const int kDummyLoadICSlot = 0;
  static const int kDummyKeyedLoadICSlot = 1;
  static const int kDummyStoreICSlot = 2;
  static const int kDummyKeyedStoreICSlot = 3;

  static Handle<TypeFeedbackVector> DummyVector(Isolate* isolate);
  static FeedbackVectorICSlot DummySlot(int dummyIndex) {
    DCHECK(dummyIndex >= 0 && dummyIndex <= kDummyKeyedStoreICSlot);
    return FeedbackVectorICSlot(dummyIndex);
  }

  static int PushAppliedArgumentsIndex();
  static Handle<TypeFeedbackVector> CreatePushAppliedArgumentsVector(
      Isolate* isolate);

  static const char* Kind2String(FeedbackVectorSlotKind kind);

 private:
  static const int kFeedbackVectorSlotKindBits = 3;
  STATIC_ASSERT(static_cast<int>(FeedbackVectorSlotKind::KINDS_NUMBER) <
                (1 << kFeedbackVectorSlotKindBits));

  void SetKind(FeedbackVectorICSlot slot, FeedbackVectorSlotKind kind);

  typedef BitSetComputer<FeedbackVectorSlotKind, kFeedbackVectorSlotKindBits,
                         kSmiValueSize, uint32_t> VectorICComputer;

  void ClearSlotsImpl(SharedFunctionInfo* shared, bool force_clear);
  void ClearICSlotsImpl(SharedFunctionInfo* shared, bool force_clear);

  DISALLOW_IMPLICIT_CONSTRUCTORS(TypeFeedbackVector);
};


// The following asserts protect an optimization in type feedback vector
// code that looks into the contents of a slot assuming to find a String,
// a Symbol, an AllocationSite, a WeakCell, or a FixedArray.
STATIC_ASSERT(WeakCell::kSize >= 2 * kPointerSize);
STATIC_ASSERT(WeakCell::kValueOffset == AllocationSite::kTransitionInfoOffset);
STATIC_ASSERT(WeakCell::kValueOffset == FixedArray::kLengthOffset);
STATIC_ASSERT(WeakCell::kValueOffset == Name::kHashFieldSlot);
// Verify that an empty hash field looks like a tagged object, but can't
// possibly be confused with a pointer.
STATIC_ASSERT((Name::kEmptyHashField & kHeapObjectTag) == kHeapObjectTag);
STATIC_ASSERT(Name::kEmptyHashField == 0x3);
// Verify that a set hash field will not look like a tagged object.
STATIC_ASSERT(Name::kHashNotComputedMask == kHeapObjectTag);


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

  // TODO(mvstanton): remove FindAllMaps, it didn't survive a code review.
  void FindAllMaps(MapHandleList* maps) const { ExtractMaps(maps); }

  virtual InlineCacheState StateFromFeedback() const = 0;
  virtual int ExtractMaps(MapHandleList* maps) const;
  virtual MaybeHandle<Code> FindHandlerForMap(Handle<Map> map) const;
  virtual bool FindHandlers(CodeHandleList* code_list, int length = -1) const;
  virtual Name* FindFirstName() const { return NULL; }

  virtual void ConfigureUninitialized();
  virtual void ConfigurePremonomorphic();
  virtual void ConfigureMegamorphic();

  inline Object* GetFeedback() const;
  inline Object* GetFeedbackExtra() const;

 protected:
  inline Isolate* GetIsolate() const;

  inline void SetFeedback(Object* feedback,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void SetFeedbackExtra(Object* feedback_extra,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  Handle<FixedArray> EnsureArrayOfSize(int length);
  Handle<FixedArray> EnsureExtraArrayOfSize(int length);
  void InstallHandlers(Handle<FixedArray> array, MapHandleList* maps,
                       CodeHandleList* handlers);

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
  // Monomorphic call ics store call counts. Platform code needs to increment
  // the count appropriately (ie, by 2).
  static const int kCallCountIncrement = 2;

  CallICNexus(Handle<TypeFeedbackVector> vector, FeedbackVectorICSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::CALL_IC, vector->GetKind(slot));
  }
  CallICNexus(TypeFeedbackVector* vector, FeedbackVectorICSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::CALL_IC, vector->GetKind(slot));
  }

  void Clear(Code* host);

  void ConfigureMonomorphicArray();
  void ConfigureMonomorphic(Handle<JSFunction> function);

  InlineCacheState StateFromFeedback() const override;

  int ExtractMaps(MapHandleList* maps) const override {
    // CallICs don't record map feedback.
    return 0;
  }
  MaybeHandle<Code> FindHandlerForMap(Handle<Map> map) const override {
    return MaybeHandle<Code>();
  }
  bool FindHandlers(CodeHandleList* code_list, int length = -1) const override {
    return length == 0;
  }

  int ExtractCallCount();
};


class LoadICNexus : public FeedbackNexus {
 public:
  LoadICNexus(Handle<TypeFeedbackVector> vector, FeedbackVectorICSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::LOAD_IC, vector->GetKind(slot));
  }
  explicit LoadICNexus(Isolate* isolate)
      : FeedbackNexus(TypeFeedbackVector::DummyVector(isolate),
                      TypeFeedbackVector::DummySlot(
                          TypeFeedbackVector::kDummyLoadICSlot)) {}
  LoadICNexus(TypeFeedbackVector* vector, FeedbackVectorICSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::LOAD_IC, vector->GetKind(slot));
  }

  void Clear(Code* host);

  void ConfigureMonomorphic(Handle<Map> receiver_map, Handle<Code> handler);

  void ConfigurePolymorphic(MapHandleList* maps, CodeHandleList* handlers);

  InlineCacheState StateFromFeedback() const override;
};


class KeyedLoadICNexus : public FeedbackNexus {
 public:
  KeyedLoadICNexus(Handle<TypeFeedbackVector> vector, FeedbackVectorICSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::KEYED_LOAD_IC, vector->GetKind(slot));
  }
  KeyedLoadICNexus(TypeFeedbackVector* vector, FeedbackVectorICSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::KEYED_LOAD_IC, vector->GetKind(slot));
  }

  void Clear(Code* host);

  // name can be a null handle for element loads.
  void ConfigureMonomorphic(Handle<Name> name, Handle<Map> receiver_map,
                            Handle<Code> handler);
  // name can be null.
  void ConfigurePolymorphic(Handle<Name> name, MapHandleList* maps,
                            CodeHandleList* handlers);

  InlineCacheState StateFromFeedback() const override;
  Name* FindFirstName() const override;
};


class StoreICNexus : public FeedbackNexus {
 public:
  StoreICNexus(Handle<TypeFeedbackVector> vector, FeedbackVectorICSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::STORE_IC, vector->GetKind(slot));
  }
  explicit StoreICNexus(Isolate* isolate)
      : FeedbackNexus(TypeFeedbackVector::DummyVector(isolate),
                      TypeFeedbackVector::DummySlot(
                          TypeFeedbackVector::kDummyStoreICSlot)) {}
  StoreICNexus(TypeFeedbackVector* vector, FeedbackVectorICSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::STORE_IC, vector->GetKind(slot));
  }

  void Clear(Code* host);

  void ConfigureMonomorphic(Handle<Map> receiver_map, Handle<Code> handler);

  void ConfigurePolymorphic(MapHandleList* maps, CodeHandleList* handlers);

  InlineCacheState StateFromFeedback() const override;
};


class KeyedStoreICNexus : public FeedbackNexus {
 public:
  KeyedStoreICNexus(Handle<TypeFeedbackVector> vector,
                    FeedbackVectorICSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::KEYED_STORE_IC, vector->GetKind(slot));
  }
  explicit KeyedStoreICNexus(Isolate* isolate)
      : FeedbackNexus(TypeFeedbackVector::DummyVector(isolate),
                      TypeFeedbackVector::DummySlot(
                          TypeFeedbackVector::kDummyKeyedStoreICSlot)) {}
  KeyedStoreICNexus(TypeFeedbackVector* vector, FeedbackVectorICSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::KEYED_STORE_IC, vector->GetKind(slot));
  }

  void Clear(Code* host);

  // name can be a null handle for element loads.
  void ConfigureMonomorphic(Handle<Name> name, Handle<Map> receiver_map,
                            Handle<Code> handler);
  // name can be null.
  void ConfigurePolymorphic(Handle<Name> name, MapHandleList* maps,
                            CodeHandleList* handlers);
  void ConfigurePolymorphic(MapHandleList* maps,
                            MapHandleList* transitioned_maps,
                            CodeHandleList* handlers);

  KeyedAccessStoreMode GetKeyedAccessStoreMode() const;
  IcCheckType GetKeyType() const;

  InlineCacheState StateFromFeedback() const override;
  Name* FindFirstName() const override;
};
}
}  // namespace v8::internal

#endif  // V8_TRANSITIONS_H_
