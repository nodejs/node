// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/object-stats.h"

#include <unordered_set>

#include "src/assembler-inl.h"
#include "src/base/bits.h"
#include "src/compilation-cache.h"
#include "src/counters.h"
#include "src/globals.h"
#include "src/heap/heap-inl.h"
#include "src/isolate.h"
#include "src/objects/compilation-cache-inl.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

static base::LazyMutex object_stats_mutex = LAZY_MUTEX_INITIALIZER;

void ObjectStats::ClearObjectStats(bool clear_last_time_stats) {
  memset(object_counts_, 0, sizeof(object_counts_));
  memset(object_sizes_, 0, sizeof(object_sizes_));
  memset(over_allocated_, 0, sizeof(over_allocated_));
  memset(size_histogram_, 0, sizeof(size_histogram_));
  memset(over_allocated_histogram_, 0, sizeof(over_allocated_histogram_));
  if (clear_last_time_stats) {
    memset(object_counts_last_time_, 0, sizeof(object_counts_last_time_));
    memset(object_sizes_last_time_, 0, sizeof(object_sizes_last_time_));
  }
  visited_fixed_array_sub_types_.clear();
}

// Tell the compiler to never inline this: occasionally, the optimizer will
// decide to inline this and unroll the loop, making the compiled code more than
// 100KB larger.
V8_NOINLINE static void PrintJSONArray(size_t* array, const int len) {
  PrintF("[ ");
  for (int i = 0; i < len; i++) {
    PrintF("%zu", array[i]);
    if (i != (len - 1)) PrintF(", ");
  }
  PrintF(" ]");
}

V8_NOINLINE static void DumpJSONArray(std::stringstream& stream, size_t* array,
                                      const int len) {
  stream << "[";
  for (int i = 0; i < len; i++) {
    stream << array[i];
    if (i != (len - 1)) stream << ",";
  }
  stream << "]";
}

void ObjectStats::PrintKeyAndId(const char* key, int gc_count) {
  PrintF("\"isolate\": \"%p\", \"id\": %d, \"key\": \"%s\", ",
         reinterpret_cast<void*>(isolate()), gc_count, key);
}

void ObjectStats::PrintInstanceTypeJSON(const char* key, int gc_count,
                                        const char* name, int index) {
  PrintF("{ ");
  PrintKeyAndId(key, gc_count);
  PrintF("\"type\": \"instance_type_data\", ");
  PrintF("\"instance_type\": %d, ", index);
  PrintF("\"instance_type_name\": \"%s\", ", name);
  PrintF("\"overall\": %zu, ", object_sizes_[index]);
  PrintF("\"count\": %zu, ", object_counts_[index]);
  PrintF("\"over_allocated\": %zu, ", over_allocated_[index]);
  PrintF("\"histogram\": ");
  PrintJSONArray(size_histogram_[index], kNumberOfBuckets);
  PrintF(",");
  PrintF("\"over_allocated_histogram\": ");
  PrintJSONArray(over_allocated_histogram_[index], kNumberOfBuckets);
  PrintF(" }\n");
}

