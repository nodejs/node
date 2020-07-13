// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SERIALIZER_H_
#define V8_SNAPSHOT_SERIALIZER_H_

#include <map>

#include "src/codegen/external-reference-encoder.h"
#include "src/execution/isolate.h"
#include "src/logging/log.h"
#include "src/objects/objects.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/snapshot/serializer-allocator.h"
#include "src/snapshot/serializer-deserializer.h"
#include "src/snapshot/snapshot-source-sink.h"
#include "src/snapshot/snapshot.h"

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

  void CodeMoveEvent(AbstractCode from, AbstractCode to) override {
    address_to_name_map_.Move(from.address(), to.address());
  }

  void CodeDisableOptEvent(Handle<AbstractCode> code,
                           Handle<SharedFunctionInfo> shared) override {}

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

  void LogRecordedBuffer(Handle<AbstractCode> code,
                         MaybeHandle<SharedFunctionInfo>, const char* name,
                         int length) override {
    address_to_name_map_.Insert(code->address(), name, length);
  }

  void LogRecordedBuffer(const wasm::WasmCode* code, const char* name,
                         int length) override {
    UNREACHABLE();
  }

  NameMap address_to_name_map_;
};

class ObjectCacheIndexMap {
 public:
  ObjectCacheIndexMap() : map_(), next_index_(0) {}

  // If |obj| is in the map, immediately return true.  Otherwise add it to the
  // map and return false. In either case set |*index_out| to the index
  // associated with the map.
  bool LookupOrInsert(HeapObject obj, int* index_out) {
    Maybe<uint32_t> maybe_index = map_.Get(obj);
    if (maybe_index.IsJust()) {
      *index_out = maybe_index.FromJust();
      return true;
    }
    *index_out = next_index_;
    map_.Set(obj, next_index_++);
    return false;
  }

 private:
  DisallowHeapAllocation no_allocation_;

  HeapObjectToIndexHashMap map_;
  int next_index_;

  DISALLOW_COPY_AND_ASSIGN(ObjectCacheIndexMap);
};

class Serializer : public SerializerDeserializer {
 public:
  Serializer(Isolate* isolate, Snapshot::SerializerFlags flags);

  std::vector<SerializedData::Reservation> EncodeReservations() const {
    return allocator_.EncodeReservations();
  }

  const std::vector<byte>* Payload() const { return sink_.data(); }

