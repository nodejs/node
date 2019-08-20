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

StartupSerializer::StartupSerializer(Isolate* isolate,
                                     ReadOnlySerializer* read_only_serializer)
    : RootsSerializer(isolate, RootIndex::kFirstStrongRoot),
      read_only_serializer_(read_only_serializer) {
  InitializeCodeAddressMap();
}

StartupSerializer::~StartupSerializer() {
  RestoreExternalReferenceRedirectors(accessor_infos_);
  RestoreExternalReferenceRedirectors(call_handler_infos_);
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
  if (!FLAG_embedded_builtins) return false;
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
    Address original_address = Foreign::cast(info.getter()).foreign_address();
    Foreign::cast(info.js_getter()).set_foreign_address(original_address);
    accessor_infos_.push_back(info);
  } else if (use_simulator && obj.IsCallHandlerInfo()) {
    CallHandlerInfo info = CallHandlerInfo::cast(obj);
    Address original_address = Foreign::cast(info.callback()).foreign_address();
    Foreign::cast(info.js_callback()).set_foreign_address(original_address);
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
  // This comes right after serialization of the partial snapshot, where we
  // add entries to the partial snapshot cache of the startup snapshot. Add
  // one entry with 'undefined' to terminate the partial snapshot cache.
  Object undefined = ReadOnlyRoots(isolate()).undefined_value();
  VisitRootPointer(Root::kPartialSnapshotCache, nullptr,
                   FullObjectSlot(&undefined));
  isolate()->heap()->IterateWeakRoots(this, VISIT_FOR_SERIALIZATION);
  SerializeDeferredObjects();
  Pad();
}

void StartupSerializer::SerializeStrongReferences() {
  Isolate* isolate = this->isolate();
  // No active threads.
  CHECK_NULL(isolate->thread_manager()->FirstThreadStateInUse());
  // No active or weak handles.
  CHECK(isolate->handle_scope_implementer()->blocks()->empty());

  // Visit smi roots.
  // Clear the stack limits to make the snapshot reproducible.
  // Reset it again afterwards.
  isolate->heap()->ClearStackLimits();
  isolate->heap()->IterateSmiRoots(this);
  isolate->heap()->SetStackLimits();
  // First visit immortal immovables to make sure they end up in the first page.
  isolate->heap()->IterateStrongRoots(this, VISIT_FOR_SERIALIZATION);
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

void StartupSerializer::SerializeUsingPartialSnapshotCache(
    SnapshotByteSink* sink, HeapObject obj) {
  int cache_index = SerializeInObjectCache(obj);
  sink->Put(kPartialSnapshotCache, "PartialSnapshotCache");
  sink->PutInt(cache_index, "partial_snapshot_cache_index");
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
