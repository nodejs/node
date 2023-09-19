// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/code-serializer.h"

#include <memory>

#include "src/base/logging.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/platform.h"
#include "src/baseline/baseline-batch-compiler.h"
#include "src/codegen/background-merge-task.h"
#include "src/common/globals.h"
#include "src/handles/maybe-handles.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/heap-inl.h"
#include "src/heap/parked-scope.h"
#include "src/logging/counters-scopes.h"
#include "src/logging/log.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/slots.h"
#include "src/objects/visitors.h"
#include "src/snapshot/object-deserializer.h"
#include "src/snapshot/snapshot-utils.h"
#include "src/snapshot/snapshot.h"
#include "src/utils/version.h"

namespace v8 {
namespace internal {

AlignedCachedData::AlignedCachedData(const byte* data, int length)
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
    Isolate* isolate, Handle<SharedFunctionInfo> info) {
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.Execute");
  NestedTimedHistogramScope histogram_timer(
      isolate->counters()->compile_serialize());
  RCS_SCOPE(isolate, RuntimeCallCounterId::kCompileSerialize);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileSerialize");

  base::ElapsedTimer timer;
  if (v8_flags.profile_deserialization) timer.Start();
  Handle<Script> script(Script::cast(info->script()), isolate);
  if (v8_flags.trace_serializer) {
    PrintF("[Serializing from");
    script->name().ShortPrint();
    PrintF("]\n");
  }
#if V8_ENABLE_WEBASSEMBLY
  // TODO(7110): Enable serialization of Asm modules once the AsmWasmData is
  // context independent.
  if (script->ContainsAsmModule()) return nullptr;
#endif  // V8_ENABLE_WEBASSEMBLY

  // Serialize code object.
  Handle<String> source(String::cast(script->source()), isolate);
  HandleScope scope(isolate);
  CodeSerializer cs(isolate, SerializedCodeData::SourceHash(
                                 source, script->origin_options()));
  DisallowGarbageCollection no_gc;
  cs.reference_map()->AddAttachedReference(*source);
  AlignedCachedData* cached_data = cs.SerializeSharedFunctionInfo(info);

  if (v8_flags.profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int length = cached_data->length();
    PrintF("[Serializing to %d bytes took %0.3f ms]\n", length, ms);
  }

  ScriptCompiler::CachedData* result =
      new ScriptCompiler::CachedData(cached_data->data(), cached_data->length(),
                                     ScriptCompiler::CachedData::BufferOwned);
  cached_data->ReleaseDataOwnership();
  delete cached_data;

  return result;
}

AlignedCachedData* CodeSerializer::SerializeSharedFunctionInfo(
    Handle<SharedFunctionInfo> info) {
  DisallowGarbageCollection no_gc;

  VisitRootPointer(Root::kHandleScope, nullptr,
                   FullObjectSlot(info.location()));
  SerializeDeferredObjects();
  Pad();

  SerializedCodeData data(sink_.data(), this);

  return data.GetScriptData();
}

void CodeSerializer::SerializeObjectImpl(Handle<HeapObject> obj) {
  ReadOnlyRoots roots(isolate());
  InstanceType instance_type;
  {
    DisallowGarbageCollection no_gc;
    HeapObject raw = *obj;
    if (SerializeHotObject(raw)) return;
    if (SerializeRoot(raw)) return;
    if (SerializeBackReference(raw)) return;
    if (SerializeReadOnlyObjectReference(raw, &sink_)) return;

    instance_type = raw.map().instance_type();
    CHECK(!InstanceTypeChecker::IsInstructionStream(instance_type));

    if (ElideObject(raw)) {
      AllowGarbageCollection allow_gc;
      return SerializeObject(roots.undefined_value_handle());
    }
  }

  if (InstanceTypeChecker::IsScript(instance_type)) {
    Handle<FixedArray> host_options;
    Handle<Object> context_data;
    {
      DisallowGarbageCollection no_gc;
      Script script_obj = Script::cast(*obj);
      DCHECK_NE(script_obj.compilation_type(), Script::COMPILATION_TYPE_EVAL);
      // We want to differentiate between undefined and uninitialized_symbol for
      // context_data for now. It is hack to allow debugging for scripts that
      // are included as a part of custom snapshot. (see
      // debug::Script::IsEmbedded())
      Object raw_context_data = script_obj.context_data();
      if (raw_context_data != roots.undefined_value() &&
          raw_context_data != roots.uninitialized_symbol()) {
        script_obj.set_context_data(roots.undefined_value());
      }
      context_data = handle(raw_context_data, isolate());
      // We don't want to serialize host options to avoid serializing
      // unnecessary object graph.
      host_options = handle(script_obj.host_defined_options(), isolate());
      script_obj.set_host_defined_options(roots.empty_fixed_array());
    }
    SerializeGeneric(obj);
    {
      DisallowGarbageCollection no_gc;
      Script script_obj = Script::cast(*obj);
      script_obj.set_host_defined_options(*host_options);
      script_obj.set_context_data(*context_data);
    }
    return;
  } else if (InstanceTypeChecker::IsSharedFunctionInfo(instance_type)) {
    Handle<DebugInfo> debug_info;
    bool restore_bytecode = false;
    {
      DisallowGarbageCollection no_gc;
      SharedFunctionInfo sfi = SharedFunctionInfo::cast(*obj);
      DCHECK(!sfi.IsApiFunction());
#if V8_ENABLE_WEBASSEMBLY
      // TODO(7110): Enable serializing of Asm modules once the AsmWasmData
      // is context independent.
      DCHECK(!sfi.HasAsmWasmData());
#endif  // V8_ENABLE_WEBASSEMBLY

      if (sfi.HasDebugInfo()) {
        // Clear debug info.
        DebugInfo raw_debug_info = sfi.GetDebugInfo();
        if (raw_debug_info.HasInstrumentedBytecodeArray()) {
          restore_bytecode = true;
          sfi.SetActiveBytecodeArray(raw_debug_info.OriginalBytecodeArray());
        }
        sfi.set_script_or_debug_info(raw_debug_info.script(), kReleaseStore);
        debug_info = handle(raw_debug_info, isolate());
      }
      DCHECK(!sfi.HasDebugInfo());
    }
    SerializeGeneric(obj);
    // Restore debug info
    if (!debug_info.is_null()) {
      DisallowGarbageCollection no_gc;
      SharedFunctionInfo sfi = SharedFunctionInfo::cast(*obj);
      sfi.set_script_or_debug_info(*debug_info, kReleaseStore);
      if (restore_bytecode) {
        sfi.SetActiveBytecodeArray(debug_info->DebugBytecodeArray());
      }
    }
    return;
  } else if (InstanceTypeChecker::IsUncompiledDataWithoutPreparseDataWithJob(
                 instance_type)) {
    Handle<UncompiledDataWithoutPreparseDataWithJob> data =
        Handle<UncompiledDataWithoutPreparseDataWithJob>::cast(obj);
    Address job = data->job();
    data->set_job(kNullAddress);
    SerializeGeneric(data);
    data->set_job(job);
    return;
  } else if (InstanceTypeChecker::IsUncompiledDataWithPreparseDataAndJob(
                 instance_type)) {
    Handle<UncompiledDataWithPreparseDataAndJob> data =
        Handle<UncompiledDataWithPreparseDataAndJob>::cast(obj);
    Address job = data->job();
    data->set_job(kNullAddress);
    SerializeGeneric(data);
    data->set_job(job);
    return;
  }

  // NOTE(mmarchini): If we try to serialize an InterpreterData our process
  // will crash since it stores a code object. Instead, we serialize the
  // bytecode array stored within the InterpreterData, which is the important
  // information. On deserialization we'll create our code objects again, if
  // --interpreted-frames-native-stack is on. See v8:9122 for more context
  if (V8_UNLIKELY(v8_flags.interpreted_frames_native_stack) &&
      obj->IsInterpreterData()) {
    obj = handle(InterpreterData::cast(*obj).bytecode_array(), isolate());
  }

  // Past this point we should not see any (context-specific) maps anymore.
  CHECK(!InstanceTypeChecker::IsMap(instance_type));
  // There should be no references to the global object embedded.
  CHECK(!InstanceTypeChecker::IsJSGlobalProxy(instance_type) &&
        !InstanceTypeChecker::IsJSGlobalObject(instance_type));
  // Embedded FixedArrays that need rehashing must support rehashing.
  CHECK_IMPLIES(obj->NeedsRehashing(cage_base()),
                obj->CanBeRehashed(cage_base()));
  // We expect no instantiated function objects or contexts.
  CHECK(!InstanceTypeChecker::IsJSFunction(instance_type) &&
        !InstanceTypeChecker::IsContext(instance_type));

  SerializeGeneric(obj);
}

void CodeSerializer::SerializeGeneric(Handle<HeapObject> heap_object) {
  // Object has not yet been serialized.  Serialize it here.
  ObjectSerializer serializer(this, heap_object, &sink_);
  serializer.Serialize();
}

namespace {

// NOTE(mmarchini): when v8_flags.interpreted_frames_native_stack is on, we want
// to create duplicates of InterpreterEntryTrampoline for the deserialized
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
    IsCompiledScope is_compiled(shared_info, isolate);
    if (!is_compiled.is_compiled()) continue;
    DCHECK(shared_info.HasBytecodeArray());
    Handle<SharedFunctionInfo> info = handle(shared_info, isolate);

