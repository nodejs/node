// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FEEDBACK_VECTOR_H_
#define V8_FEEDBACK_VECTOR_H_

#include <vector>

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/elements-kind.h"
#include "src/objects/map.h"
#include "src/objects/name.h"
#include "src/objects/object-macros.h"
#include "src/type-hints.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

enum class FeedbackSlotKind {
  // This kind means that the slot points to the middle of other slot
  // which occupies more than one feedback vector element.
  // There must be no such slots in the system.
  kInvalid,

  // Sloppy kinds come first, for easy language mode testing.
  kStoreGlobalSloppy,
  kStoreNamedSloppy,
  kStoreKeyedSloppy,
  kLastSloppyKind = kStoreKeyedSloppy,

  // Strict and language mode unaware kinds.
  kCall,
  kLoadProperty,
  kLoadGlobalNotInsideTypeof,
  kLoadGlobalInsideTypeof,
  kLoadKeyed,
  kStoreGlobalStrict,
  kStoreNamedStrict,
  kStoreOwnNamed,
  kStoreKeyedStrict,
  kBinaryOp,
  kCompareOp,
  kStoreDataPropertyInLiteral,
  kTypeProfile,
  kCreateClosure,
  kLiteral,
  kForIn,
  kInstanceOf,

  kKindsNumber  // Last value indicating number of kinds.
};

inline bool IsCallICKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kCall;
}

inline bool IsLoadICKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kLoadProperty;
}

inline bool IsLoadGlobalICKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kLoadGlobalNotInsideTypeof ||
         kind == FeedbackSlotKind::kLoadGlobalInsideTypeof;
}

inline bool IsKeyedLoadICKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kLoadKeyed;
}

inline bool IsStoreGlobalICKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kStoreGlobalSloppy ||
         kind == FeedbackSlotKind::kStoreGlobalStrict;
}

inline bool IsStoreICKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kStoreNamedSloppy ||
         kind == FeedbackSlotKind::kStoreNamedStrict;
}

inline bool IsStoreOwnICKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kStoreOwnNamed;
}

inline bool IsKeyedStoreICKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kStoreKeyedSloppy ||
         kind == FeedbackSlotKind::kStoreKeyedStrict;
}

inline bool IsTypeProfileKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kTypeProfile;
}

inline TypeofMode GetTypeofModeFromSlotKind(FeedbackSlotKind kind) {
  DCHECK(IsLoadGlobalICKind(kind));
  return (kind == FeedbackSlotKind::kLoadGlobalInsideTypeof)
             ? INSIDE_TYPEOF
             : NOT_INSIDE_TYPEOF;
}

inline LanguageMode GetLanguageModeFromSlotKind(FeedbackSlotKind kind) {
  DCHECK(IsStoreICKind(kind) || IsStoreOwnICKind(kind) ||
         IsStoreGlobalICKind(kind) || IsKeyedStoreICKind(kind));
  STATIC_ASSERT(FeedbackSlotKind::kStoreGlobalSloppy <=
                FeedbackSlotKind::kLastSloppyKind);
  STATIC_ASSERT(FeedbackSlotKind::kStoreKeyedSloppy <=
                FeedbackSlotKind::kLastSloppyKind);
  STATIC_ASSERT(FeedbackSlotKind::kStoreNamedSloppy <=
                FeedbackSlotKind::kLastSloppyKind);
  return (kind <= FeedbackSlotKind::kLastSloppyKind) ? LanguageMode::kSloppy
                                                     : LanguageMode::kStrict;
}

std::ostream& operator<<(std::ostream& os, FeedbackSlotKind kind);

typedef std::vector<Handle<Object>> ObjectHandles;

class FeedbackMetadata;

// A FeedbackVector has a fixed header with:
//  - shared function info (which includes feedback metadata)
//  - invocation count
//  - runtime profiler ticks
//  - optimized code cell (weak cell or Smi marker)
// followed by an array of feedback slots, of length determined by the feedback
// metadata.
class FeedbackVector : public HeapObject {
 public:
  // Casting.
  static inline FeedbackVector* cast(Object* obj);

