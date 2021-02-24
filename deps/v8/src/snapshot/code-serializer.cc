// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/code-serializer.h"

#include "src/base/platform/platform.h"
#include "src/codegen/macro-assembler.h"
#include "src/common/globals.h"
#include "src/debug/debug.h"
#include "src/heap/heap-inl.h"
#include "src/heap/local-factory-inl.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/objects/objects-inl.h"
#include "src/objects/slots.h"
#include "src/objects/visitors.h"
#include "src/snapshot/object-deserializer.h"
#include "src/snapshot/snapshot-utils.h"
#include "src/snapshot/snapshot.h"
#include "src/utils/version.h"

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
    : Serializer(isolate, Snapshot::kDefaultSerializerFlags),
      source_hash_(source_hash) {}

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
    script->name().ShortPrint();
    PrintF("]\n");
  }
  // TODO(7110): Enable serialization of Asm modules once the AsmWasmData is
  // context independent.
  if (script->ContainsAsmModule()) return nullptr;

  // Serialize code object.
  Handle<String> source(String::cast(script->source()), isolate);
  HandleScope scope(isolate);
  CodeSerializer cs(isolate, SerializedCodeData::SourceHash(
                                 source, script->origin_options()));
  DisallowGarbageCollection no_gc;
  cs.reference_map()->AddAttachedReference(*source);
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
  DisallowGarbageCollection no_gc;

  VisitRootPointer(Root::kHandleScope, nullptr,
                   FullObjectSlot(info.location()));
  SerializeDeferredObjects();
  Pad();

  SerializedCodeData data(sink_.data(), this);

  return data.GetScriptData();
}

bool CodeSerializer::SerializeReadOnlyObject(Handle<HeapObject> obj) {
  if (!ReadOnlyHeap::Contains(*obj)) return false;

  // For objects on the read-only heap, never serialize the object, but instead
  // create a back reference that encodes the page number as the chunk_index and
  // the offset within the page as the chunk_offset.
  Address address = obj->address();
  BasicMemoryChunk* chunk = BasicMemoryChunk::FromAddress(address);
  uint32_t chunk_index = 0;
  ReadOnlySpace* const read_only_space = isolate()->heap()->read_only_space();
  for (ReadOnlyPage* page : read_only_space->pages()) {
    if (chunk == page) break;
    ++chunk_index;
  }
  uint32_t chunk_offset = static_cast<uint32_t>(chunk->Offset(address));
  sink_.Put(kReadOnlyHeapRef, "ReadOnlyHeapRef");
  sink_.PutInt(chunk_index, "ReadOnlyHeapRefChunkIndex");
  sink_.PutInt(chunk_offset, "ReadOnlyHeapRefChunkOffset");
  return true;
}

