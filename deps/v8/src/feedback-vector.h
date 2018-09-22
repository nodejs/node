// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FEEDBACK_VECTOR_H_
#define V8_FEEDBACK_VECTOR_H_

#include <vector>

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/elements-kind.h"
#include "src/globals.h"
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
  kStoreInArrayLiteral,
  kBinaryOp,
  kCompareOp,
  kStoreDataPropertyInLiteral,
  kTypeProfile,
  kCreateClosure,
  kLiteral,
  kForIn,
  kInstanceOf,
  kCloneObject,

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

std::ostream& operator<<(std::ostream& os, FeedbackSlotKind kind);

typedef std::vector<MaybeObjectHandle> MaybeObjectHandles;

class FeedbackMetadata;

// A FeedbackVector has a fixed header with:
//  - shared function info (which includes feedback metadata)
//  - invocation count
//  - runtime profiler ticks
//  - optimized code cell (weak cell or Smi marker)
// followed by an array of feedback slots, of length determined by the feedback
// metadata.
class FeedbackVector : public HeapObject, public NeverReadOnlySpaceObject {
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

  // [optimized_code_weak_or_smi]: weak reference to optimized code or a Smi
  // marker defining optimization behaviour.
  DECL_ACCESSORS(optimized_code_weak_or_smi, MaybeObject)

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

  // Clears the optimization marker in the feedback vector.
  void ClearOptimizationMarker();

  // Conversion from a slot to an integer index to the underlying array.
  static int GetIndex(FeedbackSlot slot) { return slot.ToInt(); }

