// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_H_
#define V8_HEAP_HEAP_H_

#include <atomic>
#include <cmath>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!
#include "include/v8-internal.h"
#include "include/v8.h"
#include "src/base/atomic-utils.h"
#include "src/base/enum-set.h"
#include "src/base/platform/condition-variable.h"
#include "src/builtins/accessors.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/heap/allocation-observer.h"
#include "src/init/heap-symbols.h"
#include "src/objects/allocation-site.h"
#include "src/objects/fixed-array.h"
#include "src/objects/hash-table.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-array-buffer.h"
#include "src/objects/objects.h"
#include "src/objects/smi.h"
#include "src/objects/visitors.h"
#include "src/roots/roots.h"
#include "src/utils/allocation.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {

namespace debug {
using OutOfMemoryCallback = void (*)(void* data);
}  // namespace debug

namespace internal {

namespace heap {
class HeapTester;
class TestMemoryAllocatorScope;
}  // namespace heap

namespace third_party_heap {
class Heap;
}  // namespace third_party_heap

class IncrementalMarking;
class BackingStore;
class JSArrayBuffer;
class JSPromise;
class NativeContext;

using v8::MemoryPressureLevel;

class ArrayBufferCollector;
class ArrayBufferSweeper;
class BasicMemoryChunk;
class CodeLargeObjectSpace;
class CollectionBarrier;
class ConcurrentMarking;
class CppHeap;
class GCIdleTimeHandler;
class GCIdleTimeHeapState;
class GCTracer;
class GlobalSafepoint;
class HeapObjectAllocationTracker;
class HeapObjectsFilter;
class HeapStats;
class Isolate;
class JSFinalizationRegistry;
class LocalEmbedderHeapTracer;
class LocalHeap;
class MarkingBarrier;
class MemoryAllocator;
class MemoryChunk;
class MemoryMeasurement;
class MemoryReducer;
class MinorMarkCompactCollector;
class ObjectIterator;
class ObjectStats;
class Page;
class PagedSpace;
class ReadOnlyHeap;
class RootVisitor;
class SafepointScope;
class ScavengeJob;
class Scavenger;
class ScavengerCollector;
class SharedReadOnlySpace;
class Space;
class StressScavengeObserver;
class TimedHistogram;
class WeakObjectRetainer;

enum ArrayStorageAllocationMode {
  DONT_INITIALIZE_ARRAY_ELEMENTS,
  INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE
};

enum class ClearRecordedSlots { kYes, kNo };

enum class InvalidateRecordedSlots { kYes, kNo };

enum class ClearFreedMemoryMode { kClearFreedMemory, kDontClearFreedMemory };

enum ExternalBackingStoreType { kArrayBuffer, kExternalString, kNumTypes };

enum class RetainingPathOption { kDefault, kTrackEphemeronPath };

enum class AllocationOrigin {
  kGeneratedCode = 0,
  kRuntime = 1,
  kGC = 2,
  kFirstAllocationOrigin = kGeneratedCode,
  kLastAllocationOrigin = kGC,
  kNumberOfAllocationOrigins = kLastAllocationOrigin + 1
};

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
  kTask = 12,
  kLastResort = 13,
  kLowMemoryNotification = 14,
  kMakeHeapIterable = 15,
  kMemoryPressure = 16,
  kMemoryReducer = 17,
  kRuntime = 18,
  kSamplingProfiler = 19,
  kSnapshotCreator = 20,
  kTesting = 21,
  kExternalFinalize = 22,
  kGlobalAllocationLimit = 23,
  kMeasureMemory = 24,
  kBackgroundAllocationFailure = 25,
  // If you add new items here, then update the incremental_marking_reason,
  // mark_compact_reason, and scavenge_reason counters in counters.h.
  // Also update src/tools/metrics/histograms/enums.xml in chromium.
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

enum class GCIdleTimeAction : uint8_t;

enum class SkipRoot {
  kExternalStringTable,
  kGlobalHandles,
  kOldGeneration,
  kStack,
  kUnserializable,
  kWeak
};

class StrongRootsEntry {
  StrongRootsEntry() = default;

  FullObjectSlot start;
  FullObjectSlot end;

  StrongRootsEntry* prev;
  StrongRootsEntry* next;

  friend class Heap;
};

class AllocationResult {
 public:
  static inline AllocationResult Retry(AllocationSpace space = NEW_SPACE) {
    return AllocationResult(space);
  }

  // Implicit constructor from Object.
  AllocationResult(Object object)  // NOLINT
      : object_(object) {
    // AllocationResults can't return Smis, which are used to represent
    // failure and the space to retry in.
    CHECK(!object.IsSmi());
  }

  AllocationResult() : object_(Smi::FromInt(NEW_SPACE)) {}

  inline bool IsRetry() { return object_.IsSmi(); }
  inline HeapObject ToObjectChecked();
  inline HeapObject ToObject();
  inline Address ToAddress();
  inline AllocationSpace RetrySpace();

  template <typename T>
  bool To(T* obj) {
    if (IsRetry()) return false;
    *obj = T::cast(object_);
    return true;
  }

 private:
  explicit AllocationResult(AllocationSpace space)
      : object_(Smi::FromInt(static_cast<int>(space))) {}

  Object object_;
};

STATIC_ASSERT(sizeof(AllocationResult) == kSystemPointerSize);

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

using EphemeronRememberedSet =
    std::unordered_map<EphemeronHashTable, std::unordered_set<int>,
                       Object::Hasher>;

using CollectionEpoch = uint32_t;

class Heap {
 public:
  // Stores ephemeron entries where the EphemeronHashTable is in old-space,
  // and the key of the entry is in new-space. Such keys do not appear in the
  // usual OLD_TO_NEW remembered set.
  EphemeronRememberedSet ephemeron_remembered_set_;
  enum FindMementoMode { kForRuntime, kForGC };

  enum class HeapGrowingMode { kSlow, kConservative, kMinimal, kDefault };

  enum HeapState {
    NOT_IN_GC,
    SCAVENGE,
    MARK_COMPACT,
    MINOR_MARK_COMPACT,
    TEAR_DOWN
  };

  // Emits GC events for DevTools timeline.
  class V8_NODISCARD DevToolsTraceEventScope {
   public:
    DevToolsTraceEventScope(Heap* heap, const char* event_name,
                            const char* event_type);
    ~DevToolsTraceEventScope();

   private:
    Heap* heap_;
    const char* event_name_;
  };

  class ExternalMemoryAccounting {
   public:
    int64_t total() { return total_.load(std::memory_order_relaxed); }
    int64_t limit() { return limit_.load(std::memory_order_relaxed); }
    int64_t low_since_mark_compact() {
      return low_since_mark_compact_.load(std::memory_order_relaxed);
    }

    void ResetAfterGC() {
      set_low_since_mark_compact(total());
      set_limit(total() + kExternalAllocationSoftLimit);
    }

    int64_t Update(int64_t delta) {
      const int64_t amount =
          total_.fetch_add(delta, std::memory_order_relaxed) + delta;
      if (amount < low_since_mark_compact()) {
        set_low_since_mark_compact(amount);
        set_limit(amount + kExternalAllocationSoftLimit);
      }
      return amount;
    }

    int64_t AllocatedSinceMarkCompact() {
      int64_t total_bytes = total();
      int64_t low_since_mark_compact_bytes = low_since_mark_compact();

      if (total_bytes <= low_since_mark_compact_bytes) {
        return 0;
      }
      return static_cast<uint64_t>(total_bytes - low_since_mark_compact_bytes);
    }

   private:
    void set_total(int64_t value) {
      total_.store(value, std::memory_order_relaxed);
    }

    void set_limit(int64_t value) {
      limit_.store(value, std::memory_order_relaxed);
    }

    void set_low_since_mark_compact(int64_t value) {
      low_since_mark_compact_.store(value, std::memory_order_relaxed);
    }

    // The amount of external memory registered through the API.
    std::atomic<int64_t> total_{0};

    // The limit when to trigger memory pressure from the API.
    std::atomic<int64_t> limit_{kExternalAllocationSoftLimit};

    // Caches the amount of external memory registered at the last MC.
    std::atomic<int64_t> low_since_mark_compact_{0};
  };

  using PretenuringFeedbackMap =
      std::unordered_map<AllocationSite, size_t, Object::Hasher>;

  // Taking this mutex prevents the GC from entering a phase that relocates
  // object references.
  base::Mutex* relocation_mutex() { return &relocation_mutex_; }

  // Support for context snapshots.  After calling this we have a linear
  // space to write objects in each space.
  struct Chunk {
    uint32_t size;
    Address start;
    Address end;
  };
  using Reservation = std::vector<Chunk>;

#if V8_OS_ANDROID
  // Don't apply pointer multiplier on Android since it has no swap space and
  // should instead adapt it's heap size based on available physical memory.
  static const int kPointerMultiplier = 1;
  static const int kHeapLimitMultiplier = 1;
#else
  static const int kPointerMultiplier = kTaggedSize / 4;
  // The heap limit needs to be computed based on the system pointer size
  // because we want a pointer-compressed heap to have larger limit than
  // an orinary 32-bit which that is contrained by 2GB virtual address space.
  static const int kHeapLimitMultiplier = kSystemPointerSize / 4;
#endif

  static const size_t kMaxInitialOldGenerationSize =
      256 * MB * kHeapLimitMultiplier;