void CodeSerializer::SerializeObjectImpl(Handle<HeapObject> obj) {
  if (SerializeHotObject(obj)) return;

  if (SerializeRoot(obj)) return;

  if (SerializeBackReference(obj)) return;

  if (SerializeReadOnlyObject(obj)) return;

  CHECK(!obj->IsCode());

  ReadOnlyRoots roots(isolate());
  if (ElideObject(*obj)) {
    return SerializeObject(roots.undefined_value_handle());
  }

  if (obj->IsScript()) {
    Handle<Script> script_obj = Handle<Script>::cast(obj);
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
    SerializeGeneric(obj);
    script_obj->set_host_defined_options(host_options);
    script_obj->set_context_data(context_data);
    return;
  }

  if (obj->IsSharedFunctionInfo()) {
    Handle<SharedFunctionInfo> sfi = Handle<SharedFunctionInfo>::cast(obj);
    // TODO(7110): Enable serializing of Asm modules once the AsmWasmData
    // is context independent.
    DCHECK(!sfi->IsApiFunction() && !sfi->HasAsmWasmData());

    DebugInfo debug_info;
    BytecodeArray debug_bytecode_array;
    if (sfi->HasDebugInfo()) {
      // Clear debug info.
      debug_info = sfi->GetDebugInfo();
      if (debug_info.HasInstrumentedBytecodeArray()) {
        debug_bytecode_array = debug_info.DebugBytecodeArray();
        sfi->SetActiveBytecodeArray(debug_info.OriginalBytecodeArray());
      }
      sfi->set_script_or_debug_info(debug_info.script(), kReleaseStore);
    }
    DCHECK(!sfi->HasDebugInfo());

    SerializeGeneric(obj);

    // Restore debug info
    if (!debug_info.is_null()) {
      sfi->set_script_or_debug_info(debug_info, kReleaseStore);
      if (!debug_bytecode_array.is_null()) {
        sfi->SetActiveBytecodeArray(debug_bytecode_array);
      }
    }
    return;
  }

  // NOTE(mmarchini): If we try to serialize an InterpreterData our process
  // will crash since it stores a code object. Instead, we serialize the
  // bytecode array stored within the InterpreterData, which is the important
  // information. On deserialization we'll create our code objects again, if
  // --interpreted-frames-native-stack is on. See v8:9122 for more context
#ifndef V8_TARGET_ARCH_ARM
  if (V8_UNLIKELY(FLAG_interpreted_frames_native_stack) &&
      obj->IsInterpreterData()) {
    obj = handle(InterpreterData::cast(*obj).bytecode_array(), isolate());
  }
#endif  // V8_TARGET_ARCH_ARM

  // Past this point we should not see any (context-specific) maps anymore.
  CHECK(!obj->IsMap());
  // There should be no references to the global object embedded.
  CHECK(!obj->IsJSGlobalProxy() && !obj->IsJSGlobalObject());
  // Embedded FixedArrays that need rehashing must support rehashing.
  CHECK_IMPLIES(obj->NeedsRehashing(), obj->CanBeRehashed());
  // We expect no instantiated function objects or contexts.
  CHECK(!obj->IsJSFunction() && !obj->IsContext());

  SerializeGeneric(obj);
}

void CodeSerializer::SerializeGeneric(Handle<HeapObject> heap_object) {
  // Object has not yet been serialized.  Serialize it here.
  ObjectSerializer serializer(this, heap_object, &sink_);
  serializer.Serialize();
}

#ifndef V8_TARGET_ARCH_ARM
// NOTE(mmarchini): when FLAG_interpreted_frames_native_stack is on, we want to
// create duplicates of InterpreterEntryTrampoline for the deserialized
// functions, otherwise we'll call the builtin IET for those functions (which
// is not what a user of this flag wants).
void CreateInterpreterDataForDeserializedCode(Isolate* isolate,
                                              Handle<SharedFunctionInfo> sfi,
                                              bool log_code_creation) {
  Handle<Script> script(Script::cast(sfi->script()), isolate);
  String name = ReadOnlyRoots(isolate).empty_string();
  if (script->name().IsString()) name = String::cast(script->name());
  Handle<String> name_handle(name, isolate);

  SharedFunctionInfo::ScriptIterator iter(isolate, *script);
  for (SharedFunctionInfo shared_info = iter.Next(); !shared_info.is_null();
       shared_info = iter.Next()) {
    if (!shared_info.HasBytecodeArray()) continue;
    Handle<SharedFunctionInfo> info = handle(shared_info, isolate);
    Handle<Code> code = isolate->factory()->CopyCode(Handle<Code>::cast(
        isolate->factory()->interpreter_entry_trampoline_for_profiling()));

    Handle<InterpreterData> interpreter_data =
        Handle<InterpreterData>::cast(isolate->factory()->NewStruct(
            INTERPRETER_DATA_TYPE, AllocationType::kOld));

    interpreter_data->set_bytecode_array(info->GetBytecodeArray(isolate));
    interpreter_data->set_interpreter_trampoline(*code);

    info->set_interpreter_data(*interpreter_data);

    if (!log_code_creation) continue;
    Handle<AbstractCode> abstract_code = Handle<AbstractCode>::cast(code);
    int line_num = script->GetLineNumber(info->StartPosition()) + 1;
    int column_num = script->GetColumnNumber(info->StartPosition()) + 1;
    PROFILE(isolate,
            CodeCreateEvent(CodeEventListener::INTERPRETED_FUNCTION_TAG,
                            abstract_code, info, name_handle, line_num,
                            column_num));
  }
}
#endif  // V8_TARGET_ARCH_ARM

