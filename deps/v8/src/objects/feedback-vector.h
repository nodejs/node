// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FEEDBACK_VECTOR_H_
#define V8_OBJECTS_FEEDBACK_VECTOR_H_

#include <vector>

#include "src/base/bit-field.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/objects/elements-kind.h"
#include "src/objects/map.h"
#include "src/objects/maybe-object.h"
#include "src/objects/name.h"
#include "src/objects/type-hints.h"
#include "src/zone/zone-containers.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class IsCompiledScope;

enum class UpdateFeedbackMode { kOptionalFeedback, kGuaranteedFeedback };

enum class FeedbackSlotKind : uint8_t {
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
  kHasKeyed,
  kStoreGlobalStrict,
  kStoreNamedStrict,
  kStoreOwnNamed,
  kStoreKeyedStrict,
  kStoreInArrayLiteral,
  kBinaryOp,
  kCompareOp,
  kStoreDataPropertyInLiteral,
  kTypeProfile,
  kLiteral,
  kForIn,
  kInstanceOf,
  kCloneObject,

  kKindsNumber  // Last value indicating number of kinds.
};

using MapAndHandler = std::pair<Handle<Map>, MaybeObjectHandle>;
using MapAndFeedback = std::pair<Handle<Map>, MaybeObjectHandle>;

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

inline bool IsKeyedHasICKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kHasKeyed;
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

inline bool IsStoreDataPropertyInLiteralKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kStoreDataPropertyInLiteral;
}

inline bool IsKeyedStoreICKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kStoreKeyedSloppy ||
         kind == FeedbackSlotKind::kStoreKeyedStrict;
}

inline bool IsStoreInArrayLiteralICKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kStoreInArrayLiteral;
}

inline bool IsGlobalICKind(FeedbackSlotKind kind) {
  return IsLoadGlobalICKind(kind) || IsStoreGlobalICKind(kind);
}

inline bool IsTypeProfileKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kTypeProfile;
}

inline bool IsCloneObjectKind(FeedbackSlotKind kind) {
  return kind == FeedbackSlotKind::kCloneObject;
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

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           FeedbackSlotKind kind);

using MaybeObjectHandles = std::vector<MaybeObjectHandle>;

class FeedbackMetadata;

#include "torque-generated/src/objects/feedback-vector-tq.inc"

// ClosureFeedbackCellArray is a FixedArray that contains feedback cells used
// when creating closures from a function. This is created once the function is
// compiled and is either held by the feedback vector (if allocated) or by the
// FeedbackCell of the closure.
class ClosureFeedbackCellArray : public FixedArray {
 public:
  NEVER_READ_ONLY_SPACE

  DECL_CAST(ClosureFeedbackCellArray)

  V8_EXPORT_PRIVATE static Handle<ClosureFeedbackCellArray> New(
      Isolate* isolate, Handle<SharedFunctionInfo> shared);

  inline Handle<FeedbackCell> GetFeedbackCell(int index);
  inline FeedbackCell cell(int index);

  DECL_VERIFIER(ClosureFeedbackCellArray)
  DECL_PRINTER(ClosureFeedbackCellArray)

 private:
  OBJECT_CONSTRUCTORS(ClosureFeedbackCellArray, FixedArray);
};

class NexusConfig;