  // These constants control heap configuration based on the physical memory.
  static constexpr size_t kPhysicalMemoryToOldGenerationRatio = 4;
  // Young generation size is the same for compressed heaps and 32-bit heaps.
  static constexpr size_t kOldGenerationToSemiSpaceRatio =
      128 * kHeapLimitMultiplier / kPointerMultiplier;
  static constexpr size_t kOldGenerationToSemiSpaceRatioLowMemory =
      256 * kHeapLimitMultiplier / kPointerMultiplier;
  static constexpr size_t kOldGenerationLowMemory =
      128 * MB * kHeapLimitMultiplier;
  static constexpr size_t kNewLargeObjectSpaceToSemiSpaceRatio = 1;
  static constexpr size_t kMinSemiSpaceSize = 512 * KB * kPointerMultiplier;
  static constexpr size_t kMaxSemiSpaceSize = 8192 * KB * kPointerMultiplier;

  STATIC_ASSERT(kMinSemiSpaceSize % (1 << kPageSizeBits) == 0);
  STATIC_ASSERT(kMaxSemiSpaceSize % (1 << kPageSizeBits) == 0);

  static const int kTraceRingBufferSize = 512;
  static const int kStacktraceBufferSize = 512;

  static const int kNoGCFlags = 0;
  static const int kReduceMemoryFootprintMask = 1;
  // GCs that are forced, either through testing configurations (requring
  // --expose-gc) or through DevTools (using LowMemoryNotificaton).
  static const int kForcedGC = 2;

  // The minimum size of a HeapObject on the heap.
  static const int kMinObjectSizeInTaggedWords = 2;

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
  V8_EXPORT_PRIVATE static int GetMaximumFillToAlign(
      AllocationAlignment alignment);
  // Calculates the actual amount of filler required for a given address at the
  // given alignment.
  V8_EXPORT_PRIVATE static int GetFillToAlign(Address address,
                                              AllocationAlignment alignment);

  // Returns the size of the initial area of a code-range, which is marked
  // writable and reserved to contain unwind information.
  static size_t GetCodeRangeReservedAreaSize();

  [[noreturn]] void FatalProcessOutOfMemory(const char* location);

  // Checks whether the space is valid.
  static bool IsValidAllocationSpace(AllocationSpace space);

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

  // Helper function to get the bytecode flushing mode based on the flags. This
  // is required because it is not safe to acess flags in concurrent marker.
  static inline BytecodeFlushMode GetBytecodeFlushMode(Isolate* isolate);

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

  // Executes generational and/or marking write barrier for a [start, end) range
  // of non-weak slots inside |object|.
  template <typename TSlot>
  V8_EXPORT_PRIVATE void WriteBarrierForRange(HeapObject object, TSlot start,
                                              TSlot end);

  V8_EXPORT_PRIVATE static void WriteBarrierForCodeSlow(Code host);

  V8_EXPORT_PRIVATE static void GenerationalBarrierSlow(HeapObject object,
                                                        Address slot,
                                                        HeapObject value);
  V8_EXPORT_PRIVATE inline void RecordEphemeronKeyWrite(
      EphemeronHashTable table, Address key_slot);
  V8_EXPORT_PRIVATE static void EphemeronKeyWriteBarrierFromCode(
      Address raw_object, Address address, Isolate* isolate);
  V8_EXPORT_PRIVATE static void GenerationalBarrierForCodeSlow(
      Code host, RelocInfo* rinfo, HeapObject value);
  V8_EXPORT_PRIVATE static bool PageFlagsAreConsistent(HeapObject object);

  // Notifies the heap that is ok to start marking or other activities that
  // should not happen during deserialization.
  void NotifyDeserializationComplete();

  void NotifyBootstrapComplete();

  void NotifyOldGenerationExpansion(AllocationSpace space, MemoryChunk* chunk);

  void UpdateCurrentEpoch(GarbageCollector collector);

  inline Address* NewSpaceAllocationTopAddress();
  inline Address* NewSpaceAllocationLimitAddress();
  inline Address* OldSpaceAllocationTopAddress();
  inline Address* OldSpaceAllocationLimitAddress();

  // Move len non-weak tagged elements from src_slot to dst_slot of dst_object.
  // The source and destination memory ranges can overlap.
  V8_EXPORT_PRIVATE void MoveRange(HeapObject dst_object, ObjectSlot dst_slot,
                                   ObjectSlot src_slot, int len,
                                   WriteBarrierMode mode);

  // Copy len non-weak tagged elements from src_slot to dst_slot of dst_object.
  // The source and destination memory ranges must not overlap.
  template <typename TSlot>
  void CopyRange(HeapObject dst_object, TSlot dst_slot, TSlot src_slot, int len,
                 WriteBarrierMode mode);

  // Initialize a filler object to keep the ability to iterate over the heap
  // when introducing gaps within pages. If slots could have been recorded in
  // the freed area, then pass ClearRecordedSlots::kYes as the mode. Otherwise,
  // pass ClearRecordedSlots::kNo. Clears memory if clearing slots.
  V8_EXPORT_PRIVATE HeapObject CreateFillerObjectAt(
      Address addr, int size, ClearRecordedSlots clear_slots_mode);

  void CreateFillerObjectAtBackground(Address addr, int size,
                                      ClearFreedMemoryMode clear_memory_mode);

  template <typename T>
  void CreateFillerForArray(T object, int elements_to_trim, int bytes_to_trim);

  bool CanMoveObjectStart(HeapObject object);

  bool IsImmovable(HeapObject object);

  V8_EXPORT_PRIVATE static bool IsLargeObject(HeapObject object);

  // This method supports the deserialization allocator.  All allocations
  // are word-aligned.  The method should never fail to allocate since the
  // total space requirements of the deserializer are known at build time.
  inline Address DeserializerAllocate(AllocationType type, int size_in_bytes);

  // Trim the given array from the left. Note that this relocates the object
  // start and hence is only valid if there is only a single reference to it.
  V8_EXPORT_PRIVATE FixedArrayBase LeftTrimFixedArray(FixedArrayBase obj,
                                                      int elements_to_trim);

  // Trim the given array from the right.
  V8_EXPORT_PRIVATE void RightTrimFixedArray(FixedArrayBase obj,
                                             int elements_to_trim);
  void RightTrimWeakFixedArray(WeakFixedArray obj, int elements_to_trim);

  // Converts the given boolean condition to JavaScript boolean value.
  inline Oddball ToBoolean(bool condition);

  // Notify the heap that a context has been disposed.
  V8_EXPORT_PRIVATE int NotifyContextDisposed(bool dependant_context);

  void set_native_contexts_list(Object object) {
    native_contexts_list_ = object;
  }
  Object native_contexts_list() const { return native_contexts_list_; }

  void set_allocation_sites_list(Object object) {
    allocation_sites_list_ = object;
  }
  Object allocation_sites_list() { return allocation_sites_list_; }

  void set_dirty_js_finalization_registries_list(Object object) {
    dirty_js_finalization_registries_list_ = object;
  }
  Object dirty_js_finalization_registries_list() {
    return dirty_js_finalization_registries_list_;
  }
  void set_dirty_js_finalization_registries_list_tail(Object object) {
    dirty_js_finalization_registries_list_tail_ = object;
  }
  Object dirty_js_finalization_registries_list_tail() {
    return dirty_js_finalization_registries_list_tail_;
  }

  // Used in CreateAllocationSiteStub and the (de)serializer.
  Address allocation_sites_list_address() {
    return reinterpret_cast<Address>(&allocation_sites_list_);
  }

  // Traverse all the allocaions_sites [nested_site and weak_next] in the list
  // and foreach call the visitor
  void ForeachAllocationSite(
      Object list, const std::function<void(AllocationSite)>& visitor);

  // Number of mark-sweeps.
  int ms_count() const { return ms_count_; }

  // Checks whether the given object is allowed to be migrated from it's
  // current space into the given destination space. Used for debugging.
  bool AllowedToBeMigrated(Map map, HeapObject object, AllocationSpace dest);

  void CheckHandleCount();

  // Number of "runtime allocations" done so far.
  uint32_t allocations_count() { return allocations_count_; }

  // Print short heap statistics.
  void PrintShortHeapStatistics();

  // Print statistics of freelists of old_space:
  //  with FLAG_trace_gc_freelists: summary of each FreeListCategory.
  //  with FLAG_trace_gc_freelists_verbose: also prints the statistics of each
  //  FreeListCategory of each page.
  void PrintFreeListsStats();

  // Dump heap statistics in JSON format.
  void DumpJSONHeapStatistics(std::stringstream& stream);

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
  V8_EXPORT_PRIVATE void UnprotectAndRegisterMemoryChunk(HeapObject object);
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

  inline HeapState gc_state() const {
    return gc_state_.load(std::memory_order_relaxed);
  }
  void SetGCState(HeapState state);
  bool IsTearingDown() const { return gc_state() == TEAR_DOWN; }
  bool force_oom() const { return force_oom_; }

  inline bool IsInGCPostProcessing() { return gc_post_processing_depth_ > 0; }

  // If an object has an AllocationMemento trailing it, return it, otherwise
  // return a null AllocationMemento.
  template <FindMementoMode mode>
  inline AllocationMemento FindAllocationMemento(Map map, HeapObject object);

  // Performs GC after background allocation failure.
  void CollectGarbageForBackground(LocalHeap* local_heap);

  //
  // Support for the API.
  //

  void CreateApiObjects();

  // Implements the corresponding V8 API function.
  bool IdleNotification(double deadline_in_seconds);
  bool IdleNotification(int idle_time_in_ms);

  V8_EXPORT_PRIVATE void MemoryPressureNotification(MemoryPressureLevel level,
                                                    bool is_isolate_locked);
  void CheckMemoryPressure();

  V8_EXPORT_PRIVATE void AddNearHeapLimitCallback(v8::NearHeapLimitCallback,
                                                  void* data);
  V8_EXPORT_PRIVATE void RemoveNearHeapLimitCallback(
      v8::NearHeapLimitCallback callback, size_t heap_limit);
  V8_EXPORT_PRIVATE void AutomaticallyRestoreInitialHeapLimit(
      double threshold_percent);

