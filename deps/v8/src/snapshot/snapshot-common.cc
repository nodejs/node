// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The common functionality when building with or without snapshots.

#include "src/snapshot/snapshot.h"

#include "src/api.h"
#include "src/base/platform/platform.h"
#include "src/callable.h"
#include "src/interface-descriptors.h"
#include "src/objects-inl.h"
#include "src/snapshot/builtin-deserializer.h"
#include "src/snapshot/builtin-serializer.h"
#include "src/snapshot/partial-deserializer.h"
#include "src/snapshot/snapshot-source-sink.h"
#include "src/snapshot/startup-deserializer.h"
#include "src/utils.h"
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
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  const v8::StartupData* blob = isolate->snapshot_blob();
  CheckVersion(blob);
  Vector<const byte> startup_data = ExtractStartupData(blob);
  SnapshotData startup_snapshot_data(startup_data);
  Vector<const byte> builtin_data = ExtractBuiltinData(blob);
  BuiltinSnapshotData builtin_snapshot_data(builtin_data);
  StartupDeserializer deserializer(&startup_snapshot_data,
                                   &builtin_snapshot_data);
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

// static
Code* Snapshot::DeserializeBuiltin(Isolate* isolate, int builtin_id) {
  if (FLAG_trace_lazy_deserialization) {
    PrintF("Lazy-deserializing builtin %s\n", Builtins::name(builtin_id));
  }

  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  const v8::StartupData* blob = isolate->snapshot_blob();
  Vector<const byte> builtin_data = Snapshot::ExtractBuiltinData(blob);
  BuiltinSnapshotData builtin_snapshot_data(builtin_data);

  CodeSpaceMemoryModificationScope code_allocation(isolate->heap());
  BuiltinDeserializer builtin_deserializer(isolate, &builtin_snapshot_data);
  Code* code = builtin_deserializer.DeserializeBuiltin(builtin_id);
  DCHECK_EQ(code, isolate->builtins()->builtin(builtin_id));

  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int bytes = code->Size();
    PrintF("[Deserializing builtin %s (%d bytes) took %0.3f ms]\n",
           Builtins::name(builtin_id), bytes, ms);
  }

  if (isolate->logger()->is_logging_code_events() || isolate->is_profiling()) {
    isolate->logger()->LogCodeObject(code);
  }

  return code;
}

// static
void Snapshot::EnsureAllBuiltinsAreDeserialized(Isolate* isolate) {
  if (!FLAG_lazy_deserialization) return;

  if (FLAG_trace_lazy_deserialization) {
    PrintF("Forcing eager builtin deserialization\n");
  }

  Builtins* builtins = isolate->builtins();
  for (int i = 0; i < Builtins::builtin_count; i++) {
    if (!Builtins::IsLazy(i)) continue;

    DCHECK_NE(Builtins::kDeserializeLazy, i);
    Code* code = builtins->builtin(i);
    if (code->builtin_index() == Builtins::kDeserializeLazy) {
      code = Snapshot::DeserializeBuiltin(isolate, i);
    }

    DCHECK_EQ(i, code->builtin_index());
    DCHECK_EQ(code, builtins->builtin(i));
  }
}

// static
Code* Snapshot::EnsureBuiltinIsDeserialized(Isolate* isolate,
                                            Handle<SharedFunctionInfo> shared) {
  DCHECK(FLAG_lazy_deserialization);

  int builtin_id = shared->builtin_id();

  // We should never lazily deserialize DeserializeLazy.
  DCHECK_NE(Builtins::kDeserializeLazy, builtin_id);

  // Look up code from builtins list.
  Code* code = isolate->builtins()->builtin(builtin_id);

  // Deserialize if builtin is not on the list.
  if (code->builtin_index() != builtin_id) {
    DCHECK_EQ(code->builtin_index(), Builtins::kDeserializeLazy);
    code = Snapshot::DeserializeBuiltin(isolate, builtin_id);
    DCHECK_EQ(builtin_id, code->builtin_index());
    DCHECK_EQ(code, isolate->builtins()->builtin(builtin_id));
  }
  return code;
}

