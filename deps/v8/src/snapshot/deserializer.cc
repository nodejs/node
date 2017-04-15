// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/deserializer.h"

#include "src/bootstrapper.h"
#include "src/external-reference-table.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/macro-assembler.h"
#include "src/snapshot/natives.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

void Deserializer::DecodeReservation(
    Vector<const SerializedData::Reservation> res) {
  DCHECK_EQ(0, reservations_[NEW_SPACE].length());
  STATIC_ASSERT(NEW_SPACE == 0);
  int current_space = NEW_SPACE;
  for (auto& r : res) {
    reservations_[current_space].Add({r.chunk_size(), NULL, NULL});
    if (r.is_last()) current_space++;
  }
  DCHECK_EQ(kNumberOfSpaces, current_space);
  for (int i = 0; i < kNumberOfPreallocatedSpaces; i++) current_chunk_[i] = 0;
}

void Deserializer::FlushICacheForNewIsolate() {
  DCHECK(!deserializing_user_code_);
  // The entire isolate is newly deserialized. Simply flush all code pages.
  for (Page* p : *isolate_->heap()->code_space()) {
    Assembler::FlushICache(isolate_, p->area_start(),
                           p->area_end() - p->area_start());
  }
}

void Deserializer::FlushICacheForNewCodeObjectsAndRecordEmbeddedObjects() {
  DCHECK(deserializing_user_code_);
  for (Code* code : new_code_objects_) {
    // Record all references to embedded objects in the new code object.
    isolate_->heap()->RecordWritesIntoCode(code);

    if (FLAG_serialize_age_code) code->PreAge(isolate_);
    Assembler::FlushICache(isolate_, code->instruction_start(),
                           code->instruction_size());
  }
}

bool Deserializer::ReserveSpace() {
#ifdef DEBUG
  for (int i = NEW_SPACE; i < kNumberOfSpaces; ++i) {
    CHECK(reservations_[i].length() > 0);
  }
#endif  // DEBUG
  DCHECK(allocated_maps_.is_empty());
  if (!isolate_->heap()->ReserveSpace(reservations_, &allocated_maps_))
    return false;
  for (int i = 0; i < kNumberOfPreallocatedSpaces; i++) {
    high_water_[i] = reservations_[i][0].start;
  }
  return true;
}

void Deserializer::Initialize(Isolate* isolate) {
  DCHECK_NULL(isolate_);
  DCHECK_NOT_NULL(isolate);
  isolate_ = isolate;
  DCHECK_NULL(external_reference_table_);
  external_reference_table_ = ExternalReferenceTable::instance(isolate);
  CHECK_EQ(magic_number_,
           SerializedData::ComputeMagicNumber(external_reference_table_));
}

void Deserializer::Deserialize(Isolate* isolate) {
  Initialize(isolate);
  if (!ReserveSpace()) V8::FatalProcessOutOfMemory("deserializing context");
  // No active threads.
  DCHECK_NULL(isolate_->thread_manager()->FirstThreadStateInUse());
  // No active handles.
  DCHECK(isolate_->handle_scope_implementer()->blocks()->is_empty());
  // Partial snapshot cache is not yet populated.
  DCHECK(isolate_->partial_snapshot_cache()->is_empty());

  {
    DisallowHeapAllocation no_gc;
    isolate_->heap()->IterateStrongRoots(this, VISIT_ONLY_STRONG_ROOT_LIST);
    isolate_->heap()->IterateSmiRoots(this);
    isolate_->heap()->IterateStrongRoots(this, VISIT_ONLY_STRONG);
    isolate_->heap()->RepairFreeListsAfterDeserialization();
    isolate_->heap()->IterateWeakRoots(this, VISIT_ALL);
    DeserializeDeferredObjects();
    FlushICacheForNewIsolate();
    RestoreExternalReferenceRedirectors(&accessor_infos_);
  }

  isolate_->heap()->set_native_contexts_list(
      isolate_->heap()->undefined_value());
  // The allocation site list is build during root iteration, but if no sites
  // were encountered then it needs to be initialized to undefined.
  if (isolate_->heap()->allocation_sites_list() == Smi::kZero) {
    isolate_->heap()->set_allocation_sites_list(
        isolate_->heap()->undefined_value());
  }

  // Issue code events for newly deserialized code objects.
  LOG_CODE_EVENT(isolate_, LogCodeObjects());
  LOG_CODE_EVENT(isolate_, LogBytecodeHandlers());
  LOG_CODE_EVENT(isolate_, LogCompiledFunctions());
}