void ObjectStats::PrintJSON(const char* key) {
  double time = isolate()->time_millis_since_init();
  int gc_count = heap()->gc_count();

  // gc_descriptor
  PrintF("{ ");
  PrintKeyAndId(key, gc_count);
  PrintF("\"type\": \"gc_descriptor\", \"time\": %f }\n", time);
  // bucket_sizes
  PrintF("{ ");
  PrintKeyAndId(key, gc_count);
  PrintF("\"type\": \"bucket_sizes\", \"sizes\": [ ");
  for (int i = 0; i < kNumberOfBuckets; i++) {
    PrintF("%d", 1 << (kFirstBucketShift + i));
    if (i != (kNumberOfBuckets - 1)) PrintF(", ");
  }
  PrintF(" ] }\n");

#define INSTANCE_TYPE_WRAPPER(name) \
  PrintInstanceTypeJSON(key, gc_count, #name, name);
#define CODE_KIND_WRAPPER(name)                        \
  PrintInstanceTypeJSON(key, gc_count, "*CODE_" #name, \
                        FIRST_CODE_KIND_SUB_TYPE + Code::name);
#define FIXED_ARRAY_SUB_INSTANCE_TYPE_WRAPPER(name)           \
  PrintInstanceTypeJSON(key, gc_count, "*FIXED_ARRAY_" #name, \
                        FIRST_FIXED_ARRAY_SUB_TYPE + name);
#define VIRTUAL_INSTANCE_TYPE_WRAPPER(name) \
  PrintInstanceTypeJSON(key, gc_count, #name, FIRST_VIRTUAL_TYPE + name);

  INSTANCE_TYPE_LIST(INSTANCE_TYPE_WRAPPER)
  CODE_KIND_LIST(CODE_KIND_WRAPPER)
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(FIXED_ARRAY_SUB_INSTANCE_TYPE_WRAPPER)
  VIRTUAL_INSTANCE_TYPE_LIST(VIRTUAL_INSTANCE_TYPE_WRAPPER)

#undef INSTANCE_TYPE_WRAPPER
#undef CODE_KIND_WRAPPER
#undef FIXED_ARRAY_SUB_INSTANCE_TYPE_WRAPPER
#undef VIRTUAL_INSTANCE_TYPE_WRAPPER
}

void ObjectStats::DumpInstanceTypeData(std::stringstream& stream,
                                       const char* name, int index) {
  stream << "\"" << name << "\":{";
  stream << "\"type\":" << static_cast<int>(index) << ",";
  stream << "\"overall\":" << object_sizes_[index] << ",";
  stream << "\"count\":" << object_counts_[index] << ",";
  stream << "\"over_allocated\":" << over_allocated_[index] << ",";
  stream << "\"histogram\":";
  DumpJSONArray(stream, size_histogram_[index], kNumberOfBuckets);
  stream << ",\"over_allocated_histogram\":";
  DumpJSONArray(stream, over_allocated_histogram_[index], kNumberOfBuckets);
  stream << "},";
}

void ObjectStats::Dump(std::stringstream& stream) {
  double time = isolate()->time_millis_since_init();
  int gc_count = heap()->gc_count();

  stream << "{";
  stream << "\"isolate\":\"" << reinterpret_cast<void*>(isolate()) << "\",";
  stream << "\"id\":" << gc_count << ",";
  stream << "\"time\":" << time << ",";
  stream << "\"bucket_sizes\":[";
  for (int i = 0; i < kNumberOfBuckets; i++) {
    stream << (1 << (kFirstBucketShift + i));
    if (i != (kNumberOfBuckets - 1)) stream << ",";
  }
  stream << "],";
  stream << "\"type_data\":{";

#define INSTANCE_TYPE_WRAPPER(name) DumpInstanceTypeData(stream, #name, name);
#define CODE_KIND_WRAPPER(name)                \
  DumpInstanceTypeData(stream, "*CODE_" #name, \
                       FIRST_CODE_KIND_SUB_TYPE + Code::name);

#define FIXED_ARRAY_SUB_INSTANCE_TYPE_WRAPPER(name)   \
  DumpInstanceTypeData(stream, "*FIXED_ARRAY_" #name, \
                       FIRST_FIXED_ARRAY_SUB_TYPE + name);
#define VIRTUAL_INSTANCE_TYPE_WRAPPER(name) \
  DumpInstanceTypeData(stream, #name, FIRST_VIRTUAL_TYPE + name);

  INSTANCE_TYPE_LIST(INSTANCE_TYPE_WRAPPER);
  CODE_KIND_LIST(CODE_KIND_WRAPPER);
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(FIXED_ARRAY_SUB_INSTANCE_TYPE_WRAPPER);
  VIRTUAL_INSTANCE_TYPE_LIST(VIRTUAL_INSTANCE_TYPE_WRAPPER)
  stream << "\"END\":{}}}";

#undef INSTANCE_TYPE_WRAPPER
#undef CODE_KIND_WRAPPER
#undef FIXED_ARRAY_SUB_INSTANCE_TYPE_WRAPPER
#undef VIRTUAL_INSTANCE_TYPE_WRAPPER
}

void ObjectStats::CheckpointObjectStats() {
  base::LockGuard<base::Mutex> lock_guard(object_stats_mutex.Pointer());
  MemCopy(object_counts_last_time_, object_counts_, sizeof(object_counts_));
  MemCopy(object_sizes_last_time_, object_sizes_, sizeof(object_sizes_));
  ClearObjectStats();
}

namespace {

int Log2ForSize(size_t size) {
  DCHECK_GT(size, 0);
  return kSizetSize * 8 - 1 - base::bits::CountLeadingZeros(size);
}

}  // namespace

int ObjectStats::HistogramIndexFromSize(size_t size) {
  if (size == 0) return 0;
  return Min(Max(Log2ForSize(size) + 1 - kFirstBucketShift, 0),
             kLastValueBucketIndex);
}

void ObjectStats::RecordObjectStats(InstanceType type, size_t size) {
  DCHECK_LE(type, LAST_TYPE);
  object_counts_[type]++;
  object_sizes_[type] += size;
  size_histogram_[type][HistogramIndexFromSize(size)]++;
}

void ObjectStats::RecordVirtualObjectStats(VirtualInstanceType type,
                                           size_t size) {
  DCHECK_LE(type, LAST_VIRTUAL_TYPE);
  object_counts_[FIRST_VIRTUAL_TYPE + type]++;
  object_sizes_[FIRST_VIRTUAL_TYPE + type] += size;
  size_histogram_[FIRST_VIRTUAL_TYPE + type][HistogramIndexFromSize(size)]++;
}

void ObjectStats::RecordCodeSubTypeStats(int code_sub_type, size_t size) {
  int code_sub_type_index = FIRST_CODE_KIND_SUB_TYPE + code_sub_type;
  DCHECK_GE(code_sub_type_index, FIRST_CODE_KIND_SUB_TYPE);
  DCHECK_LT(code_sub_type_index, FIRST_FIXED_ARRAY_SUB_TYPE);
  object_counts_[code_sub_type_index]++;
  object_sizes_[code_sub_type_index] += size;
  size_histogram_[code_sub_type_index][HistogramIndexFromSize(size)]++;
}

bool ObjectStats::RecordFixedArraySubTypeStats(FixedArrayBase* array,
                                               int array_sub_type, size_t size,
                                               size_t over_allocated) {
  auto it = visited_fixed_array_sub_types_.insert(array);
  if (!it.second) return false;
  DCHECK_LE(array_sub_type, LAST_FIXED_ARRAY_SUB_TYPE);
  object_counts_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type]++;
  object_sizes_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type] += size;
  size_histogram_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type]
                 [HistogramIndexFromSize(size)]++;
  if (over_allocated > 0) {
    InstanceType type =
        array->IsHashTable() ? HASH_TABLE_TYPE : FIXED_ARRAY_TYPE;
    over_allocated_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type] +=
        over_allocated;
    over_allocated_histogram_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type]
                             [HistogramIndexFromSize(over_allocated)]++;
    over_allocated_[type] += over_allocated;
    over_allocated_histogram_[type][HistogramIndexFromSize(over_allocated)]++;
  }
  return true;
}

