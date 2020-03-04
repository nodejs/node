// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SCAVENGER_H_
#define V8_HEAP_SCAVENGER_H_

#include "src/base/platform/condition-variable.h"
#include "src/heap/local-allocator.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/slot-set.h"
#include "src/heap/worklist.h"

namespace v8 {
namespace internal {

class OneshotBarrier;

enum class CopyAndForwardResult {
  SUCCESS_YOUNG_GENERATION,
  SUCCESS_OLD_GENERATION,
  FAILURE
};

using ObjectAndSize = std::pair<HeapObject, int>;
using SurvivingNewLargeObjectsMap =
    std::unordered_map<HeapObject, Map, Object::Hasher>;
using SurvivingNewLargeObjectMapEntry = std::pair<HeapObject, Map>;

constexpr int kEphemeronTableListSegmentSize = 128;
using EphemeronTableList =
    Worklist<EphemeronHashTable, kEphemeronTableListSegmentSize>;

class ScavengerCollector {
 public:
  static const int kMaxScavengerTasks = 8;
  static const int kMaxWaitTimeMs = 2;

  explicit ScavengerCollector(Heap* heap);

  void CollectGarbage();

 private:
  void MergeSurvivingNewLargeObjects(
      const SurvivingNewLargeObjectsMap& objects);

  int NumberOfScavengeTasks();

  void ProcessWeakReferences(EphemeronTableList* ephemeron_table_list);
  void ClearYoungEphemerons(EphemeronTableList* ephemeron_table_list);
  void ClearOldEphemerons();
  void HandleSurvivingNewLargeObjects();

  Isolate* const isolate_;
  Heap* const heap_;
  base::Semaphore parallel_scavenge_semaphore_;
  SurvivingNewLargeObjectsMap surviving_new_large_objects_;

  friend class Scavenger;
};

class Scavenger {
 public:
  struct PromotionListEntry {
    HeapObject heap_object;
    Map map;
    int size;
  };

  class PromotionList {
   public:
    class View {
     public:
      View(PromotionList* promotion_list, int task_id)
          : promotion_list_(promotion_list), task_id_(task_id) {}

      inline void PushRegularObject(HeapObject object, int size);
      inline void PushLargeObject(HeapObject object, Map map, int size);
      inline bool IsEmpty();
      inline size_t LocalPushSegmentSize();
      inline bool Pop(struct PromotionListEntry* entry);
      inline bool IsGlobalPoolEmpty();
      inline bool ShouldEagerlyProcessPromotionList();

     private:
      PromotionList* promotion_list_;
      int task_id_;
    };

    explicit PromotionList(int num_tasks)
        : regular_object_promotion_list_(num_tasks),
          large_object_promotion_list_(num_tasks) {}

    inline void PushRegularObject(int task_id, HeapObject object, int size);
    inline void PushLargeObject(int task_id, HeapObject object, Map map,
                                int size);
    inline bool IsEmpty();
    inline size_t LocalPushSegmentSize(int task_id);
    inline bool Pop(int task_id, struct PromotionListEntry* entry);
    inline bool IsGlobalPoolEmpty();
    inline bool ShouldEagerlyProcessPromotionList(int task_id);

   private:
    static const int kRegularObjectPromotionListSegmentSize = 256;
    static const int kLargeObjectPromotionListSegmentSize = 4;

    using RegularObjectPromotionList =
        Worklist<ObjectAndSize, kRegularObjectPromotionListSegmentSize>;
    using LargeObjectPromotionList =
        Worklist<PromotionListEntry, kLargeObjectPromotionListSegmentSize>;

    RegularObjectPromotionList regular_object_promotion_list_;
    LargeObjectPromotionList large_object_promotion_list_;
  };

  static const int kCopiedListSegmentSize = 256;

  using CopiedList = Worklist<ObjectAndSize, kCopiedListSegmentSize>;
  Scavenger(ScavengerCollector* collector, Heap* heap, bool is_logging,
            Worklist<MemoryChunk*, 64>* empty_chunks, CopiedList* copied_list,
            PromotionList* promotion_list,
            EphemeronTableList* ephemeron_table_list, int task_id);

  // Entry point for scavenging an old generation page. For scavenging single
  // objects see RootScavengingVisitor and ScavengeVisitor below.
  void ScavengePage(MemoryChunk* page);

  // Processes remaining work (=objects) after single objects have been
  // manually scavenged using ScavengeObject or CheckAndScavengeObject.
  void Process(OneshotBarrier* barrier = nullptr);

  // Finalize the Scavenger. Needs to be called from the main thread.
  void Finalize();

  void AddEphemeronHashTable(EphemeronHashTable table);

  size_t bytes_copied() const { return copied_size_; }
  size_t bytes_promoted() const { return promoted_size_; }

