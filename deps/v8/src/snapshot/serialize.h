// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SERIALIZE_H_
#define V8_SERIALIZE_H_

#include "src/hashmap.h"
#include "src/heap-profiler.h"
#include "src/isolate.h"
#include "src/snapshot/snapshot-source-sink.h"

namespace v8 {
namespace internal {

class ScriptData;

static const int kDeoptTableSerializeEntryCount = 64;

// ExternalReferenceTable is a helper class that defines the relationship
// between external references and their encodings. It is used to build
// hashmaps in ExternalReferenceEncoder and ExternalReferenceDecoder.
class ExternalReferenceTable {
 public:
  static ExternalReferenceTable* instance(Isolate* isolate);

  int size() const { return refs_.length(); }
  Address address(int i) { return refs_[i].address; }
  const char* name(int i) { return refs_[i].name; }

  inline static Address NotAvailable() { return NULL; }

 private:
  struct ExternalReferenceEntry {
    Address address;
    const char* name;
  };

  explicit ExternalReferenceTable(Isolate* isolate);

  void Add(Address address, const char* name) {
    ExternalReferenceEntry entry = {address, name};
    refs_.Add(entry);
  }

  List<ExternalReferenceEntry> refs_;

  DISALLOW_COPY_AND_ASSIGN(ExternalReferenceTable);
};


class ExternalReferenceEncoder {
 public:
  explicit ExternalReferenceEncoder(Isolate* isolate);

  uint32_t Encode(Address key) const;

  const char* NameOfAddress(Isolate* isolate, Address address) const;

 private:
  static uint32_t Hash(Address key) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(key) >>
                                 kPointerSizeLog2);
  }

  HashMap* map_;

  DISALLOW_COPY_AND_ASSIGN(ExternalReferenceEncoder);
};


class AddressMapBase {
 protected:
  static void SetValue(HashMap::Entry* entry, uint32_t v) {
    entry->value = reinterpret_cast<void*>(v);
  }

  static uint32_t GetValue(HashMap::Entry* entry) {
    return static_cast<uint32_t>(reinterpret_cast<intptr_t>(entry->value));
  }

  inline static HashMap::Entry* LookupEntry(HashMap* map, HeapObject* obj,
                                            bool insert) {
    return map->Lookup(Key(obj), Hash(obj), insert);
  }

 private:
  static uint32_t Hash(HeapObject* obj) {
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(obj->address()));
  }

  static void* Key(HeapObject* obj) {
    return reinterpret_cast<void*>(obj->address());
  }
};


class RootIndexMap : public AddressMapBase {
 public:
  explicit RootIndexMap(Isolate* isolate);

  static const int kInvalidRootIndex = -1;

  int Lookup(HeapObject* obj) {
    HashMap::Entry* entry = LookupEntry(map_, obj, false);
    if (entry) return GetValue(entry);
    return kInvalidRootIndex;
  }

 private:
  HashMap* map_;

  DISALLOW_COPY_AND_ASSIGN(RootIndexMap);
};


class PartialCacheIndexMap : public AddressMapBase {
 public:
  PartialCacheIndexMap() : map_(HashMap::PointersMatch) {}

  static const int kInvalidIndex = -1;

  // Lookup object in the map. Return its index if found, or create
  // a new entry with new_index as value, and return kInvalidIndex.
  int LookupOrInsert(HeapObject* obj, int new_index) {
    HashMap::Entry* entry = LookupEntry(&map_, obj, false);
    if (entry != NULL) return GetValue(entry);
    SetValue(LookupEntry(&map_, obj, true), static_cast<uint32_t>(new_index));
    return kInvalidIndex;
  }

 private:
  HashMap map_;

  DISALLOW_COPY_AND_ASSIGN(PartialCacheIndexMap);
};


class BackReference {
 public:
  explicit BackReference(uint32_t bitfield) : bitfield_(bitfield) {}

  BackReference() : bitfield_(kInvalidValue) {}

  static BackReference SourceReference() { return BackReference(kSourceValue); }

  static BackReference GlobalProxyReference() {
    return BackReference(kGlobalProxyValue);
  }

  static BackReference LargeObjectReference(uint32_t index) {
    return BackReference(SpaceBits::encode(LO_SPACE) |
                         ChunkOffsetBits::encode(index));
  }