Isolate* ObjectStats::isolate() { return heap()->isolate(); }

class ObjectStatsCollectorImpl {
 public:
  ObjectStatsCollectorImpl(Heap* heap, ObjectStats* stats);

  void CollectGlobalStatistics();

  // Collects statistics of objects for virtual instance types.
  void CollectVirtualStatistics(HeapObject* obj);

  // Collects statistics of objects for regular instance types.
  void CollectStatistics(HeapObject* obj);

 private:
  class CompilationCacheTableVisitor;

  void RecordObjectStats(HeapObject* obj, InstanceType type, size_t size);
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

  void RecordVirtualObjectStats(HeapObject* obj,
                                ObjectStats::VirtualInstanceType type,
                                size_t size);
  void RecordVirtualAllocationSiteDetails(AllocationSite* site);

  Heap* heap_;
  ObjectStats* stats_;
  MarkCompactCollector::NonAtomicMarkingState* marking_state_;
  std::unordered_set<HeapObject*> virtual_objects_;

  friend class ObjectStatsCollectorImpl::CompilationCacheTableVisitor;
};

ObjectStatsCollectorImpl::ObjectStatsCollectorImpl(Heap* heap,
                                                   ObjectStats* stats)
    : heap_(heap),
      stats_(stats),
      marking_state_(
          heap->mark_compact_collector()->non_atomic_marking_state()) {}

