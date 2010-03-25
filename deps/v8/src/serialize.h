// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_SERIALIZE_H_
#define V8_SERIALIZE_H_

#include "hashmap.h"

namespace v8 {
namespace internal {

// A TypeCode is used to distinguish different kinds of external reference.
// It is a single bit to make testing for types easy.
enum TypeCode {
  UNCLASSIFIED,        // One-of-a-kind references.
  BUILTIN,
  RUNTIME_FUNCTION,
  IC_UTILITY,
  DEBUG_ADDRESS,
  STATS_COUNTER,
  TOP_ADDRESS,
  C_BUILTIN,
  EXTENSION,
  ACCESSOR,
  RUNTIME_ENTRY,
  STUB_CACHE_TABLE
};

const int kTypeCodeCount = STUB_CACHE_TABLE + 1;
const int kFirstTypeCode = UNCLASSIFIED;

const int kReferenceIdBits = 16;
const int kReferenceIdMask = (1 << kReferenceIdBits) - 1;
const int kReferenceTypeShift = kReferenceIdBits;
const int kDebugRegisterBits = 4;
const int kDebugIdShift = kDebugRegisterBits;


class ExternalReferenceEncoder {
 public:
  ExternalReferenceEncoder();

  uint32_t Encode(Address key) const;

  const char* NameOfAddress(Address key) const;

 private:
  HashMap encodings_;
  static uint32_t Hash(Address key) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(key) >> 2);
  }

  int IndexOf(Address key) const;

  static bool Match(void* key1, void* key2) { return key1 == key2; }

  void Put(Address key, int index);
};


class ExternalReferenceDecoder {
 public:
  ExternalReferenceDecoder();
  ~ExternalReferenceDecoder();

  Address Decode(uint32_t key) const {
    if (key == 0) return NULL;
    return *Lookup(key);
  }

 private:
  Address** encodings_;

  Address* Lookup(uint32_t key) const {
    int type = key >> kReferenceTypeShift;
    ASSERT(kFirstTypeCode <= type && type < kTypeCodeCount);
    int id = key & kReferenceIdMask;
    return &encodings_[type][id];
  }

  void Put(uint32_t key, Address value) {
    *Lookup(key) = value;
  }
};


class SnapshotByteSource {
 public:
  SnapshotByteSource(const byte* array, int length)
    : data_(array), length_(length), position_(0) { }

  bool HasMore() { return position_ < length_; }

  int Get() {
    ASSERT(position_ < length_);
    return data_[position_++];
  }

  inline void CopyRaw(byte* to, int number_of_bytes);

  inline int GetInt();

  bool AtEOF() {
    return position_ == length_;
  }

  int position() { return position_; }

 private:
  const byte* data_;
  int length_;
  int position_;
};


// It is very common to have a reference to the object at word 10 in space 2,
// the object at word 5 in space 2 and the object at word 28 in space 4.  This
// only works for objects in the first page of a space.
#define COMMON_REFERENCE_PATTERNS(f)                              \
  f(kNumberOfSpaces, 2, 10)                                       \
  f(kNumberOfSpaces + 1, 2, 5)                                    \
  f(kNumberOfSpaces + 2, 4, 28)                                   \
  f(kNumberOfSpaces + 3, 2, 21)                                   \
  f(kNumberOfSpaces + 4, 2, 98)                                   \
  f(kNumberOfSpaces + 5, 2, 67)                                   \
  f(kNumberOfSpaces + 6, 4, 132)

#define COMMON_RAW_LENGTHS(f)        \
  f(1, 1)  \
  f(2, 2)  \
  f(3, 3)  \
  f(4, 4)  \
  f(5, 5)  \
  f(6, 6)  \
  f(7, 7)  \
  f(8, 8)  \
  f(9, 12)  \
  f(10, 16) \
  f(11, 20) \
  f(12, 24) \
  f(13, 28) \
  f(14, 32) \
  f(15, 36)

// The Serializer/Deserializer class is a common superclass for Serializer and
// Deserializer which is used to store common constants and methods used by
// both.
class SerializerDeserializer: public ObjectVisitor {
 public:
  static void Iterate(ObjectVisitor* visitor);
  static void SetSnapshotCacheSize(int size);

