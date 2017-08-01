// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SERIALIZER_H_
#define V8_SNAPSHOT_SERIALIZER_H_

#include "src/isolate.h"
#include "src/log.h"
#include "src/objects.h"
#include "src/snapshot/serializer-common.h"
#include "src/snapshot/snapshot-source-sink.h"

namespace v8 {
namespace internal {

class CodeAddressMap : public CodeEventLogger {
 public:
  explicit CodeAddressMap(Isolate* isolate) : isolate_(isolate) {
    isolate->logger()->addCodeEventListener(this);
  }

  ~CodeAddressMap() override {
    isolate_->logger()->removeCodeEventListener(this);
  }

  void CodeMoveEvent(AbstractCode* from, Address to) override {
    address_to_name_map_.Move(from->address(), to);
  }

  void CodeDisableOptEvent(AbstractCode* code,
                           SharedFunctionInfo* shared) override {}

  const char* Lookup(Address address) {
    return address_to_name_map_.Lookup(address);
  }

 private:
  class NameMap {
   public:
    NameMap() : impl_() {}

    ~NameMap() {
      for (base::HashMap::Entry* p = impl_.Start(); p != NULL;
           p = impl_.Next(p)) {
        DeleteArray(static_cast<const char*>(p->value));
      }
    }

    void Insert(Address code_address, const char* name, int name_size) {
      base::HashMap::Entry* entry = FindOrCreateEntry(code_address);
      if (entry->value == NULL) {
        entry->value = CopyName(name, name_size);
      }
    }

    const char* Lookup(Address code_address) {
      base::HashMap::Entry* entry = FindEntry(code_address);
      return (entry != NULL) ? static_cast<const char*>(entry->value) : NULL;
    }

    void Remove(Address code_address) {
      base::HashMap::Entry* entry = FindEntry(code_address);
      if (entry != NULL) {
        DeleteArray(static_cast<char*>(entry->value));
        RemoveEntry(entry);
      }
    }

    void Move(Address from, Address to) {
      if (from == to) return;
      base::HashMap::Entry* from_entry = FindEntry(from);
      DCHECK(from_entry != NULL);
      void* value = from_entry->value;
      RemoveEntry(from_entry);
      base::HashMap::Entry* to_entry = FindOrCreateEntry(to);
      DCHECK(to_entry->value == NULL);
      to_entry->value = value;
    }

   private:
    static char* CopyName(const char* name, int name_size) {
      char* result = NewArray<char>(name_size + 1);
      for (int i = 0; i < name_size; ++i) {
        char c = name[i];
        if (c == '\0') c = ' ';
        result[i] = c;
      }
      result[name_size] = '\0';
      return result;
    }

    base::HashMap::Entry* FindOrCreateEntry(Address code_address) {
      return impl_.LookupOrInsert(code_address,
                                  ComputePointerHash(code_address));
    }

    base::HashMap::Entry* FindEntry(Address code_address) {
      return impl_.Lookup(code_address, ComputePointerHash(code_address));
    }

    void RemoveEntry(base::HashMap::Entry* entry) {
      impl_.Remove(entry->key, entry->hash);
    }

    base::HashMap impl_;

    DISALLOW_COPY_AND_ASSIGN(NameMap);
  };

  void LogRecordedBuffer(AbstractCode* code, SharedFunctionInfo*,
                         const char* name, int length) override {
    address_to_name_map_.Insert(code->address(), name, length);
  }

  NameMap address_to_name_map_;
  Isolate* isolate_;
};

// There can be only one serializer per V8 process.
class Serializer : public SerializerDeserializer {
 public:
  explicit Serializer(Isolate* isolate);
  ~Serializer() override;

  void EncodeReservations(List<SerializedData::Reservation>* out) const;

  void SerializeDeferredObjects();

  Isolate* isolate() const { return isolate_; }

  SerializerReferenceMap* reference_map() { return &reference_map_; }
  RootIndexMap* root_index_map() { return &root_index_map_; }

#ifdef OBJECT_PRINT
  void CountInstanceType(Map* map, int size);
#endif  // OBJECT_PRINT

 protected:
  class ObjectSerializer;
  class RecursionScope {
   public:
    explicit RecursionScope(Serializer* serializer) : serializer_(serializer) {
      serializer_->recursion_depth_++;
    }
    ~RecursionScope() { serializer_->recursion_depth_--; }
    bool ExceedsMaximum() {
      return serializer_->recursion_depth_ >= kMaxRecursionDepth;
    }

   private:
    static const int kMaxRecursionDepth = 32;
    Serializer* serializer_;
  };

  virtual void SerializeObject(HeapObject* o, HowToCode how_to_code,
                               WhereToPoint where_to_point, int skip) = 0;

  void VisitRootPointers(Root root, Object** start, Object** end) override;

  void PutRoot(int index, HeapObject* object, HowToCode how, WhereToPoint where,
               int skip);

  void PutSmi(Smi* smi);

  void PutBackReference(HeapObject* object, SerializerReference reference);

  void PutAttachedReference(SerializerReference reference,
                            HowToCode how_to_code, WhereToPoint where_to_point);

  // Emit alignment prefix if necessary, return required padding space in bytes.
  int PutAlignmentPrefix(HeapObject* object);

  // Returns true if the object was successfully serialized as hot object.
  bool SerializeHotObject(HeapObject* obj, HowToCode how_to_code,
                          WhereToPoint where_to_point, int skip);

