// Copyright 2006-2008 the V8 project authors. All rights reserved.
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


// A Serializer recursively visits objects to construct a serialized
// representation of the Heap stored in a string. Serialization is
// destructive. We use a similar mechanism to the GC to ensure that
// each object is visited once, namely, we modify the map pointer of
// each visited object to contain the relative address in the
// appropriate space where that object will be allocated when the heap
// is deserialized.


// Helper classes defined in serialize.cc.
class RelativeAddress;
class SimulatedHeapSpace;
class SnapshotWriter;
class ReferenceUpdater;


class Serializer: public ObjectVisitor {
 public:
  Serializer();

  virtual ~Serializer();

  // Serialize the current state of the heap. This operation destroys the
  // heap contents and the contents of the roots into the heap.
  void Serialize();

  // Returns the serialized buffer. Ownership is transferred to the
  // caller. Only the destructor and getters may be called after this call.
  void Finalize(byte** str, int* len);

  int roots() { return roots_; }
  int objects() { return objects_; }

#ifdef DEBUG
  // insert "tag" into the serialized stream
  virtual void Synchronize(const char* tag);
#endif

  static bool enabled() { return serialization_enabled_; }

  static void Enable() { serialization_enabled_ = true; }
  static void Disable() { serialization_enabled_ = false; }

 private:
  friend class ReferenceUpdater;

  virtual void VisitPointers(Object** start, Object** end);
  virtual void VisitCodeTarget(RelocInfo* rinfo);
  bool IsVisited(HeapObject* obj);

  Address GetSavedAddress(HeapObject* obj);

  void SaveAddress(HeapObject* obj, Address addr);

  void PutEncodedAddress(Address addr);
  // Write the global flags into the file.
  void PutFlags();
  // Write global information into the header of the file.
  void PutHeader();
  // Write the contents of the log into the file.
  void PutLog();
  // Serialize 'obj', and return its encoded RelativeAddress.
  Address PutObject(HeapObject* obj);
  // Write a stack of handles to the file bottom first.
  void PutGlobalHandleStack(const List<Handle<Object> >& stack);
  // Write the context stack into the file.
  void PutContextStack();

  // Return the encoded RelativeAddress where this object will be
  // allocated on deserialization. On the first visit of 'o',
  // serialize its contents. On return, *serialized will be true iff
  // 'o' has just been serialized.
  Address Encode(Object* o, bool* serialized);

  // Simulate the allocation of 'obj', returning the address where it will
  // be allocated on deserialization
  RelativeAddress Allocate(HeapObject* obj);

  void InitializeAllocators();

  SnapshotWriter* writer_;
  bool root_;  // serializing a root?
  int roots_;  // number of roots visited
  int objects_;  // number of objects serialized

  static bool serialization_enabled_;

  int flags_end_;  // The position right after the flags.

  // An array of per-space SimulatedHeapSpaces used as memory allocators.
  SimulatedHeapSpace* allocator_[LAST_SPACE+1];
  // A list of global handles at serialization time.
  List<Object**> global_handles_;

  ExternalReferenceEncoder* reference_encoder_;

  HashMap saved_addresses_;

  DISALLOW_COPY_AND_ASSIGN(Serializer);
};

// Helper class to read the bytes of the serialized heap.

class SnapshotReader {
 public:
  SnapshotReader(const byte* str, int len): str_(str), end_(str + len) {}

  void ExpectC(char expected) {
    int c = GetC();
    USE(c);
    ASSERT(c == expected);
  }

  int GetC() {
    if (str_ >= end_) return EOF;
    return *str_++;
  }

  int GetInt() {
    int result;
    GetBytes(reinterpret_cast<Address>(&result), sizeof(result));
    return result;
  }

  Address GetAddress() {
    Address result;
    GetBytes(reinterpret_cast<Address>(&result), sizeof(result));
    return result;
  }

  void GetBytes(Address a, int size) {
    ASSERT(str_ + size <= end_);
    memcpy(a, str_, size);
    str_ += size;
  }