 protected:
  enum DataType {
    RAW_DATA_SERIALIZATION = 0,
    // And 15 common raw lengths.
    OBJECT_SERIALIZATION = 16,
    // One variant per space.
    CODE_OBJECT_SERIALIZATION = 25,
    // One per space (only code spaces in use).
    EXTERNAL_REFERENCE_SERIALIZATION = 34,
    EXTERNAL_BRANCH_TARGET_SERIALIZATION = 35,
    SYNCHRONIZE = 36,
    START_NEW_PAGE_SERIALIZATION = 37,
    NATIVES_STRING_RESOURCE = 38,
    ROOT_SERIALIZATION = 39,
    PARTIAL_SNAPSHOT_CACHE_ENTRY = 40,
    // Free: 41-47.
    BACKREF_SERIALIZATION = 48,
    // One per space, must be kSpaceMask aligned.
    // Free: 57-63.
    REFERENCE_SERIALIZATION = 64,
    // One per space and common references.  Must be kSpaceMask aligned.
    CODE_BACKREF_SERIALIZATION = 80,
    // One per space, must be kSpaceMask aligned.
    // Free: 89-95.
    CODE_REFERENCE_SERIALIZATION = 96
    // One per space, must be kSpaceMask aligned.
    // Free: 105-255.
  };
  static const int kLargeData = LAST_SPACE;
  static const int kLargeCode = kLargeData + 1;
  static const int kLargeFixedArray = kLargeCode + 1;
  static const int kNumberOfSpaces = kLargeFixedArray + 1;

  // A bitmask for getting the space out of an instruction.
  static const int kSpaceMask = 15;

  static inline bool SpaceIsLarge(int space) { return space >= kLargeData; }
  static inline bool SpaceIsPaged(int space) {
    return space >= FIRST_PAGED_SPACE && space <= LAST_PAGED_SPACE;
  }

  static int partial_snapshot_cache_length_;
  static const int kPartialSnapshotCacheCapacity = 1300;
  static Object* partial_snapshot_cache_[];
};


int SnapshotByteSource::GetInt() {
  // A little unwind to catch the really small ints.
  int snapshot_byte = Get();
  if ((snapshot_byte & 0x80) == 0) {
    return snapshot_byte;
  }
  int accumulator = (snapshot_byte & 0x7f) << 7;
  while (true) {
    snapshot_byte = Get();
    if ((snapshot_byte & 0x80) == 0) {
      return accumulator | snapshot_byte;
    }
    accumulator = (accumulator | (snapshot_byte & 0x7f)) << 7;
  }
  UNREACHABLE();
  return accumulator;
}


void SnapshotByteSource::CopyRaw(byte* to, int number_of_bytes) {
  memcpy(to, data_ + position_, number_of_bytes);
  position_ += number_of_bytes;
}


// A Deserializer reads a snapshot and reconstructs the Object graph it defines.
class Deserializer: public SerializerDeserializer {
 public:
  // Create a deserializer from a snapshot byte source.
  explicit Deserializer(SnapshotByteSource* source);

  virtual ~Deserializer();

  // Deserialize the snapshot into an empty heap.
  void Deserialize();

  // Deserialize a single object and the objects reachable from it.
  void DeserializePartial(Object** root);

#ifdef DEBUG
  virtual void Synchronize(const char* tag);
#endif

 private:
  virtual void VisitPointers(Object** start, Object** end);

  virtual void VisitExternalReferences(Address* start, Address* end) {
    UNREACHABLE();
  }

  virtual void VisitRuntimeEntry(RelocInfo* rinfo) {
    UNREACHABLE();
  }

  void ReadChunk(Object** start, Object** end, int space, Address address);
  HeapObject* GetAddressFromStart(int space);
  inline HeapObject* GetAddressFromEnd(int space);
  Address Allocate(int space_number, Space* space, int size);
  void ReadObject(int space_number, Space* space, Object** write_back);

  // Keep track of the pages in the paged spaces.
  // (In large object space we are keeping track of individual objects
  // rather than pages.)  In new space we just need the address of the
  // first object and the others will flow from that.
  List<Address> pages_[SerializerDeserializer::kNumberOfSpaces];

