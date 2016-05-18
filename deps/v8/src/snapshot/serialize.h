// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SERIALIZE_H_
#define V8_SNAPSHOT_SERIALIZE_H_

#include "src/address-map.h"
#include "src/heap/heap.h"
#include "src/objects.h"
#include "src/snapshot/snapshot-source-sink.h"

namespace v8 {
namespace internal {

class Isolate;
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

  // No reservation for large object space necessary.
  static const int kNumberOfPreallocatedSpaces = LAST_PAGED_SPACE + 1;
  static const int kNumberOfSpaces = LAST_SPACE + 1;

 protected:
  static bool CanBeDeferred(HeapObject* o);

  // ---------- byte code range 0x00..0x7f ----------
  // Byte codes in this range represent Where, HowToCode and WhereToPoint.
  // Where the pointed-to object can be found:
  // The static assert below will trigger when the number of preallocated spaces
  // changed. If that happens, update the bytecode ranges in the comments below.
  STATIC_ASSERT(5 == kNumberOfSpaces);
  enum Where {
    // 0x00..0x04  Allocate new object, in specified space.
    kNewObject = 0,
    // 0x05        Unused (including 0x25, 0x45, 0x65).
    // 0x06        Unused (including 0x26, 0x46, 0x66).
    // 0x07        Unused (including 0x27, 0x47, 0x67).
    // 0x08..0x0c  Reference to previous object from space.
    kBackref = 0x08,
    // 0x0d        Unused (including 0x2d, 0x4d, 0x6d).
    // 0x0e        Unused (including 0x2e, 0x4e, 0x6e).
    // 0x0f        Unused (including 0x2f, 0x4f, 0x6f).
    // 0x10..0x14  Reference to previous object from space after skip.
    kBackrefWithSkip = 0x10,
    // 0x15        Unused (including 0x35, 0x55, 0x75).
    // 0x16        Unused (including 0x36, 0x56, 0x76).
    // 0x17        Misc (including 0x37, 0x57, 0x77).
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
  // Deferring object content.
  static const int kDeferred = 0x3f;
  // Used for the source code of the natives, which is in the executable, but
  // is referred to from external strings in the snapshot.
  static const int kNativesStringResource = 0x5d;
  // Used for the source code for compiled stubs, which is in the executable,
  // but is referred to from external strings in the snapshot.
  static const int kExtraNativesStringResource = 0x5e;
  // A tag emitted at strategic points in the snapshot to delineate sections.
  // If the deserializer does not find these at the expected moments then it
  // is an indication that the snapshot and the VM do not fit together.
  // Examine the build process for architecture, version or configuration
  // mismatches.
  static const int kSynchronize = 0x17;
  // Repeats of variable length.
  static const int kVariableRepeat = 0x37;
  // Raw data of variable length.
  static const int kVariableRawData = 0x57;
  // Alignment prefixes 0x7d..0x7f
  static const int kAlignmentPrefix = 0x7d;

  // 0x77 unused

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
        deserializing_user_code_(false),
        next_alignment_(kWordAligned) {
    DecodeReservation(data->Reservations());
  }

  ~Deserializer() override;

  // Deserialize the snapshot into an empty heap.
  void Deserialize(Isolate* isolate);

  // Deserialize a single object and the objects reachable from it.
  MaybeHandle<Object> DeserializePartial(Isolate* isolate,
                                         Handle<JSGlobalProxy> global_proxy);

  // Deserialize a shared function info. Fail gracefully.
  MaybeHandle<SharedFunctionInfo> DeserializeCode(Isolate* isolate);

  // Pass a vector of externally-provided objects referenced by the snapshot.
  // The ownership to its backing store is handed over as well.
  void SetAttachedObjects(Vector<Handle<Object> > attached_objects) {
    attached_objects_ = attached_objects;
  }

 private:
  void VisitPointers(Object** start, Object** end) override;

  void Synchronize(VisitorSynchronization::SyncTag tag) override;

  void VisitRuntimeEntry(RelocInfo* rinfo) override { UNREACHABLE(); }

  void Initialize(Isolate* isolate);

  bool deserializing_user_code() { return deserializing_user_code_; }

  void DecodeReservation(Vector<const SerializedData::Reservation> res);

  bool ReserveSpace();

  void UnalignedCopy(Object** dest, Object** src) {
    memcpy(dest, src, sizeof(*src));
  }

  void SetAlignment(byte data) {
    DCHECK_EQ(kWordAligned, next_alignment_);
    int alignment = data - (kAlignmentPrefix - 1);
    DCHECK_LE(kWordAligned, alignment);
    DCHECK_LE(alignment, kSimd128Unaligned);
    next_alignment_ = static_cast<AllocationAlignment>(alignment);
  }

  void DeserializeDeferredObjects();

  void FlushICacheForNewIsolate();
  void FlushICacheForNewCodeObjects();

  void CommitPostProcessedObjects(Isolate* isolate);

  // Fills in some heap data in an area from start to end (non-inclusive).  The
  // space id is used for the write barrier.  The object_address is the address
  // of the object we are writing into, or NULL if we are not writing into an
  // object, i.e. if we are writing a series of tagged values that are not on
  // the heap. Return false if the object content has been deferred.
  bool ReadData(Object** start, Object** end, int space,
                Address object_address);
  void ReadObject(int space_number, Object** write_back);
  Address Allocate(int space_index, int size);

  // Special handling for serialized code like hooking up internalized strings.
  HeapObject* PostProcessNewObject(HeapObject* obj, int space);

  // This returns the address of an object that has been described in the
  // snapshot by chunk index and offset.
  HeapObject* GetBackReferencedObject(int space);

