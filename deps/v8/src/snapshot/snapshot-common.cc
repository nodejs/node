// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The common functionality when building with or without snapshots.

#include "src/snapshot/snapshot.h"

#include "src/api.h"
#include "src/base/platform/platform.h"
#include "src/objects-inl.h"
#include "src/snapshot/partial-deserializer.h"
#include "src/snapshot/snapshot-source-sink.h"
#include "src/snapshot/startup-deserializer.h"
#include "src/utils.h"
#include "src/version.h"

namespace v8 {
namespace internal {

#ifdef DEBUG
bool Snapshot::SnapshotIsValid(v8::StartupData* snapshot_blob) {
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
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  const v8::StartupData* blob = isolate->snapshot_blob();
  Vector<const byte> startup_data = ExtractStartupData(blob);
  SnapshotData snapshot_data(startup_data);
  StartupDeserializer deserializer(&snapshot_data);
  deserializer.SetRehashability(ExtractRehashability(blob));
  bool success = isolate->Init(&deserializer);
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
    const SnapshotData* startup_snapshot,
    const std::vector<SnapshotData*>& context_snapshots) {
  if (FLAG_profile_deserialization) {
    int startup_total = 0;
    PrintF("Deserialization will reserve:\n");
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
    const std::vector<SnapshotData*>& context_snapshots, bool can_be_rehashed) {
  uint32_t num_contexts = static_cast<uint32_t>(context_snapshots.size());
  uint32_t startup_snapshot_offset = StartupSnapshotOffset(num_contexts);
  uint32_t total_length = startup_snapshot_offset;
  total_length += static_cast<uint32_t>(startup_snapshot->RawData().length());
  for (const auto context_snapshot : context_snapshots) {
    total_length += static_cast<uint32_t>(context_snapshot->RawData().length());
  }

  ProfileDeserialization(startup_snapshot, context_snapshots);

  char* data = new char[total_length];
  SetHeaderValue(data, kNumberOfContextsOffset, num_contexts);
  SetHeaderValue(data, kRehashabilityOffset, can_be_rehashed ? 1 : 0);
  uint32_t payload_offset = StartupSnapshotOffset(num_contexts);
  uint32_t payload_length =
      static_cast<uint32_t>(startup_snapshot->RawData().length());
  CopyBytes(data + payload_offset,
            reinterpret_cast<const char*>(startup_snapshot->RawData().start()),
            payload_length);
  if (FLAG_profile_deserialization) {
    PrintF("Snapshot blob consists of:\n%10d bytes for startup\n",
           payload_length);
  }
  payload_offset += payload_length;
  for (uint32_t i = 0; i < num_contexts; i++) {
    SetHeaderValue(data, ContextSnapshotOffsetOffset(i), payload_offset);
    SnapshotData* context_snapshot = context_snapshots[i];
    payload_length = context_snapshot->RawData().length();
    CopyBytes(
        data + payload_offset,
        reinterpret_cast<const char*>(context_snapshot->RawData().start()),
        payload_length);
    if (FLAG_profile_deserialization) {
      PrintF("%10d bytes for context #%d\n", payload_length, i);
    }
    payload_offset += payload_length;
  }

  v8::StartupData result = {data, static_cast<int>(total_length)};
  return result;
}

uint32_t Snapshot::ExtractNumContexts(const v8::StartupData* data) {
  CHECK_LT(kNumberOfContextsOffset, data->raw_size);
  uint32_t num_contexts = GetHeaderValue(data, kNumberOfContextsOffset);
  return num_contexts;
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

Vector<const byte> Snapshot::ExtractStartupData(const v8::StartupData* data) {
  uint32_t num_contexts = ExtractNumContexts(data);
  uint32_t startup_offset = StartupSnapshotOffset(num_contexts);
  CHECK_LT(startup_offset, data->raw_size);
  uint32_t first_context_offset = ExtractContextOffset(data, 0);
  CHECK_LT(first_context_offset, data->raw_size);
  uint32_t startup_length = first_context_offset - startup_offset;
  const byte* startup_data =
      reinterpret_cast<const byte*>(data->data + startup_offset);
  return Vector<const byte>(startup_data, startup_length);
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

SnapshotData::SnapshotData(const Serializer* serializer) {
  DisallowHeapAllocation no_gc;
  std::vector<Reservation> reservations;
  serializer->EncodeReservations(&reservations);
  const std::vector<byte>* payload = serializer->sink()->data();

  // Calculate sizes.
  uint32_t reservation_size =
      static_cast<uint32_t>(reservations.size()) * kUInt32Size;
  uint32_t size =
      kHeaderSize + reservation_size + static_cast<uint32_t>(payload->size());

  // Allocate backing store and create result data.
  AllocateData(size);

  // Set header values.
  SetMagicNumber(serializer->isolate());
  SetHeaderValue(kVersionHashOffset, Version::Hash());
  SetHeaderValue(kNumReservationsOffset, static_cast<int>(reservations.size()));
  SetHeaderValue(kPayloadLengthOffset, static_cast<int>(payload->size()));

  // Copy reservation chunk sizes.
  CopyBytes(data_ + kHeaderSize, reinterpret_cast<byte*>(reservations.data()),
            reservation_size);

  // Copy serialized data.
  CopyBytes(data_ + kHeaderSize + reservation_size, payload->data(),
            static_cast<size_t>(payload->size()));
}

bool SnapshotData::IsSane() {
  return GetHeaderValue(kVersionHashOffset) == Version::Hash();
}

Vector<const SerializedData::Reservation> SnapshotData::Reservations() const {
  return Vector<const Reservation>(
      reinterpret_cast<const Reservation*>(data_ + kHeaderSize),
      GetHeaderValue(kNumReservationsOffset));
}

Vector<const byte> SnapshotData::Payload() const {
  uint32_t reservations_size =
      GetHeaderValue(kNumReservationsOffset) * kUInt32Size;
  const byte* payload = data_ + kHeaderSize + reservations_size;
  uint32_t length = GetHeaderValue(kPayloadLengthOffset);
  DCHECK_EQ(data_ + size_, payload + length);
  return Vector<const byte>(payload, length);
}

}  // namespace internal
}  // namespace v8
