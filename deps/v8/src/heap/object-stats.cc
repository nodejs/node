// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/object-stats.h"

#include "src/counters.h"
#include "src/heap/heap-inl.h"
#include "src/isolate.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

static base::LazyMutex object_stats_mutex = LAZY_MUTEX_INITIALIZER;


void ObjectStats::ClearObjectStats(bool clear_last_time_stats) {
  memset(object_counts_, 0, sizeof(object_counts_));
  memset(object_sizes_, 0, sizeof(object_sizes_));
  if (clear_last_time_stats) {
    memset(object_counts_last_time_, 0, sizeof(object_counts_last_time_));
    memset(object_sizes_last_time_, 0, sizeof(object_sizes_last_time_));
  }
}


void ObjectStats::TraceObjectStat(const char* name, int count, int size,
                                  double time) {
  int ms_count = heap()->ms_count();
  PrintIsolate(isolate(),
               "heap:%p, time:%f, gc:%d, type:%s, count:%d, size:%d\n",
               static_cast<void*>(heap()), time, ms_count, name, count, size);
}


void ObjectStats::TraceObjectStats() {
  base::LockGuard<base::Mutex> lock_guard(object_stats_mutex.Pointer());
  int index;
  int count;
  int size;
  int total_size = 0;
  double time = isolate()->time_millis_since_init();
#define TRACE_OBJECT_COUNT(name)                     \
  count = static_cast<int>(object_counts_[name]);    \
  size = static_cast<int>(object_sizes_[name]) / KB; \
  total_size += size;                                \
  TraceObjectStat(#name, count, size, time);
  INSTANCE_TYPE_LIST(TRACE_OBJECT_COUNT)
#undef TRACE_OBJECT_COUNT
#define TRACE_OBJECT_COUNT(name)                      \
  index = FIRST_CODE_KIND_SUB_TYPE + Code::name;      \
  count = static_cast<int>(object_counts_[index]);    \
  size = static_cast<int>(object_sizes_[index]) / KB; \
  TraceObjectStat("*CODE_" #name, count, size, time);
  CODE_KIND_LIST(TRACE_OBJECT_COUNT)
#undef TRACE_OBJECT_COUNT
#define TRACE_OBJECT_COUNT(name)                      \
  index = FIRST_FIXED_ARRAY_SUB_TYPE + name;          \
  count = static_cast<int>(object_counts_[index]);    \
  size = static_cast<int>(object_sizes_[index]) / KB; \
  TraceObjectStat("*FIXED_ARRAY_" #name, count, size, time);
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(TRACE_OBJECT_COUNT)
#undef TRACE_OBJECT_COUNT
#define TRACE_OBJECT_COUNT(name)                                              \
  index =                                                                     \
      FIRST_CODE_AGE_SUB_TYPE + Code::k##name##CodeAge - Code::kFirstCodeAge; \
  count = static_cast<int>(object_counts_[index]);                            \
  size = static_cast<int>(object_sizes_[index]) / KB;                         \
  TraceObjectStat("*CODE_AGE_" #name, count, size, time);
  CODE_AGE_LIST_COMPLETE(TRACE_OBJECT_COUNT)
#undef TRACE_OBJECT_COUNT
}


void ObjectStats::CheckpointObjectStats() {
  base::LockGuard<base::Mutex> lock_guard(object_stats_mutex.Pointer());
  Counters* counters = isolate()->counters();
#define ADJUST_LAST_TIME_OBJECT_COUNT(name)              \
  counters->count_of_##name()->Increment(                \
      static_cast<int>(object_counts_[name]));           \
  counters->count_of_##name()->Decrement(                \
      static_cast<int>(object_counts_last_time_[name])); \
  counters->size_of_##name()->Increment(                 \
      static_cast<int>(object_sizes_[name]));            \
  counters->size_of_##name()->Decrement(                 \
      static_cast<int>(object_sizes_last_time_[name]));
  INSTANCE_TYPE_LIST(ADJUST_LAST_TIME_OBJECT_COUNT)
#undef ADJUST_LAST_TIME_OBJECT_COUNT
  int index;
#define ADJUST_LAST_TIME_OBJECT_COUNT(name)               \
  index = FIRST_CODE_KIND_SUB_TYPE + Code::name;          \
  counters->count_of_CODE_TYPE_##name()->Increment(       \
      static_cast<int>(object_counts_[index]));           \
  counters->count_of_CODE_TYPE_##name()->Decrement(       \
      static_cast<int>(object_counts_last_time_[index])); \
  counters->size_of_CODE_TYPE_##name()->Increment(        \
      static_cast<int>(object_sizes_[index]));            \
  counters->size_of_CODE_TYPE_##name()->Decrement(        \
      static_cast<int>(object_sizes_last_time_[index]));
  CODE_KIND_LIST(ADJUST_LAST_TIME_OBJECT_COUNT)
