// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The common functionality when building with or without snapshots.

#include "src/snapshot/snapshot.h"

#include "src/base/platform/platform.h"
#include "src/counters.h"
#include "src/snapshot/partial-deserializer.h"
#include "src/snapshot/read-only-deserializer.h"
#include "src/snapshot/startup-deserializer.h"
#include "src/version.h"

namespace v8 {
namespace internal {

#ifdef DEBUG
bool Snapshot::SnapshotIsValid(const v8::StartupData* snapshot_blob) {
  return Snapshot::ExtractNumContexts(snapshot_blob) > 0;
}
#endif  // DEBUG

bool Snapshot::HasContextSnapshot(Isolate* isolate, size_t index) {
  // Do not use snapshots if the isolate is used to create snapshots.
  const v8::StartupData* blob = isolate->snapshot_blob();
  if (blob == nullptr) return false;
  if (blob->data == nullptr) return false;
  size_t num_contexts = static_cast<size_t>(ExtractNumContexts(blob));
  return index < num_contexts;
}

bool Snapshot::Initialize(Isolate* isolate) {
  if (!isolate->snapshot_available()) return false;
  RuntimeCallTimerScope rcs_timer(isolate,
                                  RuntimeCallCounterId::kDeserializeIsolate);
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  const v8::StartupData* blob = isolate->snapshot_blob();
  CheckVersion(blob);
  CHECK(VerifyChecksum(blob));
  Vector<const byte> startup_data = ExtractStartupData(blob);
  SnapshotData startup_snapshot_data(startup_data);
  Vector<const byte> read_only_data = ExtractReadOnlyData(blob);
  SnapshotData read_only_snapshot_data(read_only_data);
  StartupDeserializer startup_deserializer(&startup_snapshot_data);
  ReadOnlyDeserializer read_only_deserializer(&read_only_snapshot_data);
  startup_deserializer.SetRehashability(ExtractRehashability(blob));
  read_only_deserializer.SetRehashability(ExtractRehashability(blob));
  bool success =
      isolate->InitWithSnapshot(&read_only_deserializer, &startup_deserializer);
  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int bytes = startup_data.length();
    PrintF("[Deserializing isolate (%d bytes) took %0.3f ms]\n", bytes, ms);
  }
  return success;
}

MaybeHandle<Context> Snapshot::NewContextFromSnapshot(
    Isolate* isolate, Handle<JSGlobalProxy> global_proxy, size_t context_index,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer) {
  if (!isolate->snapshot_available()) return Handle<Context>();
  RuntimeCallTimerScope rcs_timer(isolate,
                                  RuntimeCallCounterId::kDeserializeContext);
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  const v8::StartupData* blob = isolate->snapshot_blob();
  bool can_rehash = ExtractRehashability(blob);
  Vector<const byte> context_data =
      ExtractContextData(blob, static_cast<uint32_t>(context_index));
  SnapshotData snapshot_data(context_data);

  MaybeHandle<Context> maybe_result = PartialDeserializer::DeserializeContext(
      isolate, &snapshot_data, can_rehash, global_proxy,
      embedder_fields_deserializer);

  Handle<Context> result;
  if (!maybe_result.ToHandle(&result)) return MaybeHandle<Context>();

  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int bytes = context_data.length();
    PrintF("[Deserializing context #%zu (%d bytes) took %0.3f ms]\n",
           context_index, bytes, ms);
  }
  return result;
}

void ProfileDeserialization(
    const SnapshotData* read_only_snapshot,
    const SnapshotData* startup_snapshot,
    const std::vector<SnapshotData*>& context_snapshots) {
  if (FLAG_profile_deserialization) {
    int startup_total = 0;
    PrintF("Deserialization will reserve:\n");
    for (const auto& reservation : read_only_snapshot->Reservations()) {
      startup_total += reservation.chunk_size();
    }
    for (const auto& reservation : startup_snapshot->Reservations()) {
      startup_total += reservation.chunk_size();
    }
    PrintF("%10d bytes per isolate\n", startup_total);
    for (size_t i = 0; i < context_snapshots.size(); i++) {
      int context_total = 0;
      for (const auto& reservation : context_snapshots[i]->Reservations()) {
        context_total += reservation.chunk_size();
      }
      PrintF("%10d bytes per context #%zu\n", context_total, i);
    }
  }
}