// For entries which shared the same instance type (historically FixedArrays)
// we do a pre-pass and create virtual instance types.
void ObjectStatsCollectorImpl::CollectVirtualStatistics(HeapObject* obj) {
  if (obj->IsAllocationSite()) {
    RecordVirtualAllocationSiteDetails(AllocationSite::cast(obj));
  }
}

void ObjectStatsCollectorImpl::RecordVirtualObjectStats(
    HeapObject* obj, ObjectStats::VirtualInstanceType type, size_t size) {
  virtual_objects_.insert(obj);
  stats_->RecordVirtualObjectStats(type, size);
}

void ObjectStatsCollectorImpl::RecordVirtualAllocationSiteDetails(
    AllocationSite* site) {
  if (!site->PointsToLiteral()) return;
  JSObject* boilerplate = site->boilerplate();
  if (boilerplate->IsJSArray()) {
    RecordVirtualObjectStats(boilerplate,
                             ObjectStats::JS_ARRAY_BOILERPLATE_TYPE,
                             boilerplate->Size());
    // Array boilerplates cannot have properties.
  } else {
    RecordVirtualObjectStats(boilerplate,
                             ObjectStats::JS_OBJECT_BOILERPLATE_TYPE,
                             boilerplate->Size());
    if (boilerplate->HasFastProperties()) {
      // We'll misclassify the empty_proeprty_array here. Given that there is a
      // single instance, this is neglible.
      PropertyArray* properties = boilerplate->property_array();
      RecordVirtualObjectStats(properties,
                               ObjectStats::BOILERPLATE_PROPERTY_ARRAY_TYPE,
                               properties->Size());
    } else {
      NameDictionary* properties = boilerplate->property_dictionary();
      RecordVirtualObjectStats(properties,
                               ObjectStats::BOILERPLATE_NAME_DICTIONARY_TYPE,
                               properties->Size());
    }
  }
  FixedArrayBase* elements = boilerplate->elements();
  // We skip COW elements since they are shared, and we are sure that if the
  // boilerplate exists there must have been at least one instantiation.
  if (!elements->IsCowArray()) {
    RecordVirtualObjectStats(elements, ObjectStats::BOILERPLATE_ELEMENTS_TYPE,
                             elements->Size());
  }
}

void ObjectStatsCollectorImpl::CollectStatistics(HeapObject* obj) {
  Map* map = obj->map();

  // Record for the InstanceType.
  int object_size = obj->Size();
  RecordObjectStats(obj, map->instance_type(), object_size);

  // Record specific sub types where possible.
  if (obj->IsMap()) RecordMapDetails(Map::cast(obj));
  if (obj->IsObjectTemplateInfo() || obj->IsFunctionTemplateInfo()) {
    RecordTemplateInfoDetails(TemplateInfo::cast(obj));
  }
  if (obj->IsBytecodeArray()) {
    RecordBytecodeArrayDetails(BytecodeArray::cast(obj));
  }
  if (obj->IsCode()) RecordCodeDetails(Code::cast(obj));
  if (obj->IsSharedFunctionInfo()) {
    RecordSharedFunctionInfoDetails(SharedFunctionInfo::cast(obj));
  }
  if (obj->IsFixedArray()) RecordFixedArrayDetails(FixedArray::cast(obj));
  if (obj->IsJSObject()) RecordJSObjectDetails(JSObject::cast(obj));
  if (obj->IsJSWeakCollection()) {
    RecordJSWeakCollectionDetails(JSWeakCollection::cast(obj));
  }
  if (obj->IsJSCollection()) {
    RecordJSCollectionDetails(JSObject::cast(obj));
  }
  if (obj->IsScript()) RecordScriptDetails(Script::cast(obj));
}

