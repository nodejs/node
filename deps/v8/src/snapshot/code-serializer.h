// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_CODE_SERIALIZER_H_
#define V8_SNAPSHOT_CODE_SERIALIZER_H_

#include <array>
#include <optional>

#include "src/base/macros.h"
#include "src/codegen/script-details.h"
#include "src/snapshot/serializer.h"
#include "src/snapshot/snapshot-data.h"
#include "src/utils/sha-256.h"

namespace v8 {
namespace internal {

class PersistentHandles;
class BackgroundMergeTask;

class V8_EXPORT_PRIVATE AlignedCachedData {
 public:
  AlignedCachedData(const uint8_t* data, int length);
  ~AlignedCachedData() {
    if (owns_data_) DeleteArray(data_);
  }
  AlignedCachedData(const AlignedCachedData&) = delete;
  AlignedCachedData& operator=(const AlignedCachedData&) = delete;

  const uint8_t* data() const { return data_; }
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
  const uint8_t* data_;
  int length_;
};

typedef v8::ScriptCompiler::CachedData::CompatibilityCheckResult
    SerializedCodeSanityCheckResult;

// If this fails, update the static_assert AND the code_cache_reject_reason
// histogram definition.
static_assert(static_cast<int>(SerializedCodeSanityCheckResult::kLast) == 9);

class CodeSerializer;

struct OffThreadDeserializeData {
 public:
  bool HasResult() const { return !maybe_result.is_null(); }
  DirectHandle<Script> GetOnlyScript(LocalHeap* heap);

 private:
  friend class CodeSerializer;
  friend class SerializedCodeData;
  MaybeIndirectHandle<SharedFunctionInfo> maybe_result;
  std::vector<IndirectHandle<Script>> scripts;
  std::unique_ptr<PersistentHandles> persistent_handles;
  SerializedCodeSanityCheckResult sanity_check_result =
      SerializedCodeSanityCheckResult::kSuccess;
  std::optional<SerializedCodeSanityCheckResult> source_sanity_check_result;
};

// Wrapper around ScriptData to provide code-serializer-specific functionality.
class SerializedCodeData : public SerializedData {
 public:
  class SourceHash {
   public:
    V8_EXPORT_PRIVATE SourceHash(DirectHandle<String> source,
                                 DirectHandle<FixedArray> wrapped_arguments,
                                 ScriptOriginOptions origin_options,
                                 Isolate* isolate);

    static constexpr uint32_t kSize = kSizeOfSha256Digest;
    const uint8_t* data() const { return data_.data(); }

    bool operator==(const SourceHash& other) const {
      return data_ == other.data_;
    }
    bool operator!=(const SourceHash& other) const { return !(*this == other); }

   private:
    friend class SerializedCodeData;
    explicit SourceHash(const uint8_t* bytes) {
      std::copy_n(bytes, kSizeOfSha256Digest, data_.data());
    }

    std::array<uint8_t, kSizeOfSha256Digest> data_;
  };

  // The data header consists of uint32_t-sized entries (except for the 256-bit
  // source hash):
  static constexpr uint32_t kVersionHashOffset =
      kMagicNumberOffset + kUInt32Size;
  static constexpr uint32_t kSourceHashOffset =
      kVersionHashOffset + kUInt32Size;
  static constexpr uint32_t kFlagHashOffset =
      kSourceHashOffset + kSizeOfSha256Digest;
  static constexpr uint32_t kReadOnlySnapshotChecksumOffset =
      kFlagHashOffset + kUInt32Size;
  static constexpr uint32_t kPayloadLengthOffset =
      kReadOnlySnapshotChecksumOffset + kUInt32Size;
  static constexpr uint32_t kTrustedPayloadLengthOffset =
      kPayloadLengthOffset + kUInt32Size;
  static constexpr uint32_t kChecksumOffset =
      kTrustedPayloadLengthOffset + kUInt32Size;
  static constexpr uint32_t kUnalignedHeaderSize =
      kChecksumOffset + kUInt32Size;
  static constexpr uint32_t kHeaderSize =
      POINTER_SIZE_ALIGN(kUnalignedHeaderSize);

  // Used when consuming.
  static SerializedCodeData FromCachedData(
      Isolate* isolate, AlignedCachedData* cached_data,
      SourceHash expected_source_hash,
      SerializedCodeSanityCheckResult* rejection_result);
  // For cached data which is consumed before the source is available (e.g.
  // off-thread).
  static SerializedCodeData FromCachedDataWithoutSource(
      LocalIsolate* local_isolate, AlignedCachedData* cached_data,
      SerializedCodeSanityCheckResult* rejection_result);
  // For cached data which was previously already sanity checked by
  // FromCachedDataWithoutSource. The rejection result from that call should be
  // passed into this one.
  static SerializedCodeData FromPartiallySanityCheckedCachedData(
      Isolate* isolate, AlignedCachedData* cached_data,
      const OffThreadDeserializeData& data, DirectHandle<String> source,
      const ScriptDetails& script_details,
      SerializedCodeSanityCheckResult* rejection_result);

