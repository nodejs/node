// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OBJECT_STATS_H_
#define V8_HEAP_OBJECT_STATS_H_

#include "src/heap/heap.h"
#include "src/heap/objects-visiting.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class ObjectStats {
 public:
  explicit ObjectStats(Heap* heap) : heap_(heap) {}

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

  void TraceObjectStats();
  void TraceObjectStat(const char* name, int count, int size, double time);
  void CheckpointObjectStats();

  void RecordObjectStats(InstanceType type, size_t size) {
    DCHECK(type <= LAST_TYPE);
    object_counts_[type]++;
    object_sizes_[type] += size;
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
  }

  void RecordFixedArraySubTypeStats(int array_sub_type, size_t size) {
    DCHECK(array_sub_type <= LAST_FIXED_ARRAY_SUB_TYPE);
    object_counts_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type]++;
    object_sizes_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type] += size;
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
  Heap* heap_;

  // Object counts and used memory by InstanceType
  size_t object_counts_[OBJECT_STATS_COUNT];
  size_t object_counts_last_time_[OBJECT_STATS_COUNT];
  size_t object_sizes_[OBJECT_STATS_COUNT];
  size_t object_sizes_last_time_[OBJECT_STATS_COUNT];
};


class ObjectStatsVisitor : public StaticMarkingVisitor<ObjectStatsVisitor> {
 public:
  static void Initialize(VisitorDispatchTable<Callback>* original);

  static void VisitBase(VisitorId id, Map* map, HeapObject* obj);

  static void CountFixedArray(FixedArrayBase* fixed_array,
                              FixedArraySubInstanceType fast_type,
                              FixedArraySubInstanceType dictionary_type);

  template <VisitorId id>
  static inline void Visit(Map* map, HeapObject* obj);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OBJECT_STATS_H_