MaybeHandle<Object> Deserializer::DeserializePartial(
    Isolate* isolate, Handle<JSGlobalProxy> global_proxy,
    v8::DeserializeInternalFieldsCallback internal_fields_deserializer) {
  Initialize(isolate);
  if (!ReserveSpace()) {
    V8::FatalProcessOutOfMemory("deserialize context");
    return MaybeHandle<Object>();
  }

  AddAttachedObject(global_proxy);

  DisallowHeapAllocation no_gc;
  // Keep track of the code space start and end pointers in case new
  // code objects were unserialized
  OldSpace* code_space = isolate_->heap()->code_space();
  Address start_address = code_space->top();
  Object* root;
  VisitPointer(&root);
  DeserializeDeferredObjects();
  DeserializeInternalFields(internal_fields_deserializer);

  isolate->heap()->RegisterReservationsForBlackAllocation(reservations_);

  // There's no code deserialized here. If this assert fires then that's
  // changed and logging should be added to notify the profiler et al of the
  // new code, which also has to be flushed from instruction cache.
  CHECK_EQ(start_address, code_space->top());
  return Handle<Object>(root, isolate);
}

MaybeHandle<HeapObject> Deserializer::DeserializeObject(Isolate* isolate) {
  Initialize(isolate);
  if (!ReserveSpace()) {
    return MaybeHandle<HeapObject>();
  } else {
    deserializing_user_code_ = true;
    HandleScope scope(isolate);
    Handle<HeapObject> result;
    {
      DisallowHeapAllocation no_gc;
      Object* root;
      VisitPointer(&root);
      DeserializeDeferredObjects();
      FlushICacheForNewCodeObjectsAndRecordEmbeddedObjects();
      result = Handle<HeapObject>(HeapObject::cast(root));
      isolate->heap()->RegisterReservationsForBlackAllocation(reservations_);
    }
    CommitPostProcessedObjects(isolate);
    return scope.CloseAndEscape(result);
  }
}

Deserializer::~Deserializer() {
  // TODO(svenpanne) Re-enable this assertion when v8 initialization is fixed.
  // DCHECK(source_.AtEOF());
#ifdef DEBUG
  for (int space = 0; space < kNumberOfPreallocatedSpaces; space++) {
    int chunk_index = current_chunk_[space];
    CHECK_EQ(reservations_[space].length(), chunk_index + 1);
    CHECK_EQ(reservations_[space][chunk_index].end, high_water_[space]);
  }
  CHECK_EQ(allocated_maps_.length(), next_map_index_);
#endif  // DEBUG
}

// This is called on the roots.  It is the driver of the deserialization
// process.  It is also called on the body of each function.
void Deserializer::VisitPointers(Object** start, Object** end) {
  // The space must be new space.  Any other space would cause ReadChunk to try
  // to update the remembered using NULL as the address.
  ReadData(start, end, NEW_SPACE, NULL);
}

void Deserializer::Synchronize(VisitorSynchronization::SyncTag tag) {
  static const byte expected = kSynchronize;
  CHECK_EQ(expected, source_.Get());
}

void Deserializer::DeserializeDeferredObjects() {
  for (int code = source_.Get(); code != kSynchronize; code = source_.Get()) {
    switch (code) {
      case kAlignmentPrefix:
      case kAlignmentPrefix + 1:
      case kAlignmentPrefix + 2:
        SetAlignment(code);
        break;
      default: {
        int space = code & kSpaceMask;
        DCHECK(space <= kNumberOfSpaces);
        DCHECK(code - space == kNewObject);
        HeapObject* object = GetBackReferencedObject(space);
        int size = source_.GetInt() << kPointerSizeLog2;
        Address obj_address = object->address();
        Object** start = reinterpret_cast<Object**>(obj_address + kPointerSize);
        Object** end = reinterpret_cast<Object**>(obj_address + size);
        bool filled = ReadData(start, end, space, obj_address);
        CHECK(filled);
        DCHECK(CanBeDeferred(object));
        PostProcessNewObject(object, space);
      }
    }
  }
}

void Deserializer::DeserializeInternalFields(
    v8::DeserializeInternalFieldsCallback internal_fields_deserializer) {
  if (!source_.HasMore() || source_.Get() != kInternalFieldsData) return;
  DisallowHeapAllocation no_gc;
  DisallowJavascriptExecution no_js(isolate_);
  DisallowCompilation no_compile(isolate_);
  DCHECK_NOT_NULL(internal_fields_deserializer.callback);
  for (int code = source_.Get(); code != kSynchronize; code = source_.Get()) {
    HandleScope scope(isolate_);
    int space = code & kSpaceMask;
    DCHECK(space <= kNumberOfSpaces);
    DCHECK(code - space == kNewObject);
    Handle<JSObject> obj(JSObject::cast(GetBackReferencedObject(space)),
                         isolate_);
    int index = source_.GetInt();
    int size = source_.GetInt();
    byte* data = new byte[size];
    source_.CopyRaw(data, size);
    internal_fields_deserializer.callback(v8::Utils::ToLocal(obj), index,
                                          {reinterpret_cast<char*>(data), size},
                                          internal_fields_deserializer.data);
    delete[] data;
  }
}