  static BackReference Reference(AllocationSpace space, uint32_t chunk_index,
                                 uint32_t chunk_offset) {
    DCHECK(IsAligned(chunk_offset, kObjectAlignment));
    DCHECK_NE(LO_SPACE, space);
    return BackReference(
        SpaceBits::encode(space) | ChunkIndexBits::encode(chunk_index) |
        ChunkOffsetBits::encode(chunk_offset >> kObjectAlignmentBits));
  }

  bool is_valid() const { return bitfield_ != kInvalidValue; }
  bool is_source() const { return bitfield_ == kSourceValue; }
  bool is_global_proxy() const { return bitfield_ == kGlobalProxyValue; }

  AllocationSpace space() const {
    DCHECK(is_valid());
    return SpaceBits::decode(bitfield_);
  }

  uint32_t chunk_offset() const {
    DCHECK(is_valid());
    return ChunkOffsetBits::decode(bitfield_) << kObjectAlignmentBits;
  }

  uint32_t large_object_index() const {
    DCHECK(is_valid());
    DCHECK(chunk_index() == 0);
    return ChunkOffsetBits::decode(bitfield_);
  }

  uint32_t chunk_index() const {
    DCHECK(is_valid());
    return ChunkIndexBits::decode(bitfield_);
  }

  uint32_t reference() const {
    DCHECK(is_valid());
    return bitfield_ & (ChunkOffsetBits::kMask | ChunkIndexBits::kMask);
  }

  uint32_t bitfield() const { return bitfield_; }

 private:
  static const uint32_t kInvalidValue = 0xFFFFFFFF;
  static const uint32_t kSourceValue = 0xFFFFFFFE;
  static const uint32_t kGlobalProxyValue = 0xFFFFFFFD;
  static const int kChunkOffsetSize = kPageSizeBits - kObjectAlignmentBits;
  static const int kChunkIndexSize = 32 - kChunkOffsetSize - kSpaceTagSize;

 public:
  static const int kMaxChunkIndex = (1 << kChunkIndexSize) - 1;

 private:
  class ChunkOffsetBits : public BitField<uint32_t, 0, kChunkOffsetSize> {};
  class ChunkIndexBits
      : public BitField<uint32_t, ChunkOffsetBits::kNext, kChunkIndexSize> {};
  class SpaceBits
      : public BitField<AllocationSpace, ChunkIndexBits::kNext, kSpaceTagSize> {
  };

  uint32_t bitfield_;
};


// Mapping objects to their location after deserialization.
// This is used during building, but not at runtime by V8.
class BackReferenceMap : public AddressMapBase {
 public:
  BackReferenceMap()
      : no_allocation_(), map_(new HashMap(HashMap::PointersMatch)) {}

  ~BackReferenceMap() { delete map_; }

  BackReference Lookup(HeapObject* obj) {
    HashMap::Entry* entry = LookupEntry(map_, obj, false);
    return entry ? BackReference(GetValue(entry)) : BackReference();
  }

  void Add(HeapObject* obj, BackReference b) {
    DCHECK(b.is_valid());
    DCHECK_NULL(LookupEntry(map_, obj, false));
    HashMap::Entry* entry = LookupEntry(map_, obj, true);
    SetValue(entry, b.bitfield());
  }

  void AddSourceString(String* string) {
    Add(string, BackReference::SourceReference());
  }

  void AddGlobalProxy(HeapObject* global_proxy) {
    Add(global_proxy, BackReference::GlobalProxyReference());
  }

 private:
  DisallowHeapAllocation no_allocation_;
  HashMap* map_;
  DISALLOW_COPY_AND_ASSIGN(BackReferenceMap);
};


class HotObjectsList {
 public:
  HotObjectsList() : index_(0) {
    for (int i = 0; i < kSize; i++) circular_queue_[i] = NULL;
  }

  void Add(HeapObject* object) {
    circular_queue_[index_] = object;
    index_ = (index_ + 1) & kSizeMask;
  }

  HeapObject* Get(int index) {
    DCHECK_NOT_NULL(circular_queue_[index]);
    return circular_queue_[index];
  }

  static const int kNotFound = -1;