  void AppendArrayBufferExtension(JSArrayBuffer object,
                                  ArrayBufferExtension* extension);

  GlobalSafepoint* safepoint() { return safepoint_.get(); }

  V8_EXPORT_PRIVATE double MonotonicallyIncreasingTimeInMs();

  void VerifyNewSpaceTop();

  void RecordStats(HeapStats* stats, bool take_snapshot = false);

  bool MeasureMemory(std::unique_ptr<v8::MeasureMemoryDelegate> delegate,
                     v8::MeasureMemoryExecution execution);

  std::unique_ptr<v8::MeasureMemoryDelegate> MeasureMemoryDelegate(
      Handle<NativeContext> context, Handle<JSPromise> promise,
      v8::MeasureMemoryMode mode);

  // Check new space expansion criteria and expand semispaces if it was hit.
  void CheckNewSpaceExpansionCriteria();

  void VisitExternalResources(v8::ExternalResourceVisitor* visitor);

  // An object should be promoted if the object has survived a
  // scavenge operation.
  inline bool ShouldBePromoted(Address old_address);

  void IncrementDeferredCount(v8::Isolate::UseCounterFeature feature);

  inline int NextScriptId();
  inline int NextDebuggingId();
  inline int GetNextTemplateSerialNumber();

  void SetSerializedObjects(FixedArray objects);
  void SetSerializedGlobalProxySizes(FixedArray sizes);

  void SetBasicBlockProfilingData(Handle<ArrayList> list);

  // For post mortem debugging.
  void RememberUnmappedPage(Address page, bool compacted);

  int64_t external_memory_hard_limit() { return max_old_generation_size() / 2; }

  V8_INLINE int64_t external_memory();
  V8_EXPORT_PRIVATE int64_t external_memory_limit();
  V8_INLINE int64_t update_external_memory(int64_t delta);

  V8_EXPORT_PRIVATE size_t YoungArrayBufferBytes();
  V8_EXPORT_PRIVATE size_t OldArrayBufferBytes();

  size_t backing_store_bytes() const { return backing_store_bytes_; }

  void CompactWeakArrayLists(AllocationType allocation);

  V8_EXPORT_PRIVATE void AddRetainedMap(Handle<NativeContext> context,
                                        Handle<Map> map);

  // This event is triggered after successful allocation of a new object made
  // by runtime. Allocations of target space for object evacuation do not
  // trigger the event. In order to track ALL allocations one must turn off
  // FLAG_inline_new.
  inline void OnAllocationEvent(HeapObject object, int size_in_bytes);

  // This event is triggered after object is moved to a new place.
  void OnMoveEvent(HeapObject target, HeapObject source, int size_in_bytes);

  inline bool CanAllocateInReadOnlySpace();
  bool deserialization_complete() const { return deserialization_complete_; }

  bool HasLowAllocationRate();
  bool HasHighFragmentation();
  bool HasHighFragmentation(size_t used, size_t committed);

  void ActivateMemoryReducerIfNeeded();

  V8_EXPORT_PRIVATE bool ShouldOptimizeForMemoryUsage();

  bool HighMemoryPressure() {
    return memory_pressure_level_.load(std::memory_order_relaxed) !=
           MemoryPressureLevel::kNone;
  }

  bool CollectionRequested();

  void CheckCollectionRequested();

  void RestoreHeapLimit(size_t heap_limit) {
    // Do not set the limit lower than the live size + some slack.
    size_t min_limit = SizeOfObjects() + SizeOfObjects() / 4;
    set_max_old_generation_size(
        std::min(max_old_generation_size(), std::max(heap_limit, min_limit)));
  }

  // ===========================================================================
  // Initialization. ===========================================================
  // ===========================================================================

  void ConfigureHeap(const v8::ResourceConstraints& constraints);
  void ConfigureHeapDefault();

  // Prepares the heap, setting up for deserialization.
  void SetUp();

  // Sets read-only heap and space.
  void SetUpFromReadOnlyHeap(ReadOnlyHeap* ro_heap);

  void ReplaceReadOnlySpace(SharedReadOnlySpace* shared_ro_space);

  // Sets up the heap memory without creating any objects.
  void SetUpSpaces();

  // (Re-)Initialize hash seed from flag or RNG.
  void InitializeHashSeed();

  // Bootstraps the object heap with the core set of objects required to run.
  // Returns whether it succeeded.
  bool CreateHeapObjects();

  // Create ObjectStats if live_object_stats_ or dead_object_stats_ are nullptr.
  void CreateObjectStats();

  // If the code range exists, allocates executable pages in the code range and
  // copies the embedded builtins code blob there. Returns address of the copy.
  // The builtins code region will be freed with the code range at tear down.
  uint8_t* RemapEmbeddedBuiltinsIntoCodeRange(const uint8_t* embedded_blob_code,
                                              size_t embedded_blob_code_size);

  // Sets the TearDown state, so no new GC tasks get posted.
  void StartTearDown();

  // Destroys all memory allocated by the heap.
  void TearDown();

  // Returns whether SetUp has been called.
  bool HasBeenSetUp() const;

  // ===========================================================================
  // Getters for spaces. =======================================================
  // ===========================================================================

  inline Address NewSpaceTop();

  NewSpace* new_space() { return new_space_; }
  OldSpace* old_space() { return old_space_; }
  CodeSpace* code_space() { return code_space_; }
  MapSpace* map_space() { return map_space_; }
  OldLargeObjectSpace* lo_space() { return lo_space_; }
  CodeLargeObjectSpace* code_lo_space() { return code_lo_space_; }
  NewLargeObjectSpace* new_lo_space() { return new_lo_space_; }
  ReadOnlySpace* read_only_space() { return read_only_space_; }

  inline PagedSpace* paged_space(int idx);
  inline Space* space(int idx);

  // ===========================================================================
  // Getters to other components. ==============================================
  // ===========================================================================

  GCTracer* tracer() { return tracer_.get(); }

  MemoryAllocator* memory_allocator() { return memory_allocator_.get(); }
  const MemoryAllocator* memory_allocator() const {
    return memory_allocator_.get();
  }

  inline Isolate* isolate();

  MarkCompactCollector* mark_compact_collector() {
    return mark_compact_collector_.get();
  }

  MinorMarkCompactCollector* minor_mark_compact_collector() {
    return minor_mark_compact_collector_;
  }

  ArrayBufferSweeper* array_buffer_sweeper() {
    return array_buffer_sweeper_.get();
  }

  const base::AddressRegion& code_range();

  // ===========================================================================
  // Root set access. ==========================================================
  // ===========================================================================

  // Shortcut to the roots table stored in the Isolate.
  V8_INLINE RootsTable& roots_table();

// Heap root getters.
#define ROOT_ACCESSOR(type, name, CamelName) inline type name();
  MUTABLE_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  V8_INLINE void SetRootMaterializedObjects(FixedArray objects);
  V8_INLINE void SetRootScriptList(Object value);
  V8_INLINE void SetRootNoScriptSharedFunctionInfos(Object value);
  V8_INLINE void SetMessageListeners(TemplateList value);
  V8_INLINE void SetPendingOptimizeForTestBytecode(Object bytecode);

  StrongRootsEntry* RegisterStrongRoots(FullObjectSlot start,
                                        FullObjectSlot end);
  void UnregisterStrongRoots(StrongRootsEntry* entry);
  void UpdateStrongRoots(StrongRootsEntry* entry, FullObjectSlot start,
                         FullObjectSlot end);

  void SetBuiltinsConstantsTable(FixedArray cache);
  void SetDetachedContexts(WeakArrayList detached_contexts);

  // A full copy of the interpreter entry trampoline, used as a template to
  // create copies of the builtin at runtime. The copies are used to create
  // better profiling information for ticks in bytecode execution. Note that
  // this is always a copy of the full builtin, i.e. not the off-heap
  // trampoline.
  // See also: FLAG_interpreted_frames_native_stack.
  void SetInterpreterEntryTrampolineForProfiling(Code code);

  void EnqueueDirtyJSFinalizationRegistry(
      JSFinalizationRegistry finalization_registry,
      std::function<void(HeapObject object, ObjectSlot slot, Object target)>
          gc_notify_updated_slot);

  MaybeHandle<JSFinalizationRegistry> DequeueDirtyJSFinalizationRegistry();

  // Called from Heap::NotifyContextDisposed to remove all
  // FinalizationRegistries with {context} from the dirty list when the context
  // e.g. navigates away or is detached. If the dirty list is empty afterwards,
  // the cleanup task is aborted if needed.
  void RemoveDirtyFinalizationRegistriesOnContext(NativeContext context);

  inline bool HasDirtyJSFinalizationRegistries();

  void PostFinalizationRegistryCleanupTaskIfNeeded();

  void set_is_finalization_registry_cleanup_task_posted(bool posted) {
    is_finalization_registry_cleanup_task_posted_ = posted;
  }

  bool is_finalization_registry_cleanup_task_posted() {
    return is_finalization_registry_cleanup_task_posted_;
  }

  V8_EXPORT_PRIVATE void KeepDuringJob(Handle<JSReceiver> target);
  void ClearKeptObjects();

  // ===========================================================================
  // Inline allocation. ========================================================
  // ===========================================================================

  // Indicates whether inline bump-pointer allocation has been disabled.
  bool inline_allocation_disabled() { return inline_allocation_disabled_; }

  // Switch whether inline bump-pointer allocation should be used.
  V8_EXPORT_PRIVATE void EnableInlineAllocation();
  V8_EXPORT_PRIVATE void DisableInlineAllocation();

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
  V8_EXPORT_PRIVATE void CollectAllAvailableGarbage(
      GarbageCollectionReason gc_reason);