#undef ADJUST_LAST_TIME_OBJECT_COUNT
#define ADJUST_LAST_TIME_OBJECT_COUNT(name)               \
  index = FIRST_FIXED_ARRAY_SUB_TYPE + name;              \
  counters->count_of_FIXED_ARRAY_##name()->Increment(     \
      static_cast<int>(object_counts_[index]));           \
  counters->count_of_FIXED_ARRAY_##name()->Decrement(     \
      static_cast<int>(object_counts_last_time_[index])); \
  counters->size_of_FIXED_ARRAY_##name()->Increment(      \
      static_cast<int>(object_sizes_[index]));            \
  counters->size_of_FIXED_ARRAY_##name()->Decrement(      \
      static_cast<int>(object_sizes_last_time_[index]));
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(ADJUST_LAST_TIME_OBJECT_COUNT)
#undef ADJUST_LAST_TIME_OBJECT_COUNT
#define ADJUST_LAST_TIME_OBJECT_COUNT(name)                                   \
  index =                                                                     \
      FIRST_CODE_AGE_SUB_TYPE + Code::k##name##CodeAge - Code::kFirstCodeAge; \
  counters->count_of_CODE_AGE_##name()->Increment(                            \
      static_cast<int>(object_counts_[index]));                               \
  counters->count_of_CODE_AGE_##name()->Decrement(                            \
      static_cast<int>(object_counts_last_time_[index]));                     \
  counters->size_of_CODE_AGE_##name()->Increment(                             \
      static_cast<int>(object_sizes_[index]));                                \
  counters->size_of_CODE_AGE_##name()->Decrement(                             \
      static_cast<int>(object_sizes_last_time_[index]));
  CODE_AGE_LIST_COMPLETE(ADJUST_LAST_TIME_OBJECT_COUNT)
#undef ADJUST_LAST_TIME_OBJECT_COUNT

  MemCopy(object_counts_last_time_, object_counts_, sizeof(object_counts_));
  MemCopy(object_sizes_last_time_, object_sizes_, sizeof(object_sizes_));
  ClearObjectStats();
}


Isolate* ObjectStats::isolate() { return heap()->isolate(); }


void ObjectStatsVisitor::CountFixedArray(
    FixedArrayBase* fixed_array, FixedArraySubInstanceType fast_type,
    FixedArraySubInstanceType dictionary_type) {
  Heap* heap = fixed_array->map()->GetHeap();
  if (fixed_array->map() != heap->fixed_cow_array_map() &&
      fixed_array->map() != heap->fixed_double_array_map() &&
      fixed_array != heap->empty_fixed_array()) {
    if (fixed_array->IsDictionary()) {
      heap->object_stats_->RecordFixedArraySubTypeStats(dictionary_type,
                                                        fixed_array->Size());
    } else {
      heap->object_stats_->RecordFixedArraySubTypeStats(fast_type,
                                                        fixed_array->Size());
    }
  }
}