  inline void ComputeCounts(int* with_type_info, int* generic,
                            int* vector_ic_count);

  inline bool is_empty() const;

  inline FeedbackMetadata* metadata() const;

  // [shared_function_info]: The shared function info for the function with this
  // feedback vector.
  DECL_ACCESSORS(shared_function_info, SharedFunctionInfo)

  // [optimized_code_cell]: WeakCell containing optimized code or a Smi marker
  // defining optimization behaviour.
  DECL_ACCESSORS(optimized_code_cell, Object)

  // [length]: The length of the feedback vector (not including the header, i.e.
  // the number of feedback slots).
  DECL_INT32_ACCESSORS(length)

  // [invocation_count]: The number of times this function has been invoked.
  DECL_INT32_ACCESSORS(invocation_count)

  // [invocation_count]: The number of times this function has been seen by the
  // runtime profiler.
  DECL_INT32_ACCESSORS(profiler_ticks)

  // [deopt_count]: The number of times this function has deoptimized.
  DECL_INT32_ACCESSORS(deopt_count)

  inline void clear_invocation_count();
  inline void increment_deopt_count();

  inline Code* optimized_code() const;
  inline OptimizationMarker optimization_marker() const;
  inline bool has_optimized_code() const;
  inline bool has_optimization_marker() const;
  void ClearOptimizedCode();
  void EvictOptimizedCodeMarkedForDeoptimization(SharedFunctionInfo* shared,
                                                 const char* reason);
  static void SetOptimizedCode(Handle<FeedbackVector> vector,
                               Handle<Code> code);
  void SetOptimizationMarker(OptimizationMarker marker);

  // Conversion from a slot to an integer index to the underlying array.
  static int GetIndex(FeedbackSlot slot) { return slot.ToInt(); }

