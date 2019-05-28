// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_GLOBAL_HANDLES_H_
#define V8_GLOBAL_HANDLES_H_

#include <type_traits>
#include <utility>
#include <vector>

#include "include/v8.h"
#include "include/v8-profiler.h"

#include "src/handles.h"
#include "src/objects.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

class HeapStats;
class RootVisitor;

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

// Global handles hold handles that are independent of stack-state and can have
// callbacks and finalizers attached to them.
class V8_EXPORT_PRIVATE GlobalHandles final {
 public:
  template <class NodeType>
  class NodeBlock;

  //
  // API for regular handles.
  //

  static void MoveGlobal(Address** from, Address** to);

  static Handle<Object> CopyGlobal(Address* location);

  static void Destroy(Address* location);

  // Make the global handle weak and set the callback parameter for the
  // handle.  When the garbage collector recognizes that only weak global
  // handles point to an object the callback function is invoked (for each
  // handle) with the handle and corresponding parameter as arguments.  By
  // default the handle still contains a pointer to the object that is being
  // collected.  For this reason the object is not collected until the next
  // GC.  For a phantom weak handle the handle is cleared (set to a Smi)
  // before the callback is invoked, but the handle can still be identified
  // in the callback by using the location() of the handle.
  static void MakeWeak(Address* location, void* parameter,
                       WeakCallbackInfo<void>::Callback weak_callback,
                       v8::WeakCallbackType type);
  static void MakeWeak(Address** location_addr);

  static void AnnotateStrongRetainer(Address* location, const char* label);

  // Clear the weakness of a global handle.
  static void* ClearWeakness(Address* location);

  // Tells whether global handle is weak.
  static bool IsWeak(Address* location);

  //
  // API for traced handles.
  //

  static void MoveTracedGlobal(Address** from, Address** to);
  static void DestroyTraced(Address* location);
  static void SetFinalizationCallbackForTraced(
      Address* location, void* parameter,
      WeakCallbackInfo<void>::Callback callback);

  explicit GlobalHandles(Isolate* isolate);
  ~GlobalHandles();

  // Creates a new global handle that is alive until Destroy is called.
  Handle<Object> Create(Object value);
  Handle<Object> Create(Address value);

  template <typename T>
  Handle<T> Create(T value) {
    static_assert(std::is_base_of<Object, T>::value, "static type violation");
    // The compiler should only pick this method if T is not Object.
    static_assert(!std::is_same<Object, T>::value, "compiler error");
    return Handle<T>::cast(Create(Object(value)));
  }

  Handle<Object> CreateTraced(Object value, Address* slot);
  Handle<Object> CreateTraced(Address value, Address* slot);

  void RecordStats(HeapStats* stats);

  size_t InvokeFirstPassWeakCallbacks();
  void InvokeSecondPassPhantomCallbacks();

  // Process pending weak handles.
  // Returns the number of freed nodes.
  size_t PostGarbageCollectionProcessing(
      GarbageCollector collector, const v8::GCCallbackFlags gc_callback_flags);

  void IterateStrongRoots(RootVisitor* v);
  void IterateWeakRoots(RootVisitor* v);
  void IterateAllRoots(RootVisitor* v);
  void IterateAllYoungRoots(RootVisitor* v);

  // Iterates over all handles that have embedder-assigned class ID.
  void IterateAllRootsWithClassIds(v8::PersistentHandleVisitor* v);

  // Iterates over all handles in the new space that have embedder-assigned
  // class ID.
  void IterateAllYoungRootsWithClassIds(v8::PersistentHandleVisitor* v);

  // Iterate over all handles in the new space that are weak, unmodified
  // and have class IDs
  void IterateYoungWeakRootsWithClassIds(v8::PersistentHandleVisitor* v);

  // Iterates over all traces handles represented by TracedGlobal.
  void IterateTracedNodes(
      v8::EmbedderHeapTracer::TracedGlobalHandleVisitor* visitor);

  // Marks handles with finalizers on the predicate |should_reset_handle| as
  // pending.
  void IterateWeakRootsIdentifyFinalizers(
      WeakSlotCallbackWithHeap should_reset_handle);
  // Uses the provided visitor |v| to mark handles with finalizers that are
  // pending.
  void IterateWeakRootsForFinalizers(RootVisitor* v);
  // Marks handles that are phantom or have callbacks based on the predicate
  // |should_reset_handle| as pending.
  void IterateWeakRootsForPhantomHandles(
      WeakSlotCallbackWithHeap should_reset_handle);

  //  Note: The following *Young* methods are used for the Scavenger to
  //  identify and process handles in the young generation. The set of young
  //  handles is complete but the methods may encounter handles that are
  //  already in old space.

  // Iterates over strong and dependent handles. See the note above.
  void IterateYoungStrongAndDependentRoots(RootVisitor* v);

  // Marks weak unmodified handles satisfying |is_dead| as pending.
  void MarkYoungWeakUnmodifiedObjectsPending(WeakSlotCallbackWithHeap is_dead);

