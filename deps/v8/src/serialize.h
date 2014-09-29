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


// The Serializer/Deserializer class is a common superclass for Serializer and
// Deserializer which is used to store common constants and methods used by
// both.
class SerializerDeserializer: public ObjectVisitor {
 public:
  static void Iterate(Isolate* isolate, ObjectVisitor* visitor);

  static int nop() { return kNop; }

 protected:
  // Where the pointed-to object can be found:
  enum Where {
    kNewObject = 0,  // Object is next in snapshot.
    // 1-6                             One per space.
    kRootArray = 0x9,             // Object is found in root array.
    kPartialSnapshotCache = 0xa,  // Object is in the cache.
    kExternalReference = 0xb,     // Pointer to an external reference.
    kSkip = 0xc,                  // Skip n bytes.
    kBuiltin = 0xd,               // Builtin code object.
    kAttachedReference = 0xe,     // Object is described in an attached list.
    kNop = 0xf,                   // Does nothing, used to pad.
    kBackref = 0x10,              // Object is described relative to end.
    // 0x11-0x16                       One per space.
    kBackrefWithSkip = 0x18,  // Object is described relative to end.
    // 0x19-0x1e                       One per space.
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

  static const int kNumberOfSpaces = LO_SPACE;
  static const int kAnyOldSpace = -1;

  // A bitmask for getting the space out of an instruction.
  static const int kSpaceMask = 7;
};


// A Deserializer reads a snapshot and reconstructs the Object graph it defines.
class Deserializer: public SerializerDeserializer {
 public:
  // Create a deserializer from a snapshot byte source.
  explicit Deserializer(SnapshotByteSource* source);

  virtual ~Deserializer();

  // Deserialize the snapshot into an empty heap.
  void Deserialize(Isolate* isolate);

  // Deserialize a single object and the objects reachable from it.
  void DeserializePartial(Isolate* isolate, Object** root);

  void set_reservation(int space_number, int reservation) {
    DCHECK(space_number >= 0);
    DCHECK(space_number <= LAST_SPACE);
    reservations_[space_number] = reservation;
  }

  void FlushICacheForNewCodeObjects();

  // Serialized user code reference certain objects that are provided in a list
  // By calling this method, we assume that we are deserializing user code.
  void SetAttachedObjects(Vector<Object*>* attached_objects) {
    attached_objects_ = attached_objects;
  }

  bool deserializing_user_code() { return attached_objects_ != NULL; }

 private:
  virtual void VisitPointers(Object** start, Object** end);

  virtual void VisitRuntimeEntry(RelocInfo* rinfo) {
    UNREACHABLE();
  }

  // Allocation sites are present in the snapshot, and must be linked into
  // a list at deserialization time.
  void RelinkAllocationSite(AllocationSite* site);

  // Fills in some heap data in an area from start to end (non-inclusive).  The
  // space id is used for the write barrier.  The object_address is the address
  // of the object we are writing into, or NULL if we are not writing into an
  // object, i.e. if we are writing a series of tagged values that are not on
  // the heap.
  void ReadChunk(
      Object** start, Object** end, int space, Address object_address);
  void ReadObject(int space_number, Object** write_back);

  // Special handling for serialized code like hooking up internalized strings.
  HeapObject* ProcessNewObjectFromSerializedCode(HeapObject* obj);
  Object* ProcessBackRefInSerializedCode(Object* obj);

  // This routine both allocates a new object, and also keeps
  // track of where objects have been allocated so that we can
  // fix back references when deserializing.
  Address Allocate(int space_index, int size) {
    Address address = high_water_[space_index];
    high_water_[space_index] = address + size;
    return address;
  }

  // This returns the address of an object that has been described in the
  // snapshot as being offset bytes back in a particular space.
  HeapObject* GetAddressFromEnd(int space) {
    int offset = source_->GetInt();
    offset <<= kObjectAlignmentBits;
    return HeapObject::FromAddress(high_water_[space] - offset);
  }

  // Cached current isolate.
  Isolate* isolate_;

  // Objects from the attached object descriptions in the serialized user code.
  Vector<Object*>* attached_objects_;

  SnapshotByteSource* source_;
  // This is the address of the next object that will be allocated in each
  // space.  It is used to calculate the addresses of back-references.
  Address high_water_[LAST_SPACE + 1];

