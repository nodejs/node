// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/deserializer.h"

#include "src/base/logging.h"
#include "src/base/platform/wrappers.h"
#include "src/codegen/assembler-inl.h"
#include "src/common/assert-scope.h"
#include "src/common/external-pointer.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/heap-write-barrier.h"
#include "src/heap/read-only-heap.h"
#include "src/interpreter/interpreter.h"
#include "src/logging/log.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/cell-inl.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/hash-table.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/objects/objects.h"
#include "src/objects/slots.h"
#include "src/objects/smi.h"
#include "src/objects/string.h"
#include "src/roots/roots.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/snapshot/references.h"
#include "src/snapshot/serializer-deserializer.h"
#include "src/snapshot/snapshot-data.h"
#include "src/snapshot/snapshot.h"
#include "src/tracing/trace-event.h"
#include "src/tracing/traced-value.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {

// A SlotAccessor for a slot in a HeapObject, which abstracts the slot
// operations done by the deserializer in a way which is GC-safe. In particular,
// rather than an absolute slot address, this accessor holds a Handle to the
// HeapObject, which is updated if the HeapObject moves.
class SlotAccessorForHeapObject {
 public:
  static SlotAccessorForHeapObject ForSlotIndex(Handle<HeapObject> object,
                                                int index) {
    return SlotAccessorForHeapObject(object, index * kTaggedSize);
  }
  static SlotAccessorForHeapObject ForSlotOffset(Handle<HeapObject> object,
                                                 int offset) {
    return SlotAccessorForHeapObject(object, offset);
  }

  MaybeObjectSlot slot() const { return object_->RawMaybeWeakField(offset_); }
  Handle<HeapObject> object() const { return object_; }
  int offset() const { return offset_; }

  // Writes the given value to this slot, optionally with an offset (e.g. for
  // repeat writes). Returns the number of slots written (which is one).
  int Write(MaybeObject value, int slot_offset = 0) {
    MaybeObjectSlot current_slot = slot() + slot_offset;
    current_slot.Relaxed_Store(value);
    WriteBarrier::Marking(*object_, current_slot, value);
    // No need for a generational write barrier.
    DCHECK(!Heap::InYoungGeneration(value));
    return 1;
  }
  int Write(HeapObject value, HeapObjectReferenceType ref_type,
            int slot_offset = 0) {
    return Write(HeapObjectReference::From(value, ref_type), slot_offset);
  }
  int Write(Handle<HeapObject> value, HeapObjectReferenceType ref_type,
            int slot_offset = 0) {
    return Write(*value, ref_type, slot_offset);
  }

  // Same as Write, but additionally with a generational barrier.
  int WriteWithGenerationalBarrier(MaybeObject value) {
    MaybeObjectSlot current_slot = slot();
    current_slot.Relaxed_Store(value);
    WriteBarrier::Marking(*object_, current_slot, value);
    if (Heap::InYoungGeneration(value)) {
      GenerationalBarrier(*object_, current_slot, value);
    }
    return 1;
  }
  int WriteWithGenerationalBarrier(HeapObject value,
                                   HeapObjectReferenceType ref_type) {
    return WriteWithGenerationalBarrier(
        HeapObjectReference::From(value, ref_type));
  }
  int WriteWithGenerationalBarrier(Handle<HeapObject> value,
                                   HeapObjectReferenceType ref_type) {
    return WriteWithGenerationalBarrier(*value, ref_type);
  }

 private:
  SlotAccessorForHeapObject(Handle<HeapObject> object, int offset)
      : object_(object), offset_(offset) {}

  const Handle<HeapObject> object_;
  const int offset_;
};

// A SlotAccessor for absolute full slot addresses.
class SlotAccessorForRootSlots {
 public:
  explicit SlotAccessorForRootSlots(FullMaybeObjectSlot slot) : slot_(slot) {}

  FullMaybeObjectSlot slot() const { return slot_; }
  Handle<HeapObject> object() const { UNREACHABLE(); }
  int offset() const { UNREACHABLE(); }

  // Writes the given value to this slot, optionally with an offset (e.g. for
  // repeat writes). Returns the number of slots written (which is one).
  int Write(MaybeObject value, int slot_offset = 0) {
    FullMaybeObjectSlot current_slot = slot() + slot_offset;
    current_slot.Relaxed_Store(value);
    return 1;
  }
  int Write(HeapObject value, HeapObjectReferenceType ref_type,
            int slot_offset = 0) {
    return Write(HeapObjectReference::From(value, ref_type), slot_offset);
  }
  int Write(Handle<HeapObject> value, HeapObjectReferenceType ref_type,
            int slot_offset = 0) {
    return Write(*value, ref_type, slot_offset);
  }

  int WriteWithGenerationalBarrier(MaybeObject value) { return Write(value); }
  int WriteWithGenerationalBarrier(HeapObject value,
                                   HeapObjectReferenceType ref_type) {
    return WriteWithGenerationalBarrier(
        HeapObjectReference::From(value, ref_type));
  }
  int WriteWithGenerationalBarrier(Handle<HeapObject> value,
                                   HeapObjectReferenceType ref_type) {
    return WriteWithGenerationalBarrier(*value, ref_type);
  }

 private:
  const FullMaybeObjectSlot slot_;
};

// A SlotAccessor for creating a Handle, which saves a Handle allocation when
// a Handle already exists.
class SlotAccessorForHandle {
 public:
  SlotAccessorForHandle(Handle<HeapObject>* handle, Isolate* isolate)
      : handle_(handle), isolate_(isolate) {}

  MaybeObjectSlot slot() const { UNREACHABLE(); }
  Handle<HeapObject> object() const { UNREACHABLE(); }
  int offset() const { UNREACHABLE(); }