  // Conversion from an integer index to the underlying array to a slot.
  static inline FeedbackSlot ToSlot(int index);
  inline Object* Get(FeedbackSlot slot) const;
  inline Object* get(int index) const;
  inline void Set(FeedbackSlot slot, Object* value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void set(int index, Object* value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Gives access to raw memory which stores the array's data.
  inline Object** slots_start();

  // Returns slot kind for given slot.
  FeedbackSlotKind GetKind(FeedbackSlot slot) const;

  FeedbackSlot GetTypeProfileSlot() const;

  static Handle<FeedbackVector> New(Isolate* isolate,
                                    Handle<SharedFunctionInfo> shared);

  static Handle<FeedbackVector> Copy(Isolate* isolate,
                                     Handle<FeedbackVector> vector);

#define DEFINE_SLOT_KIND_PREDICATE(Name) \
  bool Name(FeedbackSlot slot) const { return Name##Kind(GetKind(slot)); }

  DEFINE_SLOT_KIND_PREDICATE(IsCallIC)
  DEFINE_SLOT_KIND_PREDICATE(IsLoadIC)
  DEFINE_SLOT_KIND_PREDICATE(IsLoadGlobalIC)
  DEFINE_SLOT_KIND_PREDICATE(IsKeyedLoadIC)
  DEFINE_SLOT_KIND_PREDICATE(IsStoreIC)
  DEFINE_SLOT_KIND_PREDICATE(IsStoreOwnIC)
  DEFINE_SLOT_KIND_PREDICATE(IsStoreGlobalIC)
  DEFINE_SLOT_KIND_PREDICATE(IsKeyedStoreIC)
  DEFINE_SLOT_KIND_PREDICATE(IsTypeProfile)
#undef DEFINE_SLOT_KIND_PREDICATE

  // Returns typeof mode encoded into kind of given slot.
  inline TypeofMode GetTypeofMode(FeedbackSlot slot) const {
    return GetTypeofModeFromSlotKind(GetKind(slot));
  }

  // Returns language mode encoded into kind of given slot.
  inline LanguageMode GetLanguageMode(FeedbackSlot slot) const {
    return GetLanguageModeFromSlotKind(GetKind(slot));
  }

#ifdef OBJECT_PRINT
  // For gdb debugging.
  void Print();
#endif  // OBJECT_PRINT

  DECL_PRINTER(FeedbackVector)
  DECL_VERIFIER(FeedbackVector)

  void FeedbackSlotPrint(std::ostream& os, FeedbackSlot slot);  // NOLINT

  // Clears the vector slots. Return true if feedback has changed.
  bool ClearSlots(Isolate* isolate);

  // The object that indicates an uninitialized cache.
  static inline Handle<Symbol> UninitializedSentinel(Isolate* isolate);

  // The object that indicates a generic state.
  static inline Handle<Symbol> GenericSentinel(Isolate* isolate);

  // The object that indicates a megamorphic state.
  static inline Handle<Symbol> MegamorphicSentinel(Isolate* isolate);

  // The object that indicates a premonomorphic state.
  static inline Handle<Symbol> PremonomorphicSentinel(Isolate* isolate);

  // A raw version of the uninitialized sentinel that's safe to read during
  // garbage collection (e.g., for patching the cache).
  static inline Symbol* RawUninitializedSentinel(Isolate* isolate);

// Layout description.
#define FEEDBACK_VECTOR_FIELDS(V)            \
  /* Header fields. */                       \
  V(kSharedFunctionInfoOffset, kPointerSize) \
  V(kOptimizedCodeOffset, kPointerSize)      \
  V(kLengthOffset, kInt32Size)               \
  V(kInvocationCountOffset, kInt32Size)      \
  V(kProfilerTicksOffset, kInt32Size)        \
  V(kDeoptCountOffset, kInt32Size)           \
  V(kUnalignedHeaderSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, FEEDBACK_VECTOR_FIELDS)
#undef FEEDBACK_VECTOR_FIELDS

  static const int kHeaderSize =
      RoundUp<kPointerAlignment>(kUnalignedHeaderSize);
  static const int kFeedbackSlotsOffset = kHeaderSize;

  class BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

  // Garbage collection support.
  static constexpr int SizeFor(int length) {
    return kFeedbackSlotsOffset + length * kPointerSize;
  }

 private:
  static void AddToVectorsForProfilingTools(Isolate* isolate,
                                            Handle<FeedbackVector> vector);

  void FeedbackSlotPrint(std::ostream& os, FeedbackSlot slot,
                         FeedbackSlotKind kind);  // NOLINT

  DISALLOW_IMPLICIT_CONSTRUCTORS(FeedbackVector);
};

template <typename Derived>
class V8_EXPORT_PRIVATE FeedbackVectorSpecBase {
 public:
  FeedbackSlot AddCallICSlot() { return AddSlot(FeedbackSlotKind::kCall); }

  FeedbackSlot AddLoadICSlot() {
    return AddSlot(FeedbackSlotKind::kLoadProperty);
  }

  FeedbackSlot AddLoadGlobalICSlot(TypeofMode typeof_mode) {
    return AddSlot(typeof_mode == INSIDE_TYPEOF
                       ? FeedbackSlotKind::kLoadGlobalInsideTypeof
                       : FeedbackSlotKind::kLoadGlobalNotInsideTypeof);
  }

  FeedbackSlot AddCreateClosureSlot() {
    return AddSlot(FeedbackSlotKind::kCreateClosure);
  }

  FeedbackSlot AddKeyedLoadICSlot() {
    return AddSlot(FeedbackSlotKind::kLoadKeyed);
  }

  FeedbackSlot AddStoreICSlot(LanguageMode language_mode) {
    STATIC_ASSERT(LanguageModeSize == 2);
    return AddSlot(is_strict(language_mode)
                       ? FeedbackSlotKind::kStoreNamedStrict
                       : FeedbackSlotKind::kStoreNamedSloppy);
  }

  FeedbackSlot AddStoreOwnICSlot() {
    return AddSlot(FeedbackSlotKind::kStoreOwnNamed);
  }

  FeedbackSlot AddStoreGlobalICSlot(LanguageMode language_mode) {
    STATIC_ASSERT(LanguageModeSize == 2);
    return AddSlot(is_strict(language_mode)
                       ? FeedbackSlotKind::kStoreGlobalStrict
                       : FeedbackSlotKind::kStoreGlobalSloppy);
  }

  FeedbackSlot AddKeyedStoreICSlot(LanguageMode language_mode) {
    STATIC_ASSERT(LanguageModeSize == 2);
    return AddSlot(is_strict(language_mode)
                       ? FeedbackSlotKind::kStoreKeyedStrict
                       : FeedbackSlotKind::kStoreKeyedSloppy);
  }

  FeedbackSlot AddBinaryOpICSlot() {
    return AddSlot(FeedbackSlotKind::kBinaryOp);
  }

  FeedbackSlot AddCompareICSlot() {
    return AddSlot(FeedbackSlotKind::kCompareOp);
  }

  FeedbackSlot AddForInSlot() { return AddSlot(FeedbackSlotKind::kForIn); }

  FeedbackSlot AddInstanceOfSlot() {
    return AddSlot(FeedbackSlotKind::kInstanceOf);
  }

  FeedbackSlot AddLiteralSlot() { return AddSlot(FeedbackSlotKind::kLiteral); }

  FeedbackSlot AddStoreDataPropertyInLiteralICSlot() {
    return AddSlot(FeedbackSlotKind::kStoreDataPropertyInLiteral);
  }

  FeedbackSlot AddTypeProfileSlot();

#ifdef OBJECT_PRINT
  // For gdb debugging.
  void Print();
#endif  // OBJECT_PRINT

  DECL_PRINTER(FeedbackVectorSpec)

 private:
  FeedbackSlot AddSlot(FeedbackSlotKind kind);

  Derived* This() { return static_cast<Derived*>(this); }
};

class StaticFeedbackVectorSpec
    : public FeedbackVectorSpecBase<StaticFeedbackVectorSpec> {
 public:
  StaticFeedbackVectorSpec() : slot_count_(0) {}

  int slots() const { return slot_count_; }

  FeedbackSlotKind GetKind(FeedbackSlot slot) const {
    DCHECK(slot.ToInt() >= 0 && slot.ToInt() < slot_count_);
    return kinds_[slot.ToInt()];
  }

 private:
  friend class FeedbackVectorSpecBase<StaticFeedbackVectorSpec>;

  void append(FeedbackSlotKind kind) {
    DCHECK_LT(slot_count_, kMaxLength);
    kinds_[slot_count_++] = kind;
  }

  static const int kMaxLength = 12;

  int slot_count_;
  FeedbackSlotKind kinds_[kMaxLength];
};

class V8_EXPORT_PRIVATE FeedbackVectorSpec
    : public FeedbackVectorSpecBase<FeedbackVectorSpec> {
 public:
  explicit FeedbackVectorSpec(Zone* zone) : slot_kinds_(zone) {
    slot_kinds_.reserve(16);
  }

  int slots() const { return static_cast<int>(slot_kinds_.size()); }

  FeedbackSlotKind GetKind(FeedbackSlot slot) const {
    return static_cast<FeedbackSlotKind>(slot_kinds_.at(slot.ToInt()));
  }

  bool HasTypeProfileSlot() const;

  // If used, the TypeProfileSlot is always added as the first slot and its
  // index is constant. If other slots are added before the TypeProfileSlot,
  // this number changes.
  static const int kTypeProfileSlotIndex = 0;

 private:
  friend class FeedbackVectorSpecBase<FeedbackVectorSpec>;

  void append(FeedbackSlotKind kind) {
    slot_kinds_.push_back(static_cast<unsigned char>(kind));
  }

  ZoneVector<unsigned char> slot_kinds_;
};

// The shape of the FeedbackMetadata is an array with:
// 0: slot_count
// 1: names table
// 2: parameters table
// 3..N: slot kinds packed into a bit vector
//
class FeedbackMetadata : public FixedArray {
 public:
  // Casting.
  static inline FeedbackMetadata* cast(Object* obj);

  static const int kSlotsCountIndex = 0;
  static const int kReservedIndexCount = 1;

  // Returns number of feedback vector elements used by given slot kind.
  static inline int GetSlotSize(FeedbackSlotKind kind);

  bool SpecDiffersFrom(const FeedbackVectorSpec* other_spec) const;

  inline bool is_empty() const;

  // Returns number of slots in the vector.
  inline int slot_count() const;

  // Returns slot kind for given slot.
  FeedbackSlotKind GetKind(FeedbackSlot slot) const;

  template <typename Spec>
  static Handle<FeedbackMetadata> New(Isolate* isolate, const Spec* spec);

#ifdef OBJECT_PRINT
  // For gdb debugging.
  void Print();
#endif  // OBJECT_PRINT

  DECL_PRINTER(FeedbackMetadata)

  static const char* Kind2String(FeedbackSlotKind kind);
  bool HasTypeProfileSlot() const;

 private:
  friend class AccessorAssembler;

  static const int kFeedbackSlotKindBits = 5;
  STATIC_ASSERT(static_cast<int>(FeedbackSlotKind::kKindsNumber) <
                (1 << kFeedbackSlotKindBits));

  void SetKind(FeedbackSlot slot, FeedbackSlotKind kind);

  typedef BitSetComputer<FeedbackSlotKind, kFeedbackSlotKindBits, kSmiValueSize,
                         uint32_t>
      VectorICComputer;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FeedbackMetadata);
};

// The following asserts protect an optimization in type feedback vector
// code that looks into the contents of a slot assuming to find a String,
// a Symbol, an AllocationSite, a WeakCell, or a FixedArray.
STATIC_ASSERT(WeakCell::kSize >= 2 * kPointerSize);
STATIC_ASSERT(WeakCell::kValueOffset ==
              AllocationSite::kTransitionInfoOrBoilerplateOffset);
STATIC_ASSERT(WeakCell::kValueOffset == FixedArray::kLengthOffset);
STATIC_ASSERT(WeakCell::kValueOffset == Name::kHashFieldSlot);
// Verify that an empty hash field looks like a tagged object, but can't
// possibly be confused with a pointer.
STATIC_ASSERT((Name::kEmptyHashField & kHeapObjectTag) == kHeapObjectTag);
STATIC_ASSERT(Name::kEmptyHashField == 0x3);
// Verify that a set hash field will not look like a tagged object.
STATIC_ASSERT(Name::kHashNotComputedMask == kHeapObjectTag);

class FeedbackMetadataIterator {
 public:
  explicit FeedbackMetadataIterator(Handle<FeedbackMetadata> metadata)
      : metadata_handle_(metadata),
        next_slot_(FeedbackSlot(0)),
        slot_kind_(FeedbackSlotKind::kInvalid) {}

  explicit FeedbackMetadataIterator(FeedbackMetadata* metadata)
      : metadata_(metadata),
        next_slot_(FeedbackSlot(0)),
        slot_kind_(FeedbackSlotKind::kInvalid) {}

  inline bool HasNext() const;

  inline FeedbackSlot Next();

  // Returns slot kind of the last slot returned by Next().
  FeedbackSlotKind kind() const {
    DCHECK_NE(FeedbackSlotKind::kInvalid, slot_kind_);
    DCHECK_NE(FeedbackSlotKind::kKindsNumber, slot_kind_);
    return slot_kind_;
  }

  // Returns entry size of the last slot returned by Next().
  inline int entry_size() const;

 private:
  FeedbackMetadata* metadata() const {
    return !metadata_handle_.is_null() ? *metadata_handle_ : metadata_;
  }

  // The reason for having a handle and a raw pointer to the meta data is
  // to have a single iterator implementation for both "handlified" and raw
  // pointer use cases.
  Handle<FeedbackMetadata> metadata_handle_;
  FeedbackMetadata* metadata_;
  FeedbackSlot cur_slot_;
  FeedbackSlot next_slot_;
  FeedbackSlotKind slot_kind_;
};

// A FeedbackNexus is the combination of a FeedbackVector and a slot.
// Derived classes customize the update and retrieval of feedback.
class FeedbackNexus {
 public:
  FeedbackNexus(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : vector_handle_(vector), vector_(nullptr), slot_(slot) {}
  FeedbackNexus(FeedbackVector* vector, FeedbackSlot slot)
      : vector_(vector), slot_(slot) {}
  virtual ~FeedbackNexus() {}

  Handle<FeedbackVector> vector_handle() const {
    DCHECK_NULL(vector_);
    return vector_handle_;
  }
  FeedbackVector* vector() const {
    return vector_handle_.is_null() ? vector_ : *vector_handle_;
  }
  FeedbackSlot slot() const { return slot_; }
  FeedbackSlotKind kind() const { return vector()->GetKind(slot()); }

  InlineCacheState ic_state() const { return StateFromFeedback(); }
  bool IsUninitialized() const { return StateFromFeedback() == UNINITIALIZED; }
  bool IsMegamorphic() const { return StateFromFeedback() == MEGAMORPHIC; }
  bool IsGeneric() const { return StateFromFeedback() == GENERIC; }
  Map* FindFirstMap() const {
    MapHandles maps;
    ExtractMaps(&maps);
    if (maps.size() > 0) return *maps.at(0);
    return nullptr;
  }

  virtual InlineCacheState StateFromFeedback() const = 0;
  virtual int ExtractMaps(MapHandles* maps) const;
  virtual MaybeHandle<Object> FindHandlerForMap(Handle<Map> map) const;
  virtual bool FindHandlers(ObjectHandles* code_list, int length = -1) const;
  virtual Name* FindFirstName() const { return nullptr; }

  bool IsCleared() {
    InlineCacheState state = StateFromFeedback();
    return !FLAG_use_ic || state == UNINITIALIZED || state == PREMONOMORPHIC;
  }

  virtual void Clear() { ConfigureUninitialized(); }
  virtual void ConfigureUninitialized();
  void ConfigurePremonomorphic();
  bool ConfigureMegamorphic(IcCheckType property_type);

  inline Object* GetFeedback() const;
  inline Object* GetFeedbackExtra() const;

  inline Isolate* GetIsolate() const;

  void ConfigureMonomorphic(Handle<Name> name, Handle<Map> receiver_map,
                            Handle<Object> handler);

  void ConfigurePolymorphic(Handle<Name> name, MapHandles const& maps,
                            ObjectHandles* handlers);

 protected:
  inline void SetFeedback(Object* feedback,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void SetFeedbackExtra(Object* feedback_extra,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  Handle<FixedArray> EnsureArrayOfSize(int length);
  Handle<FixedArray> EnsureExtraArrayOfSize(int length);

 private:
  // The reason for having a vector handle and a raw pointer is that we can and
  // should use handles during IC miss, but not during GC when we clear ICs. If
  // you have a handle to the vector that is better because more operations can
  // be done, like allocation.
  Handle<FeedbackVector> vector_handle_;
  FeedbackVector* vector_;
  FeedbackSlot slot_;
};

class CallICNexus final : public FeedbackNexus {
 public:
  CallICNexus(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->IsCallIC(slot));
  }
  CallICNexus(FeedbackVector* vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->IsCallIC(slot));
  }

  void ConfigureUninitialized() final;

  InlineCacheState StateFromFeedback() const final;

  int ExtractMaps(MapHandles* maps) const final {
    // CallICs don't record map feedback.
    return 0;
  }
  MaybeHandle<Object> FindHandlerForMap(Handle<Map> map) const final {
    return MaybeHandle<Code>();
  }
  bool FindHandlers(ObjectHandles* code_list, int length = -1) const final {
    return length == 0;
  }

  int ExtractCallCount();

  // Compute the call frequency based on the call count and the invocation
  // count (taken from the type feedback vector).
  float ComputeCallFrequency();
};

class LoadICNexus : public FeedbackNexus {
 public:
  LoadICNexus(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->IsLoadIC(slot));
  }
  LoadICNexus(FeedbackVector* vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->IsLoadIC(slot));
  }

  void Clear() override { ConfigurePremonomorphic(); }

  InlineCacheState StateFromFeedback() const override;
};

class LoadGlobalICNexus : public FeedbackNexus {
 public:
  LoadGlobalICNexus(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->IsLoadGlobalIC(slot));
  }
  LoadGlobalICNexus(FeedbackVector* vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->IsLoadGlobalIC(slot));
  }

  int ExtractMaps(MapHandles* maps) const final {
    // LoadGlobalICs don't record map feedback.
    return 0;
  }
  MaybeHandle<Object> FindHandlerForMap(Handle<Map> map) const final {
    return MaybeHandle<Code>();
  }
  bool FindHandlers(ObjectHandles* code_list, int length = -1) const final {
    return length == 0;
  }

  void ConfigureUninitialized() override;
  void ConfigurePropertyCellMode(Handle<PropertyCell> cell);
  void ConfigureHandlerMode(Handle<Object> handler);

  InlineCacheState StateFromFeedback() const override;
};