  bool ReferenceMapContains(HeapObject o) {
    return reference_map()
        ->LookupReference(reinterpret_cast<void*>(o.ptr()))
        .is_valid();
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
  virtual void SerializeObject(HeapObject o) = 0;

  virtual bool MustBeDeferred(HeapObject object);

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override;
  void SerializeRootObject(FullObjectSlot slot);

  void PutRoot(RootIndex root_index, HeapObject object);
  void PutSmiRoot(FullObjectSlot slot);
  void PutBackReference(HeapObject object, SerializerReference reference);
  void PutAttachedReference(SerializerReference reference);
  // Emit alignment prefix if necessary, return required padding space in bytes.
  int PutAlignmentPrefix(HeapObject object);
  void PutNextChunk(SnapshotSpace space);
  void PutRepeat(int repeat_count);

  // Returns true if the object was successfully serialized as a root.
  bool SerializeRoot(HeapObject obj);

  // Returns true if the object was successfully serialized as hot object.
  bool SerializeHotObject(HeapObject obj);

  // Returns true if the object was successfully serialized as back reference.
  bool SerializeBackReference(HeapObject obj);

  // Returns true if the given heap object is a bytecode handler code object.
  bool ObjectIsBytecodeHandler(HeapObject obj) const;

  ExternalReferenceEncoder::Value EncodeExternalReference(Address addr) {
    return external_reference_encoder_.Encode(addr);
  }
  Maybe<ExternalReferenceEncoder::Value> TryEncodeExternalReference(
      Address addr) {
    return external_reference_encoder_.TryEncode(addr);
  }

  // GetInt reads 4 bytes at once, requiring padding at the end.
  // Use padding_offset to specify the space you want to use after padding.
  void Pad(int padding_offset = 0);

  // We may not need the code address map for logging for every instance
  // of the serializer.  Initialize it on demand.
  void InitializeCodeAddressMap();

  Code CopyCode(Code code);

  void QueueDeferredObject(HeapObject obj) {
    DCHECK(reference_map_.LookupReference(reinterpret_cast<void*>(obj.ptr()))
               .is_back_reference());
    deferred_objects_.push_back(obj);
  }

  void OutputStatistics(const char* name);

#ifdef OBJECT_PRINT
  void CountInstanceType(Map map, int size, SnapshotSpace space);
#endif  // OBJECT_PRINT

#ifdef DEBUG
  void PushStack(HeapObject o) { stack_.push_back(o); }
  void PopStack() { stack_.pop_back(); }
  void PrintStack();
  void PrintStack(std::ostream&);
#endif  // DEBUG

  SerializerReferenceMap* reference_map() { return &reference_map_; }
  const RootIndexMap* root_index_map() const { return &root_index_map_; }
  SerializerAllocator* allocator() { return &allocator_; }

  SnapshotByteSink sink_;  // Used directly by subclasses.

  bool allow_unknown_external_references_for_testing() const {
    return (flags_ & Snapshot::kAllowUnknownExternalReferencesForTesting) != 0;
  }
  bool allow_active_isolate_for_testing() const {
    return (flags_ & Snapshot::kAllowActiveIsolateForTesting) != 0;
  }

 private:
  Isolate* isolate_;
  SerializerReferenceMap reference_map_;
  ExternalReferenceEncoder external_reference_encoder_;
  RootIndexMap root_index_map_;
  std::unique_ptr<CodeAddressMap> code_address_map_;
  std::vector<byte> code_buffer_;
  std::vector<HeapObject> deferred_objects_;  // To handle stack overflow.
  int recursion_depth_ = 0;
  const Snapshot::SerializerFlags flags_;
  SerializerAllocator allocator_;

#ifdef OBJECT_PRINT
  static constexpr int kInstanceTypes = LAST_TYPE + 1;
  std::unique_ptr<int[]> instance_type_count_[kNumberOfSpaces];
  std::unique_ptr<size_t[]> instance_type_size_[kNumberOfSpaces];
#endif  // OBJECT_PRINT

#ifdef DEBUG
  std::vector<HeapObject> stack_;
#endif  // DEBUG

  friend class SerializerAllocator;

  DISALLOW_COPY_AND_ASSIGN(Serializer);
};

class RelocInfoIterator;

class Serializer::ObjectSerializer : public ObjectVisitor {
 public:
  ObjectSerializer(Serializer* serializer, HeapObject obj,
                   SnapshotByteSink* sink)
      : serializer_(serializer),
        object_(obj),
        sink_(sink),
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
  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override;
  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override;
  void VisitEmbeddedPointer(Code host, RelocInfo* target) override;
  void VisitExternalReference(Foreign host, Address* p) override;
  void VisitExternalReference(Code host, RelocInfo* rinfo) override;
  void VisitInternalReference(Code host, RelocInfo* rinfo) override;
  void VisitCodeTarget(Code host, RelocInfo* target) override;
  void VisitRuntimeEntry(Code host, RelocInfo* reloc) override;
  void VisitOffHeapTarget(Code host, RelocInfo* target) override;

 private:
  void SerializePrologue(SnapshotSpace space, int size, Map map);

  // This function outputs or skips the raw data between the last pointer and
  // up to the current position.
  void SerializeContent(Map map, int size);
  void OutputExternalReference(Address target, int target_size,
                               bool sandboxify);
  void OutputRawData(Address up_to);
  void OutputCode(int size);
  uint32_t SerializeBackingStore(void* backing_store, int32_t byte_length);
  void SerializeJSTypedArray();
  void SerializeJSArrayBuffer();
  void SerializeExternalString();
  void SerializeExternalStringAsSequentialString();

  Serializer* serializer_;
  HeapObject object_;
  SnapshotByteSink* sink_;
  int bytes_processed_so_far_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SERIALIZER_H_
