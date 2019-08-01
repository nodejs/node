// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/serializer.h"

#include "src/codegen/assembler-inl.h"
#include "src/heap/heap-inl.h"  // For Space::identity().
#include "src/heap/read-only-heap.h"
#include "src/interpreter/interpreter.h"
#include "src/objects/code.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/map.h"
#include "src/objects/slots-inl.h"
#include "src/objects/smi.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

Serializer::Serializer(Isolate* isolate)
    : isolate_(isolate),
      external_reference_encoder_(isolate),
      root_index_map_(isolate),
      allocator_(this) {
#ifdef OBJECT_PRINT
  if (FLAG_serialization_statistics) {
    for (int space = 0; space < LAST_SPACE; ++space) {
      instance_type_count_[space] = NewArray<int>(kInstanceTypes);
      instance_type_size_[space] = NewArray<size_t>(kInstanceTypes);
      for (int i = 0; i < kInstanceTypes; i++) {
        instance_type_count_[space][i] = 0;
        instance_type_size_[space][i] = 0;
      }
    }
  } else {
    for (int space = 0; space < LAST_SPACE; ++space) {
      instance_type_count_[space] = nullptr;
      instance_type_size_[space] = nullptr;
    }
  }
#endif  // OBJECT_PRINT
}

Serializer::~Serializer() {
  if (code_address_map_ != nullptr) delete code_address_map_;
#ifdef OBJECT_PRINT
  for (int space = 0; space < LAST_SPACE; ++space) {
    if (instance_type_count_[space] != nullptr) {
      DeleteArray(instance_type_count_[space]);
      DeleteArray(instance_type_size_[space]);
    }
  }
#endif  // OBJECT_PRINT
}

#ifdef OBJECT_PRINT
void Serializer::CountInstanceType(Map map, int size, AllocationSpace space) {
  int instance_type = map.instance_type();
  instance_type_count_[space][instance_type]++;
  instance_type_size_[space][instance_type] += size;
}
#endif  // OBJECT_PRINT

