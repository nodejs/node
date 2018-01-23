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

#define VIRTUAL_INSTANCE_TYPE_WRAPPER(name) \
  PrintInstanceTypeJSON(key, gc_count, #name, FIRST_VIRTUAL_TYPE + name);

  INSTANCE_TYPE_LIST(INSTANCE_TYPE_WRAPPER)
  VIRTUAL_INSTANCE_TYPE_LIST(VIRTUAL_INSTANCE_TYPE_WRAPPER)

#undef INSTANCE_TYPE_WRAPPER
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

#define VIRTUAL_INSTANCE_TYPE_WRAPPER(name) \
  DumpInstanceTypeData(stream, #name, FIRST_VIRTUAL_TYPE + name);

  INSTANCE_TYPE_LIST(INSTANCE_TYPE_WRAPPER);
  VIRTUAL_INSTANCE_TYPE_LIST(VIRTUAL_INSTANCE_TYPE_WRAPPER)
  stream << "\"END\":{}}}";

#undef INSTANCE_TYPE_WRAPPER
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
                                           size_t size, size_t over_allocated) {
  DCHECK_LE(type, LAST_VIRTUAL_TYPE);
  object_counts_[FIRST_VIRTUAL_TYPE + type]++;
  object_sizes_[FIRST_VIRTUAL_TYPE + type] += size;
  size_histogram_[FIRST_VIRTUAL_TYPE + type][HistogramIndexFromSize(size)]++;
  over_allocated_[FIRST_VIRTUAL_TYPE + type] += over_allocated;
  over_allocated_histogram_[FIRST_VIRTUAL_TYPE + type]
                           [HistogramIndexFromSize(size)]++;
}

Isolate* ObjectStats::isolate() { return heap()->isolate(); }

class ObjectStatsCollectorImpl {
 public:
  enum Phase {
    kPhase1,
    kPhase2,
  };
  static const int kNumberOfPhases = kPhase2 + 1;

  ObjectStatsCollectorImpl(Heap* heap, ObjectStats* stats);

  void CollectGlobalStatistics();
  void CollectStatistics(HeapObject* obj, Phase phase);

 private:
  enum CowMode {
    kCheckCow,
    kIgnoreCow,
  };

  void RecordVirtualObjectStats(HeapObject* parent, HeapObject* obj,
                                ObjectStats::VirtualInstanceType type,
                                size_t size, size_t over_allocated);
  // Gets size from |ob| and assumes no over allocating.
  void RecordSimpleVirtualObjectStats(HeapObject* parent, HeapObject* obj,
                                      ObjectStats::VirtualInstanceType type);
  // For HashTable it is possible to compute over allocated memory.
  void RecordHashTableVirtualObjectStats(HeapObject* parent,
                                         FixedArray* hash_table,
                                         ObjectStats::VirtualInstanceType type);

  bool SameLiveness(HeapObject* obj1, HeapObject* obj2);
  bool CanRecordFixedArray(FixedArrayBase* array);
  bool IsCowArray(FixedArrayBase* array);

  // Blacklist for objects that should not be recorded using
  // VirtualObjectStats and RecordSimpleVirtualObjectStats. For recording those
  // objects dispatch to the low level ObjectStats::RecordObjectStats manually.
  bool ShouldRecordObject(HeapObject* object, CowMode check_cow_array);

  void RecordObjectStats(HeapObject* obj, InstanceType type, size_t size);

  // Details.
  void RecordVirtualAllocationSiteDetails(AllocationSite* site);
  void RecordVirtualBytecodeArrayDetails(BytecodeArray* bytecode);
  void RecordVirtualCodeDetails(Code* code);
  void RecordVirtualContext(Context* context);
  void RecordVirtualFeedbackVectorDetails(FeedbackVector* vector);
  void RecordVirtualFixedArrayDetails(FixedArray* array);
  void RecordVirtualJSGlobalObjectDetails(JSGlobalObject* object);
  void RecordVirtualJSCollectionDetails(JSObject* object);
  void RecordVirtualJSObjectDetails(JSObject* object);
  void RecordVirtualMapDetails(Map* map);
  void RecordVirtualSharedFunctionInfoDetails(SharedFunctionInfo* info);

  Heap* heap_;
  ObjectStats* stats_;
  MarkCompactCollector::NonAtomicMarkingState* marking_state_;
  std::unordered_set<HeapObject*> virtual_objects_;
};

ObjectStatsCollectorImpl::ObjectStatsCollectorImpl(Heap* heap,
                                                   ObjectStats* stats)
    : heap_(heap),
      stats_(stats),
      marking_state_(
          heap->mark_compact_collector()->non_atomic_marking_state()) {}

bool ObjectStatsCollectorImpl::ShouldRecordObject(HeapObject* obj,
                                                  CowMode check_cow_array) {
  if (obj->IsFixedArray()) {
    FixedArray* fixed_array = FixedArray::cast(obj);
    bool cow_check = check_cow_array == kIgnoreCow || !IsCowArray(fixed_array);
    return CanRecordFixedArray(fixed_array) && cow_check;
  }
  if (obj == heap_->empty_property_array()) return false;
  return true;
}

void ObjectStatsCollectorImpl::RecordHashTableVirtualObjectStats(
    HeapObject* parent, FixedArray* hash_table,
    ObjectStats::VirtualInstanceType type) {
  CHECK(hash_table->IsHashTable());
  // TODO(mlippautz): Implement over allocation for hash tables.
  RecordVirtualObjectStats(parent, hash_table, type, hash_table->Size(),
                           ObjectStats::kNoOverAllocation);
}

void ObjectStatsCollectorImpl::RecordSimpleVirtualObjectStats(
    HeapObject* parent, HeapObject* obj,
    ObjectStats::VirtualInstanceType type) {
  RecordVirtualObjectStats(parent, obj, type, obj->Size(),
                           ObjectStats::kNoOverAllocation);
}

void ObjectStatsCollectorImpl::RecordVirtualObjectStats(
    HeapObject* parent, HeapObject* obj, ObjectStats::VirtualInstanceType type,
    size_t size, size_t over_allocated) {
  if (!SameLiveness(parent, obj) || !ShouldRecordObject(obj, kCheckCow)) return;

  if (virtual_objects_.find(obj) == virtual_objects_.end()) {
    virtual_objects_.insert(obj);
    stats_->RecordVirtualObjectStats(type, size, over_allocated);
  }
}

void ObjectStatsCollectorImpl::RecordVirtualAllocationSiteDetails(
    AllocationSite* site) {
  if (!site->PointsToLiteral()) return;
  JSObject* boilerplate = site->boilerplate();
  if (boilerplate->IsJSArray()) {
    RecordSimpleVirtualObjectStats(site, boilerplate,
                                   ObjectStats::JS_ARRAY_BOILERPLATE_TYPE);
    // Array boilerplates cannot have properties.
  } else {
    RecordVirtualObjectStats(
        site, boilerplate, ObjectStats::JS_OBJECT_BOILERPLATE_TYPE,
        boilerplate->Size(), ObjectStats::kNoOverAllocation);
    if (boilerplate->HasFastProperties()) {
      // We'll mis-classify the empty_property_array here. Given that there is a
      // single instance, this is negligible.
      PropertyArray* properties = boilerplate->property_array();
      RecordSimpleVirtualObjectStats(
          site, properties, ObjectStats::BOILERPLATE_PROPERTY_ARRAY_TYPE);
    } else {
      NameDictionary* properties = boilerplate->property_dictionary();
      RecordSimpleVirtualObjectStats(
          site, properties, ObjectStats::BOILERPLATE_PROPERTY_DICTIONARY_TYPE);
    }
  }
  FixedArrayBase* elements = boilerplate->elements();
  RecordSimpleVirtualObjectStats(site, elements,
                                 ObjectStats::BOILERPLATE_ELEMENTS_TYPE);
}

void ObjectStatsCollectorImpl::RecordVirtualJSGlobalObjectDetails(
    JSGlobalObject* object) {
  // Properties.
  GlobalDictionary* properties = object->global_dictionary();
  RecordHashTableVirtualObjectStats(object, properties,
                                    ObjectStats::GLOBAL_PROPERTIES_TYPE);
  // Elements.
  FixedArrayBase* elements = object->elements();
  RecordSimpleVirtualObjectStats(object, elements,
                                 ObjectStats::GLOBAL_ELEMENTS_TYPE);
}

void ObjectStatsCollectorImpl::RecordVirtualJSCollectionDetails(
    JSObject* object) {
  if (object->IsJSMap()) {
    RecordSimpleVirtualObjectStats(
        object, FixedArray::cast(JSMap::cast(object)->table()),
        ObjectStats::JS_COLLETION_TABLE_TYPE);
  }
  if (object->IsJSSet()) {
    RecordSimpleVirtualObjectStats(
        object, FixedArray::cast(JSSet::cast(object)->table()),
        ObjectStats::JS_COLLETION_TABLE_TYPE);
  }
}

void ObjectStatsCollectorImpl::RecordVirtualJSObjectDetails(JSObject* object) {
  // JSGlobalObject is recorded separately.
  if (object->IsJSGlobalObject()) return;

  // Properties.
  if (object->HasFastProperties()) {
    PropertyArray* properties = object->property_array();
    RecordSimpleVirtualObjectStats(object, properties,
                                   ObjectStats::PROPERTY_ARRAY_TYPE);
  } else {
    NameDictionary* properties = object->property_dictionary();
    RecordHashTableVirtualObjectStats(object, properties,
                                      ObjectStats::PROPERTY_DICTIONARY_TYPE);
  }
  // Elements.
  FixedArrayBase* elements = object->elements();
  RecordSimpleVirtualObjectStats(object, elements, ObjectStats::ELEMENTS_TYPE);
}

void ObjectStatsCollectorImpl::RecordVirtualFeedbackVectorDetails(
    FeedbackVector* vector) {
  // Except for allocation
  for (int i = 0; i < vector->length(); i++) {
    Object* raw_object = vector->get(i);
    if (!raw_object->IsHeapObject()) continue;
    HeapObject* object = HeapObject::cast(raw_object);
    if (object->IsCell() || object->IsFixedArray()) {
      RecordSimpleVirtualObjectStats(vector, object,
                                     ObjectStats::FEEDBACK_VECTOR_ENTRY_TYPE);
    }
  }
}

void ObjectStatsCollectorImpl::RecordVirtualFixedArrayDetails(
    FixedArray* array) {
  if (IsCowArray(array)) {
    // Manually check and dispatch to lower level recording due to COW array
    // check. No need for SameLiveness as we call it on a single object.
    if (!ShouldRecordObject(array, kIgnoreCow)) return;

    if (virtual_objects_.find(array) == virtual_objects_.end()) {
      virtual_objects_.insert(array);
      stats_->RecordVirtualObjectStats(ObjectStats::COW_ARRAY_TYPE,
                                       array->Size(),
                                       ObjectStats::kNoOverAllocation);
    }
  }
}

void ObjectStatsCollectorImpl::CollectStatistics(HeapObject* obj, Phase phase) {
  Map* map = obj->map();
  switch (phase) {
    case kPhase1:
      if (obj->IsFeedbackVector()) {
        RecordVirtualFeedbackVectorDetails(FeedbackVector::cast(obj));
      } else if (obj->IsMap()) {
        RecordVirtualMapDetails(Map::cast(obj));
      } else if (obj->IsBytecodeArray()) {
        RecordVirtualBytecodeArrayDetails(BytecodeArray::cast(obj));
      } else if (obj->IsCode()) {
        RecordVirtualCodeDetails(Code::cast(obj));
      } else if (obj->IsJSGlobalObject()) {
        RecordVirtualJSGlobalObjectDetails(JSGlobalObject::cast(obj));
      } else if (obj->IsJSObject()) {
        // This phase needs to come after RecordVirtualAllocationSiteDetails
        // to properly split among boilerplates.
        RecordVirtualJSObjectDetails(JSObject::cast(obj));
      } else if (obj->IsJSCollection()) {
        RecordVirtualJSCollectionDetails(JSObject::cast(obj));
      } else if (obj->IsSharedFunctionInfo()) {
        RecordVirtualSharedFunctionInfoDetails(SharedFunctionInfo::cast(obj));
      } else if (obj->IsContext()) {
        RecordVirtualContext(Context::cast(obj));
      } else if (obj->IsFixedArray()) {
        // Has to go last as it triggers too eagerly.
        RecordVirtualFixedArrayDetails(FixedArray::cast(obj));
      }
      break;
    case kPhase2:
      RecordObjectStats(obj, map->instance_type(), obj->Size());
      break;
  }
}

void ObjectStatsCollectorImpl::CollectGlobalStatistics() {
  // Iterate boilerplates first to disambiguate them from regular JS objects.
  Object* list = heap_->allocation_sites_list();
  while (list->IsAllocationSite()) {
    AllocationSite* site = AllocationSite::cast(list);
    RecordVirtualAllocationSiteDetails(site);
    list = site->weak_next();
  }

  // FixedArray.
  RecordSimpleVirtualObjectStats(
      nullptr, heap_->weak_new_space_object_to_code_list(),
      ObjectStats::WEAK_NEW_SPACE_OBJECT_TO_CODE_TYPE);
  RecordSimpleVirtualObjectStats(nullptr, heap_->serialized_objects(),
                                 ObjectStats::SERIALIZED_OBJECTS_TYPE);
  RecordSimpleVirtualObjectStats(nullptr, heap_->number_string_cache(),
                                 ObjectStats::NUMBER_STRING_CACHE_TYPE);
  RecordSimpleVirtualObjectStats(
      nullptr, heap_->single_character_string_cache(),
      ObjectStats::SINGLE_CHARACTER_STRING_CACHE_TYPE);
  RecordSimpleVirtualObjectStats(nullptr, heap_->string_split_cache(),
                                 ObjectStats::STRING_SPLIT_CACHE_TYPE);
  RecordSimpleVirtualObjectStats(nullptr, heap_->regexp_multiple_cache(),
                                 ObjectStats::REGEXP_MULTIPLE_CACHE_TYPE);
  RecordSimpleVirtualObjectStats(nullptr, heap_->retained_maps(),
                                 ObjectStats::RETAINED_MAPS_TYPE);

  // WeakFixedArray.
  RecordSimpleVirtualObjectStats(
      nullptr, WeakFixedArray::cast(heap_->noscript_shared_function_infos()),
      ObjectStats::NOSCRIPT_SHARED_FUNCTION_INFOS_TYPE);
  RecordSimpleVirtualObjectStats(nullptr,
                                 WeakFixedArray::cast(heap_->script_list()),
                                 ObjectStats::SCRIPT_LIST_TYPE);

  // HashTable.
  RecordHashTableVirtualObjectStats(nullptr, heap_->string_table(),
                                    ObjectStats::STRING_TABLE_TYPE);
  RecordHashTableVirtualObjectStats(nullptr, heap_->code_stubs(),
                                    ObjectStats::CODE_STUBS_TABLE_TYPE);

  // WeakHashTable.
  RecordHashTableVirtualObjectStats(nullptr, heap_->weak_object_to_code_table(),
                                    ObjectStats::OBJECT_TO_CODE_TYPE);
}

void ObjectStatsCollectorImpl::RecordObjectStats(HeapObject* obj,
                                                 InstanceType type,
                                                 size_t size) {
  if (virtual_objects_.find(obj) == virtual_objects_.end()) {
    stats_->RecordObjectStats(type, size);
  }
}

bool ObjectStatsCollectorImpl::CanRecordFixedArray(FixedArrayBase* array) {
  return array != heap_->empty_fixed_array() &&
         array != heap_->empty_sloppy_arguments_elements() &&
         array != heap_->empty_slow_element_dictionary() &&
         array != heap_->empty_property_dictionary();
}

bool ObjectStatsCollectorImpl::IsCowArray(FixedArrayBase* array) {
  return array->map() == heap_->fixed_cow_array_map();
}

bool ObjectStatsCollectorImpl::SameLiveness(HeapObject* obj1,
                                            HeapObject* obj2) {
  return obj1 == nullptr || obj2 == nullptr ||
         marking_state_->Color(obj1) == marking_state_->Color(obj2);
}

void ObjectStatsCollectorImpl::RecordVirtualMapDetails(Map* map) {
  // TODO(mlippautz): map->dependent_code(): DEPENDENT_CODE_TYPE.

  DescriptorArray* array = map->instance_descriptors();
  if (map->owns_descriptors() && array != heap_->empty_descriptor_array()) {
    // DescriptorArray has its own instance type.
    EnumCache* enum_cache = array->GetEnumCache();
    RecordSimpleVirtualObjectStats(array, enum_cache->keys(),
                                   ObjectStats::ENUM_CACHE_TYPE);
    RecordSimpleVirtualObjectStats(array, enum_cache->indices(),
                                   ObjectStats::ENUM_INDICES_CACHE_TYPE);
  }

  if (map->is_prototype_map()) {
    if (map->prototype_info()->IsPrototypeInfo()) {
      PrototypeInfo* info = PrototypeInfo::cast(map->prototype_info());
      Object* users = info->prototype_users();
      if (users->IsWeakFixedArray()) {
        RecordSimpleVirtualObjectStats(map, WeakFixedArray::cast(users),
                                       ObjectStats::PROTOTYPE_USERS_TYPE);
      }
    }
  }
}

void ObjectStatsCollectorImpl::RecordVirtualSharedFunctionInfoDetails(
    SharedFunctionInfo* info) {
  // SharedFunctonInfo::feedback_metadata() is a COW array.
  RecordSimpleVirtualObjectStats(info, info->scope_info(),
                                 ObjectStats::SCOPE_INFO_TYPE);
}

void ObjectStatsCollectorImpl::RecordVirtualBytecodeArrayDetails(
    BytecodeArray* bytecode) {
  RecordVirtualObjectStats(bytecode, bytecode->constant_pool(),
                           ObjectStats::BYTECODE_ARRAY_CONSTANT_POOL_TYPE,
                           bytecode->constant_pool()->Size(),
                           ObjectStats::kNoOverAllocation);
  RecordVirtualObjectStats(bytecode, bytecode->handler_table(),
                           ObjectStats::BYTECODE_ARRAY_HANDLER_TABLE_TYPE,
                           bytecode->constant_pool()->Size(),
                           ObjectStats::kNoOverAllocation);
}

namespace {

ObjectStats::VirtualInstanceType CodeKindToVirtualInstanceType(
    Code::Kind kind) {
  switch (kind) {
#define CODE_KIND_CASE(type) \
  case Code::type:           \
    return ObjectStats::type;
    CODE_KIND_LIST(CODE_KIND_CASE)
#undef CODE_KIND_CASE
    default:
      UNREACHABLE();
  }
  UNREACHABLE();
}

}  // namespace

void ObjectStatsCollectorImpl::RecordVirtualCodeDetails(Code* code) {
  RecordVirtualObjectStats(nullptr, code,
                           CodeKindToVirtualInstanceType(code->kind()),
                           code->Size(), 0);
}

void ObjectStatsCollectorImpl::RecordVirtualContext(Context* context) {
  if (context->IsNativeContext()) {
    RecordSimpleVirtualObjectStats(nullptr, context,
                                   ObjectStats::NATIVE_CONTEXT_TYPE);
  } else if (context->IsFunctionContext()) {
    RecordSimpleVirtualObjectStats(nullptr, context,
                                   ObjectStats::FUNCTION_CONTEXT_TYPE);
  } else {
    RecordSimpleVirtualObjectStats(nullptr, context,
                                   ObjectStats::OTHER_CONTEXT_TYPE);
  }
}

class ObjectStatsVisitor {
 public:
  ObjectStatsVisitor(Heap* heap, ObjectStatsCollectorImpl* live_collector,
                     ObjectStatsCollectorImpl* dead_collector,
                     ObjectStatsCollectorImpl::Phase phase)
      : live_collector_(live_collector),
        dead_collector_(dead_collector),
        marking_state_(
            heap->mark_compact_collector()->non_atomic_marking_state()),
        phase_(phase) {}

  bool Visit(HeapObject* obj, int size) {
    if (marking_state_->IsBlack(obj)) {
      live_collector_->CollectStatistics(obj, phase_);
    } else {
      DCHECK(!marking_state_->IsGrey(obj));
      dead_collector_->CollectStatistics(obj, phase_);
    }
    return true;
  }

 private:
  ObjectStatsCollectorImpl* live_collector_;
  ObjectStatsCollectorImpl* dead_collector_;
  MarkCompactCollector::NonAtomicMarkingState* marking_state_;
  ObjectStatsCollectorImpl::Phase phase_;
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
  live_collector.CollectGlobalStatistics();
  for (int i = 0; i < ObjectStatsCollectorImpl::kNumberOfPhases; i++) {
    ObjectStatsVisitor visitor(heap_, &live_collector, &dead_collector,
                               static_cast<ObjectStatsCollectorImpl::Phase>(i));
    IterateHeap(heap_, &visitor);
  }
}

}  // namespace internal
}  // namespace v8