    Handle<Code> code =
        Builtins::CreateInterpreterEntryTrampolineForProfiling(isolate);

    Handle<InterpreterData> interpreter_data =
        Handle<InterpreterData>::cast(isolate->factory()->NewStruct(
            INTERPRETER_DATA_TYPE, AllocationType::kOld));

    interpreter_data->set_bytecode_array(info->GetBytecodeArray(isolate));
    interpreter_data->set_interpreter_trampoline(*code);
    if (info->HasBaselineCode()) {
      info->baseline_code(kAcquireLoad)
          .set_bytecode_or_interpreter_data(*interpreter_data);
    } else {
      info->set_interpreter_data(*interpreter_data);
    }

    if (!log_code_creation) continue;
    Handle<AbstractCode> abstract_code = Handle<AbstractCode>::cast(code);
    Script::InitLineEnds(isolate, script);
    int line_num = script->GetLineNumber(info->StartPosition()) + 1;
    int column_num = script->GetColumnNumber(info->StartPosition()) + 1;
    PROFILE(isolate,
            CodeCreateEvent(LogEventListener::CodeTag::kFunction, abstract_code,
                            info, name_handle, line_num, column_num));
  }
}

class StressOffThreadDeserializeThread final : public base::Thread {
 public:
  explicit StressOffThreadDeserializeThread(Isolate* isolate,
                                            AlignedCachedData* cached_data)
      : Thread(
            base::Thread::Options("StressOffThreadDeserializeThread", 2 * MB)),
        isolate_(isolate),
        cached_data_(cached_data) {}