v8::StartupData Snapshot::CreateSnapshotBlob(
    const SnapshotData* startup_snapshot,
    const SnapshotData* read_only_snapshot,
    const std::vector<SnapshotData*>& context_snapshots, bool can_be_rehashed) {
  uint32_t num_contexts = static_cast<uint32_t>(context_snapshots.size());
  uint32_t startup_snapshot_offset = StartupSnapshotOffset(num_contexts);
  uint32_t total_length = startup_snapshot_offset;
  DCHECK(IsAligned(total_length, kPointerAlignment));
  total_length += static_cast<uint32_t>(startup_snapshot->RawData().length());
  DCHECK(IsAligned(total_length, kPointerAlignment));
  total_length += static_cast<uint32_t>(read_only_snapshot->RawData().length());
  DCHECK(IsAligned(total_length, kPointerAlignment));
  for (const auto context_snapshot : context_snapshots) {
    total_length += static_cast<uint32_t>(context_snapshot->RawData().length());
    DCHECK(IsAligned(total_length, kPointerAlignment));
  }

  ProfileDeserialization(read_only_snapshot, startup_snapshot,
                         context_snapshots);

  char* data = new char[total_length];
  // Zero out pre-payload data. Part of that is only used for padding.
  memset(data, 0, StartupSnapshotOffset(num_contexts));

  SetHeaderValue(data, kNumberOfContextsOffset, num_contexts);
  SetHeaderValue(data, kRehashabilityOffset, can_be_rehashed ? 1 : 0);

  // Write version string into snapshot data.
  memset(data + kVersionStringOffset, 0, kVersionStringLength);
  Version::GetString(
      Vector<char>(data + kVersionStringOffset, kVersionStringLength));

  // Startup snapshot (isolate-specific data).
  uint32_t payload_offset = startup_snapshot_offset;
  uint32_t payload_length =
      static_cast<uint32_t>(startup_snapshot->RawData().length());
  CopyBytes(data + payload_offset,
            reinterpret_cast<const char*>(startup_snapshot->RawData().start()),
            payload_length);
  if (FLAG_profile_deserialization) {
    PrintF("Snapshot blob consists of:\n%10d bytes in %d chunks for startup\n",
           payload_length,
           static_cast<uint32_t>(startup_snapshot->Reservations().size()));
  }
  payload_offset += payload_length;

  // Read-only.
  SetHeaderValue(data, kReadOnlyOffsetOffset, payload_offset);
  payload_length = read_only_snapshot->RawData().length();
  CopyBytes(
      data + payload_offset,
      reinterpret_cast<const char*>(read_only_snapshot->RawData().start()),
      payload_length);
  if (FLAG_profile_deserialization) {
    PrintF("%10d bytes for read-only\n", payload_length);
  }
  payload_offset += payload_length;

  // Partial snapshots (context-specific data).
  for (uint32_t i = 0; i < num_contexts; i++) {
    SetHeaderValue(data, ContextSnapshotOffsetOffset(i), payload_offset);
    SnapshotData* context_snapshot = context_snapshots[i];
    payload_length = context_snapshot->RawData().length();
    CopyBytes(
        data + payload_offset,
        reinterpret_cast<const char*>(context_snapshot->RawData().start()),
        payload_length);
    if (FLAG_profile_deserialization) {
      PrintF("%10d bytes in %d chunks for context #%d\n", payload_length,
             static_cast<uint32_t>(context_snapshot->Reservations().size()), i);
    }
    payload_offset += payload_length;
  }

  DCHECK_EQ(total_length, payload_offset);
  v8::StartupData result = {data, static_cast<int>(total_length)};

  Checksum checksum(ChecksummedContent(&result));
  SetHeaderValue(data, kChecksumPartAOffset, checksum.a());
  SetHeaderValue(data, kChecksumPartBOffset, checksum.b());

  return result;
}

uint32_t Snapshot::ExtractNumContexts(const v8::StartupData* data) {
  CHECK_LT(kNumberOfContextsOffset, data->raw_size);
  uint32_t num_contexts = GetHeaderValue(data, kNumberOfContextsOffset);
  return num_contexts;
}

bool Snapshot::VerifyChecksum(const v8::StartupData* data) {
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();
  uint32_t expected_a = GetHeaderValue(data, kChecksumPartAOffset);
  uint32_t expected_b = GetHeaderValue(data, kChecksumPartBOffset);
  Checksum checksum(ChecksummedContent(data));
  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    PrintF("[Verifying snapshot checksum took %0.3f ms]\n", ms);
  }
  return checksum.Check(expected_a, expected_b);
}

uint32_t Snapshot::ExtractContextOffset(const v8::StartupData* data,
                                        uint32_t index) {
  // Extract the offset of the context at a given index from the StartupData,
  // and check that it is within bounds.
  uint32_t context_offset =
      GetHeaderValue(data, ContextSnapshotOffsetOffset(index));
  CHECK_LT(context_offset, static_cast<uint32_t>(data->raw_size));
  return context_offset;
}

bool Snapshot::ExtractRehashability(const v8::StartupData* data) {
  CHECK_LT(kRehashabilityOffset, static_cast<uint32_t>(data->raw_size));
  return GetHeaderValue(data, kRehashabilityOffset) != 0;
}