  int reservations_[LAST_SPACE + 1];
  static const intptr_t kUninitializedReservation = -1;

  ExternalReferenceDecoder* external_reference_decoder_;

  DISALLOW_COPY_AND_ASSIGN(Deserializer);
};


// Mapping objects to their location after deserialization.
// This is used during building, but not at runtime by V8.
class SerializationAddressMapper {
 public:
  SerializationAddressMapper()
      : no_allocation_(),
        serialization_map_(new HashMap(HashMap::PointersMatch)) { }

  ~SerializationAddressMapper() {
    delete serialization_map_;
  }

  bool IsMapped(HeapObject* obj) {
    return serialization_map_->Lookup(Key(obj), Hash(obj), false) != NULL;
  }

  int MappedTo(HeapObject* obj) {
    DCHECK(IsMapped(obj));
    return static_cast<int>(reinterpret_cast<intptr_t>(
        serialization_map_->Lookup(Key(obj), Hash(obj), false)->value));
  }

  void AddMapping(HeapObject* obj, int to) {
    DCHECK(!IsMapped(obj));
    HashMap::Entry* entry =
        serialization_map_->Lookup(Key(obj), Hash(obj), true);
    entry->value = Value(to);
  }

 private:
  static uint32_t Hash(HeapObject* obj) {
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(obj->address()));
  }

  static void* Key(HeapObject* obj) {
    return reinterpret_cast<void*>(obj->address());
  }

  static void* Value(int v) {
    return reinterpret_cast<void*>(v);
  }

  DisallowHeapAllocation no_allocation_;
  HashMap* serialization_map_;
  DISALLOW_COPY_AND_ASSIGN(SerializationAddressMapper);
};


class CodeAddressMap;

// There can be only one serializer per V8 process.
class Serializer : public SerializerDeserializer {
 public:
  Serializer(Isolate* isolate, SnapshotByteSink* sink);
  ~Serializer();
  void VisitPointers(Object** start, Object** end);
  // You can call this after serialization to find out how much space was used
  // in each space.
  int CurrentAllocationAddress(int space) const {
    DCHECK(space < kNumberOfSpaces);
    return fullness_[space];
  }

  Isolate* isolate() const { return isolate_; }

  SerializationAddressMapper* address_mapper() { return &address_mapper_; }
  void PutRoot(int index,
               HeapObject* object,
               HowToCode how,
               WhereToPoint where,
               int skip);

 protected:
  static const int kInvalidRootIndex = -1;

  int RootIndex(HeapObject* heap_object, HowToCode from);
  intptr_t root_index_wave_front() { return root_index_wave_front_; }
  void set_root_index_wave_front(intptr_t value) {
    DCHECK(value >= root_index_wave_front_);
    root_index_wave_front_ = value;
  }

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
    void VisitExternalAsciiString(
        v8::String::ExternalAsciiStringResource** resource);
    // We can't serialize a heap with external two byte strings.
    void VisitExternalTwoByteString(
        v8::String::ExternalStringResource** resource) {
      UNREACHABLE();
    }

   private:
    enum ReturnSkip { kCanReturnSkipInsteadOfSkipping, kIgnoringReturn };
    // This function outputs or skips the raw data between the last pointer and
    // up to the current position.  It optionally can just return the number of
    // bytes to skip instead of performing a skip instruction, in case the skip
    // can be merged into the next instruction.
    int OutputRawData(Address up_to, ReturnSkip return_skip = kIgnoringReturn);

