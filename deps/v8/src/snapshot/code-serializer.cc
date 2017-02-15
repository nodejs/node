// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/code-serializer.h"

#include <memory>

#include "src/code-stubs.h"
#include "src/log.h"
#include "src/macro-assembler.h"
#include "src/snapshot/deserializer.h"
#include "src/snapshot/snapshot.h"
#include "src/version.h"

namespace v8 {
namespace internal {

ScriptData* CodeSerializer::Serialize(Isolate* isolate,
                                      Handle<SharedFunctionInfo> info,
                                      Handle<String> source) {
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();
  if (FLAG_trace_serializer) {
    PrintF("[Serializing from");
    Object* script = info->script();
    if (script->IsScript()) Script::cast(script)->name()->ShortPrint();
    PrintF("]\n");
  }

  // Serialize code object.
  CodeSerializer cs(isolate, SerializedCodeData::SourceHash(source));
  DisallowHeapAllocation no_gc;
  cs.reference_map()->AddAttachedReference(*source);
  ScriptData* ret = cs.Serialize(info);

  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int length = ret->length();
    PrintF("[Serializing to %d bytes took %0.3f ms]\n", length, ms);
  }

  return ret;
}

ScriptData* CodeSerializer::Serialize(Handle<HeapObject> obj) {
  DisallowHeapAllocation no_gc;

  VisitPointer(Handle<Object>::cast(obj).location());
  SerializeDeferredObjects();
  Pad();

  SerializedCodeData data(sink()->data(), this);

  return data.GetScriptData();
}

void CodeSerializer::SerializeObject(HeapObject* obj, HowToCode how_to_code,
                                     WhereToPoint where_to_point, int skip) {
  if (SerializeHotObject(obj, how_to_code, where_to_point, skip)) return;

  int root_index = root_index_map_.Lookup(obj);
  if (root_index != RootIndexMap::kInvalidRootIndex) {
    PutRoot(root_index, obj, how_to_code, where_to_point, skip);
    return;
  }

  if (SerializeBackReference(obj, how_to_code, where_to_point, skip)) return;

  FlushSkip(skip);

  if (obj->IsCode()) {
    Code* code_object = Code::cast(obj);
    switch (code_object->kind()) {
      case Code::OPTIMIZED_FUNCTION:  // No optimized code compiled yet.
      case Code::HANDLER:             // No handlers patched in yet.
      case Code::REGEXP:              // No regexp literals initialized yet.
      case Code::NUMBER_OF_KINDS:     // Pseudo enum value.
      case Code::BYTECODE_HANDLER:    // No direct references to handlers.
        CHECK(false);
      case Code::BUILTIN:
        SerializeBuiltin(code_object->builtin_index(), how_to_code,
                         where_to_point);
        return;
      case Code::STUB:
#define IC_KIND_CASE(KIND) case Code::KIND:
        IC_KIND_LIST(IC_KIND_CASE)
#undef IC_KIND_CASE
        SerializeCodeStub(code_object, how_to_code, where_to_point);
        return;
      case Code::FUNCTION:
        DCHECK(code_object->has_reloc_info_for_serialization());
        SerializeGeneric(code_object, how_to_code, where_to_point);
        return;
      default:
        return SerializeCodeObject(code_object, how_to_code, where_to_point);
    }
    UNREACHABLE();
  }

  if (ElideObject(obj)) {
    return SerializeObject(*isolate()->factory()->undefined_value(),
                           how_to_code, where_to_point, skip);
  }
  // Past this point we should not see any (context-specific) maps anymore.
  CHECK(!obj->IsMap());
  // There should be no references to the global object embedded.
  CHECK(!obj->IsJSGlobalProxy() && !obj->IsJSGlobalObject());
  // There should be no hash table embedded. They would require rehashing.
  CHECK(!obj->IsHashTable());
  // We expect no instantiated function objects or contexts.
  CHECK(!obj->IsJSFunction() && !obj->IsContext());

  SerializeGeneric(obj, how_to_code, where_to_point);
}

void CodeSerializer::SerializeGeneric(HeapObject* heap_object,
                                      HowToCode how_to_code,
                                      WhereToPoint where_to_point) {
  // Object has not yet been serialized.  Serialize it here.
  ObjectSerializer serializer(this, heap_object, &sink_, how_to_code,
                              where_to_point);
  serializer.Serialize();
}

void CodeSerializer::SerializeBuiltin(int builtin_index, HowToCode how_to_code,
                                      WhereToPoint where_to_point) {
  DCHECK((how_to_code == kPlain && where_to_point == kStartOfObject) ||
         (how_to_code == kPlain && where_to_point == kInnerPointer) ||
         (how_to_code == kFromCode && where_to_point == kInnerPointer));
  DCHECK_LT(builtin_index, Builtins::builtin_count);
  DCHECK_LE(0, builtin_index);

  if (FLAG_trace_serializer) {
    PrintF(" Encoding builtin: %s\n",
           isolate()->builtins()->name(builtin_index));
  }

  sink_.Put(kBuiltin + how_to_code + where_to_point, "Builtin");
  sink_.PutInt(builtin_index, "builtin_index");
}

void CodeSerializer::SerializeCodeStub(Code* code_stub, HowToCode how_to_code,
                                       WhereToPoint where_to_point) {
  // We only arrive here if we have not encountered this code stub before.
  DCHECK(!reference_map()->Lookup(code_stub).is_valid());
  uint32_t stub_key = code_stub->stub_key();
  DCHECK(CodeStub::MajorKeyFromKey(stub_key) != CodeStub::NoCache);
  DCHECK(!CodeStub::GetCode(isolate(), stub_key).is_null());
  stub_keys_.Add(stub_key);

  SerializerReference reference =
      reference_map()->AddAttachedReference(code_stub);
  if (FLAG_trace_serializer) {
    PrintF(" Encoding code stub %s as attached reference %d\n",
           CodeStub::MajorName(CodeStub::MajorKeyFromKey(stub_key)),
           reference.attached_reference_index());
  }
  PutAttachedReference(reference, how_to_code, where_to_point);
}

MaybeHandle<SharedFunctionInfo> CodeSerializer::Deserialize(
    Isolate* isolate, ScriptData* cached_data, Handle<String> source) {
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  HandleScope scope(isolate);

  SerializedCodeData::SanityCheckResult sanity_check_result =
      SerializedCodeData::CHECK_SUCCESS;
  const SerializedCodeData scd = SerializedCodeData::FromCachedData(
      isolate, cached_data, SerializedCodeData::SourceHash(source),
      &sanity_check_result);
  if (sanity_check_result != SerializedCodeData::CHECK_SUCCESS) {
    if (FLAG_profile_deserialization) PrintF("[Cached code failed check]\n");
    DCHECK(cached_data->rejected());
    source->GetIsolate()->counters()->code_cache_reject_reason()->AddSample(
        sanity_check_result);
    return MaybeHandle<SharedFunctionInfo>();
  }

  Deserializer deserializer(&scd);
  deserializer.AddAttachedObject(source);
  Vector<const uint32_t> code_stub_keys = scd.CodeStubKeys();
  for (int i = 0; i < code_stub_keys.length(); i++) {
    deserializer.AddAttachedObject(
        CodeStub::GetCode(isolate, code_stub_keys[i]).ToHandleChecked());
  }

  // Deserialize.
  Handle<HeapObject> as_heap_object;
  if (!deserializer.DeserializeObject(isolate).ToHandle(&as_heap_object)) {
    // Deserializing may fail if the reservations cannot be fulfilled.
    if (FLAG_profile_deserialization) PrintF("[Deserializing failed]\n");
    return MaybeHandle<SharedFunctionInfo>();
  }

  Handle<SharedFunctionInfo> result =
      Handle<SharedFunctionInfo>::cast(as_heap_object);
  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int length = cached_data->length();
    PrintF("[Deserializing from %d bytes took %0.3f ms]\n", length, ms);
  }
  result->set_deserialized(true);

