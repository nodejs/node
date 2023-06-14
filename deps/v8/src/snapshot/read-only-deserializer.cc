// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/read-only-deserializer.h"

#include "src/api/api.h"
#include "src/common/globals.h"
#include "src/execution/v8threads.h"
#include "src/heap/heap-inl.h"  // crbug.com/v8/8499
#include "src/heap/read-only-heap.h"
#include "src/logging/counters-scopes.h"
#include "src/objects/slots.h"
#include "src/roots/static-roots.h"
#include "src/snapshot/embedded/embedded-data-inl.h"
#include "src/snapshot/snapshot-data.h"

namespace v8 {
namespace internal {

class ReadOnlyHeapImageDeserializer {
 public:
  using Bytecode = HeapImageSerializer::Bytecode;

  static void Deserialize(SnapshotByteSource& in, Isolate* isolate) {
    auto cage = isolate->GetPtrComprCage();

    while (true) {
      switch (in.Get()) {
        case Bytecode::kReadOnlyPage: {
          Address pos = cage->base() + in.GetInt();
          isolate->read_only_heap()->read_only_space()->AllocateNextPageAt(pos);
          break;
        }
        case Bytecode::kReadOnlySegment: {
          ReadOnlySpace* ro_space =
              isolate->read_only_heap()->read_only_space();
          ReadOnlyPage* cur_page = ro_space->pages().back();
          Address start = cur_page->area_start() + in.GetInt();
          int size = in.GetInt();
          CHECK_LE(start + size, cur_page->area_end());
          in.CopyRaw(reinterpret_cast<void*>(start), size);
          ro_space->top_ = start + size;
          break;
        }
        case Bytecode::kFinalizeReadOnlyPage: {
          isolate->read_only_heap()
              ->read_only_space()
              ->FinalizeExternallyInitializedPage();
          break;
        }
        case Bytecode::kSynchronize:
          return;
        default:
          UNREACHABLE();
      }
    }
  }
};

ReadOnlyDeserializer::ReadOnlyDeserializer(Isolate* isolate,
                                           const SnapshotData* data,
                                           bool can_rehash)
    : Deserializer(isolate, data->Payload(), data->GetMagicNumber(), false,
                   can_rehash) {}

void ReadOnlyDeserializer::DeserializeIntoIsolate() {
  NestedTimedHistogramScope histogram_timer(
      isolate()->counters()->snapshot_deserialize_rospace());
  HandleScope scope(isolate());

  ReadOnlyHeap* ro_heap = isolate()->read_only_heap();

  // No active threads.
  DCHECK_NULL(isolate()->thread_manager()->FirstThreadStateInUse());
  // No active handles.
  DCHECK(isolate()->handle_scope_implementer()->blocks()->empty());
  // Read-only object cache is not yet populated.
  DCHECK(!ro_heap->read_only_object_cache_is_initialized());
  // Startup object cache is not yet populated.
  DCHECK(isolate()->startup_object_cache()->empty());
  // Builtins are not yet created.
  DCHECK(!isolate()->builtins()->is_initialized());

  {
    ReadOnlyRoots roots(isolate());
    if (V8_STATIC_ROOTS_BOOL) {
      ReadOnlyHeapImageDeserializer::Deserialize(*source(), isolate());
      roots.InitFromStaticRootsTable(isolate()->cage_base());
      ro_heap->read_only_space()->RepairFreeSpacesAfterDeserialization();
    } else {
      roots.Iterate(this);

      // Deserialize the Read-only Object Cache.
      for (;;) {
        Object* object = ro_heap->ExtendReadOnlyObjectCache();
        // During deserialization, the visitor populates the read-only object
        // cache and eventually terminates the cache with undefined.
        VisitRootPointer(Root::kReadOnlyObjectCache, nullptr,
                         FullObjectSlot(object));
        if (object->IsUndefined(roots)) break;
      }
      DeserializeDeferredObjects();
    }

#ifdef DEBUG
    roots.VerifyNameForProtectors();
#endif
    roots.VerifyNameForProtectorsPages();
  }

  PostProcessNewObjectsIfStaticRootsEnabled();

  if (should_rehash()) {
    isolate()->heap()->InitializeHashSeed();
    Rehash();
  }
}

#ifdef V8_STATIC_ROOTS
void ReadOnlyDeserializer::PostProcessNewObjectsIfStaticRootsEnabled() {
  // Since we are not deserializing individual objects we need to scan the
  // heap and search for objects that need post-processing.
  //
  // See also Deserializer<IsolateT>::PostProcessNewObject.
  //
  // TODO(v8:13840, olivf): Unify this with non V8_STATIC_ROOTS configuration
  // builds.
  ReadOnlyHeapObjectIterator iterator(isolate()->read_only_heap());
  PtrComprCageBase cage_base(isolate());
  for (HeapObject object = iterator.Next(); !object.is_null();
       object = iterator.Next()) {
    const InstanceType instance_type = object.map(cage_base).instance_type();

    if (should_rehash()) {
      if (InstanceTypeChecker::IsString(instance_type)) {
        String str = String::cast(object);
        str.set_raw_hash_field(Name::kEmptyHashField);
        PushObjectToRehash(handle(str, isolate()));
      } else if (object.NeedsRehashing(instance_type)) {
        PushObjectToRehash(handle(object, isolate()));
      }
    }

    if (InstanceTypeChecker::IsCode(instance_type)) {
      Code code = Code::cast(object);
      code.init_instruction_start(main_thread_isolate(), kNullAddress);
      // RO space only contains builtin Code objects which don't have an
      // attached InstructionStream.
      DCHECK(code.is_builtin());
      DCHECK(!code.has_instruction_stream());
      code.SetInstructionStartForOffHeapBuiltin(
          main_thread_isolate(), EmbeddedData::FromBlob(main_thread_isolate())
                                     .InstructionStartOf(code.builtin_id()));
    }
  }
}
#endif  // V8_STATIC_ROOTS

}  // namespace internal
}  // namespace v8
