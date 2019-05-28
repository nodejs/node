// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/deserializer.h"

#include "src/assembler-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate.h"
#include "src/log.h"
#include "src/objects-body-descriptors-inl.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/cell-inl.h"
#include "src/objects/hash-table.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/slots.h"
#include "src/objects/smi.h"
#include "src/objects/string.h"
#include "src/roots.h"
#include "src/snapshot/natives.h"
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
    item->RehashBasedOnMap(ReadOnlyRoots(isolate_));
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
  // We are reading to a location outside of JS heap, so pass NEW_SPACE to
  // avoid triggering write barriers.
  ReadData(FullMaybeObjectSlot(start), FullMaybeObjectSlot(end), NEW_SPACE,
           kNullAddress);
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
      case kAlignmentPrefix + 2: {
        int alignment = code - (SerializerDeserializer::kAlignmentPrefix - 1);
        allocator()->SetAlignment(static_cast<AllocationAlignment>(alignment));
        break;
      }
      default: {
        int space = code & kSpaceMask;
        DCHECK_LE(space, kNumberOfSpaces);
        DCHECK_EQ(code - space, kNewObject);
        HeapObject object = GetBackReferencedObject(space);
        int size = source_.GetInt() << kTaggedSizeLog2;
        Address obj_address = object->address();
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
      ScriptEvent(Logger::ScriptEventType::kDeserialize, script->id()));
  LOG(isolate_, ScriptDetails(script));
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("v8.compile"), "Script",
      TRACE_ID_WITH_SCOPE("v8::internal::Script", script->id()));
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("v8.compile"), "Script",
      TRACE_ID_WITH_SCOPE("v8::internal::Script", script->id()),
      script->ToTracedValue());
}

StringTableInsertionKey::StringTableInsertionKey(String string)
    : StringTableKey(ComputeHashField(string)), string_(string) {
  DCHECK(string->IsInternalizedString());
}

bool StringTableInsertionKey::IsMatch(Object string) {
  // We know that all entries in a hash table had their hash keys created.
  // Use that knowledge to have fast failure.
  if (Hash() != String::cast(string)->Hash()) return false;
  // We want to compare the content of two internalized strings here.
  return string_->SlowEquals(String::cast(string));
}

Handle<String> StringTableInsertionKey::AsHandle(Isolate* isolate) {
  return handle(string_, isolate);
}

uint32_t StringTableInsertionKey::ComputeHashField(String string) {
  // Make sure hash_field() is computed.
  string->Hash();
  return string->hash_field();
}