  // Used when producing.
  SerializedCodeData(const std::vector<uint8_t>* untrusted_payload,
                     const std::vector<uint8_t>* trusted_payload,
                     const CodeSerializer* cs);

  // Return ScriptData object and relinquish ownership over it to the caller.
  AlignedCachedData* GetScriptData();

  base::Vector<const uint8_t> UntrustedPayload() const;
  base::Vector<const uint8_t> TrustedPayload() const;

 private:
  friend class CodeSerializer;
  explicit SerializedCodeData(const AlignedCachedData* data);
  SerializedCodeData(const uint8_t* data, int size)
      : SerializedData(const_cast<uint8_t*>(data), size) {}

  void SetHeaderSourceHash(const SourceHash& hash);
  SourceHash GetHeaderSourceHash() const;

  base::Vector<const uint8_t> ChecksummedContent() const {
    return base::Vector<const uint8_t>(data_ + kHeaderSize,
                                       size_ - kHeaderSize);
  }

  SerializedCodeSanityCheckResult SanityCheck(
      uint32_t expected_ro_snapshot_checksum,
      SourceHash expected_source_hash) const;
  SerializedCodeSanityCheckResult SanityCheckJustSource(
      Isolate* isolate, DirectHandle<String> source,
      const ScriptDetails& script_details) const;
  SerializedCodeSanityCheckResult SanityCheckJustSource(
      SourceHash expected_source_hash) const;
  SerializedCodeSanityCheckResult SanityCheckWithoutSource(
      uint32_t expected_ro_snapshot_checksum) const;
};

class CodeSerializer : public Serializer {
 public:
  using SourceHash = SerializedCodeData::SourceHash;

  CodeSerializer(const CodeSerializer&) = delete;
  CodeSerializer& operator=(const CodeSerializer&) = delete;
  V8_EXPORT_PRIVATE static ScriptCompiler::CachedData* Serialize(
      Isolate* isolate, Handle<SharedFunctionInfo> info);

  AlignedCachedData* SerializeSharedFunctionInfo(
      Handle<SharedFunctionInfo> info);

  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<SharedFunctionInfo>
  Deserialize(Isolate* isolate, AlignedCachedData* cached_data,
              DirectHandle<String> source, const ScriptDetails& script_details,
              MaybeDirectHandle<Script> maybe_cached_script = {});

  static void StartDeserializeOffThread(LocalIsolate* isolate,
                                        AlignedCachedData* cached_data,
                                        OffThreadDeserializeData* data);

  V8_WARN_UNUSED_RESULT static bool NotifySourceTextAvailable(
      Isolate* isolate, OffThreadDeserializeData* data,
      const AlignedCachedData* cached_data, DirectHandle<String> source,
      const ScriptDetails& script_details);

  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<SharedFunctionInfo>
  FinishOffThreadDeserialize(
      Isolate* isolate, OffThreadDeserializeData&& data,
      AlignedCachedData* cached_data, DirectHandle<String> source,
      const ScriptDetails& script_details,
      BackgroundMergeTask* background_merge_task = nullptr);

  SourceHash source_hash() const { return source_hash_; }

 protected:
  CodeSerializer(Isolate* isolate, SourceHash source_hash);
  ~CodeSerializer() override { OutputStatistics("CodeSerializer"); }

  void SerializeGeneric(Handle<HeapObject> heap_object, SlotType slot_type);

 private:
  class TrustedSectionSerializer final : public Serializer {
   public:
    explicit TrustedSectionSerializer(Isolate* isolate)
        : Serializer(isolate, Snapshot::kDefaultSerializerFlags) {}
    ~TrustedSectionSerializer() override {
      OutputStatistics("TrustedSectionSerializer");
    }

    void FinishSection() {
      sink_.Put(kSynchronize, "EndOfTrustedSection");
      Pad();
    }

   private:
    void SerializeObjectImpl(Handle<HeapObject> o,
                             SlotType slot_type) override {
      UNREACHABLE();
    }
  };

  void SerializeObjectImpl(Handle<HeapObject> o, SlotType slot_type) override;
  void FinishSection() {
    sink_.Put(kSynchronize, "EndOfUntrustedSection");
    Pad();
  }

  DISALLOW_GARBAGE_COLLECTION(no_gc_)
  SourceHash source_hash_;
  TrustedSectionSerializer trusted_serializer_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_CODE_SERIALIZER_H_