  char* GetString() {
    ExpectC('[');
    int size = GetInt();
    ExpectC(']');
    char* s = NewArray<char>(size + 1);
    GetBytes(reinterpret_cast<Address>(s), size);
    s[size] = 0;
    return s;
  }

 private:
  const byte* str_;
  const byte* end_;
};


// A Deserializer reads a snapshot and reconstructs the Object graph it defines.


// TODO(erikcorry): Get rid of this superclass when we are using the new
// snapshot code exclusively.
class GenericDeserializer: public ObjectVisitor {
 public:
  virtual void GetLog() = 0;
  virtual void Deserialize() = 0;
};


// TODO(erikcorry): Get rid of this class.
class Deserializer: public GenericDeserializer {
 public:
  // Create a deserializer. The snapshot is held in str and has size len.
  Deserializer(const byte* str, int len);

  virtual ~Deserializer();

  // Read the flags from the header of the file, and set those that
  // should be inherited from the snapshot.
  void GetFlags();

  // Read saved profiling information from the file and log it if required.
  void GetLog();

  // Deserialize the snapshot into an empty heap.
  void Deserialize();

  int roots() { return roots_; }
  int objects() { return objects_; }

#ifdef DEBUG
  // Check for the presence of "tag" in the serialized stream
  virtual void Synchronize(const char* tag);
#endif

 private:
  virtual void VisitPointers(Object** start, Object** end);
  virtual void VisitCodeTarget(RelocInfo* rinfo);
  virtual void VisitExternalReferences(Address* start, Address* end);
  virtual void VisitRuntimeEntry(RelocInfo* rinfo);

  Address GetEncodedAddress();

  // Read other global information (except flags) from the header of the file.
  void GetHeader();
  // Read a stack of handles from the file bottom first.
  void GetGlobalHandleStack(List<Handle<Object> >* stack);
  // Read the context stack from the file.
  void GetContextStack();

  Object* GetObject();

  // Get the encoded address. In debug mode we make sure
  // it matches the given expectations.
  void ExpectEncodedAddress(Address expected);

  // Given an encoded address (the result of
  // RelativeAddress::Encode), return the object to which it points,
  // which will be either an Smi or a HeapObject in the current heap.
  Object* Resolve(Address encoded_address);

  SnapshotReader reader_;
  bool root_;  // Deserializing a root?
  int roots_;  // number of roots visited
  int objects_;  // number of objects serialized

  bool has_log_;  // The file has log information.

  // Resolve caches the following:
  List<Page*> map_pages_;  // All pages in the map space.
  List<Page*> cell_pages_;  // All pages in the cell space.
  List<Page*> old_pointer_pages_;  // All pages in the old pointer space.
  List<Page*> old_data_pages_;  // All pages in the old data space.
  List<Page*> code_pages_;  // All pages in the code space.
  List<Object*> large_objects_;    // All known large objects.
  // A list of global handles at deserialization time.
  List<Object**> global_handles_;

  ExternalReferenceDecoder* reference_decoder_;

#ifdef DEBUG
  bool expect_debug_information_;
#endif

  DISALLOW_COPY_AND_ASSIGN(Deserializer);
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

  int GetInt() {
    // A little unwind to catch the really small ints.
    int snapshot_byte = Get();
    if ((snapshot_byte & 0x80) == 0) {
      return snapshot_byte;
    }
    uintptr_t accumulator = (snapshot_byte & 0x7f) << 7;
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

  bool AtEOF() {
    return position_ == length_;
  }

 private:
  const byte* data_;
  int length_;
  int position_;
};


// The SerDes class is a common superclass for Serializer2 and Deserializer2
// which is used to store common constants and methods used by both.
// TODO(erikcorry): This should inherit from ObjectVisitor.
class SerDes: public GenericDeserializer {
 protected:
  enum DataType {
    SMI_SERIALIZATION,
    RAW_DATA_SERIALIZATION,
    OBJECT_SERIALIZATION,
    CODE_OBJECT_SERIALIZATION,
    BACKREF_SERIALIZATION,
    CODE_BACKREF_SERIALIZATION,
    EXTERNAL_REFERENCE_SERIALIZATION,
    SYNCHRONIZE
  };
  // Our Smi encoding is much more efficient for small positive integers than it
  // is for negative numbers so we add a bias before encoding and subtract it
  // after encoding so that popular small negative Smis are efficiently encoded.
  static const int kSmiBias = 16;
  static const int kLargeData = LAST_SPACE;
  static const int kLargeCode = kLargeData + 1;
  static const int kLargeFixedArray = kLargeCode + 1;
  static const int kNumberOfSpaces = kLargeFixedArray + 1;