  if (isolate->logger()->is_logging_code_events() || isolate->is_profiling()) {
    String* name = isolate->heap()->empty_string();
    if (result->script()->IsScript()) {
      Script* script = Script::cast(result->script());
      if (script->name()->IsString()) name = String::cast(script->name());
    }
    PROFILE(isolate, CodeCreateEvent(CodeEventListener::SCRIPT_TAG,
                                     result->abstract_code(), *result, name));
  }
  return scope.CloseAndEscape(result);
}

std::unique_ptr<ScriptData> WasmCompiledModuleSerializer::SerializeWasmModule(
    Isolate* isolate, Handle<FixedArray> compiled_module) {
  WasmCompiledModuleSerializer wasm_cs(isolate, 0);
  wasm_cs.reference_map()->AddAttachedReference(*isolate->native_context());
  ScriptData* data = wasm_cs.Serialize(compiled_module);
  return std::unique_ptr<ScriptData>(data);
}

MaybeHandle<FixedArray> WasmCompiledModuleSerializer::DeserializeWasmModule(
    Isolate* isolate, ScriptData* data) {
  SerializedCodeData::SanityCheckResult sanity_check_result =
      SerializedCodeData::CHECK_SUCCESS;
  MaybeHandle<FixedArray> nothing;
  const SerializedCodeData scd = SerializedCodeData::FromCachedData(
      isolate, data, 0, &sanity_check_result);

  if (sanity_check_result != SerializedCodeData::CHECK_SUCCESS) {
    return nothing;
  }

  Deserializer deserializer(&scd, true);
  deserializer.AddAttachedObject(isolate->native_context());

  Vector<const uint32_t> stub_keys = scd.CodeStubKeys();
  for (int i = 0; i < stub_keys.length(); ++i) {
    deserializer.AddAttachedObject(
        CodeStub::GetCode(isolate, stub_keys[i]).ToHandleChecked());
  }

  MaybeHandle<HeapObject> obj = deserializer.DeserializeObject(isolate);
  if (obj.is_null() || !obj.ToHandleChecked()->IsFixedArray()) return nothing;
  return Handle<FixedArray>::cast(obj.ToHandleChecked());
}

