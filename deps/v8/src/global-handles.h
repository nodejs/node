// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_GLOBAL_HANDLES_H_
#define V8_GLOBAL_HANDLES_H_

#include "../include/v8.h"
#include "../include/v8-profiler.h"

#include "list.h"
#include "v8utils.h"

namespace v8 {
namespace internal {

class GCTracer;
class HeapStats;
class ObjectVisitor;

// Structure for tracking global handles.
// A single list keeps all the allocated global handles.
// Destroyed handles stay in the list but is added to the free list.
// At GC the destroyed global handles are removed from the free list
// and deallocated.

// Data structures for tracking object groups and implicit references.

// An object group is treated like a single JS object: if one of object in
// the group is alive, all objects in the same group are considered alive.
// An object group is used to simulate object relationship in a DOM tree.

// An implicit references group consists of two parts: a parent object and a
// list of children objects.  If the parent is alive, all the children are alive
// too.

struct ObjectGroup {
  explicit ObjectGroup(size_t length)
      : info(NULL), length(length) {
    ASSERT(length > 0);
    objects = new Object**[length];
  }
  ~ObjectGroup();

  v8::RetainedObjectInfo* info;
  Object*** objects;
  size_t length;
};


struct ImplicitRefGroup {
  ImplicitRefGroup(HeapObject** parent, size_t length)
      : parent(parent), length(length) {
    ASSERT(length > 0);
    children = new Object**[length];
  }
  ~ImplicitRefGroup();

  HeapObject** parent;
  Object*** children;
  size_t length;
};


// For internal bookkeeping.
struct ObjectGroupConnection {
  ObjectGroupConnection(UniqueId id, Object** object)
      : id(id), object(object) {}

  bool operator==(const ObjectGroupConnection& other) const {
    return id == other.id;
  }

  bool operator<(const ObjectGroupConnection& other) const {
    return id < other.id;
  }

  UniqueId id;
  Object** object;
};


struct ObjectGroupRetainerInfo {
  ObjectGroupRetainerInfo(UniqueId id, RetainedObjectInfo* info)
      : id(id), info(info) {}

  bool operator==(const ObjectGroupRetainerInfo& other) const {
    return id == other.id;
  }

  bool operator<(const ObjectGroupRetainerInfo& other) const {
    return id < other.id;
  }

  UniqueId id;
  RetainedObjectInfo* info;
};


class GlobalHandles {
 public:
  ~GlobalHandles();

  // Creates a new global handle that is alive until Destroy is called.
  Handle<Object> Create(Object* value);

  // Destroy a global handle.
  static void Destroy(Object** location);

  typedef WeakReferenceCallbacks<v8::Value, void>::Revivable RevivableCallback;

  // Make the global handle weak and set the callback parameter for the
  // handle.  When the garbage collector recognizes that only weak global
  // handles point to an object the handles are cleared and the callback
  // function is invoked (for each handle) with the handle and corresponding
  // parameter as arguments.  Note: cleared means set to Smi::FromInt(0). The
  // reason is that Smi::FromInt(0) does not change during garage collection.
  static void MakeWeak(Object** location,
                       void* parameter,
                       RevivableCallback weak_reference_callback);

  void RecordStats(HeapStats* stats);

  // Returns the current number of weak handles.
  int NumberOfWeakHandles();

  // Returns the current number of weak handles to global objects.
  // These handles are also included in NumberOfWeakHandles().
  int NumberOfGlobalObjectWeakHandles();

  // Returns the current number of handles to global objects.
  int global_handles_count() const {
    return number_of_global_handles_;
  }

  // Clear the weakness of a global handle.
  static void ClearWeakness(Object** location);

  // Clear the weakness of a global handle.
  static void MarkIndependent(Object** location);

  // Mark the reference to this object externaly unreachable.
  static void MarkPartiallyDependent(Object** location);

  static bool IsIndependent(Object** location);

  // Tells whether global handle is near death.
  static bool IsNearDeath(Object** location);

  // Tells whether global handle is weak.
  static bool IsWeak(Object** location);

  // Process pending weak handles.
  // Returns true if next major GC is likely to collect more garbage.
  bool PostGarbageCollectionProcessing(GarbageCollector collector,
                                       GCTracer* tracer);

  // Iterates over all strong handles.
  void IterateStrongRoots(ObjectVisitor* v);

  // Iterates over all handles.
  void IterateAllRoots(ObjectVisitor* v);

  // Iterates over all handles that have embedder-assigned class ID.
  void IterateAllRootsWithClassIds(ObjectVisitor* v);

  // Iterates over all handles in the new space that have embedder-assigned
  // class ID.
  void IterateAllRootsInNewSpaceWithClassIds(ObjectVisitor* v);

  // Iterates over all weak roots in heap.
  void IterateWeakRoots(ObjectVisitor* v);

