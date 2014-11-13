// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SERIALIZE_H_
#define V8_SERIALIZE_H_

#include "src/compiler.h"
#include "src/hashmap.h"
#include "src/heap-profiler.h"
#include "src/isolate.h"
#include "src/snapshot-source-sink.h"

namespace v8 {
namespace internal {

// A TypeCode is used to distinguish different kinds of external reference.
// It is a single bit to make testing for types easy.
enum TypeCode {
  UNCLASSIFIED,  // One-of-a-kind references.
  C_BUILTIN,
  BUILTIN,
  RUNTIME_FUNCTION,
  IC_UTILITY,
  STATS_COUNTER,
  TOP_ADDRESS,
  ACCESSOR,
  STUB_CACHE_TABLE,
  RUNTIME_ENTRY,
  LAZY_DEOPTIMIZATION
};

const int kTypeCodeCount = LAZY_DEOPTIMIZATION + 1;
const int kFirstTypeCode = UNCLASSIFIED;

const int kReferenceIdBits = 16;
const int kReferenceIdMask = (1 << kReferenceIdBits) - 1;
const int kReferenceTypeShift = kReferenceIdBits;

const int kDeoptTableSerializeEntryCount = 64;

// ExternalReferenceTable is a helper class that defines the relationship
// between external references and their encodings. It is used to build
// hashmaps in ExternalReferenceEncoder and ExternalReferenceDecoder.
class ExternalReferenceTable {
 public:
  static ExternalReferenceTable* instance(Isolate* isolate);

  ~ExternalReferenceTable() { }

  int size() const { return refs_.length(); }

  Address address(int i) { return refs_[i].address; }

  uint32_t code(int i) { return refs_[i].code; }

  const char* name(int i) { return refs_[i].name; }

  int max_id(int code) { return max_id_[code]; }

 private:
  explicit ExternalReferenceTable(Isolate* isolate) : refs_(64) {
    PopulateTable(isolate);
  }

  struct ExternalReferenceEntry {
    Address address;
    uint32_t code;
    const char* name;
  };

  void PopulateTable(Isolate* isolate);

  // For a few types of references, we can get their address from their id.
  void AddFromId(TypeCode type,
                 uint16_t id,
                 const char* name,
                 Isolate* isolate);

  // For other types of references, the caller will figure out the address.
  void Add(Address address, TypeCode type, uint16_t id, const char* name);

  void Add(Address address, const char* name) {
    Add(address, UNCLASSIFIED, ++max_id_[UNCLASSIFIED], name);
  }

  List<ExternalReferenceEntry> refs_;
  uint16_t max_id_[kTypeCodeCount];
};


class ExternalReferenceEncoder {
 public:
  explicit ExternalReferenceEncoder(Isolate* isolate);

  uint32_t Encode(Address key) const;

  const char* NameOfAddress(Address key) const;

 private:
  HashMap encodings_;
  static uint32_t Hash(Address key) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(key) >> 2);
  }

  int IndexOf(Address key) const;

  void Put(Address key, int index);

  Isolate* isolate_;
};


class ExternalReferenceDecoder {
 public:
  explicit ExternalReferenceDecoder(Isolate* isolate);
  ~ExternalReferenceDecoder();

  Address Decode(uint32_t key) const {
    if (key == 0) return NULL;
    return *Lookup(key);
  }

 private:
  Address** encodings_;

  Address* Lookup(uint32_t key) const {
    int type = key >> kReferenceTypeShift;
    DCHECK(kFirstTypeCode <= type && type < kTypeCodeCount);
    int id = key & kReferenceIdMask;
    return &encodings_[type][id];
  }

  void Put(uint32_t key, Address value) {
    *Lookup(key) = value;
  }

  Isolate* isolate_;
};


class AddressMapBase {
 protected:
  static void SetValue(HashMap::Entry* entry, uint32_t v) {
    entry->value = reinterpret_cast<void*>(v);
  }

  static uint32_t GetValue(HashMap::Entry* entry) {
    return static_cast<uint32_t>(reinterpret_cast<intptr_t>(entry->value));
  }