class ObjectStatsCollectorImpl::CompilationCacheTableVisitor
    : public RootVisitor {
 public:
  explicit CompilationCacheTableVisitor(ObjectStatsCollectorImpl* parent)
      : parent_(parent) {}

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    for (Object** current = start; current < end; current++) {
      HeapObject* obj = HeapObject::cast(*current);
      if (obj->IsUndefined(parent_->heap_->isolate())) continue;
      CHECK(obj->IsCompilationCacheTable());
      parent_->RecordHashTableHelper(nullptr, CompilationCacheTable::cast(obj),
                                     COMPILATION_CACHE_TABLE_SUB_TYPE);
    }
  }

 private:
  ObjectStatsCollectorImpl* parent_;
};

void ObjectStatsCollectorImpl::CollectGlobalStatistics() {
  // Global FixedArrays.
  RecordFixedArrayHelper(nullptr, heap_->weak_new_space_object_to_code_list(),
                         WEAK_NEW_SPACE_OBJECT_TO_CODE_SUB_TYPE, 0);
  RecordFixedArrayHelper(nullptr, heap_->serialized_objects(),
                         SERIALIZED_OBJECTS_SUB_TYPE, 0);
  RecordFixedArrayHelper(nullptr, heap_->number_string_cache(),
                         NUMBER_STRING_CACHE_SUB_TYPE, 0);
  RecordFixedArrayHelper(nullptr, heap_->single_character_string_cache(),
                         SINGLE_CHARACTER_STRING_CACHE_SUB_TYPE, 0);
  RecordFixedArrayHelper(nullptr, heap_->string_split_cache(),
                         STRING_SPLIT_CACHE_SUB_TYPE, 0);
  RecordFixedArrayHelper(nullptr, heap_->regexp_multiple_cache(),
                         REGEXP_MULTIPLE_CACHE_SUB_TYPE, 0);
  RecordFixedArrayHelper(nullptr, heap_->retained_maps(),
                         RETAINED_MAPS_SUB_TYPE, 0);

  // Global weak FixedArrays.
  RecordFixedArrayHelper(
      nullptr, WeakFixedArray::cast(heap_->noscript_shared_function_infos()),
      NOSCRIPT_SHARED_FUNCTION_INFOS_SUB_TYPE, 0);
  RecordFixedArrayHelper(nullptr, WeakFixedArray::cast(heap_->script_list()),
                         SCRIPT_LIST_SUB_TYPE, 0);

  // Global hash tables.
  RecordHashTableHelper(nullptr, heap_->string_table(), STRING_TABLE_SUB_TYPE);
  RecordHashTableHelper(nullptr, heap_->weak_object_to_code_table(),
                        OBJECT_TO_CODE_SUB_TYPE);
  RecordHashTableHelper(nullptr, heap_->code_stubs(),
                        CODE_STUBS_TABLE_SUB_TYPE);
  RecordHashTableHelper(nullptr, heap_->empty_property_dictionary(),
                        EMPTY_PROPERTIES_DICTIONARY_SUB_TYPE);
  CompilationCache* compilation_cache = heap_->isolate()->compilation_cache();
  CompilationCacheTableVisitor v(this);
  compilation_cache->Iterate(&v);
}

void ObjectStatsCollectorImpl::RecordObjectStats(HeapObject* obj,
                                                 InstanceType type,
                                                 size_t size) {
  if (virtual_objects_.find(obj) == virtual_objects_.end())
    stats_->RecordObjectStats(type, size);
}

static bool CanRecordFixedArray(Heap* heap, FixedArrayBase* array) {
  return array->map()->instance_type() == FIXED_ARRAY_TYPE &&
         array != heap->empty_fixed_array() &&
         array != heap->empty_sloppy_arguments_elements() &&
         array != heap->empty_slow_element_dictionary() &&
         array != heap->empty_property_dictionary();
}

static bool IsCowArray(Heap* heap, FixedArrayBase* array) {
  return array->map() == heap->fixed_cow_array_map();
}

bool ObjectStatsCollectorImpl::SameLiveness(HeapObject* obj1,
                                            HeapObject* obj2) {
  return obj1 == nullptr || obj2 == nullptr ||
         marking_state_->Color(obj1) == marking_state_->Color(obj2);
}

bool ObjectStatsCollectorImpl::RecordFixedArrayHelper(HeapObject* parent,
                                                      FixedArray* array,
                                                      int subtype,
                                                      size_t overhead) {
  if (SameLiveness(parent, array) && CanRecordFixedArray(heap_, array) &&
      !IsCowArray(heap_, array)) {
    return stats_->RecordFixedArraySubTypeStats(array, subtype, array->Size(),
                                                overhead);
  }
  return false;
}