  // Conversion from an integer index to the underlying array to a slot.
  static inline FeedbackSlot ToSlot(int index);
  inline MaybeObject* Get(FeedbackSlot slot) const;
  inline MaybeObject* get(int index) const;
  inline void Set(FeedbackSlot slot, MaybeObject* value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void set(int index, MaybeObject* value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void Set(FeedbackSlot slot, Object* value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void set(int index, Object* value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Gives access to raw memory which stores the array's data.
  inline MaybeObject** slots_start();

  // Returns slot kind for given slot.
  FeedbackSlotKind GetKind(FeedbackSlot slot) const;

  FeedbackSlot GetTypeProfileSlot() const;

  V8_EXPORT_PRIVATE static Handle<FeedbackVector> New(
      Isolate* isolate, Handle<SharedFunctionInfo> shared);

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

  static void AssertNoLegacyTypes(MaybeObject* object);

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
      RoundUp<kPointerAlignment>(int{kUnalignedHeaderSize});
  static const int kFeedbackSlotsOffset = kHeaderSize;

  class BodyDescriptor;

  // Garbage collection support.
  static constexpr int SizeFor(int length) {
    return kFeedbackSlotsOffset + length * kPointerSize;
  }

 private:
  static void AddToVectorsForProfilingTools(Isolate* isolate,
                                            Handle<FeedbackVector> vector);

  DISALLOW_IMPLICIT_CONSTRUCTORS(FeedbackVector);
};

class V8_EXPORT_PRIVATE FeedbackVectorSpec {
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

  void append(FeedbackSlotKind kind) {
    slot_kinds_.push_back(static_cast<unsigned char>(kind));
  }

  ZoneVector<unsigned char> slot_kinds_;

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
  // Casting.
  static inline FeedbackMetadata* cast(Object* obj);

  // The number of slots that this metadata contains. Stored as an int32.
  DECL_INT32_ACCESSORS(slot_count)

  // Get slot_count using an acquire load.
  inline int32_t synchronized_slot_count() const;

  // Returns number of feedback vector elements used by given slot kind.
  static inline int GetSlotSize(FeedbackSlotKind kind);

  bool SpecDiffersFrom(const FeedbackVectorSpec* other_spec) const;

  inline bool is_empty() const;

  // Returns slot kind for given slot.
  FeedbackSlotKind GetKind(FeedbackSlot slot) const;

  // If {spec} is null, then it is considered empty.
  V8_EXPORT_PRIVATE static Handle<FeedbackMetadata> New(
      Isolate* isolate, const FeedbackVectorSpec* spec = nullptr);

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

  static const int kSlotCountOffset = HeapObject::kHeaderSize;
  static const int kHeaderSize = kSlotCountOffset + kInt32Size;

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

  typedef BitSetComputer<FeedbackSlotKind, kFeedbackSlotKindBits,
                         kInt32Size * kBitsPerByte, uint32_t>
      VectorICComputer;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FeedbackMetadata);
};

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
class FeedbackNexus final {
 public:
  FeedbackNexus(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : vector_handle_(vector),
        vector_(nullptr),
        slot_(slot),
        kind_(vector->GetKind(slot)) {}
  FeedbackNexus(FeedbackVector* vector, FeedbackSlot slot)
      : vector_(vector), slot_(slot), kind_(vector->GetKind(slot)) {}

  Handle<FeedbackVector> vector_handle() const {
    DCHECK_NULL(vector_);
    return vector_handle_;
  }
  FeedbackVector* vector() const {
    return vector_handle_.is_null() ? vector_ : *vector_handle_;
  }
  FeedbackSlot slot() const { return slot_; }
  FeedbackSlotKind kind() const { return kind_; }

  inline LanguageMode GetLanguageMode() const {
    return vector()->GetLanguageMode(slot());
  }

  InlineCacheState ic_state() const { return StateFromFeedback(); }
  bool IsUninitialized() const { return StateFromFeedback() == UNINITIALIZED; }
  bool IsMegamorphic() const { return StateFromFeedback() == MEGAMORPHIC; }
  bool IsGeneric() const { return StateFromFeedback() == GENERIC; }

  void Print(std::ostream& os);  // NOLINT

  // For map-based ICs (load, keyed-load, store, keyed-store).
  Map* FindFirstMap() const {
    MapHandles maps;
    ExtractMaps(&maps);
    if (maps.size() > 0) return *maps.at(0);
    return nullptr;
  }

  InlineCacheState StateFromFeedback() const;
  int ExtractMaps(MapHandles* maps) const;
  MaybeObjectHandle FindHandlerForMap(Handle<Map> map) const;
  bool FindHandlers(MaybeObjectHandles* code_list, int length = -1) const;

  bool IsCleared() const {
    InlineCacheState state = StateFromFeedback();
    return !FLAG_use_ic || state == UNINITIALIZED || state == PREMONOMORPHIC;
  }

  // Clear() returns true if the state of the underlying vector was changed.
  bool Clear();
  void ConfigureUninitialized();
  void ConfigurePremonomorphic(Handle<Map> receiver_map);
  // ConfigureMegamorphic() returns true if the state of the underlying vector
  // was changed. Extra feedback is cleared if the 0 parameter version is used.
  bool ConfigureMegamorphic();
  bool ConfigureMegamorphic(IcCheckType property_type);

  inline MaybeObject* GetFeedback() const;
  inline MaybeObject* GetFeedbackExtra() const;

  inline Isolate* GetIsolate() const;

  void ConfigureMonomorphic(Handle<Name> name, Handle<Map> receiver_map,
                            const MaybeObjectHandle& handler);

  void ConfigurePolymorphic(Handle<Name> name, MapHandles const& maps,
                            MaybeObjectHandles* handlers);

  BinaryOperationHint GetBinaryOperationFeedback() const;
  CompareOperationHint GetCompareOperationFeedback() const;
  ForInHint GetForInFeedback() const;

  // For KeyedLoad ICs.
  KeyedAccessLoadMode GetKeyedAccessLoadMode() const;

  // For KeyedStore ICs.
  KeyedAccessStoreMode GetKeyedAccessStoreMode() const;

  // For KeyedLoad and KeyedStore ICs.
  IcCheckType GetKeyType() const;
  Name* FindFirstName() const;

  // For Call ICs.
  int GetCallCount();
  void SetSpeculationMode(SpeculationMode mode);
  SpeculationMode GetSpeculationMode();

  // Compute the call frequency based on the call count and the invocation
  // count (taken from the type feedback vector).
  float ComputeCallFrequency();

  typedef BitField<SpeculationMode, 0, 1> SpeculationModeField;
  typedef BitField<uint32_t, 1, 31> CallCountField;

  // For CreateClosure ICs.
  Handle<FeedbackCell> GetFeedbackCell() const;

  // For InstanceOf ICs.
  MaybeHandle<JSObject> GetConstructorFeedback() const;

  // For Global Load and Store ICs.
  void ConfigurePropertyCellMode(Handle<PropertyCell> cell);
  // Returns false if given combination of indices is not allowed.
  bool ConfigureLexicalVarMode(int script_context_index,
                               int context_slot_index);
  void ConfigureHandlerMode(const MaybeObjectHandle& handler);

  // For CloneObject ICs
  static constexpr int kCloneObjectPolymorphicEntrySize = 2;
  void ConfigureCloneObject(Handle<Map> source_map, Handle<Map> result_map);

// Bit positions in a smi that encodes lexical environment variable access.
#define LEXICAL_MODE_BIT_FIELDS(V, _)  \
  V(ContextIndexBits, unsigned, 12, _) \
  V(SlotIndexBits, unsigned, 19, _)

  DEFINE_BIT_FIELDS(LEXICAL_MODE_BIT_FIELDS)
#undef LEXICAL_MODE_BIT_FIELDS

  // Make sure we don't overflow the smi.
  STATIC_ASSERT(LEXICAL_MODE_BIT_FIELDS_Ranges::kBitsCount <= kSmiValueSize);

  // For TypeProfile feedback vector slots.
  // ResetTypeProfile will always reset type profile information.
  void ResetTypeProfile();

  // Add a type to the list of types for source position <position>.
  void Collect(Handle<String> type, int position);
  JSObject* GetTypeProfile() const;

  std::vector<int> GetSourcePositions() const;
  std::vector<Handle<String>> GetTypesForSourcePositions(uint32_t pos) const;

  inline void SetFeedback(Object* feedback,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void SetFeedback(MaybeObject* feedback,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void SetFeedbackExtra(Object* feedback_extra,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void SetFeedbackExtra(MaybeObject* feedback_extra,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  Handle<WeakFixedArray> EnsureArrayOfSize(int length);
  Handle<WeakFixedArray> EnsureExtraArrayOfSize(int length);

 private:
  // The reason for having a vector handle and a raw pointer is that we can and
  // should use handles during IC miss, but not during GC when we clear ICs. If
  // you have a handle to the vector that is better because more operations can
  // be done, like allocation.
  Handle<FeedbackVector> vector_handle_;
  FeedbackVector* vector_;
  FeedbackSlot slot_;
  FeedbackSlotKind kind_;
};

inline BinaryOperationHint BinaryOperationHintFromFeedback(int type_feedback);
inline CompareOperationHint CompareOperationHintFromFeedback(int type_feedback);
inline ForInHint ForInHintFromFeedback(int type_feedback);

}  // namespace internal
}  // namespace v8

#endif  // V8_FEEDBACK_VECTOR_H_