// Used to insert a deserialized internalized string into the string table.
class StringTableInsertionKey : public HashTableKey {
 public:
  explicit StringTableInsertionKey(String* string)
      : string_(string), hash_(HashForObject(string)) {
    DCHECK(string->IsInternalizedString());
  }

  bool IsMatch(Object* string) override {
    // We know that all entries in a hash table had their hash keys created.
    // Use that knowledge to have fast failure.
    if (hash_ != HashForObject(string)) return false;
    // We want to compare the content of two internalized strings here.
    return string_->SlowEquals(String::cast(string));
  }

  uint32_t Hash() override { return hash_; }

  uint32_t HashForObject(Object* key) override {
    return String::cast(key)->Hash();
  }

  MUST_USE_RESULT Handle<Object> AsHandle(Isolate* isolate) override {
    return handle(string_, isolate);
  }

 private:
  String* string_;
  uint32_t hash_;
  DisallowHeapAllocation no_gc;
};

HeapObject* Deserializer::PostProcessNewObject(HeapObject* obj, int space) {
  if (deserializing_user_code()) {
    if (obj->IsString()) {
      String* string = String::cast(obj);
      // Uninitialize hash field as the hash seed may have changed.
      string->set_hash_field(String::kEmptyHashField);
      if (string->IsInternalizedString()) {
        // Canonicalize the internalized string. If it already exists in the
        // string table, set it to forward to the existing one.
        StringTableInsertionKey key(string);
        String* canonical = StringTable::LookupKeyIfExists(isolate_, &key);
        if (canonical == NULL) {
          new_internalized_strings_.Add(handle(string));
          return string;
        } else {
          string->SetForwardedInternalizedString(canonical);
          return canonical;
        }
      }
    } else if (obj->IsScript()) {
      new_scripts_.Add(handle(Script::cast(obj)));
    } else {
      DCHECK(CanBeDeferred(obj));
    }
  }
  if (obj->IsAllocationSite()) {
    DCHECK(obj->IsAllocationSite());
    // Allocation sites are present in the snapshot, and must be linked into
    // a list at deserialization time.
    AllocationSite* site = AllocationSite::cast(obj);
    // TODO(mvstanton): consider treating the heap()->allocation_sites_list()
    // as a (weak) root. If this root is relocated correctly, this becomes
    // unnecessary.
    if (isolate_->heap()->allocation_sites_list() == Smi::kZero) {
      site->set_weak_next(isolate_->heap()->undefined_value());
    } else {
      site->set_weak_next(isolate_->heap()->allocation_sites_list());
    }
    isolate_->heap()->set_allocation_sites_list(site);
  } else if (obj->IsCode()) {
    // We flush all code pages after deserializing the startup snapshot. In that
    // case, we only need to remember code objects in the large object space.
    // When deserializing user code, remember each individual code object.
    if (deserializing_user_code() || space == LO_SPACE) {
      new_code_objects_.Add(Code::cast(obj));
    }
  } else if (obj->IsAccessorInfo()) {
    if (isolate_->external_reference_redirector()) {
      accessor_infos_.Add(AccessorInfo::cast(obj));
    }
  }
  // Check alignment.
  DCHECK_EQ(0, Heap::GetFillToAlign(obj->address(), obj->RequiredAlignment()));
  return obj;
}

void Deserializer::CommitPostProcessedObjects(Isolate* isolate) {
  StringTable::EnsureCapacityForDeserialization(
      isolate, new_internalized_strings_.length());
  for (Handle<String> string : new_internalized_strings_) {
    StringTableInsertionKey key(*string);
    DCHECK_NULL(StringTable::LookupKeyIfExists(isolate, &key));
    StringTable::LookupKey(isolate, &key);
  }

  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  for (Handle<Script> script : new_scripts_) {
    // Assign a new script id to avoid collision.
    script->set_id(isolate_->heap()->NextScriptId());
    // Add script to list.
    Handle<Object> list = WeakFixedArray::Add(factory->script_list(), script);
    heap->SetRootScriptList(*list);
  }
}