  static inline bool SpaceIsLarge(int space) { return space >= kLargeData; }
  static inline bool SpaceIsPaged(int space) {
    return space >= FIRST_PAGED_SPACE && space <= LAST_PAGED_SPACE;
  }
};



// A Deserializer reads a snapshot and reconstructs the Object graph it defines.
class Deserializer2: public SerDes {
 public:
  // Create a deserializer from a snapshot byte source.
  explicit Deserializer2(SnapshotByteSource* source);

  virtual ~Deserializer2() { }

  // Deserialize the snapshot into an empty heap.
  void Deserialize();
  void GetLog() { }   // TODO(erikcorry): Get rid of this.
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

  int CurrentAllocationAddress(int space) {
    // The three different kinds of large objects have different tags in the
    // snapshot so the deserializer knows which kind of object to allocate,
    // but they share a fullness_ entry.
    if (SpaceIsLarge(space)) space = LO_SPACE;
    return fullness_[space];
  }

  HeapObject* GetAddress(int space);
  Address Allocate(int space, int size);
  bool ReadObject(Object** write_back);

  // Keep track of the pages in the paged spaces.
  // (In large object space we are keeping track of individual objects
  // rather than pages.)  In new space we just need the address of the
  // first object and the others will flow from that.
  List<Address> pages_[SerDes::kNumberOfSpaces];

  SnapshotByteSource* source_;
  ExternalReferenceDecoder* external_reference_decoder_;
  // Keep track of the fullness of each space in order to generate
  // relative addresses for back references.  Large objects are
  // just numbered sequentially since relative addresses make no
  // sense in large object space.
  int fullness_[LAST_SPACE + 1];

  DISALLOW_COPY_AND_ASSIGN(Deserializer2);
};


class SnapshotByteSink {
 public:
  virtual ~SnapshotByteSink() { }
  virtual void Put(int byte, const char* description) = 0;
  void PutInt(uintptr_t integer, const char* description);
};


class Serializer2 : public SerDes {
 public:
  explicit Serializer2(SnapshotByteSink* sink);
  // Serialize the current state of the heap. This operation destroys the
  // heap contents.
  void Serialize();
  void VisitPointers(Object** start, Object** end);
  void GetLog() { }       // TODO(erikcorry): Get rid of this.
  void Deserialize() { }  // TODO(erikcorry): Get rid of this.
#ifdef DEBUG
  virtual void Synchronize(const char* tag);
#endif

 private:
  enum ReferenceRepresentation {
    TAGGED_REPRESENTATION,      // A tagged object reference.
    CODE_TARGET_REPRESENTATION  // A reference to first instruction in target.
  };
  class ObjectSerializer : public ObjectVisitor {
   public:
    ObjectSerializer(Serializer2* serializer,
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

   private:
    void OutputRawData(Address up_to);

    Serializer2* serializer_;
    HeapObject* object_;
    SnapshotByteSink* sink_;
    ReferenceRepresentation reference_representation_;
    int bytes_processed_so_far_;
  };

  void SerializeObject(Object* o, ReferenceRepresentation representation);
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
  int Allocate(int space, int size);
  int CurrentAllocationAddress(int space) {
    if (SpaceIsLarge(space)) space = LO_SPACE;
    return fullness_[space];
  }
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

  friend class ObjectSerializer;
  friend class Deserializer2;

  DISALLOW_COPY_AND_ASSIGN(Serializer2);
};

} }  // namespace v8::internal

#endif  // V8_SERIALIZE_H_
