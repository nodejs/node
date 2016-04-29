// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_CODE_SERIALIZER_H_
#define V8_SNAPSHOT_CODE_SERIALIZER_H_

#include "src/parsing/preparse-data.h"
#include "src/snapshot/serializer.h"

namespace v8 {
namespace internal {

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

#endif  // V8_SNAPSHOT_CODE_SERIALIZER_H_