void ObjectStatsVisitor::VisitBase(VisitorId id, Map* map, HeapObject* obj) {
  Heap* heap = map->GetHeap();
  int object_size = obj->Size();
  heap->object_stats_->RecordObjectStats(map->instance_type(), object_size);
  table_.GetVisitorById(id)(map, obj);
  if (obj->IsJSObject()) {
    JSObject* object = JSObject::cast(obj);
    CountFixedArray(object->elements(), DICTIONARY_ELEMENTS_SUB_TYPE,
                    FAST_ELEMENTS_SUB_TYPE);
    CountFixedArray(object->properties(), DICTIONARY_PROPERTIES_SUB_TYPE,
                    FAST_PROPERTIES_SUB_TYPE);
  }
}


template <ObjectStatsVisitor::VisitorId id>
void ObjectStatsVisitor::Visit(Map* map, HeapObject* obj) {
  VisitBase(id, map, obj);
}


template <>
void ObjectStatsVisitor::Visit<ObjectStatsVisitor::kVisitMap>(Map* map,
                                                              HeapObject* obj) {
  Heap* heap = map->GetHeap();
  Map* map_obj = Map::cast(obj);
  DCHECK(map->instance_type() == MAP_TYPE);
  DescriptorArray* array = map_obj->instance_descriptors();
  if (map_obj->owns_descriptors() && array != heap->empty_descriptor_array()) {
    int fixed_array_size = array->Size();
    heap->object_stats_->RecordFixedArraySubTypeStats(DESCRIPTOR_ARRAY_SUB_TYPE,
                                                      fixed_array_size);
  }
  if (map_obj->has_code_cache()) {
    CodeCache* cache = CodeCache::cast(map_obj->code_cache());
    heap->object_stats_->RecordFixedArraySubTypeStats(
        MAP_CODE_CACHE_SUB_TYPE, cache->default_cache()->Size());
    if (!cache->normal_type_cache()->IsUndefined()) {
      heap->object_stats_->RecordFixedArraySubTypeStats(
          MAP_CODE_CACHE_SUB_TYPE,
          FixedArray::cast(cache->normal_type_cache())->Size());
    }
  }
  VisitBase(kVisitMap, map, obj);
}


template <>
void ObjectStatsVisitor::Visit<ObjectStatsVisitor::kVisitCode>(
    Map* map, HeapObject* obj) {
  Heap* heap = map->GetHeap();
  int object_size = obj->Size();
  DCHECK(map->instance_type() == CODE_TYPE);
  Code* code_obj = Code::cast(obj);
  heap->object_stats_->RecordCodeSubTypeStats(code_obj->kind(),
                                              code_obj->GetAge(), object_size);
  VisitBase(kVisitCode, map, obj);
}


template <>
void ObjectStatsVisitor::Visit<ObjectStatsVisitor::kVisitSharedFunctionInfo>(
    Map* map, HeapObject* obj) {
  Heap* heap = map->GetHeap();
  SharedFunctionInfo* sfi = SharedFunctionInfo::cast(obj);
  if (sfi->scope_info() != heap->empty_fixed_array()) {
    heap->object_stats_->RecordFixedArraySubTypeStats(
        SCOPE_INFO_SUB_TYPE, FixedArray::cast(sfi->scope_info())->Size());
  }
  VisitBase(kVisitSharedFunctionInfo, map, obj);
}


template <>
void ObjectStatsVisitor::Visit<ObjectStatsVisitor::kVisitFixedArray>(
    Map* map, HeapObject* obj) {
  Heap* heap = map->GetHeap();
  FixedArray* fixed_array = FixedArray::cast(obj);
  if (fixed_array == heap->string_table()) {
    heap->object_stats_->RecordFixedArraySubTypeStats(STRING_TABLE_SUB_TYPE,
                                                      fixed_array->Size());
  }
  VisitBase(kVisitFixedArray, map, obj);
}


void ObjectStatsVisitor::Initialize(VisitorDispatchTable<Callback>* original) {
  // Copy the original visitor table to make call-through possible. After we
  // preserved a copy locally, we patch the original table to call us.
  table_.CopyFrom(original);
#define COUNT_FUNCTION(id) original->Register(kVisit##id, Visit<kVisit##id>);
  VISITOR_ID_LIST(COUNT_FUNCTION)
#undef COUNT_FUNCTION
}

}  // namespace internal
}  // namespace v8