  SnapshotByteSource* source_;
  static ExternalReferenceDecoder* external_reference_decoder_;
  // This is the address of the next object that will be allocated in each
  // space.  It is used to calculate the addresses of back-references.
  Address high_water_[LAST_SPACE + 1];
  // This is the address of the most recent object that was allocated.  It
  // is used to set the location of the new page when we encounter a
  // START_NEW_PAGE_SERIALIZATION tag.
  Address last_object_address_;

  DISALLOW_COPY_AND_ASSIGN(Deserializer);
};


class SnapshotByteSink {
 public:
  virtual ~SnapshotByteSink() { }
  virtual void Put(int byte, const char* description) = 0;
  virtual void PutSection(int byte, const char* description) {
    Put(byte, description);
  }
  void PutInt(uintptr_t integer, const char* description);
  virtual int Position() = 0;
};


// Mapping objects to their location after deserialization.
// This is used during building, but not at runtime by V8.
class SerializationAddressMapper {
 public:
  SerializationAddressMapper()
      : serialization_map_(new HashMap(&SerializationMatchFun)),
        no_allocation_(new AssertNoAllocation()) { }

  ~SerializationAddressMapper() {
    delete serialization_map_;
    delete no_allocation_;
  }

  bool IsMapped(HeapObject* obj) {
    return serialization_map_->Lookup(Key(obj), Hash(obj), false) != NULL;
  }

  int MappedTo(HeapObject* obj) {
    ASSERT(IsMapped(obj));
    return static_cast<int>(reinterpret_cast<intptr_t>(
        serialization_map_->Lookup(Key(obj), Hash(obj), false)->value));
  }

  void AddMapping(HeapObject* obj, int to) {
    ASSERT(!IsMapped(obj));
    HashMap::Entry* entry =
        serialization_map_->Lookup(Key(obj), Hash(obj), true);
    entry->value = Value(to);
  }

 private:
  static bool SerializationMatchFun(void* key1, void* key2) {
    return key1 == key2;
  }

  static uint32_t Hash(HeapObject* obj) {
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(obj->address()));
  }

  static void* Key(HeapObject* obj) {
    return reinterpret_cast<void*>(obj->address());
  }

  static void* Value(int v) {
    return reinterpret_cast<void*>(v);
  }

  HashMap* serialization_map_;
  AssertNoAllocation* no_allocation_;
  DISALLOW_COPY_AND_ASSIGN(SerializationAddressMapper);
};


class Serializer : public SerializerDeserializer {
 public:
  explicit Serializer(SnapshotByteSink* sink);
  ~Serializer();
  void VisitPointers(Object** start, Object** end);
  // You can call this after serialization to find out how much space was used
  // in each space.
  int CurrentAllocationAddress(int space) {
    if (SpaceIsLarge(space)) return large_object_total_;
    return fullness_[space];
  }

  static void Enable() {
    if (!serialization_enabled_) {
      ASSERT(!too_late_to_enable_now_);
    }
    serialization_enabled_ = true;
  }

  static void Disable() { serialization_enabled_ = false; }
  // Call this when you have made use of the fact that there is no serialization
  // going on.
  static void TooLateToEnableNow() { too_late_to_enable_now_ = true; }
  static bool enabled() { return serialization_enabled_; }
  SerializationAddressMapper* address_mapper() { return &address_mapper_; }
#ifdef DEBUG
  virtual void Synchronize(const char* tag);
#endif

 protected:
  enum ReferenceRepresentation {
    TAGGED_REPRESENTATION,      // A tagged object reference.
    CODE_TARGET_REPRESENTATION  // A reference to first instruction in target.
  };
  static const int kInvalidRootIndex = -1;
  virtual int RootIndex(HeapObject* heap_object) = 0;
  virtual bool ShouldBeInThePartialSnapshotCache(HeapObject* o) = 0;

  class ObjectSerializer : public ObjectVisitor {
   public:
    ObjectSerializer(Serializer* serializer,
                     Object* o,
                     SnapshotByteSink* sink,
                     ReferenceRepresentation representation)
      : serializer_(serializer),
        object_(HeapObject::cast(o)),
        sink_(sink),
        reference_representation_(representation),
        bytes_processed_so_far_(0) { }
    void Serialize();
    void VisitPointers(Object** start, Object** end);
    void VisitExternalReferences(Address* start, Address* end);
    void VisitCodeTarget(RelocInfo* target);
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
    void OutputRawData(Address up_to);