HeapObject* Deserializer::GetBackReferencedObject(int space) {
  HeapObject* obj;
  SerializerReference back_reference =
      SerializerReference::FromBitfield(source_.GetInt());
  if (space == LO_SPACE) {
    uint32_t index = back_reference.large_object_index();
    obj = deserialized_large_objects_[index];
  } else if (space == MAP_SPACE) {
    int index = back_reference.map_index();
    DCHECK(index < next_map_index_);
    obj = HeapObject::FromAddress(allocated_maps_[index]);
  } else {
    DCHECK(space < kNumberOfPreallocatedSpaces);
    uint32_t chunk_index = back_reference.chunk_index();
    DCHECK_LE(chunk_index, current_chunk_[space]);
    uint32_t chunk_offset = back_reference.chunk_offset();
    Address address = reservations_[space][chunk_index].start + chunk_offset;
    if (next_alignment_ != kWordAligned) {
      int padding = Heap::GetFillToAlign(address, next_alignment_);
      next_alignment_ = kWordAligned;
      DCHECK(padding == 0 || HeapObject::FromAddress(address)->IsFiller());
      address += padding;
    }
    obj = HeapObject::FromAddress(address);
  }
  if (deserializing_user_code() && obj->IsInternalizedString()) {
    obj = String::cast(obj)->GetForwardedInternalizedString();
  }
  hot_objects_.Add(obj);
  return obj;
}

// This routine writes the new object into the pointer provided and then
// returns true if the new object was in young space and false otherwise.
// The reason for this strange interface is that otherwise the object is
// written very late, which means the FreeSpace map is not set up by the
// time we need to use it to mark the space at the end of a page free.
void Deserializer::ReadObject(int space_number, Object** write_back) {
  Address address;
  HeapObject* obj;
  int size = source_.GetInt() << kObjectAlignmentBits;

  if (next_alignment_ != kWordAligned) {
    int reserved = size + Heap::GetMaximumFillToAlign(next_alignment_);
    address = Allocate(space_number, reserved);
    obj = HeapObject::FromAddress(address);
    // If one of the following assertions fails, then we are deserializing an
    // aligned object when the filler maps have not been deserialized yet.
    // We require filler maps as padding to align the object.
    Heap* heap = isolate_->heap();
    DCHECK(heap->free_space_map()->IsMap());
    DCHECK(heap->one_pointer_filler_map()->IsMap());
    DCHECK(heap->two_pointer_filler_map()->IsMap());
    obj = heap->AlignWithFiller(obj, size, reserved, next_alignment_);
    address = obj->address();
    next_alignment_ = kWordAligned;
  } else {
    address = Allocate(space_number, size);
    obj = HeapObject::FromAddress(address);
  }

  isolate_->heap()->OnAllocationEvent(obj, size);
  Object** current = reinterpret_cast<Object**>(address);
  Object** limit = current + (size >> kPointerSizeLog2);

  if (ReadData(current, limit, space_number, address)) {
    // Only post process if object content has not been deferred.
    obj = PostProcessNewObject(obj, space_number);
  }

  Object* write_back_obj = obj;
  UnalignedCopy(write_back, &write_back_obj);
#ifdef DEBUG
  if (obj->IsCode()) {
    DCHECK(space_number == CODE_SPACE || space_number == LO_SPACE);
  } else {
    DCHECK(space_number != CODE_SPACE);
  }
#endif  // DEBUG
}

// We know the space requirements before deserialization and can
// pre-allocate that reserved space. During deserialization, all we need
// to do is to bump up the pointer for each space in the reserved
// space. This is also used for fixing back references.
// We may have to split up the pre-allocation into several chunks
// because it would not fit onto a single page. We do not have to keep
// track of when to move to the next chunk. An opcode will signal this.
// Since multiple large objects cannot be folded into one large object
// space allocation, we have to do an actual allocation when deserializing
// each large object. Instead of tracking offset for back references, we
// reference large objects by index.
Address Deserializer::Allocate(int space_index, int size) {
  if (space_index == LO_SPACE) {
    AlwaysAllocateScope scope(isolate_);
    LargeObjectSpace* lo_space = isolate_->heap()->lo_space();
    Executability exec = static_cast<Executability>(source_.Get());
    AllocationResult result = lo_space->AllocateRaw(size, exec);
    HeapObject* obj = result.ToObjectChecked();
    deserialized_large_objects_.Add(obj);
    return obj->address();
  } else if (space_index == MAP_SPACE) {
    DCHECK_EQ(Map::kSize, size);
    return allocated_maps_[next_map_index_++];
  } else {
    DCHECK(space_index < kNumberOfPreallocatedSpaces);
    Address address = high_water_[space_index];
    DCHECK_NOT_NULL(address);
    high_water_[space_index] += size;
#ifdef DEBUG
    // Assert that the current reserved chunk is still big enough.
    const Heap::Reservation& reservation = reservations_[space_index];
    int chunk_index = current_chunk_[space_index];
    CHECK_LE(high_water_[space_index], reservation[chunk_index].end);
#endif
    if (space_index == CODE_SPACE) SkipList::Update(address, size);
    return address;
  }
}

