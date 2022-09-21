// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_CODE_SERIALIZER_H_
#define V8_SNAPSHOT_CODE_SERIALIZER_H_

#include "src/base/macros.h"
#include "src/snapshot/serializer.h"
#include "src/snapshot/snapshot-data.h"

namespace v8 {
namespace internal {

class PersistentHandles;
class BackgroundMergeTask;

class V8_EXPORT_PRIVATE AlignedCachedData {
 public:
  AlignedCachedData(const byte* data, int length);
  ~AlignedCachedData() {
    if (owns_data_) DeleteArray(data_);
  }
  AlignedCachedData(const AlignedCachedData&) = delete;
  AlignedCachedData& operator=(const AlignedCachedData&) = delete;

  const byte* data() const { return data_; }
  int length() const { return length_; }
  bool rejected() const { return rejected_; }

  void Reject() { rejected_ = true; }

  bool HasDataOwnership() const { return owns_data_; }

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
};

enum class SerializedCodeSanityCheckResult {
  kSuccess = 0,
  kMagicNumberMismatch = 1,
  kVersionMismatch = 2,
  kSourceMismatch = 3,
  kFlagsMismatch = 5,
  kChecksumMismatch = 6,
  kInvalidHeader = 7,
  kLengthMismatch = 8
};

class CodeSerializer : public Serializer {
 public:
  struct OffThreadDeserializeData {
   public:
    bool HasResult() const { return !maybe_result.is_null(); }
    Handle<Script> GetOnlyScript(LocalHeap* heap);

   private:
    friend class CodeSerializer;
    MaybeHandle<SharedFunctionInfo> maybe_result;
    std::vector<Handle<Script>> scripts;
    std::unique_ptr<PersistentHandles> persistent_handles;
    SerializedCodeSanityCheckResult sanity_check_result;
  };

  CodeSerializer(const CodeSerializer&) = delete;
  CodeSerializer& operator=(const CodeSerializer&) = delete;
  V8_EXPORT_PRIVATE static ScriptCompiler::CachedData* Serialize(
      Handle<SharedFunctionInfo> info);

  AlignedCachedData* SerializeSharedFunctionInfo(
      Handle<SharedFunctionInfo> info);

  V8_WARN_UNUSED_RESULT static MaybeHandle<SharedFunctionInfo> Deserialize(
      Isolate* isolate, AlignedCachedData* cached_data, Handle<String> source,
      ScriptOriginOptions origin_options);

  V8_WARN_UNUSED_RESULT static OffThreadDeserializeData
  StartDeserializeOffThread(LocalIsolate* isolate,
                            AlignedCachedData* cached_data);

  V8_WARN_UNUSED_RESULT static MaybeHandle<SharedFunctionInfo>
  FinishOffThreadDeserialize(
      Isolate* isolate, OffThreadDeserializeData&& data,
      AlignedCachedData* cached_data, Handle<String> source,
      ScriptOriginOptions origin_options,
      BackgroundMergeTask* background_merge_task = nullptr);

  uint32_t source_hash() const { return source_hash_; }

 protected:
  CodeSerializer(Isolate* isolate, uint32_t source_hash);
  ~CodeSerializer() override { OutputStatistics("CodeSerializer"); }

  virtual bool ElideObject(Object obj) { return false; }
  void SerializeGeneric(Handle<HeapObject> heap_object);

 private:
  void SerializeObjectImpl(Handle<HeapObject> o) override;

  bool SerializeReadOnlyObject(HeapObject obj,
                               const DisallowGarbageCollection& no_gc);

  DISALLOW_GARBAGE_COLLECTION(no_gc_)
  uint32_t source_hash_;
};

// Wrapper around ScriptData to provide code-serializer-specific functionality.
class SerializedCodeData : public SerializedData {
 public:
  // The data header consists of uint32_t-sized entries:
  // [0] magic number and (internally provided) external reference count
  // [1] version hash
  // [2] source hash
  // [3] flag hash
  // [4] payload length
  // [5] payload checksum
  // ...  serialized payload
  static const uint32_t kVersionHashOffset = kMagicNumberOffset + kUInt32Size;
  static const uint32_t kSourceHashOffset = kVersionHashOffset + kUInt32Size;
  static const uint32_t kFlagHashOffset = kSourceHashOffset + kUInt32Size;
  static const uint32_t kPayloadLengthOffset = kFlagHashOffset + kUInt32Size;
  static const uint32_t kChecksumOffset = kPayloadLengthOffset + kUInt32Size;
  static const uint32_t kUnalignedHeaderSize = kChecksumOffset + kUInt32Size;
  static const uint32_t kHeaderSize = POINTER_SIZE_ALIGN(kUnalignedHeaderSize);

  // Used when consuming.
  static SerializedCodeData FromCachedData(
      AlignedCachedData* cached_data, uint32_t expected_source_hash,
      SerializedCodeSanityCheckResult* rejection_result);
  // For cached data which is consumed before the source is available (e.g.
  // off-thread).
  static SerializedCodeData FromCachedDataWithoutSource(
      AlignedCachedData* cached_data,
      SerializedCodeSanityCheckResult* rejection_result);
  // For cached data which was previously already sanity checked by
  // FromCachedDataWithoutSource. The rejection result from that call should be
  // passed into this one.
  static SerializedCodeData FromPartiallySanityCheckedCachedData(
      AlignedCachedData* cached_data, uint32_t expected_source_hash,
      SerializedCodeSanityCheckResult* rejection_result);

  // Used when producing.
  SerializedCodeData(const std::vector<byte>* payload,
                     const CodeSerializer* cs);

  // Return ScriptData object and relinquish ownership over it to the caller.
  AlignedCachedData* GetScriptData();

  base::Vector<const byte> Payload() const;

  static uint32_t SourceHash(Handle<String> source,
                             ScriptOriginOptions origin_options);

 private:
  explicit SerializedCodeData(AlignedCachedData* data);
  SerializedCodeData(const byte* data, int size)
      : SerializedData(const_cast<byte*>(data), size) {}

  base::Vector<const byte> ChecksummedContent() const {
    return base::Vector<const byte>(data_ + kHeaderSize, size_ - kHeaderSize);
  }

  SerializedCodeSanityCheckResult SanityCheck(
      uint32_t expected_source_hash) const;
  SerializedCodeSanityCheckResult SanityCheckJustSource(
      uint32_t expected_source_hash) const;
  SerializedCodeSanityCheckResult SanityCheckWithoutSource() const;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_CODE_SERIALIZER_H_
