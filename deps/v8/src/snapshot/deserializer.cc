// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/deserializer.h"

#include "src/codegen/assembler-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/interpreter/interpreter.h"
#include "src/logging/log.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/cell-inl.h"
#include "src/objects/hash-table.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/objects/slots.h"
#include "src/objects/smi.h"
#include "src/objects/string.h"
#include "src/roots/roots.h"
#include "src/snapshot/snapshot.h"
#include "src/tracing/trace-event.h"
#include "src/tracing/traced-value.h"

namespace v8 {
namespace internal {

template <typename TSlot>
TSlot Deserializer::Write(TSlot dest, MaybeObject value) {
  DCHECK(!allocator()->next_reference_is_weak());
  dest.store(value);
  return dest + 1;
}

template <typename TSlot>
TSlot Deserializer::WriteAddress(TSlot dest, Address value) {
  DCHECK(!allocator()->next_reference_is_weak());
  memcpy(dest.ToVoidPtr(), &value, kSystemPointerSize);
  STATIC_ASSERT(IsAligned(kSystemPointerSize, TSlot::kSlotDataSize));
  return dest + (kSystemPointerSize / TSlot::kSlotDataSize);
}

void Deserializer::Initialize(Isolate* isolate) {
  DCHECK_NULL(isolate_);
  DCHECK_NOT_NULL(isolate);
  isolate_ = isolate;
  allocator()->Initialize(isolate->heap());

#ifdef DEBUG
  // The read-only deserializer is run by read-only heap set-up before the heap
  // is fully set up. External reference table relies on a few parts of this
  // set-up (like old-space), so it may be uninitialized at this point.
  if (isolate->isolate_data()->external_reference_table()->is_initialized()) {
    // Count the number of external references registered through the API.
    num_api_references_ = 0;
    if (isolate_->api_external_references() != nullptr) {
      while (isolate_->api_external_references()[num_api_references_] != 0) {
        num_api_references_++;
      }
    }
  }
#endif  // DEBUG
  CHECK_EQ(magic_number_, SerializedData::kMagicNumber);
}

void Deserializer::Rehash() {
  DCHECK(can_rehash() || deserializing_user_code());
  for (HeapObject item : to_rehash_) {
    item.RehashBasedOnMap(ReadOnlyRoots(isolate_));
  }
}

Deserializer::~Deserializer() {
#ifdef DEBUG
  // Do not perform checks if we aborted deserialization.
  if (source_.position() == 0) return;
  // Check that we only have padding bytes remaining.
  while (source_.HasMore()) DCHECK_EQ(kNop, source_.Get());
  // Check that we've fully used all reserved space.
  DCHECK(allocator()->ReservationsAreFullyUsed());
#endif  // DEBUG
}

// This is called on the roots.  It is the driver of the deserialization
// process.  It is also called on the body of each function.
void Deserializer::VisitRootPointers(Root root, const char* description,
                                     FullObjectSlot start, FullObjectSlot end) {
  // We are reading to a location outside of JS heap, so pass kNew to avoid
  // triggering write barriers.
  ReadData(FullMaybeObjectSlot(start), FullMaybeObjectSlot(end),
           SnapshotSpace::kNew, kNullAddress);
}

void Deserializer::Synchronize(VisitorSynchronization::SyncTag tag) {
  static const byte expected = kSynchronize;
  CHECK_EQ(expected, source_.Get());
}

void Deserializer::DeserializeDeferredObjects() {
  DisallowHeapAllocation no_gc;

  for (int code = source_.Get(); code != kSynchronize; code = source_.Get()) {
    switch (code) {
      case kAlignmentPrefix:
      case kAlignmentPrefix + 1:
      case kAlignmentPrefix + 2: {
        int alignment = code - (SerializerDeserializer::kAlignmentPrefix - 1);
        allocator()->SetAlignment(static_cast<AllocationAlignment>(alignment));
        break;
      }
      default: {
        const int space_number = code & kSpaceMask;
        DCHECK_LE(space_number, kNumberOfSpaces);
        DCHECK_EQ(code - space_number, kNewObject);
        SnapshotSpace space = static_cast<SnapshotSpace>(space_number);
        HeapObject object = GetBackReferencedObject(space);
        int size = source_.GetInt() << kTaggedSizeLog2;
        Address obj_address = object.address();
        // Object's map is already initialized, now read the rest.
        MaybeObjectSlot start(obj_address + kTaggedSize);
        MaybeObjectSlot end(obj_address + size);
        bool filled = ReadData(start, end, space, obj_address);
        CHECK(filled);
        DCHECK(CanBeDeferred(object));
        PostProcessNewObject(object, space);
      }
    }
  }
}

void Deserializer::LogNewObjectEvents() {
  {
    // {new_maps_} and {new_code_objects_} are vectors containing raw
    // pointers, hence there should be no GC happening.
    DisallowHeapAllocation no_gc;
    // Issue code events for newly deserialized code objects.
    LOG_CODE_EVENT(isolate_, LogCodeObjects());
  }
  LOG_CODE_EVENT(isolate_, LogCompiledFunctions());
  LogNewMapEvents();
}

void Deserializer::LogNewMapEvents() {
  DisallowHeapAllocation no_gc;
  for (Map map : new_maps()) {
    DCHECK(FLAG_trace_maps);
    LOG(isolate_, MapCreate(map));
    LOG(isolate_, MapDetails(map));
  }
}

void Deserializer::LogScriptEvents(Script script) {
  DisallowHeapAllocation no_gc;
  LOG(isolate_,
      ScriptEvent(Logger::ScriptEventType::kDeserialize, script.id()));
  LOG(isolate_, ScriptDetails(script));
}

StringTableInsertionKey::StringTableInsertionKey(String string)
    : StringTableKey(ComputeHashField(string), string.length()),
      string_(string) {
  DCHECK(string.IsInternalizedString());
}

bool StringTableInsertionKey::IsMatch(String string) {
  // We want to compare the content of two strings here.
  return string_.SlowEquals(string);
}

Handle<String> StringTableInsertionKey::AsHandle(Isolate* isolate) {
  return handle(string_, isolate);
}

uint32_t StringTableInsertionKey::ComputeHashField(String string) {
  // Make sure hash_field() is computed.
  string.Hash();
  return string.hash_field();
}

namespace {

String ForwardStringIfExists(Isolate* isolate, StringTableInsertionKey* key) {
  StringTable table = isolate->heap()->string_table();
  InternalIndex entry = table.FindEntry(isolate, key);
  if (entry.is_not_found()) return String();

  String canonical = String::cast(table.KeyAt(entry));
  DCHECK_NE(canonical, key->string());
  key->string().MakeThin(isolate, canonical);
  return canonical;
}

}  // namespace

HeapObject Deserializer::PostProcessNewObject(HeapObject obj,
                                              SnapshotSpace space) {
  DisallowHeapAllocation no_gc;

  if ((FLAG_rehash_snapshot && can_rehash_) || deserializing_user_code()) {
    if (obj.IsString()) {
      // Uninitialize hash field as we need to recompute the hash.
      String string = String::cast(obj);
      string.set_hash_field(String::kEmptyHashField);
      // Rehash strings before read-only space is sealed. Strings outside
      // read-only space are rehashed lazily. (e.g. when rehashing dictionaries)
      if (space == SnapshotSpace::kReadOnlyHeap) {
        to_rehash_.push_back(obj);
      }
    } else if (obj.NeedsRehashing()) {
      to_rehash_.push_back(obj);
    }
  }

  if (deserializing_user_code()) {
    if (obj.IsString()) {
      String string = String::cast(obj);
      if (string.IsInternalizedString()) {
        // Canonicalize the internalized string. If it already exists in the
        // string table, set it to forward to the existing one.
        StringTableInsertionKey key(string);
        String canonical = ForwardStringIfExists(isolate_, &key);

        if (!canonical.is_null()) return canonical;

        new_internalized_strings_.push_back(handle(string, isolate_));
        return string;
      }
    } else if (obj.IsScript()) {
      new_scripts_.push_back(handle(Script::cast(obj), isolate_));
    } else if (obj.IsAllocationSite()) {
      // We should link new allocation sites, but we can't do this immediately
      // because |AllocationSite::HasWeakNext()| internally accesses
      // |Heap::roots_| that may not have been initialized yet. So defer this to
      // |ObjectDeserializer::CommitPostProcessedObjects()|.
      new_allocation_sites_.push_back(AllocationSite::cast(obj));
    } else {
      DCHECK(CanBeDeferred(obj));
    }
  }
  if (obj.IsScript()) {
    LogScriptEvents(Script::cast(obj));
  } else if (obj.IsCode()) {
    // We flush all code pages after deserializing the startup snapshot.
    // Hence we only remember each individual code object when deserializing
    // user code.
    if (deserializing_user_code() || space == SnapshotSpace::kLargeObject) {
      new_code_objects_.push_back(Code::cast(obj));
    }
  } else if (FLAG_trace_maps && obj.IsMap()) {
    // Keep track of all seen Maps to log them later since they might be only
    // partially initialized at this point.
    new_maps_.push_back(Map::cast(obj));
  } else if (obj.IsAccessorInfo()) {
#ifdef USE_SIMULATOR
    accessor_infos_.push_back(AccessorInfo::cast(obj));
#endif
  } else if (obj.IsCallHandlerInfo()) {
#ifdef USE_SIMULATOR
    call_handler_infos_.push_back(CallHandlerInfo::cast(obj));
#endif
  } else if (obj.IsExternalString()) {
    ExternalString string = ExternalString::cast(obj);
    uint32_t index = string.resource_as_uint32();
    Address address =
        static_cast<Address>(isolate_->api_external_references()[index]);
    string.set_address_as_resource(address);
    isolate_->heap()->UpdateExternalString(string, 0,
                                           string.ExternalPayloadSize());
    isolate_->heap()->RegisterExternalString(String::cast(obj));
  } else if (obj.IsJSDataView()) {
    JSDataView data_view = JSDataView::cast(obj);
    JSArrayBuffer buffer = JSArrayBuffer::cast(data_view.buffer());
    void* backing_store = nullptr;
    if (buffer.backing_store() != nullptr) {
      // The backing store of the JSArrayBuffer has not been correctly restored
      // yet, as that may trigger GC. The backing_store field currently contains
      // a numbered reference to an already deserialized backing store.
      size_t store_index = reinterpret_cast<size_t>(buffer.backing_store());
      backing_store = backing_stores_[store_index]->buffer_start();
    }
    data_view.set_data_pointer(reinterpret_cast<uint8_t*>(backing_store) +
                               data_view.byte_offset());
  } else if (obj.IsJSTypedArray()) {
    JSTypedArray typed_array = JSTypedArray::cast(obj);
    // Fixup typed array pointers.
    if (typed_array.is_on_heap()) {
      typed_array.SetOnHeapDataPtr(HeapObject::cast(typed_array.base_pointer()),
                                   typed_array.external_pointer());
    } else {
      // Serializer writes backing store ref as a DataPtr() value.
      size_t store_index = reinterpret_cast<size_t>(typed_array.DataPtr());
      auto backing_store = backing_stores_[store_index];
      auto start = backing_store
                       ? reinterpret_cast<byte*>(backing_store->buffer_start())
                       : nullptr;
      typed_array.SetOffHeapDataPtr(start, typed_array.byte_offset());
    }
  } else if (obj.IsJSArrayBuffer()) {
    JSArrayBuffer buffer = JSArrayBuffer::cast(obj);
    buffer.set_extension(nullptr);
    // Only fixup for the off-heap case. This may trigger GC.
    if (buffer.backing_store() != nullptr) {
      new_off_heap_array_buffers_.push_back(handle(buffer, isolate_));
    }
  } else if (obj.IsBytecodeArray()) {
    // TODO(mythria): Remove these once we store the default values for these
    // fields in the serializer.
    BytecodeArray bytecode_array = BytecodeArray::cast(obj);
    bytecode_array.set_osr_loop_nesting_level(0);
  }
#ifdef DEBUG
  if (obj.IsDescriptorArray()) {
    DescriptorArray descriptor_array = DescriptorArray::cast(obj);
    DCHECK_EQ(0, descriptor_array.raw_number_of_marked_descriptors());
  }
#endif

  // Check alignment.
  DCHECK_EQ(0, Heap::GetFillToAlign(obj.address(),
                                    HeapObject::RequiredAlignment(obj.map())));
  return obj;
}

HeapObject Deserializer::GetBackReferencedObject(SnapshotSpace space) {
  HeapObject obj;
  switch (space) {
    case SnapshotSpace::kLargeObject:
      obj = allocator()->GetLargeObject(source_.GetInt());
      break;
    case SnapshotSpace::kMap:
      obj = allocator()->GetMap(source_.GetInt());
      break;
    case SnapshotSpace::kReadOnlyHeap: {
      uint32_t chunk_index = source_.GetInt();
      uint32_t chunk_offset = source_.GetInt();
      if (isolate()->heap()->deserialization_complete()) {
        PagedSpace* read_only_space = isolate()->heap()->read_only_space();
        Page* page = read_only_space->first_page();
        for (uint32_t i = 0; i < chunk_index; ++i) {
          page = page->next_page();
        }
        Address address = page->OffsetToAddress(chunk_offset);
        obj = HeapObject::FromAddress(address);
      } else {
        obj = allocator()->GetObject(space, chunk_index, chunk_offset);
      }
      break;
    }
    default: {
      uint32_t chunk_index = source_.GetInt();
      uint32_t chunk_offset = source_.GetInt();
      obj = allocator()->GetObject(space, chunk_index, chunk_offset);
      break;
    }
  }

  if (deserializing_user_code() && obj.IsThinString()) {
    obj = ThinString::cast(obj).actual();
  }

  hot_objects_.Add(obj);
  DCHECK(!HasWeakHeapObjectTag(obj));
  return obj;
}

HeapObject Deserializer::ReadObject() {
  MaybeObject object;
  // We are reading to a location outside of JS heap, so pass kNew to avoid
  // triggering write barriers.
  bool filled =
      ReadData(FullMaybeObjectSlot(&object), FullMaybeObjectSlot(&object + 1),
               SnapshotSpace::kNew, kNullAddress);
  CHECK(filled);
  return object.GetHeapObjectAssumeStrong();
}

HeapObject Deserializer::ReadObject(SnapshotSpace space) {
  DisallowHeapAllocation no_gc;

  const int size = source_.GetInt() << kObjectAlignmentBits;

  Address address = allocator()->Allocate(space, size);
  HeapObject obj = HeapObject::FromAddress(address);

  isolate_->heap()->OnAllocationEvent(obj, size);
  MaybeObjectSlot current(address);
  MaybeObjectSlot limit(address + size);

  if (ReadData(current, limit, space, address)) {
    // Only post process if object content has not been deferred.
    obj = PostProcessNewObject(obj, space);
  }

#ifdef DEBUG
  if (obj.IsCode()) {
    DCHECK(space == SnapshotSpace::kCode ||
           space == SnapshotSpace::kReadOnlyHeap);
  } else {
    DCHECK_NE(space, SnapshotSpace::kCode);
  }
#endif  // DEBUG
  return obj;
}

void Deserializer::ReadCodeObjectBody(SnapshotSpace space,
                                      Address code_object_address) {
  // At this point the code object is already allocated, its map field is
  // initialized and its raw data fields and code stream are also read.
  // Now we read the rest of code header's fields.
  MaybeObjectSlot current(code_object_address + HeapObject::kHeaderSize);
  MaybeObjectSlot limit(code_object_address + Code::kDataStart);
  bool filled = ReadData(current, limit, space, code_object_address);
  CHECK(filled);

  // Now iterate RelocInfos the same way it was done by the serialzier and
  // deserialize respective data into RelocInfos.
  Code code = Code::cast(HeapObject::FromAddress(code_object_address));
  RelocIterator it(code, Code::BodyDescriptor::kRelocModeMask);
  for (; !it.done(); it.next()) {
    RelocInfo rinfo = *it.rinfo();
    rinfo.Visit(this);
  }
}

void Deserializer::VisitCodeTarget(Code host, RelocInfo* rinfo) {
  HeapObject object = ReadObject();
  rinfo->set_target_address(Code::cast(object).raw_instruction_start());
}

void Deserializer::VisitEmbeddedPointer(Code host, RelocInfo* rinfo) {
  HeapObject object = ReadObject();
  // Embedded object reference must be a strong one.
  rinfo->set_target_object(isolate_->heap(), object);
}

void Deserializer::VisitRuntimeEntry(Code host, RelocInfo* rinfo) {
  // We no longer serialize code that contains runtime entries.
  UNREACHABLE();
}

void Deserializer::VisitExternalReference(Code host, RelocInfo* rinfo) {
  byte data = source_.Get();
  CHECK_EQ(data, kExternalReference);

  Address address = ReadExternalReferenceCase();

  if (rinfo->IsCodedSpecially()) {
    Address location_of_branch_data = rinfo->pc();
    Assembler::deserialization_set_special_target_at(location_of_branch_data,
                                                     host, address);
  } else {
    WriteUnalignedValue(rinfo->target_address_address(), address);
  }
}

void Deserializer::VisitInternalReference(Code host, RelocInfo* rinfo) {
  byte data = source_.Get();
  CHECK_EQ(data, kInternalReference);

  // Internal reference target is encoded as an offset from code entry.
  int target_offset = source_.GetInt();
  DCHECK_LT(static_cast<unsigned>(target_offset),
            static_cast<unsigned>(host.raw_instruction_size()));
  Address target = host.entry() + target_offset;
  Assembler::deserialization_set_target_internal_reference_at(
      rinfo->pc(), target, rinfo->rmode());
}

void Deserializer::VisitOffHeapTarget(Code host, RelocInfo* rinfo) {
  byte data = source_.Get();
  CHECK_EQ(data, kOffHeapTarget);

  int builtin_index = source_.GetInt();
  DCHECK(Builtins::IsBuiltinId(builtin_index));

  CHECK_NOT_NULL(isolate_->embedded_blob());
  EmbeddedData d = EmbeddedData::FromBlob();
  Address address = d.InstructionStartOfBuiltin(builtin_index);
  CHECK_NE(kNullAddress, address);

  // TODO(ishell): implement RelocInfo::set_target_off_heap_target()
  if (RelocInfo::OffHeapTargetIsCodedSpecially()) {
    Address location_of_branch_data = rinfo->pc();
    Assembler::deserialization_set_special_target_at(location_of_branch_data,
                                                     host, address);
  } else {
    WriteUnalignedValue(rinfo->target_address_address(), address);
  }
}

template <typename TSlot>
TSlot Deserializer::ReadRepeatedObject(TSlot current, int repeat_count) {
  CHECK_LE(2, repeat_count);

  HeapObject heap_object = ReadObject();
  DCHECK(!Heap::InYoungGeneration(heap_object));
  for (int i = 0; i < repeat_count; i++) {
    // Repeated values are not subject to the write barrier so we don't need
    // to trigger it.
    current = Write(current, MaybeObject::FromObject(heap_object));
  }
  return current;
}

static void NoExternalReferencesCallback() {
  // The following check will trigger if a function or object template
  // with references to native functions have been deserialized from
  // snapshot, but no actual external references were provided when the
  // isolate was created.
  CHECK_WITH_MSG(false, "No external references provided via API");
}

template <typename TSlot>
bool Deserializer::ReadData(TSlot current, TSlot limit,
                            SnapshotSpace source_space,
                            Address current_object_address) {
  Isolate* const isolate = isolate_;
  // Write barrier support costs around 1% in startup time.  In fact there
  // are no new space objects in current boot snapshots, so it's not needed,
  // but that may change.
  bool write_barrier_needed =
      (current_object_address != kNullAddress &&
       source_space != SnapshotSpace::kNew &&
       source_space != SnapshotSpace::kCode && !FLAG_disable_write_barriers);
  while (current < limit) {
    byte data = source_.Get();
    switch (data) {
#define CASE_STATEMENT(bytecode, snapshot_space)    \
  case bytecode + static_cast<int>(snapshot_space): \
    STATIC_ASSERT((static_cast<int>(snapshot_space) & ~kSpaceMask) == 0);

#define CASE_BODY(bytecode, space_number_if_any)                             \
  current = ReadDataCase<TSlot, bytecode, space_number_if_any>(              \
      isolate, current, current_object_address, data, write_barrier_needed); \
  break;

// This generates a case and a body for the new space (which has to do extra
// write barrier handling) and handles the other spaces with fall-through cases
// and one body.
#define ALL_SPACES(bytecode)                             \
  CASE_STATEMENT(bytecode, SnapshotSpace::kNew)          \
  CASE_BODY(bytecode, SnapshotSpace::kNew)               \
  CASE_STATEMENT(bytecode, SnapshotSpace::kOld)          \
  V8_FALLTHROUGH;                                        \
  CASE_STATEMENT(bytecode, SnapshotSpace::kCode)         \
  V8_FALLTHROUGH;                                        \
  CASE_STATEMENT(bytecode, SnapshotSpace::kMap)          \
  V8_FALLTHROUGH;                                        \
  CASE_STATEMENT(bytecode, SnapshotSpace::kLargeObject)  \
  V8_FALLTHROUGH;                                        \
  CASE_STATEMENT(bytecode, SnapshotSpace::kReadOnlyHeap) \
  CASE_BODY(bytecode, kAnyOldSpace)

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

#define SINGLE_CASE(bytecode, space) \
  CASE_STATEMENT(bytecode, space)    \
  CASE_BODY(bytecode, space)

      // Deserialize a new object and write a pointer to it to the current
      // object.
      ALL_SPACES(kNewObject)
      // Find a recently deserialized object using its offset from the current
      // allocation point and write a pointer to it to the current object.
      ALL_SPACES(kBackref)
      // Find an object in the roots array and write a pointer to it to the
      // current object.
      SINGLE_CASE(kRootArray, SnapshotSpace::kReadOnlyHeap)
      // Find an object in the partial snapshots cache and write a pointer to it
      // to the current object.
      SINGLE_CASE(kPartialSnapshotCache, SnapshotSpace::kReadOnlyHeap)
      // Find an object in the partial snapshots cache and write a pointer to it
      // to the current object.
      SINGLE_CASE(kReadOnlyObjectCache, SnapshotSpace::kReadOnlyHeap)
      // Find an object in the attached references and write a pointer to it to
      // the current object.
      SINGLE_CASE(kAttachedReference, SnapshotSpace::kReadOnlyHeap)

#undef CASE_STATEMENT
#undef CASE_BODY
#undef ALL_SPACES

      // Find an external reference and write a pointer to it to the current
      // object.
      case kExternalReference: {
        Address address = ReadExternalReferenceCase();
        current = WriteAddress(current, address);
        break;
      }

      case kInternalReference:
      case kOffHeapTarget: {
        // These bytecodes are expected only during RelocInfo iteration.
        UNREACHABLE();
        break;
      }

      case kNop:
        break;

      case kNextChunk: {
        int space = source_.Get();
        allocator()->MoveToNextChunk(static_cast<SnapshotSpace>(space));
        break;
      }

      case kDeferred: {
        // Deferred can only occur right after the heap object header.
        DCHECK_EQ(current.address(), current_object_address + kTaggedSize);
        HeapObject obj = HeapObject::FromAddress(current_object_address);
        // If the deferred object is a map, its instance type may be used
        // during deserialization. Initialize it with a temporary value.
        if (obj.IsMap()) Map::cast(obj).set_instance_type(FILLER_TYPE);
        current = limit;
        return false;
      }

      case kSynchronize:
        // If we get here then that indicates that you have a mismatch between
        // the number of GC roots when serializing and deserializing.
        UNREACHABLE();

      // Deserialize raw data of variable length.
      case kVariableRawData: {
        int size_in_bytes = source_.GetInt();
        DCHECK(IsAligned(size_in_bytes, kTaggedSize));
        source_.CopyRaw(current.ToVoidPtr(), size_in_bytes);
        current = TSlot(current.address() + size_in_bytes);
        break;
      }

      // Deserialize raw code directly into the body of the code object.
      case kVariableRawCode: {
        // VariableRawCode can only occur right after the heap object header.
        DCHECK_EQ(current.address(), current_object_address + kTaggedSize);
        int size_in_bytes = source_.GetInt();
        DCHECK(IsAligned(size_in_bytes, kTaggedSize));
        source_.CopyRaw(
            reinterpret_cast<void*>(current_object_address + Code::kDataStart),
            size_in_bytes);
        // Deserialize tagged fields in the code object header and reloc infos.
        ReadCodeObjectBody(source_space, current_object_address);
        // Set current to the code object end.
        current = TSlot(current.address() + Code::kDataStart -
                        HeapObject::kHeaderSize + size_in_bytes);
        CHECK_EQ(current, limit);
        break;
      }

      case kVariableRepeat: {
        int repeats = DecodeVariableRepeatCount(source_.GetInt());
        current = ReadRepeatedObject(current, repeats);
        break;
      }

      case kOffHeapBackingStore: {
        int byte_length = source_.GetInt();
        std::unique_ptr<BackingStore> backing_store =
            BackingStore::Allocate(isolate, byte_length, SharedFlag::kNotShared,
                                   InitializedFlag::kUninitialized);
        CHECK_NOT_NULL(backing_store);
        source_.CopyRaw(backing_store->buffer_start(), byte_length);
        backing_stores_.push_back(std::move(backing_store));
        break;
      }

      case kApiReference: {
        uint32_t reference_id = static_cast<uint32_t>(source_.GetInt());
        Address address;
        if (isolate->api_external_references()) {
          DCHECK_WITH_MSG(
              reference_id < num_api_references_,
              "too few external references provided through the API");
          address = static_cast<Address>(
              isolate->api_external_references()[reference_id]);
        } else {
          address = reinterpret_cast<Address>(NoExternalReferencesCallback);
        }
        current = WriteAddress(current, address);
        break;
      }

      case kClearedWeakReference:
        current = Write(current, HeapObjectReference::ClearedValue(isolate_));
        break;

      case kWeakPrefix:
        DCHECK(!allocator()->next_reference_is_weak());
        allocator()->set_next_reference_is_weak(true);
        break;

      case kAlignmentPrefix:
      case kAlignmentPrefix + 1:
      case kAlignmentPrefix + 2: {
        int alignment = data - (SerializerDeserializer::kAlignmentPrefix - 1);
        allocator()->SetAlignment(static_cast<AllocationAlignment>(alignment));
        break;
      }

      // First kNumberOfRootArrayConstants roots are guaranteed to be in
      // the old space.
      STATIC_ASSERT(
          static_cast<int>(RootIndex::kFirstImmortalImmovableRoot) == 0);
      STATIC_ASSERT(kNumberOfRootArrayConstants <=
                    static_cast<int>(RootIndex::kLastImmortalImmovableRoot));
      STATIC_ASSERT(kNumberOfRootArrayConstants == 32);
      SIXTEEN_CASES(kRootArrayConstants)
      SIXTEEN_CASES(kRootArrayConstants + 16) {
        int id = data & kRootArrayConstantsMask;
        RootIndex root_index = static_cast<RootIndex>(id);
        MaybeObject object = MaybeObject::FromObject(isolate->root(root_index));
        DCHECK(!Heap::InYoungGeneration(object));
        current = Write(current, object);
        break;
      }

      STATIC_ASSERT(kNumberOfHotObjects == 8);
      FOUR_CASES(kHotObject)
      FOUR_CASES(kHotObject + 4) {
        int index = data & kHotObjectMask;
        Object hot_object = hot_objects_.Get(index);
        MaybeObject hot_maybe_object = MaybeObject::FromObject(hot_object);
        if (allocator()->GetAndClearNextReferenceIsWeak()) {
          hot_maybe_object = MaybeObject::MakeWeak(hot_maybe_object);
        }
        // Don't update current pointer here as it may be needed for write
        // barrier.
        Write(current, hot_maybe_object);
        if (write_barrier_needed && Heap::InYoungGeneration(hot_object)) {
          HeapObject current_object =
              HeapObject::FromAddress(current_object_address);
          GenerationalBarrier(current_object,
                              MaybeObjectSlot(current.address()),
                              hot_maybe_object);
        }
        ++current;
        break;
      }

      // Deserialize raw data of fixed length from 1 to 32 words.
      STATIC_ASSERT(kNumberOfFixedRawData == 32);
      SIXTEEN_CASES(kFixedRawData)
      SIXTEEN_CASES(kFixedRawData + 16) {
        int size_in_tagged = data - kFixedRawDataStart;
        source_.CopyRaw(current.ToVoidPtr(), size_in_tagged * kTaggedSize);
        current += size_in_tagged;
        break;
      }

      STATIC_ASSERT(kNumberOfFixedRepeat == 16);
      SIXTEEN_CASES(kFixedRepeat) {
        int repeats = DecodeFixedRepeatCount(data);
        current = ReadRepeatedObject(current, repeats);
        break;
      }

#ifdef DEBUG
#define UNUSED_CASE(byte_code) \
  case byte_code:              \
    UNREACHABLE();
      UNUSED_SERIALIZER_BYTE_CODES(UNUSED_CASE)
#endif
#undef UNUSED_CASE

#undef SIXTEEN_CASES
#undef FOUR_CASES
#undef SINGLE_CASE
    }
  }
  CHECK_EQ(limit, current);
  return true;
}

Address Deserializer::ReadExternalReferenceCase() {
  uint32_t reference_id = static_cast<uint32_t>(source_.GetInt());
  return isolate_->external_reference_table()->address(reference_id);
}

template <typename TSlot, SerializerDeserializer::Bytecode bytecode,
          SnapshotSpace space_number_if_any>
TSlot Deserializer::ReadDataCase(Isolate* isolate, TSlot current,
                                 Address current_object_address, byte data,
                                 bool write_barrier_needed) {
  bool emit_write_barrier = false;
  SnapshotSpace space = static_cast<SnapshotSpace>(
      space_number_if_any == kAnyOldSpace
          ? static_cast<SnapshotSpace>(data & kSpaceMask)
          : space_number_if_any);
  HeapObject heap_object;
  HeapObjectReferenceType reference_type =
      allocator()->GetAndClearNextReferenceIsWeak()
          ? HeapObjectReferenceType::WEAK
          : HeapObjectReferenceType::STRONG;

  if (bytecode == kNewObject) {
    heap_object = ReadObject(space);
    emit_write_barrier = (space == SnapshotSpace::kNew);
  } else if (bytecode == kBackref) {
    heap_object = GetBackReferencedObject(space);
    emit_write_barrier = (space == SnapshotSpace::kNew);
  } else if (bytecode == kRootArray) {
    int id = source_.GetInt();
    RootIndex root_index = static_cast<RootIndex>(id);
    heap_object = HeapObject::cast(isolate->root(root_index));
    emit_write_barrier = Heap::InYoungGeneration(heap_object);
    hot_objects_.Add(heap_object);
  } else if (bytecode == kReadOnlyObjectCache) {
    int cache_index = source_.GetInt();
    heap_object = HeapObject::cast(
        isolate->read_only_heap()->cached_read_only_object(cache_index));
    DCHECK(!Heap::InYoungGeneration(heap_object));
    emit_write_barrier = false;
  } else if (bytecode == kPartialSnapshotCache) {
    int cache_index = source_.GetInt();
    heap_object =
        HeapObject::cast(isolate->partial_snapshot_cache()->at(cache_index));
    emit_write_barrier = Heap::InYoungGeneration(heap_object);
  } else {
    DCHECK_EQ(bytecode, kAttachedReference);
    int index = source_.GetInt();
    heap_object = *attached_objects_[index];
    emit_write_barrier = Heap::InYoungGeneration(heap_object);
  }
  HeapObjectReference heap_object_ref =
      reference_type == HeapObjectReferenceType::STRONG
          ? HeapObjectReference::Strong(heap_object)
          : HeapObjectReference::Weak(heap_object);
  // Don't update current pointer here as it may be needed for write barrier.
  Write(current, heap_object_ref);
  if (emit_write_barrier && write_barrier_needed) {
    DCHECK_IMPLIES(FLAG_disable_write_barriers, !write_barrier_needed);
    HeapObject host_object = HeapObject::FromAddress(current_object_address);
    SLOW_DCHECK(isolate->heap()->Contains(host_object));
    GenerationalBarrier(host_object, MaybeObjectSlot(current.address()),
                        heap_object_ref);
  }
  return current + 1;
}

}  // namespace internal
}  // namespace v8
