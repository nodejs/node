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

// These instance types do not exist for actual use but are merely introduced
// for object stats tracing. In contrast to Code and FixedArray sub types
// these types are not known to other counters outside of object stats
// tracing.
//
// Update LAST_VIRTUAL_TYPE below when changing this macro.
#define VIRTUAL_INSTANCE_TYPE_LIST(V) \
  V(BOILERPLATE_ELEMENTS_TYPE)        \
  V(BOILERPLATE_NAME_DICTIONARY_TYPE) \
  V(BOILERPLATE_PROPERTY_ARRAY_TYPE)  \
  V(JS_ARRAY_BOILERPLATE_TYPE)        \
  V(JS_OBJECT_BOILERPLATE_TYPE)

namespace v8 {
namespace internal {

class ObjectStats {
 public:
  explicit ObjectStats(Heap* heap) : heap_(heap) { ClearObjectStats(); }

  // See description on VIRTUAL_INSTANCE_TYPE_LIST.
  enum VirtualInstanceType {
#define DEFINE_VIRTUAL_INSTANCE_TYPE(type) type,
    VIRTUAL_INSTANCE_TYPE_LIST(DEFINE_VIRTUAL_INSTANCE_TYPE)
#undef DEFINE_FIXED_ARRAY_SUB_INSTANCE_TYPE
        LAST_VIRTUAL_TYPE = JS_OBJECT_BOILERPLATE_TYPE,
  };

  // ObjectStats are kept in two arrays, counts and sizes. Related stats are
  // stored in a contiguous linear buffer. Stats groups are stored one after
  // another.
  enum {
    FIRST_CODE_KIND_SUB_TYPE = LAST_TYPE + 1,
    FIRST_FIXED_ARRAY_SUB_TYPE =
        FIRST_CODE_KIND_SUB_TYPE + Code::NUMBER_OF_KINDS,
    FIRST_VIRTUAL_TYPE =
        FIRST_FIXED_ARRAY_SUB_TYPE + LAST_FIXED_ARRAY_SUB_TYPE + 1,
    OBJECT_STATS_COUNT = FIRST_VIRTUAL_TYPE + LAST_VIRTUAL_TYPE + 1,
  };

  void ClearObjectStats(bool clear_last_time_stats = false);

  void PrintJSON(const char* key);
  void Dump(std::stringstream& stream);

  void CheckpointObjectStats();
  void RecordObjectStats(InstanceType type, size_t size);
  void RecordVirtualObjectStats(VirtualInstanceType type, size_t size);
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
  ObjectStatsCollector(Heap* heap, ObjectStats* live, ObjectStats* dead)
      : heap_(heap), live_(live), dead_(dead) {
    DCHECK_NOT_NULL(heap_);
    DCHECK_NOT_NULL(live_);
    DCHECK_NOT_NULL(dead_);
  }

  // Collects type information of live and dead objects. Requires mark bits to
  // be present.
  void Collect();

 private:
  Heap* const heap_;
  ObjectStats* const live_;
  ObjectStats* const dead_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OBJECT_STATS_H_