  int Write(MaybeObject value, int slot_offset = 0) { UNREACHABLE(); }
  int Write(HeapObject value, HeapObjectReferenceType ref_type,
            int slot_offset = 0) {
    DCHECK_EQ(slot_offset, 0);
    DCHECK_EQ(ref_type, HeapObjectReferenceType::STRONG);
    *handle_ = handle(value, isolate_);
    return 1;
  }
  int Write(Handle<HeapObject> value, HeapObjectReferenceType ref_type,
            int slot_offset = 0) {
    DCHECK_EQ(slot_offset, 0);
    DCHECK_EQ(ref_type, HeapObjectReferenceType::STRONG);
    *handle_ = value;
    return 1;
  }

  int WriteWithGenerationalBarrier(HeapObject value,
                                   HeapObjectReferenceType ref_type) {
    return Write(value, ref_type);
  }
  int WriteWithGenerationalBarrier(Handle<HeapObject> value,
                                   HeapObjectReferenceType ref_type) {
    return Write(value, ref_type);
  }

 private:
  Handle<HeapObject>* handle_;
  Isolate* isolate_;
};

template <typename TSlot>
int Deserializer::WriteAddress(TSlot dest, Address value) {
  DCHECK(!next_reference_is_weak_);
  base::Memcpy(dest.ToVoidPtr(), &value, kSystemPointerSize);
  STATIC_ASSERT(IsAligned(kSystemPointerSize, TSlot::kSlotDataSize));
  return (kSystemPointerSize / TSlot::kSlotDataSize);
}

template <typename TSlot>
int Deserializer::WriteExternalPointer(TSlot dest, Address value,
                                       ExternalPointerTag tag) {
  DCHECK(!next_reference_is_weak_);
  InitExternalPointerField(dest.address(), isolate(), value, tag);
  STATIC_ASSERT(IsAligned(kExternalPointerSize, TSlot::kSlotDataSize));
  return (kExternalPointerSize / TSlot::kSlotDataSize);
}

Deserializer::Deserializer(Isolate* isolate, Vector<const byte> payload,
                           uint32_t magic_number, bool deserializing_user_code,
                           bool can_rehash)
    : isolate_(isolate),
      source_(payload),
      magic_number_(magic_number),
      deserializing_user_code_(deserializing_user_code),
      can_rehash_(can_rehash) {
  DCHECK_NOT_NULL(isolate);
  isolate_->RegisterDeserializerStarted();

  // We start the indices here at 1, so that we can distinguish between an
  // actual index and a nullptr (serialized as kNullRefSentinel) in a
  // deserialized object requiring fix-up.
  STATIC_ASSERT(kNullRefSentinel == 0);
  backing_stores_.push_back({});

#ifdef DEBUG
  num_api_references_ = 0;
  // The read-only deserializer is run by read-only heap set-up before the
  // heap is fully set up. External reference table relies on a few parts of
  // this set-up (like old-space), so it may be uninitialized at this point.
  if (isolate->isolate_data()->external_reference_table()->is_initialized()) {
    // Count the number of external references registered through the API.
    if (isolate->api_external_references() != nullptr) {
      while (isolate->api_external_references()[num_api_references_] != 0) {
        num_api_references_++;
      }
    }
  }
#endif  // DEBUG
  CHECK_EQ(magic_number_, SerializedData::kMagicNumber);
}

void Deserializer::Rehash() {
  DCHECK(can_rehash() || deserializing_user_code());
  for (Handle<HeapObject> item : to_rehash_) {
    item->RehashBasedOnMap(isolate());
  }
}

Deserializer::~Deserializer() {
#ifdef DEBUG
  // Do not perform checks if we aborted deserialization.
  if (source_.position() == 0) return;
  // Check that we only have padding bytes remaining.
  while (source_.HasMore()) DCHECK_EQ(kNop, source_.Get());
  // Check that there are no remaining forward refs.
  DCHECK_EQ(num_unresolved_forward_refs_, 0);
  DCHECK(unresolved_forward_refs_.empty());
#endif  // DEBUG
  isolate_->RegisterDeserializerFinished();
}

// This is called on the roots.  It is the driver of the deserialization
// process.  It is also called on the body of each function.
void Deserializer::VisitRootPointers(Root root, const char* description,
                                     FullObjectSlot start, FullObjectSlot end) {
  ReadData(FullMaybeObjectSlot(start), FullMaybeObjectSlot(end));
}

void Deserializer::Synchronize(VisitorSynchronization::SyncTag tag) {
  static const byte expected = kSynchronize;
  CHECK_EQ(expected, source_.Get());
}

void Deserializer::DeserializeDeferredObjects() {
  for (int code = source_.Get(); code != kSynchronize; code = source_.Get()) {
    SnapshotSpace space = NewObject::Decode(code);
    ReadObject(space);
  }
}

void Deserializer::LogNewMapEvents() {
  DisallowGarbageCollection no_gc;
  for (Handle<Map> map : new_maps_) {
    DCHECK(FLAG_log_maps);
    LOG(isolate(), MapCreate(*map));
    LOG(isolate(), MapDetails(*map));
  }
}

void Deserializer::WeakenDescriptorArrays() {
  DisallowGarbageCollection no_gc;
  for (Handle<DescriptorArray> descriptor_array : new_descriptor_arrays_) {
    DCHECK(descriptor_array->IsStrongDescriptorArray());
    descriptor_array->set_map(ReadOnlyRoots(isolate()).descriptor_array_map());
    WriteBarrier::Marking(*descriptor_array,
                          descriptor_array->number_of_descriptors());
  }
}

void Deserializer::LogScriptEvents(Script script) {
  DisallowGarbageCollection no_gc;
  LOG(isolate(),
      ScriptEvent(Logger::ScriptEventType::kDeserialize, script.id()));
  LOG(isolate(), ScriptDetails(script));
}

StringTableInsertionKey::StringTableInsertionKey(Handle<String> string)
    : StringTableKey(ComputeRawHashField(*string), string->length()),
      string_(string) {
  DCHECK(string->IsInternalizedString());
}

