// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SNAPSHOT_H_
#define V8_SNAPSHOT_SNAPSHOT_H_

#include "src/snapshot/partial-serializer.h"
#include "src/snapshot/startup-serializer.h"

#include "src/utils/utils.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Isolate;
class PartialSerializer;
class SnapshotCompression;
class StartupSerializer;

// Wrapper around reservation sizes and the serialization payload.
class V8_EXPORT_PRIVATE SnapshotData : public SerializedData {
 public:
  // Used when producing.
  explicit SnapshotData(const Serializer* serializer);

  // Used when consuming.
  explicit SnapshotData(const Vector<const byte> snapshot)
      : SerializedData(const_cast<byte*>(snapshot.begin()), snapshot.length()) {
  }

  std::vector<Reservation> Reservations() const;
  virtual Vector<const byte> Payload() const;

  Vector<const byte> RawData() const {
    return Vector<const byte>(data_, size_);
  }

 protected:
  // Empty constructor used by SnapshotCompression so it can manually allocate
  // memory.
  SnapshotData() : SerializedData() {}
  friend class SnapshotCompression;

  // Resize used by SnapshotCompression so it can shrink the compressed
  // SnapshotData.
  void Resize(uint32_t size) { size_ = size; }

  // The data header consists of uint32_t-sized entries:
  // [0] magic number and (internal) external reference count
  // [1] number of reservation size entries
  // [2] payload length
  // ... reservations
  // ... serialized payload
  static const uint32_t kNumReservationsOffset =
      kMagicNumberOffset + kUInt32Size;
  static const uint32_t kPayloadLengthOffset =
      kNumReservationsOffset + kUInt32Size;
  static const uint32_t kHeaderSize = kPayloadLengthOffset + kUInt32Size;
};

class Snapshot : public AllStatic {
 public:
  // ---------------- Deserialization ----------------

  // Initialize the Isolate from the internal snapshot. Returns false if no
  // snapshot could be found.
  static bool Initialize(Isolate* isolate);

  // Create a new context using the internal partial snapshot.
  static MaybeHandle<Context> NewContextFromSnapshot(
      Isolate* isolate, Handle<JSGlobalProxy> global_proxy,
      size_t context_index,
      v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer);

  // ---------------- Helper methods ----------------

  static bool HasContextSnapshot(Isolate* isolate, size_t index);
  static bool EmbedsScript(Isolate* isolate);

  // To be implemented by the snapshot source.
  static const v8::StartupData* DefaultSnapshotBlob();

  V8_EXPORT_PRIVATE static bool VerifyChecksum(const v8::StartupData* data);

  // ---------------- Serialization ----------------

  static v8::StartupData CreateSnapshotBlob(
      const SnapshotData* startup_snapshot_in,
      const SnapshotData* read_only_snapshot_in,
      const std::vector<SnapshotData*>& context_snapshots_in,
      bool can_be_rehashed);

#ifdef DEBUG
  static bool SnapshotIsValid(const v8::StartupData* snapshot_blob);
#endif  // DEBUG

  static bool ExtractRehashability(const v8::StartupData* data);

 private:
  static uint32_t ExtractNumContexts(const v8::StartupData* data);
  static uint32_t ExtractContextOffset(const v8::StartupData* data,
                                       uint32_t index);
  static Vector<const byte> ExtractStartupData(const v8::StartupData* data);
  static Vector<const byte> ExtractReadOnlyData(const v8::StartupData* data);
  static Vector<const byte> ExtractContextData(const v8::StartupData* data,
                                               uint32_t index);

  static uint32_t GetHeaderValue(const v8::StartupData* data, uint32_t offset) {
    return base::ReadLittleEndianValue<uint32_t>(
        reinterpret_cast<Address>(data->data) + offset);
  }
  static void SetHeaderValue(char* data, uint32_t offset, uint32_t value) {
    base::WriteLittleEndianValue(reinterpret_cast<Address>(data) + offset,
                                 value);
  }

  static void CheckVersion(const v8::StartupData* data);

  // Snapshot blob layout:
  // [0] number of contexts N
  // [1] rehashability
  // [2] checksum
  // [3] (128 bytes) version string
  // [4] offset to readonly
  // [5] offset to context 0
  // [6] offset to context 1
  // ...
  // ... offset to context N - 1
  // ... startup snapshot data
  // ... read-only snapshot data
  // ... context 0 snapshot data
  // ... context 1 snapshot data

  static const uint32_t kNumberOfContextsOffset = 0;
  // TODO(yangguo): generalize rehashing, and remove this flag.
  static const uint32_t kRehashabilityOffset =
      kNumberOfContextsOffset + kUInt32Size;
  static const uint32_t kChecksumOffset = kRehashabilityOffset + kUInt32Size;
  static const uint32_t kVersionStringOffset = kChecksumOffset + kUInt32Size;
  static const uint32_t kVersionStringLength = 64;
  static const uint32_t kReadOnlyOffsetOffset =
      kVersionStringOffset + kVersionStringLength;
  static const uint32_t kFirstContextOffsetOffset =
      kReadOnlyOffsetOffset + kUInt32Size;

  static Vector<const byte> ChecksummedContent(const v8::StartupData* data) {
    STATIC_ASSERT(kVersionStringOffset == kChecksumOffset + kUInt32Size);
    const uint32_t kChecksumStart = kVersionStringOffset;
    return Vector<const byte>(
        reinterpret_cast<const byte*>(data->data + kChecksumStart),
        data->raw_size - kChecksumStart);
  }

  static uint32_t StartupSnapshotOffset(int num_contexts) {
    return POINTER_SIZE_ALIGN(kFirstContextOffsetOffset +
                              num_contexts * kInt32Size);
  }

  static uint32_t ContextSnapshotOffsetOffset(int index) {
    return kFirstContextOffsetOffset + index * kInt32Size;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(Snapshot);
};

// Convenience wrapper around snapshot data blob creation used e.g. by tests and
// mksnapshot.
V8_EXPORT_PRIVATE v8::StartupData CreateSnapshotDataBlobInternal(
    v8::SnapshotCreator::FunctionCodeHandling function_code_handling,
    const char* embedded_source, v8::Isolate* isolate = nullptr);

// Convenience wrapper around snapshot data blob warmup used e.g. by tests and
// mksnapshot.
V8_EXPORT_PRIVATE v8::StartupData WarmUpSnapshotDataBlobInternal(
    v8::StartupData cold_snapshot_blob, const char* warmup_source);

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
void SetSnapshotFromFile(StartupData* snapshot_blob);
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SNAPSHOT_H_