  int Find(HeapObject* object) {
    for (int i = 0; i < kSize; i++) {
      if (circular_queue_[i] == object) return i;
    }
    return kNotFound;
  }

  static const int kSize = 8;

 private:
  STATIC_ASSERT(IS_POWER_OF_TWO(kSize));
  static const int kSizeMask = kSize - 1;
  HeapObject* circular_queue_[kSize];
  int index_;

  DISALLOW_COPY_AND_ASSIGN(HotObjectsList);
};


// The Serializer/Deserializer class is a common superclass for Serializer and
// Deserializer which is used to store common constants and methods used by
// both.
class SerializerDeserializer: public ObjectVisitor {
 public:
  static void Iterate(Isolate* isolate, ObjectVisitor* visitor);

  static int nop() { return kNop; }

  // No reservation for large object space necessary.
  static const int kNumberOfPreallocatedSpaces = LO_SPACE;
  static const int kNumberOfSpaces = LAST_SPACE + 1;

 protected:
  // ---------- byte code range 0x00..0x7f ----------
  // Byte codes in this range represent Where, HowToCode and WhereToPoint.
  // Where the pointed-to object can be found:
  enum Where {
    // 0x00..0x05  Allocate new object, in specified space.
    kNewObject = 0,
    // 0x06        Unused (including 0x26, 0x46, 0x66).
    // 0x07        Unused (including 0x27, 0x47, 0x67).
    // 0x08..0x0d  Reference to previous object from space.
    kBackref = 0x08,
    // 0x0e        Unused (including 0x2e, 0x4e, 0x6e).
    // 0x0f        Unused (including 0x2f, 0x4f, 0x6f).
    // 0x10..0x15  Reference to previous object from space after skip.
    kBackrefWithSkip = 0x10,
    // 0x16        Unused (including 0x36, 0x56, 0x76).
    // 0x17        Unused (including 0x37, 0x57, 0x77).
    // 0x18        Root array item.
    kRootArray = 0x18,
    // 0x19        Object in the partial snapshot cache.
    kPartialSnapshotCache = 0x19,
    // 0x1a        External reference referenced by id.
    kExternalReference = 0x1a,
    // 0x1b        Object provided in the attached list.
    kAttachedReference = 0x1b,
    // 0x1c        Builtin code referenced by index.
    kBuiltin = 0x1c
    // 0x1d..0x1f  Misc (including 0x3d..0x3f, 0x5d..0x5f, 0x7d..0x7f)
  };

  static const int kWhereMask = 0x1f;
  static const int kSpaceMask = 7;
  STATIC_ASSERT(kNumberOfSpaces <= kSpaceMask + 1);

  // How to code the pointer to the object.
  enum HowToCode {
    // Straight pointer.
    kPlain = 0,
    // A pointer inlined in code. What this means depends on the architecture.
    kFromCode = 0x20
  };

  static const int kHowToCodeMask = 0x20;

  // Where to point within the object.
  enum WhereToPoint {
    // Points to start of object
    kStartOfObject = 0,
    // Points to instruction in code object or payload of cell.
    kInnerPointer = 0x40
  };

  static const int kWhereToPointMask = 0x40;

  // ---------- Misc ----------
  // Skip.
  static const int kSkip = 0x1d;
  // Internal reference encoded as offsets of pc and target from code entry.
  static const int kInternalReference = 0x1e;
  static const int kInternalReferenceEncoded = 0x1f;
  // Do nothing, used for padding.
  static const int kNop = 0x3d;
  // Move to next reserved chunk.
  static const int kNextChunk = 0x3e;
  // A tag emitted at strategic points in the snapshot to delineate sections.
  // If the deserializer does not find these at the expected moments then it
  // is an indication that the snapshot and the VM do not fit together.
  // Examine the build process for architecture, version or configuration
  // mismatches.
  static const int kSynchronize = 0x5d;
  // Used for the source code of the natives, which is in the executable, but
  // is referred to from external strings in the snapshot.
  static const int kNativesStringResource = 0x5e;
  // Raw data of variable length.
  static const int kVariableRawData = 0x7d;
  // Repeats of variable length.
  static const int kVariableRepeat = 0x7e;