namespace {
class StressOffThreadDeserializeThread final : public base::Thread {
 public:
  explicit StressOffThreadDeserializeThread(Isolate* isolate,
                                            const SerializedCodeData* scd)
      : Thread(
            base::Thread::Options("StressOffThreadDeserializeThread", 2 * MB)),
        isolate_(isolate),
        scd_(scd) {}

  MaybeHandle<SharedFunctionInfo> maybe_result() const { return maybe_result_; }

  void Run() final {
    LocalIsolate local_isolate(isolate_, ThreadKind::kBackground);
    MaybeHandle<SharedFunctionInfo> local_maybe_result =
        ObjectDeserializer::DeserializeSharedFunctionInfoOffThread(
            &local_isolate, scd_, local_isolate.factory()->empty_string());

    maybe_result_ =
        local_isolate.heap()->NewPersistentMaybeHandle(local_maybe_result);
  }

 private:
  Isolate* isolate_;
  const SerializedCodeData* scd_;
  MaybeHandle<SharedFunctionInfo> maybe_result_;
};
}  // namespace

MaybeHandle<SharedFunctionInfo> CodeSerializer::Deserialize(
    Isolate* isolate, ScriptData* cached_data, Handle<String> source,
    ScriptOriginOptions origin_options) {
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization || FLAG_log_function_events) timer.Start();

  HandleScope scope(isolate);

  SerializedCodeData::SanityCheckResult sanity_check_result =
      SerializedCodeData::CHECK_SUCCESS;
  const SerializedCodeData scd = SerializedCodeData::FromCachedData(
      cached_data, SerializedCodeData::SourceHash(source, origin_options),
      &sanity_check_result);
  if (sanity_check_result != SerializedCodeData::CHECK_SUCCESS) {
    if (FLAG_profile_deserialization) PrintF("[Cached code failed check]\n");
    DCHECK(cached_data->rejected());
    isolate->counters()->code_cache_reject_reason()->AddSample(
        sanity_check_result);
    return MaybeHandle<SharedFunctionInfo>();
  }

  // Deserialize.
  MaybeHandle<SharedFunctionInfo> maybe_result;
  // TODO(leszeks): Add LocalHeap support to deserializer
  if (false && FLAG_stress_background_compile) {
    StressOffThreadDeserializeThread thread(isolate, &scd);
    CHECK(thread.Start());
    thread.Join();

    maybe_result = thread.maybe_result();

    // Fix-up result script source.
    Handle<SharedFunctionInfo> result;
    if (maybe_result.ToHandle(&result)) {
      Script::cast(result->script()).set_source(*source);
    }
  } else {
    maybe_result = ObjectDeserializer::DeserializeSharedFunctionInfo(
        isolate, &scd, source);
  }

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

  const bool log_code_creation =
      isolate->logger()->is_listening_to_code_events() ||
      isolate->is_profiling() ||
      isolate->code_event_dispatcher()->IsListeningToCodeEvents();

#ifndef V8_TARGET_ARCH_ARM
  if (V8_UNLIKELY(FLAG_interpreted_frames_native_stack))
    CreateInterpreterDataForDeserializedCode(isolate, result,
                                             log_code_creation);
#endif  // V8_TARGET_ARCH_ARM

  bool needs_source_positions = isolate->NeedsSourcePositionsForProfiling();

  if (log_code_creation || FLAG_log_function_events) {
    Handle<Script> script(Script::cast(result->script()), isolate);
    Handle<String> name(script->name().IsString()
                            ? String::cast(script->name())
                            : ReadOnlyRoots(isolate).empty_string(),
                        isolate);

    if (FLAG_log_function_events) {
      LOG(isolate,
          FunctionEvent("deserialize", script->id(),
                        timer.Elapsed().InMillisecondsF(),
                        result->StartPosition(), result->EndPosition(), *name));
    }
    if (log_code_creation) {
      Script::InitLineEnds(isolate, script);

      SharedFunctionInfo::ScriptIterator iter(isolate, *script);
      for (SharedFunctionInfo info = iter.Next(); !info.is_null();
           info = iter.Next()) {
        if (info.is_compiled()) {
          Handle<SharedFunctionInfo> shared_info(info, isolate);
          if (needs_source_positions) {
            SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate,
                                                               shared_info);
          }
          DisallowGarbageCollection no_gc;
          int line_num =
              script->GetLineNumber(shared_info->StartPosition()) + 1;
          int column_num =
              script->GetColumnNumber(shared_info->StartPosition()) + 1;
          PROFILE(isolate,
                  CodeCreateEvent(
                      CodeEventListener::SCRIPT_TAG,
                      handle(shared_info->abstract_code(isolate), isolate),
                      shared_info, name, line_num, column_num));
        }
      }
    }
  }

  if (needs_source_positions) {
    Handle<Script> script(Script::cast(result->script()), isolate);
    Script::InitLineEnds(isolate, script);
  }
  return scope.CloseAndEscape(result);
}

