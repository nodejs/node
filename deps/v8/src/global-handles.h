// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_GLOBAL_HANDLES_H_
#define V8_GLOBAL_HANDLES_H_

#include <type_traits>
#include <vector>

#include "include/v8.h"
#include "include/v8-profiler.h"

#include "src/handles.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

class HeapStats;
class RootVisitor;

// Structure for tracking global handles.
// A single list keeps all the allocated global handles.
// Destroyed handles stay in the list but is added to the free list.
// At GC the destroyed global handles are removed from the free list
// and deallocated.

enum WeaknessType {
  // Embedder gets a handle to the dying object.
  FINALIZER_WEAK,
  // In the following cases, the embedder gets the parameter they passed in
  // earlier, and 0 or 2 first embedder fields. Note that the internal
  // fields must contain aligned non-V8 pointers.  Getting pointers to V8
  // objects through this interface would be GC unsafe so in that case the
  // embedder gets a null pointer instead.
  PHANTOM_WEAK,
  PHANTOM_WEAK_2_EMBEDDER_FIELDS,
  // The handle is automatically reset by the garbage collector when
  // the object is no longer reachable.
  PHANTOM_WEAK_RESET_HANDLE
};

class GlobalHandles {
 public:
  ~GlobalHandles();

  // Creates a new global handle that is alive until Destroy is called.
  Handle<Object> Create(Object* value);

  template <typename T>
  Handle<T> Create(T* value) {
    static_assert(std::is_base_of<Object, T>::value, "static type violation");
    // The compiler should only pick this method if T is not Object.
    static_assert(!std::is_same<Object, T>::value, "compiler error");
    return Handle<T>::cast(Create(static_cast<Object*>(value)));
  }

  // Copy a global handle
  static Handle<Object> CopyGlobal(Object** location);

  // Destroy a global handle.
  static void Destroy(Object** location);

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
                       WeakCallbackInfo<void>::Callback weak_callback,
                       v8::WeakCallbackType type);

  static void MakeWeak(Object*** location_addr);

  static void AnnotateStrongRetainer(Object** location, const char* label);

  void RecordStats(HeapStats* stats);

  // Returns the current number of handles to global objects.
  int global_handles_count() const {
    return number_of_global_handles_;
  }

  size_t NumberOfPhantomHandleResets() {
    return number_of_phantom_handle_resets_;
  }

  void ResetNumberOfPhantomHandleResets() {
    number_of_phantom_handle_resets_ = 0;
  }

  size_t NumberOfNewSpaceNodes() { return new_space_nodes_.size(); }

  // Clear the weakness of a global handle.
  static void* ClearWeakness(Object** location);

  // Tells whether global handle is near death.
  static bool IsNearDeath(Object** location);

  // Tells whether global handle is weak.
  static bool IsWeak(Object** location);

  // Process pending weak handles.
  // Returns the number of freed nodes.
  int PostGarbageCollectionProcessing(
      GarbageCollector collector, const v8::GCCallbackFlags gc_callback_flags);

  void IterateStrongRoots(RootVisitor* v);

  void IterateWeakRoots(RootVisitor* v);

  void IterateAllRoots(RootVisitor* v);

  void IterateAllNewSpaceRoots(RootVisitor* v);
  void IterateNewSpaceRoots(RootVisitor* v, size_t start, size_t end);

  // Iterates over all handles that have embedder-assigned class ID.
  void IterateAllRootsWithClassIds(v8::PersistentHandleVisitor* v);

  // Iterates over all handles in the new space that have embedder-assigned
  // class ID.
  void IterateAllRootsInNewSpaceWithClassIds(v8::PersistentHandleVisitor* v);

  // Iterate over all handles in the new space that are weak, unmodified
  // and have class IDs
  void IterateWeakRootsInNewSpaceWithClassIds(v8::PersistentHandleVisitor* v);

  // Iterates over weak roots on the heap.
  void IterateWeakRootsForFinalizers(RootVisitor* v);
  void IterateWeakRootsForPhantomHandles(
      WeakSlotCallbackWithHeap should_reset_handle);

  // Marks all handles that should be finalized based on the predicate
  // |should_reset_handle| as pending.
  void IdentifyWeakHandles(WeakSlotCallbackWithHeap should_reset_handle);

  // NOTE: Five ...NewSpace... functions below are used during
  // scavenge collections and iterate over sets of handles that are
  // guaranteed to contain all handles holding new space objects (but
  // may also include old space objects).

  // Iterates over strong and dependent handles. See the note above.
  void IterateNewSpaceStrongAndDependentRoots(RootVisitor* v);

  // Iterates over strong and dependent handles. See the note above.
  // Also marks unmodified nodes in the same iteration.
  void IterateNewSpaceStrongAndDependentRootsAndIdentifyUnmodified(
      RootVisitor* v, size_t start, size_t end);

  // Marks weak unmodified handles satisfying |is_dead| as pending.
  void MarkNewSpaceWeakUnmodifiedObjectsPending(
      WeakSlotCallbackWithHeap is_dead);

  // Iterates over weak independent or unmodified handles.
  // See the note above.
  void IterateNewSpaceWeakUnmodifiedRootsForFinalizers(RootVisitor* v);
  void IterateNewSpaceWeakUnmodifiedRootsForPhantomHandles(
      RootVisitor* v, WeakSlotCallbackWithHeap should_reset_handle);

  // Identify unmodified objects that are in weak state and marks them
  // unmodified
  void IdentifyWeakUnmodifiedObjects(WeakSlotCallback is_unmodified);

  // Tear down the global handle structure.
  void TearDown();

  Isolate* isolate() { return isolate_; }