  // Precise garbage collection that potentially finalizes already running
  // incremental marking before performing an atomic garbage collection.
  // Only use if absolutely necessary or in tests to avoid floating garbage!
  V8_EXPORT_PRIVATE void PreciseCollectAllGarbage(
      int flags, GarbageCollectionReason gc_reason,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  // Reports and external memory pressure event, either performs a major GC or
  // completes incremental marking in order to free external resources.
  void ReportExternalMemoryPressure();

  using GetExternallyAllocatedMemoryInBytesCallback =
      v8::Isolate::GetExternallyAllocatedMemoryInBytesCallback;

  void SetGetExternallyAllocatedMemoryInBytesCallback(
      GetExternallyAllocatedMemoryInBytesCallback callback) {
    external_memory_callback_ = callback;
  }

  // Invoked when GC was requested via the stack guard.
  void HandleGCRequest();

  // ===========================================================================
  // Builtins. =================================================================
  // ===========================================================================

  V8_EXPORT_PRIVATE Code builtin(int index);
  Address builtin_address(int index);
  void set_builtin(int index, Code builtin);

  // ===========================================================================
  // Iterators. ================================================================
  // ===========================================================================

  // None of these methods iterate over the read-only roots. To do this use
  // ReadOnlyRoots::Iterate. Read-only root iteration is not necessary for
  // garbage collection and is usually only performed as part of
  // (de)serialization or heap verification.

  // Iterates over the strong roots and the weak roots.
  void IterateRoots(RootVisitor* v, base::EnumSet<SkipRoot> options);
  // Iterates over entries in the smi roots list.  Only interesting to the
  // serializer/deserializer, since GC does not care about smis.
  void IterateSmiRoots(RootVisitor* v);
  // Iterates over weak string tables.
  void IterateWeakRoots(RootVisitor* v, base::EnumSet<SkipRoot> options);
  void IterateWeakGlobalHandles(RootVisitor* v);
  void IterateBuiltins(RootVisitor* v);
  void IterateStackRoots(RootVisitor* v);

  // ===========================================================================
  // Store buffer API. =========================================================
  // ===========================================================================

  // Used for query incremental marking status in generated code.
  Address* IsMarkingFlagAddress() {
    return reinterpret_cast<Address*>(&is_marking_flag_);
  }

  void SetIsMarkingFlag(uint8_t flag) { is_marking_flag_ = flag; }

  V8_EXPORT_PRIVATE Address* store_buffer_top_address();
  static intptr_t store_buffer_mask_constant();
  static Address store_buffer_overflow_function_address();

  void ClearRecordedSlot(HeapObject object, ObjectSlot slot);
  void ClearRecordedSlotRange(Address start, Address end);
  static int InsertIntoRememberedSetFromCode(MemoryChunk* chunk, Address slot);

#ifdef DEBUG
  void VerifyClearedSlot(HeapObject object, ObjectSlot slot);
  void VerifySlotRangeHasNoRecordedSlots(Address start, Address end);
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
  V8_EXPORT_PRIVATE void StartIdleIncrementalMarking(
      GarbageCollectionReason gc_reason,
      GCCallbackFlags gc_callback_flags = GCCallbackFlags::kNoGCCallbackFlags);

  // Starts incremental marking assuming incremental marking is currently
  // stopped.
  V8_EXPORT_PRIVATE void StartIncrementalMarking(
      int gc_flags, GarbageCollectionReason gc_reason,
      GCCallbackFlags gc_callback_flags = GCCallbackFlags::kNoGCCallbackFlags);

  void StartIncrementalMarkingIfAllocationLimitIsReached(
      int gc_flags,
      GCCallbackFlags gc_callback_flags = GCCallbackFlags::kNoGCCallbackFlags);
  void StartIncrementalMarkingIfAllocationLimitIsReachedBackground();

  void FinalizeIncrementalMarkingIfComplete(GarbageCollectionReason gc_reason);
  // Synchronously finalizes incremental marking.
  V8_EXPORT_PRIVATE void FinalizeIncrementalMarkingAtomically(
      GarbageCollectionReason gc_reason);

  void CompleteSweepingFull();
  void CompleteSweepingYoung(GarbageCollector collector);

  void EnsureSweepingCompleted();

  IncrementalMarking* incremental_marking() {
    return incremental_marking_.get();
  }

  MarkingBarrier* marking_barrier() { return marking_barrier_.get(); }

  // ===========================================================================
  // Concurrent marking API. ===================================================
  // ===========================================================================

  ConcurrentMarking* concurrent_marking() { return concurrent_marking_.get(); }

  // The runtime uses this function to notify potentially unsafe object layout
  // changes that require special synchronization with the concurrent marker.
  // The old size is the size of the object before layout change.
  // By default recorded slots in the object are invalidated. Pass
  // InvalidateRecordedSlots::kNo if this is not necessary or to perform this
  // manually.
  void NotifyObjectLayoutChange(
      HeapObject object, const DisallowGarbageCollection&,
      InvalidateRecordedSlots invalidate_recorded_slots =
          InvalidateRecordedSlots::kYes);

#ifdef VERIFY_HEAP
  // This function checks that either
  // - the map transition is safe,
  // - or it was communicated to GC using NotifyObjectLayoutChange.
  V8_EXPORT_PRIVATE void VerifyObjectLayoutChange(HeapObject object,
                                                  Map new_map);
#endif

  // ===========================================================================
  // Deoptimization support API. ===============================================
  // ===========================================================================

  // Setters for code offsets of well-known deoptimization targets.
  void SetConstructStubCreateDeoptPCOffset(int pc_offset);
  void SetConstructStubInvokeDeoptPCOffset(int pc_offset);
  void SetInterpreterEntryReturnPCOffset(int pc_offset);

  // Invalidates references in the given {code} object that are referenced
  // transitively from the deoptimization data. Mutates write-protected code.
  void InvalidateCodeDeoptimizationData(Code code);

  void DeoptMarkedAllocationSites();

  bool DeoptMaybeTenuredAllocationSites();

  // ===========================================================================
  // Embedder heap tracer support. =============================================
  // ===========================================================================

  LocalEmbedderHeapTracer* local_embedder_heap_tracer() const {
    return local_embedder_heap_tracer_.get();
  }

  V8_EXPORT_PRIVATE void SetEmbedderHeapTracer(EmbedderHeapTracer* tracer);
  EmbedderHeapTracer* GetEmbedderHeapTracer() const;

  void RegisterExternallyReferencedObject(Address* location);
  V8_EXPORT_PRIVATE void SetEmbedderStackStateForNextFinalization(
      EmbedderHeapTracer::EmbedderStackState stack_state);

  EmbedderHeapTracer::TraceFlags flags_for_embedder_tracer() const;

  // ===========================================================================
  // Unified heap (C++) support. ===============================================
  // ===========================================================================

  V8_EXPORT_PRIVATE void AttachCppHeap(v8::CppHeap* cpp_heap);
  V8_EXPORT_PRIVATE void DetachCppHeap();

  v8::CppHeap* cpp_heap() const { return cpp_heap_; }

  // ===========================================================================
  // External string table API. ================================================
  // ===========================================================================

  // Registers an external string.
  inline void RegisterExternalString(String string);

  // Called when a string's resource is changed. The size of the payload is sent
  // as argument of the method.
  V8_EXPORT_PRIVATE void UpdateExternalString(String string, size_t old_payload,
                                              size_t new_payload);

  // Finalizes an external string by deleting the associated external
  // data and clearing the resource pointer.
  inline void FinalizeExternalString(String string);

  static String UpdateYoungReferenceInExternalStringTableEntry(
      Heap* heap, FullObjectSlot pointer);

  // ===========================================================================
  // Methods checking/returning the space of a given object/address. ===========
  // ===========================================================================

  // Returns whether the object resides in new space.
  static inline bool InYoungGeneration(Object object);
  static inline bool InYoungGeneration(MaybeObject object);
  static inline bool InYoungGeneration(HeapObject heap_object);
  static inline bool InFromPage(Object object);
  static inline bool InFromPage(MaybeObject object);
  static inline bool InFromPage(HeapObject heap_object);
  static inline bool InToPage(Object object);
  static inline bool InToPage(MaybeObject object);
  static inline bool InToPage(HeapObject heap_object);

  // Returns whether the object resides in old space.
  inline bool InOldSpace(Object object);

  // Checks whether an address/object is in the non-read-only heap (including
  // auxiliary area and unused area). Use IsValidHeapObject if checking both
  // heaps is required.
  V8_EXPORT_PRIVATE bool Contains(HeapObject value) const;

  // Checks whether an address/object in a space.
  // Currently used by tests, serialization and heap verification only.
  V8_EXPORT_PRIVATE bool InSpace(HeapObject value, AllocationSpace space) const;

  // Slow methods that can be used for verification as they can also be used
  // with off-heap Addresses.
  V8_EXPORT_PRIVATE bool InSpaceSlow(Address addr, AllocationSpace space) const;

  static inline Heap* FromWritableHeapObject(HeapObject obj);

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
  V8_EXPORT_PRIVATE size_t MaxReserved();
  size_t MaxSemiSpaceSize() { return max_semi_space_size_; }
  size_t InitialSemiSpaceSize() { return initial_semispace_size_; }
  size_t MaxOldGenerationSize() { return max_old_generation_size(); }

  // Limit on the max old generation size imposed by the underlying allocator.
  V8_EXPORT_PRIVATE static size_t AllocatorLimitOnMaxOldGenerationSize();

  V8_EXPORT_PRIVATE static size_t HeapSizeFromPhysicalMemory(
      uint64_t physical_memory);
  V8_EXPORT_PRIVATE static void GenerationSizesFromHeapSize(
      size_t heap_size, size_t* young_generation_size,
      size_t* old_generation_size);
  V8_EXPORT_PRIVATE static size_t YoungGenerationSizeFromOldGenerationSize(
      size_t old_generation_size);
  V8_EXPORT_PRIVATE static size_t YoungGenerationSizeFromSemiSpaceSize(
      size_t semi_space_size);
  V8_EXPORT_PRIVATE static size_t SemiSpaceSizeFromYoungGenerationSize(
      size_t young_generation_size);
  V8_EXPORT_PRIVATE static size_t MinYoungGenerationSize();
  V8_EXPORT_PRIVATE static size_t MinOldGenerationSize();
  V8_EXPORT_PRIVATE static size_t MaxOldGenerationSize(
      uint64_t physical_memory);

  // Returns the capacity of the heap in bytes w/o growing. Heap grows when
  // more spaces are needed until it reaches the limit.
  size_t Capacity();

  // Returns the capacity of the old generation.
  V8_EXPORT_PRIVATE size_t OldGenerationCapacity();

  // Returns the amount of memory currently held alive by the unmapper.
  size_t CommittedMemoryOfUnmapper();

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

  // Returns size of all objects residing in the heap.
  V8_EXPORT_PRIVATE size_t SizeOfObjects();

  // Returns size of all global handles in the heap.
  V8_EXPORT_PRIVATE size_t TotalGlobalHandlesSize();

  // Returns size of all allocated/used global handles in the heap.
  V8_EXPORT_PRIVATE size_t UsedGlobalHandlesSize();

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

  inline size_t SurvivedYoungObjectSize() {
    return promoted_objects_size_ + semi_space_copied_object_size_;
  }

  inline void IncrementNodesDiedInNewSpace() { nodes_died_in_new_space_++; }

  inline void IncrementNodesCopiedInNewSpace() { nodes_copied_in_new_space_++; }

  inline void IncrementNodesPromoted() { nodes_promoted_++; }

  inline void IncrementYoungSurvivorsCounter(size_t survived) {
    survived_last_scavenge_ = survived;
    survived_since_last_expansion_ += survived;
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

  size_t EmbedderAllocationCounter() const;

  // This should be used only for testing.
  void set_old_generation_allocation_counter_at_last_gc(size_t new_value) {
    old_generation_allocation_counter_at_last_gc_ = new_value;
  }

  size_t PromotedSinceLastGC() {
    size_t old_generation_size = OldGenerationSizeOfObjects();
    return old_generation_size > old_generation_size_at_last_gc_
               ? old_generation_size - old_generation_size_at_last_gc_
               : 0;
  }

  int gc_count() const { return gc_count_; }

  bool is_current_gc_forced() const { return is_current_gc_forced_; }

  // Returns the size of objects residing in non-new spaces.
  // Excludes external memory held by those objects.
  V8_EXPORT_PRIVATE size_t OldGenerationSizeOfObjects();

  V8_EXPORT_PRIVATE size_t GlobalSizeOfObjects();

  // We allow incremental marking to overshoot the V8 and global allocation
  // limit for performace reasons. If the overshoot is too large then we are
  // more eager to finalize incremental marking.
  bool AllocationLimitOvershotByLargeMargin();

  // Return the maximum size objects can be before having to allocate them as
  // large objects. This takes into account allocating in the code space for
  // which the size of the allocatable space per V8 page may depend on the OS
  // page size at runtime. You may use kMaxRegularHeapObjectSize as a constant
  // instead if you know the allocation isn't in the code spaces.
  V8_EXPORT_PRIVATE static int MaxRegularHeapObjectSize(
      AllocationType allocation);

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
  V8_EXPORT_PRIVATE static HeapObject PrecedeWithFiller(ReadOnlyRoots roots,
                                                        HeapObject object,
                                                        int filler_size);

  // Creates a filler object if needed for alignment and returns a heap object
  // immediately after it. If any space is left after the returned object,
  // another filler object is created so the over allocated memory is iterable.
  static V8_WARN_UNUSED_RESULT HeapObject
  AlignWithFiller(ReadOnlyRoots roots, HeapObject object, int object_size,
                  int allocation_size, AllocationAlignment alignment);

  // Allocate an external backing store with the given allocation callback.
  // If the callback fails (indicated by a nullptr result) then this function
  // will re-try the allocation after performing GCs. This is useful for
  // external backing stores that may be retained by (unreachable) V8 objects
  // such as ArrayBuffers, ExternalStrings, etc.
  //
  // The function may also proactively trigger GCs even if the allocation
  // callback does not fail to keep the memory usage low.
  V8_EXPORT_PRIVATE void* AllocateExternalBackingStore(
      const std::function<void*(size_t)>& allocate, size_t byte_length);

  // ===========================================================================
  // Allocation site tracking. =================================================
  // ===========================================================================

  // Updates the AllocationSite of a given {object}. The entry (including the
  // count) is cached on the local pretenuring feedback.
  inline void UpdateAllocationSite(
      Map map, HeapObject object, PretenuringFeedbackMap* pretenuring_feedback);

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

  // Check if the given object was recently allocated and its fields may appear
  // as uninitialized to background threads.
  // This predicate may be invoked from a background thread.
  inline bool IsPendingAllocation(HeapObject object);

  // Notifies that all previously allocated objects are properly initialized
  // and ensures that IsPendingAllocation returns false for them. This function
  // may be invoked only on the main thread.
  V8_EXPORT_PRIVATE void PublishPendingAllocations();

  // ===========================================================================
  // Heap object allocation tracking. ==========================================
  // ===========================================================================

  V8_EXPORT_PRIVATE void AddHeapObjectAllocationTracker(
      HeapObjectAllocationTracker* tracker);
  V8_EXPORT_PRIVATE void RemoveHeapObjectAllocationTracker(
      HeapObjectAllocationTracker* tracker);
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

  // Returns the Code object for a given interior pointer.
  Code GcSafeFindCodeForInnerPointer(Address inner_pointer);

  // Returns true if {addr} is contained within {code} and false otherwise.
  // Mostly useful for debugging.
  bool GcSafeCodeContains(Code code, Address addr);

  // Casts a heap object to a code object and checks if the inner_pointer is
  // within the object.
  Code GcSafeCastToCode(HeapObject object, Address inner_pointer);

  // Returns the map of an object. Can be used during garbage collection, i.e.
  // it supports a forwarded map. Fails if the map is not the code map.
  Map GcSafeMapOfCodeSpaceObject(HeapObject object);

// =============================================================================
#ifdef VERIFY_HEAP
  // Verify the heap is in its normal state before or after a GC.
  V8_EXPORT_PRIVATE void Verify();
  // Verify the read-only heap after all read-only heap objects have been
  // created.
  void VerifyReadOnlyHeap();
  void VerifyRememberedSetFor(HeapObject object);
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
    uintptr_t offset = reinterpret_cast<uintptr_t>(result) & kMmapRegionMask;
    result = reinterpret_cast<void*>(mmap_region_base_ + offset);
#endif  // V8_OS_MACOSX
#endif  // V8_TARGET_ARCH_X64
    return result;
  }

  static const char* GarbageCollectionReasonToString(
      GarbageCollectionReason gc_reason);

  // Calculates the nof entries for the full sized number to string cache.
  inline int MaxNumberToStringCacheSize() const;

  static Isolate* GetIsolateFromWritableObject(HeapObject object);

  CollectionEpoch epoch_young() { return epoch_young_; }
  CollectionEpoch epoch_full() { return epoch_full_; }

  void UpdateEpochFull();

 private:
  using ExternalStringTableUpdaterCallback = String (*)(Heap* heap,
                                                        FullObjectSlot pointer);

  // External strings table is a place where all external strings are
  // registered.  We need to keep track of such strings to properly
  // finalize them.
  class ExternalStringTable {
   public:
    explicit ExternalStringTable(Heap* heap) : heap_(heap) {}
    ExternalStringTable(const ExternalStringTable&) = delete;
    ExternalStringTable& operator=(const ExternalStringTable&) = delete;

    // Registers an external string.
    inline void AddString(String string);
    bool Contains(String string);

    void IterateAll(RootVisitor* v);
    void IterateYoung(RootVisitor* v);
    void PromoteYoung();

    // Restores internal invariant and gets rid of collected strings. Must be
    // called after each Iterate*() that modified the strings.
    void CleanUpAll();
    void CleanUpYoung();

    // Finalize all registered external strings and clear tables.
    void TearDown();

    void UpdateYoungReferences(
        Heap::ExternalStringTableUpdaterCallback updater_func);
    void UpdateReferences(
        Heap::ExternalStringTableUpdaterCallback updater_func);

   private:
    void Verify();
    void VerifyYoung();

    Heap* const heap_;

    // To speed up scavenge collections young string are kept separate from old
    // strings.
    std::vector<Object> young_strings_;
    std::vector<Object> old_strings_;
  };

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
    GCCallbackTuple& operator=(const GCCallbackTuple& other) V8_NOEXCEPT;

    v8::Isolate::GCCallbackWithData callback;
    GCType gc_type;
    void* data;
  };

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