// A FeedbackVector has a fixed header with:
//  - shared function info (which includes feedback metadata)
//  - invocation count
//  - runtime profiler ticks
//  - optimized code cell (weak cell or Smi marker)
// followed by an array of feedback slots, of length determined by the feedback
// metadata.
class FeedbackVector
    : public TorqueGeneratedFeedbackVector<FeedbackVector, HeapObject> {
 public:
  NEVER_READ_ONLY_SPACE
  DEFINE_TORQUE_GENERATED_FEEDBACK_VECTOR_FLAGS()
  STATIC_ASSERT(OptimizationMarker::kLastOptimizationMarker <
                OptimizationMarkerBits::kMax);
  STATIC_ASSERT(OptimizationTier::kLastOptimizationTier <
                OptimizationTierBits::kMax);

  static const bool kFeedbackVectorMaybeOptimizedCodeIsStoreRelease = true;
  using TorqueGeneratedFeedbackVector<FeedbackVector,
                                      HeapObject>::maybe_optimized_code;
  DECL_RELEASE_ACQUIRE_WEAK_ACCESSORS(maybe_optimized_code)

  static constexpr uint32_t kHasCompileOptimizedOrLogFirstExecutionMarker =
      kNoneOrInOptimizationQueueMask << OptimizationMarkerBits::kShift;
  static constexpr uint32_t kHasNoTopTierCodeOrCompileOptimizedMarkerMask =
      kNoneOrMidTierMask << OptimizationTierBits::kShift |
      kHasCompileOptimizedOrLogFirstExecutionMarker;
  static constexpr uint32_t kHasOptimizedCodeOrCompileOptimizedMarkerMask =
      OptimizationTierBits::kMask |
      kHasCompileOptimizedOrLogFirstExecutionMarker;

  inline bool is_empty() const;

  inline FeedbackMetadata metadata() const;

  // Increment profiler ticks, saturating at the maximal value.
  void SaturatingIncrementProfilerTicks();

  inline void clear_invocation_count();

  inline Code optimized_code() const;
  inline bool has_optimized_code() const;
  inline bool has_optimization_marker() const;
  inline OptimizationMarker optimization_marker() const;
  inline OptimizationTier optimization_tier() const;
  inline int global_ticks_at_last_runtime_profiler_interrupt() const;
  inline void set_global_ticks_at_last_runtime_profiler_interrupt(int ticks);
  void ClearOptimizedCode(FeedbackCell feedback_cell);
  void EvictOptimizedCodeMarkedForDeoptimization(FeedbackCell feedback_cell,
                                                 SharedFunctionInfo shared,
                                                 const char* reason);
  static void SetOptimizedCode(Handle<FeedbackVector> vector, Handle<Code> code,
                               FeedbackCell feedback_cell);
  void SetOptimizationMarker(OptimizationMarker marker);
  void ClearOptimizationTier(FeedbackCell feedback_cell);
  void InitializeOptimizationState();

  // Clears the optimization marker in the feedback vector.
  void ClearOptimizationMarker();

  // Sets the interrupt budget based on the optimized code available on the
  // feedback vector. This function expects that the feedback cell contains a
  // feedback vector.
  static void SetInterruptBudget(FeedbackCell feedback_cell);

  // Conversion from a slot to an integer index to the underlying array.
  static int GetIndex(FeedbackSlot slot) { return slot.ToInt(); }

  // Conversion from an integer index to the underlying array to a slot.
  static inline FeedbackSlot ToSlot(intptr_t index);

  inline MaybeObject SynchronizedGet(FeedbackSlot slot) const;
  inline void SynchronizedSet(FeedbackSlot slot, MaybeObject value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void SynchronizedSet(FeedbackSlot slot, Object value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline MaybeObject Get(FeedbackSlot slot) const;
  inline MaybeObject Get(PtrComprCageBase cage_base, FeedbackSlot slot) const;

  // Returns the feedback cell at |index| that is used to create the
  // closure.
  inline Handle<FeedbackCell> GetClosureFeedbackCell(int index) const;
  inline FeedbackCell closure_feedback_cell(int index) const;

  // Gives access to raw memory which stores the array's data.
  inline MaybeObjectSlot slots_start();

  // Returns slot kind for given slot.
  V8_EXPORT_PRIVATE FeedbackSlotKind GetKind(FeedbackSlot slot) const;

  FeedbackSlot GetTypeProfileSlot() const;

  V8_EXPORT_PRIVATE static Handle<FeedbackVector> New(
      Isolate* isolate, Handle<SharedFunctionInfo> shared,
      Handle<ClosureFeedbackCellArray> closure_feedback_cell_array,
      IsCompiledScope* is_compiled_scope);

  V8_EXPORT_PRIVATE static Handle<FeedbackVector>
  NewWithOneBinarySlotForTesting(Zone* zone, Isolate* isolate);
  V8_EXPORT_PRIVATE static Handle<FeedbackVector>
  NewWithOneCompareSlotForTesting(Zone* zone, Isolate* isolate);

#define DEFINE_SLOT_KIND_PREDICATE(Name) \
  bool Name(FeedbackSlot slot) const { return Name##Kind(GetKind(slot)); }

  DEFINE_SLOT_KIND_PREDICATE(IsCallIC)
  DEFINE_SLOT_KIND_PREDICATE(IsGlobalIC)
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

  DECL_PRINTER(FeedbackVector)

  void FeedbackSlotPrint(std::ostream& os, FeedbackSlot slot);  // NOLINT

  // Clears the vector slots. Return true if feedback has changed.
  bool ClearSlots(Isolate* isolate);

  // The object that indicates an uninitialized cache.
  static inline Handle<Symbol> UninitializedSentinel(Isolate* isolate);

  // The object that indicates a megamorphic state.
  static inline Handle<Symbol> MegamorphicSentinel(Isolate* isolate);

  // A raw version of the uninitialized sentinel that's safe to read during
  // garbage collection (e.g., for patching the cache).
  static inline Symbol RawUninitializedSentinel(Isolate* isolate);

  static_assert(kHeaderSize % kObjectAlignment == 0,
                "Header must be padded for alignment");

  class BodyDescriptor;

  static constexpr int OffsetOfElementAt(int index) {
    return kRawFeedbackSlotsOffset + index * kTaggedSize;
  }

  TQ_OBJECT_CONSTRUCTORS(FeedbackVector)

 private:
  static void AddToVectorsForProfilingTools(Isolate* isolate,
                                            Handle<FeedbackVector> vector);

  // Private for initializing stores in FeedbackVector::New().
  inline void Set(FeedbackSlot slot, MaybeObject value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void Set(FeedbackSlot slot, Object value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

#ifdef DEBUG
  // Returns true if value is a non-HashTable FixedArray. We want to
  // make sure not to store such objects in the vector.
  inline static bool IsOfLegacyType(MaybeObject value);
#endif  // DEBUG

  // NexusConfig controls setting slots in the vector.
  friend NexusConfig;

  // Don't expose the raw feedback slot getter/setter.
  using TorqueGeneratedFeedbackVector::raw_feedback_slots;
};

class V8_EXPORT_PRIVATE FeedbackVectorSpec {
 public:
  explicit FeedbackVectorSpec(Zone* zone) : slot_kinds_(zone) {
    slot_kinds_.reserve(16);
  }

  int slot_count() const { return static_cast<int>(slot_kinds_.size()); }
  int create_closure_slot_count() const { return create_closure_slot_count_; }

  int AddCreateClosureSlot() { return create_closure_slot_count_++; }

  FeedbackSlotKind GetKind(FeedbackSlot slot) const {
    return slot_kinds_.at(slot.ToInt());
  }

  bool HasTypeProfileSlot() const;

  // If used, the TypeProfileSlot is always added as the first slot and its
  // index is constant. If other slots are added before the TypeProfileSlot,
  // this number changes.
  static const int kTypeProfileSlotIndex = 0;

  FeedbackSlot AddCallICSlot() { return AddSlot(FeedbackSlotKind::kCall); }

  FeedbackSlot AddLoadICSlot() {
    return AddSlot(FeedbackSlotKind::kLoadProperty);
  }

  FeedbackSlot AddLoadGlobalICSlot(TypeofMode typeof_mode) {
    return AddSlot(typeof_mode == INSIDE_TYPEOF
                       ? FeedbackSlotKind::kLoadGlobalInsideTypeof
                       : FeedbackSlotKind::kLoadGlobalNotInsideTypeof);
  }

  FeedbackSlot AddKeyedLoadICSlot() {
    return AddSlot(FeedbackSlotKind::kLoadKeyed);
  }

  FeedbackSlot AddKeyedHasICSlot() {
    return AddSlot(FeedbackSlotKind::kHasKeyed);
  }

  FeedbackSlotKind GetStoreICSlot(LanguageMode language_mode) {
    STATIC_ASSERT(LanguageModeSize == 2);
    return is_strict(language_mode) ? FeedbackSlotKind::kStoreNamedStrict
                                    : FeedbackSlotKind::kStoreNamedSloppy;
  }

  FeedbackSlot AddStoreICSlot(LanguageMode language_mode) {
    return AddSlot(GetStoreICSlot(language_mode));
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

  FeedbackSlotKind GetKeyedStoreICSlotKind(LanguageMode language_mode) {
    STATIC_ASSERT(LanguageModeSize == 2);
    return is_strict(language_mode) ? FeedbackSlotKind::kStoreKeyedStrict
                                    : FeedbackSlotKind::kStoreKeyedSloppy;
  }

  FeedbackSlot AddKeyedStoreICSlot(LanguageMode language_mode) {
    return AddSlot(GetKeyedStoreICSlotKind(language_mode));
  }

  FeedbackSlot AddStoreInArrayLiteralICSlot() {
    return AddSlot(FeedbackSlotKind::kStoreInArrayLiteral);
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

  FeedbackSlot AddCloneObjectSlot() {
    return AddSlot(FeedbackSlotKind::kCloneObject);
  }

#ifdef OBJECT_PRINT
  // For gdb debugging.
  void Print();
#endif  // OBJECT_PRINT

  DECL_PRINTER(FeedbackVectorSpec)

 private:
  FeedbackSlot AddSlot(FeedbackSlotKind kind);

  void append(FeedbackSlotKind kind) { slot_kinds_.push_back(kind); }

  STATIC_ASSERT(sizeof(FeedbackSlotKind) == sizeof(uint8_t));
  ZoneVector<FeedbackSlotKind> slot_kinds_;
  int create_closure_slot_count_ = 0;

  friend class SharedFeedbackSlot;
};

// Helper class that creates a feedback slot on-demand.
class SharedFeedbackSlot {
 public:
  // FeedbackSlot default constructor constructs an invalid slot.
  SharedFeedbackSlot(FeedbackVectorSpec* spec, FeedbackSlotKind kind)
      : kind_(kind), spec_(spec) {}

  FeedbackSlot Get() {
    if (slot_.IsInvalid()) slot_ = spec_->AddSlot(kind_);
    return slot_;
  }

 private:
  FeedbackSlotKind kind_;
  FeedbackSlot slot_;
  FeedbackVectorSpec* spec_;
};

// FeedbackMetadata is an array-like object with a slot count (indicating how
// many slots are stored). We save space by packing several slots into an array
// of int32 data. The length is never stored - it is always calculated from
// slot_count. All instances are created through the static New function, and
// the number of slots is static once an instance is created.
class FeedbackMetadata : public HeapObject {
 public:
  DECL_CAST(FeedbackMetadata)

  // The number of slots that this metadata contains. Stored as an int32.
  DECL_INT32_ACCESSORS(slot_count)

  // The number of feedback cells required for create closures. Stored as an
  // int32.
  // TODO(mythria): Consider using 16 bits for this and slot_count so that we
  // can save 4 bytes.
  DECL_INT32_ACCESSORS(create_closure_slot_count)

  // Get slot_count using an acquire load.
  inline int32_t synchronized_slot_count() const;

  // Returns number of feedback vector elements used by given slot kind.
  static inline int GetSlotSize(FeedbackSlotKind kind);

  bool SpecDiffersFrom(const FeedbackVectorSpec* other_spec) const;

  inline bool is_empty() const;

  // Returns slot kind for given slot.
  V8_EXPORT_PRIVATE FeedbackSlotKind GetKind(FeedbackSlot slot) const;

  // If {spec} is null, then it is considered empty.
  template <typename LocalIsolate>
  V8_EXPORT_PRIVATE static Handle<FeedbackMetadata> New(
      LocalIsolate* isolate, const FeedbackVectorSpec* spec = nullptr);

  DECL_PRINTER(FeedbackMetadata)
  DECL_VERIFIER(FeedbackMetadata)

  static const char* Kind2String(FeedbackSlotKind kind);
  bool HasTypeProfileSlot() const;

  // Garbage collection support.
  // This includes any necessary padding at the end of the object for pointer
  // size alignment.
  static int SizeFor(int slot_count) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length(slot_count) * kInt32Size);
  }

#define FIELDS(V)                              \
  V(kSlotCountOffset, kInt32Size)              \
  V(kCreateClosureSlotCountOffset, kInt32Size) \
  V(kHeaderSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, FIELDS)
#undef FIELDS

  class BodyDescriptor;

 private:
  friend class AccessorAssembler;

  // Raw accessors to the encoded slot data.
  inline int32_t get(int index) const;
  inline void set(int index, int32_t value);

  // The number of int32 data fields needed to store {slot_count} slots.
  // Does not include any extra padding for pointer size alignment.
  static int length(int slot_count) {
    return VectorICComputer::word_count(slot_count);
  }
  inline int length() const;

  static const int kFeedbackSlotKindBits = 5;
  STATIC_ASSERT(static_cast<int>(FeedbackSlotKind::kKindsNumber) <
                (1 << kFeedbackSlotKindBits));

  void SetKind(FeedbackSlot slot, FeedbackSlotKind kind);

  using VectorICComputer =
      base::BitSetComputer<FeedbackSlotKind, kFeedbackSlotKindBits,
                           kInt32Size * kBitsPerByte, uint32_t>;

  OBJECT_CONSTRUCTORS(FeedbackMetadata, HeapObject);
};

// Verify that an empty hash field looks like a tagged object, but can't
// possibly be confused with a pointer.
// NOLINTNEXTLINE(runtime/references) (false positive)
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

  explicit FeedbackMetadataIterator(FeedbackMetadata metadata)
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
  FeedbackMetadata metadata() const {
    return !metadata_handle_.is_null() ? *metadata_handle_ : metadata_;
  }

  // The reason for having a handle and a raw pointer to the meta data is
  // to have a single iterator implementation for both "handlified" and raw
  // pointer use cases.
  Handle<FeedbackMetadata> metadata_handle_;
  FeedbackMetadata metadata_;
  FeedbackSlot cur_slot_;
  FeedbackSlot next_slot_;
  FeedbackSlotKind slot_kind_;
};

// NexusConfig adapts the FeedbackNexus to be used on the main thread
// or a background thread. It controls the actual read and writes of
// the underlying feedback vector, manages the creation of handles, and
// expresses capabilities available in the very different contexts of
// main and background thread. Here are the differences:
//
// Capability:      MainThread           BackgroundThread
// Write to vector  Allowed              Not allowed
// Handle creation  Via Isolate          Via LocalHeap
// Reads of vector  "Live"               Cached after initial read
// Thread safety    Exclusive write,     Shared read only
//                  shared read
class V8_EXPORT_PRIVATE NexusConfig {
 public:
  static NexusConfig FromMainThread(Isolate* isolate) {
    return NexusConfig(isolate);
  }

  static NexusConfig FromBackgroundThread(Isolate* isolate,
                                          LocalHeap* local_heap) {
    return NexusConfig(isolate, local_heap);
  }

  enum Mode { MainThread, BackgroundThread };

  Mode mode() const {
    return local_heap_ == nullptr ? MainThread : BackgroundThread;
  }

  Isolate* isolate() const { return isolate_; }

  MaybeObjectHandle NewHandle(MaybeObject object) const;
  template <typename T>
  Handle<T> NewHandle(T object) const;

  bool can_write() const { return mode() == MainThread; }

  inline MaybeObject GetFeedback(FeedbackVector vector,
                                 FeedbackSlot slot) const;
  inline void SetFeedback(FeedbackVector vector, FeedbackSlot slot,
                          MaybeObject object,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER) const;

  std::pair<MaybeObject, MaybeObject> GetFeedbackPair(FeedbackVector vector,
                                                      FeedbackSlot slot) const;
  void SetFeedbackPair(FeedbackVector vector, FeedbackSlot start_slot,
                       MaybeObject feedback, WriteBarrierMode mode,
                       MaybeObject feedback_extra,
                       WriteBarrierMode mode_extra) const;

 private:
  explicit NexusConfig(Isolate* isolate)
      : isolate_(isolate), local_heap_(nullptr) {}
  NexusConfig(Isolate* isolate, LocalHeap* local_heap)
      : isolate_(isolate), local_heap_(local_heap) {}

  Isolate* const isolate_;
  LocalHeap* const local_heap_;
};

// A FeedbackNexus is the combination of a FeedbackVector and a slot.
class V8_EXPORT_PRIVATE FeedbackNexus final {
 public:
  // For use on the main thread. A null {vector} is accepted as well.
  FeedbackNexus(Handle<FeedbackVector> vector, FeedbackSlot slot);
  FeedbackNexus(FeedbackVector vector, FeedbackSlot slot);

  // For use on the main or background thread as configured by {config}.
  // {vector} must be valid.
  FeedbackNexus(Handle<FeedbackVector> vector, FeedbackSlot slot,
                const NexusConfig& config);

  const NexusConfig* config() const { return &config_; }
  Handle<FeedbackVector> vector_handle() const {
    DCHECK(vector_.is_null());
    return vector_handle_;
  }
  FeedbackVector vector() const {
    return vector_handle_.is_null() ? vector_ : *vector_handle_;
  }

  FeedbackSlot slot() const { return slot_; }
  FeedbackSlotKind kind() const { return kind_; }

  inline LanguageMode GetLanguageMode() const {
    return vector().GetLanguageMode(slot());
  }

  InlineCacheState ic_state() const;
  bool IsUninitialized() const { return ic_state() == UNINITIALIZED; }
  bool IsMegamorphic() const { return ic_state() == MEGAMORPHIC; }
  bool IsGeneric() const { return ic_state() == GENERIC; }

  void Print(std::ostream& os);  // NOLINT

  // For map-based ICs (load, keyed-load, store, keyed-store).
  Map GetFirstMap() const;
  int ExtractMaps(MapHandles* maps) const;
  // Used to obtain maps and the associated handlers stored in the feedback
  // vector. This should be called when we expect only a handler to be stored in
  // the extra feedback. This is used by ICs when updating the handlers.
  using TryUpdateHandler = std::function<MaybeHandle<Map>(Handle<Map>)>;
  int ExtractMapsAndHandlers(
      std::vector<MapAndHandler>* maps_and_handlers,
      TryUpdateHandler map_handler = TryUpdateHandler()) const;
  MaybeObjectHandle FindHandlerForMap(Handle<Map> map) const;
  // Used to obtain maps and the associated feedback stored in the feedback
  // vector. The returned feedback need not be always a handler. It could be a
  // name in the case of StoreDataInPropertyLiteral. This is used by TurboFan to
  // get all the feedback stored in the vector.
  int ExtractMapsAndFeedback(
      std::vector<MapAndFeedback>* maps_and_feedback) const;

  bool IsCleared() const {
    InlineCacheState state = ic_state();
    return !FLAG_use_ic || state == UNINITIALIZED;
  }

  // Clear() returns true if the state of the underlying vector was changed.
  bool Clear();
  void ConfigureUninitialized();
  // ConfigureMegamorphic() returns true if the state of the underlying vector
  // was changed. Extra feedback is cleared if the 0 parameter version is used.
  bool ConfigureMegamorphic();
  bool ConfigureMegamorphic(IcCheckType property_type);

  inline MaybeObject GetFeedback() const;
  inline MaybeObject GetFeedbackExtra() const;
  inline std::pair<MaybeObject, MaybeObject> GetFeedbackPair() const;

  inline Isolate* GetIsolate() const;

  void ConfigureMonomorphic(Handle<Name> name, Handle<Map> receiver_map,
                            const MaybeObjectHandle& handler);

  void ConfigurePolymorphic(
      Handle<Name> name, std::vector<MapAndHandler> const& maps_and_handlers);

  BinaryOperationHint GetBinaryOperationFeedback() const;
  CompareOperationHint GetCompareOperationFeedback() const;
  ForInHint GetForInFeedback() const;

  // For KeyedLoad ICs.
  KeyedAccessLoadMode GetKeyedAccessLoadMode() const;

  // For KeyedStore ICs.
  KeyedAccessStoreMode GetKeyedAccessStoreMode() const;

  // For KeyedLoad and KeyedStore ICs.
  IcCheckType GetKeyType() const;
  Name GetName() const;

  // For Call ICs.
  int GetCallCount();
  void SetSpeculationMode(SpeculationMode mode);
  SpeculationMode GetSpeculationMode();

  // Compute the call frequency based on the call count and the invocation
  // count (taken from the type feedback vector).
  float ComputeCallFrequency();

  using SpeculationModeField = base::BitField<SpeculationMode, 0, 1>;
  using CallCountField = base::BitField<uint32_t, 1, 31>;

  // For InstanceOf ICs.
  MaybeHandle<JSObject> GetConstructorFeedback() const;

  // For Global Load and Store ICs.
  void ConfigurePropertyCellMode(Handle<PropertyCell> cell);
  // Returns false if given combination of indices is not allowed.
  bool ConfigureLexicalVarMode(int script_context_index, int context_slot_index,
                               bool immutable);
  void ConfigureHandlerMode(const MaybeObjectHandle& handler);

  // For CloneObject ICs
  static constexpr int kCloneObjectPolymorphicEntrySize = 2;
  void ConfigureCloneObject(Handle<Map> source_map, Handle<Map> result_map);

// Bit positions in a smi that encodes lexical environment variable access.
#define LEXICAL_MODE_BIT_FIELDS(V, _)  \
  V(ContextIndexBits, unsigned, 12, _) \
  V(SlotIndexBits, unsigned, 18, _)    \
  V(ImmutabilityBit, bool, 1, _)

  DEFINE_BIT_FIELDS(LEXICAL_MODE_BIT_FIELDS)
#undef LEXICAL_MODE_BIT_FIELDS

  // Make sure we don't overflow the smi.
  STATIC_ASSERT(LEXICAL_MODE_BIT_FIELDS_Ranges::kBitsCount <= kSmiValueSize);

  // For TypeProfile feedback vector slots.
  // ResetTypeProfile will always reset type profile information.
  void ResetTypeProfile();

  // Add a type to the list of types for source position <position>.
  void Collect(Handle<String> type, int position);
  JSObject GetTypeProfile() const;

  std::vector<int> GetSourcePositions() const;
  std::vector<Handle<String>> GetTypesForSourcePositions(uint32_t pos) const;

 private:
  template <typename FeedbackType>
  inline void SetFeedback(FeedbackType feedback,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  template <typename FeedbackType, typename FeedbackExtraType>
  inline void SetFeedback(FeedbackType feedback, WriteBarrierMode mode,
                          FeedbackExtraType feedback_extra,
                          WriteBarrierMode mode_extra = UPDATE_WRITE_BARRIER);

  inline MaybeObject UninitializedSentinel() const;
  inline MaybeObject MegamorphicSentinel() const;

  // Create an array. The caller must install it in a feedback vector slot.
  Handle<WeakFixedArray> CreateArrayOfSize(int length);

  // Helpers to maintain feedback_cache_.
  inline MaybeObject FromHandle(MaybeObjectHandle slot) const;
  inline MaybeObjectHandle ToHandle(MaybeObject value) const;

  // The reason for having a vector handle and a raw pointer is that we can and
  // should use handles during IC miss, but not during GC when we clear ICs. If
  // you have a handle to the vector that is better because more operations can
  // be done, like allocation.
  Handle<FeedbackVector> vector_handle_;
  FeedbackVector vector_;
  FeedbackSlot slot_;
  FeedbackSlotKind kind_;
  // When using the background-thread configuration, a cache is used to
  // guarantee a consistent view of the feedback to FeedbackNexus methods.
  mutable base::Optional<std::pair<MaybeObjectHandle, MaybeObjectHandle>>
      feedback_cache_;
  NexusConfig config_;
};

class V8_EXPORT_PRIVATE FeedbackIterator final {
 public:
  explicit FeedbackIterator(const FeedbackNexus* nexus);
  void Advance();
  bool done() { return done_; }
  Map map() { return map_; }
  MaybeObject handler() { return handler_; }

  static int SizeFor(int number_of_entries) {
    CHECK_GT(number_of_entries, 0);
    return number_of_entries * kEntrySize;
  }

  static int MapIndexForEntry(int entry) {
    CHECK_GE(entry, 0);
    return entry * kEntrySize;
  }

  static int HandlerIndexForEntry(int entry) {
    CHECK_GE(entry, 0);
    return (entry * kEntrySize) + kHandlerOffset;
  }

  static constexpr int kEntrySize = 2;
  static constexpr int kHandlerOffset = 1;

 private:
  void AdvancePolymorphic();
  enum State { kMonomorphic, kPolymorphic, kOther };

  Handle<WeakFixedArray> polymorphic_feedback_;
  Map map_;
  MaybeObject handler_;
  bool done_;
  int index_;
  State state_;
};

inline BinaryOperationHint BinaryOperationHintFromFeedback(int type_feedback);
inline CompareOperationHint CompareOperationHintFromFeedback(int type_feedback);
inline ForInHint ForInHintFromFeedback(ForInFeedback type_feedback);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FEEDBACK_VECTOR_H_
