// Copyright 2007-2008 the V8 project authors. All rights reserved.
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

#include "list-inl.h"

namespace v8 {
namespace internal {

// Structure for tracking global handles.
// A single list keeps all the allocated global handles.
// Destroyed handles stay in the list but is added to the free list.
// At GC the destroyed global handles are removed from the free list
// and deallocated.

// Callback function on handling weak global handles.
// typedef bool (*WeakSlotCallback)(Object** pointer);

// An object group is treated like a single JS object: if one of object in
// the group is alive, all objects in the same group are considered alive.
// An object group is used to simulate object relationship in a DOM tree.
class ObjectGroup : public Malloced {
 public:
  ObjectGroup() : objects_(4) {}
  explicit ObjectGroup(size_t capacity)
      : objects_(static_cast<int>(capacity)) { }

  List<Object**> objects_;
};


typedef void (*WeakReferenceGuest)(Object* object, void* parameter);

class GlobalHandles : public AllStatic {
 public:
  // Creates a new global handle that is alive until Destroy is called.
  static Handle<Object> Create(Object* value);

  // Destroy a global handle.
  static void Destroy(Object** location);

  // Make the global handle weak and set the callback parameter for the
  // handle.  When the garbage collector recognizes that only weak global
  // handles point to an object the handles are cleared and the callback
  // function is invoked (for each handle) with the handle and corresponding
  // parameter as arguments.  Note: cleared means set to Smi::FromInt(0). The
  // reason is that Smi::FromInt(0) does not change during garage collection.
  static void MakeWeak(Object** location,
                       void* parameter,
                       WeakReferenceCallback callback);

  // Returns the current number of weak handles.
  static int NumberOfWeakHandles() { return number_of_weak_handles_; }

  static void RecordStats(HeapStats* stats);

  // Returns the current number of weak handles to global objects.
  // These handles are also included in NumberOfWeakHandles().
  static int NumberOfGlobalObjectWeakHandles() {
    return number_of_global_object_weak_handles_;
  }

  // Clear the weakness of a global handle.
  static void ClearWeakness(Object** location);

  // Tells whether global handle is near death.
  static bool IsNearDeath(Object** location);

  // Tells whether global handle is weak.
  static bool IsWeak(Object** location);

  // Process pending weak handles.
  static void PostGarbageCollectionProcessing();

  // Iterates over all strong handles.
  static void IterateStrongRoots(ObjectVisitor* v);

  // Iterates over all handles.
  static void IterateAllRoots(ObjectVisitor* v);

  // Iterates over all weak roots in heap.
  static void IterateWeakRoots(ObjectVisitor* v);

  // Iterates over weak roots that are bound to a given callback.
  static void IterateWeakRoots(WeakReferenceGuest f,
                               WeakReferenceCallback callback);

  // Find all weak handles satisfying the callback predicate, mark
  // them as pending.
  static void IdentifyWeakHandles(WeakSlotCallback f);

  // Add an object group.
  // Should only used in GC callback function before a collection.
  // All groups are destroyed after a mark-compact collection.
  static void AddGroup(Object*** handles, size_t length);

  // Returns the object groups.
  static List<ObjectGroup*>* ObjectGroups();

  // Remove bags, this should only happen after GC.
  static void RemoveObjectGroups();

  // Tear down the global handle structure.
  static void TearDown();

#ifdef DEBUG
  static void PrintStats();
  static void Print();
#endif
  class Pool;
 private:
  // Internal node structure, one for each global handle.
  class Node;

  // Field always containing the number of weak and near-death handles.
  static int number_of_weak_handles_;

  // Field always containing the number of weak and near-death handles
  // to global objects.  These objects are also included in
  // number_of_weak_handles_.
  static int number_of_global_object_weak_handles_;

  // Global handles are kept in a single linked list pointed to by head_.
  static Node* head_;
  static Node* head() { return head_; }
  static void set_head(Node* value) { head_ = value; }

  // Free list for DESTROYED global handles not yet deallocated.
  static Node* first_free_;
  static Node* first_free() { return first_free_; }
  static void set_first_free(Node* value) { first_free_ = value; }

  // List of deallocated nodes.
  // Deallocated nodes form a prefix of all the nodes and
  // |first_deallocated| points to last deallocated node before
  // |head|.  Those deallocated nodes are additionally linked
  // by |next_free|:
  //                                    1st deallocated  head
  //                                           |          |
  //                                           V          V
  //    node          node        ...         node       node
  //      .next      -> .next ->                .next ->
  //   <- .next_free <- .next_free           <- .next_free
  static Node* first_deallocated_;
  static Node* first_deallocated() { return first_deallocated_; }
  static void set_first_deallocated(Node* value) {
    first_deallocated_ = value;
  }
};


} }  // namespace v8::internal

#endif  // V8_GLOBAL_HANDLES_H_