class KeyedLoadICNexus : public FeedbackNexus {
 public:
  KeyedLoadICNexus(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->IsKeyedLoadIC(slot));
  }
  KeyedLoadICNexus(FeedbackVector* vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->IsKeyedLoadIC(slot));
  }

  void Clear() override { ConfigurePremonomorphic(); }

  KeyedAccessLoadMode GetKeyedAccessLoadMode() const;
  IcCheckType GetKeyType() const;
  InlineCacheState StateFromFeedback() const override;
  Name* FindFirstName() const override;
};

class StoreICNexus : public FeedbackNexus {
 public:
  StoreICNexus(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->IsStoreIC(slot) || vector->IsStoreOwnIC(slot) ||
           vector->IsStoreGlobalIC(slot));
  }
  StoreICNexus(FeedbackVector* vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->IsStoreIC(slot) || vector->IsStoreOwnIC(slot) ||
           vector->IsStoreGlobalIC(slot));
  }

  void Clear() override { ConfigurePremonomorphic(); }

  InlineCacheState StateFromFeedback() const override;
};

// TODO(ishell): Currently we use StoreOwnIC only for storing properties that
// already exist in the boilerplate therefore we can use StoreIC.
typedef StoreICNexus StoreOwnICNexus;

class KeyedStoreICNexus : public FeedbackNexus {
 public:
  KeyedStoreICNexus(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->IsKeyedStoreIC(slot));
  }
  KeyedStoreICNexus(FeedbackVector* vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK(vector->IsKeyedStoreIC(slot));
  }

  void Clear() override { ConfigurePremonomorphic(); }

  KeyedAccessStoreMode GetKeyedAccessStoreMode() const;
  IcCheckType GetKeyType() const;

  InlineCacheState StateFromFeedback() const override;
  Name* FindFirstName() const override;
};

