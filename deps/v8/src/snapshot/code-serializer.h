// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_CODE_SERIALIZER_H_
#define V8_SNAPSHOT_CODE_SERIALIZER_H_

#include "src/snapshot/serializer.h"

namespace v8 {
namespace internal {

class ScriptData {
 public:
  ScriptData(const byte* data, int length);
  ~ScriptData() {
    if (owns_data_) DeleteArray(data_);
  }

  const byte* data() const { return data_; }
  int length() const { return length_; }
  bool rejected() const { return rejected_; }

  void Reject() { rejected_ = true; }

  void AcquireDataOwnership() {
    DCHECK(!owns_data_);
    owns_data_ = true;
  }

  void ReleaseDataOwnership() {
    DCHECK(owns_data_);
    owns_data_ = false;
  }

 private:
  bool owns_data_ : 1;
  bool rejected_ : 1;
  const byte* data_;
  int length_;

  DISALLOW_COPY_AND_ASSIGN(ScriptData);
};

class CodeSerializer : public Serializer<> {
 public:
  static ScriptCompiler::CachedData* Serialize(Handle<SharedFunctionInfo> info);

  ScriptData* SerializeSharedFunctionInfo(Handle<SharedFunctionInfo> info);

  V8_WARN_UNUSED_RESULT static MaybeHandle<SharedFunctionInfo> Deserialize(
      Isolate* isolate, ScriptData* cached_data, Handle<String> source,
      ScriptOriginOptions origin_options);

  const std::vector<uint32_t>* stub_keys() const { return &stub_keys_; }

  uint32_t source_hash() const { return source_hash_; }

 protected:
  CodeSerializer(Isolate* isolate, uint32_t source_hash);
  ~CodeSerializer() override { OutputStatistics("CodeSerializer"); }

  virtual void SerializeCodeObject(Code* code_object, HowToCode how_to_code,
                                   WhereToPoint where_to_point) {
    UNREACHABLE();
  }

  virtual bool ElideObject(Object* obj) { return false; }
  void SerializeGeneric(HeapObject* heap_object, HowToCode how_to_code,
                        WhereToPoint where_to_point);

 private:
  void SerializeObject(HeapObject* o, HowToCode how_to_code,
                       WhereToPoint where_to_point, int skip) override;

  void SerializeCodeStub(Code* code_stub, HowToCode how_to_code,
                         WhereToPoint where_to_point);

  bool SerializeReadOnlyObject(HeapObject* obj, HowToCode how_to_code,
                               WhereToPoint where_to_point, int skip);

  DisallowHeapAllocation no_gc_;
  uint32_t source_hash_;
  std::vector<uint32_t> stub_keys_;
  DISALLOW_COPY_AND_ASSIGN(CodeSerializer);
};

// Wrapper around ScriptData to provide code-serializer-specific functionality.
class SerializedCodeData : public SerializedData {
 public:
  enum SanityCheckResult {
    CHECK_SUCCESS = 0,
    MAGIC_NUMBER_MISMATCH = 1,
    VERSION_MISMATCH = 2,
    SOURCE_MISMATCH = 3,
    CPU_FEATURES_MISMATCH = 4,
    FLAGS_MISMATCH = 5,
    CHECKSUM_MISMATCH = 6,
    INVALID_HEADER = 7,
    LENGTH_MISMATCH = 8
  };

  // The data header consists of uint32_t-sized entries:
  // [0] magic number and (internally provided) external reference count
  // [1] extra (API-provided) external reference count
  // [2] version hash
  // [3] source hash
  // [4] cpu features
  // [5] flag hash
  // [6] number of code stub keys
  // [7] number of reservation size entries
  // [8] payload length
  // [9] payload checksum part A
  // [10] payload checksum part B
  // ...  reservations
  // ...  code stub keys
  // ...  serialized payload
  static const uint32_t kVersionHashOffset = kMagicNumberOffset + kUInt32Size;
  static const uint32_t kSourceHashOffset = kVersionHashOffset + kUInt32Size;
  static const uint32_t kCpuFeaturesOffset = kSourceHashOffset + kUInt32Size;
  static const uint32_t kFlagHashOffset = kCpuFeaturesOffset + kUInt32Size;
  static const uint32_t kNumReservationsOffset = kFlagHashOffset + kUInt32Size;
  static const uint32_t kNumCodeStubKeysOffset =
      kNumReservationsOffset + kUInt32Size;
  static const uint32_t kPayloadLengthOffset =
      kNumCodeStubKeysOffset + kUInt32Size;
  static const uint32_t kChecksumPartAOffset =
      kPayloadLengthOffset + kUInt32Size;
  static const uint32_t kChecksumPartBOffset =
      kChecksumPartAOffset + kUInt32Size;
  static const uint32_t kUnalignedHeaderSize =
      kChecksumPartBOffset + kUInt32Size;
  static const uint32_t kHeaderSize = POINTER_SIZE_ALIGN(kUnalignedHeaderSize);

  // Used when consuming.
  static SerializedCodeData FromCachedData(Isolate* isolate,
                                           ScriptData* cached_data,
                                           uint32_t expected_source_hash,
                                           SanityCheckResult* rejection_result);

  // Used when producing.
  SerializedCodeData(const std::vector<byte>* payload,
                     const CodeSerializer* cs);

  // Return ScriptData object and relinquish ownership over it to the caller.
  ScriptData* GetScriptData();

  std::vector<Reservation> Reservations() const;
  Vector<const byte> Payload() const;

  Vector<const uint32_t> CodeStubKeys() const;

  static uint32_t SourceHash(Handle<String> source,
                             ScriptOriginOptions origin_options);

 private:
  explicit SerializedCodeData(ScriptData* data);
  SerializedCodeData(const byte* data, int size)
      : SerializedData(const_cast<byte*>(data), size) {}

  Vector<const byte> ChecksummedContent() const {
    return Vector<const byte>(data_ + kHeaderSize, size_ - kHeaderSize);
  }

  SanityCheckResult SanityCheck(Isolate* isolate,
                                uint32_t expected_source_hash) const;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_CODE_SERIALIZER_H_
