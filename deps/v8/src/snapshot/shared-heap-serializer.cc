// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/shared-heap-serializer.h"

#include "src/heap/heap-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/read-only-serializer.h"

namespace v8 {
namespace internal {

// static
bool SharedHeapSerializer::CanBeInSharedOldSpace(HeapObject obj) {
  if (ReadOnlyHeap::Contains(obj)) return false;
  if (obj.IsString()) {
    return obj.IsInternalizedString() ||
           String::IsInPlaceInternalizable(String::cast(obj));
  }
  return false;
}

// static
bool SharedHeapSerializer::ShouldBeInSharedHeapObjectCache(HeapObject obj) {
  // To keep the shared heap object cache lean, only include objects that should
  // not be duplicated. Currently, that is only internalized strings. In-place
  // internalizable strings will still be allocated in the shared heap by the
  // deserializer, but do not need to be kept alive forever in the cache.
  if (CanBeInSharedOldSpace(obj)) {
    if (obj.IsInternalizedString()) return true;
  }
  return false;
}

SharedHeapSerializer::SharedHeapSerializer(
    Isolate* isolate, Snapshot::SerializerFlags flags,
    ReadOnlySerializer* read_only_serializer)
    : RootsSerializer(isolate, flags, RootIndex::kFirstStrongRoot),
      read_only_serializer_(read_only_serializer)
#ifdef DEBUG
      ,
      serialized_objects_(isolate->heap())
#endif
{
}

SharedHeapSerializer::~SharedHeapSerializer() {
  OutputStatistics("SharedHeapSerializer");
}

void SharedHeapSerializer::FinalizeSerialization() {
  // This is called after serialization of the startup and context snapshots
  // which entries are added to the shared heap object cache. Terminate the
  // cache with an undefined.
  Object undefined = ReadOnlyRoots(isolate()).undefined_value();
  VisitRootPointer(Root::kSharedHeapObjectCache, nullptr,
                   FullObjectSlot(&undefined));

  // When FLAG_shared_string_table is true, all internalized and
  // internalizable-in-place strings are in the shared heap.
  SerializeStringTable(isolate()->string_table());
  SerializeDeferredObjects();
  Pad();

#ifdef DEBUG
  // During snapshotting there is no shared heap.
  CHECK(!isolate()->is_shared());
  CHECK_NULL(isolate()->shared_isolate());

  // Check that all serialized object are in shared heap and not RO. RO objects
  // should be in the RO snapshot.
  IdentityMap<int, base::DefaultAllocationPolicy>::IteratableScope it_scope(
      &serialized_objects_);
  for (auto it = it_scope.begin(); it != it_scope.end(); ++it) {
    HeapObject obj = HeapObject::cast(it.key());
    CHECK(CanBeInSharedOldSpace(obj));
    CHECK(!ReadOnlyHeap::Contains(obj));
  }
#endif
}

bool SharedHeapSerializer::SerializeUsingReadOnlyObjectCache(
    SnapshotByteSink* sink, Handle<HeapObject> obj) {
  return read_only_serializer_->SerializeUsingReadOnlyObjectCache(sink, obj);
}

bool SharedHeapSerializer::SerializeUsingSharedHeapObjectCache(
    SnapshotByteSink* sink, Handle<HeapObject> obj) {
  if (!ShouldBeInSharedHeapObjectCache(*obj)) return false;
  int cache_index = SerializeInObjectCache(obj);
  sink->Put(kSharedHeapObjectCache, "SharedHeapObjectCache");
  sink->PutInt(cache_index, "shared_heap_object_cache_index");
  return true;
}

void SharedHeapSerializer::SerializeStringTable(StringTable* string_table) {
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

  sink_.PutInt(string_table->NumberOfElements(),
               "String table number of elements");

  // Custom RootVisitor which walks the string table, but only serializes the
  // string entries. This is an inline class to be able to access the non-public
  // SerializeObject method.
  class SharedHeapSerializerStringTableVisitor : public RootVisitor {
   public:
    explicit SharedHeapSerializerStringTableVisitor(
        SharedHeapSerializer* serializer)
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
    SharedHeapSerializer* serializer_;
  };

  SharedHeapSerializerStringTableVisitor string_table_visitor(this);
  isolate()->string_table()->IterateElements(&string_table_visitor);
}

void SharedHeapSerializer::SerializeObjectImpl(Handle<HeapObject> obj) {
  // Objects in the shared heap cannot depend on per-Isolate roots but can
  // depend on RO roots since sharing objects requires sharing the RO space.
  DCHECK(CanBeInSharedOldSpace(*obj) || ReadOnlyHeap::Contains(*obj));

  if (SerializeHotObject(obj)) return;
  if (IsRootAndHasBeenSerialized(*obj) && SerializeRoot(obj)) return;
  if (SerializeUsingReadOnlyObjectCache(&sink_, obj)) return;
  if (SerializeBackReference(obj)) return;

  CheckRehashability(*obj);

  DCHECK(!ReadOnlyHeap::Contains(*obj));
  ObjectSerializer object_serializer(this, obj, &sink_);
  object_serializer.Serialize();

#ifdef DEBUG
  CHECK_NULL(serialized_objects_.Find(obj));
  // There's no "IdentitySet", so use an IdentityMap with a value that is
  // later ignored.
  serialized_objects_.Insert(obj, 0);
#endif
}

}  // namespace internal
}  // namespace v8