class BinaryOpICNexus final : public FeedbackNexus {
 public:
  BinaryOpICNexus(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackSlotKind::kBinaryOp, vector->GetKind(slot));
  }
  BinaryOpICNexus(FeedbackVector* vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackSlotKind::kBinaryOp, vector->GetKind(slot));
  }

  InlineCacheState StateFromFeedback() const final;
  BinaryOperationHint GetBinaryOperationFeedback() const;

  int ExtractMaps(MapHandles* maps) const final {
    // BinaryOpICs don't record map feedback.
    return 0;
  }
  MaybeHandle<Object> FindHandlerForMap(Handle<Map> map) const final {
    return MaybeHandle<Code>();
  }
  bool FindHandlers(ObjectHandles* code_list, int length = -1) const final {
    return length == 0;
  }
};

class CompareICNexus final : public FeedbackNexus {
 public:
  CompareICNexus(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackSlotKind::kCompareOp, vector->GetKind(slot));
  }
  CompareICNexus(FeedbackVector* vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackSlotKind::kCompareOp, vector->GetKind(slot));
  }

  InlineCacheState StateFromFeedback() const final;
  CompareOperationHint GetCompareOperationFeedback() const;

  int ExtractMaps(MapHandles* maps) const final {
    // CompareICs don't record map feedback.
    return 0;
  }
  MaybeHandle<Object> FindHandlerForMap(Handle<Map> map) const final {
    return MaybeHandle<Code>();
  }
  bool FindHandlers(ObjectHandles* code_list, int length = -1) const final {
    return length == 0;
  }
};

