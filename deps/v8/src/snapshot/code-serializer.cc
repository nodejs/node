// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/code-serializer.h"

#include "src/counters.h"
#include "src/debug/debug.h"
#include "src/log.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/objects/slots.h"
#include "src/snapshot/object-deserializer.h"
#include "src/snapshot/snapshot.h"
#include "src/version.h"
#include "src/visitors.h"

namespace v8 {
namespace internal {

ScriptData::ScriptData(const byte* data, int length)
    : owns_data_(false), rejected_(false), data_(data), length_(length) {
  if (!IsAligned(reinterpret_cast<intptr_t>(data), kPointerAlignment)) {
    byte* copy = NewArray<byte>(length);
    DCHECK(IsAligned(reinterpret_cast<intptr_t>(copy), kPointerAlignment));
    CopyBytes(copy, data, length);
    data_ = copy;
    AcquireDataOwnership();
  }
}

CodeSerializer::CodeSerializer(Isolate* isolate, uint32_t source_hash)
    : Serializer(isolate), source_hash_(source_hash) {
  allocator()->UseCustomChunkSize(FLAG_serialization_chunk_size);
}

// static
ScriptCompiler::CachedData* CodeSerializer::Serialize(
    Handle<SharedFunctionInfo> info) {
  Isolate* isolate = info->GetIsolate();
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.Execute");
  HistogramTimerScope histogram_timer(isolate->counters()->compile_serialize());
  RuntimeCallTimerScope runtimeTimer(isolate,
                                     RuntimeCallCounterId::kCompileSerialize);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileSerialize");

  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();
  Handle<Script> script(Script::cast(info->script()), isolate);
  if (FLAG_trace_serializer) {
    PrintF("[Serializing from");
    script->name()->ShortPrint();
    PrintF("]\n");
  }
  // TODO(7110): Enable serialization of Asm modules once the AsmWasmData is
  // context independent.
  if (script->ContainsAsmModule()) return nullptr;

  isolate->heap()->read_only_space()->ClearStringPaddingIfNeeded();

  // Serialize code object.
  Handle<String> source(String::cast(script->source()), isolate);
  CodeSerializer cs(isolate, SerializedCodeData::SourceHash(
                                 source, script->origin_options()));
  DisallowHeapAllocation no_gc;
  cs.reference_map()->AddAttachedReference(
      reinterpret_cast<void*>(source->ptr()));
  ScriptData* script_data = cs.SerializeSharedFunctionInfo(info);

  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int length = script_data->length();
    PrintF("[Serializing to %d bytes took %0.3f ms]\n", length, ms);
  }

  ScriptCompiler::CachedData* result =
      new ScriptCompiler::CachedData(script_data->data(), script_data->length(),
                                     ScriptCompiler::CachedData::BufferOwned);
  script_data->ReleaseDataOwnership();
  delete script_data;