 private:
  // Number of objects to process before interrupting for potentially waking
  // up other tasks.
  static const int kInterruptThreshold = 128;
  static const int kInitialLocalPretenuringFeedbackCapacity = 256;

  inline Heap* heap() { return heap_; }

  inline void PageMemoryFence(MaybeObject object);

  void AddPageToSweeperIfNecessary(MemoryChunk* page);

  // Potentially scavenges an object referenced from |slot| if it is
  // indeed a HeapObject and resides in from space.
  template <typename TSlot>
  inline SlotCallbackResult CheckAndScavengeObject(Heap* heap, TSlot slot);

  // Scavenges an object |object| referenced from slot |p|. |object| is required
  // to be in from space.
  template <typename THeapObjectSlot>
  inline SlotCallbackResult ScavengeObject(THeapObjectSlot p,
                                           HeapObject object);

  // Copies |source| to |target| and sets the forwarding pointer in |source|.
  V8_INLINE bool MigrateObject(Map map, HeapObject source, HeapObject target,
                               int size);

  V8_INLINE SlotCallbackResult
  RememberedSetEntryNeeded(CopyAndForwardResult result);

  template <typename THeapObjectSlot>
  V8_INLINE CopyAndForwardResult
  SemiSpaceCopyObject(Map map, THeapObjectSlot slot, HeapObject object,
                      int object_size, ObjectFields object_fields);

  template <typename THeapObjectSlot>
  V8_INLINE CopyAndForwardResult PromoteObject(Map map, THeapObjectSlot slot,
                                               HeapObject object,
                                               int object_size,
                                               ObjectFields object_fields);

  template <typename THeapObjectSlot>
  V8_INLINE SlotCallbackResult EvacuateObject(THeapObjectSlot slot, Map map,
                                              HeapObject source);

  V8_INLINE bool HandleLargeObject(Map map, HeapObject object, int object_size,
                                   ObjectFields object_fields);

  // Different cases for object evacuation.
  template <typename THeapObjectSlot>
  V8_INLINE SlotCallbackResult
  EvacuateObjectDefault(Map map, THeapObjectSlot slot, HeapObject object,
                        int object_size, ObjectFields object_fields);

  template <typename THeapObjectSlot>
  inline SlotCallbackResult EvacuateThinString(Map map, THeapObjectSlot slot,
                                               ThinString object,
                                               int object_size);

  template <typename THeapObjectSlot>
  inline SlotCallbackResult EvacuateShortcutCandidate(Map map,
                                                      THeapObjectSlot slot,
                                                      ConsString object,
                                                      int object_size);

  void IterateAndScavengePromotedObject(HeapObject target, Map map, int size);
  void RememberPromotedEphemeron(EphemeronHashTable table, int index);

  ScavengerCollector* const collector_;
  Heap* const heap_;
  Worklist<MemoryChunk*, 64>::View empty_chunks_;
  PromotionList::View promotion_list_;
  CopiedList::View copied_list_;
  EphemeronTableList::View ephemeron_table_list_;
  Heap::PretenuringFeedbackMap local_pretenuring_feedback_;
  size_t copied_size_;
  size_t promoted_size_;
  LocalAllocator allocator_;
  SurvivingNewLargeObjectsMap surviving_new_large_objects_;

  EphemeronRememberedSet ephemeron_remembered_set_;
  const bool is_logging_;
  const bool is_incremental_marking_;
  const bool is_compacting_;

  friend class IterateAndScavengePromotedObjectsVisitor;
  friend class RootScavengeVisitor;
  friend class ScavengeVisitor;
};

// Helper class for turning the scavenger into an object visitor that is also
// filtering out non-HeapObjects and objects which do not reside in new space.
class RootScavengeVisitor final : public RootVisitor {
 public:
  explicit RootScavengeVisitor(Scavenger* scavenger);

  void VisitRootPointer(Root root, const char* description,
                        FullObjectSlot p) final;
  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) final;

 private:
  void ScavengePointer(FullObjectSlot p);

  Scavenger* const scavenger_;
};

class ScavengeVisitor final : public NewSpaceVisitor<ScavengeVisitor> {
 public:
  explicit ScavengeVisitor(Scavenger* scavenger);

  V8_INLINE void VisitPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final;

  V8_INLINE void VisitPointers(HeapObject host, MaybeObjectSlot start,
                               MaybeObjectSlot end) final;

  V8_INLINE void VisitCodeTarget(Code host, RelocInfo* rinfo) final;
  V8_INLINE void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) final;
  V8_INLINE int VisitEphemeronHashTable(Map map, EphemeronHashTable object);

 private:
  template <typename TSlot>
  V8_INLINE void VisitHeapObjectImpl(TSlot slot, HeapObject heap_object);

  template <typename TSlot>
  V8_INLINE void VisitPointersImpl(HeapObject host, TSlot start, TSlot end);

  Scavenger* const scavenger_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SCAVENGER_H_