  static HashMap::Entry* LookupEntry(HashMap* map, HeapObject* obj,
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

  ~RootIndexMap() { delete map_; }

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


class BackReference {
 public:
  explicit BackReference(uint32_t bitfield) : bitfield_(bitfield) {}

  BackReference() : bitfield_(kInvalidValue) {}

  static BackReference SourceReference() { return BackReference(kSourceValue); }

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

  AllocationSpace space() const {
    DCHECK(is_valid());
    return SpaceBits::decode(bitfield_);
  }

  uint32_t chunk_offset() const {
    DCHECK(is_valid());
    return ChunkOffsetBits::decode(bitfield_) << kObjectAlignmentBits;
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
    DCHECK_EQ(NULL, LookupEntry(map_, obj, false));
    HashMap::Entry* entry = LookupEntry(map_, obj, true);
    SetValue(entry, b.bitfield());
  }

  void AddSourceString(String* string) {
    Add(string, BackReference::SourceReference());
  }

 private:
  DisallowHeapAllocation no_allocation_;
  HashMap* map_;
  DISALLOW_COPY_AND_ASSIGN(BackReferenceMap);
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
  // Where the pointed-to object can be found:
  enum Where {
    kNewObject = 0,  // Object is next in snapshot.
    // 1-7                             One per space.
    kRootArray = 0x9,             // Object is found in root array.
    kPartialSnapshotCache = 0xa,  // Object is in the cache.
    kExternalReference = 0xb,     // Pointer to an external reference.
    kSkip = 0xc,                  // Skip n bytes.
    kBuiltin = 0xd,               // Builtin code object.
    kAttachedReference = 0xe,     // Object is described in an attached list.
    kNop = 0xf,                   // Does nothing, used to pad.
    kBackref = 0x10,              // Object is described relative to end.
    // 0x11-0x17                       One per space.
    kBackrefWithSkip = 0x18,  // Object is described relative to end.
    // 0x19-0x1f                       One per space.
    // 0x20-0x3f                       Used by misc. tags below.
    kPointedToMask = 0x3f
  };

  // How to code the pointer to the object.
  enum HowToCode {
    kPlain = 0,                          // Straight pointer.
    // What this means depends on the architecture:
    kFromCode = 0x40,                    // A pointer inlined in code.
    kHowToCodeMask = 0x40
  };

  // For kRootArrayConstants
  enum WithSkip {
    kNoSkipDistance = 0,
    kHasSkipDistance = 0x40,
    kWithSkipMask = 0x40
  };

  // Where to point within the object.
  enum WhereToPoint {
    kStartOfObject = 0,
    kInnerPointer = 0x80,  // First insn in code object or payload of cell.
    kWhereToPointMask = 0x80
  };

  // Misc.
  // Raw data to be copied from the snapshot.  This byte code does not advance
  // the current pointer, which is used for code objects, where we write the
  // entire code in one memcpy, then fix up stuff with kSkip and other byte
  // codes that overwrite data.
  static const int kRawData = 0x20;
  // Some common raw lengths: 0x21-0x3f.  These autoadvance the current pointer.
  // A tag emitted at strategic points in the snapshot to delineate sections.
  // If the deserializer does not find these at the expected moments then it
  // is an indication that the snapshot and the VM do not fit together.
  // Examine the build process for architecture, version or configuration
  // mismatches.
  static const int kSynchronize = 0x70;
  // Used for the source code of the natives, which is in the executable, but
  // is referred to from external strings in the snapshot.
  static const int kNativesStringResource = 0x71;
  static const int kRepeat = 0x72;
  static const int kConstantRepeat = 0x73;
  // 0x73-0x7f            Repeat last word (subtract 0x72 to get the count).
  static const int kMaxRepeats = 0x7f - 0x72;
  static int CodeForRepeats(int repeats) {
    DCHECK(repeats >= 1 && repeats <= kMaxRepeats);
    return 0x72 + repeats;
  }
  static int RepeatsForCode(int byte_code) {
    DCHECK(byte_code >= kConstantRepeat && byte_code <= 0x7f);
    return byte_code - 0x72;
  }
  static const int kRootArrayConstants = 0xa0;
  // 0xa0-0xbf            Things from the first 32 elements of the root array.
  static const int kRootArrayNumberOfConstantEncodings = 0x20;
  static int RootArrayConstantFromByteCode(int byte_code) {
    return byte_code & 0x1f;
  }

  static const int kAnyOldSpace = -1;

  // A bitmask for getting the space out of an instruction.
  static const int kSpaceMask = 7;
  STATIC_ASSERT(kNumberOfSpaces <= kSpaceMask + 1);
};


// A Deserializer reads a snapshot and reconstructs the Object graph it defines.
class Deserializer: public SerializerDeserializer {
 public:
  // Create a deserializer from a snapshot byte source.
  explicit Deserializer(SnapshotByteSource* source);

  virtual ~Deserializer();

  // Deserialize the snapshot into an empty heap.
  void Deserialize(Isolate* isolate);

  enum OnOOM { FATAL_ON_OOM, NULL_ON_OOM };

  // Deserialize a single object and the objects reachable from it.
  // We may want to abort gracefully even if deserialization fails.
  void DeserializePartial(Isolate* isolate, Object** root,
                          OnOOM on_oom = FATAL_ON_OOM);

  void AddReservation(int space, uint32_t chunk) {
    DCHECK(space >= 0);
    DCHECK(space < kNumberOfSpaces);
    reservations_[space].Add({chunk, NULL, NULL});
  }

  void FlushICacheForNewCodeObjects();

  // Serialized user code reference certain objects that are provided in a list
  // By calling this method, we assume that we are deserializing user code.
  void SetAttachedObjects(Vector<Handle<Object> >* attached_objects) {
    attached_objects_ = attached_objects;
  }

  bool deserializing_user_code() { return attached_objects_ != NULL; }

 private:
  virtual void VisitPointers(Object** start, Object** end);

  virtual void VisitRuntimeEntry(RelocInfo* rinfo) {
    UNREACHABLE();
  }

  bool ReserveSpace();

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
  Object* ProcessBackRefInSerializedCode(Object* obj);

  // This returns the address of an object that has been described in the
  // snapshot by chunk index and offset.
  HeapObject* GetBackReferencedObject(int space) {
    if (space == LO_SPACE) {
      uint32_t index = source_->GetInt();
      return deserialized_large_objects_[index];
    } else {
      BackReference back_reference(source_->GetInt());
      DCHECK(space < kNumberOfPreallocatedSpaces);
      uint32_t chunk_index = back_reference.chunk_index();
      DCHECK_LE(chunk_index, current_chunk_[space]);
      uint32_t chunk_offset = back_reference.chunk_offset();
      return HeapObject::FromAddress(reservations_[space][chunk_index].start +
                                     chunk_offset);
    }
  }

  // Cached current isolate.
  Isolate* isolate_;

  // Objects from the attached object descriptions in the serialized user code.
  Vector<Handle<Object> >* attached_objects_;

  SnapshotByteSource* source_;
  // The address of the next object that will be allocated in each space.
  // Each space has a number of chunks reserved by the GC, with each chunk
  // fitting into a page. Deserialized objects are allocated into the
  // current chunk of the target space by bumping up high water mark.
  Heap::Reservation reservations_[kNumberOfSpaces];
  uint32_t current_chunk_[kNumberOfPreallocatedSpaces];
  Address high_water_[kNumberOfPreallocatedSpaces];

  ExternalReferenceDecoder* external_reference_decoder_;

  List<HeapObject*> deserialized_large_objects_;

  DISALLOW_COPY_AND_ASSIGN(Deserializer);
};


class CodeAddressMap;

// There can be only one serializer per V8 process.
class Serializer : public SerializerDeserializer {
 public:
  Serializer(Isolate* isolate, SnapshotByteSink* sink);
  ~Serializer();
  virtual void VisitPointers(Object** start, Object** end) OVERRIDE;

  void FinalizeAllocation();

  Vector<const uint32_t> FinalAllocationChunks(int space) const {
    if (space == LO_SPACE) {
      return Vector<const uint32_t>(&large_objects_total_size_, 1);
    } else {
      DCHECK_EQ(0, pending_chunk_[space]);  // No pending chunks.
      return completed_chunks_[space].ToConstVector();
    }
  }

  Isolate* isolate() const { return isolate_; }

  BackReferenceMap* back_reference_map() { return &back_reference_map_; }
  RootIndexMap* root_index_map() { return &root_index_map_; }

 protected:
  class ObjectSerializer : public ObjectVisitor {
   public:
    ObjectSerializer(Serializer* serializer,
                     Object* o,
                     SnapshotByteSink* sink,
                     HowToCode how_to_code,
                     WhereToPoint where_to_point)
      : serializer_(serializer),
        object_(HeapObject::cast(o)),
        sink_(sink),
        reference_representation_(how_to_code + where_to_point),
        bytes_processed_so_far_(0),
        code_object_(o->IsCode()),
        code_has_been_output_(false) { }
    void Serialize();
    void VisitPointers(Object** start, Object** end);
    void VisitEmbeddedPointer(RelocInfo* target);
    void VisitExternalReference(Address* p);
    void VisitExternalReference(RelocInfo* rinfo);
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

    Serializer* serializer_;
    HeapObject* object_;
    SnapshotByteSink* sink_;
    int reference_representation_;
    int bytes_processed_so_far_;
    bool code_object_;
    bool code_has_been_output_;
  };

  virtual void SerializeObject(HeapObject* o, HowToCode how_to_code,
                               WhereToPoint where_to_point, int skip) = 0;

  void PutRoot(int index, HeapObject* object, HowToCode how, WhereToPoint where,
               int skip);

  void SerializeBackReference(BackReference back_reference,
                              HowToCode how_to_code,
                              WhereToPoint where_to_point, int skip);
  void InitializeAllocators();
  // This will return the space for an object.
  static AllocationSpace SpaceOfObject(HeapObject* object);
  BackReference AllocateLargeObject(int size);
  BackReference Allocate(AllocationSpace space, int size);
  int EncodeExternalReference(Address addr) {
    return external_reference_encoder_->Encode(addr);
  }

  // GetInt reads 4 bytes at once, requiring padding at the end.
  void Pad();

  // Some roots should not be serialized, because their actual value depends on
  // absolute addresses and they are reset after deserialization, anyway.
  bool ShouldBeSkipped(Object** current);

  // We may not need the code address map for logging for every instance
  // of the serializer.  Initialize it on demand.
  void InitializeCodeAddressMap();

  inline uint32_t max_chunk_size(int space) const {
    DCHECK_LE(0, space);
    DCHECK_LT(space, kNumberOfSpaces);
    return max_chunk_size_[space];
  }

  Isolate* isolate_;

  SnapshotByteSink* sink_;
  ExternalReferenceEncoder* external_reference_encoder_;

  BackReferenceMap back_reference_map_;
  RootIndexMap root_index_map_;

  friend class ObjectSerializer;
  friend class Deserializer;

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

  DISALLOW_COPY_AND_ASSIGN(Serializer);
};


class PartialSerializer : public Serializer {
 public:
  PartialSerializer(Isolate* isolate,
                    Serializer* startup_snapshot_serializer,
                    SnapshotByteSink* sink)
    : Serializer(isolate, sink),
      startup_serializer_(startup_snapshot_serializer) {
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


  Serializer* startup_serializer_;
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
    isolate->set_serialize_partial_snapshot_cache_length(0);
    InitializeCodeAddressMap();
  }

  // The StartupSerializer has to serialize the root array, which is slightly
  // different.
  virtual void VisitPointers(Object** start, Object** end) OVERRIDE;

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
      Isolate* isolate, ScriptData* data, Handle<String> source);

  static const int kSourceObjectIndex = 0;
  static const int kCodeStubsBaseIndex = 1;

  String* source() const {
    DCHECK(!AllowHeapAllocation::IsAllowed());
    return source_;
  }

  List<uint32_t>* stub_keys() { return &stub_keys_; }
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
  void SerializeSourceObject(HowToCode how_to_code,
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


// Wrapper around ScriptData to provide code-serializer-specific functionality.
class SerializedCodeData {
 public:
  // Used by when consuming.
  explicit SerializedCodeData(ScriptData* data, String* source)
      : script_data_(data), owns_script_data_(false) {
    DisallowHeapAllocation no_gc;
    CHECK(IsSane(source));
  }

  // Used when producing.
  SerializedCodeData(const List<byte>& payload, CodeSerializer* cs);

  ~SerializedCodeData() {
    if (owns_script_data_) delete script_data_;
  }

  // Return ScriptData object and relinquish ownership over it to the caller.
  ScriptData* GetScriptData() {
    ScriptData* result = script_data_;
    script_data_ = NULL;
    DCHECK(owns_script_data_);
    owns_script_data_ = false;
    return result;
  }

  class Reservation {
   public:
    uint32_t chunk_size() const { return ChunkSizeBits::decode(reservation); }
    bool is_last_chunk() const { return IsLastChunkBits::decode(reservation); }

   private:
    uint32_t reservation;

    DISALLOW_COPY_AND_ASSIGN(Reservation);
  };

  int NumInternalizedStrings() const {
    return GetHeaderValue(kNumInternalizedStringsOffset);
  }

  Vector<const Reservation> Reservations() const {
    return Vector<const Reservation>(reinterpret_cast<const Reservation*>(
                                         script_data_->data() + kHeaderSize),
                                     GetHeaderValue(kReservationsOffset));
  }

  Vector<const uint32_t> CodeStubKeys() const {
    int reservations_size = GetHeaderValue(kReservationsOffset) * kInt32Size;
    const byte* start = script_data_->data() + kHeaderSize + reservations_size;
    return Vector<const uint32_t>(reinterpret_cast<const uint32_t*>(start),
                                  GetHeaderValue(kNumCodeStubKeysOffset));
  }

  const byte* Payload() const {
    int reservations_size = GetHeaderValue(kReservationsOffset) * kInt32Size;
    int code_stubs_size = GetHeaderValue(kNumCodeStubKeysOffset) * kInt32Size;
    return script_data_->data() + kHeaderSize + reservations_size +
           code_stubs_size;
  }

  int PayloadLength() const {
    int payload_length = GetHeaderValue(kPayloadLengthOffset);
    DCHECK_EQ(script_data_->data() + script_data_->length(),
              Payload() + payload_length);
    return payload_length;
  }

 private:
  void SetHeaderValue(int offset, int value) {
    reinterpret_cast<int*>(const_cast<byte*>(script_data_->data()))[offset] =
        value;
  }

  int GetHeaderValue(int offset) const {
    return reinterpret_cast<const int*>(script_data_->data())[offset];
  }

  bool IsSane(String* source);

  int CheckSum(String* source);

  // The data header consists of int-sized entries:
  // [0] version hash
  // [1] number of internalized strings
  // [2] number of code stub keys
  // [3] payload length
  // [4..10] reservation sizes for spaces from NEW_SPACE to PROPERTY_CELL_SPACE.
  static const int kCheckSumOffset = 0;
  static const int kNumInternalizedStringsOffset = 1;
  static const int kReservationsOffset = 2;
  static const int kNumCodeStubKeysOffset = 3;
  static const int kPayloadLengthOffset = 4;
  static const int kHeaderSize = (kPayloadLengthOffset + 1) * kIntSize;

  class ChunkSizeBits : public BitField<uint32_t, 0, 31> {};
  class IsLastChunkBits : public BitField<bool, 31, 1> {};

  // Following the header, we store, in sequential order
  // - code stub keys
  // - serialization payload

  ScriptData* script_data_;
  bool owns_script_data_;
};
} }  // namespace v8::internal

#endif  // V8_SERIALIZE_H_