HeapObject Deserializer::PostProcessNewObject(HeapObject obj, int space) {
  if ((FLAG_rehash_snapshot && can_rehash_) || deserializing_user_code()) {
    if (obj->IsString()) {
      // Uninitialize hash field as we need to recompute the hash.
      String string = String::cast(obj);
      string->set_hash_field(String::kEmptyHashField);
      // Rehash strings before read-only space is sealed. Strings outside
      // read-only space are rehashed lazily. (e.g. when rehashing dictionaries)
      if (space == RO_SPACE) {
        to_rehash_.push_back(obj);
      }
    } else if (obj->NeedsRehashing()) {
      to_rehash_.push_back(obj);
    }
  }

  if (deserializing_user_code()) {
    if (obj->IsString()) {
      String string = String::cast(obj);
      if (string->IsInternalizedString()) {
        // Canonicalize the internalized string. If it already exists in the
        // string table, set it to forward to the existing one.
        StringTableInsertionKey key(string);
        String canonical =
            StringTable::ForwardStringIfExists(isolate_, &key, string);

        if (!canonical.is_null()) return canonical;

        new_internalized_strings_.push_back(handle(string, isolate_));
        return string;
      }
    } else if (obj->IsScript()) {
      new_scripts_.push_back(handle(Script::cast(obj), isolate_));
    } else if (obj->IsAllocationSite()) {
      // We should link new allocation sites, but we can't do this immediately
      // because |AllocationSite::HasWeakNext()| internally accesses
      // |Heap::roots_| that may not have been initialized yet. So defer this to
      // |ObjectDeserializer::CommitPostProcessedObjects()|.
      new_allocation_sites_.push_back(AllocationSite::cast(obj));
    } else {
      DCHECK(CanBeDeferred(obj));
    }
  }
  if (obj->IsScript()) {
    LogScriptEvents(Script::cast(obj));
  } else if (obj->IsCode()) {
    // We flush all code pages after deserializing the startup snapshot.
    // Hence we only remember each individual code object when deserializing
    // user code.
    if (deserializing_user_code() || space == LO_SPACE) {
      new_code_objects_.push_back(Code::cast(obj));
    }
  } else if (FLAG_trace_maps && obj->IsMap()) {
    // Keep track of all seen Maps to log them later since they might be only
    // partially initialized at this point.
    new_maps_.push_back(Map::cast(obj));
  } else if (obj->IsAccessorInfo()) {
#ifdef USE_SIMULATOR
    accessor_infos_.push_back(AccessorInfo::cast(obj));
#endif
  } else if (obj->IsCallHandlerInfo()) {
#ifdef USE_SIMULATOR
    call_handler_infos_.push_back(CallHandlerInfo::cast(obj));
#endif
  } else if (obj->IsExternalString()) {
    if (obj->map() == ReadOnlyRoots(isolate_).native_source_string_map()) {
      ExternalOneByteString string = ExternalOneByteString::cast(obj);
      DCHECK(string->is_uncached());
      string->SetResource(
          isolate_, NativesExternalStringResource::DecodeForDeserialization(
                        string->resource()));
    } else {
      ExternalString string = ExternalString::cast(obj);
      uint32_t index = string->resource_as_uint32();
      Address address =
          static_cast<Address>(isolate_->api_external_references()[index]);
      string->set_address_as_resource(address);
      isolate_->heap()->UpdateExternalString(string, 0,
                                             string->ExternalPayloadSize());
    }
    isolate_->heap()->RegisterExternalString(String::cast(obj));
  } else if (obj->IsJSTypedArray()) {
    JSTypedArray typed_array = JSTypedArray::cast(obj);
    CHECK_LE(typed_array->byte_offset(), Smi::kMaxValue);
    int32_t byte_offset = static_cast<int32_t>(typed_array->byte_offset());
    if (byte_offset > 0) {
      FixedTypedArrayBase elements =
          FixedTypedArrayBase::cast(typed_array->elements());
      // Must be off-heap layout.
      DCHECK(!typed_array->is_on_heap());

      void* pointer_with_offset = reinterpret_cast<void*>(
          reinterpret_cast<intptr_t>(elements->external_pointer()) +
          byte_offset);
      elements->set_external_pointer(pointer_with_offset);
    }
  } else if (obj->IsJSArrayBuffer()) {
    JSArrayBuffer buffer = JSArrayBuffer::cast(obj);
    // Only fixup for the off-heap case.
    if (buffer->backing_store() != nullptr) {
      Smi store_index(reinterpret_cast<Address>(buffer->backing_store()));
      void* backing_store = off_heap_backing_stores_[store_index->value()];

      buffer->set_backing_store(backing_store);
      isolate_->heap()->RegisterNewArrayBuffer(buffer);
    }
  } else if (obj->IsFixedTypedArrayBase()) {
    FixedTypedArrayBase fta = FixedTypedArrayBase::cast(obj);
    // Only fixup for the off-heap case.
    if (fta->base_pointer() == Smi::kZero) {
      Smi store_index(reinterpret_cast<Address>(fta->external_pointer()));
      void* backing_store = off_heap_backing_stores_[store_index->value()];
      fta->set_external_pointer(backing_store);
    }
  } else if (obj->IsBytecodeArray()) {
    // TODO(mythria): Remove these once we store the default values for these
    // fields in the serializer.
    BytecodeArray bytecode_array = BytecodeArray::cast(obj);
    bytecode_array->set_osr_loop_nesting_level(0);
  }
#ifdef DEBUG
  if (obj->IsDescriptorArray()) {
    DescriptorArray descriptor_array = DescriptorArray::cast(obj);
    DCHECK_EQ(0, descriptor_array->raw_number_of_marked_descriptors());
  }
#endif

  // Check alignment.
  DCHECK_EQ(0, Heap::GetFillToAlign(obj->address(),
                                    HeapObject::RequiredAlignment(obj->map())));
  return obj;
}