  static const int kInitialFeedbackCapacity = 256;

  Heap();
  ~Heap();

  Heap(const Heap&) = delete;
  Heap& operator=(const Heap&) = delete;

  static bool IsRegularObjectAllocation(AllocationType allocation) {
    return AllocationType::kYoung == allocation ||
           AllocationType::kOld == allocation;
  }

  static size_t DefaultGetExternallyAllocatedMemoryInBytesCallback() {
    return 0;
  }

#define ROOT_ACCESSOR(type, name, CamelName) inline void set_##name(type value);
  ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  void set_current_gc_flags(int flags) { current_gc_flags_ = flags; }

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

  // Ensure that LABs of local heaps are iterable.
  void MakeLocalHeapLabsIterable();

  // Performs garbage collection in a safepoint.
  // Returns the number of freed global handles.
  size_t PerformGarbageCollection(
      GarbageCollector collector,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  inline void UpdateOldSpaceLimits();

  bool CreateInitialMaps();
  void CreateInternalAccessorInfoObjects();
  void CreateInitialObjects();

  // Commits from space if it is uncommitted.
  void EnsureFromSpaceIsCommitted();

  // Uncommit unused semi space.
  V8_EXPORT_PRIVATE bool UncommitFromSpace();

  // Fill in bogus values in from space
  void ZapFromSpace();

  // Zaps the memory of a code object.
  V8_EXPORT_PRIVATE void ZapCodeObject(Address start_address,
                                       int size_in_bytes);

  // Initialize a filler object to keep the ability to iterate over the heap
  // when introducing gaps within pages. If the memory after the object header
  // of the filler should be cleared, pass in kClearFreedMemory. The default is
  // kDontClearFreedMemory.
  V8_EXPORT_PRIVATE static HeapObject CreateFillerObjectAt(
      ReadOnlyRoots roots, Address addr, int size,
      ClearFreedMemoryMode clear_memory_mode =
          ClearFreedMemoryMode::kDontClearFreedMemory);

  // Range write barrier implementation.
  template <int kModeMask, typename TSlot>
  V8_INLINE void WriteBarrierForRangeImpl(MemoryChunk* source_page,
                                          HeapObject object, TSlot start_slot,
                                          TSlot end_slot);

  // Deopts all code that contains allocation instruction which are tenured or
  // not tenured. Moreover it clears the pretenuring allocation site statistics.
  void ResetAllAllocationSitesDependentCode(AllocationType allocation);

  // Evaluates local pretenuring for the old space and calls
  // ResetAllTenuredAllocationSitesDependentCode if too many objects died in
  // the old space.
  void EvaluateOldSpaceLocalPretenuring(uint64_t size_of_objects_before_gc);

  // Record statistics after garbage collection.
  void ReportStatisticsAfterGC();

  // Flush the number to string cache.
  void FlushNumberStringCache();

  void ConfigureInitialOldGenerationSize();

  double ComputeMutatorUtilization(const char* tag, double mutator_speed,
                                   double gc_speed);
  bool HasLowYoungGenerationAllocationRate();
  bool HasLowOldGenerationAllocationRate();
  bool HasLowEmbedderAllocationRate();

  void ReduceNewSpaceSize();

  GCIdleTimeHeapState ComputeHeapState();

  bool PerformIdleTimeAction(GCIdleTimeAction action,
                             GCIdleTimeHeapState heap_state,
                             double deadline_in_ms);

  void IdleNotificationEpilogue(GCIdleTimeAction action,
                                GCIdleTimeHeapState heap_state, double start_ms,
                                double deadline_in_ms);

  int NextAllocationTimeout(int current_timeout = 0);
  inline void UpdateAllocationsHash(HeapObject object);
  inline void UpdateAllocationsHash(uint32_t value);
  void PrintAllocationsHash();

  void PrintMaxMarkingLimitReached();
  void PrintMaxNewSpaceSizeReached();

  int NextStressMarkingLimit();

  void AddToRingBuffer(const char* string);
  void GetFromRingBuffer(char* buffer);

  void CompactRetainedMaps(WeakArrayList retained_maps);

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

  void InvokeIncrementalMarkingPrologueCallbacks();
  void InvokeIncrementalMarkingEpilogueCallbacks();

  // Returns the timer used for a given GC type.
  // - GCScavenger: young generation GC
  // - GCCompactor: full GC
  // - GCFinalzeMC: finalization of incremental full GC
  // - GCFinalizeMCReduceMemory: finalization of incremental full GC with
  // memory reduction
  TimedHistogram* GCTypeTimer(GarbageCollector collector);
  TimedHistogram* GCTypePriorityTimer(GarbageCollector collector);

  // ===========================================================================
  // Pretenuring. ==============================================================
  // ===========================================================================

  // Pretenuring decisions are made based on feedback collected during new space
  // evacuation. Note that between feedback collection and calling this method
  // object in old space must not move.
  void ProcessPretenuringFeedback();

  // Removes an entry from the global pretenuring storage.
  void RemoveAllocationSitePretenuringFeedback(AllocationSite site);

  // ===========================================================================
  // Actual GC. ================================================================
  // ===========================================================================

  // Code that should be run before and after each GC.  Includes some
  // reporting/verification activities when compiled with DEBUG set.
  void GarbageCollectionPrologue();
  void GarbageCollectionPrologueInSafepoint();
  void GarbageCollectionEpilogue();
  void GarbageCollectionEpilogueInSafepoint(GarbageCollector collector);

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

  void UpdateYoungReferencesInExternalStringTable(
      ExternalStringTableUpdaterCallback updater_func);

  void UpdateReferencesInExternalStringTable(
      ExternalStringTableUpdaterCallback updater_func);

  void ProcessAllWeakReferences(WeakObjectRetainer* retainer);
  void ProcessYoungWeakReferences(WeakObjectRetainer* retainer);
  void ProcessNativeContexts(WeakObjectRetainer* retainer);
  void ProcessAllocationSites(WeakObjectRetainer* retainer);
  void ProcessDirtyJSFinalizationRegistries(WeakObjectRetainer* retainer);
  void ProcessWeakListRoots(WeakObjectRetainer* retainer);

  // ===========================================================================
  // GC statistics. ============================================================
  // ===========================================================================

  inline size_t OldGenerationSpaceAvailable() {
    uint64_t bytes = OldGenerationSizeOfObjects() +
                     AllocatedExternalMemorySinceMarkCompact();

    if (old_generation_allocation_limit() <= bytes) return 0;
    return old_generation_allocation_limit() - static_cast<size_t>(bytes);
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

  MemoryReducer* memory_reducer() { return memory_reducer_.get(); }

  // For some webpages RAIL mode does not switch from PERFORMANCE_LOAD.
  // This constant limits the effect of load RAIL mode on GC.
  // The value is arbitrary and chosen as the largest load time observed in
  // v8 browsing benchmarks.
  static const int kMaxLoadTimeMs = 7000;

  bool ShouldOptimizeForLoadTime();

  size_t old_generation_allocation_limit() const {
    return old_generation_allocation_limit_.load(std::memory_order_relaxed);
  }

  void set_old_generation_allocation_limit(size_t newlimit) {
    old_generation_allocation_limit_.store(newlimit, std::memory_order_relaxed);
  }

  size_t global_allocation_limit() const { return global_allocation_limit_; }

  size_t max_old_generation_size() {
    return max_old_generation_size_.load(std::memory_order_relaxed);
  }

  void set_max_old_generation_size(size_t value) {
    max_old_generation_size_.store(value, std::memory_order_relaxed);
  }

  bool always_allocate() { return always_allocate_scope_count_ != 0; }

  V8_EXPORT_PRIVATE bool CanExpandOldGeneration(size_t size);
  V8_EXPORT_PRIVATE bool CanExpandOldGenerationBackground(LocalHeap* local_heap,
                                                          size_t size);
  V8_EXPORT_PRIVATE bool CanPromoteYoungAndExpandOldGeneration(size_t size);

  bool ShouldExpandOldGenerationOnSlowAllocation(
      LocalHeap* local_heap = nullptr);
  bool IsRetryOfFailedAllocation(LocalHeap* local_heap);
  bool IsMainThreadParked(LocalHeap* local_heap);

  HeapGrowingMode CurrentHeapGrowingMode();

  double PercentToOldGenerationLimit();
  double PercentToGlobalMemoryLimit();
  enum class IncrementalMarkingLimit { kNoLimit, kSoftLimit, kHardLimit };
  IncrementalMarkingLimit IncrementalMarkingLimitReached();

  bool ShouldStressCompaction() const;

  bool UseGlobalMemoryScheduling() const {
    return FLAG_global_gc_scheduling && local_embedder_heap_tracer();
  }

  base::Optional<size_t> GlobalMemoryAvailable();

  void RecomputeLimits(GarbageCollector collector);

  // ===========================================================================
  // Idle notification. ========================================================
  // ===========================================================================

  bool RecentIdleNotificationHappened();

  // ===========================================================================
  // GC Tasks. =================================================================
  // ===========================================================================

  void ScheduleScavengeTaskIfNeeded();

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
      int size_in_bytes, AllocationType allocation,
      AllocationOrigin origin = AllocationOrigin::kRuntime,
      AllocationAlignment alignment = kWordAligned);

  // This method will try to allocate objects quickly (AllocationType::kYoung)
  // otherwise it falls back to a slower path indicated by the mode.
  enum AllocationRetryMode { kLightRetry, kRetryOrFail };
  template <AllocationRetryMode mode>
  V8_WARN_UNUSED_RESULT inline HeapObject AllocateRawWith(
      int size, AllocationType allocation,
      AllocationOrigin origin = AllocationOrigin::kRuntime,
      AllocationAlignment alignment = kWordAligned);

  // This method will try to perform an allocation of a given size of a given
  // AllocationType. If the allocation fails, a regular full garbage collection
  // is triggered and the allocation is retried. This is performed multiple
  // times. If after that retry procedure the allocation still fails nullptr is
  // returned.
  V8_WARN_UNUSED_RESULT HeapObject AllocateRawWithLightRetrySlowPath(
      int size, AllocationType allocation, AllocationOrigin origin,
      AllocationAlignment alignment = kWordAligned);

  // This method will try to perform an allocation of a given size of a given
  // AllocationType. If the allocation fails, a regular full garbage collection
  // is triggered and the allocation is retried. This is performed multiple
  // times. If after that retry procedure the allocation still fails a "hammer"
  // garbage collection is triggered which tries to significantly reduce memory.
  // If the allocation still fails after that a fatal error is thrown.
  V8_WARN_UNUSED_RESULT HeapObject AllocateRawWithRetryOrFailSlowPath(
      int size, AllocationType allocation, AllocationOrigin origin,
      AllocationAlignment alignment = kWordAligned);

  // Allocates a heap object based on the map.
  V8_WARN_UNUSED_RESULT AllocationResult Allocate(Map map,
                                                  AllocationType allocation);

  // Allocates a partial map for bootstrapping.
  V8_WARN_UNUSED_RESULT AllocationResult
  AllocatePartialMap(InstanceType instance_type, int instance_size);

  void FinalizePartialMap(Map map);

  void set_force_oom(bool value) { force_oom_ = value; }
  void set_force_gc_on_next_allocation() {
    force_gc_on_next_allocation_ = true;
  }

  // ===========================================================================
  // Retaining path tracing ====================================================
  // ===========================================================================

  void AddRetainer(HeapObject retainer, HeapObject object);
  void AddEphemeronRetainer(HeapObject retainer, HeapObject object);
  void AddRetainingRoot(Root root, HeapObject object);
  // Returns true if the given object is a target of retaining path tracking.
  // Stores the option corresponding to the object in the provided *option.
  bool IsRetainingPathTarget(HeapObject object, RetainingPathOption* option);
  void PrintRetainingPath(HeapObject object, RetainingPathOption option);

#ifdef DEBUG
  V8_EXPORT_PRIVATE void IncrementObjectCounters();
#endif  // DEBUG

  std::vector<Handle<NativeContext>> FindAllNativeContexts();
  std::vector<WeakArrayList> FindAllRetainedMaps();
  MemoryMeasurement* memory_measurement() { return memory_measurement_.get(); }

  // The amount of memory that has been freed concurrently.
  std::atomic<uintptr_t> external_memory_concurrently_freed_{0};
  ExternalMemoryAccounting external_memory_;

  // This can be calculated directly from a pointer to the heap; however, it is
  // more expedient to get at the isolate directly from within Heap methods.
  Isolate* isolate_ = nullptr;

  // These limits are initialized in Heap::ConfigureHeap based on the resource
  // constraints and flags.
  size_t code_range_size_ = 0;
  size_t max_semi_space_size_ = 0;
  size_t initial_semispace_size_ = 0;
  // Full garbage collections can be skipped if the old generation size
  // is below this threshold.
  size_t min_old_generation_size_ = 0;
  // If the old generation size exceeds this limit, then V8 will
  // crash with out-of-memory error.
  std::atomic<size_t> max_old_generation_size_{0};
  // TODO(mlippautz): Clarify whether this should take some embedder
  // configurable limit into account.
  size_t min_global_memory_size_ = 0;
  size_t max_global_memory_size_ = 0;

  size_t initial_max_old_generation_size_ = 0;
  size_t initial_max_old_generation_size_threshold_ = 0;
  size_t initial_old_generation_size_ = 0;
  bool old_generation_size_configured_ = false;
  size_t maximum_committed_ = 0;
  size_t old_generation_capacity_after_bootstrap_ = 0;

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

  std::vector<std::pair<v8::NearHeapLimitCallback, void*>>
      near_heap_limit_callbacks_;

  // For keeping track of context disposals.
  int contexts_disposed_ = 0;

  NewSpace* new_space_ = nullptr;
  OldSpace* old_space_ = nullptr;
  CodeSpace* code_space_ = nullptr;
  MapSpace* map_space_ = nullptr;
  OldLargeObjectSpace* lo_space_ = nullptr;
  CodeLargeObjectSpace* code_lo_space_ = nullptr;
  NewLargeObjectSpace* new_lo_space_ = nullptr;
  ReadOnlySpace* read_only_space_ = nullptr;
  // Map from the space id to the space.
  Space* space_[LAST_SPACE + 1];

  // List for tracking ArrayBufferExtensions
  ArrayBufferExtension* old_array_buffer_extensions_ = nullptr;
  ArrayBufferExtension* young_array_buffer_extensions_ = nullptr;

  // Determines whether code space is write-protected. This is essentially a
  // race-free copy of the {FLAG_write_protect_code_memory} flag.
  bool write_protect_code_memory_ = false;

  // Holds the number of open CodeSpaceMemoryModificationScopes.
  uintptr_t code_space_memory_modification_scope_depth_ = 0;

  std::atomic<HeapState> gc_state_{NOT_IN_GC};

  int gc_post_processing_depth_ = 0;

  // Returns the amount of external memory registered since last global gc.
  V8_EXPORT_PRIVATE uint64_t AllocatedExternalMemorySinceMarkCompact();

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
  std::atomic<size_t> old_generation_allocation_limit_{0};
  size_t global_allocation_limit_ = 0;

  // Indicates that inline bump-pointer allocation has been globally disabled
  // for all spaces. This is used to disable allocations in generated code.
  bool inline_allocation_disabled_ = false;

  // Weak list heads, threaded through the objects.
  // List heads are initialized lazily and contain the undefined_value at start.
  Object native_contexts_list_;
  Object allocation_sites_list_;
  Object dirty_js_finalization_registries_list_;
  // Weak list tails.
  Object dirty_js_finalization_registries_list_tail_;

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
  double total_gc_time_ms_ = 0.0;

  // Last time an idle notification happened.
  double last_idle_notification_time_ = 0.0;

  // Last time a garbage collection happened.
  double last_gc_time_ = 0.0;

  std::unique_ptr<GCTracer> tracer_;
  std::unique_ptr<MarkCompactCollector> mark_compact_collector_;
  MinorMarkCompactCollector* minor_mark_compact_collector_ = nullptr;
  std::unique_ptr<ScavengerCollector> scavenger_collector_;
  std::unique_ptr<ArrayBufferSweeper> array_buffer_sweeper_;

  std::unique_ptr<MemoryAllocator> memory_allocator_;
  std::unique_ptr<IncrementalMarking> incremental_marking_;
  std::unique_ptr<ConcurrentMarking> concurrent_marking_;
  std::unique_ptr<GCIdleTimeHandler> gc_idle_time_handler_;
  std::unique_ptr<MemoryMeasurement> memory_measurement_;
  std::unique_ptr<MemoryReducer> memory_reducer_;
  std::unique_ptr<ObjectStats> live_object_stats_;
  std::unique_ptr<ObjectStats> dead_object_stats_;
  std::unique_ptr<ScavengeJob> scavenge_job_;
  std::unique_ptr<AllocationObserver> scavenge_task_observer_;
  std::unique_ptr<AllocationObserver> stress_concurrent_allocation_observer_;
  std::unique_ptr<LocalEmbedderHeapTracer> local_embedder_heap_tracer_;
  std::unique_ptr<MarkingBarrier> marking_barrier_;

  // The embedder owns the C++ heap.
  v8::CppHeap* cpp_heap_ = nullptr;

  StrongRootsEntry* strong_roots_head_ = nullptr;
  base::Mutex strong_roots_mutex_;

  bool need_to_remove_stress_concurrent_allocation_observer_ = false;

  // This counter is increased before each GC and never reset.
  // To account for the bytes allocated since the last GC, use the
  // NewSpaceAllocationCounter() function.
  size_t new_space_allocation_counter_ = 0;

  // This counter is increased before each GC and never reset. To
  // account for the bytes allocated since the last GC, use the
  // OldGenerationAllocationCounter() function.
  size_t old_generation_allocation_counter_at_last_gc_ = 0;

  // The size of objects in old generation after the last MarkCompact GC.
  size_t old_generation_size_at_last_gc_{0};

  // The size of global memory after the last MarkCompact GC.
  size_t global_memory_at_last_gc_ = 0;

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
  GCCallbackFlags current_gc_callback_flags_ =
      GCCallbackFlags::kNoGCCallbackFlags;

  std::unique_ptr<GlobalSafepoint> safepoint_;

  bool is_current_gc_forced_ = false;

  ExternalStringTable external_string_table_;

  base::Mutex relocation_mutex_;

  std::unique_ptr<CollectionBarrier> collection_barrier_;

  int gc_callbacks_depth_ = 0;

  bool deserialization_complete_ = false;

  bool fast_promotion_mode_ = false;

  // Used for testing purposes.
  bool force_oom_ = false;
  bool force_gc_on_next_allocation_ = false;
  bool delay_sweeper_tasks_for_testing_ = false;

  HeapObject pending_layout_change_object_;

  base::Mutex unprotected_memory_chunks_mutex_;
  std::unordered_set<MemoryChunk*> unprotected_memory_chunks_;
  bool unprotected_memory_chunks_registry_enabled_ = false;

#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  // If the --gc-interval flag is set to a positive value, this
  // variable holds the value indicating the number of allocations
  // remain until the next failure and garbage collection.
  int allocation_timeout_ = 0;
#endif  // V8_ENABLE_ALLOCATION_TIMEOUT

  std::map<HeapObject, HeapObject, Object::Comparer> retainer_;
  std::map<HeapObject, Root, Object::Comparer> retaining_root_;
  // If an object is retained by an ephemeron, then the retaining key of the
  // ephemeron is stored in this map.
  std::map<HeapObject, HeapObject, Object::Comparer> ephemeron_retainer_;
  // For each index inthe retaining_path_targets_ array this map
  // stores the option of the corresponding target.
  std::map<int, RetainingPathOption> retaining_path_target_option_;

  std::vector<HeapObjectAllocationTracker*> allocation_trackers_;

  bool is_finalization_registry_cleanup_task_posted_ = false;

  std::unique_ptr<third_party_heap::Heap> tp_heap_;

  // We need two epochs, since there can be scavenges during incremental
  // marking.
  CollectionEpoch epoch_young_ = 0;
  CollectionEpoch epoch_full_ = 0;

  // Classes in "heap" can be friends.
  friend class AlwaysAllocateScope;
  friend class ArrayBufferCollector;
  friend class ArrayBufferSweeper;
  friend class ConcurrentMarking;
  friend class GCCallbacksScope;
  friend class GCTracer;
  friend class HeapObjectIterator;
  friend class ScavengeTaskObserver;
  friend class IncrementalMarking;
  friend class IncrementalMarkingJob;
  friend class LocalHeap;
  friend class OldLargeObjectSpace;
  template <typename ConcreteVisitor, typename MarkingState>
  friend class MarkingVisitorBase;
  friend class MarkCompactCollector;
  friend class MarkCompactCollectorBase;
  friend class MinorMarkCompactCollector;
  friend class NewLargeObjectSpace;
  friend class NewSpace;
  friend class ObjectStatsCollector;
  friend class Page;
  friend class PagedSpace;
  friend class ReadOnlyRoots;
  friend class Scavenger;
  friend class ScavengerCollector;
  friend class StressConcurrentAllocationObserver;
  friend class Space;
  friend class Sweeper;
  friend class heap::TestMemoryAllocatorScope;

  // The allocator interface.
  friend class Factory;
  friend class Deserializer;

  // The Isolate constructs us.
  friend class Isolate;

  // Used in cctest.
  friend class heap::HeapTester;
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
  size_t* code_lo_space_size;              // 12
  size_t* global_handle_count;             // 13
  size_t* weak_global_handle_count;        // 14
  size_t* pending_global_handle_count;     // 15
  size_t* near_death_global_handle_count;  // 16
  size_t* free_global_handle_count;        // 17
  size_t* memory_allocator_size;           // 18
  size_t* memory_allocator_capacity;       // 19
  size_t* malloced_memory;                 // 20
  size_t* malloced_peak_memory;            // 21
  size_t* objects_per_type;                // 22
  size_t* size_per_type;                   // 23
  int* os_error;                           // 24
  char* last_few_messages;                 // 25
  char* js_stacktrace;                     // 26
  intptr_t* end_marker;                    // 27
};

// Disables GC for all allocations. It should not be used
// outside heap, deserializer, and isolate bootstrap.
// Use AlwaysAllocateScopeForTesting in tests.
class V8_NODISCARD AlwaysAllocateScope {
 public:
  inline ~AlwaysAllocateScope();

