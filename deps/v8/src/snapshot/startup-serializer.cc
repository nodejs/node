// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/startup-serializer.h"

#include "src/api.h"
#include "src/code-tracer.h"
#include "src/global-handles.h"
#include "src/objects-inl.h"
#include "src/v8threads.h"

namespace v8 {
namespace internal {

StartupSerializer::StartupSerializer(Isolate* isolate)
    : Serializer(isolate), can_be_rehashed_(true) {
  InitializeCodeAddressMap();
}

StartupSerializer::~StartupSerializer() {
  RestoreExternalReferenceRedirectors(accessor_infos_);
  RestoreExternalReferenceRedirectors(call_handler_infos_);
  OutputStatistics("StartupSerializer");
}

void StartupSerializer::SerializeObject(HeapObject* obj, HowToCode how_to_code,
                                        WhereToPoint where_to_point, int skip) {
  DCHECK(!ObjectIsBytecodeHandler(obj));  // Only referenced in dispatch table.
  DCHECK(!obj->IsJSFunction());

  if (SerializeBuiltinReference(obj, how_to_code, where_to_point, skip)) {
    return;
  }
  if (SerializeHotObject(obj, how_to_code, where_to_point, skip)) return;

  RootIndex root_index;
  // We can only encode roots as such if it has already been serialized.
  // That applies to root indices below the wave front.
  if (root_index_map()->Lookup(obj, &root_index)) {
    if (root_has_been_serialized(root_index)) {
      PutRoot(root_index, obj, how_to_code, where_to_point, skip);
      return;
    }
  }

  if (SerializeBackReference(obj, how_to_code, where_to_point, skip)) return;

  FlushSkip(skip);
  bool use_simulator = false;
#ifdef USE_SIMULATOR
  use_simulator = true;
#endif

  if (use_simulator && obj->IsAccessorInfo()) {
    // Wipe external reference redirects in the accessor info.
    AccessorInfo* info = AccessorInfo::cast(obj);
    Address original_address = Foreign::cast(info->getter())->foreign_address();
    Foreign::cast(info->js_getter())->set_foreign_address(original_address);
    accessor_infos_.push_back(info);
  } else if (use_simulator && obj->IsCallHandlerInfo()) {
    CallHandlerInfo* info = CallHandlerInfo::cast(obj);
    Address original_address =
        Foreign::cast(info->callback())->foreign_address();
    Foreign::cast(info->js_callback())->set_foreign_address(original_address);
    call_handler_infos_.push_back(info);
  } else if (obj->IsScript() && Script::cast(obj)->IsUserJavaScript()) {
    Script::cast(obj)->set_context_data(
        ReadOnlyRoots(isolate()).uninitialized_symbol());
  } else if (obj->IsSharedFunctionInfo()) {
    // Clear inferred name for native functions.
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(obj);
    if (!shared->IsSubjectToDebugging() && shared->HasUncompiledData()) {
      shared->uncompiled_data()->set_inferred_name(
          ReadOnlyRoots(isolate()).empty_string());
    }
  }

  CheckRehashability(obj);

  // Object has not yet been serialized.  Serialize it here.
  ObjectSerializer object_serializer(this, obj, &sink_, how_to_code,
                                     where_to_point);
  object_serializer.Serialize();
}

void StartupSerializer::SerializeWeakReferencesAndDeferred() {
  // This comes right after serialization of the partial snapshot, where we
  // add entries to the partial snapshot cache of the startup snapshot. Add
  // one entry with 'undefined' to terminate the partial snapshot cache.
  Object* undefined = ReadOnlyRoots(isolate()).undefined_value();
  VisitRootPointer(Root::kPartialSnapshotCache, nullptr, &undefined);
  isolate()->heap()->IterateWeakRoots(this, VISIT_FOR_SERIALIZATION);
  SerializeDeferredObjects();
  Pad();
}

int StartupSerializer::PartialSnapshotCacheIndex(HeapObject* heap_object) {
  int index;
  if (!partial_cache_index_map_.LookupOrInsert(heap_object, &index)) {
    // This object is not part of the partial snapshot cache yet. Add it to the
    // startup snapshot so we can refer to it via partial snapshot index from
    // the partial snapshot.
    VisitRootPointer(Root::kPartialSnapshotCache, nullptr,
                     reinterpret_cast<Object**>(&heap_object));
  }
  return index;
}

void StartupSerializer::Synchronize(VisitorSynchronization::SyncTag tag) {
  sink_.Put(kSynchronize, "Synchronize");
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

void StartupSerializer::VisitRootPointers(Root root, const char* description,
                                          Object** start, Object** end) {
  if (start == isolate()->heap()->roots_array_start()) {
    // Serializing the root list needs special handling:
    // - Only root list elements that have been fully serialized can be
    //   referenced using kRootArray bytecodes.
    for (Object** current = start; current < end; current++) {
      SerializeRootObject(*current);
      size_t root_index = static_cast<size_t>(current - start);
      root_has_been_serialized_.set(root_index);
    }
  } else {
    Serializer::VisitRootPointers(root, description, start, end);
  }
}

void StartupSerializer::CheckRehashability(HeapObject* obj) {
  if (!can_be_rehashed_) return;
  if (!obj->NeedsRehashing()) return;
  if (obj->CanBeRehashed()) return;
  can_be_rehashed_ = false;
}

bool StartupSerializer::MustBeDeferred(HeapObject* object) {
  if (root_has_been_serialized(RootIndex::kFreeSpaceMap) &&
      root_has_been_serialized(RootIndex::kOnePointerFillerMap) &&
      root_has_been_serialized(RootIndex::kTwoPointerFillerMap)) {
    // All required root objects are serialized, so any aligned objects can
    // be saved without problems.
    return false;
  }
  // Just defer everything except of Map objects until all required roots are
  // serialized. Some objects may have special alignment requirements, that may
  // not be fulfilled during deserialization until few first root objects are
  // serialized. But we must serialize Map objects since deserializer checks
  // that these root objects are indeed Maps.
  return !object->IsMap();
}

SerializedHandleChecker::SerializedHandleChecker(
    Isolate* isolate, std::vector<Context*>* contexts)
    : isolate_(isolate) {
  AddToSet(isolate->heap()->serialized_objects());
  for (auto const& context : *contexts) {
    AddToSet(context->serialized_objects());
  }
}

void SerializedHandleChecker::AddToSet(FixedArray* serialized) {
  int length = serialized->length();
  for (int i = 0; i < length; i++) serialized_.insert(serialized->get(i));
}

void SerializedHandleChecker::VisitRootPointers(Root root,
                                                const char* description,
                                                Object** start, Object** end) {
  for (Object** p = start; p < end; p++) {
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