  return result;
}

ScriptData* CodeSerializer::SerializeSharedFunctionInfo(
    Handle<SharedFunctionInfo> info) {
  DisallowHeapAllocation no_gc;

  VisitRootPointer(Root::kHandleScope, nullptr,
                   FullObjectSlot(info.location()));
  SerializeDeferredObjects();
  Pad();

  SerializedCodeData data(sink_.data(), this);

  return data.GetScriptData();
}

bool CodeSerializer::SerializeReadOnlyObject(HeapObject obj,
                                             HowToCode how_to_code,
                                             WhereToPoint where_to_point,
                                             int skip) {
  PagedSpace* read_only_space = isolate()->heap()->read_only_space();
  if (!read_only_space->Contains(obj)) return false;

  // For objects in RO_SPACE, never serialize the object, but instead create a
  // back reference that encodes the page number as the chunk_index and the
  // offset within the page as the chunk_offset.
  Address address = obj->address();
  Page* page = Page::FromAddress(address);
  uint32_t chunk_index = 0;
  for (Page* p : *read_only_space) {
    if (p == page) break;
    ++chunk_index;
  }
  uint32_t chunk_offset = static_cast<uint32_t>(page->Offset(address));
  SerializerReference back_reference =
      SerializerReference::BackReference(RO_SPACE, chunk_index, chunk_offset);
  reference_map()->Add(reinterpret_cast<void*>(obj->ptr()), back_reference);
  CHECK(SerializeBackReference(obj, how_to_code, where_to_point, skip));
  return true;
}

void CodeSerializer::SerializeObject(HeapObject obj, HowToCode how_to_code,
                                     WhereToPoint where_to_point, int skip) {
  if (SerializeHotObject(obj, how_to_code, where_to_point, skip)) return;

  if (SerializeRoot(obj, how_to_code, where_to_point, skip)) return;

  if (SerializeBackReference(obj, how_to_code, where_to_point, skip)) return;

  if (SerializeReadOnlyObject(obj, how_to_code, where_to_point, skip)) return;

  FlushSkip(skip);

  if (obj->IsCode()) {
    Code code_object = Code::cast(obj);
    switch (code_object->kind()) {
      case Code::OPTIMIZED_FUNCTION:  // No optimized code compiled yet.
      case Code::REGEXP:              // No regexp literals initialized yet.
      case Code::NUMBER_OF_KINDS:     // Pseudo enum value.
      case Code::BYTECODE_HANDLER:    // No direct references to handlers.
        break;                        // hit UNREACHABLE below.
      case Code::STUB:
      case Code::BUILTIN:
      default:
        return SerializeCodeObject(code_object, how_to_code, where_to_point);
    }
    UNREACHABLE();
  }

  ReadOnlyRoots roots(isolate());
  if (ElideObject(obj)) {
    return SerializeObject(roots.undefined_value(), how_to_code, where_to_point,
                           skip);
  }

  if (obj->IsScript()) {
    Script script_obj = Script::cast(obj);
    DCHECK_NE(script_obj->compilation_type(), Script::COMPILATION_TYPE_EVAL);
    // We want to differentiate between undefined and uninitialized_symbol for
    // context_data for now. It is hack to allow debugging for scripts that are
    // included as a part of custom snapshot. (see debug::Script::IsEmbedded())
    Object context_data = script_obj->context_data();
    if (context_data != roots.undefined_value() &&
        context_data != roots.uninitialized_symbol()) {
      script_obj->set_context_data(roots.undefined_value());
    }
    // We don't want to serialize host options to avoid serializing unnecessary
    // object graph.
    FixedArray host_options = script_obj->host_defined_options();
    script_obj->set_host_defined_options(roots.empty_fixed_array());
    SerializeGeneric(obj, how_to_code, where_to_point);
    script_obj->set_host_defined_options(host_options);
    script_obj->set_context_data(context_data);
    return;
  }

  if (obj->IsSharedFunctionInfo()) {
    SharedFunctionInfo sfi = SharedFunctionInfo::cast(obj);
    // TODO(7110): Enable serializing of Asm modules once the AsmWasmData
    // is context independent.
    DCHECK(!sfi->IsApiFunction() && !sfi->HasAsmWasmData());

    DebugInfo debug_info;
    BytecodeArray debug_bytecode_array;
    if (sfi->HasDebugInfo()) {
      // Clear debug info.
      debug_info = sfi->GetDebugInfo();
      if (debug_info->HasInstrumentedBytecodeArray()) {
        debug_bytecode_array = debug_info->DebugBytecodeArray();
        sfi->SetDebugBytecodeArray(debug_info->OriginalBytecodeArray());
      }
      sfi->set_script_or_debug_info(debug_info->script());
    }
    DCHECK(!sfi->HasDebugInfo());

    SerializeGeneric(obj, how_to_code, where_to_point);

    // Restore debug info
    if (!debug_info.is_null()) {
      sfi->set_script_or_debug_info(debug_info);
      if (!debug_bytecode_array.is_null()) {
        sfi->SetDebugBytecodeArray(debug_bytecode_array);
      }
    }
    return;
  }

  if (obj->IsBytecodeArray()) {
    // Clear the stack frame cache if present
    BytecodeArray::cast(obj)->ClearFrameCacheFromSourcePositionTable();
  }

  // Past this point we should not see any (context-specific) maps anymore.
  CHECK(!obj->IsMap());
  // There should be no references to the global object embedded.
  CHECK(!obj->IsJSGlobalProxy() && !obj->IsJSGlobalObject());
  // Embedded FixedArrays that need rehashing must support rehashing.
  CHECK_IMPLIES(obj->NeedsRehashing(), obj->CanBeRehashed());
  // We expect no instantiated function objects or contexts.
  CHECK(!obj->IsJSFunction() && !obj->IsContext());

  SerializeGeneric(obj, how_to_code, where_to_point);
}

void CodeSerializer::SerializeGeneric(HeapObject heap_object,
                                      HowToCode how_to_code,
                                      WhereToPoint where_to_point) {
  // Object has not yet been serialized.  Serialize it here.
  ObjectSerializer serializer(this, heap_object, &sink_, how_to_code,
                              where_to_point);
  serializer.Serialize();
}

MaybeHandle<SharedFunctionInfo> CodeSerializer::Deserialize(
    Isolate* isolate, ScriptData* cached_data, Handle<String> source,
    ScriptOriginOptions origin_options) {
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization || FLAG_log_function_events) timer.Start();

