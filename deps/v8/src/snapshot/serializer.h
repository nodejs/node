// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SERIALIZER_H_
#define V8_SNAPSHOT_SERIALIZER_H_

#include <map>

#include "src/instruction-stream.h"
#include "src/isolate.h"
#include "src/log.h"
#include "src/objects.h"
#include "src/snapshot/default-serializer-allocator.h"
#include "src/snapshot/serializer-common.h"
#include "src/snapshot/snapshot-source-sink.h"

namespace v8 {
namespace internal {

class CodeAddressMap : public CodeEventLogger {
 public:
  explicit CodeAddressMap(Isolate* isolate) : CodeEventLogger(isolate) {
    isolate->logger()->AddCodeEventListener(this);
  }

  ~CodeAddressMap() override {
    isolate_->logger()->RemoveCodeEventListener(this);
  }

  void CodeMoveEvent(AbstractCode* from, AbstractCode* to) override {
    address_to_name_map_.Move(from->address(), to->address());
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
      for (base::HashMap::Entry* p = impl_.Start(); p != nullptr;
           p = impl_.Next(p)) {
        DeleteArray(static_cast<const char*>(p->value));
      }
    }

    void Insert(Address code_address, const char* name, int name_size) {
      base::HashMap::Entry* entry = FindOrCreateEntry(code_address);
      if (entry->value == nullptr) {
        entry->value = CopyName(name, name_size);
      }
    }

    const char* Lookup(Address code_address) {
      base::HashMap::Entry* entry = FindEntry(code_address);
      return (entry != nullptr) ? static_cast<const char*>(entry->value)
                                : nullptr;
    }

    void Remove(Address code_address) {
      base::HashMap::Entry* entry = FindEntry(code_address);
      if (entry != nullptr) {
        DeleteArray(static_cast<char*>(entry->value));
        RemoveEntry(entry);
      }
    }

    void Move(Address from, Address to) {
      if (from == to) return;
      base::HashMap::Entry* from_entry = FindEntry(from);
      DCHECK_NOT_NULL(from_entry);
      void* value = from_entry->value;
      RemoveEntry(from_entry);
      base::HashMap::Entry* to_entry = FindOrCreateEntry(to);
      DCHECK_NULL(to_entry->value);
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
      return impl_.LookupOrInsert(reinterpret_cast<void*>(code_address),
                                  ComputeAddressHash(code_address));
    }

    base::HashMap::Entry* FindEntry(Address code_address) {
      return impl_.Lookup(reinterpret_cast<void*>(code_address),
                          ComputeAddressHash(code_address));
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

  void LogRecordedBuffer(const wasm::WasmCode* code, const char* name,
                         int length) override {
    UNREACHABLE();
  }

  NameMap address_to_name_map_;
};

template <class AllocatorT = DefaultSerializerAllocator>
class Serializer : public SerializerDeserializer {
 public:
  explicit Serializer(Isolate* isolate);
  ~Serializer() override;

  std::vector<SerializedData::Reservation> EncodeReservations() const {
    return allocator_.EncodeReservations();
  }

  const std::vector<byte>* Payload() const { return sink_.data(); }

  bool ReferenceMapContains(HeapObject* o) {
    return reference_map()->LookupReference(o).is_valid();
  }

  Isolate* isolate() const { return isolate_; }

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

  void SerializeDeferredObjects();
  virtual void SerializeObject(HeapObject* o, HowToCode how_to_code,
                               WhereToPoint where_to_point, int skip) = 0;

  virtual bool MustBeDeferred(HeapObject* object);

  void VisitRootPointers(Root root, const char* description, Object** start,
                         Object** end) override;
  void SerializeRootObject(Object* object);

  void PutRoot(RootIndex root_index, HeapObject* object, HowToCode how,
               WhereToPoint where, int skip);
  void PutSmi(Smi* smi);
  void PutBackReference(HeapObject* object, SerializerReference reference);
  void PutAttachedReference(SerializerReference reference,
                            HowToCode how_to_code, WhereToPoint where_to_point);
  // Emit alignment prefix if necessary, return required padding space in bytes.
  int PutAlignmentPrefix(HeapObject* object);
  void PutNextChunk(int space);

  // Returns true if the object was successfully serialized as hot object.
  bool SerializeHotObject(HeapObject* obj, HowToCode how_to_code,
                          WhereToPoint where_to_point, int skip);

  // Returns true if the object was successfully serialized as back reference.
  bool SerializeBackReference(HeapObject* obj, HowToCode how_to_code,
                              WhereToPoint where_to_point, int skip);

  // Returns true if the object was successfully serialized as a builtin
  // reference.
  bool SerializeBuiltinReference(HeapObject* obj, HowToCode how_to_code,
                                 WhereToPoint where_to_point, int skip);

  // Returns true if the given heap object is a bytecode handler code object.
  bool ObjectIsBytecodeHandler(HeapObject* obj) const;