void ObjectStatsCollectorImpl::RecursivelyRecordFixedArrayHelper(
    HeapObject* parent, FixedArray* array, int subtype) {
  if (RecordFixedArrayHelper(parent, array, subtype, 0)) {
    for (int i = 0; i < array->length(); i++) {
      if (array->get(i)->IsFixedArray()) {
        RecursivelyRecordFixedArrayHelper(
            parent, FixedArray::cast(array->get(i)), subtype);
      }
    }
  }
}

template <class HashTable>
void ObjectStatsCollectorImpl::RecordHashTableHelper(HeapObject* parent,
                                                     HashTable* array,
                                                     int subtype) {
  int used = array->NumberOfElements() * HashTable::kEntrySize * kPointerSize;
  CHECK_GE(array->Size(), used);
  size_t overhead = array->Size() - used -
                    HashTable::kElementsStartIndex * kPointerSize -
                    FixedArray::kHeaderSize;
  RecordFixedArrayHelper(parent, array, subtype, overhead);
}

void ObjectStatsCollectorImpl::RecordJSObjectDetails(JSObject* object) {
  size_t overhead = 0;
  FixedArrayBase* elements = object->elements();
  if (CanRecordFixedArray(heap_, elements) && !IsCowArray(heap_, elements)) {
    if (elements->IsDictionary() && SameLiveness(object, elements)) {
      NumberDictionary* dict = NumberDictionary::cast(elements);
      RecordHashTableHelper(object, dict, DICTIONARY_ELEMENTS_SUB_TYPE);
    } else {
      if (IsHoleyElementsKind(object->GetElementsKind())) {
        int used = object->GetFastElementsUsage() * kPointerSize;
        if (object->GetElementsKind() == HOLEY_DOUBLE_ELEMENTS) used *= 2;
        CHECK_GE(elements->Size(), used);
        overhead = elements->Size() - used - FixedArray::kHeaderSize;
      }
      stats_->RecordFixedArraySubTypeStats(elements, PACKED_ELEMENTS_SUB_TYPE,
                                           elements->Size(), overhead);
    }
  }

  if (object->IsJSGlobalObject()) {
    GlobalDictionary* properties =
        JSGlobalObject::cast(object)->global_dictionary();
    if (CanRecordFixedArray(heap_, properties) &&
        SameLiveness(object, properties)) {
      RecordHashTableHelper(object, properties, DICTIONARY_PROPERTIES_SUB_TYPE);
    }
  } else if (!object->HasFastProperties()) {
    NameDictionary* properties = object->property_dictionary();
    if (CanRecordFixedArray(heap_, properties) &&
        SameLiveness(object, properties)) {
      RecordHashTableHelper(object, properties, DICTIONARY_PROPERTIES_SUB_TYPE);
    }
  }
}

void ObjectStatsCollectorImpl::RecordJSWeakCollectionDetails(
    JSWeakCollection* obj) {
  if (obj->table()->IsHashTable()) {
    ObjectHashTable* table = ObjectHashTable::cast(obj->table());
    int used = table->NumberOfElements() * ObjectHashTable::kEntrySize;
    size_t overhead = table->Size() - used;
    RecordFixedArrayHelper(obj, table, JS_WEAK_COLLECTION_SUB_TYPE, overhead);
  }
}

void ObjectStatsCollectorImpl::RecordJSCollectionDetails(JSObject* obj) {
  // The JS versions use a different HashTable implementation that cannot use
  // the regular helper. Since overall impact is usually small just record
  // without overhead.
  if (obj->IsJSMap()) {
    RecordFixedArrayHelper(nullptr, FixedArray::cast(JSMap::cast(obj)->table()),
                           JS_COLLECTION_SUB_TYPE, 0);
  }
  if (obj->IsJSSet()) {
    RecordFixedArrayHelper(nullptr, FixedArray::cast(JSSet::cast(obj)->table()),
                           JS_COLLECTION_SUB_TYPE, 0);
  }
}