 private:
  friend class AlwaysAllocateScopeForTesting;
  friend class Deserializer;
  friend class DeserializerAllocator;
  friend class Evacuator;
  friend class Heap;
  friend class Isolate;

  explicit inline AlwaysAllocateScope(Heap* heap);
  Heap* heap_;
};

class V8_NODISCARD AlwaysAllocateScopeForTesting {
 public:
  explicit inline AlwaysAllocateScopeForTesting(Heap* heap);

 private:
  AlwaysAllocateScope scope_;
};

// The CodeSpaceMemoryModificationScope can only be used by the main thread.
class V8_NODISCARD CodeSpaceMemoryModificationScope {
 public:
  explicit inline CodeSpaceMemoryModificationScope(Heap* heap);
  inline ~CodeSpaceMemoryModificationScope();

 private:
  Heap* heap_;
};

// The CodePageCollectionMemoryModificationScope can only be used by the main
// thread. It will not be enabled if a CodeSpaceMemoryModificationScope is
// already active.
class V8_NODISCARD CodePageCollectionMemoryModificationScope {
 public:
  explicit inline CodePageCollectionMemoryModificationScope(Heap* heap);
  inline ~CodePageCollectionMemoryModificationScope();

 private:
  Heap* heap_;
};

// The CodePageMemoryModificationScope does not check if tansitions to
// writeable and back to executable are actually allowed, i.e. the MemoryChunk
// was registered to be executable. It can be used by concurrent threads.
class V8_NODISCARD CodePageMemoryModificationScope {
 public:
  explicit inline CodePageMemoryModificationScope(BasicMemoryChunk* chunk);
  explicit inline CodePageMemoryModificationScope(Code object);
  inline ~CodePageMemoryModificationScope();