// static
Code* Snapshot::DeserializeHandler(Isolate* isolate,
                                   interpreter::Bytecode bytecode,
                                   interpreter::OperandScale operand_scale) {
  if (FLAG_trace_lazy_deserialization) {
    PrintF("Lazy-deserializing handler %s\n",
           interpreter::Bytecodes::ToString(bytecode, operand_scale).c_str());
  }

  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  const v8::StartupData* blob = isolate->snapshot_blob();
  Vector<const byte> builtin_data = Snapshot::ExtractBuiltinData(blob);
  BuiltinSnapshotData builtin_snapshot_data(builtin_data);

  CodeSpaceMemoryModificationScope code_allocation(isolate->heap());
  BuiltinDeserializer builtin_deserializer(isolate, &builtin_snapshot_data);
  Code* code = builtin_deserializer.DeserializeHandler(bytecode, operand_scale);

  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int bytes = code->Size();
    PrintF("[Deserializing handler %s (%d bytes) took %0.3f ms]\n",
           interpreter::Bytecodes::ToString(bytecode, operand_scale).c_str(),
           bytes, ms);
  }

  if (isolate->logger()->is_logging_code_events() || isolate->is_profiling()) {
    isolate->logger()->LogBytecodeHandler(bytecode, operand_scale, code);
  }

  return code;
}

void ProfileDeserialization(
    const SnapshotData* startup_snapshot, const SnapshotData* builtin_snapshot,
    const std::vector<SnapshotData*>& context_snapshots) {
  if (FLAG_profile_deserialization) {
    int startup_total = 0;
    PrintF("Deserialization will reserve:\n");
    for (const auto& reservation : startup_snapshot->Reservations()) {
      startup_total += reservation.chunk_size();
    }
    for (const auto& reservation : builtin_snapshot->Reservations()) {
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
    const BuiltinSnapshotData* builtin_snapshot,
    const std::vector<SnapshotData*>& context_snapshots, bool can_be_rehashed) {
  uint32_t num_contexts = static_cast<uint32_t>(context_snapshots.size());
  uint32_t startup_snapshot_offset = StartupSnapshotOffset(num_contexts);
  uint32_t total_length = startup_snapshot_offset;
  total_length += static_cast<uint32_t>(startup_snapshot->RawData().length());
  total_length += static_cast<uint32_t>(builtin_snapshot->RawData().length());
  for (const auto context_snapshot : context_snapshots) {
    total_length += static_cast<uint32_t>(context_snapshot->RawData().length());
  }

  ProfileDeserialization(startup_snapshot, builtin_snapshot, context_snapshots);

  char* data = new char[total_length];
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
    PrintF("Snapshot blob consists of:\n%10d bytes for startup\n",
           payload_length);
  }
  payload_offset += payload_length;

  // Builtins.
  SetHeaderValue(data, kBuiltinOffsetOffset, payload_offset);
  payload_length = builtin_snapshot->RawData().length();
  CopyBytes(data + payload_offset,
            reinterpret_cast<const char*>(builtin_snapshot->RawData().start()),
            payload_length);
  if (FLAG_profile_deserialization) {
    PrintF("%10d bytes for builtins\n", payload_length);
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
      PrintF("%10d bytes for context #%d\n", payload_length, i);
    }
    payload_offset += payload_length;
  }

  v8::StartupData result = {data, static_cast<int>(total_length)};
  DCHECK_EQ(total_length, payload_offset);
  return result;
}

#ifdef V8_EMBEDDED_BUILTINS
namespace {
bool BuiltinAliasesOffHeapTrampolineRegister(Isolate* isolate, Code* code) {
  DCHECK(Builtins::IsIsolateIndependent(code->builtin_index()));
  switch (Builtins::KindOf(code->builtin_index())) {
    case Builtins::CPP:
    case Builtins::TFC:
    case Builtins::TFH:
    case Builtins::TFJ:
    case Builtins::TFS:
      break;
    case Builtins::API:
    case Builtins::ASM:
      // TODO(jgruber): Extend checks to remaining kinds.
      return false;
  }

  Callable callable = Builtins::CallableFor(
      isolate, static_cast<Builtins::Name>(code->builtin_index()));
  CallInterfaceDescriptor descriptor = callable.descriptor();

  if (descriptor.ContextRegister() == kOffHeapTrampolineRegister) {
    return true;
  }

  for (int i = 0; i < descriptor.GetRegisterParameterCount(); i++) {
    Register reg = descriptor.GetRegisterParameter(i);
    if (reg == kOffHeapTrampolineRegister) return true;
  }

  return false;
}
}  // namespace