    Serializer* serializer_;
    HeapObject* object_;
    SnapshotByteSink* sink_;
    ReferenceRepresentation reference_representation_;
    int bytes_processed_so_far_;
  };

  virtual void SerializeObject(Object* o,
                               ReferenceRepresentation representation) = 0;
  void SerializeReferenceToPreviousObject(
      int space,
      int address,
      ReferenceRepresentation reference_representation);
  void InitializeAllocators();
  // This will return the space for an object.  If the object is in large
  // object space it may return kLargeCode or kLargeFixedArray in order
  // to indicate to the deserializer what kind of large object allocation
  // to make.
  static int SpaceOfObject(HeapObject* object);
  // This just returns the space of the object.  It will return LO_SPACE
  // for all large objects since you can't check the type of the object
  // once the map has been used for the serialization address.
  static int SpaceOfAlreadySerializedObject(HeapObject* object);
  int Allocate(int space, int size, bool* new_page_started);
  int EncodeExternalReference(Address addr) {
    return external_reference_encoder_->Encode(addr);
  }

  // Keep track of the fullness of each space in order to generate
  // relative addresses for back references.  Large objects are
  // just numbered sequentially since relative addresses make no
  // sense in large object space.
  int fullness_[LAST_SPACE + 1];
  SnapshotByteSink* sink_;
  int current_root_index_;
  ExternalReferenceEncoder* external_reference_encoder_;
  static bool serialization_enabled_;
  // Did we already make use of the fact that serialization was not enabled?
  static bool too_late_to_enable_now_;
  int large_object_total_;
  SerializationAddressMapper address_mapper_;

  friend class ObjectSerializer;
  friend class Deserializer;

  DISALLOW_COPY_AND_ASSIGN(Serializer);
};


class PartialSerializer : public Serializer {
 public:
  PartialSerializer(Serializer* startup_snapshot_serializer,
                    SnapshotByteSink* sink)
    : Serializer(sink),
      startup_serializer_(startup_snapshot_serializer) {
  }

  // Serialize the objects reachable from a single object pointer.
  virtual void Serialize(Object** o);
  virtual void SerializeObject(Object* o,
                               ReferenceRepresentation representation);

 protected:
  virtual int RootIndex(HeapObject* o);
  virtual int PartialSnapshotCacheIndex(HeapObject* o);
  virtual bool ShouldBeInThePartialSnapshotCache(HeapObject* o) {
    // Scripts should be referred only through shared function infos.  We can't
    // allow them to be part of the partial snapshot because they contain a
    // unique ID, and deserializing several partial snapshots containing script
    // would cause dupes.
    ASSERT(!o->IsScript());
    return o->IsString() || o->IsSharedFunctionInfo() || o->IsHeapNumber();
  }

 private:
  Serializer* startup_serializer_;
  DISALLOW_COPY_AND_ASSIGN(PartialSerializer);
};


class StartupSerializer : public Serializer {
 public:
  explicit StartupSerializer(SnapshotByteSink* sink) : Serializer(sink) {
    // Clear the cache of objects used by the partial snapshot.  After the
    // strong roots have been serialized we can create a partial snapshot
    // which will repopulate the cache with objects neede by that partial
    // snapshot.
    partial_snapshot_cache_length_ = 0;
  }
  // Serialize the current state of the heap.  The order is:
  // 1) Strong references.
  // 2) Partial snapshot cache.
  // 3) Weak references (eg the symbol table).
  virtual void SerializeStrongReferences();
  virtual void SerializeObject(Object* o,
                               ReferenceRepresentation representation);
  void SerializeWeakReferences();
  void Serialize() {
    SerializeStrongReferences();
    SerializeWeakReferences();
  }

 private:
  virtual int RootIndex(HeapObject* o) { return kInvalidRootIndex; }
  virtual bool ShouldBeInThePartialSnapshotCache(HeapObject* o) {
    return false;
  }
};


} }  // namespace v8::internal

#endif  // V8_SERIALIZE_H_