void ObjectStatsCollectorImpl::RecordScriptDetails(Script* obj) {
  FixedArray* infos = FixedArray::cast(obj->shared_function_infos());
  RecordFixedArrayHelper(obj, infos, SHARED_FUNCTION_INFOS_SUB_TYPE, 0);
}

void ObjectStatsCollectorImpl::RecordMapDetails(Map* map_obj) {
  DescriptorArray* array = map_obj->instance_descriptors();
  if (map_obj->owns_descriptors() && array != heap_->empty_descriptor_array() &&
      SameLiveness(map_obj, array)) {
    RecordFixedArrayHelper(map_obj, array, DESCRIPTOR_ARRAY_SUB_TYPE, 0);
    EnumCache* enum_cache = array->GetEnumCache();
    RecordFixedArrayHelper(array, enum_cache->keys(), ENUM_CACHE_SUB_TYPE, 0);
    RecordFixedArrayHelper(array, enum_cache->indices(),
                           ENUM_INDICES_CACHE_SUB_TYPE, 0);
  }

  for (DependentCode* cur_dependent_code = map_obj->dependent_code();
       cur_dependent_code != heap_->empty_fixed_array();
       cur_dependent_code = DependentCode::cast(
           cur_dependent_code->get(DependentCode::kNextLinkIndex))) {
    RecordFixedArrayHelper(map_obj, cur_dependent_code, DEPENDENT_CODE_SUB_TYPE,
                           0);
  }

  if (map_obj->is_prototype_map()) {
    if (map_obj->prototype_info()->IsPrototypeInfo()) {
      PrototypeInfo* info = PrototypeInfo::cast(map_obj->prototype_info());
      Object* users = info->prototype_users();
      if (users->IsWeakFixedArray()) {
        RecordFixedArrayHelper(map_obj, WeakFixedArray::cast(users),
                               PROTOTYPE_USERS_SUB_TYPE, 0);
      }
    }
  }
}

void ObjectStatsCollectorImpl::RecordTemplateInfoDetails(TemplateInfo* obj) {
  if (obj->property_accessors()->IsFixedArray()) {
    RecordFixedArrayHelper(obj, FixedArray::cast(obj->property_accessors()),
                           TEMPLATE_INFO_SUB_TYPE, 0);
  }
  if (obj->property_list()->IsFixedArray()) {
    RecordFixedArrayHelper(obj, FixedArray::cast(obj->property_list()),
                           TEMPLATE_INFO_SUB_TYPE, 0);
  }
}

void ObjectStatsCollectorImpl::RecordBytecodeArrayDetails(BytecodeArray* obj) {
  RecordFixedArrayHelper(obj, obj->constant_pool(),
                         BYTECODE_ARRAY_CONSTANT_POOL_SUB_TYPE, 0);
  RecordFixedArrayHelper(obj, obj->handler_table(),
                         BYTECODE_ARRAY_HANDLER_TABLE_SUB_TYPE, 0);
}

void ObjectStatsCollectorImpl::RecordCodeDetails(Code* code) {
  stats_->RecordCodeSubTypeStats(code->kind(), code->Size());
  RecordFixedArrayHelper(code, code->deoptimization_data(),
                         DEOPTIMIZATION_DATA_SUB_TYPE, 0);
  if (code->kind() == Code::Kind::OPTIMIZED_FUNCTION) {
    DeoptimizationData* input_data =
        DeoptimizationData::cast(code->deoptimization_data());
    if (input_data->length() > 0) {
      RecordFixedArrayHelper(code->deoptimization_data(),
                             input_data->LiteralArray(),
                             OPTIMIZED_CODE_LITERALS_SUB_TYPE, 0);
    }
  }
  RecordFixedArrayHelper(code, code->handler_table(), HANDLER_TABLE_SUB_TYPE,
                         0);
  int const mode_mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(code, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (mode == RelocInfo::EMBEDDED_OBJECT) {
      Object* target = it.rinfo()->target_object();
      if (target->IsFixedArray()) {
        RecursivelyRecordFixedArrayHelper(code, FixedArray::cast(target),
                                          EMBEDDED_OBJECT_SUB_TYPE);
      }
    }
  }
}