HeapObject Deserializer::GetBackReferencedObject(int space) {
  HeapObject obj;
  switch (space) {
    case LO_SPACE:
      obj = allocator()->GetLargeObject(source_.GetInt());
      break;
    case MAP_SPACE:
      obj = allocator()->GetMap(source_.GetInt());
      break;
    case RO_SPACE: {
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
        obj = allocator()->GetObject(static_cast<AllocationSpace>(space),
                                     chunk_index, chunk_offset);
      }
      break;
    }
    default: {
      uint32_t chunk_index = source_.GetInt();
      uint32_t chunk_offset = source_.GetInt();
      obj = allocator()->GetObject(static_cast<AllocationSpace>(space),
                                   chunk_index, chunk_offset);
      break;
    }
  }

  if (deserializing_user_code() && obj->IsThinString()) {
    obj = ThinString::cast(obj)->actual();
  }

  hot_objects_.Add(obj);
  DCHECK(!HasWeakHeapObjectTag(obj->ptr()));
  return obj;
}

HeapObject Deserializer::ReadObject() {
  MaybeObject object;
  // We are reading to a location outside of JS heap, so pass NEW_SPACE to
  // avoid triggering write barriers.
  bool filled =
      ReadData(FullMaybeObjectSlot(&object), FullMaybeObjectSlot(&object + 1),
               NEW_SPACE, kNullAddress);
  CHECK(filled);
  return object.GetHeapObjectAssumeStrong();
}

HeapObject Deserializer::ReadObject(int space_number) {
  const int size = source_.GetInt() << kObjectAlignmentBits;

  Address address =
      allocator()->Allocate(static_cast<AllocationSpace>(space_number), size);
  HeapObject obj = HeapObject::FromAddress(address);

  isolate_->heap()->OnAllocationEvent(obj, size);
  MaybeObjectSlot current(address);
  MaybeObjectSlot limit(address + size);

  if (ReadData(current, limit, space_number, address)) {
    // Only post process if object content has not been deferred.
    obj = PostProcessNewObject(obj, space_number);
  }

#ifdef DEBUG
  if (obj->IsCode()) {
    DCHECK(space_number == CODE_SPACE || space_number == CODE_LO_SPACE);
  } else {
    DCHECK(space_number != CODE_SPACE && space_number != CODE_LO_SPACE);
  }
#endif  // DEBUG
  return obj;
}

void Deserializer::ReadCodeObjectBody(int space_number,
                                      Address code_object_address) {
  // At this point the code object is already allocated, its map field is
  // initialized and its raw data fields and code stream are also read.
  // Now we read the rest of code header's fields.
  MaybeObjectSlot current(code_object_address + HeapObject::kHeaderSize);
  MaybeObjectSlot limit(code_object_address + Code::kDataStart);
  bool filled = ReadData(current, limit, space_number, code_object_address);
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
  rinfo->set_target_address(Code::cast(object)->raw_instruction_start());
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
            static_cast<unsigned>(host->raw_instruction_size()));
  Address target = host->entry() + target_offset;
  Assembler::deserialization_set_target_internal_reference_at(
      rinfo->pc(), target, rinfo->rmode());
}

