// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/object-stats.h"

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

  INSTANCE_TYPE_LIST(INSTANCE_TYPE_WRAPPER)
  CODE_KIND_LIST(CODE_KIND_WRAPPER)
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(FIXED_ARRAY_SUB_INSTANCE_TYPE_WRAPPER)

#undef INSTANCE_TYPE_WRAPPER
#undef CODE_KIND_WRAPPER
#undef FIXED_ARRAY_SUB_INSTANCE_TYPE_WRAPPER
#undef PRINT_INSTANCE_TYPE_DATA
#undef PRINT_KEY_AND_ID
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

  INSTANCE_TYPE_LIST(INSTANCE_TYPE_WRAPPER);
  CODE_KIND_LIST(CODE_KIND_WRAPPER);
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(FIXED_ARRAY_SUB_INSTANCE_TYPE_WRAPPER);
  stream << "\"END\":{}}}";

#undef INSTANCE_TYPE_WRAPPER
#undef CODE_KIND_WRAPPER
#undef FIXED_ARRAY_SUB_INSTANCE_TYPE_WRAPPER
#undef PRINT_INSTANCE_TYPE_DATA
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

ObjectStatsCollector::ObjectStatsCollector(Heap* heap, ObjectStats* stats)
    : heap_(heap),
      stats_(stats),
      marking_state_(
          heap->mark_compact_collector()->non_atomic_marking_state()) {}

void ObjectStatsCollector::CollectStatistics(HeapObject* obj) {
  Map* map = obj->map();

  // Record for the InstanceType.
  int object_size = obj->Size();
  stats_->RecordObjectStats(map->instance_type(), object_size);

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

class ObjectStatsCollector::CompilationCacheTableVisitor : public RootVisitor {
 public:
  explicit CompilationCacheTableVisitor(ObjectStatsCollector* parent)
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
  ObjectStatsCollector* parent_;
};

void ObjectStatsCollector::CollectGlobalStatistics() {
  // Global FixedArrays.
  RecordFixedArrayHelper(nullptr, heap_->weak_new_space_object_to_code_list(),
                         WEAK_NEW_SPACE_OBJECT_TO_CODE_SUB_TYPE, 0);
  RecordFixedArrayHelper(nullptr, heap_->serialized_templates(),
                         SERIALIZED_TEMPLATES_SUB_TYPE, 0);
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

bool ObjectStatsCollector::SameLiveness(HeapObject* obj1, HeapObject* obj2) {
  return obj1 == nullptr || obj2 == nullptr ||
         marking_state_->Color(obj1) == marking_state_->Color(obj2);
}

bool ObjectStatsCollector::RecordFixedArrayHelper(HeapObject* parent,
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

void ObjectStatsCollector::RecursivelyRecordFixedArrayHelper(HeapObject* parent,
                                                             FixedArray* array,
                                                             int subtype) {
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
void ObjectStatsCollector::RecordHashTableHelper(HeapObject* parent,
                                                 HashTable* array,
                                                 int subtype) {
  int used = array->NumberOfElements() * HashTable::kEntrySize * kPointerSize;
  CHECK_GE(array->Size(), used);
  size_t overhead = array->Size() - used -
                    HashTable::kElementsStartIndex * kPointerSize -
                    FixedArray::kHeaderSize;
  RecordFixedArrayHelper(parent, array, subtype, overhead);
}

void ObjectStatsCollector::RecordJSObjectDetails(JSObject* object) {
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

void ObjectStatsCollector::RecordJSWeakCollectionDetails(
    JSWeakCollection* obj) {
  if (obj->table()->IsHashTable()) {
    ObjectHashTable* table = ObjectHashTable::cast(obj->table());
    int used = table->NumberOfElements() * ObjectHashTable::kEntrySize;
    size_t overhead = table->Size() - used;
    RecordFixedArrayHelper(obj, table, JS_WEAK_COLLECTION_SUB_TYPE, overhead);
  }
}

void ObjectStatsCollector::RecordJSCollectionDetails(JSObject* obj) {
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

void ObjectStatsCollector::RecordScriptDetails(Script* obj) {
  FixedArray* infos = FixedArray::cast(obj->shared_function_infos());
  RecordFixedArrayHelper(obj, infos, SHARED_FUNCTION_INFOS_SUB_TYPE, 0);
}

void ObjectStatsCollector::RecordMapDetails(Map* map_obj) {
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

void ObjectStatsCollector::RecordTemplateInfoDetails(TemplateInfo* obj) {
  if (obj->property_accessors()->IsFixedArray()) {
    RecordFixedArrayHelper(obj, FixedArray::cast(obj->property_accessors()),
                           TEMPLATE_INFO_SUB_TYPE, 0);
  }
  if (obj->property_list()->IsFixedArray()) {
    RecordFixedArrayHelper(obj, FixedArray::cast(obj->property_list()),
                           TEMPLATE_INFO_SUB_TYPE, 0);
  }
}

void ObjectStatsCollector::RecordBytecodeArrayDetails(BytecodeArray* obj) {
  RecordFixedArrayHelper(obj, obj->constant_pool(),
                         BYTECODE_ARRAY_CONSTANT_POOL_SUB_TYPE, 0);
  RecordFixedArrayHelper(obj, obj->handler_table(),
                         BYTECODE_ARRAY_HANDLER_TABLE_SUB_TYPE, 0);
}

void ObjectStatsCollector::RecordCodeDetails(Code* code) {
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

void ObjectStatsCollector::RecordSharedFunctionInfoDetails(
    SharedFunctionInfo* sfi) {
  FixedArray* scope_info = sfi->scope_info();
  RecordFixedArrayHelper(sfi, scope_info, SCOPE_INFO_SUB_TYPE, 0);
  FeedbackMetadata* feedback_metadata = sfi->feedback_metadata();
  if (!feedback_metadata->is_empty()) {
    RecordFixedArrayHelper(sfi, feedback_metadata, FEEDBACK_METADATA_SUB_TYPE,
                           0);
  }
}

void ObjectStatsCollector::RecordFixedArrayDetails(FixedArray* array) {
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

}  // namespace internal
}  // namespace v8
