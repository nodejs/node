// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_H_
#define V8_HEAP_HEAP_H_

#include <cmath>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!
#include "include/v8-internal.h"
#include "include/v8.h"
#include "src/accessors.h"
#include "src/allocation.h"
#include "src/assert-scope.h"
#include "src/base/atomic-utils.h"
#include "src/external-reference-table.h"
#include "src/globals.h"
#include "src/heap-symbols.h"
#include "src/objects.h"
#include "src/objects/fixed-array.h"
#include "src/objects/string-table.h"
#include "src/roots.h"
#include "src/visitors.h"

namespace v8 {

namespace debug {
typedef void (*OutOfMemoryCallback)(void* data);
}  // namespace debug

namespace internal {

namespace heap {
class HeapTester;
class TestMemoryAllocatorScope;
}  // namespace heap

class AllocationMemento;
class ObjectBoilerplateDescription;
class BytecodeArray;
class CodeDataContainer;
class DeoptimizationData;
class HandlerTable;
class IncrementalMarking;
class JSArrayBuffer;
class ExternalString;
using v8::MemoryPressureLevel;

// Adapts PRIVATE_SYMBOL_LIST_GERNATOR entry to IMMORTAL_IMMOVABLE_ROOT_LIST
// entry
#define PRIVATE_SYMBOL_LIST_TO_IMMORTAL_IMMOVABLE_LIST_ADAPTER(V, name) V(name)

// Heap roots that are known to be immortal immovable, for which we can safely
// skip write barriers. This list is not complete and has omissions.
#define IMMORTAL_IMMOVABLE_ROOT_LIST(V)     \
  V(ArgumentsMarker)                        \
  V(ArgumentsMarkerMap)                     \
  V(ArrayBufferNeuteringProtector)          \
  V(ArrayIteratorProtector)                 \
  V(AwaitContextMap)                        \
  V(BigIntMap)                              \
  V(BlockContextMap)                        \
  V(ObjectBoilerplateDescriptionMap)        \
  V(BooleanMap)                             \
  V(ByteArrayMap)                           \
  V(BytecodeArrayMap)                       \
  V(CatchContextMap)                        \
  V(CellMap)                                \
  V(CodeMap)                                \
  V(DebugEvaluateContextMap)                \
  V(DescriptorArrayMap)                     \
  V(EphemeronHashTableMap)                  \
  V(EmptyByteArray)                         \
  V(EmptyDescriptorArray)                   \
  V(EmptyFixedArray)                        \
  V(EmptyFixedFloat32Array)                 \
  V(EmptyFixedFloat64Array)                 \
  V(EmptyFixedInt16Array)                   \
  V(EmptyFixedInt32Array)                   \
  V(EmptyFixedInt8Array)                    \
  V(EmptyFixedUint16Array)                  \
  V(EmptyFixedUint32Array)                  \
  V(EmptyFixedUint8Array)                   \
  V(EmptyFixedUint8ClampedArray)            \
  V(EmptyOrderedHashMap)                    \
  V(EmptyOrderedHashSet)                    \
  V(EmptyPropertyCell)                      \
  V(EmptyScopeInfo)                         \
  V(EmptyScript)                            \
  V(EmptySloppyArgumentsElements)           \
  V(EmptySlowElementDictionary)             \
  V(EvalContextMap)                         \
  V(Exception)                              \
  V(FalseValue)                             \
  V(FixedArrayMap)                          \
  V(FixedCOWArrayMap)                       \
  V(FixedDoubleArrayMap)                    \
  V(ForeignMap)                             \
  V(FreeSpaceMap)                           \
  V(FunctionContextMap)                     \
  V(GlobalDictionaryMap)                    \
  V(GlobalPropertyCellMap)                  \
  V(HashTableMap)                           \
  V(HeapNumberMap)                          \
  V(HoleNanValue)                           \
  V(InfinityValue)                          \
  V(IsConcatSpreadableProtector)            \
  V(JSMessageObjectMap)                     \
  V(JsConstructEntryCode)                   \
  V(JsEntryCode)                            \
  V(ManyClosuresCell)                       \
  V(ManyClosuresCellMap)                    \
  V(MetaMap)                                \
  V(MinusInfinityValue)                     \
  V(MinusZeroValue)                         \
  V(ModuleContextMap)                       \
  V(ModuleInfoMap)                          \
  V(MutableHeapNumberMap)                   \
  V(NameDictionaryMap)                      \
  V(NanValue)                               \
  V(NativeContextMap)                       \
  V(NoClosuresCellMap)                      \
  V(NoElementsProtector)                    \
  V(NullMap)                                \
  V(NullValue)                              \
  V(NumberDictionaryMap)                    \
  V(OneClosureCellMap)                      \
  V(OnePointerFillerMap)                    \
  V(OptimizedOut)                           \
  V(OrderedHashMapMap)                      \
  V(OrderedHashSetMap)                      \
  V(PreParsedScopeDataMap)                  \
  V(PropertyArrayMap)                       \
  V(ScopeInfoMap)                           \
  V(ScriptContextMap)                       \
  V(ScriptContextTableMap)                  \
  V(SelfReferenceMarker)                    \
  V(SharedFunctionInfoMap)                  \
  V(SimpleNumberDictionaryMap)              \
  V(SloppyArgumentsElementsMap)             \
  V(SmallOrderedHashMapMap)                 \
  V(SmallOrderedHashSetMap)                 \
  V(ArraySpeciesProtector)                  \
  V(TypedArraySpeciesProtector)             \
  V(PromiseSpeciesProtector)                \
  V(StaleRegister)                          \
  V(StringIteratorProtector)                \
  V(StringLengthProtector)                  \
  V(StringTableMap)                         \
  V(SymbolMap)                              \
  V(TerminationException)                   \
  V(TheHoleMap)                             \
  V(TheHoleValue)                           \
  V(TransitionArrayMap)                     \
  V(TrueValue)                              \
  V(TwoPointerFillerMap)                    \
  V(UndefinedMap)                           \
  V(UndefinedValue)                         \
  V(UninitializedMap)                       \
  V(UninitializedValue)                     \
  V(UncompiledDataWithoutPreParsedScopeMap) \
  V(UncompiledDataWithPreParsedScopeMap)    \
  V(WeakFixedArrayMap)                      \
  V(WeakArrayListMap)                       \
  V(WithContextMap)                         \
  V(empty_string)                           \
  PRIVATE_SYMBOL_LIST_GENERATOR(            \
      PRIVATE_SYMBOL_LIST_TO_IMMORTAL_IMMOVABLE_LIST_ADAPTER, V)

class AllocationObserver;
class ArrayBufferCollector;
class ArrayBufferTracker;
class ConcurrentMarking;
class GCIdleTimeAction;
class GCIdleTimeHandler;
class GCIdleTimeHeapState;
class GCTracer;
class HeapController;
class HeapObjectAllocationTracker;
class HeapObjectsFilter;
class HeapStats;
class HistogramTimer;
class Isolate;
class LocalEmbedderHeapTracer;
class MemoryAllocator;
class MemoryReducer;
class MinorMarkCompactCollector;
class ObjectIterator;
class ObjectStats;
class Page;
class PagedSpace;
class RootVisitor;
class ScavengeJob;
class Scavenger;
class ScavengerCollector;
class Space;
class StoreBuffer;
class StressScavengeObserver;
class TracePossibleWrapperReporter;
class WeakObjectRetainer;

typedef void (*ObjectSlotCallback)(HeapObject** from, HeapObject* to);

enum ArrayStorageAllocationMode {
  DONT_INITIALIZE_ARRAY_ELEMENTS,
  INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE
};

enum class ClearRecordedSlots { kYes, kNo };

enum class ClearFreedMemoryMode { kClearFreedMemory, kDontClearFreedMemory };

enum ExternalBackingStoreType { kArrayBuffer, kExternalString, kNumTypes };

enum class FixedArrayVisitationMode { kRegular, kIncremental };

enum class TraceRetainingPathMode { kEnabled, kDisabled };

enum class RetainingPathOption { kDefault, kTrackEphemeronPath };

enum class GarbageCollectionReason {
  kUnknown = 0,
  kAllocationFailure = 1,
  kAllocationLimit = 2,
  kContextDisposal = 3,
  kCountersExtension = 4,
  kDebugger = 5,
  kDeserializer = 6,
  kExternalMemoryPressure = 7,
  kFinalizeMarkingViaStackGuard = 8,
  kFinalizeMarkingViaTask = 9,
  kFullHashtable = 10,
  kHeapProfiler = 11,
  kIdleTask = 12,
  kLastResort = 13,
  kLowMemoryNotification = 14,
  kMakeHeapIterable = 15,
  kMemoryPressure = 16,
  kMemoryReducer = 17,
  kRuntime = 18,
  kSamplingProfiler = 19,
  kSnapshotCreator = 20,
  kTesting = 21,
  kExternalFinalize = 22
  // If you add new items here, then update the incremental_marking_reason,
  // mark_compact_reason, and scavenge_reason counters in counters.h.
  // Also update src/tools/metrics/histograms/histograms.xml in chromium.
};

enum class YoungGenerationHandling {
  kRegularScavenge = 0,
  kFastPromotionDuringScavenge = 1,
  // Histogram::InspectConstructionArguments in chromium requires us to have at
  // least three buckets.
  kUnusedBucket = 2,
  // If you add new items here, then update the young_generation_handling in
  // counters.h.
  // Also update src/tools/metrics/histograms/histograms.xml in chromium.
};

class AllocationResult {
 public:
  static inline AllocationResult Retry(AllocationSpace space = NEW_SPACE) {
    return AllocationResult(space);
  }

  // Implicit constructor from Object*.
  AllocationResult(Object* object)  // NOLINT
      : object_(object) {
    // AllocationResults can't return Smis, which are used to represent
    // failure and the space to retry in.
    CHECK(!object->IsSmi());
  }

  AllocationResult() : object_(Smi::FromInt(NEW_SPACE)) {}

  inline bool IsRetry() { return object_->IsSmi(); }
  inline HeapObject* ToObjectChecked();
  inline AllocationSpace RetrySpace();

  template <typename T>
  bool To(T** obj) {
    if (IsRetry()) return false;
    *obj = T::cast(object_);
    return true;
  }

 private:
  explicit AllocationResult(AllocationSpace space)
      : object_(Smi::FromInt(static_cast<int>(space))) {}

  Object* object_;
};

STATIC_ASSERT(sizeof(AllocationResult) == kPointerSize);

#ifdef DEBUG
struct CommentStatistic {
  const char* comment;
  int size;
  int count;
  void Clear() {
    comment = nullptr;
    size = 0;
    count = 0;
  }
  // Must be small, since an iteration is used for lookup.
  static const int kMaxComments = 64;
};
#endif

class Heap {
 public:
  enum FindMementoMode { kForRuntime, kForGC };

  enum HeapState {
    NOT_IN_GC,
    SCAVENGE,
    MARK_COMPACT,
    MINOR_MARK_COMPACT,
    TEAR_DOWN
  };

  using PretenuringFeedbackMap = std::unordered_map<AllocationSite*, size_t>;

  // Taking this mutex prevents the GC from entering a phase that relocates
  // object references.
  base::Mutex* relocation_mutex() { return &relocation_mutex_; }

