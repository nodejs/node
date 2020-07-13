// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/startup-serializer.h"

#include "src/api/api.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/v8threads.h"
#include "src/handles/global-handles.h"
#include "src/heap/heap-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/objects/contexts.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/slots.h"
#include "src/snapshot/read-only-serializer.h"

namespace v8 {
namespace internal {

namespace {

// The isolate roots may not point at context-specific objects during
// serialization.
class SanitizeIsolateScope final {
 public:
  SanitizeIsolateScope(Isolate* isolate, bool allow_active_isolate_for_testing,
                       const DisallowHeapAllocation& no_gc)
      : isolate_(isolate),
        feedback_vectors_for_profiling_tools_(
            isolate->heap()->feedback_vectors_for_profiling_tools()),
        detached_contexts_(isolate->heap()->detached_contexts()) {
#ifdef DEBUG
    if (!allow_active_isolate_for_testing) {
      // These should already be empty when creating a real snapshot.
      DCHECK_EQ(feedback_vectors_for_profiling_tools_,
                ReadOnlyRoots(isolate).undefined_value());
      DCHECK_EQ(detached_contexts_,
                ReadOnlyRoots(isolate).empty_weak_array_list());
    }
#endif

    isolate->SetFeedbackVectorsForProfilingTools(
        ReadOnlyRoots(isolate).undefined_value());
    isolate->heap()->SetDetachedContexts(
        ReadOnlyRoots(isolate).empty_weak_array_list());
  }

  ~SanitizeIsolateScope() {
    // Restore saved fields.
    isolate_->SetFeedbackVectorsForProfilingTools(
        feedback_vectors_for_profiling_tools_);
    isolate_->heap()->SetDetachedContexts(detached_contexts_);
  }