// static
EmbeddedData EmbeddedData::FromIsolate(Isolate* isolate) {
  Builtins* builtins = isolate->builtins();

  // Store instruction stream lengths and offsets.
  std::vector<uint32_t> lengths(kTableSize);
  std::vector<uint32_t> offsets(kTableSize);

  bool saw_unsafe_builtin = false;
  uint32_t raw_data_size = 0;
  for (int i = 0; i < Builtins::builtin_count; i++) {
    Code* code = builtins->builtin(i);

    if (Builtins::IsIsolateIndependent(i)) {
      DCHECK(!Builtins::IsLazy(i));

      // Sanity-check that the given builtin is process-independent and does not
      // use the trampoline register in its calling convention.
      if (!code->IsProcessIndependent()) {
        saw_unsafe_builtin = true;
        fprintf(stderr, "%s is not process-independent.\n", Builtins::name(i));
      }
      if (BuiltinAliasesOffHeapTrampolineRegister(isolate, code)) {
        saw_unsafe_builtin = true;
        fprintf(stderr, "%s aliases the off-heap trampoline register.\n",
                Builtins::name(i));
      }

      uint32_t length = static_cast<uint32_t>(code->raw_instruction_size());

      DCHECK_EQ(0, raw_data_size % kCodeAlignment);
      offsets[i] = raw_data_size;
      lengths[i] = length;

      // Align the start of each instruction stream.
      raw_data_size += RoundUp<kCodeAlignment>(length);
    } else {
      offsets[i] = raw_data_size;
      lengths[i] = 0;
    }
  }
  CHECK(!saw_unsafe_builtin);

  const uint32_t blob_size = RawDataOffset() + raw_data_size;
  uint8_t* blob = new uint8_t[blob_size];
  std::memset(blob, 0, blob_size);

  // Write the offsets and length tables.
  DCHECK_EQ(OffsetsSize(), sizeof(offsets[0]) * offsets.size());
  std::memcpy(blob + OffsetsOffset(), offsets.data(), OffsetsSize());

  DCHECK_EQ(LengthsSize(), sizeof(lengths[0]) * lengths.size());
  std::memcpy(blob + LengthsOffset(), lengths.data(), LengthsSize());

  // Write the raw data section.
  for (int i = 0; i < Builtins::builtin_count; i++) {
    if (!Builtins::IsIsolateIndependent(i)) continue;
    Code* code = builtins->builtin(i);
    uint32_t offset = offsets[i];
    uint8_t* dst = blob + RawDataOffset() + offset;
    DCHECK_LE(RawDataOffset() + offset + code->raw_instruction_size(),
              blob_size);
    std::memcpy(dst, code->raw_instruction_start(),
                code->raw_instruction_size());
  }

  return {blob, blob_size};
}

EmbeddedData EmbeddedData::FromBlob() {
  const uint8_t* data = Isolate::CurrentEmbeddedBlob();
  uint32_t size = Isolate::CurrentEmbeddedBlobSize();
  DCHECK_NOT_NULL(data);
  DCHECK_LT(0, size);
  return {data, size};
}

const uint8_t* EmbeddedData::InstructionStartOfBuiltin(int i) const {
  DCHECK(Builtins::IsBuiltinId(i));

  const uint32_t* offsets = Offsets();
  const uint8_t* result = RawData() + offsets[i];
  DCHECK_LT(result, data_ + size_);
  return result;
}

uint32_t EmbeddedData::InstructionSizeOfBuiltin(int i) const {
  DCHECK(Builtins::IsBuiltinId(i));
  const uint32_t* lengths = Lengths();
  return lengths[i];
}
#endif

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
  uint32_t builtin_offset = GetHeaderValue(data, kBuiltinOffsetOffset);
  CHECK_LT(builtin_offset, data->raw_size);
  CHECK_GT(builtin_offset, startup_offset);
  uint32_t startup_length = builtin_offset - startup_offset;
  const byte* startup_data =
      reinterpret_cast<const byte*>(data->data + startup_offset);
  return Vector<const byte>(startup_data, startup_length);
}