bool StringTableInsertionKey::IsMatch(Isolate* isolate, String string) {
  // We want to compare the content of two strings here.
  return string_->SlowEquals(string);
}

Handle<String> StringTableInsertionKey::AsHandle(Isolate* isolate) {
  return string_;
}

uint32_t StringTableInsertionKey::ComputeRawHashField(String string) {
  // Make sure raw_hash_field() is computed.
  string.EnsureHash();
  return string.raw_hash_field();
}

void Deserializer::PostProcessNewObject(Handle<Map> map, Handle<HeapObject> obj,
                                        SnapshotSpace space) {
  DCHECK_EQ(*map, obj->map());
  DisallowGarbageCollection no_gc;
  InstanceType instance_type = map->instance_type();

  if ((FLAG_rehash_snapshot && can_rehash_) || deserializing_user_code()) {
    if (InstanceTypeChecker::IsString(instance_type)) {
      // Uninitialize hash field as we need to recompute the hash.
      Handle<String> string = Handle<String>::cast(obj);
      string->set_raw_hash_field(String::kEmptyHashField);
      // Rehash strings before read-only space is sealed. Strings outside
      // read-only space are rehashed lazily. (e.g. when rehashing dictionaries)
      if (space == SnapshotSpace::kReadOnlyHeap) {
        to_rehash_.push_back(obj);
      }
    } else if (obj->NeedsRehashing(instance_type)) {
      to_rehash_.push_back(obj);
    }
  }

  if (deserializing_user_code()) {
    if (InstanceTypeChecker::IsInternalizedString(instance_type)) {
      // Canonicalize the internalized string. If it already exists in the
      // string table, set it to forward to the existing one.
      Handle<String> string = Handle<String>::cast(obj);

      StringTableInsertionKey key(string);
      Handle<String> result =
          isolate()->string_table()->LookupKey(isolate(), &key);

      if (FLAG_thin_strings && *result != *string) {
        string->MakeThin(isolate(), *result);
        // Mutate the given object handle so that the backreference entry is
        // also updated.
        obj.PatchValue(*result);
      }
      return;
    } else if (InstanceTypeChecker::IsScript(instance_type)) {
      new_scripts_.push_back(Handle<Script>::cast(obj));
    } else if (InstanceTypeChecker::IsAllocationSite(instance_type)) {
      // We should link new allocation sites, but we can't do this immediately
      // because |AllocationSite::HasWeakNext()| internally accesses
      // |Heap::roots_| that may not have been initialized yet. So defer this to
      // |ObjectDeserializer::CommitPostProcessedObjects()|.
      new_allocation_sites_.push_back(Handle<AllocationSite>::cast(obj));
    } else {
      DCHECK(CanBeDeferred(*obj));
    }
  }

  if (InstanceTypeChecker::IsScript(instance_type)) {
    LogScriptEvents(Script::cast(*obj));
  } else if (InstanceTypeChecker::IsCode(instance_type)) {
    // We flush all code pages after deserializing the startup snapshot.
    // Hence we only remember each individual code object when deserializing
    // user code.
    if (deserializing_user_code()) {
      new_code_objects_.push_back(Handle<Code>::cast(obj));
    }
  } else if (InstanceTypeChecker::IsMap(instance_type)) {
    if (FLAG_log_maps) {
      // Keep track of all seen Maps to log them later since they might be only
      // partially initialized at this point.
      new_maps_.push_back(Handle<Map>::cast(obj));
    }
  } else if (InstanceTypeChecker::IsAccessorInfo(instance_type)) {
#ifdef USE_SIMULATOR
    accessor_infos_.push_back(Handle<AccessorInfo>::cast(obj));
#endif
  } else if (InstanceTypeChecker::IsCallHandlerInfo(instance_type)) {
#ifdef USE_SIMULATOR
    call_handler_infos_.push_back(Handle<CallHandlerInfo>::cast(obj));
#endif
  } else if (InstanceTypeChecker::IsExternalString(instance_type)) {
    Handle<ExternalString> string = Handle<ExternalString>::cast(obj);
    uint32_t index = string->GetResourceRefForDeserialization();
    Address address =
        static_cast<Address>(isolate()->api_external_references()[index]);
    string->AllocateExternalPointerEntries(isolate());
    string->set_address_as_resource(isolate(), address);
    isolate()->heap()->UpdateExternalString(*string, 0,
                                            string->ExternalPayloadSize());
    isolate()->heap()->RegisterExternalString(*string);
  } else if (InstanceTypeChecker::IsJSDataView(instance_type)) {
    Handle<JSDataView> data_view = Handle<JSDataView>::cast(obj);
    JSArrayBuffer buffer = JSArrayBuffer::cast(data_view->buffer());
    void* backing_store = nullptr;
    uint32_t store_index = buffer.GetBackingStoreRefForDeserialization();
    if (store_index != kNullRefSentinel) {
      // The backing store of the JSArrayBuffer has not been correctly restored
      // yet, as that may trigger GC. The backing_store field currently contains
      // a numbered reference to an already deserialized backing store.
      backing_store = backing_stores_[store_index]->buffer_start();
    }
    data_view->AllocateExternalPointerEntries(isolate());
    data_view->set_data_pointer(
        isolate(),
        reinterpret_cast<uint8_t*>(backing_store) + data_view->byte_offset());
  } else if (InstanceTypeChecker::IsJSTypedArray(instance_type)) {
    Handle<JSTypedArray> typed_array = Handle<JSTypedArray>::cast(obj);
    // Fixup typed array pointers.
    if (typed_array->is_on_heap()) {
      Address raw_external_pointer = typed_array->external_pointer_raw();
      typed_array->AllocateExternalPointerEntries(isolate());
      typed_array->SetOnHeapDataPtr(
          isolate(), HeapObject::cast(typed_array->base_pointer()),
          raw_external_pointer);
    } else {
      // Serializer writes backing store ref as a DataPtr() value.
      uint32_t store_index =
          typed_array->GetExternalBackingStoreRefForDeserialization();
      auto backing_store = backing_stores_[store_index];
      auto start = backing_store
                       ? reinterpret_cast<byte*>(backing_store->buffer_start())
                       : nullptr;
      typed_array->AllocateExternalPointerEntries(isolate());
      typed_array->SetOffHeapDataPtr(isolate(), start,
                                     typed_array->byte_offset());
    }
  } else if (InstanceTypeChecker::IsJSArrayBuffer(instance_type)) {
    Handle<JSArrayBuffer> buffer = Handle<JSArrayBuffer>::cast(obj);
    // Postpone allocation of backing store to avoid triggering the GC.
    if (buffer->GetBackingStoreRefForDeserialization() != kNullRefSentinel) {
      new_off_heap_array_buffers_.push_back(buffer);
    } else {
      buffer->AllocateExternalPointerEntries(isolate());
      buffer->set_backing_store(isolate(), nullptr);
    }
  } else if (InstanceTypeChecker::IsBytecodeArray(instance_type)) {
    // TODO(mythria): Remove these once we store the default values for these
    // fields in the serializer.
    Handle<BytecodeArray> bytecode_array = Handle<BytecodeArray>::cast(obj);
    bytecode_array->set_osr_loop_nesting_level(0);
  } else if (InstanceTypeChecker::IsDescriptorArray(instance_type)) {
    DCHECK(InstanceTypeChecker::IsStrongDescriptorArray(instance_type));
    Handle<DescriptorArray> descriptors = Handle<DescriptorArray>::cast(obj);
    new_descriptor_arrays_.push_back(descriptors);
  }

  // Check alignment.
  DCHECK_EQ(0, Heap::GetFillToAlign(obj->address(),
                                    HeapObject::RequiredAlignment(*map)));
}