 private:
  Isolate* isolate_;
  const Object feedback_vectors_for_profiling_tools_;
  const WeakArrayList detached_contexts_;
};

}  // namespace

StartupSerializer::StartupSerializer(Isolate* isolate,
                                     Snapshot::SerializerFlags flags,
                                     ReadOnlySerializer* read_only_serializer)
    : RootsSerializer(isolate, flags, RootIndex::kFirstStrongRoot),
      read_only_serializer_(read_only_serializer) {
  allocator()->UseCustomChunkSize(FLAG_serialization_chunk_size);
  InitializeCodeAddressMap();
}

StartupSerializer::~StartupSerializer() {
  RestoreExternalReferenceRedirectors(isolate(), accessor_infos_);
  RestoreExternalReferenceRedirectors(isolate(), call_handler_infos_);
  OutputStatistics("StartupSerializer");
}

#ifdef DEBUG
namespace {

bool IsUnexpectedCodeObject(Isolate* isolate, HeapObject obj) {
  if (!obj.IsCode()) return false;

  Code code = Code::cast(obj);

  // TODO(v8:8768): Deopt entry code should not be serialized.
  if (code.kind() == Code::STUB && isolate->deoptimizer_data() != nullptr) {
    if (isolate->deoptimizer_data()->IsDeoptEntryCode(code)) return false;
  }

  if (code.kind() == Code::REGEXP) return false;
  if (!code.is_builtin()) return true;
  if (code.is_off_heap_trampoline()) return false;

  // An on-heap builtin. We only expect this for the interpreter entry
  // trampoline copy stored on the root list and transitively called builtins.
  // See Heap::interpreter_entry_trampoline_for_profiling.

  switch (code.builtin_index()) {
    case Builtins::kAbort:
    case Builtins::kCEntry_Return1_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit:
    case Builtins::kInterpreterEntryTrampoline:
    case Builtins::kRecordWrite:
      return false;
    default:
      return true;
  }

  UNREACHABLE();
}

}  // namespace
#endif  // DEBUG

void StartupSerializer::SerializeObject(HeapObject obj) {
#ifdef DEBUG
  if (obj.IsJSFunction()) {
    v8::base::OS::PrintError("Reference stack:\n");
    PrintStack(std::cerr);
    obj.Print(std::cerr);
    FATAL(
        "JSFunction should be added through the context snapshot instead of "
        "the isolate snapshot");
  }
#endif  // DEBUG
  DCHECK(!IsUnexpectedCodeObject(isolate(), obj));

  if (SerializeHotObject(obj)) return;
  if (IsRootAndHasBeenSerialized(obj) && SerializeRoot(obj)) return;
  if (SerializeUsingReadOnlyObjectCache(&sink_, obj)) return;
  if (SerializeBackReference(obj)) return;

  bool use_simulator = false;
#ifdef USE_SIMULATOR
  use_simulator = true;
#endif

  if (use_simulator && obj.IsAccessorInfo()) {
    // Wipe external reference redirects in the accessor info.
    AccessorInfo info = AccessorInfo::cast(obj);
    Address original_address =
        Foreign::cast(info.getter()).foreign_address(isolate());
    Foreign::cast(info.js_getter())
        .set_foreign_address(isolate(), original_address);
    accessor_infos_.push_back(info);
  } else if (use_simulator && obj.IsCallHandlerInfo()) {
    CallHandlerInfo info = CallHandlerInfo::cast(obj);
    Address original_address =
        Foreign::cast(info.callback()).foreign_address(isolate());
    Foreign::cast(info.js_callback())
        .set_foreign_address(isolate(), original_address);
    call_handler_infos_.push_back(info);
  } else if (obj.IsScript() && Script::cast(obj).IsUserJavaScript()) {
    Script::cast(obj).set_context_data(
        ReadOnlyRoots(isolate()).uninitialized_symbol());
  } else if (obj.IsSharedFunctionInfo()) {
    // Clear inferred name for native functions.
    SharedFunctionInfo shared = SharedFunctionInfo::cast(obj);
    if (!shared.IsSubjectToDebugging() && shared.HasUncompiledData()) {
      shared.uncompiled_data().set_inferred_name(
          ReadOnlyRoots(isolate()).empty_string());
    }
  }

  CheckRehashability(obj);

  // Object has not yet been serialized.  Serialize it here.
  DCHECK(!ReadOnlyHeap::Contains(obj));
  ObjectSerializer object_serializer(this, obj, &sink_);
  object_serializer.Serialize();
}

void StartupSerializer::SerializeWeakReferencesAndDeferred() {
  // This comes right after serialization of the context snapshot, where we
  // add entries to the startup object cache of the startup snapshot. Add
  // one entry with 'undefined' to terminate the startup object cache.
  Object undefined = ReadOnlyRoots(isolate()).undefined_value();
  VisitRootPointer(Root::kStartupObjectCache, nullptr,
                   FullObjectSlot(&undefined));
  isolate()->heap()->IterateWeakRoots(
      this, base::EnumSet<SkipRoot>{SkipRoot::kUnserializable});
  SerializeDeferredObjects();
  Pad();
}

void StartupSerializer::SerializeStrongReferences(
    const DisallowHeapAllocation& no_gc) {
  Isolate* isolate = this->isolate();
  // No active threads.
  CHECK_NULL(isolate->thread_manager()->FirstThreadStateInUse());
  // No active or weak handles.
  CHECK_IMPLIES(!allow_active_isolate_for_testing(),
                isolate->handle_scope_implementer()->blocks()->empty());

  SanitizeIsolateScope sanitize_isolate(
      isolate, allow_active_isolate_for_testing(), no_gc);

  // Visit smi roots and immortal immovables first to make sure they end up in
  // the first page.
  isolate->heap()->IterateSmiRoots(this);
  isolate->heap()->IterateRoots(
      this,
      base::EnumSet<SkipRoot>{SkipRoot::kUnserializable, SkipRoot::kWeak});
}

SerializedHandleChecker::SerializedHandleChecker(Isolate* isolate,
                                                 std::vector<Context>* contexts)
    : isolate_(isolate) {
  AddToSet(isolate->heap()->serialized_objects());
  for (auto const& context : *contexts) {
    AddToSet(context.serialized_objects());
  }
}

bool StartupSerializer::SerializeUsingReadOnlyObjectCache(
    SnapshotByteSink* sink, HeapObject obj) {
  return read_only_serializer_->SerializeUsingReadOnlyObjectCache(sink, obj);
}

void StartupSerializer::SerializeUsingStartupObjectCache(SnapshotByteSink* sink,
                                                         HeapObject obj) {
  int cache_index = SerializeInObjectCache(obj);
  sink->Put(kStartupObjectCache, "StartupObjectCache");
  sink->PutInt(cache_index, "startup_object_cache_index");
}

void StartupSerializer::CheckNoDirtyFinalizationRegistries() {
  Isolate* isolate = this->isolate();
  CHECK(isolate->heap()->dirty_js_finalization_registries_list().IsUndefined(
      isolate));
  CHECK(
      isolate->heap()->dirty_js_finalization_registries_list_tail().IsUndefined(
          isolate));
}

void SerializedHandleChecker::AddToSet(FixedArray serialized) {
  int length = serialized.length();
  for (int i = 0; i < length; i++) serialized_.insert(serialized.get(i));
}

void SerializedHandleChecker::VisitRootPointers(Root root,
                                                const char* description,
                                                FullObjectSlot start,
                                                FullObjectSlot end) {
  for (FullObjectSlot p = start; p < end; ++p) {
    if (serialized_.find(*p) != serialized_.end()) continue;
    PrintF("%s handle not serialized: ",
           root == Root::kGlobalHandles ? "global" : "eternal");
    (*p).Print();
    PrintF("\n");
    ok_ = false;
  }
}

bool SerializedHandleChecker::CheckGlobalAndEternalHandles() {
  isolate_->global_handles()->IterateAllRoots(this);
  isolate_->eternal_handles()->IterateAllRoots(this);
  return ok_;
}

}  // namespace internal
}  // namespace v8
