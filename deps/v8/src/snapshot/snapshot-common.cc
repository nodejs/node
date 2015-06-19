// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The common functionality when building with or without snapshots.

#include "src/v8.h"

#include "src/api.h"
#include "src/base/platform/platform.h"
#include "src/full-codegen.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

#ifdef DEBUG
bool Snapshot::SnapshotIsValid(v8::StartupData* snapshot_blob) {
  return !Snapshot::ExtractStartupData(snapshot_blob).is_empty() &&
         !Snapshot::ExtractContextData(snapshot_blob).is_empty();
}
#endif  // DEBUG


bool Snapshot::EmbedsScript(Isolate* isolate) {
  if (!isolate->snapshot_available()) return false;
  return ExtractMetadata(isolate->snapshot_blob()).embeds_script();
}


uint32_t Snapshot::SizeOfFirstPage(Isolate* isolate, AllocationSpace space) {
  DCHECK(space >= FIRST_PAGED_SPACE && space <= LAST_PAGED_SPACE);
  if (!isolate->snapshot_available()) {
    return static_cast<uint32_t>(MemoryAllocator::PageAreaSize(space));
  }
  uint32_t size;
  int offset = kFirstPageSizesOffset + (space - FIRST_PAGED_SPACE) * kInt32Size;
  memcpy(&size, isolate->snapshot_blob()->data + offset, kInt32Size);
  return size;
}


bool Snapshot::Initialize(Isolate* isolate) {
  if (!isolate->snapshot_available()) return false;
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  const v8::StartupData* blob = isolate->snapshot_blob();
  Vector<const byte> startup_data = ExtractStartupData(blob);
  SnapshotData snapshot_data(startup_data);
  Deserializer deserializer(&snapshot_data);
  bool success = isolate->Init(&deserializer);
  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int bytes = startup_data.length();
    PrintF("[Deserializing isolate (%d bytes) took %0.3f ms]\n", bytes, ms);
  }
  return success;
}


MaybeHandle<Context> Snapshot::NewContextFromSnapshot(
    Isolate* isolate, Handle<JSGlobalProxy> global_proxy,
    Handle<FixedArray>* outdated_contexts_out) {
  if (!isolate->snapshot_available()) return Handle<Context>();
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  const v8::StartupData* blob = isolate->snapshot_blob();
  Vector<const byte> context_data = ExtractContextData(blob);
  SnapshotData snapshot_data(context_data);
  Deserializer deserializer(&snapshot_data);

  MaybeHandle<Object> maybe_context = deserializer.DeserializePartial(
      isolate, global_proxy, outdated_contexts_out);
  Handle<Object> result;
  if (!maybe_context.ToHandle(&result)) return MaybeHandle<Context>();
  CHECK(result->IsContext());
  // If the snapshot does not contain a custom script, we need to update
  // the global object for exactly one context.
  CHECK(EmbedsScript(isolate) || (*outdated_contexts_out)->length() == 1);
  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int bytes = context_data.length();
    PrintF("[Deserializing context (%d bytes) took %0.3f ms]\n", bytes, ms);
  }
  return Handle<Context>::cast(result);
}