  inline void FlushSkip(int skip) {
    if (skip != 0) {
      sink_.Put(kSkip, "SkipFromSerializeObject");
      sink_.PutInt(skip, "SkipDistanceFromSerializeObject");
    }
  }

  ExternalReferenceEncoder::Value EncodeExternalReference(Address addr) {
    return external_reference_encoder_.Encode(addr);
  }

  // GetInt reads 4 bytes at once, requiring padding at the end.
  // Use padding_offset to specify the space you want to use after padding.
  void Pad(int padding_offset = 0);

  // We may not need the code address map for logging for every instance
  // of the serializer.  Initialize it on demand.
  void InitializeCodeAddressMap();

  Code* CopyCode(Code* code);

  void QueueDeferredObject(HeapObject* obj) {
    DCHECK(reference_map_.LookupReference(obj).is_back_reference());
    deferred_objects_.push_back(obj);
  }

  void OutputStatistics(const char* name);

#ifdef OBJECT_PRINT
  void CountInstanceType(Map* map, int size, AllocationSpace space);
#endif  // OBJECT_PRINT

#ifdef DEBUG
  void PushStack(HeapObject* o) { stack_.push_back(o); }
  void PopStack() { stack_.pop_back(); }
  void PrintStack();
#endif  // DEBUG

  SerializerReferenceMap* reference_map() { return &reference_map_; }
  RootIndexMap* root_index_map() { return &root_index_map_; }
  AllocatorT* allocator() { return &allocator_; }

  SnapshotByteSink sink_;  // Used directly by subclasses.

 private:
  Isolate* isolate_;
  SerializerReferenceMap reference_map_;
  ExternalReferenceEncoder external_reference_encoder_;
  RootIndexMap root_index_map_;
  CodeAddressMap* code_address_map_ = nullptr;
  std::vector<byte> code_buffer_;
  std::vector<HeapObject*> deferred_objects_;  // To handle stack overflow.
  int recursion_depth_ = 0;
  AllocatorT allocator_;

#ifdef OBJECT_PRINT
  static const int kInstanceTypes = LAST_TYPE + 1;
  int* instance_type_count_[LAST_SPACE];
  size_t* instance_type_size_[LAST_SPACE];
#endif  // OBJECT_PRINT

#ifdef DEBUG
  std::vector<HeapObject*> stack_;
#endif  // DEBUG

  friend class DefaultSerializerAllocator;

  DISALLOW_COPY_AND_ASSIGN(Serializer);
};

class RelocInfoIterator;

template <class AllocatorT>
class Serializer<AllocatorT>::ObjectSerializer : public ObjectVisitor {
 public:
  ObjectSerializer(Serializer* serializer, HeapObject* obj,
                   SnapshotByteSink* sink, HowToCode how_to_code,
                   WhereToPoint where_to_point)
      : serializer_(serializer),
        object_(obj),
        sink_(sink),
        reference_representation_(how_to_code + where_to_point),
        bytes_processed_so_far_(0) {
#ifdef DEBUG
    serializer_->PushStack(obj);
#endif  // DEBUG
  }
  // NOLINTNEXTLINE (modernize-use-equals-default)
  ~ObjectSerializer() override {
#ifdef DEBUG
    serializer_->PopStack();
#endif  // DEBUG
  }
  void Serialize();
  void SerializeObject();
  void SerializeDeferred();
  void VisitPointers(HeapObject* host, Object** start, Object** end) override;
  void VisitPointers(HeapObject* host, MaybeObject** start,
                     MaybeObject** end) override;
  void VisitEmbeddedPointer(Code* host, RelocInfo* target) override;
  void VisitExternalReference(Foreign* host, Address* p) override;
  void VisitExternalReference(Code* host, RelocInfo* rinfo) override;
  void VisitInternalReference(Code* host, RelocInfo* rinfo) override;
  void VisitCodeTarget(Code* host, RelocInfo* target) override;
  void VisitRuntimeEntry(Code* host, RelocInfo* reloc) override;
  void VisitOffHeapTarget(Code* host, RelocInfo* target) override;
  // Relocation info needs to be visited sorted by target_address_address.
  void VisitRelocInfo(RelocIterator* it) override;

 private:
  void SerializePrologue(AllocationSpace space, int size, Map* map);

  // This function outputs or skips the raw data between the last pointer and
  // up to the current position.
  void SerializeContent(Map* map, int size);
  void OutputRawData(Address up_to);
  void OutputCode(int size);
  int SkipTo(Address to);
  int32_t SerializeBackingStore(void* backing_store, int32_t byte_length);
  void SerializeJSTypedArray();
  void SerializeJSArrayBuffer();
  void SerializeExternalString();
  void SerializeExternalStringAsSequentialString();

  Serializer* serializer_;
  HeapObject* object_;
  SnapshotByteSink* sink_;
  std::map<void*, Smi*> backing_stores;
  int reference_representation_;
  int bytes_processed_so_far_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SERIALIZER_H_
