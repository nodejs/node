// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPE_FEEDBACK_VECTOR_H_
#define V8_TYPE_FEEDBACK_VECTOR_H_

#include <vector>

#include "src/base/logging.h"
#include "src/elements-kind.h"
#include "src/objects.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {


enum class FeedbackVectorSlotKind {
  // This kind means that the slot points to the middle of other slot
  // which occupies more than one feedback vector element.
  // There must be no such slots in the system.
  INVALID,

  CALL_IC,
  LOAD_IC,
  KEYED_LOAD_IC,
  STORE_IC,
  KEYED_STORE_IC,

  // This is a general purpose slot that occupies one feedback vector element.
  GENERAL,

  KINDS_NUMBER  // Last value indicating number of kinds.
};


std::ostream& operator<<(std::ostream& os, FeedbackVectorSlotKind kind);


template <typename Derived>
class FeedbackVectorSpecBase {
 public:
  inline FeedbackVectorSlot AddSlot(FeedbackVectorSlotKind kind);

  FeedbackVectorSlot AddCallICSlot() {
    return AddSlot(FeedbackVectorSlotKind::CALL_IC);
  }

  FeedbackVectorSlot AddLoadICSlot() {
    return AddSlot(FeedbackVectorSlotKind::LOAD_IC);
  }

  FeedbackVectorSlot AddKeyedLoadICSlot() {
    return AddSlot(FeedbackVectorSlotKind::KEYED_LOAD_IC);
  }

  FeedbackVectorSlot AddStoreICSlot() {
    return AddSlot(FeedbackVectorSlotKind::STORE_IC);
  }

  FeedbackVectorSlot AddKeyedStoreICSlot() {
    return AddSlot(FeedbackVectorSlotKind::KEYED_STORE_IC);
  }

  FeedbackVectorSlot AddGeneralSlot() {
    return AddSlot(FeedbackVectorSlotKind::GENERAL);
  }
};


class StaticFeedbackVectorSpec
    : public FeedbackVectorSpecBase<StaticFeedbackVectorSpec> {
 public:
  StaticFeedbackVectorSpec() : slots_(0) {}

  int slots() const { return slots_; }

  FeedbackVectorSlotKind GetKind(int slot) const {
    DCHECK(slot >= 0 && slot < slots_);
    return kinds_[slot];
  }

 private:
  friend class FeedbackVectorSpecBase<StaticFeedbackVectorSpec>;

  void append(FeedbackVectorSlotKind kind) {
    DCHECK(slots_ < kMaxLength);
    kinds_[slots_++] = kind;
  }

  static const int kMaxLength = 12;

  int slots_;
  FeedbackVectorSlotKind kinds_[kMaxLength];
};


class FeedbackVectorSpec : public FeedbackVectorSpecBase<FeedbackVectorSpec> {
 public:
  explicit FeedbackVectorSpec(Zone* zone) : slot_kinds_(zone) {
    slot_kinds_.reserve(16);
  }

  int slots() const { return static_cast<int>(slot_kinds_.size()); }

  FeedbackVectorSlotKind GetKind(int slot) const {
    return static_cast<FeedbackVectorSlotKind>(slot_kinds_.at(slot));
  }

 private:
  friend class FeedbackVectorSpecBase<FeedbackVectorSpec>;

  void append(FeedbackVectorSlotKind kind) {
    slot_kinds_.push_back(static_cast<unsigned char>(kind));
  }

  ZoneVector<unsigned char> slot_kinds_;
};


// The shape of the TypeFeedbackMetadata is an array with:
// 0: slot_count
// 1..N: slot kinds packed into a bit vector
//
class TypeFeedbackMetadata : public FixedArray {
 public:
  // Casting.
  static inline TypeFeedbackMetadata* cast(Object* obj);

  static const int kSlotsCountIndex = 0;
  static const int kReservedIndexCount = 1;

  // Returns number of feedback vector elements used by given slot kind.
  static inline int GetSlotSize(FeedbackVectorSlotKind kind);

  bool SpecDiffersFrom(const FeedbackVectorSpec* other_spec) const;

  // Returns number of slots in the vector.
  inline int slot_count() const;

  // Returns slot kind for given slot.
  FeedbackVectorSlotKind GetKind(FeedbackVectorSlot slot) const;

  template <typename Spec>
  static Handle<TypeFeedbackMetadata> New(Isolate* isolate, const Spec* spec);

#ifdef OBJECT_PRINT
  // For gdb debugging.
  void Print();
#endif  // OBJECT_PRINT

  DECLARE_PRINTER(TypeFeedbackMetadata)

  static const char* Kind2String(FeedbackVectorSlotKind kind);

 private:
  static const int kFeedbackVectorSlotKindBits = 3;
  STATIC_ASSERT(static_cast<int>(FeedbackVectorSlotKind::KINDS_NUMBER) <
                (1 << kFeedbackVectorSlotKindBits));