  // Iterates over weak independent or unmodified handles.
  // See the note above.
  void IterateYoungWeakUnmodifiedRootsForFinalizers(RootVisitor* v);
  void IterateYoungWeakUnmodifiedRootsForPhantomHandles(
      RootVisitor* v, WeakSlotCallbackWithHeap should_reset_handle);

  // Identify unmodified objects that are in weak state and marks them
  // unmodified
  void IdentifyWeakUnmodifiedObjects(WeakSlotCallback is_unmodified);

  Isolate* isolate() const { return isolate_; }

  // Number of global handles.
  size_t handles_count() const { return handles_count_; }

  size_t GetAndResetGlobalHandleResetCount() {
    size_t old = number_of_phantom_handle_resets_;
    number_of_phantom_handle_resets_ = 0;
    return old;
  }

#ifdef DEBUG
  void PrintStats();
  void Print();
#endif  // DEBUG

 private:
  // Internal node structures.
  class Node;
  template <class BlockType>
  class NodeIterator;
  template <class NodeType>
  class NodeSpace;
  class PendingPhantomCallback;
  class TracedNode;

  bool InRecursiveGC(unsigned gc_processing_counter);

  void InvokeSecondPassPhantomCallbacksFromTask();
  void InvokeOrScheduleSecondPassPhantomCallbacks(bool synchronous_second_pass);
  size_t PostScavengeProcessing(unsigned post_processing_count);
  size_t PostMarkSweepProcessing(unsigned post_processing_count);

  template <typename T>
  size_t InvokeFirstPassWeakCallbacks(
      std::vector<std::pair<T*, PendingPhantomCallback>>* pending);

  template <typename T>
  void UpdateAndCompactListOfYoungNode(std::vector<T*>* node_list);
  void UpdateListOfYoungNodes();

  void ApplyPersistentHandleVisitor(v8::PersistentHandleVisitor* visitor,
                                    Node* node);

  Isolate* const isolate_;

  std::unique_ptr<NodeSpace<Node>> regular_nodes_;
  // Contains all nodes holding young objects. Note: when the list
  // is accessed, some of the objects may have been promoted already.
  std::vector<Node*> young_nodes_;

  std::unique_ptr<NodeSpace<TracedNode>> traced_nodes_;
  std::vector<TracedNode*> traced_young_nodes_;

  // Field always containing the number of handles to global objects.
  size_t handles_count_ = 0;
  size_t number_of_phantom_handle_resets_ = 0;

  std::vector<std::pair<Node*, PendingPhantomCallback>>
      regular_pending_phantom_callbacks_;
  std::vector<std::pair<TracedNode*, PendingPhantomCallback>>
      traced_pending_phantom_callbacks_;
  std::vector<PendingPhantomCallback> second_pass_callbacks_;
  bool second_pass_callbacks_task_posted_ = false;

  // Counter for recursive garbage collections during callback processing.
  unsigned post_gc_processing_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(GlobalHandles);
};

class GlobalHandles::PendingPhantomCallback final {
 public:
  typedef v8::WeakCallbackInfo<void> Data;

  enum InvocationType { kFirstPass, kSecondPass };

  PendingPhantomCallback(
      Data::Callback callback, void* parameter,
      void* embedder_fields[v8::kEmbedderFieldsInWeakCallback])
      : callback_(callback), parameter_(parameter) {
    for (int i = 0; i < v8::kEmbedderFieldsInWeakCallback; ++i) {
      embedder_fields_[i] = embedder_fields[i];
    }
  }

  void Invoke(Isolate* isolate, InvocationType type);

  Data::Callback callback() const { return callback_; }

 private:
  Data::Callback callback_;
  void* parameter_;
  void* embedder_fields_[v8::kEmbedderFieldsInWeakCallback];
};

class EternalHandles final {
 public:
  EternalHandles() = default;
  ~EternalHandles();

  // Create an EternalHandle, overwriting the index.
  V8_EXPORT_PRIVATE void Create(Isolate* isolate, Object object, int* index);

  // Grab the handle for an existing EternalHandle.
  inline Handle<Object> Get(int index) {
    return Handle<Object>(GetLocation(index));
  }

  // Iterates over all handles.
  void IterateAllRoots(RootVisitor* visitor);
  // Iterates over all handles which might be in the young generation.
  void IterateYoungRoots(RootVisitor* visitor);
  // Rebuilds new space list.
  void PostGarbageCollectionProcessing();

  size_t handles_count() const { return size_; }

 private:
  static const int kInvalidIndex = -1;
  static const int kShift = 8;
  static const int kSize = 1 << kShift;
  static const int kMask = 0xff;

  // Gets the slot for an index. This returns an Address* rather than an
  // ObjectSlot in order to avoid #including slots.h in this header file.
  inline Address* GetLocation(int index) {
    DCHECK(index >= 0 && index < size_);
    return &blocks_[index >> kShift][index & kMask];
  }

  int size_ = 0;
  std::vector<Address*> blocks_;
  std::vector<int> young_node_indices_;

  DISALLOW_COPY_AND_ASSIGN(EternalHandles);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_GLOBAL_HANDLES_H_
