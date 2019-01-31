// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/startup-serializer.h"

#include "src/api.h"
#include "src/code-tracer.h"
#include "src/contexts.h"
#include "src/global-handles.h"
#include "src/objects-inl.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/slots.h"
#include "src/snapshot/read-only-serializer.h"
#include "src/v8threads.h"

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

namespace {

// Due to how we currently create the embedded blob, we may encounter both
// off-heap trampolines and old, outdated full Code objects during
// serialization. This ensures that we only serialize the canonical version of
// each builtin.
// See also CreateOffHeapTrampolines().
HeapObject MaybeCanonicalizeBuiltin(Isolate* isolate, HeapObject obj) {
  if (!obj->IsCode()) return obj;

  const int builtin_index = Code::cast(obj)->builtin_index();
  if (!Builtins::IsBuiltinId(builtin_index)) return obj;

  return isolate->builtins()->builtin(builtin_index);
}

}  // namespace

void StartupSerializer::SerializeObject(HeapObject obj, HowToCode how_to_code,
                                        WhereToPoint where_to_point, int skip) {
  DCHECK(!obj->IsJSFunction());

  // TODO(jgruber): Remove canonicalization once off-heap trampoline creation
  // moves to Isolate::Init().
  obj = MaybeCanonicalizeBuiltin(isolate(), obj);

  if (SerializeHotObject(obj, how_to_code, where_to_point, skip)) return;
  if (IsRootAndHasBeenSerialized(obj) &&
      SerializeRoot(obj, how_to_code, where_to_point, skip))
    return;
  if (SerializeUsingReadOnlyObjectCache(&sink_, obj, how_to_code,
                                        where_to_point, skip))
    return;
  if (SerializeBackReference(obj, how_to_code, where_to_point, skip)) return;

  FlushSkip(skip);
  bool use_simulator = false;
#ifdef USE_SIMULATOR
  use_simulator = true;
#endif

  if (use_simulator && obj->IsAccessorInfo()) {
    // Wipe external reference redirects in the accessor info.
    AccessorInfo info = AccessorInfo::cast(obj);
    Address original_address = Foreign::cast(info->getter())->foreign_address();
    Foreign::cast(info->js_getter())->set_foreign_address(original_address);
    accessor_infos_.push_back(info);
  } else if (use_simulator && obj->IsCallHandlerInfo()) {
    CallHandlerInfo info = CallHandlerInfo::cast(obj);
    Address original_address =
        Foreign::cast(info->callback())->foreign_address();
    Foreign::cast(info->js_callback())->set_foreign_address(original_address);
    call_handler_infos_.push_back(info);
  } else if (obj->IsScript() && Script::cast(obj)->IsUserJavaScript()) {
    Script::cast(obj)->set_context_data(
        ReadOnlyRoots(isolate()).uninitialized_symbol());
  } else if (obj->IsSharedFunctionInfo()) {
    // Clear inferred name for native functions.
    SharedFunctionInfo shared = SharedFunctionInfo::cast(obj);
    if (!shared->IsSubjectToDebugging() && shared->HasUncompiledData()) {
      shared->uncompiled_data()->set_inferred_name(
          ReadOnlyRoots(isolate()).empty_string());
    }
  }

  CheckRehashability(obj);

  // Object has not yet been serialized.  Serialize it here.
  DCHECK(!isolate()->heap()->read_only_space()->Contains(obj));
  ObjectSerializer object_serializer(this, obj, &sink_, how_to_code,
                                     where_to_point);
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
    AddToSet(context->serialized_objects());
  }
}

bool StartupSerializer::SerializeUsingReadOnlyObjectCache(
    SnapshotByteSink* sink, HeapObject obj, HowToCode how_to_code,
    WhereToPoint where_to_point, int skip) {
  return read_only_serializer_->SerializeUsingReadOnlyObjectCache(
      sink, obj, how_to_code, where_to_point, skip);
}

void StartupSerializer::SerializeUsingPartialSnapshotCache(
    SnapshotByteSink* sink, HeapObject obj, HowToCode how_to_code,
    WhereToPoint where_to_point, int skip) {
  FlushSkip(sink, skip);

  int cache_index = SerializeInObjectCache(obj);
  sink->Put(kPartialSnapshotCache + how_to_code + where_to_point,
            "PartialSnapshotCache");
  sink->PutInt(cache_index, "partial_snapshot_cache_index");
}

void SerializedHandleChecker::AddToSet(FixedArray serialized) {
  int length = serialized->length();
  for (int i = 0; i < length; i++) serialized_.insert(serialized->get(i));
}

void SerializedHandleChecker::VisitRootPointers(Root root,
                                                const char* description,
                                                FullObjectSlot start,
                                                FullObjectSlot end) {
  for (FullObjectSlot p = start; p < end; ++p) {
    if (serialized_.find(*p) != serialized_.end()) continue;
    PrintF("%s handle not serialized: ",
           root == Root::kGlobalHandles ? "global" : "eternal");
    (*p)->Print();
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
