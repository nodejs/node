// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OBJECT_STATS_H_
#define V8_HEAP_OBJECT_STATS_H_

#include <set>

#include "src/base/ieee754.h"
#include "src/heap/heap.h"
#include "src/heap/mark-compact.h"
#include "src/heap/objects-visiting.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class ObjectStats {
 public:
  explicit ObjectStats(Heap* heap) : heap_(heap) { ClearObjectStats(); }

  // ObjectStats are kept in two arrays, counts and sizes. Related stats are
  // stored in a contiguous linear buffer. Stats groups are stored one after
  // another.
  enum {
    FIRST_CODE_KIND_SUB_TYPE = LAST_TYPE + 1,
    FIRST_FIXED_ARRAY_SUB_TYPE =
        FIRST_CODE_KIND_SUB_TYPE + Code::NUMBER_OF_KINDS,
    OBJECT_STATS_COUNT =
        FIRST_FIXED_ARRAY_SUB_TYPE + LAST_FIXED_ARRAY_SUB_TYPE + 1,
  };

  void ClearObjectStats(bool clear_last_time_stats = false);

  void CheckpointObjectStats();
  void PrintJSON(const char* key);
  void Dump(std::stringstream& stream);

  void RecordObjectStats(InstanceType type, size_t size);
  void RecordCodeSubTypeStats(int code_sub_type, size_t size);
  bool RecordFixedArraySubTypeStats(FixedArrayBase* array, int array_sub_type,
                                    size_t size, size_t over_allocated);

  size_t object_count_last_gc(size_t index) {
    return object_counts_last_time_[index];
  }

  size_t object_size_last_gc(size_t index) {
    return object_sizes_last_time_[index];
  }

  Isolate* isolate();
  Heap* heap() { return heap_; }

 private:
  static const int kFirstBucketShift = 5;  // <32
  static const int kLastBucketShift = 20;  // >=1M
  static const int kFirstBucket = 1 << kFirstBucketShift;
  static const int kLastBucket = 1 << kLastBucketShift;
  static const int kNumberOfBuckets = kLastBucketShift - kFirstBucketShift + 1;
  static const int kLastValueBucketIndex = kLastBucketShift - kFirstBucketShift;

  void PrintKeyAndId(const char* key, int gc_count);
  // The following functions are excluded from inline to reduce the overall
  // binary size of VB. On x64 this save around 80KB.
  V8_NOINLINE void PrintInstanceTypeJSON(const char* key, int gc_count,
                                         const char* name, int index);
  V8_NOINLINE void DumpInstanceTypeData(std::stringstream& stream,
                                        const char* name, int index);

  int HistogramIndexFromSize(size_t size);

  Heap* heap_;
  // Object counts and used memory by InstanceType.
  size_t object_counts_[OBJECT_STATS_COUNT];
  size_t object_counts_last_time_[OBJECT_STATS_COUNT];
  size_t object_sizes_[OBJECT_STATS_COUNT];
  size_t object_sizes_last_time_[OBJECT_STATS_COUNT];
  // Approximation of overallocated memory by InstanceType.
  size_t over_allocated_[OBJECT_STATS_COUNT];
  // Detailed histograms by InstanceType.
  size_t size_histogram_[OBJECT_STATS_COUNT][kNumberOfBuckets];
  size_t over_allocated_histogram_[OBJECT_STATS_COUNT][kNumberOfBuckets];

  std::set<FixedArrayBase*> visited_fixed_array_sub_types_;
};

class ObjectStatsCollector {
 public:
  ObjectStatsCollector(Heap* heap, ObjectStats* stats);

  void CollectGlobalStatistics();
  void CollectStatistics(HeapObject* obj);

 private:
  class CompilationCacheTableVisitor;

  void RecordBytecodeArrayDetails(BytecodeArray* obj);
  void RecordCodeDetails(Code* code);
  void RecordFixedArrayDetails(FixedArray* array);
  void RecordJSCollectionDetails(JSObject* obj);
  void RecordJSObjectDetails(JSObject* object);
  void RecordJSWeakCollectionDetails(JSWeakCollection* obj);
  void RecordMapDetails(Map* map);
  void RecordScriptDetails(Script* obj);
  void RecordTemplateInfoDetails(TemplateInfo* obj);
  void RecordSharedFunctionInfoDetails(SharedFunctionInfo* sfi);

  bool RecordFixedArrayHelper(HeapObject* parent, FixedArray* array,
                              int subtype, size_t overhead);
  void RecursivelyRecordFixedArrayHelper(HeapObject* parent, FixedArray* array,
                                         int subtype);
  template <class HashTable>
  void RecordHashTableHelper(HeapObject* parent, HashTable* array, int subtype);
  bool SameLiveness(HeapObject* obj1, HeapObject* obj2);
  Heap* heap_;
  ObjectStats* stats_;
  MarkCompactCollector::NonAtomicMarkingState* marking_state_;

  friend class ObjectStatsCollector::CompilationCacheTableVisitor;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OBJECT_STATS_H_