  // Support for partial snapshots.  After calling this we have a linear
  // space to write objects in each space.
  struct Chunk {
    uint32_t size;
    Address start;
    Address end;
  };
  typedef std::vector<Chunk> Reservation;

  static const int kInitalOldGenerationLimitFactor = 2;

#if V8_OS_ANDROID
  // Don't apply pointer multiplier on Android since it has no swap space and
  // should instead adapt it's heap size based on available physical memory.
  static const int kPointerMultiplier = 1;
#else
  static const int kPointerMultiplier = i::kPointerSize / 4;
#endif

  // Semi-space size needs to be a multiple of page size.
  static const size_t kMinSemiSpaceSizeInKB =
      1 * kPointerMultiplier * ((1 << kPageSizeBits) / KB);
  static const size_t kMaxSemiSpaceSizeInKB =
      16 * kPointerMultiplier * ((1 << kPageSizeBits) / KB);

  static const int kTraceRingBufferSize = 512;
  static const int kStacktraceBufferSize = 512;

  static const int kNoGCFlags = 0;
  static const int kReduceMemoryFootprintMask = 1;

  // The roots that have an index less than this are always in old space.
  static const int kOldSpaceRoots = 0x20;

  // The minimum size of a HeapObject on the heap.
  static const int kMinObjectSizeInWords = 2;

  static const int kMinPromotedPercentForFastPromotionMode = 90;

  STATIC_ASSERT(static_cast<int>(RootIndex::kUndefinedValue) ==
                Internals::kUndefinedValueRootIndex);
  STATIC_ASSERT(static_cast<int>(RootIndex::kTheHoleValue) ==
                Internals::kTheHoleValueRootIndex);
  STATIC_ASSERT(static_cast<int>(RootIndex::kNullValue) ==
                Internals::kNullValueRootIndex);
  STATIC_ASSERT(static_cast<int>(RootIndex::kTrueValue) ==
                Internals::kTrueValueRootIndex);
  STATIC_ASSERT(static_cast<int>(RootIndex::kFalseValue) ==
                Internals::kFalseValueRootIndex);
  STATIC_ASSERT(static_cast<int>(RootIndex::kempty_string) ==
                Internals::kEmptyStringRootIndex);

  // Calculates the maximum amount of filler that could be required by the
  // given alignment.
  static int GetMaximumFillToAlign(AllocationAlignment alignment);
  // Calculates the actual amount of filler required for a given address at the
  // given alignment.
  static int GetFillToAlign(Address address, AllocationAlignment alignment);

  void FatalProcessOutOfMemory(const char* location);

  V8_EXPORT_PRIVATE static bool RootIsImmortalImmovable(RootIndex root_index);

  // Checks whether the space is valid.
  static bool IsValidAllocationSpace(AllocationSpace space);

  // Generated code can embed direct references to non-writable roots if
  // they are in new space.
  static bool RootCanBeWrittenAfterInitialization(RootIndex root_index);

  // Zapping is needed for verify heap, and always done in debug builds.
  static inline bool ShouldZapGarbage() {
#ifdef DEBUG
    return true;
#else
#ifdef VERIFY_HEAP
    return FLAG_verify_heap;
#else
    return false;
#endif
#endif
  }

  static uintptr_t ZapValue() {
    return FLAG_clear_free_memory ? kClearedFreeMemoryValue : kZapValue;
  }

  static inline bool IsYoungGenerationCollector(GarbageCollector collector) {
    return collector == SCAVENGER || collector == MINOR_MARK_COMPACTOR;
  }

  static inline GarbageCollector YoungGenerationCollector() {
#if ENABLE_MINOR_MC
    return (FLAG_minor_mc) ? MINOR_MARK_COMPACTOR : SCAVENGER;
#else
    return SCAVENGER;
#endif  // ENABLE_MINOR_MC
  }

  static inline const char* CollectorName(GarbageCollector collector) {
    switch (collector) {
      case SCAVENGER:
        return "Scavenger";
      case MARK_COMPACTOR:
        return "Mark-Compact";
      case MINOR_MARK_COMPACTOR:
        return "Minor Mark-Compact";
    }
    return "Unknown collector";
  }

  // Copy block of memory from src to dst. Size of block should be aligned
  // by pointer size.
  static inline void CopyBlock(Address dst, Address src, int byte_size);

  V8_EXPORT_PRIVATE static void WriteBarrierForCodeSlow(Code* host);
  V8_EXPORT_PRIVATE static void GenerationalBarrierSlow(HeapObject* object,
                                                        Address slot,
                                                        HeapObject* value);
  V8_EXPORT_PRIVATE static void GenerationalBarrierForElementsSlow(
      Heap* heap, FixedArray* array, int offset, int length);
  V8_EXPORT_PRIVATE static void GenerationalBarrierForCodeSlow(
      Code* host, RelocInfo* rinfo, HeapObject* value);
  V8_EXPORT_PRIVATE static void MarkingBarrierSlow(HeapObject* object,
                                                   Address slot,
                                                   HeapObject* value);
  V8_EXPORT_PRIVATE static void MarkingBarrierForElementsSlow(
      Heap* heap, HeapObject* object);
  V8_EXPORT_PRIVATE static void MarkingBarrierForCodeSlow(Code* host,
                                                          RelocInfo* rinfo,
                                                          HeapObject* value);
  V8_EXPORT_PRIVATE static bool PageFlagsAreConsistent(HeapObject* object);

  // Notifies the heap that is ok to start marking or other activities that
  // should not happen during deserialization.
  void NotifyDeserializationComplete();

  inline Address* NewSpaceAllocationTopAddress();
  inline Address* NewSpaceAllocationLimitAddress();
  inline Address* OldSpaceAllocationTopAddress();
  inline Address* OldSpaceAllocationLimitAddress();

  // FreeSpace objects have a null map after deserialization. Update the map.
  void RepairFreeListsAfterDeserialization();