class ForInICNexus final : public FeedbackNexus {
 public:
  ForInICNexus(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackSlotKind::kForIn, vector->GetKind(slot));
  }
  ForInICNexus(FeedbackVector* vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackSlotKind::kForIn, vector->GetKind(slot));
  }

  InlineCacheState StateFromFeedback() const final;
  ForInHint GetForInFeedback() const;

  int ExtractMaps(MapHandles* maps) const final { return 0; }
  MaybeHandle<Object> FindHandlerForMap(Handle<Map> map) const final {
    return MaybeHandle<Code>();
  }
  bool FindHandlers(ObjectHandles* code_list, int length = -1) const final {
    return length == 0;
  }
};

class InstanceOfICNexus final : public FeedbackNexus {
 public:
  InstanceOfICNexus(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackSlotKind::kInstanceOf, vector->GetKind(slot));
  }
  InstanceOfICNexus(FeedbackVector* vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackSlotKind::kInstanceOf, vector->GetKind(slot));
  }

  void ConfigureUninitialized() final;

  InlineCacheState StateFromFeedback() const final;
  MaybeHandle<JSObject> GetConstructorFeedback() const;

  int ExtractMaps(MapHandles* maps) const final { return 0; }
  MaybeHandle<Object> FindHandlerForMap(Handle<Map> map) const final {
    return MaybeHandle<Code>();
  }
  bool FindHandlers(ObjectHandles* code_list, int length = -1) const final {
    return length == 0;
  }
};