  void SetKind(FeedbackVectorSlot slot, FeedbackVectorSlotKind kind);

  typedef BitSetComputer<FeedbackVectorSlotKind, kFeedbackVectorSlotKindBits,
                         kSmiValueSize, uint32_t> VectorICComputer;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TypeFeedbackMetadata);
};


// The shape of the TypeFeedbackVector is an array with:
// 0: feedback metadata
// 1: ics_with_types
// 2: ics_with_generic_info
// 3: feedback slot #0 (N >= 3)
// ...
// N + slot_count - 1: feedback slot #(slot_count-1)
//
class TypeFeedbackVector : public FixedArray {
 public:
  // Casting.
  static inline TypeFeedbackVector* cast(Object* obj);

  static const int kMetadataIndex = 0;
  static const int kReservedIndexCount = 1;

  inline void ComputeCounts(int* with_type_info, int* generic);

  inline bool is_empty() const;

  // Returns number of slots in the vector.
  inline int slot_count() const;

  inline TypeFeedbackMetadata* metadata() const;

  // Conversion from a slot to an integer index to the underlying array.
  inline int GetIndex(FeedbackVectorSlot slot) const;
  static int GetIndexFromSpec(const FeedbackVectorSpec* spec,
                              FeedbackVectorSlot slot);

  // Conversion from an integer index to the underlying array to a slot.
  inline FeedbackVectorSlot ToSlot(int index) const;
  inline Object* Get(FeedbackVectorSlot slot) const;
  inline void Set(FeedbackVectorSlot slot, Object* value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Returns slot kind for given slot.
  inline FeedbackVectorSlotKind GetKind(FeedbackVectorSlot slot) const;

  static Handle<TypeFeedbackVector> New(Isolate* isolate,
                                        Handle<TypeFeedbackMetadata> metadata);

  static Handle<TypeFeedbackVector> Copy(Isolate* isolate,
                                         Handle<TypeFeedbackVector> vector);

#ifdef OBJECT_PRINT
  // For gdb debugging.
  void Print();
#endif  // OBJECT_PRINT

  DECLARE_PRINTER(TypeFeedbackVector)

  // Clears the vector slots.
  void ClearSlots(SharedFunctionInfo* shared) { ClearSlotsImpl(shared, true); }

  void ClearSlotsAtGCTime(SharedFunctionInfo* shared) {
    ClearSlotsImpl(shared, false);
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
  static inline Object* RawUninitializedSentinel(Isolate* isolate);

  static const int kDummyLoadICSlot = 0;
  static const int kDummyKeyedLoadICSlot = 2;
  static const int kDummyStoreICSlot = 4;
  static const int kDummyKeyedStoreICSlot = 6;

  static Handle<TypeFeedbackVector> DummyVector(Isolate* isolate);
  static FeedbackVectorSlot DummySlot(int dummyIndex) {
    DCHECK(dummyIndex >= 0 && dummyIndex <= kDummyKeyedStoreICSlot);
    return FeedbackVectorSlot(dummyIndex);
  }

 private:
  void ClearSlotsImpl(SharedFunctionInfo* shared, bool force_clear);

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


class TypeFeedbackMetadataIterator {
 public:
  explicit TypeFeedbackMetadataIterator(Handle<TypeFeedbackMetadata> metadata)
      : metadata_handle_(metadata),
        slot_(FeedbackVectorSlot(0)),
        slot_kind_(FeedbackVectorSlotKind::INVALID) {}

  explicit TypeFeedbackMetadataIterator(TypeFeedbackMetadata* metadata)
      : metadata_(metadata),
        slot_(FeedbackVectorSlot(0)),
        slot_kind_(FeedbackVectorSlotKind::INVALID) {}

  bool HasNext() const { return slot_.ToInt() < metadata()->slot_count(); }

  FeedbackVectorSlot Next() {
    DCHECK(HasNext());
    FeedbackVectorSlot slot = slot_;
    slot_kind_ = metadata()->GetKind(slot);
    slot_ = FeedbackVectorSlot(slot_.ToInt() + entry_size());
    return slot;
  }

  // Returns slot kind of the last slot returned by Next().
  FeedbackVectorSlotKind kind() const {
    DCHECK_NE(FeedbackVectorSlotKind::INVALID, slot_kind_);
    DCHECK_NE(FeedbackVectorSlotKind::KINDS_NUMBER, slot_kind_);
    return slot_kind_;
  }

  // Returns entry size of the last slot returned by Next().
  int entry_size() const { return TypeFeedbackMetadata::GetSlotSize(kind()); }

 private:
  TypeFeedbackMetadata* metadata() const {
    return !metadata_handle_.is_null() ? *metadata_handle_ : metadata_;
  }

  // The reason for having a handle and a raw pointer to the meta data is
  // to have a single iterator implementation for both "handlified" and raw
  // pointer use cases.
  Handle<TypeFeedbackMetadata> metadata_handle_;
  TypeFeedbackMetadata* metadata_;
  FeedbackVectorSlot slot_;
  FeedbackVectorSlotKind slot_kind_;
};


// A FeedbackNexus is the combination of a TypeFeedbackVector and a slot.
// Derived classes customize the update and retrieval of feedback.
class FeedbackNexus {
 public:
  FeedbackNexus(Handle<TypeFeedbackVector> vector, FeedbackVectorSlot slot)
      : vector_handle_(vector), vector_(NULL), slot_(slot) {}
  FeedbackNexus(TypeFeedbackVector* vector, FeedbackVectorSlot slot)
      : vector_(vector), slot_(slot) {}
  virtual ~FeedbackNexus() {}

  Handle<TypeFeedbackVector> vector_handle() const {
    DCHECK(vector_ == NULL);
    return vector_handle_;
  }
  TypeFeedbackVector* vector() const {
    return vector_handle_.is_null() ? vector_ : *vector_handle_;
  }
  FeedbackVectorSlot slot() const { return slot_; }

  InlineCacheState ic_state() const { return StateFromFeedback(); }
  bool IsUninitialized() const { return StateFromFeedback() == UNINITIALIZED; }
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

  inline Isolate* GetIsolate() const;

 protected:
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
  FeedbackVectorSlot slot_;
};


class CallICNexus final : public FeedbackNexus {
 public:
  // Monomorphic call ics store call counts. Platform code needs to increment
  // the count appropriately (ie, by 2).
  static const int kCallCountIncrement = 2;

  CallICNexus(Handle<TypeFeedbackVector> vector, FeedbackVectorSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::CALL_IC, vector->GetKind(slot));
  }
  CallICNexus(TypeFeedbackVector* vector, FeedbackVectorSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::CALL_IC, vector->GetKind(slot));
  }

  void Clear(Code* host);

  void ConfigureMonomorphicArray();
  void ConfigureMonomorphic(Handle<JSFunction> function);
  void ConfigureMegamorphic() final;
  void ConfigureMegamorphic(int call_count);

  InlineCacheState StateFromFeedback() const final;

  int ExtractMaps(MapHandleList* maps) const final {
    // CallICs don't record map feedback.
    return 0;
  }
  MaybeHandle<Code> FindHandlerForMap(Handle<Map> map) const final {
    return MaybeHandle<Code>();
  }
  bool FindHandlers(CodeHandleList* code_list, int length = -1) const final {
    return length == 0;
  }

  int ExtractCallCount();
};


class LoadICNexus : public FeedbackNexus {
 public:
  LoadICNexus(Handle<TypeFeedbackVector> vector, FeedbackVectorSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::LOAD_IC, vector->GetKind(slot));
  }
  explicit LoadICNexus(Isolate* isolate)
      : FeedbackNexus(
            TypeFeedbackVector::DummyVector(isolate),
            FeedbackVectorSlot(TypeFeedbackVector::kDummyLoadICSlot)) {}
  LoadICNexus(TypeFeedbackVector* vector, FeedbackVectorSlot slot)
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
  KeyedLoadICNexus(Handle<TypeFeedbackVector> vector, FeedbackVectorSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::KEYED_LOAD_IC, vector->GetKind(slot));
  }
  KeyedLoadICNexus(TypeFeedbackVector* vector, FeedbackVectorSlot slot)
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

  void ConfigureMegamorphicKeyed(IcCheckType property_type);

  IcCheckType GetKeyType() const;
  InlineCacheState StateFromFeedback() const override;
  Name* FindFirstName() const override;
};