HeapObjectReferenceType Deserializer::GetAndResetNextReferenceType() {
  HeapObjectReferenceType type = next_reference_is_weak_
                                     ? HeapObjectReferenceType::WEAK
                                     : HeapObjectReferenceType::STRONG;
  next_reference_is_weak_ = false;
  return type;
}

Handle<HeapObject> Deserializer::GetBackReferencedObject() {
  Handle<HeapObject> obj = back_refs_[source_.GetInt()];

  // We don't allow ThinStrings in backreferences -- if internalization produces
  // a thin string, then it should also update the backref handle.
  DCHECK(!obj->IsThinString());

  hot_objects_.Add(obj);
  DCHECK(!HasWeakHeapObjectTag(*obj));
  return obj;
}

Handle<HeapObject> Deserializer::ReadObject() {
  Handle<HeapObject> ret;
  CHECK_EQ(ReadSingleBytecodeData(source_.Get(),
                                  SlotAccessorForHandle(&ret, isolate())),
           1);
  return ret;
}

Handle<HeapObject> Deserializer::ReadObject(SnapshotSpace space) {
  const int size_in_tagged = source_.GetInt();
  const int size_in_bytes = size_in_tagged * kTaggedSize;

  // The map can't be a forward ref. If you want the map to be a forward ref,
  // then you're probably serializing the meta-map, in which case you want to
  // use the kNewMetaMap bytecode.
  DCHECK_NE(source()->Peek(), kRegisterPendingForwardRef);
  Handle<Map> map = Handle<Map>::cast(ReadObject());

  // Filling an object's fields can cause GCs and heap walks, so this object has
  // to be in a 'sufficiently initialised' state by the time the next allocation
  // can happen. For this to be the case, the object is carefully deserialized
  // as follows:
  //   * The space for the object is allocated.
  //   * The map is set on the object so that the GC knows what type the object
  //     has.
  //   * The rest of the object is filled with a fixed Smi value
  //     - This is a Smi so that tagged fields become initialized to a valid
  //       tagged value.
  //     - It's a fixed value, "uninitialized_field_value", so that we can
  //       DCHECK for it when reading objects that are assumed to be partially
  //       initialized objects.
  //   * The fields of the object are deserialized in order, under the
  //     assumption that objects are laid out in such a way that any fields
  //     required for object iteration (e.g. length fields) are deserialized
  //     before fields with objects.
  //     - We ensure this is the case by DCHECKing on object allocation that the
  //       previously allocated object has a valid size (see `Allocate`).
  HeapObject raw_obj =
      Allocate(space, size_in_bytes, HeapObject::RequiredAlignment(*map));
  raw_obj.set_map_after_allocation(*map);
  MemsetTagged(raw_obj.RawField(kTaggedSize), uninitialized_field_value(),
               size_in_tagged - 1);

  // Make sure BytecodeArrays have a valid age, so that the marker doesn't
  // break when making them older.
  if (raw_obj.IsBytecodeArray(isolate())) {
    BytecodeArray::cast(raw_obj).set_bytecode_age(
        BytecodeArray::kFirstBytecodeAge);
  }

#ifdef DEBUG
  // We want to make sure that all embedder pointers are initialized to null.
  if (raw_obj.IsJSObject() && JSObject::cast(raw_obj).IsApiWrapper()) {
    JSObject js_obj = JSObject::cast(raw_obj);
    for (int i = 0; i < js_obj.GetEmbedderFieldCount(); ++i) {
      void* pointer;
      CHECK(EmbedderDataSlot(js_obj, i).ToAlignedPointerSafe(isolate(),
                                                             &pointer));
      CHECK_NULL(pointer);
    }
  } else if (raw_obj.IsEmbedderDataArray()) {
    EmbedderDataArray array = EmbedderDataArray::cast(raw_obj);
    EmbedderDataSlot start(array, 0);
    EmbedderDataSlot end(array, array.length());
    for (EmbedderDataSlot slot = start; slot < end; ++slot) {
      void* pointer;
      CHECK(slot.ToAlignedPointerSafe(isolate(), &pointer));
      CHECK_NULL(pointer);
    }
  }
#endif

  Handle<HeapObject> obj = handle(raw_obj, isolate());
  back_refs_.push_back(obj);

  ReadData(obj, 1, size_in_tagged);
  PostProcessNewObject(map, obj, space);

  DCHECK(!obj->IsThinString(isolate()));

#ifdef DEBUG
  if (obj->IsCode()) {
    DCHECK(space == SnapshotSpace::kCode ||
           space == SnapshotSpace::kReadOnlyHeap);
  } else {
    DCHECK_NE(space, SnapshotSpace::kCode);
  }
#endif  // DEBUG

  return obj;
}