  void Run() final {
    LocalIsolate local_isolate(isolate_, ThreadKind::kBackground);
    UnparkedScope unparked_scope(&local_isolate);
    LocalHandleScope handle_scope(&local_isolate);
    off_thread_data_ =
        CodeSerializer::StartDeserializeOffThread(&local_isolate, cached_data_);
  }

  MaybeHandle<SharedFunctionInfo> Finalize(Isolate* isolate,
                                           Handle<String> source,
                                           ScriptOriginOptions origin_options) {
    return CodeSerializer::FinishOffThreadDeserialize(
        isolate, std::move(off_thread_data_), cached_data_, source,
        origin_options);
  }

 private:
  Isolate* isolate_;
  AlignedCachedData* cached_data_;
  CodeSerializer::OffThreadDeserializeData off_thread_data_;
};

void FinalizeDeserialization(Isolate* isolate,
                             Handle<SharedFunctionInfo> result,
                             const base::ElapsedTimer& timer) {
  const bool log_code_creation =
      isolate->v8_file_logger()->is_listening_to_code_events() ||
      isolate->is_profiling() ||
      isolate->logger()->is_listening_to_code_events();

  if (V8_UNLIKELY(v8_flags.interpreted_frames_native_stack)) {
    CreateInterpreterDataForDeserializedCode(isolate, result,
                                             log_code_creation);
  }

  bool needs_source_positions = isolate->NeedsSourcePositionsForProfiling();

  if (log_code_creation || v8_flags.log_function_events) {
    Handle<Script> script(Script::cast(result->script()), isolate);
    Handle<String> name(script->name().IsString()
                            ? String::cast(script->name())
                            : ReadOnlyRoots(isolate).empty_string(),
                        isolate);

    if (V8_UNLIKELY(v8_flags.log_function_events)) {
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
                      shared_info->is_toplevel()
                          ? LogEventListener::CodeTag::kScript
                          : LogEventListener::CodeTag::kFunction,
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
}

void BaselineBatchCompileIfSparkplugCompiled(Isolate* isolate, Script script) {
  // Here is main thread, we trigger early baseline compilation only in
  // concurrent sparkplug and baseline batch compilation mode which consumes
  // little main thread execution time.
  if (v8_flags.concurrent_sparkplug && v8_flags.baseline_batch_compilation) {
    SharedFunctionInfo::ScriptIterator iter(isolate, script);
    for (SharedFunctionInfo info = iter.Next(); !info.is_null();
         info = iter.Next()) {
      if (info.sparkplug_compiled() && CanCompileWithBaseline(isolate, info)) {
        isolate->baseline_batch_compiler()->EnqueueSFI(info);
      }
    }
  }
}

}  // namespace

MaybeHandle<SharedFunctionInfo> CodeSerializer::Deserialize(
    Isolate* isolate, AlignedCachedData* cached_data, Handle<String> source,
    ScriptOriginOptions origin_options,
    MaybeHandle<Script> maybe_cached_script) {
  if (v8_flags.stress_background_compile) {
    StressOffThreadDeserializeThread thread(isolate, cached_data);
    CHECK(thread.Start());
    thread.Join();
    return thread.Finalize(isolate, source, origin_options);
    // TODO(leszeks): Compare off-thread deserialized data to on-thread.
  }

  base::ElapsedTimer timer;
  if (v8_flags.profile_deserialization || v8_flags.log_function_events)
    timer.Start();

  HandleScope scope(isolate);

  SerializedCodeSanityCheckResult sanity_check_result =
      SerializedCodeSanityCheckResult::kSuccess;
  const SerializedCodeData scd = SerializedCodeData::FromCachedData(
      cached_data, SerializedCodeData::SourceHash(source, origin_options),
      &sanity_check_result);
  if (sanity_check_result != SerializedCodeSanityCheckResult::kSuccess) {
    if (v8_flags.profile_deserialization)
      PrintF("[Cached code failed check]\n");
    DCHECK(cached_data->rejected());
    isolate->counters()->code_cache_reject_reason()->AddSample(
        static_cast<int>(sanity_check_result));
    return MaybeHandle<SharedFunctionInfo>();
  }

  // Deserialize.
  MaybeHandle<SharedFunctionInfo> maybe_result =
      ObjectDeserializer::DeserializeSharedFunctionInfo(isolate, &scd, source);

  Handle<SharedFunctionInfo> result;
  if (!maybe_result.ToHandle(&result)) {
    // Deserializing may fail if the reservations cannot be fulfilled.
    if (v8_flags.profile_deserialization) PrintF("[Deserializing failed]\n");
    return MaybeHandle<SharedFunctionInfo>();
  }

  // Check whether the newly deserialized data should be merged into an
  // existing Script from the Isolate compilation cache. If so, perform
  // the merge in a single-threaded manner since this deserialization was
  // single-threaded.
  if (Handle<Script> cached_script;
      maybe_cached_script.ToHandle(&cached_script)) {
    BackgroundMergeTask merge;
    merge.SetUpOnMainThread(isolate, cached_script);
    CHECK(merge.HasPendingBackgroundWork());
    Handle<Script> new_script = handle(Script::cast(result->script()), isolate);
    merge.BeginMergeInBackground(isolate->AsLocalIsolate(), new_script);
    CHECK(merge.HasPendingForegroundWork());
    result = merge.CompleteMergeInForeground(isolate, new_script);
  }

  BaselineBatchCompileIfSparkplugCompiled(isolate,
                                          Script::cast(result->script()));
  if (v8_flags.profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int length = cached_data->length();
    PrintF("[Deserializing from %d bytes took %0.3f ms]\n", length, ms);
  }

  FinalizeDeserialization(isolate, result, timer);

  return scope.CloseAndEscape(result);
}

Handle<Script> CodeSerializer::OffThreadDeserializeData::GetOnlyScript(
    LocalHeap* heap) {
  std::unique_ptr<PersistentHandles> previous_persistent_handles =
      heap->DetachPersistentHandles();
  heap->AttachPersistentHandles(std::move(persistent_handles));

  DCHECK_EQ(scripts.size(), 1);
  // Make a non-persistent handle to return.
  Handle<Script> script = handle(*scripts[0], heap);
  DCHECK_EQ(*script, maybe_result.ToHandleChecked()->script());

  persistent_handles = heap->DetachPersistentHandles();
  if (previous_persistent_handles) {
    heap->AttachPersistentHandles(std::move(previous_persistent_handles));
  }

  return script;
}

CodeSerializer::OffThreadDeserializeData
CodeSerializer::StartDeserializeOffThread(LocalIsolate* local_isolate,
                                          AlignedCachedData* cached_data) {
  OffThreadDeserializeData result;

  DCHECK(!local_isolate->heap()->HasPersistentHandles());

  const SerializedCodeData scd =
      SerializedCodeData::FromCachedDataWithoutSource(
          cached_data, &result.sanity_check_result);
  if (result.sanity_check_result != SerializedCodeSanityCheckResult::kSuccess) {
    // Exit early but don't report yet, we'll re-check this when finishing on
    // the main thread
    DCHECK(cached_data->rejected());
    return result;
  }

  MaybeHandle<SharedFunctionInfo> local_maybe_result =
      OffThreadObjectDeserializer::DeserializeSharedFunctionInfo(
          local_isolate, &scd, &result.scripts);

  result.maybe_result =
      local_isolate->heap()->NewPersistentMaybeHandle(local_maybe_result);
  result.persistent_handles = local_isolate->heap()->DetachPersistentHandles();

  return result;
}

MaybeHandle<SharedFunctionInfo> CodeSerializer::FinishOffThreadDeserialize(
    Isolate* isolate, OffThreadDeserializeData&& data,
    AlignedCachedData* cached_data, Handle<String> source,
    ScriptOriginOptions origin_options,
    BackgroundMergeTask* background_merge_task) {
  base::ElapsedTimer timer;
  if (v8_flags.profile_deserialization || v8_flags.log_function_events)
    timer.Start();

  HandleScope scope(isolate);

  // Do a source sanity check now that we have the source. It's important for
  // FromPartiallySanityCheckedCachedData call that the sanity_check_result
  // holds the result of the off-thread sanity check.
  SerializedCodeSanityCheckResult sanity_check_result =
      data.sanity_check_result;
  const SerializedCodeData scd =
      SerializedCodeData::FromPartiallySanityCheckedCachedData(
          cached_data, SerializedCodeData::SourceHash(source, origin_options),
          &sanity_check_result);
  if (sanity_check_result != SerializedCodeSanityCheckResult::kSuccess) {
    // The only case where the deserialization result could exist despite a
    // check failure is on a source mismatch, since we can't test for this
    // off-thread.
    DCHECK_IMPLIES(!data.maybe_result.is_null(),
                   sanity_check_result ==
                       SerializedCodeSanityCheckResult::kSourceMismatch);
    // The only kind of sanity check we can't test for off-thread is a source
    // mismatch.
    DCHECK_IMPLIES(sanity_check_result != data.sanity_check_result,
                   sanity_check_result ==
                       SerializedCodeSanityCheckResult::kSourceMismatch);
    if (v8_flags.profile_deserialization)
      PrintF("[Cached code failed check]\n");
    DCHECK(cached_data->rejected());
    isolate->counters()->code_cache_reject_reason()->AddSample(
        static_cast<int>(sanity_check_result));
    return MaybeHandle<SharedFunctionInfo>();
  }

  Handle<SharedFunctionInfo> result;
  if (!data.maybe_result.ToHandle(&result)) {
    // Deserializing may fail if the reservations cannot be fulfilled.
    if (v8_flags.profile_deserialization) {
      PrintF("[Off-thread deserializing failed]\n");
    }
    return MaybeHandle<SharedFunctionInfo>();
  }

  // Change the result persistent handle into a regular handle.
  DCHECK(data.persistent_handles->Contains(result.location()));
  result = handle(*result, isolate);

  if (background_merge_task &&
      background_merge_task->HasPendingForegroundWork()) {
    Handle<Script> script = handle(Script::cast(result->script()), isolate);
    result = background_merge_task->CompleteMergeInForeground(isolate, script);
    DCHECK(Script::cast(result->script()).source().StrictEquals(*source));
    DCHECK(isolate->factory()->script_list()->Contains(
        MaybeObject::MakeWeak(MaybeObject::FromObject(result->script()))));
  } else {
    // Fix up the source on the script. This should be the only deserialized
    // script, and the off-thread deserializer should have set its source to
    // the empty string.
    DCHECK_EQ(data.scripts.size(), 1);
    DCHECK_EQ(result->script(), *data.scripts[0]);
    DCHECK_EQ(Script::cast(result->script()).source(),
              ReadOnlyRoots(isolate).empty_string());
    Script::cast(result->script()).set_source(*source);

    // Fix up the script list to include the newly deserialized script.
    Handle<WeakArrayList> list = isolate->factory()->script_list();
    for (Handle<Script> script : data.scripts) {
      BaselineBatchCompileIfSparkplugCompiled(isolate, *script);
      DCHECK(data.persistent_handles->Contains(script.location()));
      list = WeakArrayList::AddToEnd(isolate, list,
                                     MaybeObjectHandle::Weak(script));
    }
    isolate->heap()->SetRootScriptList(*list);
  }

  if (v8_flags.profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int length = cached_data->length();
    PrintF("[Finishing off-thread deserialize from %d bytes took %0.3f ms]\n",
           length, ms);
  }

  FinalizeDeserialization(isolate, result, timer);

  DCHECK(!background_merge_task ||
         !background_merge_task->HasPendingForegroundWork());

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
  uint32_t checksum =
      v8_flags.verify_snapshot_checksum ? Checksum(ChecksummedContent()) : 0;
  SetHeaderValue(kChecksumOffset, checksum);
}

SerializedCodeSanityCheckResult SerializedCodeData::SanityCheck(
    uint32_t expected_source_hash) const {
  SerializedCodeSanityCheckResult result = SanityCheckWithoutSource();
  if (result != SerializedCodeSanityCheckResult::kSuccess) return result;
  return SanityCheckJustSource(expected_source_hash);
}

SerializedCodeSanityCheckResult SerializedCodeData::SanityCheckJustSource(
    uint32_t expected_source_hash) const {
  uint32_t source_hash = GetHeaderValue(kSourceHashOffset);
  if (source_hash != expected_source_hash) {
    return SerializedCodeSanityCheckResult::kSourceMismatch;
  }
  return SerializedCodeSanityCheckResult::kSuccess;
}

SerializedCodeSanityCheckResult SerializedCodeData::SanityCheckWithoutSource()
    const {
  if (this->size_ < kHeaderSize) {
    return SerializedCodeSanityCheckResult::kInvalidHeader;
  }
  uint32_t magic_number = GetMagicNumber();
  if (magic_number != kMagicNumber) {
    return SerializedCodeSanityCheckResult::kMagicNumberMismatch;
  }
  uint32_t version_hash = GetHeaderValue(kVersionHashOffset);
  if (version_hash != Version::Hash()) {
    return SerializedCodeSanityCheckResult::kVersionMismatch;
  }
  uint32_t flags_hash = GetHeaderValue(kFlagHashOffset);
  if (flags_hash != FlagList::Hash()) {
    return SerializedCodeSanityCheckResult::kFlagsMismatch;
  }
  uint32_t payload_length = GetHeaderValue(kPayloadLengthOffset);
  uint32_t max_payload_length = this->size_ - kHeaderSize;
  if (payload_length > max_payload_length) {
    return SerializedCodeSanityCheckResult::kLengthMismatch;
  }
  if (v8_flags.verify_snapshot_checksum) {
    uint32_t checksum = GetHeaderValue(kChecksumOffset);
    if (Checksum(ChecksummedContent()) != checksum) {
      return SerializedCodeSanityCheckResult::kChecksumMismatch;
    }
  }
  return SerializedCodeSanityCheckResult::kSuccess;
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
AlignedCachedData* SerializedCodeData::GetScriptData() {
  DCHECK(owns_data_);
  AlignedCachedData* result = new AlignedCachedData(data_, size_);
  result->AcquireDataOwnership();
  owns_data_ = false;
  data_ = nullptr;
  return result;
}

base::Vector<const byte> SerializedCodeData::Payload() const {
  const byte* payload = data_ + kHeaderSize;
  DCHECK(IsAligned(reinterpret_cast<intptr_t>(payload), kPointerAlignment));
  int length = GetHeaderValue(kPayloadLengthOffset);
  DCHECK_EQ(data_ + size_, payload + length);
  return base::Vector<const byte>(payload, length);
}

SerializedCodeData::SerializedCodeData(AlignedCachedData* data)
    : SerializedData(const_cast<byte*>(data->data()), data->length()) {}

SerializedCodeData SerializedCodeData::FromCachedData(
    AlignedCachedData* cached_data, uint32_t expected_source_hash,
    SerializedCodeSanityCheckResult* rejection_result) {
  DisallowGarbageCollection no_gc;
  SerializedCodeData scd(cached_data);
  *rejection_result = scd.SanityCheck(expected_source_hash);
  if (*rejection_result != SerializedCodeSanityCheckResult::kSuccess) {
    cached_data->Reject();
    return SerializedCodeData(nullptr, 0);
  }
  return scd;
}

SerializedCodeData SerializedCodeData::FromCachedDataWithoutSource(
    AlignedCachedData* cached_data,
    SerializedCodeSanityCheckResult* rejection_result) {
  DisallowGarbageCollection no_gc;
  SerializedCodeData scd(cached_data);
  *rejection_result = scd.SanityCheckWithoutSource();
  if (*rejection_result != SerializedCodeSanityCheckResult::kSuccess) {
    cached_data->Reject();
    return SerializedCodeData(nullptr, 0);
  }
  return scd;
}

SerializedCodeData SerializedCodeData::FromPartiallySanityCheckedCachedData(
    AlignedCachedData* cached_data, uint32_t expected_source_hash,
    SerializedCodeSanityCheckResult* rejection_result) {
  DisallowGarbageCollection no_gc;
  // The previous call to FromCachedDataWithoutSource may have already rejected
  // the cached data, so re-use the previous rejection result if it's not a
  // success.
  if (*rejection_result != SerializedCodeSanityCheckResult::kSuccess) {
    // FromCachedDataWithoutSource doesn't check the source, so there can't be
    // a source mismatch.
    DCHECK_NE(*rejection_result,
              SerializedCodeSanityCheckResult::kSourceMismatch);
    cached_data->Reject();
    return SerializedCodeData(nullptr, 0);
  }
  SerializedCodeData scd(cached_data);
  *rejection_result = scd.SanityCheckJustSource(expected_source_hash);
  if (*rejection_result != SerializedCodeSanityCheckResult::kSuccess) {
    // This check only checks the source, so the only possible failure is a
    // source mismatch.
    DCHECK_EQ(*rejection_result,
              SerializedCodeSanityCheckResult::kSourceMismatch);
    cached_data->Reject();
    return SerializedCodeData(nullptr, 0);
  }
  return scd;
}

}  // namespace internal
}  // namespace v8