#ifdef DEBUG
  void PrintStats();
  void Print();
#endif  // DEBUG

  void InvokeSecondPassPhantomCallbacks();

 private:
  // Internal node structures.
  class Node;
  class NodeBlock;
  class NodeIterator;
  class PendingPhantomCallback;

  explicit GlobalHandles(Isolate* isolate);

  void InvokeSecondPassPhantomCallbacksFromTask();
  int PostScavengeProcessing(int initial_post_gc_processing_count);
  int PostMarkSweepProcessing(int initial_post_gc_processing_count);
  int DispatchPendingPhantomCallbacks(bool synchronous_second_pass);
  void UpdateListOfNewSpaceNodes();
  void ApplyPersistentHandleVisitor(v8::PersistentHandleVisitor* visitor,
                                    Node* node);

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
  std::vector<Node*> new_space_nodes_;

  int post_gc_processing_count_;

  size_t number_of_phantom_handle_resets_;

  std::vector<PendingPhantomCallback> pending_phantom_callbacks_;
  std::vector<PendingPhantomCallback> second_pass_callbacks_;
  bool second_pass_callbacks_task_posted_ = false;

  friend class Isolate;

  DISALLOW_COPY_AND_ASSIGN(GlobalHandles);
};


class GlobalHandles::PendingPhantomCallback {
 public:
  typedef v8::WeakCallbackInfo<void> Data;
  PendingPhantomCallback(
      Node* node, Data::Callback callback, void* parameter,
      void* embedder_fields[v8::kEmbedderFieldsInWeakCallback])
      : node_(node), callback_(callback), parameter_(parameter) {
    for (int i = 0; i < v8::kEmbedderFieldsInWeakCallback; ++i) {
      embedder_fields_[i] = embedder_fields[i];
    }
  }

  void Invoke(Isolate* isolate);

  Node* node() { return node_; }
  Data::Callback callback() { return callback_; }

 private:
  Node* node_;
  Data::Callback callback_;
  void* parameter_;
  void* embedder_fields_[v8::kEmbedderFieldsInWeakCallback];
};


class EternalHandles {
 public:
  enum SingletonHandle {
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
  void IterateAllRoots(RootVisitor* visitor);
  // Iterates over all handles which might be in new space.
  void IterateNewSpaceRoots(RootVisitor* visitor);
  // Rebuilds new space list.
  void PostGarbageCollectionProcessing();

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
  std::vector<Object**> blocks_;
  std::vector<int> new_space_indices_;
  int singleton_handles_[NUMBER_OF_SINGLETON_HANDLES];

  DISALLOW_COPY_AND_ASSIGN(EternalHandles);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_GLOBAL_HANDLES_H_