Handle<HeapObject> Deserializer::ReadMetaMap() {
  const SnapshotSpace space = SnapshotSpace::kReadOnlyHeap;
  const int size_in_bytes = Map::kSize;
  const int size_in_tagged = size_in_bytes / kTaggedSize;

  HeapObject raw_obj = Allocate(space, size_in_bytes, kWordAligned);
  raw_obj.set_map_after_allocation(Map::unchecked_cast(raw_obj));
  MemsetTagged(raw_obj.RawField(kTaggedSize), uninitialized_field_value(),
               size_in_tagged - 1);

  Handle<HeapObject> obj = handle(raw_obj, isolate());
  back_refs_.push_back(obj);

  // Set the instance-type manually, to allow backrefs to read it.
  Map::unchecked_cast(*obj).set_instance_type(MAP_TYPE);

  ReadData(obj, 1, size_in_tagged);
  PostProcessNewObject(Handle<Map>::cast(obj), obj, space);

  return obj;
}

class Deserializer::RelocInfoVisitor {
 public:
  RelocInfoVisitor(Deserializer* deserializer,
                   const std::vector<Handle<HeapObject>>* objects)
      : deserializer_(deserializer), objects_(objects), current_object_(0) {}
  ~RelocInfoVisitor() { DCHECK_EQ(current_object_, objects_->size()); }

  void VisitCodeTarget(Code host, RelocInfo* rinfo);
  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo);
  void VisitRuntimeEntry(Code host, RelocInfo* rinfo);
  void VisitExternalReference(Code host, RelocInfo* rinfo);
  void VisitInternalReference(Code host, RelocInfo* rinfo);
  void VisitOffHeapTarget(Code host, RelocInfo* rinfo);

 private:
  Isolate* isolate() { return deserializer_->isolate(); }
  SnapshotByteSource& source() { return deserializer_->source_; }

  Deserializer* deserializer_;
  const std::vector<Handle<HeapObject>>* objects_;
  int current_object_;
};

void Deserializer::RelocInfoVisitor::VisitCodeTarget(Code host,
                                                     RelocInfo* rinfo) {
  HeapObject object = *objects_->at(current_object_++);
  rinfo->set_target_address(Code::cast(object).raw_instruction_start());
}

void Deserializer::RelocInfoVisitor::VisitEmbeddedPointer(Code host,
                                                          RelocInfo* rinfo) {
  HeapObject object = *objects_->at(current_object_++);
  // Embedded object reference must be a strong one.
  rinfo->set_target_object(isolate()->heap(), object);
}

void Deserializer::RelocInfoVisitor::VisitRuntimeEntry(Code host,
                                                       RelocInfo* rinfo) {
  // We no longer serialize code that contains runtime entries.
  UNREACHABLE();
}

void Deserializer::RelocInfoVisitor::VisitExternalReference(Code host,
                                                            RelocInfo* rinfo) {
  byte data = source().Get();
  CHECK_EQ(data, kExternalReference);

  Address address = deserializer_->ReadExternalReferenceCase();

  if (rinfo->IsCodedSpecially()) {
    Address location_of_branch_data = rinfo->pc();
    Assembler::deserialization_set_special_target_at(location_of_branch_data,
                                                     host, address);
  } else {
    WriteUnalignedValue(rinfo->target_address_address(), address);
  }
}

void Deserializer::RelocInfoVisitor::VisitInternalReference(Code host,
                                                            RelocInfo* rinfo) {
  byte data = source().Get();
  CHECK_EQ(data, kInternalReference);

  // Internal reference target is encoded as an offset from code entry.
  int target_offset = source().GetInt();
  // TODO(jgruber,v8:11036): We are being permissive for this DCHECK, but
  // consider using raw_instruction_size() instead of raw_body_size() in the
  // future.
  STATIC_ASSERT(Code::kOnHeapBodyIsContiguous);
  DCHECK_LT(static_cast<unsigned>(target_offset),
            static_cast<unsigned>(host.raw_body_size()));
  Address target = host.entry() + target_offset;
  Assembler::deserialization_set_target_internal_reference_at(
      rinfo->pc(), target, rinfo->rmode());
}