class StoreICNexus : public FeedbackNexus {
 public:
  StoreICNexus(Handle<TypeFeedbackVector> vector, FeedbackVectorSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::STORE_IC, vector->GetKind(slot));
  }
  explicit StoreICNexus(Isolate* isolate)
      : FeedbackNexus(
            TypeFeedbackVector::DummyVector(isolate),
            FeedbackVectorSlot(TypeFeedbackVector::kDummyStoreICSlot)) {}
  StoreICNexus(TypeFeedbackVector* vector, FeedbackVectorSlot slot)
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
  KeyedStoreICNexus(Handle<TypeFeedbackVector> vector, FeedbackVectorSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackVectorSlotKind::KEYED_STORE_IC, vector->GetKind(slot));
  }
  explicit KeyedStoreICNexus(Isolate* isolate)
      : FeedbackNexus(
            TypeFeedbackVector::DummyVector(isolate),
            FeedbackVectorSlot(TypeFeedbackVector::kDummyKeyedStoreICSlot)) {}
  KeyedStoreICNexus(TypeFeedbackVector* vector, FeedbackVectorSlot slot)
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
  void ConfigureMegamorphicKeyed(IcCheckType property_type);

  KeyedAccessStoreMode GetKeyedAccessStoreMode() const;
  IcCheckType GetKeyType() const;

  InlineCacheState StateFromFeedback() const override;
  Name* FindFirstName() const override;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_TRANSITIONS_H_