void Deserializer::VisitOffHeapTarget(Code host, RelocInfo* rinfo) {
  DCHECK(FLAG_embedded_builtins);
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
bool Deserializer::ReadData(TSlot current, TSlot limit, int source_space,
                            Address current_object_address) {
  Isolate* const isolate = isolate_;
  // Write barrier support costs around 1% in startup time.  In fact there
  // are no new space objects in current boot snapshots, so it's not needed,
  // but that may change.
  bool write_barrier_needed =
      (current_object_address != kNullAddress && source_space != NEW_SPACE &&
       source_space != CODE_SPACE);
  while (current < limit) {
    byte data = source_.Get();
    switch (data) {
#define CASE_STATEMENT(bytecode, space_number) \
  case bytecode + space_number:                \
    STATIC_ASSERT((space_number & ~kSpaceMask) == 0);

#define CASE_BODY(bytecode, space_number_if_any)                             \
  current = ReadDataCase<TSlot, bytecode, space_number_if_any>(              \
      isolate, current, current_object_address, data, write_barrier_needed); \
  break;

// This generates a case and a body for the new space (which has to do extra
// write barrier handling) and handles the other spaces with fall-through cases
// and one body.
#define ALL_SPACES(bytecode)           \
  CASE_STATEMENT(bytecode, NEW_SPACE)  \
  CASE_BODY(bytecode, NEW_SPACE)       \
  CASE_STATEMENT(bytecode, OLD_SPACE)  \
  V8_FALLTHROUGH;                      \
  CASE_STATEMENT(bytecode, CODE_SPACE) \
  V8_FALLTHROUGH;                      \
  CASE_STATEMENT(bytecode, MAP_SPACE)  \
  V8_FALLTHROUGH;                      \
  CASE_STATEMENT(bytecode, LO_SPACE)   \
  V8_FALLTHROUGH;                      \
  CASE_STATEMENT(bytecode, RO_SPACE)   \
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
      SINGLE_CASE(kRootArray, RO_SPACE)
      // Find an object in the partial snapshots cache and write a pointer to it
      // to the current object.
      SINGLE_CASE(kPartialSnapshotCache, RO_SPACE)
      // Find an object in the partial snapshots cache and write a pointer to it
      // to the current object.
      SINGLE_CASE(kReadOnlyObjectCache, RO_SPACE)
      // Find an object in the attached references and write a pointer to it to
      // the current object.
      SINGLE_CASE(kAttachedReference, RO_SPACE)

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
        allocator()->MoveToNextChunk(static_cast<AllocationSpace>(space));
        break;
      }

      case kDeferred: {
        // Deferred can only occur right after the heap object header.
        DCHECK_EQ(current.address(), current_object_address + kTaggedSize);
        HeapObject obj = HeapObject::FromAddress(current_object_address);
        // If the deferred object is a map, its instance type may be used
        // during deserialization. Initialize it with a temporary value.
        if (obj->IsMap()) Map::cast(obj)->set_instance_type(FILLER_TYPE);
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
        byte* backing_store = static_cast<byte*>(
            isolate->array_buffer_allocator()->AllocateUninitialized(
                byte_length));
        CHECK_NOT_NULL(backing_store);
        source_.CopyRaw(backing_store, byte_length);
        off_heap_backing_stores_.push_back(backing_store);
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
          int space_number_if_any>
TSlot Deserializer::ReadDataCase(Isolate* isolate, TSlot current,
                                 Address current_object_address, byte data,
                                 bool write_barrier_needed) {
  bool emit_write_barrier = false;
  int space_number = space_number_if_any == kAnyOldSpace ? (data & kSpaceMask)
                                                         : space_number_if_any;
  HeapObject heap_object;
  HeapObjectReferenceType reference_type =
      allocator()->GetAndClearNextReferenceIsWeak()
          ? HeapObjectReferenceType::WEAK
          : HeapObjectReferenceType::STRONG;

  if (bytecode == kNewObject) {
    heap_object = ReadObject(space_number);
    emit_write_barrier = (space_number == NEW_SPACE);
  } else if (bytecode == kBackref) {
    heap_object = GetBackReferencedObject(space_number);
    emit_write_barrier = (space_number == NEW_SPACE);
  } else if (bytecode == kRootArray) {
    int id = source_.GetInt();
    RootIndex root_index = static_cast<RootIndex>(id);
    heap_object = HeapObject::cast(isolate->root(root_index));
    emit_write_barrier = Heap::InYoungGeneration(heap_object);
    hot_objects_.Add(heap_object);
  } else if (bytecode == kReadOnlyObjectCache) {
    int cache_index = source_.GetInt();
    heap_object = HeapObject::cast(
        isolate->heap()->read_only_heap()->read_only_object_cache()->at(
            cache_index));
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
    HeapObject host_object = HeapObject::FromAddress(current_object_address);
    SLOW_DCHECK(isolate->heap()->Contains(host_object));
    GenerationalBarrier(host_object, MaybeObjectSlot(current.address()),
                        heap_object_ref);
  }
  return current + 1;
}

}  // namespace internal
}  // namespace v8