class Checksum {
 public:
  explicit Checksum(Vector<const byte> payload) {
#ifdef MEMORY_SANITIZER
    // Computing the checksum includes padding bytes for objects like strings.
    // Mark every object as initialized in the code serializer.
    MSAN_MEMORY_IS_INITIALIZED(payload.start(), payload.length());
#endif  // MEMORY_SANITIZER
    // Fletcher's checksum. Modified to reduce 64-bit sums to 32-bit.
    uintptr_t a = 1;
    uintptr_t b = 0;
    const uintptr_t* cur = reinterpret_cast<const uintptr_t*>(payload.start());
    DCHECK(IsAligned(payload.length(), kIntptrSize));
    const uintptr_t* end = cur + payload.length() / kIntptrSize;
    while (cur < end) {
      // Unsigned overflow expected and intended.
      a += *cur++;
      b += a;
    }
#if V8_HOST_ARCH_64_BIT
    a ^= a >> 32;
    b ^= b >> 32;
#endif  // V8_HOST_ARCH_64_BIT
    a_ = static_cast<uint32_t>(a);
    b_ = static_cast<uint32_t>(b);
  }

  bool Check(uint32_t a, uint32_t b) const { return a == a_ && b == b_; }

  uint32_t a() const { return a_; }
  uint32_t b() const { return b_; }

 private:
  uint32_t a_;
  uint32_t b_;

  DISALLOW_COPY_AND_ASSIGN(Checksum);
};

SerializedCodeData::SerializedCodeData(const List<byte>* payload,
                                       const CodeSerializer* cs) {
  DisallowHeapAllocation no_gc;
  const List<uint32_t>* stub_keys = cs->stub_keys();

  List<Reservation> reservations;
  cs->EncodeReservations(&reservations);

  // Calculate sizes.
  int reservation_size = reservations.length() * kInt32Size;
  int num_stub_keys = stub_keys->length();
  int stub_keys_size = stub_keys->length() * kInt32Size;
  int payload_offset = kHeaderSize + reservation_size + stub_keys_size;
  int padded_payload_offset = POINTER_SIZE_ALIGN(payload_offset);
  int size = padded_payload_offset + payload->length();

  // Allocate backing store and create result data.
  AllocateData(size);

  // Set header values.
  SetMagicNumber(cs->isolate());
  SetHeaderValue(kVersionHashOffset, Version::Hash());
  SetHeaderValue(kSourceHashOffset, cs->source_hash());
  SetHeaderValue(kCpuFeaturesOffset,
                 static_cast<uint32_t>(CpuFeatures::SupportedFeatures()));
  SetHeaderValue(kFlagHashOffset, FlagList::Hash());
  SetHeaderValue(kNumReservationsOffset, reservations.length());
  SetHeaderValue(kNumCodeStubKeysOffset, num_stub_keys);
  SetHeaderValue(kPayloadLengthOffset, payload->length());

  // Copy reservation chunk sizes.
  CopyBytes(data_ + kHeaderSize, reinterpret_cast<byte*>(reservations.begin()),
            reservation_size);

  // Copy code stub keys.
  CopyBytes(data_ + kHeaderSize + reservation_size,
            reinterpret_cast<byte*>(stub_keys->begin()), stub_keys_size);

  memset(data_ + payload_offset, 0, padded_payload_offset - payload_offset);

  // Copy serialized data.
  CopyBytes(data_ + padded_payload_offset, payload->begin(),
            static_cast<size_t>(payload->length()));

  Checksum checksum(DataWithoutHeader());
  SetHeaderValue(kChecksum1Offset, checksum.a());
  SetHeaderValue(kChecksum2Offset, checksum.b());
}