  HandleScope scope(isolate);

  SerializedCodeData::SanityCheckResult sanity_check_result =
      SerializedCodeData::CHECK_SUCCESS;
  const SerializedCodeData scd = SerializedCodeData::FromCachedData(
      isolate, cached_data,
      SerializedCodeData::SourceHash(source, origin_options),
      &sanity_check_result);
  if (sanity_check_result != SerializedCodeData::CHECK_SUCCESS) {
    if (FLAG_profile_deserialization) PrintF("[Cached code failed check]\n");
    DCHECK(cached_data->rejected());
    isolate->counters()->code_cache_reject_reason()->AddSample(
        sanity_check_result);
    return MaybeHandle<SharedFunctionInfo>();
  }

  // Deserialize.
  MaybeHandle<SharedFunctionInfo> maybe_result =
      ObjectDeserializer::DeserializeSharedFunctionInfo(isolate, &scd, source);

  Handle<SharedFunctionInfo> result;
  if (!maybe_result.ToHandle(&result)) {
    // Deserializing may fail if the reservations cannot be fulfilled.
    if (FLAG_profile_deserialization) PrintF("[Deserializing failed]\n");
    return MaybeHandle<SharedFunctionInfo>();
  }

  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int length = cached_data->length();
    PrintF("[Deserializing from %d bytes took %0.3f ms]\n", length, ms);
  }

  bool log_code_creation =
      isolate->logger()->is_listening_to_code_events() ||
      isolate->is_profiling() ||
      isolate->code_event_dispatcher()->IsListeningToCodeEvents();
  if (log_code_creation || FLAG_log_function_events) {
    String name = ReadOnlyRoots(isolate).empty_string();
    Script script = Script::cast(result->script());
    Handle<Script> script_handle(script, isolate);
    if (script->name()->IsString()) name = String::cast(script->name());
    if (FLAG_log_function_events) {
      LOG(isolate,
          FunctionEvent("deserialize", script->id(),
                        timer.Elapsed().InMillisecondsF(),
                        result->StartPosition(), result->EndPosition(), name));
    }
    if (log_code_creation) {
      Script::InitLineEnds(Handle<Script>(script, isolate));
      DisallowHeapAllocation no_gc;
      SharedFunctionInfo::ScriptIterator iter(isolate, script);
      for (i::SharedFunctionInfo info = iter.Next(); !info.is_null();
           info = iter.Next()) {
        if (info->is_compiled()) {
          int line_num = script->GetLineNumber(info->StartPosition()) + 1;
          int column_num = script->GetColumnNumber(info->StartPosition()) + 1;
          PROFILE(isolate, CodeCreateEvent(CodeEventListener::SCRIPT_TAG,
                                           info->abstract_code(), info, name,
                                           line_num, column_num));
        }
      }
    }
  }

  if (isolate->NeedsSourcePositionsForProfiling()) {
    Handle<Script> script(Script::cast(result->script()), isolate);
    Script::InitLineEnds(script);
  }
  return scope.CloseAndEscape(result);
}


SerializedCodeData::SerializedCodeData(const std::vector<byte>* payload,
                                       const CodeSerializer* cs) {
  DisallowHeapAllocation no_gc;
  std::vector<Reservation> reservations = cs->EncodeReservations();

  // Calculate sizes.
  uint32_t reservation_size =
      static_cast<uint32_t>(reservations.size()) * kUInt32Size;
  uint32_t num_stub_keys = 0;  // TODO(jgruber): Remove.
  uint32_t stub_keys_size = num_stub_keys * kUInt32Size;
  uint32_t payload_offset = kHeaderSize + reservation_size + stub_keys_size;
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
  SetHeaderValue(kVersionHashOffset, Version::Hash());
  SetHeaderValue(kSourceHashOffset, cs->source_hash());
  SetHeaderValue(kCpuFeaturesOffset,
                 static_cast<uint32_t>(CpuFeatures::SupportedFeatures()));
  SetHeaderValue(kFlagHashOffset, FlagList::Hash());
  SetHeaderValue(kNumReservationsOffset,
                 static_cast<uint32_t>(reservations.size()));
  SetHeaderValue(kPayloadLengthOffset, static_cast<uint32_t>(payload->size()));

  // Zero out any padding in the header.
  memset(data_ + kUnalignedHeaderSize, 0, kHeaderSize - kUnalignedHeaderSize);

  // Copy reservation chunk sizes.
  CopyBytes(data_ + kHeaderSize,
            reinterpret_cast<const byte*>(reservations.data()),
            reservation_size);

  // Copy serialized data.
  CopyBytes(data_ + padded_payload_offset, payload->data(),
            static_cast<size_t>(payload->size()));

  Checksum checksum(ChecksummedContent());
  SetHeaderValue(kChecksumPartAOffset, checksum.a());
  SetHeaderValue(kChecksumPartBOffset, checksum.b());
}