void Serializer::OutputStatistics(const char* name) {
  if (!FLAG_serialization_statistics) return;

  PrintF("%s:\n", name);
  allocator()->OutputStatistics();

#ifdef OBJECT_PRINT
  PrintF("  Instance types (count and bytes):\n");
#define PRINT_INSTANCE_TYPE(Name)                                             \
  for (int space = 0; space < LAST_SPACE; ++space) {                          \
    if (instance_type_count_[space][Name]) {                                  \
      PrintF("%10d %10zu  %-10s %s\n", instance_type_count_[space][Name],     \
             instance_type_size_[space][Name],                                \
             Heap::GetSpaceName(static_cast<AllocationSpace>(space)), #Name); \
    }                                                                         \
  }
  INSTANCE_TYPE_LIST(PRINT_INSTANCE_TYPE)
#undef PRINT_INSTANCE_TYPE

  PrintF("\n");
#endif  // OBJECT_PRINT
}

void Serializer::SerializeDeferredObjects() {
  while (!deferred_objects_.empty()) {
    HeapObject obj = deferred_objects_.back();
    deferred_objects_.pop_back();
    ObjectSerializer obj_serializer(this, obj, &sink_);
    obj_serializer.SerializeDeferred();
  }
  sink_.Put(kSynchronize, "Finished with deferred objects");
}

bool Serializer::MustBeDeferred(HeapObject object) { return false; }

void Serializer::VisitRootPointers(Root root, const char* description,
                                   FullObjectSlot start, FullObjectSlot end) {
  for (FullObjectSlot current = start; current < end; ++current) {
    SerializeRootObject(*current);
  }
}

void Serializer::SerializeRootObject(Object object) {
  if (object.IsSmi()) {
    PutSmi(Smi::cast(object));
  } else {
    SerializeObject(HeapObject::cast(object));
  }
}

#ifdef DEBUG
void Serializer::PrintStack() {
  for (const auto o : stack_) {
    o.Print();
    PrintF("\n");
  }
}
#endif  // DEBUG

bool Serializer::SerializeRoot(HeapObject obj) {
  RootIndex root_index;
  // Derived serializers are responsible for determining if the root has
  // actually been serialized before calling this.
  if (root_index_map()->Lookup(obj, &root_index)) {
    PutRoot(root_index, obj);
    return true;
  }
  return false;
}

bool Serializer::SerializeHotObject(HeapObject obj) {
  // Encode a reference to a hot object by its index in the working set.
  int index = hot_objects_.Find(obj);
  if (index == HotObjectsList::kNotFound) return false;
  DCHECK(index >= 0 && index < kNumberOfHotObjects);
  if (FLAG_trace_serializer) {
    PrintF(" Encoding hot object %d:", index);
    obj.ShortPrint();
    PrintF("\n");
  }
  sink_.Put(kHotObject + index, "HotObject");
  return true;
}

bool Serializer::SerializeBackReference(HeapObject obj) {
  SerializerReference reference =
      reference_map_.LookupReference(reinterpret_cast<void*>(obj.ptr()));
  if (!reference.is_valid()) return false;
  // Encode the location of an already deserialized object in order to write
  // its location into a later object.  We can encode the location as an
  // offset fromthe start of the deserialized objects or as an offset
  // backwards from thecurrent allocation pointer.
  if (reference.is_attached_reference()) {
    if (FLAG_trace_serializer) {
      PrintF(" Encoding attached reference %d\n",
             reference.attached_reference_index());
    }
    PutAttachedReference(reference);
  } else {
    DCHECK(reference.is_back_reference());
    if (FLAG_trace_serializer) {
      PrintF(" Encoding back reference to: ");
      obj.ShortPrint();
      PrintF("\n");
    }

    PutAlignmentPrefix(obj);
    AllocationSpace space = reference.space();
    sink_.Put(kBackref + space, "BackRef");
    PutBackReference(obj, reference);
  }
  return true;
}

bool Serializer::ObjectIsBytecodeHandler(HeapObject obj) const {
  if (!obj.IsCode()) return false;
  return (Code::cast(obj).kind() == Code::BYTECODE_HANDLER);
}

void Serializer::PutRoot(RootIndex root, HeapObject object) {
  int root_index = static_cast<int>(root);
  if (FLAG_trace_serializer) {
    PrintF(" Encoding root %d:", root_index);
    object.ShortPrint();
    PrintF("\n");
  }

  // Assert that the first 32 root array items are a conscious choice. They are
  // chosen so that the most common ones can be encoded more efficiently.
  STATIC_ASSERT(static_cast<int>(RootIndex::kArgumentsMarker) ==
                kNumberOfRootArrayConstants - 1);

  // TODO(ulan): Check that it works with young large objects.
  if (root_index < kNumberOfRootArrayConstants &&
      !Heap::InYoungGeneration(object)) {
    sink_.Put(kRootArrayConstants + root_index, "RootConstant");
  } else {
    sink_.Put(kRootArray, "RootSerialization");
    sink_.PutInt(root_index, "root_index");
    hot_objects_.Add(object);
  }
}

void Serializer::PutSmi(Smi smi) {
  sink_.Put(kOnePointerRawData, "Smi");
  Tagged_t raw_value = static_cast<Tagged_t>(smi.ptr());
  byte bytes[kTaggedSize];
  memcpy(bytes, &raw_value, kTaggedSize);
  for (int i = 0; i < kTaggedSize; i++) sink_.Put(bytes[i], "Byte");
}

void Serializer::PutBackReference(HeapObject object,
                                  SerializerReference reference) {
  DCHECK(allocator()->BackReferenceIsAlreadyAllocated(reference));
  switch (reference.space()) {
    case MAP_SPACE:
      sink_.PutInt(reference.map_index(), "BackRefMapIndex");
      break;

    case LO_SPACE:
      sink_.PutInt(reference.large_object_index(), "BackRefLargeObjectIndex");
      break;

    default:
      sink_.PutInt(reference.chunk_index(), "BackRefChunkIndex");
      sink_.PutInt(reference.chunk_offset(), "BackRefChunkOffset");
      break;
  }

  hot_objects_.Add(object);
}

void Serializer::PutAttachedReference(SerializerReference reference) {
  DCHECK(reference.is_attached_reference());
  sink_.Put(kAttachedReference, "AttachedRef");
  sink_.PutInt(reference.attached_reference_index(), "AttachedRefIndex");
}

int Serializer::PutAlignmentPrefix(HeapObject object) {
  AllocationAlignment alignment = HeapObject::RequiredAlignment(object.map());
  if (alignment != kWordAligned) {
    DCHECK(1 <= alignment && alignment <= 3);
    byte prefix = (kAlignmentPrefix - 1) + alignment;
    sink_.Put(prefix, "Alignment");
    return Heap::GetMaximumFillToAlign(alignment);
  }
  return 0;
}

void Serializer::PutNextChunk(int space) {
  sink_.Put(kNextChunk, "NextChunk");
  sink_.Put(space, "NextChunkSpace");
}

void Serializer::PutRepeat(int repeat_count) {
  if (repeat_count <= kLastEncodableFixedRepeatCount) {
    sink_.Put(EncodeFixedRepeat(repeat_count), "FixedRepeat");
  } else {
    sink_.Put(kVariableRepeat, "VariableRepeat");
    sink_.PutInt(EncodeVariableRepeatCount(repeat_count), "repeat count");
  }
}

void Serializer::Pad(int padding_offset) {
  // The non-branching GetInt will read up to 3 bytes too far, so we need
  // to pad the snapshot to make sure we don't read over the end.
  for (unsigned i = 0; i < sizeof(int32_t) - 1; i++) {
    sink_.Put(kNop, "Padding");
  }
  // Pad up to pointer size for checksum.
  while (!IsAligned(sink_.Position() + padding_offset, kPointerAlignment)) {
    sink_.Put(kNop, "Padding");
  }
}

void Serializer::InitializeCodeAddressMap() {
  isolate_->InitializeLoggingAndCounters();
  code_address_map_ = new CodeAddressMap(isolate_);
}

Code Serializer::CopyCode(Code code) {
  code_buffer_.clear();  // Clear buffer without deleting backing store.
  int size = code.CodeSize();
  code_buffer_.insert(code_buffer_.end(),
                      reinterpret_cast<byte*>(code.address()),
                      reinterpret_cast<byte*>(code.address() + size));
  // When pointer compression is enabled the checked cast will try to
  // decompress map field of off-heap Code object.
  return Code::unchecked_cast(HeapObject::FromAddress(
      reinterpret_cast<Address>(&code_buffer_.front())));
}

void Serializer::ObjectSerializer::SerializePrologue(AllocationSpace space,
                                                     int size, Map map) {
  if (serializer_->code_address_map_) {
    const char* code_name =
        serializer_->code_address_map_->Lookup(object_.address());
    LOG(serializer_->isolate_,
        CodeNameEvent(object_.address(), sink_->Position(), code_name));
  }

  SerializerReference back_reference;
  if (space == LO_SPACE) {
    sink_->Put(kNewObject + space, "NewLargeObject");
    sink_->PutInt(size >> kObjectAlignmentBits, "ObjectSizeInWords");
    CHECK(!object_.IsCode());
    back_reference = serializer_->allocator()->AllocateLargeObject(size);
  } else if (space == MAP_SPACE) {
    DCHECK_EQ(Map::kSize, size);
    back_reference = serializer_->allocator()->AllocateMap();
    sink_->Put(kNewObject + space, "NewMap");
    // This is redundant, but we include it anyways.
    sink_->PutInt(size >> kObjectAlignmentBits, "ObjectSizeInWords");
  } else {
    int fill = serializer_->PutAlignmentPrefix(object_);
    back_reference = serializer_->allocator()->Allocate(space, size + fill);
    sink_->Put(kNewObject + space, "NewObject");
    sink_->PutInt(size >> kObjectAlignmentBits, "ObjectSizeInWords");
  }

#ifdef OBJECT_PRINT
  if (FLAG_serialization_statistics) {
    serializer_->CountInstanceType(map, size, space);
  }
#endif  // OBJECT_PRINT

  // Mark this object as already serialized.
  serializer_->reference_map()->Add(reinterpret_cast<void*>(object_.ptr()),
                                    back_reference);

  // Serialize the map (first word of the object).
  serializer_->SerializeObject(map);
}

int32_t Serializer::ObjectSerializer::SerializeBackingStore(
    void* backing_store, int32_t byte_length) {
  SerializerReference reference =
      serializer_->reference_map()->LookupReference(backing_store);

  // Serialize the off-heap backing store.
  if (!reference.is_valid()) {
    sink_->Put(kOffHeapBackingStore, "Off-heap backing store");
    sink_->PutInt(byte_length, "length");
    sink_->PutRaw(static_cast<byte*>(backing_store), byte_length,
                  "BackingStore");
    reference = serializer_->allocator()->AllocateOffHeapBackingStore();
    // Mark this backing store as already serialized.
    serializer_->reference_map()->Add(backing_store, reference);
  }

  return static_cast<int32_t>(reference.off_heap_backing_store_index());
}

void Serializer::ObjectSerializer::SerializeJSTypedArray() {
  JSTypedArray typed_array = JSTypedArray::cast(object_);
  if (!typed_array.WasDetached()) {
    if (!typed_array.is_on_heap()) {
      // Explicitly serialize the backing store now.
      JSArrayBuffer buffer = JSArrayBuffer::cast(typed_array.buffer());
      CHECK_LE(buffer.byte_length(), Smi::kMaxValue);
      CHECK_LE(typed_array.byte_offset(), Smi::kMaxValue);
      int32_t byte_length = static_cast<int32_t>(buffer.byte_length());
      int32_t byte_offset = static_cast<int32_t>(typed_array.byte_offset());

      // We need to calculate the backing store from the external pointer
      // because the ArrayBuffer may already have been serialized.
      void* backing_store = reinterpret_cast<void*>(
          reinterpret_cast<intptr_t>(typed_array.external_pointer()) -
          byte_offset);
      int32_t ref = SerializeBackingStore(backing_store, byte_length);

      // The external_pointer is the backing_store + typed_array->byte_offset.
      // To properly share the buffer, we set the backing store ref here. On
      // deserialization we re-add the byte_offset to external_pointer.
      typed_array.set_external_pointer(
          reinterpret_cast<void*>(Smi::FromInt(ref).ptr()));
    }
  } else {
    typed_array.set_external_pointer(nullptr);
  }
  SerializeObject();
}

void Serializer::ObjectSerializer::SerializeJSArrayBuffer() {
  JSArrayBuffer buffer = JSArrayBuffer::cast(object_);
  void* backing_store = buffer.backing_store();
  // We cannot store byte_length larger than Smi range in the snapshot.
  CHECK_LE(buffer.byte_length(), Smi::kMaxValue);
  int32_t byte_length = static_cast<int32_t>(buffer.byte_length());

  // The embedder-allocated backing store only exists for the off-heap case.
  if (backing_store != nullptr) {
    int32_t ref = SerializeBackingStore(backing_store, byte_length);
    buffer.set_backing_store(reinterpret_cast<void*>(Smi::FromInt(ref).ptr()));
  }
  SerializeObject();
  buffer.set_backing_store(backing_store);
}

void Serializer::ObjectSerializer::SerializeExternalString() {
  Heap* heap = serializer_->isolate()->heap();
  // For external strings with known resources, we replace the resource field
  // with the encoded external reference, which we restore upon deserialize.
  // for native native source code strings, we replace the resource field
  // with the native source id.
  // For the rest we serialize them to look like ordinary sequential strings.
  if (object_.map() != ReadOnlyRoots(heap).native_source_string_map()) {
    ExternalString string = ExternalString::cast(object_);
    Address resource = string.resource_as_address();
    ExternalReferenceEncoder::Value reference;
    if (serializer_->external_reference_encoder_.TryEncode(resource).To(
            &reference)) {
      DCHECK(reference.is_from_api());
      string.set_uint32_as_resource(reference.index());
      SerializeObject();
      string.set_address_as_resource(resource);
    } else {
      SerializeExternalStringAsSequentialString();
    }
  } else {
    ExternalOneByteString string = ExternalOneByteString::cast(object_);
    DCHECK(string.is_uncached());
    const NativesExternalStringResource* resource =
        reinterpret_cast<const NativesExternalStringResource*>(
            string.resource());
    // Replace the resource field with the type and index of the native source.
    string.set_resource(resource->EncodeForSerialization());
    SerializeObject();
    // Restore the resource field.
    string.set_resource(resource);
  }
}

void Serializer::ObjectSerializer::SerializeExternalStringAsSequentialString() {
  // Instead of serializing this as an external string, we serialize
  // an imaginary sequential string with the same content.
  ReadOnlyRoots roots(serializer_->isolate());
  DCHECK(object_.IsExternalString());
  DCHECK(object_.map() != roots.native_source_string_map());
  ExternalString string = ExternalString::cast(object_);
  int length = string.length();
  Map map;
  int content_size;
  int allocation_size;
  const byte* resource;
  // Find the map and size for the imaginary sequential string.
  bool internalized = object_.IsInternalizedString();
  if (object_.IsExternalOneByteString()) {
    map = internalized ? roots.one_byte_internalized_string_map()
                       : roots.one_byte_string_map();
    allocation_size = SeqOneByteString::SizeFor(length);
    content_size = length * kCharSize;
    resource = reinterpret_cast<const byte*>(
        ExternalOneByteString::cast(string).resource()->data());
  } else {
    map = internalized ? roots.internalized_string_map() : roots.string_map();
    allocation_size = SeqTwoByteString::SizeFor(length);
    content_size = length * kShortSize;
    resource = reinterpret_cast<const byte*>(
        ExternalTwoByteString::cast(string).resource()->data());
  }

  AllocationSpace space =
      (allocation_size > kMaxRegularHeapObjectSize) ? LO_SPACE : OLD_SPACE;
  SerializePrologue(space, allocation_size, map);

  // Output the rest of the imaginary string.
  int bytes_to_output = allocation_size - HeapObject::kHeaderSize;
  DCHECK(IsAligned(bytes_to_output, kTaggedSize));

  // Output raw data header. Do not bother with common raw length cases here.
  sink_->Put(kVariableRawData, "RawDataForString");
  sink_->PutInt(bytes_to_output, "length");

  // Serialize string header (except for map).
  uint8_t* string_start = reinterpret_cast<uint8_t*>(string.address());
  for (int i = HeapObject::kHeaderSize; i < SeqString::kHeaderSize; i++) {
    sink_->PutSection(string_start[i], "StringHeader");
  }

  // Serialize string content.
  sink_->PutRaw(resource, content_size, "StringContent");

  // Since the allocation size is rounded up to object alignment, there
  // maybe left-over bytes that need to be padded.
  int padding_size = allocation_size - SeqString::kHeaderSize - content_size;
  DCHECK(0 <= padding_size && padding_size < kObjectAlignment);
  for (int i = 0; i < padding_size; i++) sink_->PutSection(0, "StringPadding");
}

// Clear and later restore the next link in the weak cell or allocation site.
// TODO(all): replace this with proper iteration of weak slots in serializer.
class UnlinkWeakNextScope {
 public:
  explicit UnlinkWeakNextScope(Heap* heap, HeapObject object) {
    if (object.IsAllocationSite() &&
        AllocationSite::cast(object).HasWeakNext()) {
      object_ = object;
      next_ = AllocationSite::cast(object).weak_next();
      AllocationSite::cast(object).set_weak_next(
          ReadOnlyRoots(heap).undefined_value());
    }
  }

  ~UnlinkWeakNextScope() {
    if (!object_.is_null()) {
      AllocationSite::cast(object_).set_weak_next(next_,
                                                  UPDATE_WEAK_WRITE_BARRIER);
    }
  }

 private:
  HeapObject object_;
  Object next_;
  DISALLOW_HEAP_ALLOCATION(no_gc_)
};

void Serializer::ObjectSerializer::Serialize() {
  if (FLAG_trace_serializer) {
    PrintF(" Encoding heap object: ");
    object_.ShortPrint();
    PrintF("\n");
  }

  if (object_.IsExternalString()) {
    SerializeExternalString();
    return;
  } else if (!ReadOnlyHeap::Contains(object_)) {
    // Only clear padding for strings outside RO_SPACE. RO_SPACE should have
    // been cleared elsewhere.
    if (object_.IsSeqOneByteString()) {
      // Clear padding bytes at the end. Done here to avoid having to do this
      // at allocation sites in generated code.
      SeqOneByteString::cast(object_).clear_padding();
    } else if (object_.IsSeqTwoByteString()) {
      SeqTwoByteString::cast(object_).clear_padding();
    }
  }
  if (object_.IsJSTypedArray()) {
    SerializeJSTypedArray();
    return;
  }
  if (object_.IsJSArrayBuffer()) {
    SerializeJSArrayBuffer();
    return;
  }

  // We don't expect fillers.
  DCHECK(!object_.IsFiller());

  if (object_.IsScript()) {
    // Clear cached line ends.
    Object undefined = ReadOnlyRoots(serializer_->isolate()).undefined_value();
    Script::cast(object_).set_line_ends(undefined);
  }

  SerializeObject();
}

void Serializer::ObjectSerializer::SerializeObject() {
  int size = object_.Size();
  Map map = object_.map();
  AllocationSpace space =
      MemoryChunk::FromHeapObject(object_)->owner()->identity();
  // Young generation large objects are tenured.
  if (space == NEW_LO_SPACE) {
    space = LO_SPACE;
  }
  SerializePrologue(space, size, map);

  // Serialize the rest of the object.
  CHECK_EQ(0, bytes_processed_so_far_);
  bytes_processed_so_far_ = kTaggedSize;

  RecursionScope recursion(serializer_);
  // Objects that are immediately post processed during deserialization
  // cannot be deferred, since post processing requires the object content.
  if ((recursion.ExceedsMaximum() && CanBeDeferred(object_)) ||
      serializer_->MustBeDeferred(object_)) {
    serializer_->QueueDeferredObject(object_);
    sink_->Put(kDeferred, "Deferring object content");
    return;
  }

  SerializeContent(map, size);
}

void Serializer::ObjectSerializer::SerializeDeferred() {
  if (FLAG_trace_serializer) {
    PrintF(" Encoding deferred heap object: ");
    object_.ShortPrint();
    PrintF("\n");
  }

  int size = object_.Size();
  Map map = object_.map();
  SerializerReference back_reference =
      serializer_->reference_map()->LookupReference(
          reinterpret_cast<void*>(object_.ptr()));
  DCHECK(back_reference.is_back_reference());

  // Serialize the rest of the object.
  CHECK_EQ(0, bytes_processed_so_far_);
  bytes_processed_so_far_ = kTaggedSize;

  serializer_->PutAlignmentPrefix(object_);
  sink_->Put(kNewObject + back_reference.space(), "deferred object");
  serializer_->PutBackReference(object_, back_reference);
  sink_->PutInt(size >> kTaggedSizeLog2, "deferred object size");

  SerializeContent(map, size);
}

void Serializer::ObjectSerializer::SerializeContent(Map map, int size) {
  UnlinkWeakNextScope unlink_weak_next(serializer_->isolate()->heap(), object_);
  if (object_.IsCode()) {
    // For code objects, output raw bytes first.
    OutputCode(size);
    // Then iterate references via reloc info.
    object_.IterateBody(map, size, this);
  } else {
    // For other objects, iterate references first.
    object_.IterateBody(map, size, this);
    // Then output data payload, if any.
    OutputRawData(object_.address() + size);
  }
}

void Serializer::ObjectSerializer::VisitPointers(HeapObject host,
                                                 ObjectSlot start,
                                                 ObjectSlot end) {
  VisitPointers(host, MaybeObjectSlot(start), MaybeObjectSlot(end));
}

void Serializer::ObjectSerializer::VisitPointers(HeapObject host,
                                                 MaybeObjectSlot start,
                                                 MaybeObjectSlot end) {
  MaybeObjectSlot current = start;
  while (current < end) {
    while (current < end && (*current)->IsSmi()) {
      ++current;
    }
    if (current < end) {
      OutputRawData(current.address());
    }
    // TODO(ishell): Revisit this change once we stick to 32-bit compressed
    // tagged values.
    while (current < end && (*current)->IsCleared()) {
      sink_->Put(kClearedWeakReference, "ClearedWeakReference");
      bytes_processed_so_far_ += kTaggedSize;
      ++current;
    }
    HeapObject current_contents;
    HeapObjectReferenceType reference_type;
    while (current < end &&
           (*current)->GetHeapObject(&current_contents, &reference_type)) {
      RootIndex root_index;
      // Compute repeat count and write repeat prefix if applicable.
      // Repeats are not subject to the write barrier so we can only use
      // immortal immovable root members. They are never in new space.
      MaybeObjectSlot repeat_end = current + 1;
      if (repeat_end < end &&
          serializer_->root_index_map()->Lookup(current_contents,
                                                &root_index) &&
          RootsTable::IsImmortalImmovable(root_index) &&
          *current == *repeat_end) {
        DCHECK_EQ(reference_type, HeapObjectReferenceType::STRONG);
        DCHECK(!Heap::InYoungGeneration(current_contents));
        while (repeat_end < end && *repeat_end == *current) {
          repeat_end++;
        }
        int repeat_count = static_cast<int>(repeat_end - current);
        current = repeat_end;
        bytes_processed_so_far_ += repeat_count * kTaggedSize;
        serializer_->PutRepeat(repeat_count);
      } else {
        bytes_processed_so_far_ += kTaggedSize;
        ++current;
      }
      // Now write the object itself.
      if (reference_type == HeapObjectReferenceType::WEAK) {
        sink_->Put(kWeakPrefix, "WeakReference");
      }
      serializer_->SerializeObject(current_contents);
    }
  }
}

void Serializer::ObjectSerializer::VisitEmbeddedPointer(Code host,
                                                        RelocInfo* rinfo) {
  Object object = rinfo->target_object();
  serializer_->SerializeObject(HeapObject::cast(object));
  bytes_processed_so_far_ += rinfo->target_address_size();
}

void Serializer::ObjectSerializer::VisitExternalReference(Foreign host,
                                                          Address* p) {
  auto encoded_reference =
      serializer_->EncodeExternalReference(host.foreign_address());
  if (encoded_reference.is_from_api()) {
    sink_->Put(kApiReference, "ApiRef");
  } else {
    sink_->Put(kExternalReference, "ExternalRef");
  }
  sink_->PutInt(encoded_reference.index(), "reference index");
  bytes_processed_so_far_ += kSystemPointerSize;
}

void Serializer::ObjectSerializer::VisitExternalReference(Code host,
                                                          RelocInfo* rinfo) {
  Address target = rinfo->target_external_reference();
  auto encoded_reference = serializer_->EncodeExternalReference(target);
  if (encoded_reference.is_from_api()) {
    DCHECK(!rinfo->IsCodedSpecially());
    sink_->Put(kApiReference, "ApiRef");
  } else {
    sink_->Put(kExternalReference, "ExternalRef");
  }
  DCHECK_NE(target, kNullAddress);  // Code does not reference null.
  sink_->PutInt(encoded_reference.index(), "reference index");
  bytes_processed_so_far_ += rinfo->target_address_size();
}

void Serializer::ObjectSerializer::VisitInternalReference(Code host,
                                                          RelocInfo* rinfo) {
  Address entry = Code::cast(object_).entry();
  DCHECK_GE(rinfo->target_internal_reference(), entry);
  uintptr_t target_offset = rinfo->target_internal_reference() - entry;
  DCHECK_LE(target_offset, Code::cast(object_).raw_instruction_size());
  sink_->Put(kInternalReference, "InternalRef");
  sink_->PutInt(target_offset, "internal ref value");
}

void Serializer::ObjectSerializer::VisitRuntimeEntry(Code host,
                                                     RelocInfo* rinfo) {
  // We no longer serialize code that contains runtime entries.
  UNREACHABLE();
}

void Serializer::ObjectSerializer::VisitOffHeapTarget(Code host,
                                                      RelocInfo* rinfo) {
  DCHECK(FLAG_embedded_builtins);
  STATIC_ASSERT(EmbeddedData::kTableSize == Builtins::builtin_count);

  Address addr = rinfo->target_off_heap_target();
  CHECK_NE(kNullAddress, addr);

  Code target = InstructionStream::TryLookupCode(serializer_->isolate(), addr);
  CHECK(Builtins::IsIsolateIndependentBuiltin(target));

  sink_->Put(kOffHeapTarget, "OffHeapTarget");
  sink_->PutInt(target.builtin_index(), "builtin index");
  bytes_processed_so_far_ += rinfo->target_address_size();
}

void Serializer::ObjectSerializer::VisitCodeTarget(Code host,
                                                   RelocInfo* rinfo) {
#ifdef V8_TARGET_ARCH_ARM
  DCHECK(!RelocInfo::IsRelativeCodeTarget(rinfo->rmode()));
#endif
  Code object = Code::GetCodeFromTargetAddress(rinfo->target_address());
  serializer_->SerializeObject(object);
  bytes_processed_so_far_ += rinfo->target_address_size();
}

namespace {

// Similar to OutputRawData, but substitutes the given field with the given
// value instead of reading it from the object.
void OutputRawWithCustomField(SnapshotByteSink* sink, Address object_start,
                              int written_so_far, int bytes_to_write,
                              int field_offset, int field_size,
                              const byte* field_value) {
  int offset = field_offset - written_so_far;
  if (0 <= offset && offset < bytes_to_write) {
    DCHECK_GE(bytes_to_write, offset + field_size);
    sink->PutRaw(reinterpret_cast<byte*>(object_start + written_so_far), offset,
                 "Bytes");
    sink->PutRaw(field_value, field_size, "Bytes");
    written_so_far += offset + field_size;
    bytes_to_write -= offset + field_size;
    sink->PutRaw(reinterpret_cast<byte*>(object_start + written_so_far),
                 bytes_to_write, "Bytes");
  } else {
    sink->PutRaw(reinterpret_cast<byte*>(object_start + written_so_far),
                 bytes_to_write, "Bytes");
  }
}
}  // anonymous namespace

void Serializer::ObjectSerializer::OutputRawData(Address up_to) {
  Address object_start = object_.address();
  int base = bytes_processed_so_far_;
  int up_to_offset = static_cast<int>(up_to - object_start);
  int to_skip = up_to_offset - bytes_processed_so_far_;
  int bytes_to_output = to_skip;
  bytes_processed_so_far_ += to_skip;
  DCHECK_GE(to_skip, 0);
  if (bytes_to_output != 0) {
    DCHECK(to_skip == bytes_to_output);
    if (IsAligned(bytes_to_output, kObjectAlignment) &&
        bytes_to_output <= kNumberOfFixedRawData * kTaggedSize) {
      int size_in_words = bytes_to_output >> kTaggedSizeLog2;
      sink_->PutSection(kFixedRawDataStart + size_in_words, "FixedRawData");
    } else {
      sink_->Put(kVariableRawData, "VariableRawData");
      sink_->PutInt(bytes_to_output, "length");
    }
#ifdef MEMORY_SANITIZER
    // Check that we do not serialize uninitialized memory.
    __msan_check_mem_is_initialized(
        reinterpret_cast<void*>(object_start + base), bytes_to_output);
#endif  // MEMORY_SANITIZER
    if (object_.IsBytecodeArray()) {
      // The bytecode age field can be changed by GC concurrently.
      byte field_value = BytecodeArray::kNoAgeBytecodeAge;
      OutputRawWithCustomField(sink_, object_start, base, bytes_to_output,
                               BytecodeArray::kBytecodeAgeOffset,
                               sizeof(field_value), &field_value);
    } else if (object_.IsDescriptorArray()) {
      // The number of marked descriptors field can be changed by GC
      // concurrently.
      byte field_value[2];
      field_value[0] = 0;
      field_value[1] = 0;
      OutputRawWithCustomField(
          sink_, object_start, base, bytes_to_output,
          DescriptorArray::kRawNumberOfMarkedDescriptorsOffset,
          sizeof(field_value), field_value);
    } else {
      sink_->PutRaw(reinterpret_cast<byte*>(object_start + base),
                    bytes_to_output, "Bytes");
    }
  }
}

void Serializer::ObjectSerializer::OutputCode(int size) {
  DCHECK_EQ(kTaggedSize, bytes_processed_so_far_);
  Code on_heap_code = Code::cast(object_);
  // To make snapshots reproducible, we make a copy of the code object
  // and wipe all pointers in the copy, which we then serialize.
  Code off_heap_code = serializer_->CopyCode(on_heap_code);
  int mode_mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
                  RelocInfo::ModeMask(RelocInfo::FULL_EMBEDDED_OBJECT) |
                  RelocInfo::ModeMask(RelocInfo::COMPRESSED_EMBEDDED_OBJECT) |
                  RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
                  RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
                  RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) |
                  RelocInfo::ModeMask(RelocInfo::OFF_HEAP_TARGET) |
                  RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY);
  // With enabled pointer compression normal accessors no longer work for
  // off-heap objects, so we have to get the relocation info data via the
  // on-heap code object.
  ByteArray relocation_info = on_heap_code.unchecked_relocation_info();
  for (RelocIterator it(off_heap_code, relocation_info, mode_mask); !it.done();
       it.next()) {
    RelocInfo* rinfo = it.rinfo();
    rinfo->WipeOut();
  }
  // We need to wipe out the header fields *after* wiping out the
  // relocations, because some of these fields are needed for the latter.
  off_heap_code.WipeOutHeader();

  Address start = off_heap_code.address() + Code::kDataStart;
  int bytes_to_output = size - Code::kDataStart;
  DCHECK(IsAligned(bytes_to_output, kTaggedSize));

  sink_->Put(kVariableRawCode, "VariableRawCode");
  sink_->PutInt(bytes_to_output, "length");

#ifdef MEMORY_SANITIZER
  // Check that we do not serialize uninitialized memory.
  __msan_check_mem_is_initialized(reinterpret_cast<void*>(start),
                                  bytes_to_output);
#endif  // MEMORY_SANITIZER
  sink_->PutRaw(reinterpret_cast<byte*>(start), bytes_to_output, "Code");
}

}  // namespace internal
}  // namespace v8