  // Move len elements within a given array from src_index index to dst_index
  // index.
  void MoveElements(FixedArray* array, int dst_index, int src_index, int len,
                    WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Initialize a filler object to keep the ability to iterate over the heap
  // when introducing gaps within pages. If slots could have been recorded in
  // the freed area, then pass ClearRecordedSlots::kYes as the mode. Otherwise,
  // pass ClearRecordedSlots::kNo. If the memory after the object header of
  // the filler should be cleared, pass in kClearFreedMemory. The default is
  // kDontClearFreedMemory.
  V8_EXPORT_PRIVATE HeapObject* CreateFillerObjectAt(
      Address addr, int size, ClearRecordedSlots clear_slots_mode,
      ClearFreedMemoryMode clear_memory_mode =
          ClearFreedMemoryMode::kDontClearFreedMemory);

  template <typename T>
  void CreateFillerForArray(T* object, int elements_to_trim, int bytes_to_trim);

  bool CanMoveObjectStart(HeapObject* object);

  static bool IsImmovable(HeapObject* object);

  // Trim the given array from the left. Note that this relocates the object
  // start and hence is only valid if there is only a single reference to it.
  FixedArrayBase* LeftTrimFixedArray(FixedArrayBase* obj, int elements_to_trim);

  // Trim the given array from the right.
  void RightTrimFixedArray(FixedArrayBase* obj, int elements_to_trim);
  void RightTrimWeakFixedArray(WeakFixedArray* obj, int elements_to_trim);

  // Converts the given boolean condition to JavaScript boolean value.
  inline Oddball* ToBoolean(bool condition);

  // Notify the heap that a context has been disposed.
  int NotifyContextDisposed(bool dependant_context);

  void set_native_contexts_list(Object* object) {
    native_contexts_list_ = object;
  }
  Object* native_contexts_list() const { return native_contexts_list_; }

  void set_allocation_sites_list(Object* object) {
    allocation_sites_list_ = object;
  }
  Object* allocation_sites_list() { return allocation_sites_list_; }

  // Used in CreateAllocationSiteStub and the (de)serializer.
  Object** allocation_sites_list_address() { return &allocation_sites_list_; }

  // Traverse all the allocaions_sites [nested_site and weak_next] in the list
  // and foreach call the visitor
  void ForeachAllocationSite(
      Object* list, const std::function<void(AllocationSite*)>& visitor);

  // Number of mark-sweeps.
  int ms_count() const { return ms_count_; }

  // Checks whether the given object is allowed to be migrated from it's
  // current space into the given destination space. Used for debugging.
  bool AllowedToBeMigrated(HeapObject* object, AllocationSpace dest);

  void CheckHandleCount();

  // Number of "runtime allocations" done so far.
  uint32_t allocations_count() { return allocations_count_; }

  // Print short heap statistics.
  void PrintShortHeapStatistics();

  bool write_protect_code_memory() const { return write_protect_code_memory_; }

  uintptr_t code_space_memory_modification_scope_depth() {
    return code_space_memory_modification_scope_depth_;
  }

  void increment_code_space_memory_modification_scope_depth() {
    code_space_memory_modification_scope_depth_++;
  }

  void decrement_code_space_memory_modification_scope_depth() {
    code_space_memory_modification_scope_depth_--;
  }

  void UnprotectAndRegisterMemoryChunk(MemoryChunk* chunk);
  void UnprotectAndRegisterMemoryChunk(HeapObject* object);
  void UnregisterUnprotectedMemoryChunk(MemoryChunk* chunk);
  V8_EXPORT_PRIVATE void ProtectUnprotectedMemoryChunks();

  void EnableUnprotectedMemoryChunksRegistry() {
    unprotected_memory_chunks_registry_enabled_ = true;
  }

  void DisableUnprotectedMemoryChunksRegistry() {
    unprotected_memory_chunks_registry_enabled_ = false;
  }

  bool unprotected_memory_chunks_registry_enabled() {
    return unprotected_memory_chunks_registry_enabled_;
  }

  inline HeapState gc_state() { return gc_state_; }
  void SetGCState(HeapState state);
  bool IsTearingDown() const { return gc_state_ == TEAR_DOWN; }

  inline bool IsInGCPostProcessing() { return gc_post_processing_depth_ > 0; }

  // If an object has an AllocationMemento trailing it, return it, otherwise
  // return nullptr;
  template <FindMementoMode mode>
  inline AllocationMemento* FindAllocationMemento(Map* map, HeapObject* object);

  // Returns false if not able to reserve.
  bool ReserveSpace(Reservation* reservations, std::vector<Address>* maps);

  //
  // Support for the API.
  //

  void CreateApiObjects();

  // Implements the corresponding V8 API function.
  bool IdleNotification(double deadline_in_seconds);
  bool IdleNotification(int idle_time_in_ms);

  void MemoryPressureNotification(MemoryPressureLevel level,
                                  bool is_isolate_locked);
  void CheckMemoryPressure();

  void AddNearHeapLimitCallback(v8::NearHeapLimitCallback, void* data);
  void RemoveNearHeapLimitCallback(v8::NearHeapLimitCallback callback,
                                   size_t heap_limit);

  double MonotonicallyIncreasingTimeInMs();

  void RecordStats(HeapStats* stats, bool take_snapshot = false);

  // Check new space expansion criteria and expand semispaces if it was hit.
  void CheckNewSpaceExpansionCriteria();

  void VisitExternalResources(v8::ExternalResourceVisitor* visitor);

  // An object should be promoted if the object has survived a
  // scavenge operation.
  inline bool ShouldBePromoted(Address old_address);

  void IncrementDeferredCount(v8::Isolate::UseCounterFeature feature);

  inline uint64_t HashSeed();

  inline int NextScriptId();
  inline int NextDebuggingId();
  inline int GetNextTemplateSerialNumber();

  void SetSerializedObjects(FixedArray* objects);
  void SetSerializedGlobalProxySizes(FixedArray* sizes);

  // For post mortem debugging.
  void RememberUnmappedPage(Address page, bool compacted);

  int64_t external_memory_hard_limit() { return MaxOldGenerationSize() / 2; }

  int64_t external_memory() { return external_memory_; }
  void update_external_memory(int64_t delta) { external_memory_ += delta; }

  void update_external_memory_concurrently_freed(intptr_t freed) {
    external_memory_concurrently_freed_ += freed;
  }

  void account_external_memory_concurrently_freed() {
    external_memory_ -= external_memory_concurrently_freed_;
    external_memory_concurrently_freed_ = 0;
  }

  size_t backing_store_bytes() const { return backing_store_bytes_; }

  void CompactWeakArrayLists(PretenureFlag pretenure);

  void AddRetainedMap(Handle<Map> map);

  // This event is triggered after successful allocation of a new object made
  // by runtime. Allocations of target space for object evacuation do not
  // trigger the event. In order to track ALL allocations one must turn off
  // FLAG_inline_new.
  inline void OnAllocationEvent(HeapObject* object, int size_in_bytes);

  // This event is triggered after object is moved to a new place.
  inline void OnMoveEvent(HeapObject* target, HeapObject* source,
                          int size_in_bytes);

  inline bool CanAllocateInReadOnlySpace();
  bool deserialization_complete() const { return deserialization_complete_; }

  bool HasLowAllocationRate();
  bool HasHighFragmentation();
  bool HasHighFragmentation(size_t used, size_t committed);

  void ActivateMemoryReducerIfNeeded();

  bool ShouldOptimizeForMemoryUsage();

  bool HighMemoryPressure() {
    return memory_pressure_level_ != MemoryPressureLevel::kNone;
  }

  void RestoreHeapLimit(size_t heap_limit) {
    // Do not set the limit lower than the live size + some slack.
    size_t min_limit = SizeOfObjects() + SizeOfObjects() / 4;
    max_old_generation_size_ =
        Min(max_old_generation_size_, Max(heap_limit, min_limit));
  }

  // ===========================================================================
  // Initialization. ===========================================================
  // ===========================================================================

  // Configure heap sizes
  // max_semi_space_size_in_kb: maximum semi-space size in KB
  // max_old_generation_size_in_mb: maximum old generation size in MB
  // code_range_size_in_mb: code range size in MB
  void ConfigureHeap(size_t max_semi_space_size_in_kb,
                     size_t max_old_generation_size_in_mb,
                     size_t code_range_size_in_mb);
  void ConfigureHeapDefault();

  // Prepares the heap, setting up memory areas that are needed in the isolate
  // without actually creating any objects.
  void SetUp();

  // (Re-)Initialize hash seed from flag or RNG.
  void InitializeHashSeed();

  // Bootstraps the object heap with the core set of objects required to run.
  // Returns whether it succeeded.
  bool CreateHeapObjects();

  // Create ObjectStats if live_object_stats_ or dead_object_stats_ are nullptr.
  void CreateObjectStats();

  // Sets the TearDown state, so no new GC tasks get posted.
  void StartTearDown();

  // Destroys all memory allocated by the heap.
  void TearDown();

  // Returns whether SetUp has been called.
  bool HasBeenSetUp();

  // ===========================================================================
  // Getters for spaces. =======================================================
  // ===========================================================================

  inline Address NewSpaceTop();

  NewSpace* new_space() { return new_space_; }
  OldSpace* old_space() { return old_space_; }
  CodeSpace* code_space() { return code_space_; }
  MapSpace* map_space() { return map_space_; }
  LargeObjectSpace* lo_space() { return lo_space_; }
  NewLargeObjectSpace* new_lo_space() { return new_lo_space_; }
  ReadOnlySpace* read_only_space() { return read_only_space_; }

  inline PagedSpace* paged_space(int idx);
  inline Space* space(int idx);

  // Returns name of the space.
  const char* GetSpaceName(int idx);

  // ===========================================================================
  // Getters to other components. ==============================================
  // ===========================================================================

  GCTracer* tracer() { return tracer_; }

  MemoryAllocator* memory_allocator() { return memory_allocator_; }

  inline Isolate* isolate();

  MarkCompactCollector* mark_compact_collector() {
    return mark_compact_collector_;
  }

  MinorMarkCompactCollector* minor_mark_compact_collector() {
    return minor_mark_compact_collector_;
  }

  ArrayBufferCollector* array_buffer_collector() {
    return array_buffer_collector_;
  }

  // ===========================================================================
  // Root set access. ==========================================================
  // ===========================================================================
  friend class ReadOnlyRoots;

 public:
  RootsTable& roots_table() { return roots_; }

// Heap root getters.
#define ROOT_ACCESSOR(type, name, CamelName) inline type* name();
  MUTABLE_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  Object* root(RootIndex index) { return roots_[index]; }
  Handle<Object> root_handle(RootIndex index) {
    return Handle<Object>(&roots_[index]);
  }

  bool IsRootHandleLocation(Object** handle_location, RootIndex* index) const {
    return roots_.IsRootHandleLocation(handle_location, index);
  }

  template <typename T>
  bool IsRootHandle(Handle<T> handle, RootIndex* index) const {
    return roots_.IsRootHandle(handle, index);
  }

  // Generated code can embed this address to get access to the roots.
  Object** roots_array_start() { return roots_.roots_; }

  ExternalReferenceTable* external_reference_table() {
    DCHECK(external_reference_table_.is_initialized());
    return &external_reference_table_;
  }

  static constexpr int roots_to_external_reference_table_offset() {
    return kRootsExternalReferenceTableOffset;
  }

  static constexpr int roots_to_builtins_offset() {
    return kRootsBuiltinsOffset;
  }

  static constexpr int root_register_addressable_end_offset() {
    return kRootRegisterAddressableEndOffset;
  }

  Address root_register_addressable_end() {
    return reinterpret_cast<Address>(roots_array_start()) +
           kRootRegisterAddressableEndOffset;
  }

  // Sets the stub_cache_ (only used when expanding the dictionary).
  void SetRootCodeStubs(SimpleNumberDictionary* value);

  void SetRootMaterializedObjects(FixedArray* objects) {
    roots_[RootIndex::kMaterializedObjects] = objects;
  }

  void SetRootScriptList(Object* value) {
    roots_[RootIndex::kScriptList] = value;
  }

  void SetRootStringTable(StringTable* value) {
    roots_[RootIndex::kStringTable] = value;
  }

  void SetRootNoScriptSharedFunctionInfos(Object* value) {
    roots_[RootIndex::kNoScriptSharedFunctionInfos] = value;
  }

  void SetMessageListeners(TemplateList* value) {
    roots_[RootIndex::kMessageListeners] = value;
  }

  // Set the stack limit in the roots_ array.  Some architectures generate
  // code that looks here, because it is faster than loading from the static
  // jslimit_/real_jslimit_ variable in the StackGuard.
  void SetStackLimits();

  // The stack limit is thread-dependent. To be able to reproduce the same
  // snapshot blob, we need to reset it before serializing.
  void ClearStackLimits();

  // Generated code can treat direct references to this root as constant.
  bool RootCanBeTreatedAsConstant(RootIndex root_index);

  void RegisterStrongRoots(Object** start, Object** end);
  void UnregisterStrongRoots(Object** start);

  void SetBuiltinsConstantsTable(FixedArray* cache);

  // ===========================================================================
  // Inline allocation. ========================================================
  // ===========================================================================

  // Indicates whether inline bump-pointer allocation has been disabled.
  bool inline_allocation_disabled() { return inline_allocation_disabled_; }

  // Switch whether inline bump-pointer allocation should be used.
  void EnableInlineAllocation();
  void DisableInlineAllocation();

  // ===========================================================================
  // Methods triggering GCs. ===================================================
  // ===========================================================================

  // Performs garbage collection operation.
  // Returns whether there is a chance that another major GC could
  // collect more garbage.
  V8_EXPORT_PRIVATE bool CollectGarbage(
      AllocationSpace space, GarbageCollectionReason gc_reason,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  // Performs a full garbage collection.
  V8_EXPORT_PRIVATE void CollectAllGarbage(
      int flags, GarbageCollectionReason gc_reason,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  // Last hope GC, should try to squeeze as much as possible.
  void CollectAllAvailableGarbage(GarbageCollectionReason gc_reason);

  // Precise garbage collection that potentially finalizes already running
  // incremental marking before performing an atomic garbage collection.
  // Only use if absolutely necessary or in tests to avoid floating garbage!
  void PreciseCollectAllGarbage(
      int flags, GarbageCollectionReason gc_reason,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  // Reports and external memory pressure event, either performs a major GC or
  // completes incremental marking in order to free external resources.
  void ReportExternalMemoryPressure();

  typedef v8::Isolate::GetExternallyAllocatedMemoryInBytesCallback
      GetExternallyAllocatedMemoryInBytesCallback;

  void SetGetExternallyAllocatedMemoryInBytesCallback(
      GetExternallyAllocatedMemoryInBytesCallback callback) {
    external_memory_callback_ = callback;
  }

  // Invoked when GC was requested via the stack guard.
  void HandleGCRequest();

  // ===========================================================================
  // Builtins. =================================================================
  // ===========================================================================

  Code* builtin(int index);
  Address builtin_address(int index);
  void set_builtin(int index, HeapObject* builtin);

  // ===========================================================================
  // Iterators. ================================================================
  // ===========================================================================

  void IterateRoots(RootVisitor* v, VisitMode mode);
  void IterateStrongRoots(RootVisitor* v, VisitMode mode);
  // Iterates over entries in the smi roots list.  Only interesting to the
  // serializer/deserializer, since GC does not care about smis.
  void IterateSmiRoots(RootVisitor* v);
  // Iterates over weak string tables.
  void IterateWeakRoots(RootVisitor* v, VisitMode mode);
  // Iterates over weak global handles.
  void IterateWeakGlobalHandles(RootVisitor* v);
  // Iterates over builtins.
  void IterateBuiltins(RootVisitor* v);

  // ===========================================================================
  // Store buffer API. =========================================================
  // ===========================================================================

  // Used for query incremental marking status in generated code.
  Address* IsMarkingFlagAddress() {
    return reinterpret_cast<Address*>(&is_marking_flag_);
  }

  void SetIsMarkingFlag(uint8_t flag) { is_marking_flag_ = flag; }

  Address* store_buffer_top_address();
  static intptr_t store_buffer_mask_constant();
  static Address store_buffer_overflow_function_address();

  void ClearRecordedSlot(HeapObject* object, Object** slot);
  void ClearRecordedSlotRange(Address start, Address end);

#ifdef DEBUG
  void VerifyClearedSlot(HeapObject* object, Object** slot);
#endif

  // ===========================================================================
  // Incremental marking API. ==================================================
  // ===========================================================================

  int GCFlagsForIncrementalMarking() {
    return ShouldOptimizeForMemoryUsage() ? kReduceMemoryFootprintMask
                                          : kNoGCFlags;
  }

  // Start incremental marking and ensure that idle time handler can perform
  // incremental steps.
  void StartIdleIncrementalMarking(
      GarbageCollectionReason gc_reason,
      GCCallbackFlags gc_callback_flags = GCCallbackFlags::kNoGCCallbackFlags);

  // Starts incremental marking assuming incremental marking is currently
  // stopped.
  void StartIncrementalMarking(
      int gc_flags, GarbageCollectionReason gc_reason,
      GCCallbackFlags gc_callback_flags = GCCallbackFlags::kNoGCCallbackFlags);

  void StartIncrementalMarkingIfAllocationLimitIsReached(
      int gc_flags,
      GCCallbackFlags gc_callback_flags = GCCallbackFlags::kNoGCCallbackFlags);

  void FinalizeIncrementalMarkingIfComplete(GarbageCollectionReason gc_reason);
  // Synchronously finalizes incremental marking.
  void FinalizeIncrementalMarkingAtomically(GarbageCollectionReason gc_reason);

  void RegisterDeserializedObjectsForBlackAllocation(
      Reservation* reservations, const std::vector<HeapObject*>& large_objects,
      const std::vector<Address>& maps);

  IncrementalMarking* incremental_marking() { return incremental_marking_; }

  // ===========================================================================
  // Concurrent marking API. ===================================================
  // ===========================================================================

  ConcurrentMarking* concurrent_marking() { return concurrent_marking_; }

  // The runtime uses this function to notify potentially unsafe object layout
  // changes that require special synchronization with the concurrent marker.
  // The old size is the size of the object before layout change.
  void NotifyObjectLayoutChange(HeapObject* object, int old_size,
                                const DisallowHeapAllocation&);

#ifdef VERIFY_HEAP
  // This function checks that either
  // - the map transition is safe,
  // - or it was communicated to GC using NotifyObjectLayoutChange.
  void VerifyObjectLayoutChange(HeapObject* object, Map* new_map);
#endif

  // ===========================================================================
  // Deoptimization support API. ===============================================
  // ===========================================================================

  // Setters for code offsets of well-known deoptimization targets.
  void SetArgumentsAdaptorDeoptPCOffset(int pc_offset);
  void SetConstructStubCreateDeoptPCOffset(int pc_offset);
  void SetConstructStubInvokeDeoptPCOffset(int pc_offset);
  void SetInterpreterEntryReturnPCOffset(int pc_offset);

  // Invalidates references in the given {code} object that are directly
  // embedded within the instruction stream. Mutates write-protected code.
  void InvalidateCodeEmbeddedObjects(Code* code);

  // Invalidates references in the given {code} object that are referenced
  // transitively from the deoptimization data. Mutates write-protected code.
  void InvalidateCodeDeoptimizationData(Code* code);

  void DeoptMarkedAllocationSites();

  bool DeoptMaybeTenuredAllocationSites();

  // ===========================================================================
  // Embedder heap tracer support. =============================================
  // ===========================================================================

  LocalEmbedderHeapTracer* local_embedder_heap_tracer() const {
    return local_embedder_heap_tracer_;
  }

  void SetEmbedderHeapTracer(EmbedderHeapTracer* tracer);
  EmbedderHeapTracer* GetEmbedderHeapTracer() const;

  void TracePossibleWrapper(JSObject* js_object);
  void RegisterExternallyReferencedObject(Object** object);
  void SetEmbedderStackStateForNextFinalizaton(
      EmbedderHeapTracer::EmbedderStackState stack_state);

  // ===========================================================================
  // External string table API. ================================================
  // ===========================================================================

  // Registers an external string.
  inline void RegisterExternalString(String* string);

  // Called when a string's resource is changed. The size of the payload is sent
  // as argument of the method.
  inline void UpdateExternalString(String* string, size_t old_payload,
                                   size_t new_payload);

  // Finalizes an external string by deleting the associated external
  // data and clearing the resource pointer.
  inline void FinalizeExternalString(String* string);

  static String* UpdateNewSpaceReferenceInExternalStringTableEntry(
      Heap* heap, Object** pointer);

  // ===========================================================================
  // Methods checking/returning the space of a given object/address. ===========
  // ===========================================================================

  // Returns whether the object resides in new space.
  static inline bool InNewSpace(Object* object);
  static inline bool InNewSpace(MaybeObject* object);
  static inline bool InNewSpace(HeapObject* heap_object);
  static inline bool InFromSpace(Object* object);
  static inline bool InFromSpace(MaybeObject* object);
  static inline bool InFromSpace(HeapObject* heap_object);
  static inline bool InToSpace(Object* object);
  static inline bool InToSpace(MaybeObject* object);
  static inline bool InToSpace(HeapObject* heap_object);

  // Returns whether the object resides in old space.
  inline bool InOldSpace(Object* object);

  // Returns whether the object resides in read-only space.
  inline bool InReadOnlySpace(Object* object);

  // Checks whether an address/object in the heap (including auxiliary
  // area and unused area).
  bool Contains(HeapObject* value);

  // Checks whether an address/object in a space.
  // Currently used by tests, serialization and heap verification only.
  bool InSpace(HeapObject* value, AllocationSpace space);

  // Slow methods that can be used for verification as they can also be used
  // with off-heap Addresses.
  bool ContainsSlow(Address addr);
  bool InSpaceSlow(Address addr, AllocationSpace space);
  inline bool InNewSpaceSlow(Address address);
  inline bool InOldSpaceSlow(Address address);

  // Find the heap which owns this HeapObject. Should never be called for
  // objects in RO space.
  static inline Heap* FromWritableHeapObject(const HeapObject* obj);

  // ===========================================================================
  // Object statistics tracking. ===============================================
  // ===========================================================================

  // Returns the number of buckets used by object statistics tracking during a
  // major GC. Note that the following methods fail gracefully when the bounds
  // are exceeded though.
  size_t NumberOfTrackedHeapObjectTypes();

  // Returns object statistics about count and size at the last major GC.
  // Objects are being grouped into buckets that roughly resemble existing
  // instance types.
  size_t ObjectCountAtLastGC(size_t index);
  size_t ObjectSizeAtLastGC(size_t index);

  // Retrieves names of buckets used by object statistics tracking.
  bool GetObjectTypeName(size_t index, const char** object_type,
                         const char** object_sub_type);

  // The total number of native contexts object on the heap.
  size_t NumberOfNativeContexts();
  // The total number of native contexts that were detached but were not
  // garbage collected yet.
  size_t NumberOfDetachedContexts();

  // ===========================================================================
  // Code statistics. ==========================================================
  // ===========================================================================

  // Collect code (Code and BytecodeArray objects) statistics.
  void CollectCodeStatistics();

  // ===========================================================================
  // GC statistics. ============================================================
  // ===========================================================================

  // Returns the maximum amount of memory reserved for the heap.
  size_t MaxReserved();
  size_t MaxSemiSpaceSize() { return max_semi_space_size_; }
  size_t InitialSemiSpaceSize() { return initial_semispace_size_; }
  size_t MaxOldGenerationSize() { return max_old_generation_size_; }

  V8_EXPORT_PRIVATE static size_t ComputeMaxOldGenerationSize(
      uint64_t physical_memory);

  static size_t ComputeMaxSemiSpaceSize(uint64_t physical_memory) {
    const uint64_t min_physical_memory = 512 * MB;
    const uint64_t max_physical_memory = 3 * static_cast<uint64_t>(GB);

    uint64_t capped_physical_memory =
        Max(Min(physical_memory, max_physical_memory), min_physical_memory);
    // linearly scale max semi-space size: (X-A)/(B-A)*(D-C)+C
    size_t semi_space_size_in_kb =
        static_cast<size_t>(((capped_physical_memory - min_physical_memory) *
                             (kMaxSemiSpaceSizeInKB - kMinSemiSpaceSizeInKB)) /
                                (max_physical_memory - min_physical_memory) +
                            kMinSemiSpaceSizeInKB);
    return RoundUp(semi_space_size_in_kb, (1 << kPageSizeBits) / KB);
  }

  // Returns the capacity of the heap in bytes w/o growing. Heap grows when
  // more spaces are needed until it reaches the limit.
  size_t Capacity();

  // Returns the capacity of the old generation.
  size_t OldGenerationCapacity();

  // Returns the amount of memory currently committed for the heap and memory
  // held alive by the unmapper.
  size_t CommittedMemoryOfHeapAndUnmapper();

  // Returns the amount of memory currently committed for the heap.
  size_t CommittedMemory();

  // Returns the amount of memory currently committed for the old space.
  size_t CommittedOldGenerationMemory();

  // Returns the amount of executable memory currently committed for the heap.
  size_t CommittedMemoryExecutable();

  // Returns the amount of phyical memory currently committed for the heap.
  size_t CommittedPhysicalMemory();

  // Returns the maximum amount of memory ever committed for the heap.
  size_t MaximumCommittedMemory() { return maximum_committed_; }

  // Updates the maximum committed memory for the heap. Should be called
  // whenever a space grows.
  void UpdateMaximumCommitted();

  // Returns the available bytes in space w/o growing.
  // Heap doesn't guarantee that it can allocate an object that requires
  // all available bytes. Check MaxHeapObjectSize() instead.
  size_t Available();

  // Returns of size of all objects residing in the heap.
  size_t SizeOfObjects();

  void UpdateSurvivalStatistics(int start_new_space_size);

  inline void IncrementPromotedObjectsSize(size_t object_size) {
    promoted_objects_size_ += object_size;
  }
  inline size_t promoted_objects_size() { return promoted_objects_size_; }

  inline void IncrementSemiSpaceCopiedObjectSize(size_t object_size) {
    semi_space_copied_object_size_ += object_size;
  }
  inline size_t semi_space_copied_object_size() {
    return semi_space_copied_object_size_;
  }

  inline size_t SurvivedNewSpaceObjectSize() {
    return promoted_objects_size_ + semi_space_copied_object_size_;
  }

  inline void IncrementNodesDiedInNewSpace() { nodes_died_in_new_space_++; }

  inline void IncrementNodesCopiedInNewSpace() { nodes_copied_in_new_space_++; }

  inline void IncrementNodesPromoted() { nodes_promoted_++; }

  inline void IncrementYoungSurvivorsCounter(size_t survived) {
    survived_last_scavenge_ = survived;
    survived_since_last_expansion_ += survived;
  }

  inline uint64_t OldGenerationObjectsAndPromotedExternalMemorySize() {
    return OldGenerationSizeOfObjects() + PromotedExternalMemorySize();
  }

  inline void UpdateNewSpaceAllocationCounter();

  inline size_t NewSpaceAllocationCounter();

  // This should be used only for testing.
  void set_new_space_allocation_counter(size_t new_value) {
    new_space_allocation_counter_ = new_value;
  }

  void UpdateOldGenerationAllocationCounter() {
    old_generation_allocation_counter_at_last_gc_ =
        OldGenerationAllocationCounter();
    old_generation_size_at_last_gc_ = 0;
  }

  size_t OldGenerationAllocationCounter() {
    return old_generation_allocation_counter_at_last_gc_ +
           PromotedSinceLastGC();
  }

  // This should be used only for testing.
  void set_old_generation_allocation_counter_at_last_gc(size_t new_value) {
    old_generation_allocation_counter_at_last_gc_ = new_value;
  }

  size_t PromotedSinceLastGC() {
    size_t old_generation_size = OldGenerationSizeOfObjects();
    DCHECK_GE(old_generation_size, old_generation_size_at_last_gc_);
    return old_generation_size - old_generation_size_at_last_gc_;
  }

  // This is called by the sweeper when it discovers more free space
  // than expected at the end of the preceding GC.
  void NotifyRefinedOldGenerationSize(size_t decreased_bytes) {
    if (old_generation_size_at_last_gc_ != 0) {
      // OldGenerationSizeOfObjects() is now smaller by |decreased_bytes|.
      // Adjust old_generation_size_at_last_gc_ too, so that PromotedSinceLastGC
      // continues to increase monotonically, rather than decreasing here.
      DCHECK_GE(old_generation_size_at_last_gc_, decreased_bytes);
      old_generation_size_at_last_gc_ -= decreased_bytes;
    }
  }

  int gc_count() const { return gc_count_; }

  // Returns the size of objects residing in non-new spaces.
  // Excludes external memory held by those objects.
  size_t OldGenerationSizeOfObjects();

  // ===========================================================================
  // Prologue/epilogue callback methods.========================================
  // ===========================================================================

  void AddGCPrologueCallback(v8::Isolate::GCCallbackWithData callback,
                             GCType gc_type_filter, void* data);
  void RemoveGCPrologueCallback(v8::Isolate::GCCallbackWithData callback,
                                void* data);

  void AddGCEpilogueCallback(v8::Isolate::GCCallbackWithData callback,
                             GCType gc_type_filter, void* data);
  void RemoveGCEpilogueCallback(v8::Isolate::GCCallbackWithData callback,
                                void* data);

  void CallGCPrologueCallbacks(GCType gc_type, GCCallbackFlags flags);
  void CallGCEpilogueCallbacks(GCType gc_type, GCCallbackFlags flags);

  // ===========================================================================
  // Allocation methods. =======================================================
  // ===========================================================================

  // Creates a filler object and returns a heap object immediately after it.
  V8_WARN_UNUSED_RESULT HeapObject* PrecedeWithFiller(HeapObject* object,
                                                      int filler_size);

  // Creates a filler object if needed for alignment and returns a heap object
  // immediately after it. If any space is left after the returned object,
  // another filler object is created so the over allocated memory is iterable.
  V8_WARN_UNUSED_RESULT HeapObject* AlignWithFiller(
      HeapObject* object, int object_size, int allocation_size,
      AllocationAlignment alignment);

  // ===========================================================================
  // ArrayBuffer tracking. =====================================================
  // ===========================================================================

  // TODO(gc): API usability: encapsulate mutation of JSArrayBuffer::is_external
  // in the registration/unregistration APIs. Consider dropping the "New" from
  // "RegisterNewArrayBuffer" because one can re-register a previously
  // unregistered buffer, too, and the name is confusing.
  void RegisterNewArrayBuffer(JSArrayBuffer* buffer);
  void UnregisterArrayBuffer(JSArrayBuffer* buffer);

  // ===========================================================================
  // Allocation site tracking. =================================================
  // ===========================================================================

  // Updates the AllocationSite of a given {object}. The entry (including the
  // count) is cached on the local pretenuring feedback.
  inline void UpdateAllocationSite(
      Map* map, HeapObject* object,
      PretenuringFeedbackMap* pretenuring_feedback);

  // Merges local pretenuring feedback into the global one. Note that this
  // method needs to be called after evacuation, as allocation sites may be
  // evacuated and this method resolves forward pointers accordingly.
  void MergeAllocationSitePretenuringFeedback(
      const PretenuringFeedbackMap& local_pretenuring_feedback);

  // ===========================================================================
  // Allocation tracking. ======================================================
  // ===========================================================================

  // Adds {new_space_observer} to new space and {observer} to any other space.
  void AddAllocationObserversToAllSpaces(
      AllocationObserver* observer, AllocationObserver* new_space_observer);

  // Removes {new_space_observer} from new space and {observer} from any other
  // space.
  void RemoveAllocationObserversFromAllSpaces(
      AllocationObserver* observer, AllocationObserver* new_space_observer);

  bool allocation_step_in_progress() { return allocation_step_in_progress_; }
  void set_allocation_step_in_progress(bool val) {
    allocation_step_in_progress_ = val;
  }

  // ===========================================================================
  // Heap object allocation tracking. ==========================================
  // ===========================================================================

  void AddHeapObjectAllocationTracker(HeapObjectAllocationTracker* tracker);
  void RemoveHeapObjectAllocationTracker(HeapObjectAllocationTracker* tracker);
  bool has_heap_object_allocation_tracker() const {
    return !allocation_trackers_.empty();
  }

  // ===========================================================================
  // Retaining path tracking. ==================================================
  // ===========================================================================

  // Adds the given object to the weak table of retaining path targets.
  // On each GC if the marker discovers the object, it will print the retaining
  // path. This requires --track-retaining-path flag.
  void AddRetainingPathTarget(Handle<HeapObject> object,
                              RetainingPathOption option);

  // ===========================================================================
  // Stack frame support. ======================================================
  // ===========================================================================

  // Returns the Code object for a given interior pointer. Returns nullptr if
  // {inner_pointer} is not contained within a Code object.
  Code* GcSafeFindCodeForInnerPointer(Address inner_pointer);

  // Returns true if {addr} is contained within {code} and false otherwise.
  // Mostly useful for debugging.
  bool GcSafeCodeContains(HeapObject* code, Address addr);

// =============================================================================
#ifdef VERIFY_HEAP
  // Verify the heap is in its normal state before or after a GC.
  void Verify();
  void VerifyRememberedSetFor(HeapObject* object);
#endif

#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  void set_allocation_timeout(int timeout) { allocation_timeout_ = timeout; }
#endif

#ifdef DEBUG
  void VerifyCountersAfterSweeping();
  void VerifyCountersBeforeConcurrentSweeping();

  void Print();
  void PrintHandles();

  // Report code statistics.
  void ReportCodeStatistics(const char* title);
#endif
  void* GetRandomMmapAddr() {
    void* result = v8::internal::GetRandomMmapAddr();
#if V8_TARGET_ARCH_X64
#if V8_OS_MACOSX
    // The Darwin kernel [as of macOS 10.12.5] does not clean up page
    // directory entries [PDE] created from mmap or mach_vm_allocate, even
    // after the region is destroyed. Using a virtual address space that is
    // too large causes a leak of about 1 wired [can never be paged out] page
    // per call to mmap(). The page is only reclaimed when the process is
    // killed. Confine the hint to a 32-bit section of the virtual address
    // space. See crbug.com/700928.
    uintptr_t offset =
        reinterpret_cast<uintptr_t>(v8::internal::GetRandomMmapAddr()) &
        kMmapRegionMask;
    result = reinterpret_cast<void*>(mmap_region_base_ + offset);
#endif  // V8_OS_MACOSX
#endif  // V8_TARGET_ARCH_X64
    return result;
  }

  static const char* GarbageCollectionReasonToString(
      GarbageCollectionReason gc_reason);

  // Calculates the nof entries for the full sized number to string cache.
  inline int MaxNumberToStringCacheSize() const;

 private:
  class SkipStoreBufferScope;

  typedef String* (*ExternalStringTableUpdaterCallback)(Heap* heap,
                                                        Object** pointer);

  // External strings table is a place where all external strings are
  // registered.  We need to keep track of such strings to properly
  // finalize them.
  class ExternalStringTable {
   public:
    explicit ExternalStringTable(Heap* heap) : heap_(heap) {}

    // Registers an external string.
    inline void AddString(String* string);
    bool Contains(HeapObject* obj);

    void IterateAll(RootVisitor* v);
    void IterateNewSpaceStrings(RootVisitor* v);
    void PromoteAllNewSpaceStrings();

    // Restores internal invariant and gets rid of collected strings. Must be
    // called after each Iterate*() that modified the strings.
    void CleanUpAll();
    void CleanUpNewSpaceStrings();

    // Finalize all registered external strings and clear tables.
    void TearDown();

    void UpdateNewSpaceReferences(
        Heap::ExternalStringTableUpdaterCallback updater_func);
    void UpdateReferences(
        Heap::ExternalStringTableUpdaterCallback updater_func);

   private:
    void Verify();
    void VerifyNewSpace();

    Heap* const heap_;

    // To speed up scavenge collections new space string are kept
    // separate from old space strings.
    std::vector<Object*> new_space_strings_;
    std::vector<Object*> old_space_strings_;

    DISALLOW_COPY_AND_ASSIGN(ExternalStringTable);
  };

  struct StrongRootsList;

  struct StringTypeTable {
    InstanceType type;
    int size;
    RootIndex index;
  };

  struct ConstantStringTable {
    const char* contents;
    RootIndex index;
  };

  struct StructTable {
    InstanceType type;
    int size;
    RootIndex index;
  };

  struct GCCallbackTuple {
    GCCallbackTuple(v8::Isolate::GCCallbackWithData callback, GCType gc_type,
                    void* data)
        : callback(callback), gc_type(gc_type), data(data) {}

    bool operator==(const GCCallbackTuple& other) const;
    GCCallbackTuple& operator=(const GCCallbackTuple& other);

    v8::Isolate::GCCallbackWithData callback;
    GCType gc_type;
    void* data;
  };

  static const int kInitialStringTableSize = StringTable::kMinCapacity;
  static const int kInitialEvalCacheSize = 64;
  static const int kInitialNumberStringCacheSize = 256;

  static const int kRememberedUnmappedPages = 128;

  static const StringTypeTable string_type_table[];
  static const ConstantStringTable constant_string_table[];
  static const StructTable struct_table[];

  static const int kYoungSurvivalRateHighThreshold = 90;
  static const int kYoungSurvivalRateAllowedDeviation = 15;
  static const int kOldSurvivalRateLowThreshold = 10;

  static const int kMaxMarkCompactsInIdleRound = 7;
  static const int kIdleScavengeThreshold = 5;

  static const int kInitialFeedbackCapacity = 256;

  Heap();

  // Selects the proper allocation space based on the pretenuring decision.
  static AllocationSpace SelectSpace(PretenureFlag pretenure) {
    switch (pretenure) {
      case TENURED_READ_ONLY:
        return RO_SPACE;
      case TENURED:
        return OLD_SPACE;
      case NOT_TENURED:
        return NEW_SPACE;
      default:
        UNREACHABLE();
    }
  }

  static size_t DefaultGetExternallyAllocatedMemoryInBytesCallback() {
    return 0;
  }

#define ROOT_ACCESSOR(type, name, CamelName) \
  inline void set_##name(type* value);
  ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  StoreBuffer* store_buffer() { return store_buffer_; }

  void set_current_gc_flags(int flags) {
    current_gc_flags_ = flags;
  }

  inline bool ShouldReduceMemory() const {
    return (current_gc_flags_ & kReduceMemoryFootprintMask) != 0;
  }

  int NumberOfScavengeTasks();

  // Checks whether a global GC is necessary
  GarbageCollector SelectGarbageCollector(AllocationSpace space,
                                          const char** reason);

  // Make sure there is a filler value behind the top of the new space
  // so that the GC does not confuse some unintialized/stale memory
  // with the allocation memento of the object at the top
  void EnsureFillerObjectAtTop();

  // Ensure that we have swept all spaces in such a way that we can iterate
  // over all objects.  May cause a GC.
  void MakeHeapIterable();

  // Performs garbage collection
  // Returns whether there is a chance another major GC could
  // collect more garbage.
  bool PerformGarbageCollection(
      GarbageCollector collector,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  inline void UpdateOldSpaceLimits();

  bool CreateInitialMaps();
  void CreateInternalAccessorInfoObjects();
  void CreateInitialObjects();

  // These five Create*EntryStub functions are here and forced to not be inlined
  // because of a gcc-4.4 bug that assigns wrong vtable entries.
  V8_NOINLINE void CreateJSEntryStub();
  V8_NOINLINE void CreateJSConstructEntryStub();
  V8_NOINLINE void CreateJSRunMicrotasksEntryStub();

  void CreateFixedStubs();

  // Commits from space if it is uncommitted.
  void EnsureFromSpaceIsCommitted();

  // Uncommit unused semi space.
  bool UncommitFromSpace();

  // Fill in bogus values in from space
  void ZapFromSpace();

  // Zaps the memory of a code object.
  void ZapCodeObject(Address start_address, int size_in_bytes);

  // Deopts all code that contains allocation instruction which are tenured or
  // not tenured. Moreover it clears the pretenuring allocation site statistics.
  void ResetAllAllocationSitesDependentCode(PretenureFlag flag);

  // Evaluates local pretenuring for the old space and calls
  // ResetAllTenuredAllocationSitesDependentCode if too many objects died in
  // the old space.
  void EvaluateOldSpaceLocalPretenuring(uint64_t size_of_objects_before_gc);

  // Record statistics after garbage collection.
  void ReportStatisticsAfterGC();

  // Flush the number to string cache.
  void FlushNumberStringCache();

  void ConfigureInitialOldGenerationSize();

  bool HasLowYoungGenerationAllocationRate();
  bool HasLowOldGenerationAllocationRate();
  double YoungGenerationMutatorUtilization();
  double OldGenerationMutatorUtilization();

  void ReduceNewSpaceSize();

  GCIdleTimeHeapState ComputeHeapState();

  bool PerformIdleTimeAction(GCIdleTimeAction action,
                             GCIdleTimeHeapState heap_state,
                             double deadline_in_ms);

  void IdleNotificationEpilogue(GCIdleTimeAction action,
                                GCIdleTimeHeapState heap_state, double start_ms,
                                double deadline_in_ms);

  int NextAllocationTimeout(int current_timeout = 0);
  inline void UpdateAllocationsHash(HeapObject* object);
  inline void UpdateAllocationsHash(uint32_t value);
  void PrintAllocationsHash();

  void PrintMaxMarkingLimitReached();
  void PrintMaxNewSpaceSizeReached();

  int NextStressMarkingLimit();

  void AddToRingBuffer(const char* string);
  void GetFromRingBuffer(char* buffer);

  void CompactRetainedMaps(WeakArrayList* retained_maps);

  void CollectGarbageOnMemoryPressure();

  void EagerlyFreeExternalMemory();

  bool InvokeNearHeapLimitCallback();

  void ComputeFastPromotionMode();

  // Attempt to over-approximate the weak closure by marking object groups and
  // implicit references from global handles, but don't atomically complete
  // marking. If we continue to mark incrementally, we might have marked
  // objects that die later.
  void FinalizeIncrementalMarkingIncrementally(
      GarbageCollectionReason gc_reason);

  // Returns the timer used for a given GC type.
  // - GCScavenger: young generation GC
  // - GCCompactor: full GC
  // - GCFinalzeMC: finalization of incremental full GC
  // - GCFinalizeMCReduceMemory: finalization of incremental full GC with
  // memory reduction
  HistogramTimer* GCTypeTimer(GarbageCollector collector);
  HistogramTimer* GCTypePriorityTimer(GarbageCollector collector);

  // ===========================================================================
  // Pretenuring. ==============================================================
  // ===========================================================================

  // Pretenuring decisions are made based on feedback collected during new space
  // evacuation. Note that between feedback collection and calling this method
  // object in old space must not move.
  void ProcessPretenuringFeedback();

  // Removes an entry from the global pretenuring storage.
  void RemoveAllocationSitePretenuringFeedback(AllocationSite* site);

  // ===========================================================================
  // Actual GC. ================================================================
  // ===========================================================================

  // Code that should be run before and after each GC.  Includes some
  // reporting/verification activities when compiled with DEBUG set.
  void GarbageCollectionPrologue();
  void GarbageCollectionEpilogue();

  // Performs a major collection in the whole heap.
  void MarkCompact();
  // Performs a minor collection of just the young generation.
  void MinorMarkCompact();

  // Code to be run before and after mark-compact.
  void MarkCompactPrologue();
  void MarkCompactEpilogue();

  // Performs a minor collection in new generation.
  void Scavenge();
  void EvacuateYoungGeneration();

  void UpdateNewSpaceReferencesInExternalStringTable(
      ExternalStringTableUpdaterCallback updater_func);

  void UpdateReferencesInExternalStringTable(
      ExternalStringTableUpdaterCallback updater_func);

  void ProcessAllWeakReferences(WeakObjectRetainer* retainer);
  void ProcessYoungWeakReferences(WeakObjectRetainer* retainer);
  void ProcessNativeContexts(WeakObjectRetainer* retainer);
  void ProcessAllocationSites(WeakObjectRetainer* retainer);
  void ProcessWeakListRoots(WeakObjectRetainer* retainer);

  // ===========================================================================
  // GC statistics. ============================================================
  // ===========================================================================

  inline size_t OldGenerationSpaceAvailable() {
    if (old_generation_allocation_limit_ <=
        OldGenerationObjectsAndPromotedExternalMemorySize())
      return 0;
    return old_generation_allocation_limit_ -
           static_cast<size_t>(
               OldGenerationObjectsAndPromotedExternalMemorySize());
  }

  // We allow incremental marking to overshoot the allocation limit for
  // performace reasons. If the overshoot is too large then we are more
  // eager to finalize incremental marking.
  inline bool AllocationLimitOvershotByLargeMargin() {
    // This guards against too eager finalization in small heaps.
    // The number is chosen based on v8.browsing_mobile on Nexus 7v2.
    size_t kMarginForSmallHeaps = 32u * MB;
    if (old_generation_allocation_limit_ >=
        OldGenerationObjectsAndPromotedExternalMemorySize())
      return false;
    uint64_t overshoot = OldGenerationObjectsAndPromotedExternalMemorySize() -
                         old_generation_allocation_limit_;
    // Overshoot margin is 50% of allocation limit or half-way to the max heap
    // with special handling of small heaps.
    uint64_t margin =
        Min(Max(old_generation_allocation_limit_ / 2, kMarginForSmallHeaps),
            (max_old_generation_size_ - old_generation_allocation_limit_) / 2);
    return overshoot >= margin;
  }

  void UpdateTotalGCTime(double duration);

  bool MaximumSizeScavenge() { return maximum_size_scavenges_ > 0; }

  bool IsIneffectiveMarkCompact(size_t old_generation_size,
                                double mutator_utilization);
  void CheckIneffectiveMarkCompact(size_t old_generation_size,
                                   double mutator_utilization);

  inline void IncrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                                 size_t amount);

  inline void DecrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                                 size_t amount);

  // ===========================================================================
  // Growing strategy. =========================================================
  // ===========================================================================

  HeapController* heap_controller() { return heap_controller_; }
  MemoryReducer* memory_reducer() { return memory_reducer_; }

  // For some webpages RAIL mode does not switch from PERFORMANCE_LOAD.
  // This constant limits the effect of load RAIL mode on GC.
  // The value is arbitrary and chosen as the largest load time observed in
  // v8 browsing benchmarks.
  static const int kMaxLoadTimeMs = 7000;

  bool ShouldOptimizeForLoadTime();

  size_t old_generation_allocation_limit() const {
    return old_generation_allocation_limit_;
  }

  bool always_allocate() { return always_allocate_scope_count_ != 0; }

  bool CanExpandOldGeneration(size_t size);

  bool ShouldExpandOldGenerationOnSlowAllocation();

  enum class HeapGrowingMode { kSlow, kConservative, kMinimal, kDefault };

  HeapGrowingMode CurrentHeapGrowingMode();

  enum class IncrementalMarkingLimit { kNoLimit, kSoftLimit, kHardLimit };
  IncrementalMarkingLimit IncrementalMarkingLimitReached();

  // ===========================================================================
  // Idle notification. ========================================================
  // ===========================================================================

  bool RecentIdleNotificationHappened();
  void ScheduleIdleScavengeIfNeeded(int bytes_allocated);

  // ===========================================================================
  // HeapIterator helpers. =====================================================
  // ===========================================================================

  void heap_iterator_start() { heap_iterator_depth_++; }

  void heap_iterator_end() { heap_iterator_depth_--; }

  bool in_heap_iterator() { return heap_iterator_depth_ > 0; }

  // ===========================================================================
  // Allocation methods. =======================================================
  // ===========================================================================

  // Allocates a JS Map in the heap.
  V8_WARN_UNUSED_RESULT AllocationResult
  AllocateMap(InstanceType instance_type, int instance_size,
              ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
              int inobject_properties = 0);

  // Allocate an uninitialized object.  The memory is non-executable if the
  // hardware and OS allow.  This is the single choke-point for allocations
  // performed by the runtime and should not be bypassed (to extend this to
  // inlined allocations, use the Heap::DisableInlineAllocation() support).
  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRaw(
      int size_in_bytes, AllocationSpace space,
      AllocationAlignment aligment = kWordAligned);

  // This method will try to perform an allocation of a given size in a given
  // space. If the allocation fails, a regular full garbage collection is
  // triggered and the allocation is retried. This is performed multiple times.
  // If after that retry procedure the allocation still fails nullptr is
  // returned.
  HeapObject* AllocateRawWithLightRetry(
      int size, AllocationSpace space,
      AllocationAlignment alignment = kWordAligned);

  // This method will try to perform an allocation of a given size in a given
  // space. If the allocation fails, a regular full garbage collection is
  // triggered and the allocation is retried. This is performed multiple times.
  // If after that retry procedure the allocation still fails a "hammer"
  // garbage collection is triggered which tries to significantly reduce memory.
  // If the allocation still fails after that a fatal error is thrown.
  HeapObject* AllocateRawWithRetryOrFail(
      int size, AllocationSpace space,
      AllocationAlignment alignment = kWordAligned);
  HeapObject* AllocateRawCodeInLargeObjectSpace(int size);

  // Allocates a heap object based on the map.
  V8_WARN_UNUSED_RESULT AllocationResult Allocate(Map* map,
                                                  AllocationSpace space);

  // Takes a code object and checks if it is on memory which is not subject to
  // compaction. This method will return a new code object on an immovable
  // memory location if the original code object was movable.
  HeapObject* EnsureImmovableCode(HeapObject* heap_object, int object_size);

  // Allocates a partial map for bootstrapping.
  V8_WARN_UNUSED_RESULT AllocationResult
  AllocatePartialMap(InstanceType instance_type, int instance_size);

  void FinalizePartialMap(Map* map);

  // Allocate empty fixed typed array of given type.
  V8_WARN_UNUSED_RESULT AllocationResult
  AllocateEmptyFixedTypedArray(ExternalArrayType array_type);

  void set_force_oom(bool value) { force_oom_ = value; }

  // ===========================================================================
  // Retaining path tracing ====================================================
  // ===========================================================================

  void AddRetainer(HeapObject* retainer, HeapObject* object);
  void AddEphemeronRetainer(HeapObject* retainer, HeapObject* object);
  void AddRetainingRoot(Root root, HeapObject* object);
  // Returns true if the given object is a target of retaining path tracking.
  // Stores the option corresponding to the object in the provided *option.
  bool IsRetainingPathTarget(HeapObject* object, RetainingPathOption* option);
  void PrintRetainingPath(HeapObject* object, RetainingPathOption option);

  // The amount of external memory registered through the API.
  int64_t external_memory_ = 0;

  // The limit when to trigger memory pressure from the API.
  int64_t external_memory_limit_ = kExternalAllocationSoftLimit;

  // Caches the amount of external memory registered at the last MC.
  int64_t external_memory_at_last_mark_compact_ = 0;

  // The amount of memory that has been freed concurrently.
  std::atomic<intptr_t> external_memory_concurrently_freed_{0};

  // This can be calculated directly from a pointer to the heap; however, it is
  // more expedient to get at the isolate directly from within Heap methods.
  Isolate* isolate_ = nullptr;

  RootsTable roots_;

  // This table is accessed from builtin code compiled into the snapshot, and
  // thus its offset from roots_ must remain static. This is verified in
  // Isolate::Init() using runtime checks.
  static constexpr int kRootsExternalReferenceTableOffset =
      static_cast<int>(RootIndex::kRootListLength) * kPointerSize;
  ExternalReferenceTable external_reference_table_;

  // As external references above, builtins are accessed through an offset from
  // the roots register. Its offset from roots_ must remain static. This is
  // verified in Isolate::Init() using runtime checks.
  static constexpr int kRootsBuiltinsOffset =
      kRootsExternalReferenceTableOffset +
      ExternalReferenceTable::SizeInBytes();
  Object* builtins_[Builtins::builtin_count];

  // kRootRegister may be used to address any location that starts at the
  // Isolate and ends at this point. Fields past this point are not guaranteed
  // to live at a static offset from kRootRegister.
  static constexpr int kRootRegisterAddressableEndOffset =
      kRootsBuiltinsOffset + Builtins::builtin_count * kPointerSize;

  size_t code_range_size_ = 0;
  size_t max_semi_space_size_ = 8 * (kPointerSize / 4) * MB;
  size_t initial_semispace_size_ = kMinSemiSpaceSizeInKB * KB;
  size_t max_old_generation_size_ = 700ul * (kPointerSize / 4) * MB;
  size_t initial_max_old_generation_size_;
  size_t initial_old_generation_size_;
  bool old_generation_size_configured_ = false;
  size_t maximum_committed_ = 0;

  // Backing store bytes (array buffers and external strings).
  std::atomic<size_t> backing_store_bytes_{0};

  // For keeping track of how much data has survived
  // scavenge since last new space expansion.
  size_t survived_since_last_expansion_ = 0;

  // ... and since the last scavenge.
  size_t survived_last_scavenge_ = 0;

  // This is not the depth of nested AlwaysAllocateScope's but rather a single
  // count, as scopes can be acquired from multiple tasks (read: threads).
  std::atomic<size_t> always_allocate_scope_count_{0};

  // Stores the memory pressure level that set by MemoryPressureNotification
  // and reset by a mark-compact garbage collection.
  std::atomic<MemoryPressureLevel> memory_pressure_level_;

  std::vector<std::pair<v8::NearHeapLimitCallback, void*> >
      near_heap_limit_callbacks_;

  // For keeping track of context disposals.
  int contexts_disposed_ = 0;

  // The length of the retained_maps array at the time of context disposal.
  // This separates maps in the retained_maps array that were created before
  // and after context disposal.
  int number_of_disposed_maps_ = 0;

  NewSpace* new_space_ = nullptr;
  OldSpace* old_space_ = nullptr;
  CodeSpace* code_space_ = nullptr;
  MapSpace* map_space_ = nullptr;
  LargeObjectSpace* lo_space_ = nullptr;
  NewLargeObjectSpace* new_lo_space_ = nullptr;
  ReadOnlySpace* read_only_space_ = nullptr;
  // Map from the space id to the space.
  Space* space_[LAST_SPACE + 1];

  // Determines whether code space is write-protected. This is essentially a
  // race-free copy of the {FLAG_write_protect_code_memory} flag.
  bool write_protect_code_memory_ = false;

  // Holds the number of open CodeSpaceMemoryModificationScopes.
  uintptr_t code_space_memory_modification_scope_depth_ = 0;

  HeapState gc_state_ = NOT_IN_GC;

  int gc_post_processing_depth_ = 0;

  // Returns the amount of external memory registered since last global gc.
  uint64_t PromotedExternalMemorySize();

  // How many "runtime allocations" happened.
  uint32_t allocations_count_ = 0;

  // Running hash over allocations performed.
  uint32_t raw_allocations_hash_ = 0;

  // Starts marking when stress_marking_percentage_% of the marking start limit
  // is reached.
  int stress_marking_percentage_ = 0;

  // Observer that causes more frequent checks for reached incremental marking
  // limit.
  AllocationObserver* stress_marking_observer_ = nullptr;

  // Observer that can cause early scavenge start.
  StressScavengeObserver* stress_scavenge_observer_ = nullptr;

  bool allocation_step_in_progress_ = false;

  // The maximum percent of the marking limit reached wihout causing marking.
  // This is tracked when specyfing --fuzzer-gc-analysis.
  double max_marking_limit_reached_ = 0.0;

  // How many mark-sweep collections happened.
  unsigned int ms_count_ = 0;

  // How many gc happened.
  unsigned int gc_count_ = 0;

  // The number of Mark-Compact garbage collections that are considered as
  // ineffective. See IsIneffectiveMarkCompact() predicate.
  int consecutive_ineffective_mark_compacts_ = 0;

  static const uintptr_t kMmapRegionMask = 0xFFFFFFFFu;
  uintptr_t mmap_region_base_ = 0;

  // For post mortem debugging.
  int remembered_unmapped_pages_index_ = 0;
  Address remembered_unmapped_pages_[kRememberedUnmappedPages];

  // Limit that triggers a global GC on the next (normally caused) GC.  This
  // is checked when we have already decided to do a GC to help determine
  // which collector to invoke, before expanding a paged space in the old
  // generation and on every allocation in large object space.
  size_t old_generation_allocation_limit_;

  // Indicates that inline bump-pointer allocation has been globally disabled
  // for all spaces. This is used to disable allocations in generated code.
  bool inline_allocation_disabled_ = false;

  // Weak list heads, threaded through the objects.
  // List heads are initialized lazily and contain the undefined_value at start.
  Object* native_contexts_list_;
  Object* allocation_sites_list_;

  std::vector<GCCallbackTuple> gc_epilogue_callbacks_;
  std::vector<GCCallbackTuple> gc_prologue_callbacks_;

  GetExternallyAllocatedMemoryInBytesCallback external_memory_callback_;

  int deferred_counters_[v8::Isolate::kUseCounterFeatureCount];

  size_t promoted_objects_size_ = 0;
  double promotion_ratio_ = 0.0;
  double promotion_rate_ = 0.0;
  size_t semi_space_copied_object_size_ = 0;
  size_t previous_semi_space_copied_object_size_ = 0;
  double semi_space_copied_rate_ = 0.0;
  int nodes_died_in_new_space_ = 0;
  int nodes_copied_in_new_space_ = 0;
  int nodes_promoted_ = 0;

  // This is the pretenuring trigger for allocation sites that are in maybe
  // tenure state. When we switched to the maximum new space size we deoptimize
  // the code that belongs to the allocation site and derive the lifetime
  // of the allocation site.
  unsigned int maximum_size_scavenges_ = 0;

  // Total time spent in GC.
  double total_gc_time_ms_;

  // Last time an idle notification happened.
  double last_idle_notification_time_ = 0.0;

  // Last time a garbage collection happened.
  double last_gc_time_ = 0.0;

  GCTracer* tracer_ = nullptr;
  MarkCompactCollector* mark_compact_collector_ = nullptr;
  MinorMarkCompactCollector* minor_mark_compact_collector_ = nullptr;
  ScavengerCollector* scavenger_collector_ = nullptr;
  ArrayBufferCollector* array_buffer_collector_ = nullptr;
  MemoryAllocator* memory_allocator_ = nullptr;
  StoreBuffer* store_buffer_ = nullptr;
  HeapController* heap_controller_ = nullptr;
  IncrementalMarking* incremental_marking_ = nullptr;
  ConcurrentMarking* concurrent_marking_ = nullptr;
  GCIdleTimeHandler* gc_idle_time_handler_ = nullptr;
  MemoryReducer* memory_reducer_ = nullptr;
  ObjectStats* live_object_stats_ = nullptr;
  ObjectStats* dead_object_stats_ = nullptr;
  ScavengeJob* scavenge_job_ = nullptr;
  AllocationObserver* idle_scavenge_observer_ = nullptr;
  LocalEmbedderHeapTracer* local_embedder_heap_tracer_ = nullptr;
  StrongRootsList* strong_roots_list_ = nullptr;

  // This counter is increased before each GC and never reset.
  // To account for the bytes allocated since the last GC, use the
  // NewSpaceAllocationCounter() function.
  size_t new_space_allocation_counter_ = 0;

  // This counter is increased before each GC and never reset. To
  // account for the bytes allocated since the last GC, use the
  // OldGenerationAllocationCounter() function.
  size_t old_generation_allocation_counter_at_last_gc_ = 0;

  // The size of objects in old generation after the last MarkCompact GC.
  size_t old_generation_size_at_last_gc_ = 0;

  // The feedback storage is used to store allocation sites (keys) and how often
  // they have been visited (values) by finding a memento behind an object. The
  // storage is only alive temporary during a GC. The invariant is that all
  // pointers in this map are already fixed, i.e., they do not point to
  // forwarding pointers.
  PretenuringFeedbackMap global_pretenuring_feedback_;

  char trace_ring_buffer_[kTraceRingBufferSize];

  // Used as boolean.
  uint8_t is_marking_flag_ = 0;

  // If it's not full then the data is from 0 to ring_buffer_end_.  If it's
  // full then the data is from ring_buffer_end_ to the end of the buffer and
  // from 0 to ring_buffer_end_.
  bool ring_buffer_full_ = false;
  size_t ring_buffer_end_ = 0;

  // Flag is set when the heap has been configured.  The heap can be repeatedly
  // configured through the API until it is set up.
  bool configured_ = false;

  // Currently set GC flags that are respected by all GC components.
  int current_gc_flags_ = Heap::kNoGCFlags;

  // Currently set GC callback flags that are used to pass information between
  // the embedder and V8's GC.
  GCCallbackFlags current_gc_callback_flags_;

  ExternalStringTable external_string_table_;

  base::Mutex relocation_mutex_;

  int gc_callbacks_depth_ = 0;

  bool deserialization_complete_ = false;

  // The depth of HeapIterator nestings.
  int heap_iterator_depth_ = 0;

  bool fast_promotion_mode_ = false;

  // Used for testing purposes.
  bool force_oom_ = false;
  bool delay_sweeper_tasks_for_testing_ = false;

  HeapObject* pending_layout_change_object_ = nullptr;

  base::Mutex unprotected_memory_chunks_mutex_;
  std::unordered_set<MemoryChunk*> unprotected_memory_chunks_;
  bool unprotected_memory_chunks_registry_enabled_ = false;

#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  // If the --gc-interval flag is set to a positive value, this
  // variable holds the value indicating the number of allocations
  // remain until the next failure and garbage collection.
  int allocation_timeout_ = 0;
#endif  // V8_ENABLE_ALLOCATION_TIMEOUT

  std::map<HeapObject*, HeapObject*> retainer_;
  std::map<HeapObject*, Root> retaining_root_;
  // If an object is retained by an ephemeron, then the retaining key of the
  // ephemeron is stored in this map.
  std::map<HeapObject*, HeapObject*> ephemeron_retainer_;
  // For each index inthe retaining_path_targets_ array this map
  // stores the option of the corresponding target.
  std::map<int, RetainingPathOption> retaining_path_target_option_;

  std::vector<HeapObjectAllocationTracker*> allocation_trackers_;

  // Classes in "heap" can be friends.
  friend class AlwaysAllocateScope;
  friend class ArrayBufferCollector;
  friend class ConcurrentMarking;
  friend class EphemeronHashTableMarkingTask;
  friend class GCCallbacksScope;
  friend class GCTracer;
  friend class MemoryController;
  friend class HeapIterator;
  friend class IdleScavengeObserver;
  friend class IncrementalMarking;
  friend class IncrementalMarkingJob;
  friend class LargeObjectSpace;
  template <FixedArrayVisitationMode fixed_array_mode,
            TraceRetainingPathMode retaining_path_mode, typename MarkingState>
  friend class MarkingVisitor;
  friend class MarkCompactCollector;
  friend class MarkCompactCollectorBase;
  friend class MinorMarkCompactCollector;
  friend class NewSpace;
  friend class ObjectStatsCollector;
  friend class Page;
  friend class PagedSpace;
  friend class Scavenger;
  friend class ScavengerCollector;
  friend class Space;
  friend class StoreBuffer;
  friend class Sweeper;
  friend class heap::TestMemoryAllocatorScope;

  // The allocator interface.
  friend class Factory;

  // The Isolate constructs us.
  friend class Isolate;

  // Used in cctest.
  friend class heap::HeapTester;

  FRIEND_TEST(HeapControllerTest, OldGenerationAllocationLimit);
  FRIEND_TEST(HeapTest, ExternalLimitDefault);
  FRIEND_TEST(HeapTest, ExternalLimitStaysAboveDefaultForExplicitHandling);
  DISALLOW_COPY_AND_ASSIGN(Heap);
};


class HeapStats {
 public:
  static const int kStartMarker = 0xDECADE00;
  static const int kEndMarker = 0xDECADE01;

  intptr_t* start_marker;                  //  0
  size_t* ro_space_size;                   //  1
  size_t* ro_space_capacity;               //  2
  size_t* new_space_size;                  //  3
  size_t* new_space_capacity;              //  4
  size_t* old_space_size;                  //  5
  size_t* old_space_capacity;              //  6
  size_t* code_space_size;                 //  7
  size_t* code_space_capacity;             //  8
  size_t* map_space_size;                  //  9
  size_t* map_space_capacity;              // 10
  size_t* lo_space_size;                   // 11
  size_t* global_handle_count;             // 12
  size_t* weak_global_handle_count;        // 13
  size_t* pending_global_handle_count;     // 14
  size_t* near_death_global_handle_count;  // 15
  size_t* free_global_handle_count;        // 16
  size_t* memory_allocator_size;           // 17
  size_t* memory_allocator_capacity;       // 18
  size_t* malloced_memory;                 // 19
  size_t* malloced_peak_memory;            // 20
  size_t* objects_per_type;                // 21
  size_t* size_per_type;                   // 22
  int* os_error;                           // 23
  char* last_few_messages;                 // 24
  char* js_stacktrace;                     // 25
  intptr_t* end_marker;                    // 26
};


class AlwaysAllocateScope {
 public:
  explicit inline AlwaysAllocateScope(Isolate* isolate);
  inline ~AlwaysAllocateScope();

 private:
  Heap* heap_;
};

// The CodeSpaceMemoryModificationScope can only be used by the main thread.
class CodeSpaceMemoryModificationScope {
 public:
  explicit inline CodeSpaceMemoryModificationScope(Heap* heap);
  inline ~CodeSpaceMemoryModificationScope();

 private:
  Heap* heap_;
};

// The CodePageCollectionMemoryModificationScope can only be used by the main
// thread. It will not be enabled if a CodeSpaceMemoryModificationScope is
// already active.
class CodePageCollectionMemoryModificationScope {
 public:
  explicit inline CodePageCollectionMemoryModificationScope(Heap* heap);
  inline ~CodePageCollectionMemoryModificationScope();

 private:
  Heap* heap_;
};

// The CodePageMemoryModificationScope does not check if tansitions to
// writeable and back to executable are actually allowed, i.e. the MemoryChunk
// was registered to be executable. It can be used by concurrent threads.
class CodePageMemoryModificationScope {
 public:
  explicit inline CodePageMemoryModificationScope(MemoryChunk* chunk);
  inline ~CodePageMemoryModificationScope();

 private:
  MemoryChunk* chunk_;
  bool scope_active_;

  // Disallow any GCs inside this scope, as a relocation of the underlying
  // object would change the {MemoryChunk} that this scope targets.
  DisallowHeapAllocation no_heap_allocation_;
};

// Visitor class to verify interior pointers in spaces that do not contain
// or care about intergenerational references. All heap object pointers have to
// point into the heap to a location that has a map pointer at its first word.
// Caveat: Heap::Contains is an approximation because it can return true for
// objects in a heap space but above the allocation pointer.
class VerifyPointersVisitor : public ObjectVisitor, public RootVisitor {
 public:
  explicit VerifyPointersVisitor(Heap* heap) : heap_(heap) {}
  void VisitPointers(HeapObject* host, Object** start, Object** end) override;
  void VisitPointers(HeapObject* host, MaybeObject** start,
                     MaybeObject** end) override;
  void VisitRootPointers(Root root, const char* description, Object** start,
                         Object** end) override;

 protected:
  virtual void VerifyPointers(HeapObject* host, MaybeObject** start,
                              MaybeObject** end);

  Heap* heap_;
};


// Verify that all objects are Smis.
class VerifySmisVisitor : public RootVisitor {
 public:
  void VisitRootPointers(Root root, const char* description, Object** start,
                         Object** end) override;
};

// Space iterator for iterating over all the paged spaces of the heap: Map
// space, old space, code space and optionally read only space. Returns each
// space in turn, and null when it is done.
class V8_EXPORT_PRIVATE PagedSpaces {
 public:
  enum class SpacesSpecifier { kSweepablePagedSpaces, kAllPagedSpaces };

  explicit PagedSpaces(Heap* heap, SpacesSpecifier specifier =
                                       SpacesSpecifier::kSweepablePagedSpaces)
      : heap_(heap),
        counter_(specifier == SpacesSpecifier::kAllPagedSpaces ? RO_SPACE
                                                               : OLD_SPACE) {}
  PagedSpace* next();

 private:
  Heap* heap_;
  int counter_;
};


class SpaceIterator : public Malloced {
 public:
  explicit SpaceIterator(Heap* heap);
  virtual ~SpaceIterator();

  bool has_next();
  Space* next();

 private:
  Heap* heap_;
  int current_space_;         // from enum AllocationSpace.
};


// A HeapIterator provides iteration over the whole heap. It
// aggregates the specific iterators for the different spaces as
// these can only iterate over one space only.
//
// HeapIterator ensures there is no allocation during its lifetime
// (using an embedded DisallowHeapAllocation instance).
//
// HeapIterator can skip free list nodes (that is, de-allocated heap
// objects that still remain in the heap). As implementation of free
// nodes filtering uses GC marks, it can't be used during MS/MC GC
// phases. Also, it is forbidden to interrupt iteration in this mode,
// as this will leave heap objects marked (and thus, unusable).
class HeapIterator {
 public:
  enum HeapObjectsFiltering { kNoFiltering, kFilterUnreachable };

  explicit HeapIterator(Heap* heap,
                        HeapObjectsFiltering filtering = kNoFiltering);
  ~HeapIterator();

  HeapObject* next();

 private:
  HeapObject* NextObject();

  DisallowHeapAllocation no_heap_allocation_;

  Heap* heap_;
  HeapObjectsFiltering filtering_;
  HeapObjectsFilter* filter_;
  // Space iterator for iterating all the spaces.
  SpaceIterator* space_iterator_;
  // Object iterator for the space currently being iterated.
  std::unique_ptr<ObjectIterator> object_iterator_;
};

// Abstract base class for checking whether a weak object should be retained.
class WeakObjectRetainer {
 public:
  virtual ~WeakObjectRetainer() = default;

  // Return whether this object should be retained. If nullptr is returned the
  // object has no references. Otherwise the address of the retained object
  // should be returned as in some GC situations the object has been moved.
  virtual Object* RetainAs(Object* object) = 0;
};

// -----------------------------------------------------------------------------
// Allows observation of allocations.
class AllocationObserver {
 public:
  explicit AllocationObserver(intptr_t step_size)
      : step_size_(step_size), bytes_to_next_step_(step_size) {
    DCHECK_LE(kPointerSize, step_size);
  }
  virtual ~AllocationObserver() = default;

  // Called each time the observed space does an allocation step. This may be
  // more frequently than the step_size we are monitoring (e.g. when there are
  // multiple observers, or when page or space boundary is encountered.)
  void AllocationStep(int bytes_allocated, Address soon_object, size_t size);

 protected:
  intptr_t step_size() const { return step_size_; }
  intptr_t bytes_to_next_step() const { return bytes_to_next_step_; }

  // Pure virtual method provided by the subclasses that gets called when at
  // least step_size bytes have been allocated. soon_object is the address just
  // allocated (but not yet initialized.) size is the size of the object as
  // requested (i.e. w/o the alignment fillers). Some complexities to be aware
  // of:
  // 1) soon_object will be nullptr in cases where we end up observing an
  //    allocation that happens to be a filler space (e.g. page boundaries.)
  // 2) size is the requested size at the time of allocation. Right-trimming
  //    may change the object size dynamically.
  // 3) soon_object may actually be the first object in an allocation-folding
  //    group. In such a case size is the size of the group rather than the
  //    first object.
  virtual void Step(int bytes_allocated, Address soon_object, size_t size) = 0;

  // Subclasses can override this method to make step size dynamic.
  virtual intptr_t GetNextStepSize() { return step_size_; }

  intptr_t step_size_;
  intptr_t bytes_to_next_step_;

 private:
  friend class Space;
  DISALLOW_COPY_AND_ASSIGN(AllocationObserver);
};

V8_EXPORT_PRIVATE const char* AllocationSpaceName(AllocationSpace space);

// -----------------------------------------------------------------------------
// Allows observation of heap object allocations.
class HeapObjectAllocationTracker {
 public:
  virtual void AllocationEvent(Address addr, int size) = 0;
  virtual void MoveEvent(Address from, Address to, int size) {}
  virtual void UpdateObjectSizeEvent(Address addr, int size) {}
  virtual ~HeapObjectAllocationTracker() = default;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_H_