    Serializer* serializer_;
    HeapObject* object_;
    SnapshotByteSink* sink_;
    int reference_representation_;
    int bytes_processed_so_far_;
    bool code_object_;
    bool code_has_been_output_;
  };

  virtual void SerializeObject(Object* o,
                               HowToCode how_to_code,
                               WhereToPoint where_to_point,
                               int skip) = 0;
  void SerializeReferenceToPreviousObject(
      int space,
      int address,
      HowToCode how_to_code,
      WhereToPoint where_to_point,
      int skip);
  void InitializeAllocators();
  // This will return the space for an object.
  static int SpaceOfObject(HeapObject* object);
  int Allocate(int space, int size);
  int EncodeExternalReference(Address addr) {
    return external_reference_encoder_->Encode(addr);
  }

  int SpaceAreaSize(int space);

  // Some roots should not be serialized, because their actual value depends on
  // absolute addresses and they are reset after deserialization, anyway.
  bool ShouldBeSkipped(Object** current);

  Isolate* isolate_;
  // Keep track of the fullness of each space in order to generate
  // relative addresses for back references.
  int fullness_[LAST_SPACE + 1];
  SnapshotByteSink* sink_;
  ExternalReferenceEncoder* external_reference_encoder_;

  SerializationAddressMapper address_mapper_;
  intptr_t root_index_wave_front_;
  void Pad();

  friend class ObjectSerializer;
  friend class Deserializer;

  // We may not need the code address map for logging for every instance
  // of the serializer.  Initialize it on demand.
  void InitializeCodeAddressMap();

 private:
  CodeAddressMap* code_address_map_;
  DISALLOW_COPY_AND_ASSIGN(Serializer);
};


class PartialSerializer : public Serializer {
 public:
  PartialSerializer(Isolate* isolate,
                    Serializer* startup_snapshot_serializer,
                    SnapshotByteSink* sink)
    : Serializer(isolate, sink),
      startup_serializer_(startup_snapshot_serializer) {
    set_root_index_wave_front(Heap::kStrongRootListLength);
    InitializeCodeAddressMap();
  }

  // Serialize the objects reachable from a single object pointer.
  void Serialize(Object** o);
  virtual void SerializeObject(Object* o,
                               HowToCode how_to_code,
                               WhereToPoint where_to_point,
                               int skip);

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
    : Serializer(isolate, sink) {
    // Clear the cache of objects used by the partial snapshot.  After the
    // strong roots have been serialized we can create a partial snapshot
    // which will repopulate the cache with objects needed by that partial
    // snapshot.
    isolate->set_serialize_partial_snapshot_cache_length(0);
    InitializeCodeAddressMap();
  }
  // Serialize the current state of the heap.  The order is:
  // 1) Strong references.
  // 2) Partial snapshot cache.
  // 3) Weak references (e.g. the string table).
  virtual void SerializeStrongReferences();
  virtual void SerializeObject(Object* o,
                               HowToCode how_to_code,
                               WhereToPoint where_to_point,
                               int skip);
  void SerializeWeakReferences();
  void Serialize() {
    SerializeStrongReferences();
    SerializeWeakReferences();
    Pad();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StartupSerializer);
};


class CodeSerializer : public Serializer {
 public:
  CodeSerializer(Isolate* isolate, SnapshotByteSink* sink, String* source)
      : Serializer(isolate, sink), source_(source) {
    set_root_index_wave_front(Heap::kStrongRootListLength);
    InitializeCodeAddressMap();
  }

  static ScriptData* Serialize(Isolate* isolate,
                               Handle<SharedFunctionInfo> info,
                               Handle<String> source);

  virtual void SerializeObject(Object* o, HowToCode how_to_code,
                               WhereToPoint where_to_point, int skip);

  static Handle<SharedFunctionInfo> Deserialize(Isolate* isolate,
                                                ScriptData* data,
                                                Handle<String> source);

  static const int kSourceObjectIndex = 0;

  String* source() {
    DCHECK(!AllowHeapAllocation::IsAllowed());
    return source_;
  }

 private:
  void SerializeBuiltin(Code* builtin, HowToCode how_to_code,
                        WhereToPoint where_to_point, int skip);
  void SerializeSourceObject(HowToCode how_to_code, WhereToPoint where_to_point,
                             int skip);

  DisallowHeapAllocation no_gc_;
  String* source_;
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
  SerializedCodeData(List<byte>* payload, CodeSerializer* cs);

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

  const byte* Payload() const {
    return script_data_->data() + kHeaderEntries * kIntSize;
  }

  int PayloadLength() const {
    return script_data_->length() - kHeaderEntries * kIntSize;
  }

  int GetReservation(int space) const {
    return GetHeaderValue(kReservationsOffset + space);
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
  // [1..7] reservation sizes for spaces from NEW_SPACE to PROPERTY_CELL_SPACE.
  static const int kCheckSumOffset = 0;
  static const int kReservationsOffset = 1;
  static const int kHeaderEntries = 8;

  ScriptData* script_data_;
  bool owns_script_data_;
};
} }  // namespace v8::internal

#endif  // V8_SERIALIZE_H_
