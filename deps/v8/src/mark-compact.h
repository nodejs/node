// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_MARK_COMPACT_H_
#define V8_MARK_COMPACT_H_

namespace v8 {
namespace internal {

// Callback function, returns whether an object is alive. The heap size
// of the object is returned in size. It optionally updates the offset
// to the first live object in the page (only used for old and map objects).
typedef bool (*IsAliveFunction)(HeapObject* obj, int* size, int* offset);

// Callback function for non-live blocks in the old generation.
// If add_to_freelist is false then just accounting stats are updated and
// no attempt to add area to free list is made.
typedef void (*DeallocateFunction)(Address start,
                                   int size_in_bytes,
                                   bool add_to_freelist);


// Forward declarations.
class RootMarkingVisitor;
class MarkingVisitor;


// -------------------------------------------------------------------------
// Mark-Compact collector
//
// All methods are static.

class MarkCompactCollector: public AllStatic {
 public:
  // Type of functions to compute forwarding addresses of objects in
  // compacted spaces.  Given an object and its size, return a (non-failure)
  // Object* that will be the object after forwarding.  There is a separate
  // allocation function for each (compactable) space based on the location
  // of the object before compaction.
  typedef Object* (*AllocationFunction)(HeapObject* object, int object_size);

  // Type of functions to encode the forwarding address for an object.
  // Given the object, its size, and the new (non-failure) object it will be
  // forwarded to, encode the forwarding address.  For paged spaces, the
  // 'offset' input/output parameter contains the offset of the forwarded
  // object from the forwarding address of the previous live object in the
  // page as input, and is updated to contain the offset to be used for the
  // next live object in the same page.  For spaces using a different
  // encoding (ie, contiguous spaces), the offset parameter is ignored.
  typedef void (*EncodingFunction)(HeapObject* old_object,
                                   int object_size,
                                   Object* new_object,
                                   int* offset);

  // Type of functions to process non-live objects.
  typedef void (*ProcessNonLiveFunction)(HeapObject* object);

  // Set the global force_compaction flag, it must be called before Prepare
  // to take effect.
  static void SetForceCompaction(bool value) {
    force_compaction_ = value;
  }

  // Prepares for GC by resetting relocation info in old and map spaces and
  // choosing spaces to compact.
  static void Prepare(GCTracer* tracer);

  // Performs a global garbage collection.
  static void CollectGarbage();

  // True if the last full GC performed heap compaction.
  static bool HasCompacted() { return compacting_collection_; }

  // True after the Prepare phase if the compaction is taking place.
  static bool IsCompacting() {
#ifdef DEBUG
    // For the purposes of asserts we don't want this to keep returning true
    // after the collection is completed.
    return state_ != IDLE && compacting_collection_;
#else
    return compacting_collection_;
#endif
  }

  // The count of the number of objects left marked at the end of the last
  // completed full GC (expected to be zero).
  static int previous_marked_count() { return previous_marked_count_; }

  // During a full GC, there is a stack-allocated GCTracer that is used for
  // bookkeeping information.  Return a pointer to that tracer.
  static GCTracer* tracer() { return tracer_; }

#ifdef DEBUG
  // Checks whether performing mark-compact collection.
  static bool in_use() { return state_ > PREPARE_GC; }
#endif

  // Determine type of object and emit deletion log event.
  static void ReportDeleteIfNeeded(HeapObject* obj);

 private:
#ifdef DEBUG
  enum CollectorState {
    IDLE,
    PREPARE_GC,
    MARK_LIVE_OBJECTS,
    SWEEP_SPACES,
    ENCODE_FORWARDING_ADDRESSES,
    UPDATE_POINTERS,
    RELOCATE_OBJECTS,
    REBUILD_RSETS
  };

  // The current stage of the collector.
  static CollectorState state_;
#endif

  // Global flag that forces a compaction.
  static bool force_compaction_;

  // Global flag indicating whether spaces were compacted on the last GC.
  static bool compacting_collection_;

  // Global flag indicating whether spaces will be compacted on the next GC.
  static bool compact_on_next_gc_;

  // The number of objects left marked at the end of the last completed full
  // GC (expected to be zero).
  static int previous_marked_count_;

  // A pointer to the current stack-allocated GC tracer object during a full
  // collection (NULL before and after).
  static GCTracer* tracer_;

  // Finishes GC, performs heap verification if enabled.
  static void Finish();

  // -----------------------------------------------------------------------
  // Phase 1: Marking live objects.
  //
  //  Before: The heap has been prepared for garbage collection by
  //          MarkCompactCollector::Prepare() and is otherwise in its
  //          normal state.
  //
  //   After: Live objects are marked and non-live objects are unmarked.


  friend class RootMarkingVisitor;
  friend class MarkingVisitor;

  // Marking operations for objects reachable from roots.
  static void MarkLiveObjects();