void CalculateFirstPageSizes(bool is_default_snapshot,
                             const SnapshotData& startup_snapshot,
                             const SnapshotData& context_snapshot,
                             uint32_t* sizes_out) {
  Vector<const SerializedData::Reservation> startup_reservations =
      startup_snapshot.Reservations();
  Vector<const SerializedData::Reservation> context_reservations =
      context_snapshot.Reservations();
  int startup_index = 0;
  int context_index = 0;

  if (FLAG_profile_deserialization) {
    int startup_total = 0;
    int context_total = 0;
    for (auto& reservation : startup_reservations) {
      startup_total += reservation.chunk_size();
    }
    for (auto& reservation : context_reservations) {
      context_total += reservation.chunk_size();
    }
    PrintF(
        "Deserialization will reserve:\n"
        "%10d bytes per isolate\n"
        "%10d bytes per context\n",
        startup_total, context_total);
  }

  for (int space = 0; space < i::Serializer::kNumberOfSpaces; space++) {
    bool single_chunk = true;
    while (!startup_reservations[startup_index].is_last()) {
      single_chunk = false;
      startup_index++;
    }
    while (!context_reservations[context_index].is_last()) {
      single_chunk = false;
      context_index++;
    }

    uint32_t required = kMaxUInt32;
    if (single_chunk) {
      // If both the startup snapshot data and the context snapshot data on
      // this space fit in a single page, then we consider limiting the size
      // of the first page. For this, we add the chunk sizes and some extra
      // allowance. This way we achieve a smaller startup memory footprint.
      required = (startup_reservations[startup_index].chunk_size() +
                  2 * context_reservations[context_index].chunk_size()) +
                 Page::kObjectStartOffset;
      // Add a small allowance to the code space for small scripts.
      if (space == CODE_SPACE) required += 32 * KB;
    } else {
      // We expect the vanilla snapshot to only require on page per space.
      DCHECK(!is_default_snapshot);
    }

    if (space >= FIRST_PAGED_SPACE && space <= LAST_PAGED_SPACE) {
      uint32_t max_size =
          MemoryAllocator::PageAreaSize(static_cast<AllocationSpace>(space));
      sizes_out[space - FIRST_PAGED_SPACE] = Min(required, max_size);
    } else {
      DCHECK(single_chunk);
    }
    startup_index++;
    context_index++;
  }

  DCHECK_EQ(startup_reservations.length(), startup_index);
  DCHECK_EQ(context_reservations.length(), context_index);
}


v8::StartupData Snapshot::CreateSnapshotBlob(
    const i::StartupSerializer& startup_ser,
    const i::PartialSerializer& context_ser, Snapshot::Metadata metadata) {
  SnapshotData startup_snapshot(startup_ser);
  SnapshotData context_snapshot(context_ser);
  Vector<const byte> startup_data = startup_snapshot.RawData();
  Vector<const byte> context_data = context_snapshot.RawData();

  uint32_t first_page_sizes[kNumPagedSpaces];

  CalculateFirstPageSizes(!metadata.embeds_script(), startup_snapshot,
                          context_snapshot, first_page_sizes);

  int startup_length = startup_data.length();
  int context_length = context_data.length();
  int context_offset = ContextOffset(startup_length);

  int length = context_offset + context_length;
  char* data = new char[length];

  memcpy(data + kMetadataOffset, &metadata.RawValue(), kInt32Size);
  memcpy(data + kFirstPageSizesOffset, first_page_sizes,
         kNumPagedSpaces * kInt32Size);
  memcpy(data + kStartupLengthOffset, &startup_length, kInt32Size);
  memcpy(data + kStartupDataOffset, startup_data.begin(), startup_length);
  memcpy(data + context_offset, context_data.begin(), context_length);
  v8::StartupData result = {data, length};

  if (FLAG_profile_deserialization) {
    PrintF(
        "Snapshot blob consists of:\n"
        "%10d bytes for startup\n"
        "%10d bytes for context\n",
        startup_length, context_length);
  }
  return result;
}


Snapshot::Metadata Snapshot::ExtractMetadata(const v8::StartupData* data) {
  uint32_t raw;
  memcpy(&raw, data->data + kMetadataOffset, kInt32Size);
  return Metadata(raw);
}


Vector<const byte> Snapshot::ExtractStartupData(const v8::StartupData* data) {
  DCHECK_LT(kIntSize, data->raw_size);
  int startup_length;
  memcpy(&startup_length, data->data + kStartupLengthOffset, kInt32Size);
  DCHECK_LT(startup_length, data->raw_size);
  const byte* startup_data =
      reinterpret_cast<const byte*>(data->data + kStartupDataOffset);
  return Vector<const byte>(startup_data, startup_length);
}


Vector<const byte> Snapshot::ExtractContextData(const v8::StartupData* data) {
  DCHECK_LT(kIntSize, data->raw_size);
  int startup_length;
  memcpy(&startup_length, data->data + kStartupLengthOffset, kIntSize);
  int context_offset = ContextOffset(startup_length);
  const byte* context_data =
      reinterpret_cast<const byte*>(data->data + context_offset);
  DCHECK_LT(context_offset, data->raw_size);
  int context_length = data->raw_size - context_offset;
  return Vector<const byte>(context_data, context_length);
}
} }  // namespace v8::internal