  // Find all weak handles satisfying the callback predicate, mark
  // them as pending.
  void IdentifyWeakHandles(WeakSlotCallback f);

  // NOTE: Three ...NewSpace... functions below are used during
  // scavenge collections and iterate over sets of handles that are
  // guaranteed to contain all handles holding new space objects (but
  // may also include old space objects).

  // Iterates over strong and dependent handles. See the node above.
  void IterateNewSpaceStrongAndDependentRoots(ObjectVisitor* v);

  // Finds weak independent or partially independent handles satisfying
  // the callback predicate and marks them as pending. See the note above.
  void IdentifyNewSpaceWeakIndependentHandles(WeakSlotCallbackWithHeap f);

  // Iterates over weak independent or partially independent handles.
  // See the note above.
  void IterateNewSpaceWeakIndependentRoots(ObjectVisitor* v);

  // Iterate over objects in object groups that have at least one object
  // which requires visiting. The callback has to return true if objects
  // can be skipped and false otherwise.
  bool IterateObjectGroups(ObjectVisitor* v, WeakSlotCallbackWithHeap can_skip);

  // Add an object group.
  // Should be only used in GC callback function before a collection.
  // All groups are destroyed after a garbage collection.
  void AddObjectGroup(Object*** handles,
                      size_t length,
                      v8::RetainedObjectInfo* info);

  // Associates handle with the object group represented by id.
  // Should be only used in GC callback function before a collection.
  // All groups are destroyed after a garbage collection.
  void SetObjectGroupId(Object** handle, UniqueId id);

  // Set RetainedObjectInfo for an object group. Should not be called more than
  // once for a group. Should not be called for a group which contains no
  // handles.
  void SetRetainedObjectInfo(UniqueId id, RetainedObjectInfo* info);

  // Add an implicit references' group.
  // Should be only used in GC callback function before a collection.
  // All groups are destroyed after a mark-compact collection.
  void AddImplicitReferences(HeapObject** parent,
                             Object*** children,
                             size_t length);

  // Adds an implicit reference from a group to an object. Should be only used
  // in GC callback function before a collection. All implicit references are
  // destroyed after a mark-compact collection.
  void SetReferenceFromGroup(UniqueId id, Object** child);

  // Adds an implicit reference from a parent object to a child object. Should
  // be only used in GC callback function before a collection. All implicit
  // references are destroyed after a mark-compact collection.
  void SetReference(HeapObject** parent, Object** child);

  List<ObjectGroup*>* object_groups() {
    ComputeObjectGroupsAndImplicitReferences();
    return &object_groups_;
  }

  List<ImplicitRefGroup*>* implicit_ref_groups() {
    ComputeObjectGroupsAndImplicitReferences();
    return &implicit_ref_groups_;
  }

  // Remove bags, this should only happen after GC.
  void RemoveObjectGroups();
  void RemoveImplicitRefGroups();

  // Tear down the global handle structure.
  void TearDown();

  Isolate* isolate() { return isolate_; }

#ifdef DEBUG
  void PrintStats();
  void Print();
#endif

 private:
  explicit GlobalHandles(Isolate* isolate);

  // Migrates data from the internal representation (object_group_connections_,
  // retainer_infos_ and implicit_ref_connections_) to the public and more
  // efficient representation (object_groups_ and implicit_ref_groups_).
  void ComputeObjectGroupsAndImplicitReferences();

  // v8::internal::List is inefficient even for small number of elements, if we
  // don't assign any initial capacity.
  static const int kObjectGroupConnectionsCapacity = 20;

  // Internal node structures.
  class Node;
  class NodeBlock;
  class NodeIterator;

  Isolate* isolate_;

  // Field always containing the number of handles to global objects.
  int number_of_global_handles_;

  // List of all allocated node blocks.
  NodeBlock* first_block_;

  // List of node blocks with used nodes.
  NodeBlock* first_used_block_;

  // Free list of nodes.
  Node* first_free_;

  // Contains all nodes holding new space objects. Note: when the list
  // is accessed, some of the objects may have been promoted already.
  List<Node*> new_space_nodes_;

  int post_gc_processing_count_;

  // Object groups and implicit references, public and more efficient
  // representation.
  List<ObjectGroup*> object_groups_;
  List<ImplicitRefGroup*> implicit_ref_groups_;

  // Object groups and implicit references, temporary representation while
  // constructing the groups.
  List<ObjectGroupConnection> object_group_connections_;
  List<ObjectGroupRetainerInfo> retainer_infos_;
  List<ObjectGroupConnection> implicit_ref_connections_;

  friend class Isolate;

  DISALLOW_COPY_AND_ASSIGN(GlobalHandles);
};


} }  // namespace v8::internal

#endif  // V8_GLOBAL_HANDLES_H_