Vector<const byte> Snapshot::ExtractBuiltinData(const v8::StartupData* data) {
  DCHECK(SnapshotIsValid(data));

  uint32_t from_offset = GetHeaderValue(data, kBuiltinOffsetOffset);
  CHECK_LT(from_offset, data->raw_size);

  uint32_t to_offset = GetHeaderValue(data, ContextSnapshotOffsetOffset(0));
  CHECK_LT(to_offset, data->raw_size);

  CHECK_GT(to_offset, from_offset);
  uint32_t length = to_offset - from_offset;
  const byte* builtin_data =
      reinterpret_cast<const byte*>(data->data + from_offset);
  return Vector<const byte>(builtin_data, length);
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

template <class AllocatorT>
SnapshotData::SnapshotData(const Serializer<AllocatorT>* serializer) {
  DisallowHeapAllocation no_gc;
  std::vector<Reservation> reservations = serializer->EncodeReservations();
  const std::vector<byte>* payload = serializer->Payload();

  // Calculate sizes.
  uint32_t reservation_size =
      static_cast<uint32_t>(reservations.size()) * kUInt32Size;
  uint32_t size =
      kHeaderSize + reservation_size + static_cast<uint32_t>(payload->size());

  // Allocate backing store and create result data.
  AllocateData(size);

  // Set header values.
  SetMagicNumber(serializer->isolate());
  SetHeaderValue(kNumReservationsOffset, static_cast<int>(reservations.size()));
  SetHeaderValue(kPayloadLengthOffset, static_cast<int>(payload->size()));

  // Copy reservation chunk sizes.
  CopyBytes(data_ + kHeaderSize, reinterpret_cast<byte*>(reservations.data()),
            reservation_size);

  // Copy serialized data.
  CopyBytes(data_ + kHeaderSize + reservation_size, payload->data(),
            static_cast<size_t>(payload->size()));
}

// Explicit instantiation.
template SnapshotData::SnapshotData(
    const Serializer<DefaultSerializerAllocator>* serializer);

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
  const byte* payload = data_ + kHeaderSize + reservations_size;
  uint32_t length = GetHeaderValue(kPayloadLengthOffset);
  DCHECK_EQ(data_ + size_, payload + length);
  return Vector<const byte>(payload, length);
}

BuiltinSnapshotData::BuiltinSnapshotData(const BuiltinSerializer* serializer)
    : SnapshotData(serializer) {}

Vector<const byte> BuiltinSnapshotData::Payload() const {
  uint32_t reservations_size =
      GetHeaderValue(kNumReservationsOffset) * kUInt32Size;
  const byte* payload = data_ + kHeaderSize + reservations_size;
  const int builtin_offsets_size =
      BuiltinSnapshotUtils::kNumberOfCodeObjects * kUInt32Size;
  uint32_t payload_length = GetHeaderValue(kPayloadLengthOffset);
  DCHECK_EQ(data_ + size_, payload + payload_length);
  DCHECK_GT(payload_length, builtin_offsets_size);
  return Vector<const byte>(payload, payload_length - builtin_offsets_size);
}

Vector<const uint32_t> BuiltinSnapshotData::BuiltinOffsets() const {
  uint32_t reservations_size =
      GetHeaderValue(kNumReservationsOffset) * kUInt32Size;
  const byte* payload = data_ + kHeaderSize + reservations_size;
  const int builtin_offsets_size =
      BuiltinSnapshotUtils::kNumberOfCodeObjects * kUInt32Size;
  uint32_t payload_length = GetHeaderValue(kPayloadLengthOffset);
  DCHECK_EQ(data_ + size_, payload + payload_length);
  DCHECK_GT(payload_length, builtin_offsets_size);
  const uint32_t* data = reinterpret_cast<const uint32_t*>(
      payload + payload_length - builtin_offsets_size);
  return Vector<const uint32_t>(data,
                                BuiltinSnapshotUtils::kNumberOfCodeObjects);
}

}  // namespace internal
}  // namespace v8