  // ---------- byte code range 0x80..0xff ----------
  // First 32 root array items.
  static const int kNumberOfRootArrayConstants = 0x20;
  // 0x80..0x9f
  static const int kRootArrayConstants = 0x80;
  // 0xa0..0xbf
  static const int kRootArrayConstantsWithSkip = 0xa0;
  static const int kRootArrayConstantsMask = 0x1f;

  // 8 hot (recently seen or back-referenced) objects with optional skip.
  static const int kNumberOfHotObjects = 0x08;
  // 0xc0..0xc7
  static const int kHotObject = 0xc0;
  // 0xc8..0xcf
  static const int kHotObjectWithSkip = 0xc8;
  static const int kHotObjectMask = 0x07;

  // 32 common raw data lengths.
  static const int kNumberOfFixedRawData = 0x20;
  // 0xd0..0xef
  static const int kFixedRawData = 0xd0;
  static const int kOnePointerRawData = kFixedRawData;
  static const int kFixedRawDataStart = kFixedRawData - 1;

  // 16 repeats lengths.
  static const int kNumberOfFixedRepeat = 0x10;
  // 0xf0..0xff
  static const int kFixedRepeat = 0xf0;
  static const int kFixedRepeatStart = kFixedRepeat - 1;

  // ---------- special values ----------
  static const int kAnyOldSpace = -1;

  // Sentinel after a new object to indicate that double alignment is needed.
  static const int kDoubleAlignmentSentinel = 0;

  // Used as index for the attached reference representing the source object.
  static const int kSourceObjectReference = 0;

  // Used as index for the attached reference representing the global proxy.
  static const int kGlobalProxyReference = 0;

  // ---------- member variable ----------
  HotObjectsList hot_objects_;
};


class SerializedData {
 public:
  class Reservation {
   public:
    explicit Reservation(uint32_t size)
        : reservation_(ChunkSizeBits::encode(size)) {}

    uint32_t chunk_size() const { return ChunkSizeBits::decode(reservation_); }
    bool is_last() const { return IsLastChunkBits::decode(reservation_); }

    void mark_as_last() { reservation_ |= IsLastChunkBits::encode(true); }

   private:
    uint32_t reservation_;
  };

  SerializedData(byte* data, int size)
      : data_(data), size_(size), owns_data_(false) {}
  SerializedData() : data_(NULL), size_(0), owns_data_(false) {}

  ~SerializedData() {
    if (owns_data_) DeleteArray<byte>(data_);
  }

  uint32_t GetMagicNumber() const { return GetHeaderValue(kMagicNumberOffset); }

  class ChunkSizeBits : public BitField<uint32_t, 0, 31> {};
  class IsLastChunkBits : public BitField<bool, 31, 1> {};

  static uint32_t ComputeMagicNumber(ExternalReferenceTable* table) {
    uint32_t external_refs = table->size();
    return 0xC0DE0000 ^ external_refs;
  }

 protected:
  void SetHeaderValue(int offset, uint32_t value) {
    uint32_t* address = reinterpret_cast<uint32_t*>(data_ + offset);
    memcpy(reinterpret_cast<uint32_t*>(address), &value, sizeof(value));
  }

  uint32_t GetHeaderValue(int offset) const {
    uint32_t value;
    memcpy(&value, reinterpret_cast<int*>(data_ + offset), sizeof(value));
    return value;
  }

  void AllocateData(int size);

  static uint32_t ComputeMagicNumber(Isolate* isolate) {
    return ComputeMagicNumber(ExternalReferenceTable::instance(isolate));
  }

  void SetMagicNumber(Isolate* isolate) {
    SetHeaderValue(kMagicNumberOffset, ComputeMagicNumber(isolate));
  }

  static const int kMagicNumberOffset = 0;

  byte* data_;
  int size_;
  bool owns_data_;
};


// A Deserializer reads a snapshot and reconstructs the Object graph it defines.
class Deserializer: public SerializerDeserializer {
 public:
  // Create a deserializer from a snapshot byte source.
  template <class Data>
  explicit Deserializer(Data* data)
      : isolate_(NULL),
        source_(data->Payload()),
        magic_number_(data->GetMagicNumber()),
        external_reference_table_(NULL),
        deserialized_large_objects_(0),
        deserializing_user_code_(false) {
    DecodeReservation(data->Reservations());
  }