  static void MarkUnmarkedObject(HeapObject* obj);

  static inline void MarkObject(HeapObject* obj) {
    if (!obj->IsMarked()) MarkUnmarkedObject(obj);
  }

  static inline void SetMark(HeapObject* obj) {
    tracer_->increment_marked_count();
#ifdef DEBUG
    UpdateLiveObjectCount(obj);
#endif
    obj->SetMark();
  }

  // Creates back pointers for all map transitions, stores them in
  // the prototype field.  The original prototype pointers are restored
  // in ClearNonLiveTransitions().  All JSObject maps
  // connected by map transitions have the same prototype object, which
  // is why we can use this field temporarily for back pointers.
  static void CreateBackPointers();

  // Mark a Map and its DescriptorArray together, skipping transitions.
  static void MarkMapContents(Map* map);
  static void MarkDescriptorArray(DescriptorArray* descriptors);

  // Mark the heap roots and all objects reachable from them.
  static void MarkRoots(RootMarkingVisitor* visitor);

  // Mark the symbol table specially.  References to symbols from the
  // symbol table are weak.
  static void MarkSymbolTable();

  // Mark objects in object groups that have at least one object in the
  // group marked.
  static void MarkObjectGroups();

  // Mark all objects in an object group with at least one marked
  // object, then all objects reachable from marked objects in object
  // groups, and repeat.
  static void ProcessObjectGroups(MarkingVisitor* visitor);

  // Mark objects reachable (transitively) from objects in the marking stack
  // or overflowed in the heap.
  static void ProcessMarkingStack(MarkingVisitor* visitor);

  // Mark objects reachable (transitively) from objects in the marking
  // stack.  This function empties the marking stack, but may leave
  // overflowed objects in the heap, in which case the marking stack's
  // overflow flag will be set.
  static void EmptyMarkingStack(MarkingVisitor* visitor);

  // Refill the marking stack with overflowed objects from the heap.  This
  // function either leaves the marking stack full or clears the overflow
  // flag on the marking stack.
  static void RefillMarkingStack();

  // Callback function for telling whether the object *p is an unmarked
  // heap object.
  static bool IsUnmarkedHeapObject(Object** p);

#ifdef DEBUG
  static void UpdateLiveObjectCount(HeapObject* obj);
#endif

  // We sweep the large object space in the same way whether we are
  // compacting or not, because the large object space is never compacted.
  static void SweepLargeObjectSpace();

  // Test whether a (possibly marked) object is a Map.
  static inline bool SafeIsMap(HeapObject* object);

  // Map transitions from a live map to a dead map must be killed.
  // We replace them with a null descriptor, with the same key.
  static void ClearNonLiveTransitions();

  // -----------------------------------------------------------------------
  // Phase 2: Sweeping to clear mark bits and free non-live objects for
  // a non-compacting collection, or else computing and encoding
  // forwarding addresses for a compacting collection.
  //
  //  Before: Live objects are marked and non-live objects are unmarked.
  //
  //   After: (Non-compacting collection.)  Live objects are unmarked,
  //          non-live regions have been added to their space's free
  //          list.
  //
  //   After: (Compacting collection.)  The forwarding address of live
  //          objects in the paged spaces is encoded in their map word
  //          along with their (non-forwarded) map pointer.
  //
  //          The forwarding address of live objects in the new space is
  //          written to their map word's offset in the inactive
  //          semispace.
  //
  //          Bookkeeping data is written to the remembered-set are of
  //          eached paged-space page that contains live objects after
  //          compaction:
  //
  //          The 3rd word of the page (first word of the remembered
  //          set) contains the relocation top address, the address of
  //          the first word after the end of the last live object in
  //          the page after compaction.
  //
  //          The 4th word contains the zero-based index of the page in
  //          its space.  This word is only used for map space pages, in
  //          order to encode the map addresses in 21 bits to free 11
  //          bits per map word for the forwarding address.
  //
  //          The 5th word contains the (nonencoded) forwarding address
  //          of the first live object in the page.
  //
  //          In both the new space and the paged spaces, a linked list
  //          of live regions is constructructed (linked through
  //          pointers in the non-live region immediately following each
  //          live region) to speed further passes of the collector.

  // Encodes forwarding addresses of objects in compactable parts of the
  // heap.
  static void EncodeForwardingAddresses();

  // Encodes the forwarding addresses of objects in new space.
  static void EncodeForwardingAddressesInNewSpace();

  // Function template to encode the forwarding addresses of objects in
  // paged spaces, parameterized by allocation and non-live processing
  // functions.
  template<AllocationFunction Alloc, ProcessNonLiveFunction ProcessNonLive>
  static void EncodeForwardingAddressesInPagedSpace(PagedSpace* space);

  // Iterates live objects in a space, passes live objects
  // to a callback function which returns the heap size of the object.
  // Returns the number of live objects iterated.
  static int IterateLiveObjects(NewSpace* space, HeapObjectCallback size_f);
  static int IterateLiveObjects(PagedSpace* space, HeapObjectCallback size_f);