namespace {
Vector<const byte> ExtractData(const v8::StartupData* snapshot,
                               uint32_t start_offset, uint32_t end_offset) {
  CHECK_LT(start_offset, end_offset);
  CHECK_LT(end_offset, snapshot->raw_size);
  uint32_t length = end_offset - start_offset;
  const byte* data =
      reinterpret_cast<const byte*>(snapshot->data + start_offset);
  return Vector<const byte>(data, length);
}
}  // namespace

Vector<const byte> Snapshot::ExtractStartupData(const v8::StartupData* data) {
  DCHECK(SnapshotIsValid(data));

  uint32_t num_contexts = ExtractNumContexts(data);
  return ExtractData(data, StartupSnapshotOffset(num_contexts),
                     GetHeaderValue(data, kReadOnlyOffsetOffset));
}

Vector<const byte> Snapshot::ExtractReadOnlyData(const v8::StartupData* data) {
  DCHECK(SnapshotIsValid(data));

  return ExtractData(data, GetHeaderValue(data, kReadOnlyOffsetOffset),
                     GetHeaderValue(data, ContextSnapshotOffsetOffset(0)));
}

Vector<const byte> Snapshot::ExtractContextData(const v8::StartupData* data,
                                                uint32_t index) {
  uint32_t num_contexts = ExtractNumContexts(data);
  CHECK_LT(index, num_contexts);

  uint32_t context_offset = ExtractContextOffset(data, index);
  uint32_t next_context_offset;
  if (index == num_contexts - 1) {
    next_context_offset = data->raw_size;
  } else {
    next_context_offset = ExtractContextOffset(data, index + 1);
    CHECK_LT(next_context_offset, data->raw_size);
  }

  const byte* context_data =
      reinterpret_cast<const byte*>(data->data + context_offset);
  uint32_t context_length = next_context_offset - context_offset;
  return Vector<const byte>(context_data, context_length);
}

void Snapshot::CheckVersion(const v8::StartupData* data) {
  char version[kVersionStringLength];
  memset(version, 0, kVersionStringLength);
  CHECK_LT(kVersionStringOffset + kVersionStringLength,
           static_cast<uint32_t>(data->raw_size));
  Version::GetString(Vector<char>(version, kVersionStringLength));
  if (strncmp(version, data->data + kVersionStringOffset,
              kVersionStringLength) != 0) {
    FATAL(
        "Version mismatch between V8 binary and snapshot.\n"
        "#   V8 binary version: %.*s\n"
        "#    Snapshot version: %.*s\n"
        "# The snapshot consists of %d bytes and contains %d context(s).",
        kVersionStringLength, version, kVersionStringLength,
        data->data + kVersionStringOffset, data->raw_size,
        ExtractNumContexts(data));
  }
}

SnapshotData::SnapshotData(const Serializer* serializer) {
  DisallowHeapAllocation no_gc;
  std::vector<Reservation> reservations = serializer->EncodeReservations();
  const std::vector<byte>* payload = serializer->Payload();

  // Calculate sizes.
  uint32_t reservation_size =
      static_cast<uint32_t>(reservations.size()) * kUInt32Size;
  uint32_t payload_offset = kHeaderSize + reservation_size;
  uint32_t padded_payload_offset = POINTER_SIZE_ALIGN(payload_offset);
  uint32_t size =
      padded_payload_offset + static_cast<uint32_t>(payload->size());
  DCHECK(IsAligned(size, kPointerAlignment));

  // Allocate backing store and create result data.
  AllocateData(size);

  // Zero out pre-payload data. Part of that is only used for padding.
  memset(data_, 0, padded_payload_offset);

  // Set header values.
  SetMagicNumber();
  SetHeaderValue(kNumReservationsOffset, static_cast<int>(reservations.size()));
  SetHeaderValue(kPayloadLengthOffset, static_cast<int>(payload->size()));

  // Copy reservation chunk sizes.
  CopyBytes(data_ + kHeaderSize, reinterpret_cast<byte*>(reservations.data()),
            reservation_size);

  // Copy serialized data.
  CopyBytes(data_ + padded_payload_offset, payload->data(),
            static_cast<size_t>(payload->size()));
}

std::vector<SerializedData::Reservation> SnapshotData::Reservations() const {
  uint32_t size = GetHeaderValue(kNumReservationsOffset);
  std::vector<SerializedData::Reservation> reservations(size);
  memcpy(reservations.data(), data_ + kHeaderSize,
         size * sizeof(SerializedData::Reservation));
  return reservations;
}

Vector<const byte> SnapshotData::Payload() const {
  uint32_t reservations_size =
      GetHeaderValue(kNumReservationsOffset) * kUInt32Size;
  uint32_t padded_payload_offset =
      POINTER_SIZE_ALIGN(kHeaderSize + reservations_size);
  const byte* payload = data_ + padded_payload_offset;
  uint32_t length = GetHeaderValue(kPayloadLengthOffset);
  DCHECK_EQ(data_ + size_, payload + length);
  return Vector<const byte>(payload, length);
}

}  // namespace internal
}  // namespace v8
