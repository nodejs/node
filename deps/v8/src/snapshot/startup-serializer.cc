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
class V8_NODISCARD SanitizeIsolateScope final {
 public:
  SanitizeIsolateScope(Isolate* isolate, bool allow_active_isolate_for_testing,
                       const DisallowGarbageCollection& no_gc)
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
      read_only_serializer_(read_only_serializer),
      accessor_infos_(isolate->heap()),
      call_handler_infos_(isolate->heap()) {
  InitializeCodeAddressMap();
}

StartupSerializer::~StartupSerializer() {
  for (Handle<AccessorInfo> info : accessor_infos_) {
    RestoreExternalReferenceRedirector(isolate(), info);
  }
  for (Handle<CallHandlerInfo> info : call_handler_infos_) {
    RestoreExternalReferenceRedirector(isolate(), info);
  }
  OutputStatistics("StartupSerializer");
}

#ifdef DEBUG
namespace {

bool IsUnexpectedCodeObject(Isolate* isolate, HeapObject obj) {
  if (!obj.IsCode()) return false;

  Code code = Code::cast(obj);
  if (code.kind() == CodeKind::REGEXP) return false;
  if (!code.is_builtin()) return true;
  if (code.is_off_heap_trampoline()) return false;

  // An on-heap builtin. We only expect this for the interpreter entry
  // trampoline copy stored on the root list and transitively called builtins.
  // See Heap::interpreter_entry_trampoline_for_profiling.

  switch (code.builtin_id()) {
    case Builtin::kAbort:
    case Builtin::kCEntry_Return1_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit:
    case Builtin::kInterpreterEntryTrampoline:
    case Builtin::kRecordWriteEmitRememberedSetSaveFP:
    case Builtin::kRecordWriteOmitRememberedSetSaveFP:
    case Builtin::kRecordWriteEmitRememberedSetIgnoreFP:
    case Builtin::kRecordWriteOmitRememberedSetIgnoreFP:
#ifdef V8_IS_TSAN
    case Builtin::kTSANRelaxedStore8IgnoreFP:
    case Builtin::kTSANRelaxedStore8SaveFP:
    case Builtin::kTSANRelaxedStore16IgnoreFP:
    case Builtin::kTSANRelaxedStore16SaveFP:
    case Builtin::kTSANRelaxedStore32IgnoreFP:
    case Builtin::kTSANRelaxedStore32SaveFP:
    case Builtin::kTSANRelaxedStore64IgnoreFP:
    case Builtin::kTSANRelaxedStore64SaveFP:
    case Builtin::kTSANRelaxedLoad32IgnoreFP:
    case Builtin::kTSANRelaxedLoad32SaveFP:
    case Builtin::kTSANRelaxedLoad64IgnoreFP:
    case Builtin::kTSANRelaxedLoad64SaveFP:
#endif  // V8_IS_TSAN
      return false;
    default:
      return true;
  }

  UNREACHABLE();
}

}  // namespace
#endif  // DEBUG

void StartupSerializer::SerializeObjectImpl(Handle<HeapObject> obj) {
#ifdef DEBUG
  if (obj->IsJSFunction()) {
    v8::base::OS::PrintError("Reference stack:\n");
    PrintStack(std::cerr);
    obj->Print(std::cerr);
    FATAL(
        "JSFunction should be added through the context snapshot instead of "
        "the isolate snapshot");
  }
#endif  // DEBUG
  DCHECK(!IsUnexpectedCodeObject(isolate(), *obj));

  if (SerializeHotObject(obj)) return;
  if (IsRootAndHasBeenSerialized(*obj) && SerializeRoot(obj)) return;
  if (SerializeUsingReadOnlyObjectCache(&sink_, obj)) return;
  if (SerializeBackReference(obj)) return;

  bool use_simulator = false;
#ifdef USE_SIMULATOR
  use_simulator = true;
#endif

  if (use_simulator && obj->IsAccessorInfo()) {
    // Wipe external reference redirects in the accessor info.
    Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(obj);
    Address original_address =
        Foreign::cast(info->getter()).foreign_address(isolate());
    Foreign::cast(info->js_getter())
        .set_foreign_address(isolate(), original_address);
    accessor_infos_.Push(*info);
  } else if (use_simulator && obj->IsCallHandlerInfo()) {
    Handle<CallHandlerInfo> info = Handle<CallHandlerInfo>::cast(obj);
    Address original_address =
        Foreign::cast(info->callback()).foreign_address(isolate());
    Foreign::cast(info->js_callback())
        .set_foreign_address(isolate(), original_address);
    call_handler_infos_.Push(*info);
  } else if (obj->IsScript() && Handle<Script>::cast(obj)->IsUserJavaScript()) {
    Handle<Script>::cast(obj)->set_context_data(
        ReadOnlyRoots(isolate()).uninitialized_symbol());
  } else if (obj->IsSharedFunctionInfo()) {
    // Clear inferred name for native functions.
    Handle<SharedFunctionInfo> shared = Handle<SharedFunctionInfo>::cast(obj);
    if (!shared->IsSubjectToDebugging() && shared->HasUncompiledData()) {
      shared->uncompiled_data().set_inferred_name(
          ReadOnlyRoots(isolate()).empty_string());
    }
  }

  CheckRehashability(*obj);

  // Object has not yet been serialized.  Serialize it here.
  DCHECK(!ReadOnlyHeap::Contains(*obj));
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

  SerializeStringTable(isolate()->string_table());

  isolate()->heap()->IterateWeakRoots(
      this, base::EnumSet<SkipRoot>{SkipRoot::kUnserializable});
  SerializeDeferredObjects();
  Pad();
}

void StartupSerializer::SerializeStringTable(StringTable* string_table) {
  // A StringTable is serialized as:
  //
  //   N : int
  //   string 1
  //   string 2
  //   ...
  //   string N
  //
  // Notably, the hashmap structure, including empty and deleted elements, is
  // not serialized.

  sink_.PutInt(isolate()->string_table()->NumberOfElements(),
               "String table number of elements");

  // Custom RootVisitor which walks the string table, but only serializes the
  // string entries. This is an inline class to be able to access the non-public
  // SerializeObject method.
  class StartupSerializerStringTableVisitor : public RootVisitor {
   public:
    explicit StartupSerializerStringTableVisitor(StartupSerializer* serializer)
        : serializer_(serializer) {}

    void VisitRootPointers(Root root, const char* description,
                           FullObjectSlot start, FullObjectSlot end) override {
      UNREACHABLE();
    }

    void VisitRootPointers(Root root, const char* description,
                           OffHeapObjectSlot start,
                           OffHeapObjectSlot end) override {
      DCHECK_EQ(root, Root::kStringTable);
      Isolate* isolate = serializer_->isolate();
      for (OffHeapObjectSlot current = start; current < end; ++current) {
        Object obj = current.load(isolate);
        if (obj.IsHeapObject()) {
          DCHECK(obj.IsInternalizedString());
          serializer_->SerializeObject(handle(HeapObject::cast(obj), isolate));
        }
      }
    }

   private:
    StartupSerializer* serializer_;
  };

  StartupSerializerStringTableVisitor string_table_visitor(this);
  isolate()->string_table()->IterateElements(&string_table_visitor);
}

void StartupSerializer::SerializeStrongReferences(
    const DisallowGarbageCollection& no_gc) {
  Isolate* isolate = this->isolate();
  // No active threads.
  CHECK_NULL(isolate->thread_manager()->FirstThreadStateInUse());

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
    SnapshotByteSink* sink, Handle<HeapObject> obj) {
  return read_only_serializer_->SerializeUsingReadOnlyObjectCache(sink, obj);
}

void StartupSerializer::SerializeUsingStartupObjectCache(
    SnapshotByteSink* sink, Handle<HeapObject> obj) {
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