SerializedCodeData::SanityCheckResult SerializedCodeData::SanityCheck(
    Isolate* isolate, uint32_t expected_source_hash) const {
  uint32_t magic_number = GetMagicNumber();
  if (magic_number != ComputeMagicNumber(isolate)) return MAGIC_NUMBER_MISMATCH;
  uint32_t version_hash = GetHeaderValue(kVersionHashOffset);
  uint32_t source_hash = GetHeaderValue(kSourceHashOffset);
  uint32_t cpu_features = GetHeaderValue(kCpuFeaturesOffset);
  uint32_t flags_hash = GetHeaderValue(kFlagHashOffset);
  uint32_t c1 = GetHeaderValue(kChecksum1Offset);
  uint32_t c2 = GetHeaderValue(kChecksum2Offset);
  if (version_hash != Version::Hash()) return VERSION_MISMATCH;
  if (source_hash != expected_source_hash) return SOURCE_MISMATCH;
  if (cpu_features != static_cast<uint32_t>(CpuFeatures::SupportedFeatures())) {
    return CPU_FEATURES_MISMATCH;
  }
  if (flags_hash != FlagList::Hash()) return FLAGS_MISMATCH;
  if (!Checksum(DataWithoutHeader()).Check(c1, c2)) return CHECKSUM_MISMATCH;
  return CHECK_SUCCESS;
}

uint32_t SerializedCodeData::SourceHash(Handle<String> source) {
  return source->length();
}

// Return ScriptData object and relinquish ownership over it to the caller.
ScriptData* SerializedCodeData::GetScriptData() {
  DCHECK(owns_data_);
  ScriptData* result = new ScriptData(data_, size_);
  result->AcquireDataOwnership();
  owns_data_ = false;
  data_ = NULL;
  return result;
}

Vector<const SerializedData::Reservation> SerializedCodeData::Reservations()
    const {
  return Vector<const Reservation>(
      reinterpret_cast<const Reservation*>(data_ + kHeaderSize),
      GetHeaderValue(kNumReservationsOffset));
}

Vector<const byte> SerializedCodeData::Payload() const {
  int reservations_size = GetHeaderValue(kNumReservationsOffset) * kInt32Size;
  int code_stubs_size = GetHeaderValue(kNumCodeStubKeysOffset) * kInt32Size;
  int payload_offset = kHeaderSize + reservations_size + code_stubs_size;
  int padded_payload_offset = POINTER_SIZE_ALIGN(payload_offset);
  const byte* payload = data_ + padded_payload_offset;
  DCHECK(IsAligned(reinterpret_cast<intptr_t>(payload), kPointerAlignment));
  int length = GetHeaderValue(kPayloadLengthOffset);
  DCHECK_EQ(data_ + size_, payload + length);
  return Vector<const byte>(payload, length);
}

Vector<const uint32_t> SerializedCodeData::CodeStubKeys() const {
  int reservations_size = GetHeaderValue(kNumReservationsOffset) * kInt32Size;
  const byte* start = data_ + kHeaderSize + reservations_size;
  return Vector<const uint32_t>(reinterpret_cast<const uint32_t*>(start),
                                GetHeaderValue(kNumCodeStubKeysOffset));
}

SerializedCodeData::SerializedCodeData(ScriptData* data)
    : SerializedData(const_cast<byte*>(data->data()), data->length()) {}

const SerializedCodeData SerializedCodeData::FromCachedData(
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