 private:
  BasicMemoryChunk* chunk_;
  bool scope_active_;

  // Disallow any GCs inside this scope, as a relocation of the underlying
  // object would change the {MemoryChunk} that this scope targets.
  DISALLOW_GARBAGE_COLLECTION(no_heap_allocation_)
};

// Visitor class to verify interior pointers in spaces that do not contain
// or care about intergenerational references. All heap object pointers have to
// point into the heap to a location that has a map pointer at its first word.
// Caveat: Heap::Contains is an approximation because it can return true for
// objects in a heap space but above the allocation pointer.
class VerifyPointersVisitor : public ObjectVisitor, public RootVisitor {
 public:
  explicit VerifyPointersVisitor(Heap* heap) : heap_(heap) {}
  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override;
  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override;
  void VisitCodeTarget(Code host, RelocInfo* rinfo) override;
  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override;

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override;
  void VisitRootPointers(Root root, const char* description,
                         OffHeapObjectSlot start,
                         OffHeapObjectSlot end) override;

 protected:
  V8_INLINE void VerifyHeapObjectImpl(HeapObject heap_object);

  template <typename TSlot>
  V8_INLINE void VerifyPointersImpl(TSlot start, TSlot end);

  virtual void VerifyPointers(HeapObject host, MaybeObjectSlot start,
                              MaybeObjectSlot end);

