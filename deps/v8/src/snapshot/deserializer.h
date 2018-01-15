// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_DESERIALIZER_H_
#define V8_SNAPSHOT_DESERIALIZER_H_

#include <vector>

#include "src/heap/heap.h"
#include "src/objects.h"
#include "src/snapshot/serializer-common.h"
#include "src/snapshot/snapshot-source-sink.h"

namespace v8 {
namespace internal {

// Used for platforms with embedded constant pools to trigger deserialization
// of objects found in code.
#if defined(V8_TARGET_ARCH_MIPS) || defined(V8_TARGET_ARCH_MIPS64) || \
    defined(V8_TARGET_ARCH_PPC) || defined(V8_TARGET_ARCH_S390) ||    \
    V8_EMBEDDED_CONSTANT_POOL
#define V8_CODE_EMBEDS_OBJECT_POINTER 1
#else
#define V8_CODE_EMBEDS_OBJECT_POINTER 0
#endif

class Heap;

// A Deserializer reads a snapshot and reconstructs the Object graph it defines.
class Deserializer : public SerializerDeserializer {
 public:
  ~Deserializer() override;

  // Add an object to back an attached reference. The order to add objects must
  // mirror the order they are added in the serializer.
  void AddAttachedObject(Handle<HeapObject> attached_object) {
    attached_objects_.push_back(attached_object);
  }

  void SetRehashability(bool v) { can_rehash_ = v; }

 protected:
  // Create a deserializer from a snapshot byte source.
  template <class Data>
  Deserializer(Data* data, bool deserializing_user_code)
      : isolate_(NULL),
        source_(data->Payload()),
        magic_number_(data->GetMagicNumber()),
        next_map_index_(0),
        external_reference_table_(NULL),
        deserialized_large_objects_(0),
        deserializing_user_code_(deserializing_user_code),
        next_alignment_(kWordAligned),
        can_rehash_(false) {
    DecodeReservation(data->Reservations());
    // We start the indicies here at 1, so that we can distinguish between an
    // actual index and a nullptr in a deserialized object requiring fix-up.
    off_heap_backing_stores_.push_back(nullptr);
  }

  void Initialize(Isolate* isolate);
  bool ReserveSpace();
  void DeserializeDeferredObjects();
  void RegisterDeserializedObjectsForBlackAllocation();

  // This returns the address of an object that has been described in the
  // snapshot by chunk index and offset.
  HeapObject* GetBackReferencedObject(int space);

  // Sort descriptors of deserialized maps using new string hashes.
  void SortMapDescriptors();

  Isolate* isolate() const { return isolate_; }
  SnapshotByteSource* source() { return &source_; }
  const std::vector<Code*>& new_code_objects() const {
    return new_code_objects_;
  }
  const std::vector<AccessorInfo*>& accessor_infos() const {
    return accessor_infos_;
  }
  const std::vector<Handle<String>>& new_internalized_strings() const {
    return new_internalized_strings_;
  }
  const std::vector<Handle<Script>>& new_scripts() const {
    return new_scripts_;
  }
  const std::vector<TransitionArray*>& transition_arrays() const {
    return transition_arrays_;
  }
  bool deserializing_user_code() const { return deserializing_user_code_; }
  bool can_rehash() const { return can_rehash_; }

 private:
  void VisitRootPointers(Root root, Object** start, Object** end) override;

  void Synchronize(VisitorSynchronization::SyncTag tag) override;

  void DecodeReservation(Vector<const SerializedData::Reservation> res);

  void UnalignedCopy(Object** dest, Object** src) {
    memcpy(dest, src, sizeof(*src));
  }

  void SetAlignment(byte data) {
    DCHECK_EQ(kWordAligned, next_alignment_);
    int alignment = data - (kAlignmentPrefix - 1);
    DCHECK_LE(kWordAligned, alignment);
    DCHECK_LE(alignment, kDoubleUnaligned);
    next_alignment_ = static_cast<AllocationAlignment>(alignment);
  }

  // Fills in some heap data in an area from start to end (non-inclusive).  The
  // space id is used for the write barrier.  The object_address is the address
  // of the object we are writing into, or NULL if we are not writing into an
  // object, i.e. if we are writing a series of tagged values that are not on
  // the heap. Return false if the object content has been deferred.
  bool ReadData(Object** start, Object** end, int space,
                Address object_address);

  // A helper function for ReadData, templatized on the bytecode for efficiency.
  // Returns the new value of {current}.
  template <int where, int how, int within, int space_number_if_any>
  inline Object** ReadDataCase(Isolate* isolate, Object** current,
                               Address current_object_address, byte data,
                               bool write_barrier_needed);

  void ReadObject(int space_number, Object** write_back);
  Address Allocate(int space_index, int size);

  // Special handling for serialized code like hooking up internalized strings.
  HeapObject* PostProcessNewObject(HeapObject* obj, int space);

  // Cached current isolate.
  Isolate* isolate_;

  // Objects from the attached object descriptions in the serialized user code.
  std::vector<Handle<HeapObject>> attached_objects_;

  SnapshotByteSource source_;
  uint32_t magic_number_;

  // The address of the next object that will be allocated in each space.
  // Each space has a number of chunks reserved by the GC, with each chunk
  // fitting into a page. Deserialized objects are allocated into the
  // current chunk of the target space by bumping up high water mark.
  Heap::Reservation reservations_[kNumberOfSpaces];
  uint32_t current_chunk_[kNumberOfPreallocatedSpaces];
  Address high_water_[kNumberOfPreallocatedSpaces];
  int next_map_index_;
  std::vector<Address> allocated_maps_;

  ExternalReferenceTable* external_reference_table_;

  std::vector<HeapObject*> deserialized_large_objects_;
  std::vector<Code*> new_code_objects_;
  std::vector<AccessorInfo*> accessor_infos_;
  std::vector<Handle<String>> new_internalized_strings_;
  std::vector<Handle<Script>> new_scripts_;
  std::vector<TransitionArray*> transition_arrays_;
  std::vector<byte*> off_heap_backing_stores_;

  const bool deserializing_user_code_;

  AllocationAlignment next_alignment_;

  // TODO(6593): generalize rehashing, and remove this flag.
  bool can_rehash_;

#ifdef DEBUG
  uint32_t num_api_references_;
#endif  // DEBUG

  DISALLOW_COPY_AND_ASSIGN(Deserializer);
};

// Used to insert a deserialized internalized string into the string table.
class StringTableInsertionKey : public StringTableKey {
 public:
  explicit StringTableInsertionKey(String* string);

  bool IsMatch(Object* string) override;

  MUST_USE_RESULT Handle<String> AsHandle(Isolate* isolate) override;

 private:
  uint32_t ComputeHashField(String* string);

  String* string_;
  DisallowHeapAllocation no_gc;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_DESERIALIZER_H_