void ObjectStatsCollectorImpl::RecordSharedFunctionInfoDetails(
    SharedFunctionInfo* sfi) {
  FixedArray* scope_info = sfi->scope_info();
  RecordFixedArrayHelper(sfi, scope_info, SCOPE_INFO_SUB_TYPE, 0);
  FeedbackMetadata* feedback_metadata = sfi->feedback_metadata();
  if (!feedback_metadata->is_empty()) {
    RecordFixedArrayHelper(sfi, feedback_metadata, FEEDBACK_METADATA_SUB_TYPE,
                           0);
  }
}

void ObjectStatsCollectorImpl::RecordFixedArrayDetails(FixedArray* array) {
  if (array->IsContext()) {
    RecordFixedArrayHelper(nullptr, array, CONTEXT_SUB_TYPE, 0);
  }
  if (IsCowArray(heap_, array) && CanRecordFixedArray(heap_, array)) {
    stats_->RecordFixedArraySubTypeStats(array, COPY_ON_WRITE_SUB_TYPE,
                                         array->Size(), 0);
  }
  if (array->IsNativeContext()) {
    Context* native_ctx = Context::cast(array);
    RecordHashTableHelper(array,
                          native_ctx->slow_template_instantiations_cache(),
                          SLOW_TEMPLATE_INSTANTIATIONS_CACHE_SUB_TYPE);
    FixedArray* fast_cache = native_ctx->fast_template_instantiations_cache();
    stats_->RecordFixedArraySubTypeStats(
        fast_cache, FAST_TEMPLATE_INSTANTIATIONS_CACHE_SUB_TYPE,
        fast_cache->Size(), 0);
  }
}

class ObjectStatsVisitor {
 public:
  enum CollectionMode {
    kRegular,
    kVirtual,
  };

  ObjectStatsVisitor(Heap* heap, ObjectStatsCollectorImpl* live_collector,
                     ObjectStatsCollectorImpl* dead_collector,
                     CollectionMode mode)
      : live_collector_(live_collector),
        dead_collector_(dead_collector),
        marking_state_(
            heap->mark_compact_collector()->non_atomic_marking_state()),
        mode_(mode) {}

  bool Visit(HeapObject* obj, int size) {
    if (marking_state_->IsBlack(obj)) {
      Collect(live_collector_, obj);
    } else {
      DCHECK(!marking_state_->IsGrey(obj));
      Collect(dead_collector_, obj);
    }
    return true;
  }

 private:
  void Collect(ObjectStatsCollectorImpl* collector, HeapObject* obj) {
    switch (mode_) {
      case kRegular:
        collector->CollectStatistics(obj);
        break;
      case kVirtual:
        collector->CollectVirtualStatistics(obj);
        break;
    }
  }

  ObjectStatsCollectorImpl* live_collector_;
  ObjectStatsCollectorImpl* dead_collector_;
  MarkCompactCollector::NonAtomicMarkingState* marking_state_;
  CollectionMode mode_;
};

namespace {

void IterateHeap(Heap* heap, ObjectStatsVisitor* visitor) {
  SpaceIterator space_it(heap);
  HeapObject* obj = nullptr;
  while (space_it.has_next()) {
    std::unique_ptr<ObjectIterator> it(space_it.next()->GetObjectIterator());
    ObjectIterator* obj_it = it.get();
    while ((obj = obj_it->Next()) != nullptr) {
      visitor->Visit(obj, obj->Size());
    }
  }
}

}  // namespace

void ObjectStatsCollector::Collect() {
  ObjectStatsCollectorImpl live_collector(heap_, live_);
  ObjectStatsCollectorImpl dead_collector(heap_, dead_);
  // 1. Collect system type otherwise indistinguishable from other types.
  {
    ObjectStatsVisitor visitor(heap_, &live_collector, &dead_collector,
                               ObjectStatsVisitor::kVirtual);
    IterateHeap(heap_, &visitor);
  }

  // 2. Collect globals; only applies to live objects.
  live_collector.CollectGlobalStatistics();
  // 3. Collect rest.
  {
    ObjectStatsVisitor visitor(heap_, &live_collector, &dead_collector,
                               ObjectStatsVisitor::kRegular);
    IterateHeap(heap_, &visitor);
  }
}

}  // namespace internal
}  // namespace v8
