// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_DESERIALIZER_H_
#define V8_SNAPSHOT_DESERIALIZER_H_

#include <vector>

#include "src/objects/js-array.h"
#include "src/snapshot/default-deserializer-allocator.h"
#include "src/snapshot/serializer-common.h"
#include "src/snapshot/snapshot-source-sink.h"

namespace v8 {
namespace internal {

class AllocationSite;
class HeapObject;
class Object;

// Used for platforms with embedded constant pools to trigger deserialization
// of objects found in code.
#if defined(V8_TARGET_ARCH_MIPS) || defined(V8_TARGET_ARCH_MIPS64) || \
    defined(V8_TARGET_ARCH_PPC) || defined(V8_TARGET_ARCH_S390) ||    \
    V8_EMBEDDED_CONSTANT_POOL
#define V8_CODE_EMBEDS_OBJECT_POINTER 1
#else
#define V8_CODE_EMBEDS_OBJECT_POINTER 0
#endif

// A Deserializer reads a snapshot and reconstructs the Object graph it defines.
template <class AllocatorT = DefaultDeserializerAllocator>
class Deserializer : public SerializerDeserializer {
 public:
  ~Deserializer() override;

  void SetRehashability(bool v) { can_rehash_ = v; }

 protected:
  // Create a deserializer from a snapshot byte source.
  template <class Data>
  Deserializer(Data* data, bool deserializing_user_code)
      : isolate_(nullptr),
        source_(data->Payload()),
        magic_number_(data->GetMagicNumber()),
        external_reference_table_(nullptr),
        allocator_(this),
        deserializing_user_code_(deserializing_user_code),
        can_rehash_(false) {
    allocator()->DecodeReservation(data->Reservations());
    // We start the indices here at 1, so that we can distinguish between an
    // actual index and a nullptr in a deserialized object requiring fix-up.
    off_heap_backing_stores_.push_back(nullptr);
  }

  void Initialize(Isolate* isolate);
  void DeserializeDeferredObjects();

  // Deserializes into a single pointer and returns the resulting object.
  Object* ReadDataSingle();

  // This returns the address of an object that has been described in the
  // snapshot by chunk index and offset.
  HeapObject* GetBackReferencedObject(int space);

  // Add an object to back an attached reference. The order to add objects must
  // mirror the order they are added in the serializer.
  void AddAttachedObject(Handle<HeapObject> attached_object) {
    attached_objects_.push_back(attached_object);
  }

  Isolate* isolate() const { return isolate_; }
  SnapshotByteSource* source() { return &source_; }
  const std::vector<AllocationSite*>& new_allocation_sites() const {
    return new_allocation_sites_;
  }
  const std::vector<Code*>& new_code_objects() const {
    return new_code_objects_;
  }
  const std::vector<AccessorInfo*>& accessor_infos() const {
    return accessor_infos_;
  }
  const std::vector<CallHandlerInfo*>& call_handler_infos() const {
    return call_handler_infos_;
  }
  const std::vector<Handle<String>>& new_internalized_strings() const {
    return new_internalized_strings_;
  }
  const std::vector<Handle<Script>>& new_scripts() const {
    return new_scripts_;
  }

  AllocatorT* allocator() { return &allocator_; }
  bool deserializing_user_code() const { return deserializing_user_code_; }
  bool can_rehash() const { return can_rehash_; }

  bool IsLazyDeserializationEnabled() const;

  void Rehash();

 private:
  void VisitRootPointers(Root root, const char* description, Object** start,
                         Object** end) override;

  void Synchronize(VisitorSynchronization::SyncTag tag) override;

  template <typename T>
  void UnalignedCopy(T** dest, T** src) {
    DCHECK(!allocator()->next_reference_is_weak());
    memcpy(dest, src, sizeof(*src));
  }

  // Fills in some heap data in an area from start to end (non-inclusive).  The
  // space id is used for the write barrier.  The object_address is the address
  // of the object we are writing into, or nullptr if we are not writing into an
  // object, i.e. if we are writing a series of tagged values that are not on
  // the heap. Return false if the object content has been deferred.
  bool ReadData(MaybeObject** start, MaybeObject** end, int space,
                Address object_address);

  // A helper function for ReadData, templatized on the bytecode for efficiency.
  // Returns the new value of {current}.
  template <int where, int how, int within, int space_number_if_any>
  inline MaybeObject** ReadDataCase(Isolate* isolate, MaybeObject** current,
                                    Address current_object_address, byte data,
                                    bool write_barrier_needed);

  // A helper function for ReadData for reading external references.
  // Returns the new value of {current}.
  inline void** ReadExternalReferenceCase(HowToCode how, void** current,
                                          Address current_object_address);

  void ReadObject(int space_number, MaybeObject** write_back,
                  HeapObjectReferenceType reference_type);

  // Special handling for serialized code like hooking up internalized strings.
  HeapObject* PostProcessNewObject(HeapObject* obj, int space);

  // May replace the given builtin_id with the DeserializeLazy builtin for lazy
  // deserialization.
  int MaybeReplaceWithDeserializeLazy(int builtin_id);

  // Cached current isolate.
  Isolate* isolate_;

  // Objects from the attached object descriptions in the serialized user code.
  std::vector<Handle<HeapObject>> attached_objects_;

  SnapshotByteSource source_;
  uint32_t magic_number_;

  ExternalReferenceTable* external_reference_table_;

  std::vector<AllocationSite*> new_allocation_sites_;
  std::vector<Code*> new_code_objects_;
  std::vector<AccessorInfo*> accessor_infos_;
  std::vector<CallHandlerInfo*> call_handler_infos_;
  std::vector<Handle<String>> new_internalized_strings_;
  std::vector<Handle<Script>> new_scripts_;
  std::vector<byte*> off_heap_backing_stores_;

  AllocatorT allocator_;
  const bool deserializing_user_code_;

  // TODO(6593): generalize rehashing, and remove this flag.
  bool can_rehash_;
  std::vector<HeapObject*> to_rehash_;

#ifdef DEBUG
  uint32_t num_api_references_;
#endif  // DEBUG

  // For source(), isolate(), and allocator().
  friend class DefaultDeserializerAllocator;

  DISALLOW_COPY_AND_ASSIGN(Deserializer);
};

// Used to insert a deserialized internalized string into the string table.
class StringTableInsertionKey : public StringTableKey {
 public:
  explicit StringTableInsertionKey(String* string);

  bool IsMatch(Object* string) override;

  V8_WARN_UNUSED_RESULT Handle<String> AsHandle(Isolate* isolate) override;

 private:
  uint32_t ComputeHashField(String* string);

  String* string_;
  DisallowHeapAllocation no_gc;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_DESERIALIZER_H_