SerializedCodeData::SanityCheckResult SerializedCodeData::SanityCheck(
    Isolate* isolate, uint32_t expected_source_hash) const {
  if (this->size_ < kHeaderSize) return INVALID_HEADER;
  uint32_t magic_number = GetMagicNumber();
  if (magic_number != kMagicNumber) return MAGIC_NUMBER_MISMATCH;
  uint32_t version_hash = GetHeaderValue(kVersionHashOffset);
  uint32_t source_hash = GetHeaderValue(kSourceHashOffset);
  uint32_t cpu_features = GetHeaderValue(kCpuFeaturesOffset);
  uint32_t flags_hash = GetHeaderValue(kFlagHashOffset);
  uint32_t payload_length = GetHeaderValue(kPayloadLengthOffset);
  uint32_t c1 = GetHeaderValue(kChecksumPartAOffset);
  uint32_t c2 = GetHeaderValue(kChecksumPartBOffset);
  if (version_hash != Version::Hash()) return VERSION_MISMATCH;
  if (source_hash != expected_source_hash) return SOURCE_MISMATCH;
  if (cpu_features != static_cast<uint32_t>(CpuFeatures::SupportedFeatures())) {
    return CPU_FEATURES_MISMATCH;
  }
  if (flags_hash != FlagList::Hash()) return FLAGS_MISMATCH;
  uint32_t max_payload_length =
      this->size_ -
      POINTER_SIZE_ALIGN(kHeaderSize +
                         GetHeaderValue(kNumReservationsOffset) * kInt32Size);
  if (payload_length > max_payload_length) return LENGTH_MISMATCH;
  if (!Checksum(ChecksummedContent()).Check(c1, c2)) return CHECKSUM_MISMATCH;
  return CHECK_SUCCESS;
}

uint32_t SerializedCodeData::SourceHash(Handle<String> source,
                                        ScriptOriginOptions origin_options) {
  const uint32_t source_length = source->length();

  static constexpr uint32_t kModuleFlagMask = (1 << 31);
  const uint32_t is_module = origin_options.IsModule() ? kModuleFlagMask : 0;
  DCHECK_EQ(0, source_length & kModuleFlagMask);

  return source_length | is_module;
}

// Return ScriptData object and relinquish ownership over it to the caller.
ScriptData* SerializedCodeData::GetScriptData() {
  DCHECK(owns_data_);
  ScriptData* result = new ScriptData(data_, size_);
  result->AcquireDataOwnership();
  owns_data_ = false;
  data_ = nullptr;
  return result;
}

std::vector<SerializedData::Reservation> SerializedCodeData::Reservations()
    const {
  uint32_t size = GetHeaderValue(kNumReservationsOffset);
  std::vector<Reservation> reservations(size);
  memcpy(reservations.data(), data_ + kHeaderSize,
         size * sizeof(SerializedData::Reservation));
  return reservations;
}

Vector<const byte> SerializedCodeData::Payload() const {
  int reservations_size = GetHeaderValue(kNumReservationsOffset) * kInt32Size;
  int payload_offset = kHeaderSize + reservations_size;
  int padded_payload_offset = POINTER_SIZE_ALIGN(payload_offset);
  const byte* payload = data_ + padded_payload_offset;
  DCHECK(IsAligned(reinterpret_cast<intptr_t>(payload), kPointerAlignment));
  int length = GetHeaderValue(kPayloadLengthOffset);
  DCHECK_EQ(data_ + size_, payload + length);
  return Vector<const byte>(payload, length);
}

SerializedCodeData::SerializedCodeData(ScriptData* data)
    : SerializedData(const_cast<byte*>(data->data()), data->length()) {}

SerializedCodeData SerializedCodeData::FromCachedData(
    Isolate* isolate, ScriptData* cached_data, uint32_t expected_source_hash,
    SanityCheckResult* rejection_result) {
  DisallowHeapAllocation no_gc;
  SerializedCodeData scd(cached_data);
  *rejection_result = scd.SanityCheck(isolate, expected_source_hash);
  if (*rejection_result != CHECK_SUCCESS) {
    cached_data->Reject();
    return SerializedCodeData(nullptr, 0);
  }
  return scd;
}

}  // namespace internal
}  // namespace v8