class StoreDataPropertyInLiteralICNexus : public FeedbackNexus {
 public:
  StoreDataPropertyInLiteralICNexus(Handle<FeedbackVector> vector,
                                    FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackSlotKind::kStoreDataPropertyInLiteral,
              vector->GetKind(slot));
  }
  StoreDataPropertyInLiteralICNexus(FeedbackVector* vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackSlotKind::kStoreDataPropertyInLiteral,
              vector->GetKind(slot));
  }

  void ConfigureMonomorphic(Handle<Name> name, Handle<Map> receiver_map);

  InlineCacheState StateFromFeedback() const override;
};

// For each assignment, store the type of the value in the collection of types
// in the feedback vector.
class CollectTypeProfileNexus : public FeedbackNexus {
 public:
  CollectTypeProfileNexus(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackSlotKind::kTypeProfile, vector->GetKind(slot));
  }
  CollectTypeProfileNexus(FeedbackVector* vector, FeedbackSlot slot)
      : FeedbackNexus(vector, slot) {
    DCHECK_EQ(FeedbackSlotKind::kTypeProfile, vector->GetKind(slot));
  }

  // Add a type to the list of types for source position <position>.
  void Collect(Handle<String> type, int position);
  JSObject* GetTypeProfile() const;

  std::vector<int> GetSourcePositions() const;
  std::vector<Handle<String>> GetTypesForSourcePositions(uint32_t pos) const;

  void Clear() override;

  InlineCacheState StateFromFeedback() const override;
};

inline BinaryOperationHint BinaryOperationHintFromFeedback(int type_feedback);
inline CompareOperationHint CompareOperationHintFromFeedback(int type_feedback);
inline ForInHint ForInHintFromFeedback(int type_feedback);

}  // namespace internal
}  // namespace v8

#endif  // V8_FEEDBACK_VECTOR_H_
