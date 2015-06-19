// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_GLOBAL_HANDLES_H_
#define V8_GLOBAL_HANDLES_H_

#include "include/v8.h"
#include "include/v8-profiler.h"

#include "src/handles.h"
#include "src/list.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

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
    DCHECK(length > 0);
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
    DCHECK(length > 0);
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


enum WeaknessType {
  NORMAL_WEAK,  // Embedder gets a handle to the dying object.
  // In the following cases, the embedder gets the parameter they passed in
  // earlier, and 0 or 2 first internal fields. Note that the internal
  // fields must contain aligned non-V8 pointers.  Getting pointers to V8
  // objects through this interface would be GC unsafe so in that case the
  // embedder gets a null pointer instead.
  PHANTOM_WEAK,
  PHANTOM_WEAK_2_INTERNAL_FIELDS
};


class GlobalHandles {
 public:
  ~GlobalHandles();

  // Creates a new global handle that is alive until Destroy is called.
  Handle<Object> Create(Object* value);

  // Copy a global handle
  static Handle<Object> CopyGlobal(Object** location);

  // Destroy a global handle.
  static void Destroy(Object** location);

  typedef WeakCallbackData<v8::Value, void>::Callback WeakCallback;

  // For a phantom weak reference, the callback does not have access to the
  // dying object.  Phantom weak references are preferred because they allow
  // memory to be reclaimed in one GC cycle rather than two.  However, for
  // historical reasons the default is non-phantom.
  enum PhantomState { Nonphantom, Phantom };

  // Make the global handle weak and set the callback parameter for the
  // handle.  When the garbage collector recognizes that only weak global
  // handles point to an object the callback function is invoked (for each
  // handle) with the handle and corresponding parameter as arguments.  By
  // default the handle still contains a pointer to the object that is being
  // collected.  For this reason the object is not collected until the next
  // GC.  For a phantom weak handle the handle is cleared (set to a Smi)
  // before the callback is invoked, but the handle can still be identified
  // in the callback by using the location() of the handle.
  static void MakeWeak(Object** location, void* parameter,
                       WeakCallback weak_callback);

  // It would be nice to template this one, but it's really hard to get
  // the template instantiator to work right if you do.
  static void MakeWeak(Object** location, void* parameter,
                       WeakCallbackInfo<void>::Callback weak_callback,
                       v8::WeakCallbackType type);

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
  static void* ClearWeakness(Object** location);

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
  // Returns the number of freed nodes.
  int PostGarbageCollectionProcessing(GarbageCollector collector);

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

  // Helpers for PostGarbageCollectionProcessing.
  int PostScavengeProcessing(int initial_post_gc_processing_count);
  int PostMarkSweepProcessing(int initial_post_gc_processing_count);
  int DispatchPendingPhantomCallbacks();
  void UpdateListOfNewSpaceNodes();

  // Internal node structures.
  class Node;
  class NodeBlock;
  class NodeIterator;
  class PendingPhantomCallback;

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

  List<PendingPhantomCallback> pending_phantom_callbacks_;

  friend class Isolate;

  DISALLOW_COPY_AND_ASSIGN(GlobalHandles);
};


class GlobalHandles::PendingPhantomCallback {
 public:
  typedef v8::WeakCallbackInfo<void> Data;
  PendingPhantomCallback(
      Node* node, Data::Callback callback, void* parameter,
      void* internal_fields[v8::kInternalFieldsInWeakCallback])
      : node_(node), callback_(callback), parameter_(parameter) {
    for (int i = 0; i < v8::kInternalFieldsInWeakCallback; ++i) {
      internal_fields_[i] = internal_fields[i];
    }
  }

  void Invoke(Isolate* isolate);

  Node* node() { return node_; }
  Data::Callback callback() { return callback_; }

 private:
  Node* node_;
  Data::Callback callback_;
  void* parameter_;
  void* internal_fields_[v8::kInternalFieldsInWeakCallback];
};


class EternalHandles {
 public:
  enum SingletonHandle {
    I18N_TEMPLATE_ONE,
    I18N_TEMPLATE_TWO,
    DATE_CACHE_VERSION,

    NUMBER_OF_SINGLETON_HANDLES
  };

  EternalHandles();
  ~EternalHandles();

  int NumberOfHandles() { return size_; }

  // Create an EternalHandle, overwriting the index.
  void Create(Isolate* isolate, Object* object, int* index);

  // Grab the handle for an existing EternalHandle.
  inline Handle<Object> Get(int index) {
    return Handle<Object>(GetLocation(index));
  }

  // Grab the handle for an existing SingletonHandle.
  inline Handle<Object> GetSingleton(SingletonHandle singleton) {
    DCHECK(Exists(singleton));
    return Get(singleton_handles_[singleton]);
  }

  // Checks whether a SingletonHandle has been assigned.
  inline bool Exists(SingletonHandle singleton) {
    return singleton_handles_[singleton] != kInvalidIndex;
  }

  // Assign a SingletonHandle to an empty slot and returns the handle.
  Handle<Object> CreateSingleton(Isolate* isolate,
                                 Object* object,
                                 SingletonHandle singleton) {
    Create(isolate, object, &singleton_handles_[singleton]);
    return Get(singleton_handles_[singleton]);
  }

  // Iterates over all handles.
  void IterateAllRoots(ObjectVisitor* visitor);
  // Iterates over all handles which might be in new space.
  void IterateNewSpaceRoots(ObjectVisitor* visitor);
  // Rebuilds new space list.
  void PostGarbageCollectionProcessing(Heap* heap);

 private:
  static const int kInvalidIndex = -1;
  static const int kShift = 8;
  static const int kSize = 1 << kShift;
  static const int kMask = 0xff;

  // Gets the slot for an index
  inline Object** GetLocation(int index) {
    DCHECK(index >= 0 && index < size_);
    return &blocks_[index >> kShift][index & kMask];
  }

  int size_;
  List<Object**> blocks_;
  List<int> new_space_indices_;
  int singleton_handles_[NUMBER_OF_SINGLETON_HANDLES];

  DISALLOW_COPY_AND_ASSIGN(EternalHandles);
};


} }  // namespace v8::internal

#endif  // V8_GLOBAL_HANDLES_H_