  // Iterates the live objects between a range of addresses, returning the
  // number of live objects.
  static int IterateLiveObjectsInRange(Address start, Address end,
                                       HeapObjectCallback size_func);

  // Callback functions for deallocating non-live blocks in the old
  // generation.
  static void DeallocateOldPointerBlock(Address start,
                                        int size_in_bytes,
                                        bool add_to_freelist);

  static void DeallocateOldDataBlock(Address start,
                                     int size_in_bytes,
                                     bool add_to_freelist);

  static void DeallocateCodeBlock(Address start,
                                  int size_in_bytes,
                                  bool add_to_freelist);

  static void DeallocateMapBlock(Address start,
                                 int size_in_bytes,
                                 bool add_to_freelist);

  static void DeallocateCellBlock(Address start,
                                  int size_in_bytes,
                                  bool add_to_freelist);

  // If we are not compacting the heap, we simply sweep the spaces except
  // for the large object space, clearing mark bits and adding unmarked
  // regions to each space's free list.
  static void SweepSpaces();

  // -----------------------------------------------------------------------
  // Phase 3: Updating pointers in live objects.
  //
  //  Before: Same as after phase 2 (compacting collection).
  //
  //   After: All pointers in live objects, including encoded map
  //          pointers, are updated to point to their target's new
  //          location.  The remembered set area of each paged-space
  //          page containing live objects still contains bookkeeping
  //          information.

  friend class UpdatingVisitor;  // helper for updating visited objects

  // Updates pointers in all spaces.
  static void UpdatePointers();

  // Updates pointers in an object in new space.
  // Returns the heap size of the object.
  static int UpdatePointersInNewObject(HeapObject* obj);

  // Updates pointers in an object in old spaces.
  // Returns the heap size of the object.
  static int UpdatePointersInOldObject(HeapObject* obj);

  // Calculates the forwarding address of an object in an old space.
  static Address GetForwardingAddressInOldSpace(HeapObject* obj);

  // -----------------------------------------------------------------------
  // Phase 4: Relocating objects.
  //
  //  Before: Pointers to live objects are updated to point to their
  //          target's new location.  The remembered set area of each
  //          paged-space page containing live objects still contains
  //          bookkeeping information.
  //
  //   After: Objects have been moved to their new addresses. The
  //          remembered set area of each paged-space page containing
  //          live objects still contains bookkeeping information.

  // Relocates objects in all spaces.
  static void RelocateObjects();

  // Converts a code object's inline target to addresses, convention from
  // address to target happens in the marking phase.
  static int ConvertCodeICTargetToAddress(HeapObject* obj);

  // Relocate a map object.
  static int RelocateMapObject(HeapObject* obj);

  // Relocates an old object.
  static int RelocateOldPointerObject(HeapObject* obj);
  static int RelocateOldDataObject(HeapObject* obj);

  // Relocate a property cell object.
  static int RelocateCellObject(HeapObject* obj);

  // Helper function.
  static inline int RelocateOldNonCodeObject(HeapObject* obj,
                                             PagedSpace* space);

  // Relocates an object in the code space.
  static int RelocateCodeObject(HeapObject* obj);

  // Copy a new object.
  static int RelocateNewObject(HeapObject* obj);

  // -----------------------------------------------------------------------
  // Phase 5: Rebuilding remembered sets.
  //
  //  Before: The heap is in a normal state except that remembered sets
  //          in the paged spaces are not correct.
  //
  //   After: The heap is in a normal state.

  // Rebuild remembered set in old and map spaces.
  static void RebuildRSets();

#ifdef DEBUG
  // -----------------------------------------------------------------------
  // Debugging variables, functions and classes
  // Counters used for debugging the marking phase of mark-compact or
  // mark-sweep collection.

  // Size of live objects in Heap::to_space_.
  static int live_young_objects_size_;

  // Size of live objects in Heap::old_pointer_space_.
  static int live_old_pointer_objects_size_;

  // Size of live objects in Heap::old_data_space_.
  static int live_old_data_objects_size_;

  // Size of live objects in Heap::code_space_.
  static int live_code_objects_size_;

  // Size of live objects in Heap::map_space_.
  static int live_map_objects_size_;

  // Size of live objects in Heap::cell_space_.
  static int live_cell_objects_size_;

  // Size of live objects in Heap::lo_space_.
  static int live_lo_objects_size_;

  // Number of live bytes in this collection.
  static int live_bytes_;

  friend class MarkObjectVisitor;
  static void VisitObject(HeapObject* obj);

  friend class UnmarkObjectVisitor;
  static void UnmarkObject(HeapObject* obj);
#endif
};


} }  // namespace v8::internal

#endif  // V8_MARK_COMPACT_H_
