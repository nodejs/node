// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OBJECT_STATS_H_
#define V8_HEAP_OBJECT_STATS_H_

#include <set>

#include "src/base/ieee754.h"
#include "src/heap/heap.h"
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
    FIRST_CODE_AGE_SUB_TYPE =
        FIRST_FIXED_ARRAY_SUB_TYPE + LAST_FIXED_ARRAY_SUB_TYPE + 1,
    OBJECT_STATS_COUNT = FIRST_CODE_AGE_SUB_TYPE + Code::kCodeAgeCount + 1
  };

  void ClearObjectStats(bool clear_last_time_stats = false);

  void CheckpointObjectStats();
  void PrintJSON(const char* key);
  void Dump(std::stringstream& stream);

  void RecordObjectStats(InstanceType type, size_t size) {
    DCHECK(type <= LAST_TYPE);
    object_counts_[type]++;
    object_sizes_[type] += size;
    size_histogram_[type][HistogramIndexFromSize(size)]++;
  }

  void RecordCodeSubTypeStats(int code_sub_type, int code_age, size_t size) {
    int code_sub_type_index = FIRST_CODE_KIND_SUB_TYPE + code_sub_type;
    int code_age_index =
        FIRST_CODE_AGE_SUB_TYPE + code_age - Code::kFirstCodeAge;
    DCHECK(code_sub_type_index >= FIRST_CODE_KIND_SUB_TYPE &&
           code_sub_type_index < FIRST_CODE_AGE_SUB_TYPE);
    DCHECK(code_age_index >= FIRST_CODE_AGE_SUB_TYPE &&
           code_age_index < OBJECT_STATS_COUNT);
    object_counts_[code_sub_type_index]++;
    object_sizes_[code_sub_type_index] += size;
    object_counts_[code_age_index]++;
    object_sizes_[code_age_index] += size;
    const int idx = HistogramIndexFromSize(size);
    size_histogram_[code_sub_type_index][idx]++;
    size_histogram_[code_age_index][idx]++;
  }

  bool RecordFixedArraySubTypeStats(FixedArrayBase* array, int array_sub_type,
                                    size_t size, size_t over_allocated) {
    auto it = visited_fixed_array_sub_types_.insert(array);
    if (!it.second) return false;
    DCHECK(array_sub_type <= LAST_FIXED_ARRAY_SUB_TYPE);
    object_counts_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type]++;
    object_sizes_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type] += size;
    size_histogram_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type]
                   [HistogramIndexFromSize(size)]++;
    if (over_allocated > 0) {
      over_allocated_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type] +=
          over_allocated;
      over_allocated_histogram_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type]
                               [HistogramIndexFromSize(over_allocated)]++;
    }
    return true;
  }

  size_t object_count_last_gc(size_t index) {
    return object_counts_last_time_[index];
  }

  size_t object_size_last_gc(size_t index) {
    return object_sizes_last_time_[index];
  }

  Isolate* isolate();
  Heap* heap() { return heap_; }

 private:
  static const int kFirstBucketShift = 5;  // <=32
  static const int kLastBucketShift = 19;  // >512k
  static const int kFirstBucket = 1 << kFirstBucketShift;
  static const int kLastBucket = 1 << kLastBucketShift;
  static const int kNumberOfBuckets = kLastBucketShift - kFirstBucketShift + 1;

  int HistogramIndexFromSize(size_t size) {
    if (size == 0) return 0;
    int idx = static_cast<int>(base::ieee754::log2(static_cast<double>(size))) -
              kFirstBucketShift;
    return idx < 0 ? 0 : idx;
  }

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
  ObjectStatsCollector(Heap* heap, ObjectStats* stats)
      : heap_(heap), stats_(stats) {}

  void CollectGlobalStatistics();
  void CollectStatistics(HeapObject* obj);

 private:
  class CompilationCacheTableVisitor;

  void RecordBytecodeArrayDetails(BytecodeArray* obj);
  void RecordCodeDetails(Code* code);
  void RecordFixedArrayDetails(FixedArray* array);
  void RecordJSCollectionDetails(JSObject* obj);
  void RecordJSFunctionDetails(JSFunction* function);
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
  Heap* heap_;
  ObjectStats* stats_;

  friend class ObjectStatsCollector::CompilationCacheTableVisitor;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OBJECT_STATS_H_