  // Returns true if the object was successfully serialized as back reference.
  bool SerializeBackReference(HeapObject* obj, HowToCode how_to_code,
                              WhereToPoint where_to_point, int skip);

  inline void FlushSkip(int skip) {
    if (skip != 0) {
      sink_.Put(kSkip, "SkipFromSerializeObject");
      sink_.PutInt(skip, "SkipDistanceFromSerializeObject");
    }
  }

  bool BackReferenceIsAlreadyAllocated(SerializerReference back_reference);

  // This will return the space for an object.
  SerializerReference AllocateLargeObject(int size);
  SerializerReference AllocateMap();
  SerializerReference Allocate(AllocationSpace space, int size);
  int EncodeExternalReference(Address addr) {
    return external_reference_encoder_.Encode(addr);
  }

  bool HasNotExceededFirstPageOfEachSpace();

  // GetInt reads 4 bytes at once, requiring padding at the end.
  void Pad();

  // We may not need the code address map for logging for every instance
  // of the serializer.  Initialize it on demand.
  void InitializeCodeAddressMap();

  Code* CopyCode(Code* code);

  inline uint32_t max_chunk_size(int space) const {
    DCHECK_LE(0, space);
    DCHECK_LT(space, kNumberOfSpaces);
    return max_chunk_size_[space];
  }

  const SnapshotByteSink* sink() const { return &sink_; }

  void QueueDeferredObject(HeapObject* obj) {
    DCHECK(reference_map_.Lookup(obj).is_back_reference());
    deferred_objects_.Add(obj);
  }

  void OutputStatistics(const char* name);

  Isolate* isolate_;

  SnapshotByteSink sink_;
  ExternalReferenceEncoder external_reference_encoder_;

  SerializerReferenceMap reference_map_;
  RootIndexMap root_index_map_;

  int recursion_depth_;

  friend class Deserializer;
  friend class ObjectSerializer;
  friend class RecursionScope;
  friend class SnapshotData;

 private:
  CodeAddressMap* code_address_map_;
  // Objects from the same space are put into chunks for bulk-allocation
  // when deserializing. We have to make sure that each chunk fits into a
  // page. So we track the chunk size in pending_chunk_ of a space, but
  // when it exceeds a page, we complete the current chunk and start a new one.
  uint32_t pending_chunk_[kNumberOfPreallocatedSpaces];
  List<uint32_t> completed_chunks_[kNumberOfPreallocatedSpaces];
  uint32_t max_chunk_size_[kNumberOfPreallocatedSpaces];
  // Number of maps that we need to allocate.
  uint32_t num_maps_;

  // We map serialized large objects to indexes for back-referencing.
  uint32_t large_objects_total_size_;
  uint32_t seen_large_objects_index_;

  List<byte> code_buffer_;

  // To handle stack overflow.
  List<HeapObject*> deferred_objects_;

#ifdef OBJECT_PRINT
  static const int kInstanceTypes = 256;
  int* instance_type_count_;
  size_t* instance_type_size_;
#endif  // OBJECT_PRINT

  DISALLOW_COPY_AND_ASSIGN(Serializer);
};

class Serializer::ObjectSerializer : public ObjectVisitor {
 public:
  ObjectSerializer(Serializer* serializer, HeapObject* obj,
                   SnapshotByteSink* sink, HowToCode how_to_code,
                   WhereToPoint where_to_point)
      : serializer_(serializer),
        object_(obj),
        sink_(sink),
        reference_representation_(how_to_code + where_to_point),
        bytes_processed_so_far_(0),
        code_has_been_output_(false) {}
  ~ObjectSerializer() override {}
  void Serialize();
  void SerializeContent();
  void SerializeDeferred();
  void VisitPointers(HeapObject* host, Object** start, Object** end) override;
  void VisitEmbeddedPointer(Code* host, RelocInfo* target) override;
  void VisitExternalReference(Foreign* host, Address* p) override;
  void VisitExternalReference(Code* host, RelocInfo* rinfo) override;
  void VisitInternalReference(Code* host, RelocInfo* rinfo) override;
  void VisitCodeTarget(Code* host, RelocInfo* target) override;
  void VisitCodeEntry(JSFunction* host, Address entry_address) override;
  void VisitCellPointer(Code* host, RelocInfo* rinfo) override;
  void VisitRuntimeEntry(Code* host, RelocInfo* reloc) override;

 private:
  bool TryEncodeDeoptimizationEntry(HowToCode how_to_code, Address target,
                                    int skip);
  void SerializePrologue(AllocationSpace space, int size, Map* map);


  enum ReturnSkip { kCanReturnSkipInsteadOfSkipping, kIgnoringReturn };
  // This function outputs or skips the raw data between the last pointer and
  // up to the current position.  It optionally can just return the number of
  // bytes to skip instead of performing a skip instruction, in case the skip
  // can be merged into the next instruction.
  int OutputRawData(Address up_to, ReturnSkip return_skip = kIgnoringReturn);
  void SerializeExternalString();
  void SerializeExternalStringAsSequentialString();

  Address PrepareCode();

  Serializer* serializer_;
  HeapObject* object_;
  SnapshotByteSink* sink_;
  int reference_representation_;
  int bytes_processed_so_far_;
  bool code_has_been_output_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SERIALIZER_H_