SerializedCodeData::SerializedCodeData(const std::vector<byte>* payload,
                                       const CodeSerializer* cs) {
  DisallowGarbageCollection no_gc;

  // Calculate sizes.
  uint32_t size = kHeaderSize + static_cast<uint32_t>(payload->size());
  DCHECK(IsAligned(size, kPointerAlignment));

  // Allocate backing store and create result data.
  AllocateData(size);

  // Zero out pre-payload data. Part of that is only used for padding.
  memset(data_, 0, kHeaderSize);

  // Set header values.
  SetMagicNumber();
  SetHeaderValue(kVersionHashOffset, Version::Hash());
  SetHeaderValue(kSourceHashOffset, cs->source_hash());
  SetHeaderValue(kFlagHashOffset, FlagList::Hash());
  SetHeaderValue(kPayloadLengthOffset, static_cast<uint32_t>(payload->size()));

  // Zero out any padding in the header.
  memset(data_ + kUnalignedHeaderSize, 0, kHeaderSize - kUnalignedHeaderSize);

  // Copy serialized data.
  CopyBytes(data_ + kHeaderSize, payload->data(),
            static_cast<size_t>(payload->size()));

  SetHeaderValue(kChecksumOffset, Checksum(ChecksummedContent()));
}

SerializedCodeData::SanityCheckResult SerializedCodeData::SanityCheck(
    uint32_t expected_source_hash) const {
  if (this->size_ < kHeaderSize) return INVALID_HEADER;
  uint32_t magic_number = GetMagicNumber();
  if (magic_number != kMagicNumber) return MAGIC_NUMBER_MISMATCH;
  uint32_t version_hash = GetHeaderValue(kVersionHashOffset);
  uint32_t source_hash = GetHeaderValue(kSourceHashOffset);
  uint32_t flags_hash = GetHeaderValue(kFlagHashOffset);
  uint32_t payload_length = GetHeaderValue(kPayloadLengthOffset);
  uint32_t c = GetHeaderValue(kChecksumOffset);
  if (version_hash != Version::Hash()) return VERSION_MISMATCH;
  if (source_hash != expected_source_hash) return SOURCE_MISMATCH;
  if (flags_hash != FlagList::Hash()) return FLAGS_MISMATCH;
  uint32_t max_payload_length = this->size_ - kHeaderSize;
  if (payload_length > max_payload_length) return LENGTH_MISMATCH;
  if (Checksum(ChecksummedContent()) != c) return CHECKSUM_MISMATCH;
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

Vector<const byte> SerializedCodeData::Payload() const {
  const byte* payload = data_ + kHeaderSize;
  DCHECK(IsAligned(reinterpret_cast<intptr_t>(payload), kPointerAlignment));
  int length = GetHeaderValue(kPayloadLengthOffset);
  DCHECK_EQ(data_ + size_, payload + length);
  return Vector<const byte>(payload, length);
}

SerializedCodeData::SerializedCodeData(ScriptData* data)
    : SerializedData(const_cast<byte*>(data->data()), data->length()) {}

SerializedCodeData SerializedCodeData::FromCachedData(
    ScriptData* cached_data, uint32_t expected_source_hash,
    SanityCheckResult* rejection_result) {
  DisallowGarbageCollection no_gc;
  SerializedCodeData scd(cached_data);
  *rejection_result = scd.SanityCheck(expected_source_hash);
  if (*rejection_result != CHECK_SUCCESS) {
    cached_data->Reject();
    return SerializedCodeData(nullptr, 0);
  }
  return scd;
}

}  // namespace internal
}  // namespace v8