  virtual ~Deserializer();

  // Deserialize the snapshot into an empty heap.
  void Deserialize(Isolate* isolate);

  // Deserialize a single object and the objects reachable from it.
  MaybeHandle<Object> DeserializePartial(
      Isolate* isolate, Handle<JSGlobalProxy> global_proxy,
      Handle<FixedArray>* outdated_contexts_out);

  // Deserialize a shared function info. Fail gracefully.
  MaybeHandle<SharedFunctionInfo> DeserializeCode(Isolate* isolate);

  void FlushICacheForNewCodeObjects();

  // Pass a vector of externally-provided objects referenced by the snapshot.
  // The ownership to its backing store is handed over as well.
  void SetAttachedObjects(Vector<Handle<Object> > attached_objects) {
    attached_objects_ = attached_objects;
  }

 private:
  virtual void VisitPointers(Object** start, Object** end);

  virtual void VisitRuntimeEntry(RelocInfo* rinfo) {
    UNREACHABLE();
  }

  void Initialize(Isolate* isolate);

  bool deserializing_user_code() { return deserializing_user_code_; }

  void DecodeReservation(Vector<const SerializedData::Reservation> res);

  bool ReserveSpace();

  void UnalignedCopy(Object** dest, Object** src) {
    memcpy(dest, src, sizeof(*src));
  }

  // Allocation sites are present in the snapshot, and must be linked into
  // a list at deserialization time.
  void RelinkAllocationSite(AllocationSite* site);

  // Fills in some heap data in an area from start to end (non-inclusive).  The
  // space id is used for the write barrier.  The object_address is the address
  // of the object we are writing into, or NULL if we are not writing into an
  // object, i.e. if we are writing a series of tagged values that are not on
  // the heap.
  void ReadData(Object** start, Object** end, int space,
                Address object_address);
  void ReadObject(int space_number, Object** write_back);
  Address Allocate(int space_index, int size);

  // Special handling for serialized code like hooking up internalized strings.
  HeapObject* ProcessNewObjectFromSerializedCode(HeapObject* obj);

  // This returns the address of an object that has been described in the
  // snapshot by chunk index and offset.
  HeapObject* GetBackReferencedObject(int space);

  // Cached current isolate.
  Isolate* isolate_;

  // Objects from the attached object descriptions in the serialized user code.
  Vector<Handle<Object> > attached_objects_;

  SnapshotByteSource source_;
  uint32_t magic_number_;

  // The address of the next object that will be allocated in each space.
  // Each space has a number of chunks reserved by the GC, with each chunk
  // fitting into a page. Deserialized objects are allocated into the
  // current chunk of the target space by bumping up high water mark.
  Heap::Reservation reservations_[kNumberOfSpaces];
  uint32_t current_chunk_[kNumberOfPreallocatedSpaces];
  Address high_water_[kNumberOfPreallocatedSpaces];

  ExternalReferenceTable* external_reference_table_;

  List<HeapObject*> deserialized_large_objects_;

  bool deserializing_user_code_;

  DISALLOW_COPY_AND_ASSIGN(Deserializer);
};


class CodeAddressMap;

// There can be only one serializer per V8 process.
class Serializer : public SerializerDeserializer {
 public:
  Serializer(Isolate* isolate, SnapshotByteSink* sink);
  ~Serializer();
  void VisitPointers(Object** start, Object** end) OVERRIDE;

  void EncodeReservations(List<SerializedData::Reservation>* out) const;

  Isolate* isolate() const { return isolate_; }

  BackReferenceMap* back_reference_map() { return &back_reference_map_; }
  RootIndexMap* root_index_map() { return &root_index_map_; }