  Heap* heap_;
};

// Verify that all objects are Smis.
class VerifySmisVisitor : public RootVisitor {
 public:
  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override;
};

// Space iterator for iterating over all the paged spaces of the heap: Map
// space, old space and code space. Returns each space in turn, and null when it
// is done.
class V8_EXPORT_PRIVATE PagedSpaceIterator {
 public:
  explicit PagedSpaceIterator(Heap* heap)
      : heap_(heap), counter_(FIRST_GROWABLE_PAGED_SPACE) {}
  PagedSpace* Next();

 private:
  Heap* heap_;
  int counter_;
};

class V8_EXPORT_PRIVATE SpaceIterator : public Malloced {
 public:
  explicit SpaceIterator(Heap* heap);
  virtual ~SpaceIterator();

  bool HasNext();
  Space* Next();

 private:
  Heap* heap_;
  int current_space_;  // from enum AllocationSpace.
};

// A HeapObjectIterator provides iteration over the entire non-read-only heap.
// It aggregates the specific iterators for the different spaces as these can
// only iterate over one space only.
//
// HeapObjectIterator ensures there is no allocation during its lifetime (using
// an embedded DisallowGarbageCollection instance).
//
// HeapObjectIterator can skip free list nodes (that is, de-allocated heap
// objects that still remain in the heap). As implementation of free nodes
// filtering uses GC marks, it can't be used during MS/MC GC phases. Also, it is
// forbidden to interrupt iteration in this mode, as this will leave heap
// objects marked (and thus, unusable).
//
// See ReadOnlyHeapObjectIterator if you need to iterate over read-only space
// objects, or CombinedHeapObjectIterator if you need to iterate over both
// heaps.
class V8_EXPORT_PRIVATE HeapObjectIterator {
 public:
  enum HeapObjectsFiltering { kNoFiltering, kFilterUnreachable };

  explicit HeapObjectIterator(Heap* heap,
                              HeapObjectsFiltering filtering = kNoFiltering);
  ~HeapObjectIterator();

  HeapObject Next();

 private:
  HeapObject NextObject();

  DISALLOW_GARBAGE_COLLECTION(no_heap_allocation_)

  Heap* heap_;
  std::unique_ptr<SafepointScope> safepoint_scope_;
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
  virtual Object RetainAs(Object object) = 0;
};

// -----------------------------------------------------------------------------
// Allows observation of heap object allocations.
class HeapObjectAllocationTracker {
 public:
  virtual void AllocationEvent(Address addr, int size) = 0;
  virtual void MoveEvent(Address from, Address to, int size) {}
  virtual void UpdateObjectSizeEvent(Address addr, int size) {}
  virtual ~HeapObjectAllocationTracker() = default;
};

template <typename T>
T ForwardingAddress(T heap_obj) {
  MapWord map_word = heap_obj.map_word();

  if (map_word.IsForwardingAddress()) {
    return T::cast(map_word.ToForwardingAddress());
  } else if (Heap::InFromPage(heap_obj)) {
    return T();
  } else {
    // TODO(ulan): Support minor mark-compactor here.
    return heap_obj;
  }
}

// Address block allocator compatible with standard containers which registers
// its allocated range as strong roots.
class StrongRootBlockAllocator {
 public:
  using pointer = Address*;
  using const_pointer = const Address*;
  using reference = Address&;
  using const_reference = const Address&;
  using value_type = Address;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  template <class U>
  struct rebind;

  explicit StrongRootBlockAllocator(Heap* heap) : heap_(heap) {}

  Address* allocate(size_t n);
  void deallocate(Address* p, size_t n) noexcept;

 private:
  Heap* heap_;
};

// Rebinding to Address gives another StrongRootBlockAllocator.
template <>
struct StrongRootBlockAllocator::rebind<Address> {
  using other = StrongRootBlockAllocator;
};

// Rebinding to something other than Address gives a std::allocator that
// is copy-constructable from StrongRootBlockAllocator.
template <class U>
struct StrongRootBlockAllocator::rebind {
  class other : public std::allocator<U> {
   public:
    // NOLINTNEXTLINE
    other(const StrongRootBlockAllocator&) {}
  };
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_H_