Object** Deserializer::CopyInNativesSource(Vector<const char> source_vector,
                                           Object** current) {
  DCHECK(!isolate_->heap()->deserialization_complete());
  NativesExternalStringResource* resource = new NativesExternalStringResource(
      source_vector.start(), source_vector.length());
  Object* resource_obj = reinterpret_cast<Object*>(resource);
  UnalignedCopy(current++, &resource_obj);
  return current;
}

bool Deserializer::ReadData(Object** current, Object** limit, int source_space,
                            Address current_object_address) {
  Isolate* const isolate = isolate_;
  // Write barrier support costs around 1% in startup time.  In fact there
  // are no new space objects in current boot snapshots, so it's not needed,
  // but that may change.
  bool write_barrier_needed =
      (current_object_address != NULL && source_space != NEW_SPACE &&
       source_space != CODE_SPACE);
  while (current < limit) {
    byte data = source_.Get();
    switch (data) {
#define CASE_STATEMENT(where, how, within, space_number) \
  case where + how + within + space_number:              \
    STATIC_ASSERT((where & ~kWhereMask) == 0);           \
    STATIC_ASSERT((how & ~kHowToCodeMask) == 0);         \
    STATIC_ASSERT((within & ~kWhereToPointMask) == 0);   \
    STATIC_ASSERT((space_number & ~kSpaceMask) == 0);

#define CASE_BODY(where, how, within, space_number_if_any)                     \
  {                                                                            \
    bool emit_write_barrier = false;                                           \
    bool current_was_incremented = false;                                      \
    int space_number = space_number_if_any == kAnyOldSpace                     \
                           ? (data & kSpaceMask)                               \
                           : space_number_if_any;                              \
    if (where == kNewObject && how == kPlain && within == kStartOfObject) {    \
      ReadObject(space_number, current);                                       \
      emit_write_barrier = (space_number == NEW_SPACE);                        \
    } else {                                                                   \
      Object* new_object = NULL; /* May not be a real Object pointer. */       \
      if (where == kNewObject) {                                               \
        ReadObject(space_number, &new_object);                                 \
      } else if (where == kBackref) {                                          \
        emit_write_barrier = (space_number == NEW_SPACE);                      \
        new_object = GetBackReferencedObject(data & kSpaceMask);               \
      } else if (where == kBackrefWithSkip) {                                  \
        int skip = source_.GetInt();                                           \
        current = reinterpret_cast<Object**>(                                  \
            reinterpret_cast<Address>(current) + skip);                        \
        emit_write_barrier = (space_number == NEW_SPACE);                      \
        new_object = GetBackReferencedObject(data & kSpaceMask);               \
      } else if (where == kRootArray) {                                        \
        int id = source_.GetInt();                                             \
        Heap::RootListIndex root_index = static_cast<Heap::RootListIndex>(id); \
        new_object = isolate->heap()->root(root_index);                        \
        emit_write_barrier = isolate->heap()->InNewSpace(new_object);          \
        hot_objects_.Add(HeapObject::cast(new_object));                        \
      } else if (where == kPartialSnapshotCache) {                             \
        int cache_index = source_.GetInt();                                    \
        new_object = isolate->partial_snapshot_cache()->at(cache_index);       \
        emit_write_barrier = isolate->heap()->InNewSpace(new_object);          \
      } else if (where == kExternalReference) {                                \
        int skip = source_.GetInt();                                           \
        current = reinterpret_cast<Object**>(                                  \
            reinterpret_cast<Address>(current) + skip);                        \
        uint32_t reference_id = static_cast<uint32_t>(source_.GetInt());       \
        Address address = external_reference_table_->address(reference_id);    \
        new_object = reinterpret_cast<Object*>(address);                       \
      } else if (where == kAttachedReference) {                                \
        int index = source_.GetInt();                                          \
        new_object = *attached_objects_[index];                                \
        emit_write_barrier = isolate->heap()->InNewSpace(new_object);          \
      } else {                                                                 \
        DCHECK(where == kBuiltin);                                             \
        DCHECK(deserializing_user_code());                                     \
        int builtin_id = source_.GetInt();                                     \
        DCHECK_LE(0, builtin_id);                                              \
        DCHECK_LT(builtin_id, Builtins::builtin_count);                        \
        Builtins::Name name = static_cast<Builtins::Name>(builtin_id);         \
        new_object = isolate->builtins()->builtin(name);                       \
        emit_write_barrier = false;                                            \
      }                                                                        \
      if (within == kInnerPointer) {                                           \
        if (new_object->IsCode()) {                                            \
          Code* new_code_object = Code::cast(new_object);                      \
          new_object =                                                         \
              reinterpret_cast<Object*>(new_code_object->instruction_start()); \
        } else {                                                               \
          Cell* cell = Cell::cast(new_object);                                 \
          new_object = reinterpret_cast<Object*>(cell->ValueAddress());        \
        }                                                                      \
      }                                                                        \
      if (how == kFromCode) {                                                  \
        Address location_of_branch_data = reinterpret_cast<Address>(current);  \
        Assembler::deserialization_set_special_target_at(                      \
            isolate, location_of_branch_data,                                  \
            Code::cast(HeapObject::FromAddress(current_object_address)),       \
            reinterpret_cast<Address>(new_object));                            \
        location_of_branch_data += Assembler::kSpecialTargetSize;              \
        current = reinterpret_cast<Object**>(location_of_branch_data);         \
        current_was_incremented = true;                                        \
      } else {                                                                 \
        UnalignedCopy(current, &new_object);                                   \
      }                                                                        \
    }                                                                          \
    if (emit_write_barrier && write_barrier_needed) {                          \
      Address current_address = reinterpret_cast<Address>(current);            \
      SLOW_DCHECK(isolate->heap()->ContainsSlow(current_object_address));      \
      isolate->heap()->RecordWrite(                                            \
          HeapObject::FromAddress(current_object_address),                     \
          static_cast<int>(current_address - current_object_address),          \
          *reinterpret_cast<Object**>(current_address));                       \
    }                                                                          \
    if (!current_was_incremented) {                                            \
      current++;                                                               \
    }                                                                          \
    break;                                                                     \
  }

// This generates a case and a body for the new space (which has to do extra
// write barrier handling) and handles the other spaces with fall-through cases
// and one body.
#define ALL_SPACES(where, how, within)           \
  CASE_STATEMENT(where, how, within, NEW_SPACE)  \
  CASE_BODY(where, how, within, NEW_SPACE)       \
  CASE_STATEMENT(where, how, within, OLD_SPACE)  \
  CASE_STATEMENT(where, how, within, CODE_SPACE) \
  CASE_STATEMENT(where, how, within, MAP_SPACE)  \
  CASE_STATEMENT(where, how, within, LO_SPACE)   \
  CASE_BODY(where, how, within, kAnyOldSpace)

#define FOUR_CASES(byte_code) \
  case byte_code:             \
  case byte_code + 1:         \
  case byte_code + 2:         \
  case byte_code + 3:

#define SIXTEEN_CASES(byte_code) \
  FOUR_CASES(byte_code)          \
  FOUR_CASES(byte_code + 4)      \
  FOUR_CASES(byte_code + 8)      \
  FOUR_CASES(byte_code + 12)

#define SINGLE_CASE(where, how, within, space) \
  CASE_STATEMENT(where, how, within, space)    \
  CASE_BODY(where, how, within, space)

      // Deserialize a new object and write a pointer to it to the current
      // object.
      ALL_SPACES(kNewObject, kPlain, kStartOfObject)
      // Support for direct instruction pointers in functions.  It's an inner
      // pointer because it points at the entry point, not at the start of the
      // code object.
      SINGLE_CASE(kNewObject, kPlain, kInnerPointer, CODE_SPACE)
      // Support for pointers into a cell. It's an inner pointer because it
      // points directly at the value field, not the start of the cell object.
      SINGLE_CASE(kNewObject, kPlain, kInnerPointer, OLD_SPACE)
      // Deserialize a new code object and write a pointer to its first
      // instruction to the current code object.
      ALL_SPACES(kNewObject, kFromCode, kInnerPointer)
      // Find a recently deserialized object using its offset from the current
      // allocation point and write a pointer to it to the current object.
      ALL_SPACES(kBackref, kPlain, kStartOfObject)
      ALL_SPACES(kBackrefWithSkip, kPlain, kStartOfObject)
#if V8_CODE_EMBEDS_OBJECT_POINTER
      // Deserialize a new object from pointer found in code and write
      // a pointer to it to the current object. Required only for MIPS, PPC, ARM
      // or S390 with embedded constant pool, and omitted on the other
      // architectures because it is fully unrolled and would cause bloat.
      ALL_SPACES(kNewObject, kFromCode, kStartOfObject)
      // Find a recently deserialized code object using its offset from the
      // current allocation point and write a pointer to it to the current
      // object. Required only for MIPS, PPC, ARM or S390 with embedded
      // constant pool.
      ALL_SPACES(kBackref, kFromCode, kStartOfObject)
      ALL_SPACES(kBackrefWithSkip, kFromCode, kStartOfObject)
#endif
      // Find a recently deserialized code object using its offset from the
      // current allocation point and write a pointer to its first instruction
      // to the current code object or the instruction pointer in a function
      // object.
      ALL_SPACES(kBackref, kFromCode, kInnerPointer)
      ALL_SPACES(kBackrefWithSkip, kFromCode, kInnerPointer)
      // Support for direct instruction pointers in functions.
      SINGLE_CASE(kBackref, kPlain, kInnerPointer, CODE_SPACE)
      SINGLE_CASE(kBackrefWithSkip, kPlain, kInnerPointer, CODE_SPACE)
      // Support for pointers into a cell.
      SINGLE_CASE(kBackref, kPlain, kInnerPointer, OLD_SPACE)
      SINGLE_CASE(kBackrefWithSkip, kPlain, kInnerPointer, OLD_SPACE)
      // Find an object in the roots array and write a pointer to it to the
      // current object.
      SINGLE_CASE(kRootArray, kPlain, kStartOfObject, 0)
#if V8_CODE_EMBEDS_OBJECT_POINTER
      // Find an object in the roots array and write a pointer to it to in code.
      SINGLE_CASE(kRootArray, kFromCode, kStartOfObject, 0)
#endif
      // Find an object in the partial snapshots cache and write a pointer to it
      // to the current object.
      SINGLE_CASE(kPartialSnapshotCache, kPlain, kStartOfObject, 0)
      // Find an code entry in the partial snapshots cache and
      // write a pointer to it to the current object.
      SINGLE_CASE(kPartialSnapshotCache, kPlain, kInnerPointer, 0)
      // Find an external reference and write a pointer to it to the current
      // object.
      SINGLE_CASE(kExternalReference, kPlain, kStartOfObject, 0)
      // Find an external reference and write a pointer to it in the current
      // code object.
      SINGLE_CASE(kExternalReference, kFromCode, kStartOfObject, 0)
      // Find an object in the attached references and write a pointer to it to
      // the current object.
      SINGLE_CASE(kAttachedReference, kPlain, kStartOfObject, 0)
      SINGLE_CASE(kAttachedReference, kPlain, kInnerPointer, 0)
      SINGLE_CASE(kAttachedReference, kFromCode, kStartOfObject, 0)
      SINGLE_CASE(kAttachedReference, kFromCode, kInnerPointer, 0)
      // Find a builtin and write a pointer to it to the current object.
      SINGLE_CASE(kBuiltin, kPlain, kStartOfObject, 0)
      SINGLE_CASE(kBuiltin, kPlain, kInnerPointer, 0)
      SINGLE_CASE(kBuiltin, kFromCode, kInnerPointer, 0)

#undef CASE_STATEMENT
#undef CASE_BODY
#undef ALL_SPACES

      case kSkip: {
        int size = source_.GetInt();
        current = reinterpret_cast<Object**>(
            reinterpret_cast<intptr_t>(current) + size);
        break;
      }

      case kInternalReferenceEncoded:
      case kInternalReference: {
        // Internal reference address is not encoded via skip, but by offset
        // from code entry.
        int pc_offset = source_.GetInt();
        int target_offset = source_.GetInt();
        Code* code =
            Code::cast(HeapObject::FromAddress(current_object_address));
        DCHECK(0 <= pc_offset && pc_offset <= code->instruction_size());
        DCHECK(0 <= target_offset && target_offset <= code->instruction_size());
        Address pc = code->entry() + pc_offset;
        Address target = code->entry() + target_offset;
        Assembler::deserialization_set_target_internal_reference_at(
            isolate, pc, target, data == kInternalReference
                                     ? RelocInfo::INTERNAL_REFERENCE
                                     : RelocInfo::INTERNAL_REFERENCE_ENCODED);
        break;
      }

      case kNop:
        break;

      case kNextChunk: {
        int space = source_.Get();
        DCHECK(space < kNumberOfPreallocatedSpaces);
        int chunk_index = current_chunk_[space];
        const Heap::Reservation& reservation = reservations_[space];
        // Make sure the current chunk is indeed exhausted.
        CHECK_EQ(reservation[chunk_index].end, high_water_[space]);
        // Move to next reserved chunk.
        chunk_index = ++current_chunk_[space];
        CHECK_LT(chunk_index, reservation.length());
        high_water_[space] = reservation[chunk_index].start;
        break;
      }

      case kDeferred: {
        // Deferred can only occur right after the heap object header.
        DCHECK(current == reinterpret_cast<Object**>(current_object_address +
                                                     kPointerSize));
        HeapObject* obj = HeapObject::FromAddress(current_object_address);
        // If the deferred object is a map, its instance type may be used
        // during deserialization. Initialize it with a temporary value.
        if (obj->IsMap()) Map::cast(obj)->set_instance_type(FILLER_TYPE);
        current = limit;
        return false;
      }

      case kSynchronize:
        // If we get here then that indicates that you have a mismatch between
        // the number of GC roots when serializing and deserializing.
        CHECK(false);
        break;

      case kNativesStringResource:
        current = CopyInNativesSource(Natives::GetScriptSource(source_.Get()),
                                      current);
        break;

      case kExtraNativesStringResource:
        current = CopyInNativesSource(
            ExtraNatives::GetScriptSource(source_.Get()), current);
        break;

      // Deserialize raw data of variable length.
      case kVariableRawData: {
        int size_in_bytes = source_.GetInt();
        byte* raw_data_out = reinterpret_cast<byte*>(current);
        source_.CopyRaw(raw_data_out, size_in_bytes);
        break;
      }

      case kVariableRepeat: {
        int repeats = source_.GetInt();
        Object* object = current[-1];
        DCHECK(!isolate->heap()->InNewSpace(object));
        for (int i = 0; i < repeats; i++) UnalignedCopy(current++, &object);
        break;
      }

      case kAlignmentPrefix:
      case kAlignmentPrefix + 1:
      case kAlignmentPrefix + 2:
        SetAlignment(data);
        break;

      STATIC_ASSERT(kNumberOfRootArrayConstants == Heap::kOldSpaceRoots);
      STATIC_ASSERT(kNumberOfRootArrayConstants == 32);
      SIXTEEN_CASES(kRootArrayConstantsWithSkip)
      SIXTEEN_CASES(kRootArrayConstantsWithSkip + 16) {
        int skip = source_.GetInt();
        current = reinterpret_cast<Object**>(
            reinterpret_cast<intptr_t>(current) + skip);
        // Fall through.
      }

      SIXTEEN_CASES(kRootArrayConstants)
      SIXTEEN_CASES(kRootArrayConstants + 16) {
        int id = data & kRootArrayConstantsMask;
        Heap::RootListIndex root_index = static_cast<Heap::RootListIndex>(id);
        Object* object = isolate->heap()->root(root_index);
        DCHECK(!isolate->heap()->InNewSpace(object));
        UnalignedCopy(current++, &object);
        break;
      }

      STATIC_ASSERT(kNumberOfHotObjects == 8);
      FOUR_CASES(kHotObjectWithSkip)
      FOUR_CASES(kHotObjectWithSkip + 4) {
        int skip = source_.GetInt();
        current = reinterpret_cast<Object**>(
            reinterpret_cast<Address>(current) + skip);
        // Fall through.
      }

      FOUR_CASES(kHotObject)
      FOUR_CASES(kHotObject + 4) {
        int index = data & kHotObjectMask;
        Object* hot_object = hot_objects_.Get(index);
        UnalignedCopy(current, &hot_object);
        if (write_barrier_needed && isolate->heap()->InNewSpace(hot_object)) {
          Address current_address = reinterpret_cast<Address>(current);
          isolate->heap()->RecordWrite(
              HeapObject::FromAddress(current_object_address),
              static_cast<int>(current_address - current_object_address),
              hot_object);
        }
        current++;
        break;
      }

      // Deserialize raw data of fixed length from 1 to 32 words.
      STATIC_ASSERT(kNumberOfFixedRawData == 32);
      SIXTEEN_CASES(kFixedRawData)
      SIXTEEN_CASES(kFixedRawData + 16) {
        byte* raw_data_out = reinterpret_cast<byte*>(current);
        int size_in_bytes = (data - kFixedRawDataStart) << kPointerSizeLog2;
        source_.CopyRaw(raw_data_out, size_in_bytes);
        current = reinterpret_cast<Object**>(raw_data_out + size_in_bytes);
        break;
      }

      STATIC_ASSERT(kNumberOfFixedRepeat == 16);
      SIXTEEN_CASES(kFixedRepeat) {
        int repeats = data - kFixedRepeatStart;
        Object* object;
        UnalignedCopy(&object, current - 1);
        DCHECK(!isolate->heap()->InNewSpace(object));
        for (int i = 0; i < repeats; i++) UnalignedCopy(current++, &object);
        break;
      }

#undef SIXTEEN_CASES
#undef FOUR_CASES
#undef SINGLE_CASE

      default:
        CHECK(false);
    }
  }
  CHECK_EQ(limit, current);
  return true;
}
}  // namespace internal
}  // namespace v8