 protected:
  class ObjectSerializer : public ObjectVisitor {
   public:
    ObjectSerializer(Serializer* serializer, Object* o, SnapshotByteSink* sink,
                     HowToCode how_to_code, WhereToPoint where_to_point)
        : serializer_(serializer),
          object_(HeapObject::cast(o)),
          sink_(sink),
          reference_representation_(how_to_code + where_to_point),
          bytes_processed_so_far_(0),
          is_code_object_(o->IsCode()),
          code_has_been_output_(false) {}
    void Serialize();
    void VisitPointers(Object** start, Object** end);
    void VisitEmbeddedPointer(RelocInfo* target);
    void VisitExternalReference(Address* p);
    void VisitExternalReference(RelocInfo* rinfo);
    void VisitInternalReference(RelocInfo* rinfo);
    void VisitCodeTarget(RelocInfo* target);
    void VisitCodeEntry(Address entry_address);
    void VisitCell(RelocInfo* rinfo);
    void VisitRuntimeEntry(RelocInfo* reloc);
    // Used for seralizing the external strings that hold the natives source.
    void VisitExternalOneByteString(
        v8::String::ExternalOneByteStringResource** resource);
    // We can't serialize a heap with external two byte strings.
    void VisitExternalTwoByteString(
        v8::String::ExternalStringResource** resource) {
      UNREACHABLE();
    }

   private:
    void SerializePrologue(AllocationSpace space, int size, Map* map);

    enum ReturnSkip { kCanReturnSkipInsteadOfSkipping, kIgnoringReturn };
    // This function outputs or skips the raw data between the last pointer and
    // up to the current position.  It optionally can just return the number of
    // bytes to skip instead of performing a skip instruction, in case the skip
    // can be merged into the next instruction.
    int OutputRawData(Address up_to, ReturnSkip return_skip = kIgnoringReturn);
    // External strings are serialized in a way to resemble sequential strings.
    void SerializeExternalString();

    Address PrepareCode();

    Serializer* serializer_;
    HeapObject* object_;
    SnapshotByteSink* sink_;
    int reference_representation_;
    int bytes_processed_so_far_;
    bool is_code_object_;
    bool code_has_been_output_;
  };

  virtual void SerializeObject(HeapObject* o, HowToCode how_to_code,
                               WhereToPoint where_to_point, int skip) = 0;

  void PutRoot(int index, HeapObject* object, HowToCode how, WhereToPoint where,
               int skip);

  // Returns true if the object was successfully serialized.
  bool SerializeKnownObject(HeapObject* obj, HowToCode how_to_code,
                            WhereToPoint where_to_point, int skip);

  inline void FlushSkip(int skip) {
    if (skip != 0) {
      sink_->Put(kSkip, "SkipFromSerializeObject");
      sink_->PutInt(skip, "SkipDistanceFromSerializeObject");
    }
  }

  bool BackReferenceIsAlreadyAllocated(BackReference back_reference);

  // This will return the space for an object.
  BackReference AllocateLargeObject(int size);
  BackReference Allocate(AllocationSpace space, int size);
  int EncodeExternalReference(Address addr) {
    return external_reference_encoder_.Encode(addr);
  }

  // GetInt reads 4 bytes at once, requiring padding at the end.
  void Pad();

  // Some roots should not be serialized, because their actual value depends on
  // absolute addresses and they are reset after deserialization, anyway.
  bool ShouldBeSkipped(Object** current);

  // We may not need the code address map for logging for every instance
  // of the serializer.  Initialize it on demand.
  void InitializeCodeAddressMap();

  Code* CopyCode(Code* code);

  inline uint32_t max_chunk_size(int space) const {
    DCHECK_LE(0, space);
    DCHECK_LT(space, kNumberOfSpaces);
    return max_chunk_size_[space];
  }

  SnapshotByteSink* sink() const { return sink_; }

  Isolate* isolate_;

  SnapshotByteSink* sink_;
  ExternalReferenceEncoder external_reference_encoder_;

  BackReferenceMap back_reference_map_;
  RootIndexMap root_index_map_;

  friend class Deserializer;
  friend class ObjectSerializer;
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

  // We map serialized large objects to indexes for back-referencing.
  uint32_t large_objects_total_size_;
  uint32_t seen_large_objects_index_;

  List<byte> code_buffer_;

  DISALLOW_COPY_AND_ASSIGN(Serializer);
};


class PartialSerializer : public Serializer {
 public:
  PartialSerializer(Isolate* isolate, Serializer* startup_snapshot_serializer,
                    SnapshotByteSink* sink)
      : Serializer(isolate, sink),
        startup_serializer_(startup_snapshot_serializer),
        outdated_contexts_(0),
        global_object_(NULL) {
    InitializeCodeAddressMap();
  }