  Object** CopyInNativesSource(Vector<const char> source_vector,
                               Object** current);

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
  List<Code*> new_code_objects_;
  List<Handle<String> > new_internalized_strings_;
  List<Handle<Script> > new_scripts_;

  bool deserializing_user_code_;

  AllocationAlignment next_alignment_;

  DISALLOW_COPY_AND_ASSIGN(Deserializer);
};


class CodeAddressMap;

// There can be only one serializer per V8 process.
class Serializer : public SerializerDeserializer {
 public:
  Serializer(Isolate* isolate, SnapshotByteSink* sink);
  ~Serializer() override;

  void EncodeReservations(List<SerializedData::Reservation>* out) const;

  void SerializeDeferredObjects();

  Isolate* isolate() const { return isolate_; }

  BackReferenceMap* back_reference_map() { return &back_reference_map_; }
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

  void PutRoot(int index, HeapObject* object, HowToCode how, WhereToPoint where,
               int skip);

  void PutBackReference(HeapObject* object, BackReference reference);

  // Emit alignment prefix if necessary, return required padding space in bytes.
  int PutAlignmentPrefix(HeapObject* object);

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

  void QueueDeferredObject(HeapObject* obj) {
    DCHECK(back_reference_map_.Lookup(obj).is_valid());
    deferred_objects_.Add(obj);
  }

  void OutputStatistics(const char* name);

  Isolate* isolate_;

  SnapshotByteSink* sink_;
  ExternalReferenceEncoder external_reference_encoder_;

  BackReferenceMap back_reference_map_;
  RootIndexMap root_index_map_;

  int recursion_depth_;

  friend class Deserializer;
  friend class ObjectSerializer;
  friend class RecursionScope;
  friend class SnapshotData;

 private:
  void VisitPointers(Object** start, Object** end) override;

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

  // To handle stack overflow.
  List<HeapObject*> deferred_objects_;

#ifdef OBJECT_PRINT
  static const int kInstanceTypes = 256;
  int* instance_type_count_;
  size_t* instance_type_size_;
#endif  // OBJECT_PRINT

  DISALLOW_COPY_AND_ASSIGN(Serializer);
};


class PartialSerializer : public Serializer {
 public:
  PartialSerializer(Isolate* isolate, Serializer* startup_snapshot_serializer,
                    SnapshotByteSink* sink)
      : Serializer(isolate, sink),
        startup_serializer_(startup_snapshot_serializer),
        global_object_(NULL) {
    InitializeCodeAddressMap();
  }

  ~PartialSerializer() override { OutputStatistics("PartialSerializer"); }

  // Serialize the objects reachable from a single object pointer.
  void Serialize(Object** o);

 private:
  void SerializeObject(HeapObject* o, HowToCode how_to_code,
                       WhereToPoint where_to_point, int skip) override;

  int PartialSnapshotCacheIndex(HeapObject* o);
  bool ShouldBeInThePartialSnapshotCache(HeapObject* o);

  Serializer* startup_serializer_;
  Object* global_object_;
  PartialCacheIndexMap partial_cache_index_map_;
  DISALLOW_COPY_AND_ASSIGN(PartialSerializer);
};


class StartupSerializer : public Serializer {
 public:
  StartupSerializer(Isolate* isolate, SnapshotByteSink* sink);
  ~StartupSerializer() override { OutputStatistics("StartupSerializer"); }

  // Serialize the current state of the heap.  The order is:
  // 1) Strong references.
  // 2) Partial snapshot cache.
  // 3) Weak references (e.g. the string table).
  void SerializeStrongReferences();
  void SerializeWeakReferencesAndDeferred();

 private:
  // The StartupSerializer has to serialize the root array, which is slightly
  // different.
  void VisitPointers(Object** start, Object** end) override;
  void SerializeObject(HeapObject* o, HowToCode how_to_code,
                       WhereToPoint where_to_point, int skip) override;
  void Synchronize(VisitorSynchronization::SyncTag tag) override;

  intptr_t root_index_wave_front_;
  bool serializing_builtins_;
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

 private:
  CodeSerializer(Isolate* isolate, SnapshotByteSink* sink, String* source)
      : Serializer(isolate, sink), source_(source) {
    back_reference_map_.AddSourceString(source);
  }

  ~CodeSerializer() override { OutputStatistics("CodeSerializer"); }

  void SerializeObject(HeapObject* o, HowToCode how_to_code,
                       WhereToPoint where_to_point, int skip) override;

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

  uint32_t SourceHash(String* source) const;

  // The data header consists of uint32_t-sized entries:
  // [0] magic number and external reference count
  // [1] version hash
  // [2] source hash
  // [3] cpu features
  // [4] flag hash
  // [5] number of code stub keys
  // [6] number of reservation size entries
  // [7] payload length
  // [8] payload checksum part 1
  // [9] payload checksum part 2
  // ...  reservations
  // ...  code stub keys
  // ...  serialized payload
  static const int kVersionHashOffset = kMagicNumberOffset + kInt32Size;
  static const int kSourceHashOffset = kVersionHashOffset + kInt32Size;
  static const int kCpuFeaturesOffset = kSourceHashOffset + kInt32Size;
  static const int kFlagHashOffset = kCpuFeaturesOffset + kInt32Size;
  static const int kNumReservationsOffset = kFlagHashOffset + kInt32Size;
  static const int kNumCodeStubKeysOffset = kNumReservationsOffset + kInt32Size;
  static const int kPayloadLengthOffset = kNumCodeStubKeysOffset + kInt32Size;
  static const int kChecksum1Offset = kPayloadLengthOffset + kInt32Size;
  static const int kChecksum2Offset = kChecksum1Offset + kInt32Size;
  static const int kHeaderSize = kChecksum2Offset + kInt32Size;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SERIALIZE_H_
