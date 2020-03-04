// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_CODE_SERIALIZER_H_
#define V8_SNAPSHOT_CODE_SERIALIZER_H_

#include "src/base/macros.h"
#include "src/snapshot/serializer.h"

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE ScriptData {
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

class CodeSerializer : public Serializer {
 public:
  V8_EXPORT_PRIVATE static ScriptCompiler::CachedData* Serialize(
      Handle<SharedFunctionInfo> info);

  ScriptData* SerializeSharedFunctionInfo(Handle<SharedFunctionInfo> info);

  V8_WARN_UNUSED_RESULT static MaybeHandle<SharedFunctionInfo> Deserialize(
      Isolate* isolate, ScriptData* cached_data, Handle<String> source,
      ScriptOriginOptions origin_options);

  uint32_t source_hash() const { return source_hash_; }

 protected:
  CodeSerializer(Isolate* isolate, uint32_t source_hash);
  ~CodeSerializer() override { OutputStatistics("CodeSerializer"); }

  virtual bool ElideObject(Object obj) { return false; }
  void SerializeGeneric(HeapObject heap_object);

 private:
  void SerializeObject(HeapObject o) override;

  bool SerializeReadOnlyObject(HeapObject obj);

  DISALLOW_HEAP_ALLOCATION(no_gc_)
  uint32_t source_hash_;
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
    FLAGS_MISMATCH = 5,
    CHECKSUM_MISMATCH = 6,
    INVALID_HEADER = 7,
    LENGTH_MISMATCH = 8
  };

  // The data header consists of uint32_t-sized entries:
  // [0] magic number and (internally provided) external reference count
  // [1] version hash
  // [2] source hash
  // [3] flag hash
  // [4] number of reservation size entries
  // [5] payload length
  // [6] payload checksum
  // ...  reservations
  // ...  code stub keys
  // ...  serialized payload
  static const uint32_t kVersionHashOffset = kMagicNumberOffset + kUInt32Size;
  static const uint32_t kSourceHashOffset = kVersionHashOffset + kUInt32Size;
  static const uint32_t kFlagHashOffset = kSourceHashOffset + kUInt32Size;
  static const uint32_t kNumReservationsOffset = kFlagHashOffset + kUInt32Size;
  static const uint32_t kPayloadLengthOffset =
      kNumReservationsOffset + kUInt32Size;
  static const uint32_t kChecksumOffset = kPayloadLengthOffset + kUInt32Size;
  static const uint32_t kUnalignedHeaderSize = kChecksumOffset + kUInt32Size;
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