  // Serialize the objects reachable from a single object pointer.
  void Serialize(Object** o);
  virtual void SerializeObject(HeapObject* o, HowToCode how_to_code,
                               WhereToPoint where_to_point, int skip) OVERRIDE;

 private:
  int PartialSnapshotCacheIndex(HeapObject* o);
  bool ShouldBeInThePartialSnapshotCache(HeapObject* o) {
    // Scripts should be referred only through shared function infos.  We can't
    // allow them to be part of the partial snapshot because they contain a
    // unique ID, and deserializing several partial snapshots containing script
    // would cause dupes.
    DCHECK(!o->IsScript());
    return o->IsName() || o->IsSharedFunctionInfo() ||
           o->IsHeapNumber() || o->IsCode() ||
           o->IsScopeInfo() ||
           o->map() ==
               startup_serializer_->isolate()->heap()->fixed_cow_array_map();
  }

  void SerializeOutdatedContextsAsFixedArray();

  Serializer* startup_serializer_;
  List<BackReference> outdated_contexts_;
  Object* global_object_;
  PartialCacheIndexMap partial_cache_index_map_;
  DISALLOW_COPY_AND_ASSIGN(PartialSerializer);
};


class StartupSerializer : public Serializer {
 public:
  StartupSerializer(Isolate* isolate, SnapshotByteSink* sink)
      : Serializer(isolate, sink), root_index_wave_front_(0) {
    // Clear the cache of objects used by the partial snapshot.  After the
    // strong roots have been serialized we can create a partial snapshot
    // which will repopulate the cache with objects needed by that partial
    // snapshot.
    isolate->partial_snapshot_cache()->Clear();
    InitializeCodeAddressMap();
  }

  // The StartupSerializer has to serialize the root array, which is slightly
  // different.
  void VisitPointers(Object** start, Object** end) OVERRIDE;

  // Serialize the current state of the heap.  The order is:
  // 1) Strong references.
  // 2) Partial snapshot cache.
  // 3) Weak references (e.g. the string table).
  virtual void SerializeStrongReferences();
  virtual void SerializeObject(HeapObject* o, HowToCode how_to_code,
                               WhereToPoint where_to_point, int skip) OVERRIDE;
  void SerializeWeakReferences();
  void Serialize() {
    SerializeStrongReferences();
    SerializeWeakReferences();
    Pad();
  }

 private:
  intptr_t root_index_wave_front_;
  DISALLOW_COPY_AND_ASSIGN(StartupSerializer);
};


class CodeSerializer : public Serializer {
 public:
  static ScriptData* Serialize(Isolate* isolate,
                               Handle<SharedFunctionInfo> info,
                               Handle<String> source);

  MUST_USE_RESULT static MaybeHandle<SharedFunctionInfo> Deserialize(
      Isolate* isolate, ScriptData* cached_data, Handle<String> source);

  static const int kSourceObjectIndex = 0;
  STATIC_ASSERT(kSourceObjectReference == kSourceObjectIndex);

  static const int kCodeStubsBaseIndex = 1;

  String* source() const {
    DCHECK(!AllowHeapAllocation::IsAllowed());
    return source_;
  }

  const List<uint32_t>* stub_keys() const { return &stub_keys_; }
  int num_internalized_strings() const { return num_internalized_strings_; }

 private:
  CodeSerializer(Isolate* isolate, SnapshotByteSink* sink, String* source,
                 Code* main_code)
      : Serializer(isolate, sink),
        source_(source),
        main_code_(main_code),
        num_internalized_strings_(0) {
    back_reference_map_.AddSourceString(source);
  }

  virtual void SerializeObject(HeapObject* o, HowToCode how_to_code,
                               WhereToPoint where_to_point, int skip) OVERRIDE;

  void SerializeBuiltin(int builtin_index, HowToCode how_to_code,
                        WhereToPoint where_to_point);
  void SerializeIC(Code* ic, HowToCode how_to_code,
                   WhereToPoint where_to_point);
  void SerializeCodeStub(uint32_t stub_key, HowToCode how_to_code,
                         WhereToPoint where_to_point);
  void SerializeGeneric(HeapObject* heap_object, HowToCode how_to_code,
                        WhereToPoint where_to_point);
  int AddCodeStubKey(uint32_t stub_key);