void Deserializer::RelocInfoVisitor::VisitOffHeapTarget(Code host,
                                                        RelocInfo* rinfo) {
  byte data = source().Get();
  CHECK_EQ(data, kOffHeapTarget);

  int builtin_index = source().GetInt();
  DCHECK(Builtins::IsBuiltinId(builtin_index));

  CHECK_NOT_NULL(isolate()->embedded_blob_code());
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

template <typename SlotAccessor>
int Deserializer::ReadRepeatedObject(SlotAccessor slot_accessor,
                                     int repeat_count) {
  CHECK_LE(2, repeat_count);

  Handle<HeapObject> heap_object = ReadObject();
  DCHECK(!Heap::InYoungGeneration(*heap_object));
  for (int i = 0; i < repeat_count; i++) {
    // TODO(leszeks): Use a ranged barrier here.
    slot_accessor.Write(heap_object, HeapObjectReferenceType::STRONG, i);
  }
  return repeat_count;
}

namespace {

void NoExternalReferencesCallback() {
  // The following check will trigger if a function or object template
  // with references to native functions have been deserialized from
  // snapshot, but no actual external references were provided when the
  // isolate was created.
  FATAL("No external references provided via API");
}

// Template used by the below CASE_RANGE macro to statically verify that the
// given number of cases matches the number of expected cases for that bytecode.
template <int byte_code_count, int expected>
constexpr byte VerifyBytecodeCount(byte bytecode) {
  STATIC_ASSERT(byte_code_count == expected);
  return bytecode;
}

}  // namespace

// Helper macro (and its implementation detail) for specifying a range of cases.
// Use as "case CASE_RANGE(byte_code, num_bytecodes):"
#define CASE_RANGE(byte_code, num_bytecodes) \
  CASE_R##num_bytecodes(                     \
      (VerifyBytecodeCount<byte_code##Count, num_bytecodes>(byte_code)))
#define CASE_R1(byte_code) byte_code
#define CASE_R2(byte_code) CASE_R1(byte_code) : case CASE_R1(byte_code + 1)
#define CASE_R3(byte_code) CASE_R2(byte_code) : case CASE_R1(byte_code + 2)
#define CASE_R4(byte_code) CASE_R2(byte_code) : case CASE_R2(byte_code + 2)
#define CASE_R8(byte_code) CASE_R4(byte_code) : case CASE_R4(byte_code + 4)
#define CASE_R16(byte_code) CASE_R8(byte_code) : case CASE_R8(byte_code + 8)
#define CASE_R32(byte_code) CASE_R16(byte_code) : case CASE_R16(byte_code + 16)

// This generates a case range for all the spaces.
#define CASE_RANGE_ALL_SPACES(bytecode)                           \
  SpaceEncoder<bytecode>::Encode(SnapshotSpace::kOld)             \
      : case SpaceEncoder<bytecode>::Encode(SnapshotSpace::kCode) \
      : case SpaceEncoder<bytecode>::Encode(SnapshotSpace::kMap)  \
      : case SpaceEncoder<bytecode>::Encode(SnapshotSpace::kReadOnlyHeap)

void Deserializer::ReadData(Handle<HeapObject> object, int start_slot_index,
                            int end_slot_index) {
  int current = start_slot_index;
  while (current < end_slot_index) {
    byte data = source_.Get();
    current += ReadSingleBytecodeData(
        data, SlotAccessorForHeapObject::ForSlotIndex(object, current));
  }
  CHECK_EQ(current, end_slot_index);
}

void Deserializer::ReadData(FullMaybeObjectSlot start,
                            FullMaybeObjectSlot end) {
  FullMaybeObjectSlot current = start;
  while (current < end) {
    byte data = source_.Get();
    current += ReadSingleBytecodeData(data, SlotAccessorForRootSlots(current));
  }
  CHECK_EQ(current, end);
}

template <typename SlotAccessor>
int Deserializer::ReadSingleBytecodeData(byte data,
                                         SlotAccessor slot_accessor) {
  using TSlot = decltype(slot_accessor.slot());

  switch (data) {
    // Deserialize a new object and write a pointer to it to the current
    // object.
    case CASE_RANGE_ALL_SPACES(kNewObject): {
      SnapshotSpace space = NewObject::Decode(data);
      // Save the reference type before recursing down into reading the object.
      HeapObjectReferenceType ref_type = GetAndResetNextReferenceType();
      Handle<HeapObject> heap_object = ReadObject(space);
      return slot_accessor.Write(heap_object, ref_type);
    }

    // Find a recently deserialized object using its offset from the current
    // allocation point and write a pointer to it to the current object.
    case kBackref: {
      Handle<HeapObject> heap_object = GetBackReferencedObject();
      return slot_accessor.Write(heap_object, GetAndResetNextReferenceType());
    }

    // Reference an object in the read-only heap. This should be used when an
    // object is read-only, but is not a root.
    case kReadOnlyHeapRef: {
      DCHECK(isolate()->heap()->deserialization_complete());
      uint32_t chunk_index = source_.GetInt();
      uint32_t chunk_offset = source_.GetInt();

      ReadOnlySpace* read_only_space = isolate()->heap()->read_only_space();
      ReadOnlyPage* page = read_only_space->pages()[chunk_index];
      Address address = page->OffsetToAddress(chunk_offset);
      HeapObject heap_object = HeapObject::FromAddress(address);

      return slot_accessor.Write(heap_object, GetAndResetNextReferenceType());
    }

    // Find an object in the roots array and write a pointer to it to the
    // current object.
    case kRootArray: {
      int id = source_.GetInt();
      RootIndex root_index = static_cast<RootIndex>(id);
      Handle<HeapObject> heap_object =
          Handle<HeapObject>::cast(isolate()->root_handle(root_index));
      hot_objects_.Add(heap_object);
      return slot_accessor.Write(heap_object, GetAndResetNextReferenceType());
    }

    // Find an object in the startup object cache and write a pointer to it to
    // the current object.
    case kStartupObjectCache: {
      int cache_index = source_.GetInt();
      // TODO(leszeks): Could we use the address of the startup_object_cache
      // entry as a Handle backing?
      HeapObject heap_object =
          HeapObject::cast(isolate()->startup_object_cache()->at(cache_index));
      return slot_accessor.Write(heap_object, GetAndResetNextReferenceType());
    }

    // Find an object in the read-only object cache and write a pointer to it
    // to the current object.
    case kReadOnlyObjectCache: {
      int cache_index = source_.GetInt();
      // TODO(leszeks): Could we use the address of the cached_read_only_object
      // entry as a Handle backing?
      HeapObject heap_object = HeapObject::cast(
          isolate()->read_only_heap()->cached_read_only_object(cache_index));
      return slot_accessor.Write(heap_object, GetAndResetNextReferenceType());
    }

    // Deserialize a new meta-map and write a pointer to it to the current
    // object.
    case kNewMetaMap: {
      Handle<HeapObject> heap_object = ReadMetaMap();
      return slot_accessor.Write(heap_object, HeapObjectReferenceType::STRONG);
    }

    // Find an external reference and write a pointer to it to the current
    // object.
    case kSandboxedExternalReference:
    case kExternalReference: {
      Address address = ReadExternalReferenceCase();
      if (V8_HEAP_SANDBOX_BOOL && data == kSandboxedExternalReference) {
        return WriteExternalPointer(slot_accessor.slot(), address,
                                    kForeignForeignAddressTag);
      } else {
        DCHECK(!V8_HEAP_SANDBOX_BOOL);
        return WriteAddress(slot_accessor.slot(), address);
      }
    }

    case kInternalReference:
    case kOffHeapTarget:
      // These bytecodes are expected only during RelocInfo iteration.
      UNREACHABLE();

    // Find an object in the attached references and write a pointer to it to
    // the current object.
    case kAttachedReference: {
      int index = source_.GetInt();
      Handle<HeapObject> heap_object = attached_objects_[index];

      // This is the only case where we might encounter new space objects, so
      // maybe emit a generational write barrier.
      return slot_accessor.WriteWithGenerationalBarrier(
          heap_object, GetAndResetNextReferenceType());
    }

    case kNop:
      return 0;

    case kRegisterPendingForwardRef: {
      HeapObjectReferenceType ref_type = GetAndResetNextReferenceType();
      unresolved_forward_refs_.emplace_back(slot_accessor.object(),
                                            slot_accessor.offset(), ref_type);
      num_unresolved_forward_refs_++;
      return 1;
    }

    case kResolvePendingForwardRef: {
      // Pending forward refs can only be resolved after the heap object's map
      // field is deserialized; currently they only appear immediately after
      // the map field.
      DCHECK_EQ(slot_accessor.offset(), HeapObject::kHeaderSize);
      Handle<HeapObject> obj = slot_accessor.object();
      int index = source_.GetInt();
      auto& forward_ref = unresolved_forward_refs_[index];
      SlotAccessorForHeapObject::ForSlotOffset(forward_ref.object,
                                               forward_ref.offset)
          .Write(*obj, forward_ref.ref_type);
      num_unresolved_forward_refs_--;
      if (num_unresolved_forward_refs_ == 0) {
        // If there's no more pending fields, clear the entire pending field
        // vector.
        unresolved_forward_refs_.clear();
      } else {
        // Otherwise, at least clear the pending field.
        forward_ref.object = Handle<HeapObject>();
      }
      return 0;
    }

    case kSynchronize:
      // If we get here then that indicates that you have a mismatch between
      // the number of GC roots when serializing and deserializing.
      UNREACHABLE();

    // Deserialize raw data of variable length.
    case kVariableRawData: {
      // This operation is only supported for tagged-size slots, else we might
      // become misaligned.
      DCHECK_EQ(TSlot::kSlotDataSize, kTaggedSize);
      int size_in_tagged = source_.GetInt();
      // TODO(leszeks): Only copy slots when there are Smis in the serialized
      // data.
      source_.CopySlots(slot_accessor.slot().location(), size_in_tagged);
      return size_in_tagged;
    }

    // Deserialize raw code directly into the body of the code object.
    case kCodeBody: {
      // This operation is only supported for tagged-size slots, else we might
      // become misaligned.
      DCHECK_EQ(TSlot::kSlotDataSize, kTaggedSize);
      // CodeBody can only occur right after the heap object header.
      DCHECK_EQ(slot_accessor.offset(), HeapObject::kHeaderSize);

      int size_in_tagged = source_.GetInt();
      int size_in_bytes = size_in_tagged * kTaggedSize;

      {
        DisallowGarbageCollection no_gc;
        Code code = Code::cast(*slot_accessor.object());

        // First deserialize the code itself.
        source_.CopyRaw(
            reinterpret_cast<void*>(code.address() + Code::kDataStart),
            size_in_bytes);
      }

      // Then deserialize the code header
      ReadData(slot_accessor.object(), HeapObject::kHeaderSize / kTaggedSize,
               Code::kDataStart / kTaggedSize);

      // Then deserialize the pre-serialized RelocInfo objects.
      std::vector<Handle<HeapObject>> preserialized_objects;
      while (source_.Peek() != kSynchronize) {
        Handle<HeapObject> obj = ReadObject();
        preserialized_objects.push_back(obj);
      }
      // Skip the synchronize bytecode.
      source_.Advance(1);

      // Finally iterate RelocInfos (the same way it was done by the serializer)
      // and deserialize respective data into RelocInfos. The RelocIterator
      // holds a raw pointer to the code, so we have to disable garbage
      // collection here. It's ok though, any objects it would have needed are
      // in the preserialized_objects vector.
      {
        DisallowGarbageCollection no_gc;

        Code code = Code::cast(*slot_accessor.object());
        RelocInfoVisitor visitor(this, &preserialized_objects);
        for (RelocIterator it(code, Code::BodyDescriptor::kRelocModeMask);
             !it.done(); it.next()) {
          it.rinfo()->Visit(&visitor);
        }
      }

      // Advance to the end of the code object.
      return (Code::kDataStart - HeapObject::kHeaderSize) / kTaggedSize +
             size_in_tagged;
    }

    case kVariableRepeat: {
      int repeats = VariableRepeatCount::Decode(source_.GetInt());
      return ReadRepeatedObject(slot_accessor, repeats);
    }

    case kOffHeapBackingStore: {
      AlwaysAllocateScope scope(isolate()->heap());
      int byte_length = source_.GetInt();
      std::unique_ptr<BackingStore> backing_store =
          BackingStore::Allocate(isolate(), byte_length, SharedFlag::kNotShared,
                                 InitializedFlag::kUninitialized);
      CHECK_NOT_NULL(backing_store);
      source_.CopyRaw(backing_store->buffer_start(), byte_length);
      backing_stores_.push_back(std::move(backing_store));
      return 0;
    }

    case kSandboxedApiReference:
    case kApiReference: {
      uint32_t reference_id = static_cast<uint32_t>(source_.GetInt());
      Address address;
      if (isolate()->api_external_references()) {
        DCHECK_WITH_MSG(reference_id < num_api_references_,
                        "too few external references provided through the API");
        address = static_cast<Address>(
            isolate()->api_external_references()[reference_id]);
      } else {
        address = reinterpret_cast<Address>(NoExternalReferencesCallback);
      }
      if (V8_HEAP_SANDBOX_BOOL && data == kSandboxedApiReference) {
        return WriteExternalPointer(slot_accessor.slot(), address,
                                    kForeignForeignAddressTag);
      } else {
        DCHECK(!V8_HEAP_SANDBOX_BOOL);
        return WriteAddress(slot_accessor.slot(), address);
      }
    }

    case kClearedWeakReference:
      return slot_accessor.Write(HeapObjectReference::ClearedValue(isolate()));

    case kWeakPrefix: {
      // We shouldn't have two weak prefixes in a row.
      DCHECK(!next_reference_is_weak_);
      // We shouldn't have weak refs without a current object.
      DCHECK_NE(slot_accessor.object()->address(), kNullAddress);
      next_reference_is_weak_ = true;
      return 0;
    }

    case CASE_RANGE(kRootArrayConstants, 32): {
      // First kRootArrayConstantsCount roots are guaranteed to be in
      // the old space.
      STATIC_ASSERT(static_cast<int>(RootIndex::kFirstImmortalImmovableRoot) ==
                    0);
      STATIC_ASSERT(kRootArrayConstantsCount <=
                    static_cast<int>(RootIndex::kLastImmortalImmovableRoot));

      RootIndex root_index = RootArrayConstant::Decode(data);
      Handle<HeapObject> heap_object =
          Handle<HeapObject>::cast(isolate()->root_handle(root_index));
      return slot_accessor.Write(heap_object, HeapObjectReferenceType::STRONG);
    }

    case CASE_RANGE(kHotObject, 8): {
      int index = HotObject::Decode(data);
      Handle<HeapObject> hot_object = hot_objects_.Get(index);
      return slot_accessor.Write(hot_object, GetAndResetNextReferenceType());
    }

    case CASE_RANGE(kFixedRawData, 32): {
      // Deserialize raw data of fixed length from 1 to 32 times kTaggedSize.
      int size_in_tagged = FixedRawDataWithSize::Decode(data);
      STATIC_ASSERT(TSlot::kSlotDataSize == kTaggedSize ||
                    TSlot::kSlotDataSize == 2 * kTaggedSize);
      int size_in_slots = size_in_tagged / (TSlot::kSlotDataSize / kTaggedSize);
      // kFixedRawData can have kTaggedSize != TSlot::kSlotDataSize when
      // serializing Smi roots in pointer-compressed builds. In this case, the
      // size in bytes is unconditionally the (full) slot size.
      DCHECK_IMPLIES(kTaggedSize != TSlot::kSlotDataSize, size_in_slots == 1);
      // TODO(leszeks): Only copy slots when there are Smis in the serialized
      // data.
      source_.CopySlots(slot_accessor.slot().location(), size_in_slots);
      return size_in_slots;
    }

    case CASE_RANGE(kFixedRepeat, 16): {
      int repeats = FixedRepeatWithCount::Decode(data);
      return ReadRepeatedObject(slot_accessor, repeats);
    }

#ifdef DEBUG
#define UNUSED_CASE(byte_code) \
  case byte_code:              \
    UNREACHABLE();
      UNUSED_SERIALIZER_BYTE_CODES(UNUSED_CASE)
#endif
#undef UNUSED_CASE
  }

  // The above switch, including UNUSED_SERIALIZER_BYTE_CODES, covers all
  // possible bytecodes; but, clang doesn't realize this, so we have an explicit
  // UNREACHABLE here too.
  UNREACHABLE();
}

#undef CASE_RANGE_ALL_SPACES
#undef CASE_RANGE
#undef CASE_R32
#undef CASE_R16
#undef CASE_R8
#undef CASE_R4
#undef CASE_R3
#undef CASE_R2
#undef CASE_R1

Address Deserializer::ReadExternalReferenceCase() {
  uint32_t reference_id = static_cast<uint32_t>(source_.GetInt());
  return isolate()->external_reference_table()->address(reference_id);
}

namespace {
AllocationType SpaceToType(SnapshotSpace space) {
  switch (space) {
    case SnapshotSpace::kCode:
      return AllocationType::kCode;
    case SnapshotSpace::kMap:
      return AllocationType::kMap;
    case SnapshotSpace::kOld:
      return AllocationType::kOld;
    case SnapshotSpace::kReadOnlyHeap:
      return AllocationType::kReadOnly;
  }
}
}  // namespace

HeapObject Deserializer::Allocate(SnapshotSpace space, int size,
                                  AllocationAlignment alignment) {
#ifdef DEBUG
  if (!previous_allocation_obj_.is_null()) {
    // Make sure that the previous object is initialized sufficiently to
    // be iterated over by the GC.
    int object_size = previous_allocation_obj_->Size();
    DCHECK_LE(object_size, previous_allocation_size_);
  }
#endif

  HeapObject obj = isolate()->heap()->AllocateRawWith<Heap::kRetryOrFail>(
      size, SpaceToType(space), AllocationOrigin::kRuntime, alignment);

#ifdef DEBUG
  previous_allocation_obj_ = handle(obj, isolate());
  previous_allocation_size_ = size;
#endif

  return obj;
}

}  // namespace internal
}  // namespace v8