  DisallowHeapAllocation no_gc_;
  String* source_;
  Code* main_code_;
  int num_internalized_strings_;
  List<uint32_t> stub_keys_;
  DISALLOW_COPY_AND_ASSIGN(CodeSerializer);
};


// Wrapper around reservation sizes and the serialization payload.
class SnapshotData : public SerializedData {
 public:
  // Used when producing.
  explicit SnapshotData(const Serializer& ser);

  // Used when consuming.
  explicit SnapshotData(const Vector<const byte> snapshot)
      : SerializedData(const_cast<byte*>(snapshot.begin()), snapshot.length()) {
    CHECK(IsSane());
  }

  Vector<const Reservation> Reservations() const;
  Vector<const byte> Payload() const;

  Vector<const byte> RawData() const {
    return Vector<const byte>(data_, size_);
  }

 private:
  bool IsSane();

  // The data header consists of uint32_t-sized entries:
  // [0] magic number and external reference count
  // [1] version hash
  // [2] number of reservation size entries
  // [3] payload length
  // ... reservations
  // ... serialized payload
  static const int kCheckSumOffset = kMagicNumberOffset + kInt32Size;
  static const int kNumReservationsOffset = kCheckSumOffset + kInt32Size;
  static const int kPayloadLengthOffset = kNumReservationsOffset + kInt32Size;
  static const int kHeaderSize = kPayloadLengthOffset + kInt32Size;
};


// Wrapper around ScriptData to provide code-serializer-specific functionality.
class SerializedCodeData : public SerializedData {
 public:
  // Used when consuming.
  static SerializedCodeData* FromCachedData(Isolate* isolate,
                                            ScriptData* cached_data,
                                            String* source);

  // Used when producing.
  SerializedCodeData(const List<byte>& payload, const CodeSerializer& cs);

  // Return ScriptData object and relinquish ownership over it to the caller.
  ScriptData* GetScriptData();

  Vector<const Reservation> Reservations() const;
  Vector<const byte> Payload() const;

  int NumInternalizedStrings() const;
  Vector<const uint32_t> CodeStubKeys() const;

 private:
  explicit SerializedCodeData(ScriptData* data);

  enum SanityCheckResult {
    CHECK_SUCCESS = 0,
    MAGIC_NUMBER_MISMATCH = 1,
    VERSION_MISMATCH = 2,
    SOURCE_MISMATCH = 3,
    CPU_FEATURES_MISMATCH = 4,
    FLAGS_MISMATCH = 5,
    CHECKSUM_MISMATCH = 6
  };

  SanityCheckResult SanityCheck(Isolate* isolate, String* source) const;

  uint32_t SourceHash(String* source) const { return source->length(); }

  // The data header consists of uint32_t-sized entries:
  // [ 0] magic number and external reference count
  // [ 1] version hash
  // [ 2] source hash
  // [ 3] cpu features
  // [ 4] flag hash
  // [ 5] number of internalized strings
  // [ 6] number of code stub keys
  // [ 7] number of reservation size entries
  // [ 8] payload length
  // [ 9] payload checksum part 1
  // [10] payload checksum part 2
  // ...  reservations
  // ...  code stub keys
  // ...  serialized payload
  static const int kVersionHashOffset = kMagicNumberOffset + kInt32Size;
  static const int kSourceHashOffset = kVersionHashOffset + kInt32Size;
  static const int kCpuFeaturesOffset = kSourceHashOffset + kInt32Size;
  static const int kFlagHashOffset = kCpuFeaturesOffset + kInt32Size;
  static const int kNumInternalizedStringsOffset = kFlagHashOffset + kInt32Size;
  static const int kNumReservationsOffset =
      kNumInternalizedStringsOffset + kInt32Size;
  static const int kNumCodeStubKeysOffset = kNumReservationsOffset + kInt32Size;
  static const int kPayloadLengthOffset = kNumCodeStubKeysOffset + kInt32Size;
  static const int kChecksum1Offset = kPayloadLengthOffset + kInt32Size;
  static const int kChecksum2Offset = kChecksum1Offset + kInt32Size;
  static const int kHeaderSize = kChecksum2Offset + kInt32Size;
};
} }  // namespace v8::internal

#endif  // V8_SERIALIZE_H_
