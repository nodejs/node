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

#include "v8.h"

#include "execution.h"
#include "global-handles.h"
#include "ic-inl.h"
#include "mark-compact.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// MarkCompactCollector

bool MarkCompactCollector::force_compaction_ = false;
bool MarkCompactCollector::compacting_collection_ = false;
bool MarkCompactCollector::compact_on_next_gc_ = false;

int MarkCompactCollector::previous_marked_count_ = 0;
GCTracer* MarkCompactCollector::tracer_ = NULL;


#ifdef DEBUG
MarkCompactCollector::CollectorState MarkCompactCollector::state_ = IDLE;

// Counters used for debugging the marking phase of mark-compact or mark-sweep
// collection.
int MarkCompactCollector::live_bytes_ = 0;
int MarkCompactCollector::live_young_objects_size_ = 0;
int MarkCompactCollector::live_old_data_objects_size_ = 0;
int MarkCompactCollector::live_old_pointer_objects_size_ = 0;
int MarkCompactCollector::live_code_objects_size_ = 0;
int MarkCompactCollector::live_map_objects_size_ = 0;
int MarkCompactCollector::live_cell_objects_size_ = 0;
int MarkCompactCollector::live_lo_objects_size_ = 0;
#endif

void MarkCompactCollector::CollectGarbage() {
  // Make sure that Prepare() has been called. The individual steps below will
  // update the state as they proceed.
  ASSERT(state_ == PREPARE_GC);

  // Prepare has selected whether to compact the old generation or not.
  // Tell the tracer.
  if (IsCompacting()) tracer_->set_is_compacting();

  MarkLiveObjects();

  if (FLAG_collect_maps) ClearNonLiveTransitions();

  SweepLargeObjectSpace();

  if (IsCompacting()) {
    GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_COMPACT);
    EncodeForwardingAddresses();

    UpdatePointers();

    RelocateObjects();
  } else {
    SweepSpaces();
  }

  Finish();

  // Save the count of marked objects remaining after the collection and
  // null out the GC tracer.
  previous_marked_count_ = tracer_->marked_count();
  ASSERT(previous_marked_count_ == 0);
  tracer_ = NULL;
}


void MarkCompactCollector::Prepare(GCTracer* tracer) {
  // Rather than passing the tracer around we stash it in a static member
  // variable.
  tracer_ = tracer;

#ifdef DEBUG
  ASSERT(state_ == IDLE);
  state_ = PREPARE_GC;
#endif
  ASSERT(!FLAG_always_compact || !FLAG_never_compact);

  compacting_collection_ =
      FLAG_always_compact || force_compaction_ || compact_on_next_gc_;
  compact_on_next_gc_ = false;

  if (FLAG_never_compact) compacting_collection_ = false;
  if (!Heap::map_space()->MapPointersEncodable())
      compacting_collection_ = false;
  if (FLAG_collect_maps) CreateBackPointers();

  PagedSpaces spaces;
  for (PagedSpace* space = spaces.next();
       space != NULL; space = spaces.next()) {
    space->PrepareForMarkCompact(compacting_collection_);
  }

#ifdef DEBUG
  live_bytes_ = 0;
  live_young_objects_size_ = 0;
  live_old_pointer_objects_size_ = 0;
  live_old_data_objects_size_ = 0;
  live_code_objects_size_ = 0;
  live_map_objects_size_ = 0;
  live_cell_objects_size_ = 0;
  live_lo_objects_size_ = 0;
#endif
}


void MarkCompactCollector::Finish() {
#ifdef DEBUG
  ASSERT(state_ == SWEEP_SPACES || state_ == RELOCATE_OBJECTS);
  state_ = IDLE;
#endif
  // The stub cache is not traversed during GC; clear the cache to
  // force lazy re-initialization of it. This must be done after the
  // GC, because it relies on the new address of certain old space
  // objects (empty string, illegal builtin).
  StubCache::Clear();

  ExternalStringTable::CleanUp();

  // If we've just compacted old space there's no reason to check the
  // fragmentation limit. Just return.
  if (HasCompacted()) return;

  // We compact the old generation on the next GC if it has gotten too
  // fragmented (ie, we could recover an expected amount of space by
  // reclaiming the waste and free list blocks).
  static const int kFragmentationLimit = 15;        // Percent.
  static const int kFragmentationAllowed = 1 * MB;  // Absolute.
  int old_gen_recoverable = 0;
  int old_gen_used = 0;

  OldSpaces spaces;
  for (OldSpace* space = spaces.next(); space != NULL; space = spaces.next()) {
    old_gen_recoverable += space->Waste() + space->AvailableFree();
    old_gen_used += space->Size();
  }

  int old_gen_fragmentation =
      static_cast<int>((old_gen_recoverable * 100.0) / old_gen_used);
  if (old_gen_fragmentation > kFragmentationLimit &&
      old_gen_recoverable > kFragmentationAllowed) {
    compact_on_next_gc_ = true;
  }
}


// -------------------------------------------------------------------------
// Phase 1: tracing and marking live objects.
//   before: all objects are in normal state.
//   after: a live object's map pointer is marked as '00'.

// Marking all live objects in the heap as part of mark-sweep or mark-compact
// collection.  Before marking, all objects are in their normal state.  After
// marking, live objects' map pointers are marked indicating that the object
// has been found reachable.
//
// The marking algorithm is a (mostly) depth-first (because of possible stack
// overflow) traversal of the graph of objects reachable from the roots.  It
// uses an explicit stack of pointers rather than recursion.  The young
// generation's inactive ('from') space is used as a marking stack.  The
// objects in the marking stack are the ones that have been reached and marked
// but their children have not yet been visited.
//
// The marking stack can overflow during traversal.  In that case, we set an
// overflow flag.  When the overflow flag is set, we continue marking objects
// reachable from the objects on the marking stack, but no longer push them on
// the marking stack.  Instead, we mark them as both marked and overflowed.
// When the stack is in the overflowed state, objects marked as overflowed
// have been reached and marked but their children have not been visited yet.
// After emptying the marking stack, we clear the overflow flag and traverse
// the heap looking for objects marked as overflowed, push them on the stack,
// and continue with marking.  This process repeats until all reachable
// objects have been marked.

static MarkingStack marking_stack;


static inline HeapObject* ShortCircuitConsString(Object** p) {
  // Optimization: If the heap object pointed to by p is a non-symbol
  // cons string whose right substring is Heap::empty_string, update
  // it in place to its left substring.  Return the updated value.
  //
  // Here we assume that if we change *p, we replace it with a heap object
  // (ie, the left substring of a cons string is always a heap object).
  //
  // The check performed is:
  //   object->IsConsString() && !object->IsSymbol() &&
  //   (ConsString::cast(object)->second() == Heap::empty_string())
  // except the maps for the object and its possible substrings might be
  // marked.
  HeapObject* object = HeapObject::cast(*p);
  MapWord map_word = object->map_word();
  map_word.ClearMark();
  InstanceType type = map_word.ToMap()->instance_type();
  if ((type & kShortcutTypeMask) != kShortcutTypeTag) return object;

  Object* second = reinterpret_cast<ConsString*>(object)->unchecked_second();
  if (second != Heap::raw_unchecked_empty_string()) {
    return object;
  }

  // Since we don't have the object's start, it is impossible to update the
  // page dirty marks. Therefore, we only replace the string with its left
  // substring when page dirty marks do not change.
  Object* first = reinterpret_cast<ConsString*>(object)->unchecked_first();
  if (!Heap::InNewSpace(object) && Heap::InNewSpace(first)) return object;

  *p = first;
  return HeapObject::cast(first);
}


// Helper class for marking pointers in HeapObjects.
class MarkingVisitor : public ObjectVisitor {
 public:
  void VisitPointer(Object** p) {
    MarkObjectByPointer(p);
  }

  void VisitPointers(Object** start, Object** end) {
    // Mark all objects pointed to in [start, end).
    const int kMinRangeForMarkingRecursion = 64;
    if (end - start >= kMinRangeForMarkingRecursion) {
      if (VisitUnmarkedObjects(start, end)) return;
      // We are close to a stack overflow, so just mark the objects.
    }
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

  void VisitCodeTarget(RelocInfo* rinfo) {
    ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
    Code* code = Code::GetCodeFromTargetAddress(rinfo->target_address());
    if (FLAG_cleanup_ics_at_gc && code->is_inline_cache_stub()) {
      IC::Clear(rinfo->pc());
      // Please note targets for cleared inline cached do not have to be
      // marked since they are contained in Heap::non_monomorphic_cache().
    } else {
      MarkCompactCollector::MarkObject(code);
    }
  }

  void VisitDebugTarget(RelocInfo* rinfo) {
    ASSERT((RelocInfo::IsJSReturn(rinfo->rmode()) &&
            rinfo->IsPatchedReturnSequence()) ||
           (RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
            rinfo->IsPatchedDebugBreakSlotSequence()));
    HeapObject* code = Code::GetCodeFromTargetAddress(rinfo->call_address());
    MarkCompactCollector::MarkObject(code);
  }

 private:
  // Mark object pointed to by p.
  void MarkObjectByPointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;
    HeapObject* object = ShortCircuitConsString(p);
    MarkCompactCollector::MarkObject(object);
  }

  // Tells whether the mark sweep collection will perform compaction.
  bool IsCompacting() { return MarkCompactCollector::IsCompacting(); }

  // Visit an unmarked object.
  void VisitUnmarkedObject(HeapObject* obj) {
#ifdef DEBUG
    ASSERT(Heap::Contains(obj));
    ASSERT(!obj->IsMarked());
#endif
    Map* map = obj->map();
    MarkCompactCollector::SetMark(obj);
    // Mark the map pointer and the body.
    MarkCompactCollector::MarkObject(map);
    obj->IterateBody(map->instance_type(), obj->SizeFromMap(map), this);
  }

  // Visit all unmarked objects pointed to by [start, end).
  // Returns false if the operation fails (lack of stack space).
  inline bool VisitUnmarkedObjects(Object** start, Object** end) {
    // Return false is we are close to the stack limit.
    StackLimitCheck check;
    if (check.HasOverflowed()) return false;

    // Visit the unmarked objects.
    for (Object** p = start; p < end; p++) {
      if (!(*p)->IsHeapObject()) continue;
      HeapObject* obj = HeapObject::cast(*p);
      if (obj->IsMarked()) continue;
      VisitUnmarkedObject(obj);
    }
    return true;
  }
};


// Visitor class for marking heap roots.
class RootMarkingVisitor : public ObjectVisitor {
 public:
  void VisitPointer(Object** p) {
    MarkObjectByPointer(p);
  }

  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

  MarkingVisitor* stack_visitor() { return &stack_visitor_; }

 private:
  MarkingVisitor stack_visitor_;

  void MarkObjectByPointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;

    // Replace flat cons strings in place.
    HeapObject* object = ShortCircuitConsString(p);
    if (object->IsMarked()) return;

    Map* map = object->map();
    // Mark the object.
    MarkCompactCollector::SetMark(object);
    // Mark the map pointer and body, and push them on the marking stack.
    MarkCompactCollector::MarkObject(map);
    object->IterateBody(map->instance_type(), object->SizeFromMap(map),
                        &stack_visitor_);

    // Mark all the objects reachable from the map and body.  May leave
    // overflowed objects in the heap.
    MarkCompactCollector::EmptyMarkingStack(&stack_visitor_);
  }
};


// Helper class for pruning the symbol table.
class SymbolTableCleaner : public ObjectVisitor {
 public:
  SymbolTableCleaner() : pointers_removed_(0) { }

  virtual void VisitPointers(Object** start, Object** end) {
    // Visit all HeapObject pointers in [start, end).
    for (Object** p = start; p < end; p++) {
      if ((*p)->IsHeapObject() && !HeapObject::cast(*p)->IsMarked()) {
        // Check if the symbol being pruned is an external symbol. We need to
        // delete the associated external data as this symbol is going away.

        // Since no objects have yet been moved we can safely access the map of
        // the object.
        if ((*p)->IsExternalString()) {
          Heap::FinalizeExternalString(String::cast(*p));
        }
        // Set the entry to null_value (as deleted).
        *p = Heap::raw_unchecked_null_value();
        pointers_removed_++;
      }
    }
  }

  int PointersRemoved() {
    return pointers_removed_;
  }
 private:
  int pointers_removed_;
};


void MarkCompactCollector::MarkUnmarkedObject(HeapObject* object) {
  ASSERT(!object->IsMarked());
  ASSERT(Heap::Contains(object));
  if (object->IsMap()) {
    Map* map = Map::cast(object);
    if (FLAG_cleanup_caches_in_maps_at_gc) {
      map->ClearCodeCache();
    }
    SetMark(map);
    if (FLAG_collect_maps &&
        map->instance_type() >= FIRST_JS_OBJECT_TYPE &&
        map->instance_type() <= JS_FUNCTION_TYPE) {
      MarkMapContents(map);
    } else {
      marking_stack.Push(map);
    }
  } else {
    SetMark(object);
    marking_stack.Push(object);
  }
}


void MarkCompactCollector::MarkMapContents(Map* map) {
  MarkDescriptorArray(reinterpret_cast<DescriptorArray*>(
      *HeapObject::RawField(map, Map::kInstanceDescriptorsOffset)));

  // Mark the Object* fields of the Map.
  // Since the descriptor array has been marked already, it is fine
  // that one of these fields contains a pointer to it.
  MarkingVisitor visitor;  // Has no state or contents.
  visitor.VisitPointers(HeapObject::RawField(map,
                                             Map::kPointerFieldsBeginOffset),
                        HeapObject::RawField(map,
                                             Map::kPointerFieldsEndOffset));
}


void MarkCompactCollector::MarkDescriptorArray(
    DescriptorArray* descriptors) {
  if (descriptors->IsMarked()) return;
  // Empty descriptor array is marked as a root before any maps are marked.
  ASSERT(descriptors != Heap::raw_unchecked_empty_descriptor_array());
  SetMark(descriptors);

  FixedArray* contents = reinterpret_cast<FixedArray*>(
      descriptors->get(DescriptorArray::kContentArrayIndex));
  ASSERT(contents->IsHeapObject());
  ASSERT(!contents->IsMarked());
  ASSERT(contents->IsFixedArray());
  ASSERT(contents->length() >= 2);
  SetMark(contents);
  // Contents contains (value, details) pairs.  If the details say
  // that the type of descriptor is MAP_TRANSITION, CONSTANT_TRANSITION,
  // or NULL_DESCRIPTOR, we don't mark the value as live.  Only for
  // type MAP_TRANSITION is the value a Object* (a Map*).
  for (int i = 0; i < contents->length(); i += 2) {
    // If the pair (value, details) at index i, i+1 is not
    // a transition or null descriptor, mark the value.
    PropertyDetails details(Smi::cast(contents->get(i + 1)));
    if (details.type() < FIRST_PHANTOM_PROPERTY_TYPE) {
      HeapObject* object = reinterpret_cast<HeapObject*>(contents->get(i));
      if (object->IsHeapObject() && !object->IsMarked()) {
        SetMark(object);
        marking_stack.Push(object);
      }
    }
  }
  // The DescriptorArray descriptors contains a pointer to its contents array,
  // but the contents array is already marked.
  marking_stack.Push(descriptors);
}


void MarkCompactCollector::CreateBackPointers() {
  HeapObjectIterator iterator(Heap::map_space());
  for (HeapObject* next_object = iterator.next();
       next_object != NULL; next_object = iterator.next()) {
    if (next_object->IsMap()) {  // Could also be ByteArray on free list.
      Map* map = Map::cast(next_object);
      if (map->instance_type() >= FIRST_JS_OBJECT_TYPE &&
          map->instance_type() <= JS_FUNCTION_TYPE) {
        map->CreateBackPointers();
      } else {
        ASSERT(map->instance_descriptors() == Heap::empty_descriptor_array());
      }
    }
  }
}


static int OverflowObjectSize(HeapObject* obj) {
  // Recover the normal map pointer, it might be marked as live and
  // overflowed.
  MapWord map_word = obj->map_word();
  map_word.ClearMark();
  map_word.ClearOverflow();
  return obj->SizeFromMap(map_word.ToMap());
}


// Fill the marking stack with overflowed objects returned by the given
// iterator.  Stop when the marking stack is filled or the end of the space
// is reached, whichever comes first.
template<class T>
static void ScanOverflowedObjects(T* it) {
  // The caller should ensure that the marking stack is initially not full,
  // so that we don't waste effort pointlessly scanning for objects.
  ASSERT(!marking_stack.is_full());

  for (HeapObject* object = it->next(); object != NULL; object = it->next()) {
    if (object->IsOverflowed()) {
      object->ClearOverflow();
      ASSERT(object->IsMarked());
      ASSERT(Heap::Contains(object));
      marking_stack.Push(object);
      if (marking_stack.is_full()) return;
    }
  }
}


bool MarkCompactCollector::IsUnmarkedHeapObject(Object** p) {
  return (*p)->IsHeapObject() && !HeapObject::cast(*p)->IsMarked();
}


void MarkCompactCollector::MarkSymbolTable() {
  SymbolTable* symbol_table = Heap::raw_unchecked_symbol_table();
  // Mark the symbol table itself.
  SetMark(symbol_table);
  // Explicitly mark the prefix.
  MarkingVisitor marker;
  symbol_table->IteratePrefix(&marker);
  ProcessMarkingStack(&marker);
}


void MarkCompactCollector::MarkRoots(RootMarkingVisitor* visitor) {
  // Mark the heap roots including global variables, stack variables,
  // etc., and all objects reachable from them.
  Heap::IterateStrongRoots(visitor, VISIT_ONLY_STRONG);

  // Handle the symbol table specially.
  MarkSymbolTable();

  // There may be overflowed objects in the heap.  Visit them now.
  while (marking_stack.overflowed()) {
    RefillMarkingStack();
    EmptyMarkingStack(visitor->stack_visitor());
  }
}


void MarkCompactCollector::MarkObjectGroups() {
  List<ObjectGroup*>* object_groups = GlobalHandles::ObjectGroups();

  for (int i = 0; i < object_groups->length(); i++) {
    ObjectGroup* entry = object_groups->at(i);
    if (entry == NULL) continue;

    List<Object**>& objects = entry->objects_;
    bool group_marked = false;
    for (int j = 0; j < objects.length(); j++) {
      Object* object = *objects[j];
      if (object->IsHeapObject() && HeapObject::cast(object)->IsMarked()) {
        group_marked = true;
        break;
      }
    }

    if (!group_marked) continue;

    // An object in the group is marked, so mark as gray all white heap
    // objects in the group.
    for (int j = 0; j < objects.length(); ++j) {
      if ((*objects[j])->IsHeapObject()) {
        MarkObject(HeapObject::cast(*objects[j]));
      }
    }
    // Once the entire group has been colored gray, set the object group
    // to NULL so it won't be processed again.
    delete object_groups->at(i);
    object_groups->at(i) = NULL;
  }
}


// Mark all objects reachable from the objects on the marking stack.
// Before: the marking stack contains zero or more heap object pointers.
// After: the marking stack is empty, and all objects reachable from the
// marking stack have been marked, or are overflowed in the heap.
void MarkCompactCollector::EmptyMarkingStack(MarkingVisitor* visitor) {
  while (!marking_stack.is_empty()) {
    HeapObject* object = marking_stack.Pop();
    ASSERT(object->IsHeapObject());
    ASSERT(Heap::Contains(object));
    ASSERT(object->IsMarked());
    ASSERT(!object->IsOverflowed());

    // Because the object is marked, we have to recover the original map
    // pointer and use it to mark the object's body.
    MapWord map_word = object->map_word();
    map_word.ClearMark();
    Map* map = map_word.ToMap();
    MarkObject(map);
    object->IterateBody(map->instance_type(), object->SizeFromMap(map),
                        visitor);
  }
}


// Sweep the heap for overflowed objects, clear their overflow bits, and
// push them on the marking stack.  Stop early if the marking stack fills
// before sweeping completes.  If sweeping completes, there are no remaining
// overflowed objects in the heap so the overflow flag on the markings stack
// is cleared.
void MarkCompactCollector::RefillMarkingStack() {
  ASSERT(marking_stack.overflowed());

  SemiSpaceIterator new_it(Heap::new_space(), &OverflowObjectSize);
  ScanOverflowedObjects(&new_it);
  if (marking_stack.is_full()) return;

  HeapObjectIterator old_pointer_it(Heap::old_pointer_space(),
                                    &OverflowObjectSize);
  ScanOverflowedObjects(&old_pointer_it);
  if (marking_stack.is_full()) return;

  HeapObjectIterator old_data_it(Heap::old_data_space(), &OverflowObjectSize);
  ScanOverflowedObjects(&old_data_it);
  if (marking_stack.is_full()) return;

  HeapObjectIterator code_it(Heap::code_space(), &OverflowObjectSize);
  ScanOverflowedObjects(&code_it);
  if (marking_stack.is_full()) return;

  HeapObjectIterator map_it(Heap::map_space(), &OverflowObjectSize);
  ScanOverflowedObjects(&map_it);
  if (marking_stack.is_full()) return;

  HeapObjectIterator cell_it(Heap::cell_space(), &OverflowObjectSize);
  ScanOverflowedObjects(&cell_it);
  if (marking_stack.is_full()) return;

  LargeObjectIterator lo_it(Heap::lo_space(), &OverflowObjectSize);
  ScanOverflowedObjects(&lo_it);
  if (marking_stack.is_full()) return;

  marking_stack.clear_overflowed();
}


// Mark all objects reachable (transitively) from objects on the marking
// stack.  Before: the marking stack contains zero or more heap object
// pointers.  After: the marking stack is empty and there are no overflowed
// objects in the heap.
void MarkCompactCollector::ProcessMarkingStack(MarkingVisitor* visitor) {
  EmptyMarkingStack(visitor);
  while (marking_stack.overflowed()) {
    RefillMarkingStack();
    EmptyMarkingStack(visitor);
  }
}


void MarkCompactCollector::ProcessObjectGroups(MarkingVisitor* visitor) {
  bool work_to_do = true;
  ASSERT(marking_stack.is_empty());
  while (work_to_do) {
    MarkObjectGroups();
    work_to_do = !marking_stack.is_empty();
    ProcessMarkingStack(visitor);
  }
}


void MarkCompactCollector::MarkLiveObjects() {
  GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_MARK);
#ifdef DEBUG
  ASSERT(state_ == PREPARE_GC);
  state_ = MARK_LIVE_OBJECTS;
#endif
  // The to space contains live objects, the from space is used as a marking
  // stack.
  marking_stack.Initialize(Heap::new_space()->FromSpaceLow(),
                           Heap::new_space()->FromSpaceHigh());

  ASSERT(!marking_stack.overflowed());

  RootMarkingVisitor root_visitor;
  MarkRoots(&root_visitor);

  // The objects reachable from the roots are marked, yet unreachable
  // objects are unmarked.  Mark objects reachable from object groups
  // containing at least one marked object, and continue until no new
  // objects are reachable from the object groups.
  ProcessObjectGroups(root_visitor.stack_visitor());

  // The objects reachable from the roots or object groups are marked,
  // yet unreachable objects are unmarked.  Mark objects reachable
  // only from weak global handles.
  //
  // First we identify nonlive weak handles and mark them as pending
  // destruction.
  GlobalHandles::IdentifyWeakHandles(&IsUnmarkedHeapObject);
  // Then we mark the objects and process the transitive closure.
  GlobalHandles::IterateWeakRoots(&root_visitor);
  while (marking_stack.overflowed()) {
    RefillMarkingStack();
    EmptyMarkingStack(root_visitor.stack_visitor());
  }

  // Repeat the object groups to mark unmarked groups reachable from the
  // weak roots.
  ProcessObjectGroups(root_visitor.stack_visitor());

  // Prune the symbol table removing all symbols only pointed to by the
  // symbol table.  Cannot use symbol_table() here because the symbol
  // table is marked.
  SymbolTable* symbol_table = Heap::raw_unchecked_symbol_table();
  SymbolTableCleaner v;
  symbol_table->IterateElements(&v);
  symbol_table->ElementsRemoved(v.PointersRemoved());
  ExternalStringTable::Iterate(&v);
  ExternalStringTable::CleanUp();

  // Remove object groups after marking phase.
  GlobalHandles::RemoveObjectGroups();
}


static int CountMarkedCallback(HeapObject* obj) {
  MapWord map_word = obj->map_word();
  map_word.ClearMark();
  return obj->SizeFromMap(map_word.ToMap());
}


#ifdef DEBUG
void MarkCompactCollector::UpdateLiveObjectCount(HeapObject* obj) {
  live_bytes_ += obj->Size();
  if (Heap::new_space()->Contains(obj)) {
    live_young_objects_size_ += obj->Size();
  } else if (Heap::map_space()->Contains(obj)) {
    ASSERT(obj->IsMap());
    live_map_objects_size_ += obj->Size();
  } else if (Heap::cell_space()->Contains(obj)) {
    ASSERT(obj->IsJSGlobalPropertyCell());
    live_cell_objects_size_ += obj->Size();
  } else if (Heap::old_pointer_space()->Contains(obj)) {
    live_old_pointer_objects_size_ += obj->Size();
  } else if (Heap::old_data_space()->Contains(obj)) {
    live_old_data_objects_size_ += obj->Size();
  } else if (Heap::code_space()->Contains(obj)) {
    live_code_objects_size_ += obj->Size();
  } else if (Heap::lo_space()->Contains(obj)) {
    live_lo_objects_size_ += obj->Size();
  } else {
    UNREACHABLE();
  }
}
#endif  // DEBUG


void MarkCompactCollector::SweepLargeObjectSpace() {
#ifdef DEBUG
  ASSERT(state_ == MARK_LIVE_OBJECTS);
  state_ =
      compacting_collection_ ? ENCODE_FORWARDING_ADDRESSES : SWEEP_SPACES;
#endif
  // Deallocate unmarked objects and clear marked bits for marked objects.
  Heap::lo_space()->FreeUnmarkedObjects();
}


// Safe to use during marking phase only.
bool MarkCompactCollector::SafeIsMap(HeapObject* object) {
  MapWord metamap = object->map_word();
  metamap.ClearMark();
  return metamap.ToMap()->instance_type() == MAP_TYPE;
}


void MarkCompactCollector::ClearNonLiveTransitions() {
  HeapObjectIterator map_iterator(Heap::map_space(), &CountMarkedCallback);
  // Iterate over the map space, setting map transitions that go from
  // a marked map to an unmarked map to null transitions.  At the same time,
  // set all the prototype fields of maps back to their original value,
  // dropping the back pointers temporarily stored in the prototype field.
  // Setting the prototype field requires following the linked list of
  // back pointers, reversing them all at once.  This allows us to find
  // those maps with map transitions that need to be nulled, and only
  // scan the descriptor arrays of those maps, not all maps.
  // All of these actions are carried out only on maps of JSObjects
  // and related subtypes.
  for (HeapObject* obj = map_iterator.next();
       obj != NULL; obj = map_iterator.next()) {
    Map* map = reinterpret_cast<Map*>(obj);
    if (!map->IsMarked() && map->IsByteArray()) continue;

    ASSERT(SafeIsMap(map));
    // Only JSObject and subtypes have map transitions and back pointers.
    if (map->instance_type() < FIRST_JS_OBJECT_TYPE) continue;
    if (map->instance_type() > JS_FUNCTION_TYPE) continue;
    // Follow the chain of back pointers to find the prototype.
    Map* current = map;
    while (SafeIsMap(current)) {
      current = reinterpret_cast<Map*>(current->prototype());
      ASSERT(current->IsHeapObject());
    }
    Object* real_prototype = current;

    // Follow back pointers, setting them to prototype,
    // clearing map transitions when necessary.
    current = map;
    bool on_dead_path = !current->IsMarked();
    Object* next;
    while (SafeIsMap(current)) {
      next = current->prototype();
      // There should never be a dead map above a live map.
      ASSERT(on_dead_path || current->IsMarked());

      // A live map above a dead map indicates a dead transition.
      // This test will always be false on the first iteration.
      if (on_dead_path && current->IsMarked()) {
        on_dead_path = false;
        current->ClearNonLiveTransitions(real_prototype);
      }
      *HeapObject::RawField(current, Map::kPrototypeOffset) =
          real_prototype;
      current = reinterpret_cast<Map*>(next);
    }
  }
}

// -------------------------------------------------------------------------
// Phase 2: Encode forwarding addresses.
// When compacting, forwarding addresses for objects in old space and map
// space are encoded in their map pointer word (along with an encoding of
// their map pointers).
//
// The excact encoding is described in the comments for class MapWord in
// objects.h.
//
// An address range [start, end) can have both live and non-live objects.
// Maximal non-live regions are marked so they can be skipped on subsequent
// sweeps of the heap.  A distinguished map-pointer encoding is used to mark
// free regions of one-word size (in which case the next word is the start
// of a live object).  A second distinguished map-pointer encoding is used
// to mark free regions larger than one word, and the size of the free
// region (including the first word) is written to the second word of the
// region.
//
// Any valid map page offset must lie in the object area of the page, so map
// page offsets less than Page::kObjectStartOffset are invalid.  We use a
// pair of distinguished invalid map encodings (for single word and multiple
// words) to indicate free regions in the page found during computation of
// forwarding addresses and skipped over in subsequent sweeps.
static const uint32_t kSingleFreeEncoding = 0;
static const uint32_t kMultiFreeEncoding = 1;


// Encode a free region, defined by the given start address and size, in the
// first word or two of the region.
void EncodeFreeRegion(Address free_start, int free_size) {
  ASSERT(free_size >= kIntSize);
  if (free_size == kIntSize) {
    Memory::uint32_at(free_start) = kSingleFreeEncoding;
  } else {
    ASSERT(free_size >= 2 * kIntSize);
    Memory::uint32_at(free_start) = kMultiFreeEncoding;
    Memory::int_at(free_start + kIntSize) = free_size;
  }

#ifdef DEBUG
  // Zap the body of the free region.
  if (FLAG_enable_slow_asserts) {
    for (int offset = 2 * kIntSize;
         offset < free_size;
         offset += kPointerSize) {
      Memory::Address_at(free_start + offset) = kZapValue;
    }
  }
#endif
}


// Try to promote all objects in new space.  Heap numbers and sequential
// strings are promoted to the code space, large objects to large object space,
// and all others to the old space.
inline Object* MCAllocateFromNewSpace(HeapObject* object, int object_size) {
  Object* forwarded;
  if (object_size > Heap::MaxObjectSizeInPagedSpace()) {
    forwarded = Failure::Exception();
  } else {
    OldSpace* target_space = Heap::TargetSpace(object);
    ASSERT(target_space == Heap::old_pointer_space() ||
           target_space == Heap::old_data_space());
    forwarded = target_space->MCAllocateRaw(object_size);
  }
  if (forwarded->IsFailure()) {
    forwarded = Heap::new_space()->MCAllocateRaw(object_size);
  }
  return forwarded;
}


// Allocation functions for the paged spaces call the space's MCAllocateRaw.
inline Object* MCAllocateFromOldPointerSpace(HeapObject* ignore,
                                             int object_size) {
  return Heap::old_pointer_space()->MCAllocateRaw(object_size);
}


inline Object* MCAllocateFromOldDataSpace(HeapObject* ignore, int object_size) {
  return Heap::old_data_space()->MCAllocateRaw(object_size);
}


inline Object* MCAllocateFromCodeSpace(HeapObject* ignore, int object_size) {
  return Heap::code_space()->MCAllocateRaw(object_size);
}


inline Object* MCAllocateFromMapSpace(HeapObject* ignore, int object_size) {
  return Heap::map_space()->MCAllocateRaw(object_size);
}


inline Object* MCAllocateFromCellSpace(HeapObject* ignore, int object_size) {
  return Heap::cell_space()->MCAllocateRaw(object_size);
}


// The forwarding address is encoded at the same offset as the current
// to-space object, but in from space.
inline void EncodeForwardingAddressInNewSpace(HeapObject* old_object,
                                              int object_size,
                                              Object* new_object,
                                              int* ignored) {
  int offset =
      Heap::new_space()->ToSpaceOffsetForAddress(old_object->address());
  Memory::Address_at(Heap::new_space()->FromSpaceLow() + offset) =
      HeapObject::cast(new_object)->address();
}


// The forwarding address is encoded in the map pointer of the object as an
// offset (in terms of live bytes) from the address of the first live object
// in the page.
inline void EncodeForwardingAddressInPagedSpace(HeapObject* old_object,
                                                int object_size,
                                                Object* new_object,
                                                int* offset) {
  // Record the forwarding address of the first live object if necessary.
  if (*offset == 0) {
    Page::FromAddress(old_object->address())->mc_first_forwarded =
        HeapObject::cast(new_object)->address();
  }

  MapWord encoding =
      MapWord::EncodeAddress(old_object->map()->address(), *offset);
  old_object->set_map_word(encoding);
  *offset += object_size;
  ASSERT(*offset <= Page::kObjectAreaSize);
}


// Most non-live objects are ignored.
inline void IgnoreNonLiveObject(HeapObject* object) {}


// Function template that, given a range of addresses (eg, a semispace or a
// paged space page), iterates through the objects in the range to clear
// mark bits and compute and encode forwarding addresses.  As a side effect,
// maximal free chunks are marked so that they can be skipped on subsequent
// sweeps.
//
// The template parameters are an allocation function, a forwarding address
// encoding function, and a function to process non-live objects.
template<MarkCompactCollector::AllocationFunction Alloc,
         MarkCompactCollector::EncodingFunction Encode,
         MarkCompactCollector::ProcessNonLiveFunction ProcessNonLive>
inline void EncodeForwardingAddressesInRange(Address start,
                                             Address end,
                                             int* offset) {
  // The start address of the current free region while sweeping the space.
  // This address is set when a transition from live to non-live objects is
  // encountered.  A value (an encoding of the 'next free region' pointer)
  // is written to memory at this address when a transition from non-live to
  // live objects is encountered.
  Address free_start = NULL;

  // A flag giving the state of the previously swept object.  Initially true
  // to ensure that free_start is initialized to a proper address before
  // trying to write to it.
  bool is_prev_alive = true;

  int object_size;  // Will be set on each iteration of the loop.
  for (Address current = start; current < end; current += object_size) {
    HeapObject* object = HeapObject::FromAddress(current);
    if (object->IsMarked()) {
      object->ClearMark();
      MarkCompactCollector::tracer()->decrement_marked_count();
      object_size = object->Size();

      Object* forwarded = Alloc(object, object_size);
      // Allocation cannot fail, because we are compacting the space.
      ASSERT(!forwarded->IsFailure());
      Encode(object, object_size, forwarded, offset);

#ifdef DEBUG
      if (FLAG_gc_verbose) {
        PrintF("forward %p -> %p.\n", object->address(),
               HeapObject::cast(forwarded)->address());
      }
#endif
      if (!is_prev_alive) {  // Transition from non-live to live.
        EncodeFreeRegion(free_start, static_cast<int>(current - free_start));
        is_prev_alive = true;
      }
    } else {  // Non-live object.
      object_size = object->Size();
      ProcessNonLive(object);
      if (is_prev_alive) {  // Transition from live to non-live.
        free_start = current;
        is_prev_alive = false;
      }
    }
  }

  // If we ended on a free region, mark it.
  if (!is_prev_alive) {
    EncodeFreeRegion(free_start, static_cast<int>(end - free_start));
  }
}


// Functions to encode the forwarding pointers in each compactable space.
void MarkCompactCollector::EncodeForwardingAddressesInNewSpace() {
  int ignored;
  EncodeForwardingAddressesInRange<MCAllocateFromNewSpace,
                                   EncodeForwardingAddressInNewSpace,
                                   IgnoreNonLiveObject>(
      Heap::new_space()->bottom(),
      Heap::new_space()->top(),
      &ignored);
}


template<MarkCompactCollector::AllocationFunction Alloc,
         MarkCompactCollector::ProcessNonLiveFunction ProcessNonLive>
void MarkCompactCollector::EncodeForwardingAddressesInPagedSpace(
    PagedSpace* space) {
  PageIterator it(space, PageIterator::PAGES_IN_USE);
  while (it.has_next()) {
    Page* p = it.next();

    // The offset of each live object in the page from the first live object
    // in the page.
    int offset = 0;
    EncodeForwardingAddressesInRange<Alloc,
                                     EncodeForwardingAddressInPagedSpace,
                                     ProcessNonLive>(
        p->ObjectAreaStart(),
        p->AllocationTop(),
        &offset);
  }
}


// We scavange new space simultaneously with sweeping. This is done in two
// passes.
// The first pass migrates all alive objects from one semispace to another or
// promotes them to old space. Forwading address is written directly into
// first word of object without any encoding. If object is dead we are writing
// NULL as a forwarding address.
// The second pass updates pointers to new space in all spaces. It is possible
// to encounter pointers to dead objects during traversal of dirty regions we
// should clear them to avoid encountering them during next dirty regions
// iteration.
static void MigrateObject(Address dst,
                          Address src,
                          int size,
                          bool to_old_space) {
  if (to_old_space) {
    Heap::CopyBlockToOldSpaceAndUpdateRegionMarks(dst, src, size);
  } else {
    Heap::CopyBlock(dst, src, size);
  }

  Memory::Address_at(src) = dst;
}


// Visitor for updating pointers from live objects in old spaces to new space.
// It does not expect to encounter pointers to dead objects.
class PointersToNewGenUpdatingVisitor: public ObjectVisitor {
 public:
  void VisitPointer(Object** p) {
    UpdatePointer(p);
  }

  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) UpdatePointer(p);
  }

  void VisitCodeTarget(RelocInfo* rinfo) {
    ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
    Object* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    VisitPointer(&target);
    rinfo->set_target_address(Code::cast(target)->instruction_start());
  }

  void VisitDebugTarget(RelocInfo* rinfo) {
    ASSERT((RelocInfo::IsJSReturn(rinfo->rmode()) &&
            rinfo->IsPatchedReturnSequence()) ||
           (RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
            rinfo->IsPatchedDebugBreakSlotSequence()));
    Object* target = Code::GetCodeFromTargetAddress(rinfo->call_address());
    VisitPointer(&target);
    rinfo->set_call_address(Code::cast(target)->instruction_start());
  }

 private:
  void UpdatePointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;

    HeapObject* obj = HeapObject::cast(*p);
    Address old_addr = obj->address();

    if (Heap::new_space()->Contains(obj)) {
      ASSERT(Heap::InFromSpace(*p));
      *p = HeapObject::FromAddress(Memory::Address_at(old_addr));
    }
  }
};


// Visitor for updating pointers from live objects in old spaces to new space.
// It can encounter pointers to dead objects in new space when traversing map
// space (see comment for MigrateObject).
static void UpdatePointerToNewGen(HeapObject** p) {
  if (!(*p)->IsHeapObject()) return;

  Address old_addr = (*p)->address();
  ASSERT(Heap::InFromSpace(*p));

  Address new_addr = Memory::Address_at(old_addr);

  if (new_addr == NULL) {
    // We encountered pointer to a dead object. Clear it so we will
    // not visit it again during next iteration of dirty regions.
    *p = NULL;
  } else {
    *p = HeapObject::FromAddress(new_addr);
  }
}


static String* UpdateNewSpaceReferenceInExternalStringTableEntry(Object **p) {
  Address old_addr = HeapObject::cast(*p)->address();
  Address new_addr = Memory::Address_at(old_addr);
  return String::cast(HeapObject::FromAddress(new_addr));
}


static bool TryPromoteObject(HeapObject* object, int object_size) {
  Object* result;

  if (object_size > Heap::MaxObjectSizeInPagedSpace()) {
    result = Heap::lo_space()->AllocateRawFixedArray(object_size);
    if (!result->IsFailure()) {
      HeapObject* target = HeapObject::cast(result);
      MigrateObject(target->address(), object->address(), object_size, true);
      MarkCompactCollector::tracer()->
          increment_promoted_objects_size(object_size);
      return true;
    }
  } else {
    OldSpace* target_space = Heap::TargetSpace(object);

    ASSERT(target_space == Heap::old_pointer_space() ||
           target_space == Heap::old_data_space());
    result = target_space->AllocateRaw(object_size);
    if (!result->IsFailure()) {
      HeapObject* target = HeapObject::cast(result);
      MigrateObject(target->address(),
                    object->address(),
                    object_size,
                    target_space == Heap::old_pointer_space());
      MarkCompactCollector::tracer()->
          increment_promoted_objects_size(object_size);
      return true;
    }
  }

  return false;
}


static void SweepNewSpace(NewSpace* space) {
  Heap::CheckNewSpaceExpansionCriteria();

  Address from_bottom = space->bottom();
  Address from_top = space->top();

  // Flip the semispaces.  After flipping, to space is empty, from space has
  // live objects.
  space->Flip();
  space->ResetAllocationInfo();

  int size = 0;
  int survivors_size = 0;

  // First pass: traverse all objects in inactive semispace, remove marks,
  // migrate live objects and write forwarding addresses.
  for (Address current = from_bottom; current < from_top; current += size) {
    HeapObject* object = HeapObject::FromAddress(current);

    if (object->IsMarked()) {
      object->ClearMark();
      MarkCompactCollector::tracer()->decrement_marked_count();

      size = object->Size();
      survivors_size += size;

      // Aggressively promote young survivors to the old space.
      if (TryPromoteObject(object, size)) {
        continue;
      }

      // Promotion failed. Just migrate object to another semispace.
      Object* target = space->AllocateRaw(size);

      // Allocation cannot fail at this point: semispaces are of equal size.
      ASSERT(!target->IsFailure());

      MigrateObject(HeapObject::cast(target)->address(),
                    current,
                    size,
                    false);
    } else {
      size = object->Size();
      Memory::Address_at(current) = NULL;
    }
  }

  // Second pass: find pointers to new space and update them.
  PointersToNewGenUpdatingVisitor updating_visitor;

  // Update pointers in to space.
  HeapObject* object;
  for (Address current = space->bottom();
       current < space->top();
       current += object->Size()) {
    object = HeapObject::FromAddress(current);

    object->IterateBody(object->map()->instance_type(),
                        object->Size(),
                        &updating_visitor);
  }

  // Update roots.
  Heap::IterateRoots(&updating_visitor, VISIT_ALL_IN_SCAVENGE);

  // Update pointers in old spaces.
  Heap::IterateDirtyRegions(Heap::old_pointer_space(),
                            &Heap::IteratePointersInDirtyRegion,
                            &UpdatePointerToNewGen,
                            Heap::WATERMARK_SHOULD_BE_VALID);

  Heap::lo_space()->IterateDirtyRegions(&UpdatePointerToNewGen);

  // Update pointers from cells.
  HeapObjectIterator cell_iterator(Heap::cell_space());
  for (HeapObject* cell = cell_iterator.next();
       cell != NULL;
       cell = cell_iterator.next()) {
    if (cell->IsJSGlobalPropertyCell()) {
      Address value_address =
          reinterpret_cast<Address>(cell) +
          (JSGlobalPropertyCell::kValueOffset - kHeapObjectTag);
      updating_visitor.VisitPointer(reinterpret_cast<Object**>(value_address));
    }
  }

  // Update pointers from external string table.
  Heap::UpdateNewSpaceReferencesInExternalStringTable(
      &UpdateNewSpaceReferenceInExternalStringTableEntry);

  // All pointers were updated. Update auxiliary allocation info.
  Heap::IncrementYoungSurvivorsCounter(survivors_size);
  space->set_age_mark(space->top());
}


static void SweepSpace(PagedSpace* space, DeallocateFunction dealloc) {
  PageIterator it(space, PageIterator::PAGES_IN_USE);

  // During sweeping of paged space we are trying to find longest sequences
  // of pages without live objects and free them (instead of putting them on
  // the free list).

  // Page preceding current.
  Page* prev = Page::FromAddress(NULL);

  // First empty page in a sequence.
  Page* first_empty_page = Page::FromAddress(NULL);

  // Page preceding first empty page.
  Page* prec_first_empty_page = Page::FromAddress(NULL);

  // If last used page of space ends with a sequence of dead objects
  // we can adjust allocation top instead of puting this free area into
  // the free list. Thus during sweeping we keep track of such areas
  // and defer their deallocation until the sweeping of the next page
  // is done: if one of the next pages contains live objects we have
  // to put such area into the free list.
  Address last_free_start = NULL;
  int last_free_size = 0;

  while (it.has_next()) {
    Page* p = it.next();

    bool is_previous_alive = true;
    Address free_start = NULL;
    HeapObject* object;

    for (Address current = p->ObjectAreaStart();
         current < p->AllocationTop();
         current += object->Size()) {
      object = HeapObject::FromAddress(current);
      if (object->IsMarked()) {
        object->ClearMark();
        MarkCompactCollector::tracer()->decrement_marked_count();

        if (!is_previous_alive) {  // Transition from free to live.
          dealloc(free_start,
                  static_cast<int>(current - free_start),
                  true,
                  false);
          is_previous_alive = true;
        }
      } else {
        MarkCompactCollector::ReportDeleteIfNeeded(object);
        if (is_previous_alive) {  // Transition from live to free.
          free_start = current;
          is_previous_alive = false;
        }
      }
      // The object is now unmarked for the call to Size() at the top of the
      // loop.
    }

    bool page_is_empty = (p->ObjectAreaStart() == p->AllocationTop())
        || (!is_previous_alive && free_start == p->ObjectAreaStart());

    if (page_is_empty) {
      // This page is empty. Check whether we are in the middle of
      // sequence of empty pages and start one if not.
      if (!first_empty_page->is_valid()) {
        first_empty_page = p;
        prec_first_empty_page = prev;
      }

      if (!is_previous_alive) {
        // There are dead objects on this page. Update space accounting stats
        // without putting anything into free list.
        int size_in_bytes = static_cast<int>(p->AllocationTop() - free_start);
        if (size_in_bytes > 0) {
          dealloc(free_start, size_in_bytes, false, true);
        }
      }
    } else {
      // This page is not empty. Sequence of empty pages ended on the previous
      // one.
      if (first_empty_page->is_valid()) {
        space->FreePages(prec_first_empty_page, prev);
        prec_first_empty_page = first_empty_page = Page::FromAddress(NULL);
      }

      // If there is a free ending area on one of the previous pages we have
      // deallocate that area and put it on the free list.
      if (last_free_size > 0) {
        Page::FromAddress(last_free_start)->
            SetAllocationWatermark(last_free_start);
        dealloc(last_free_start, last_free_size, true, true);
        last_free_start = NULL;
        last_free_size  = 0;
      }

      // If the last region of this page was not live we remember it.
      if (!is_previous_alive) {
        ASSERT(last_free_size == 0);
        last_free_size = static_cast<int>(p->AllocationTop() - free_start);
        last_free_start = free_start;
      }
    }

    prev = p;
  }

  // We reached end of space. See if we need to adjust allocation top.
  Address new_allocation_top = NULL;

  if (first_empty_page->is_valid()) {
    // Last used pages in space are empty. We can move allocation top backwards
    // to the beginning of first empty page.
    ASSERT(prev == space->AllocationTopPage());

    new_allocation_top = first_empty_page->ObjectAreaStart();
  }

  if (last_free_size > 0) {
    // There was a free ending area on the previous page.
    // Deallocate it without putting it into freelist and move allocation
    // top to the beginning of this free area.
    dealloc(last_free_start, last_free_size, false, true);
    new_allocation_top = last_free_start;
  }

  if (new_allocation_top != NULL) {
#ifdef DEBUG
    Page* new_allocation_top_page = Page::FromAllocationTop(new_allocation_top);
    if (!first_empty_page->is_valid()) {
      ASSERT(new_allocation_top_page == space->AllocationTopPage());
    } else if (last_free_size > 0) {
      ASSERT(new_allocation_top_page == prec_first_empty_page);
    } else {
      ASSERT(new_allocation_top_page == first_empty_page);
    }
#endif

    space->SetTop(new_allocation_top);
  }
}


void MarkCompactCollector::DeallocateOldPointerBlock(Address start,
                                                     int size_in_bytes,
                                                     bool add_to_freelist,
                                                     bool last_on_page) {
  Heap::old_pointer_space()->Free(start, size_in_bytes, add_to_freelist);
}


void MarkCompactCollector::DeallocateOldDataBlock(Address start,
                                                  int size_in_bytes,
                                                  bool add_to_freelist,
                                                  bool last_on_page) {
  Heap::old_data_space()->Free(start, size_in_bytes, add_to_freelist);
}


void MarkCompactCollector::DeallocateCodeBlock(Address start,
                                               int size_in_bytes,
                                               bool add_to_freelist,
                                               bool last_on_page) {
  Heap::code_space()->Free(start, size_in_bytes, add_to_freelist);
}


void MarkCompactCollector::DeallocateMapBlock(Address start,
                                              int size_in_bytes,
                                              bool add_to_freelist,
                                              bool last_on_page) {
  // Objects in map space are assumed to have size Map::kSize and a
  // valid map in their first word.  Thus, we break the free block up into
  // chunks and free them separately.
  ASSERT(size_in_bytes % Map::kSize == 0);
  Address end = start + size_in_bytes;
  for (Address a = start; a < end; a += Map::kSize) {
    Heap::map_space()->Free(a, add_to_freelist);
  }
}


void MarkCompactCollector::DeallocateCellBlock(Address start,
                                               int size_in_bytes,
                                               bool add_to_freelist,
                                               bool last_on_page) {
  // Free-list elements in cell space are assumed to have a fixed size.
  // We break the free block into chunks and add them to the free list
  // individually.
  int size = Heap::cell_space()->object_size_in_bytes();
  ASSERT(size_in_bytes % size == 0);
  Address end = start + size_in_bytes;
  for (Address a = start; a < end; a += size) {
    Heap::cell_space()->Free(a, add_to_freelist);
  }
}


void MarkCompactCollector::EncodeForwardingAddresses() {
  ASSERT(state_ == ENCODE_FORWARDING_ADDRESSES);
  // Objects in the active semispace of the young generation may be
  // relocated to the inactive semispace (if not promoted).  Set the
  // relocation info to the beginning of the inactive semispace.
  Heap::new_space()->MCResetRelocationInfo();

  // Compute the forwarding pointers in each space.
  EncodeForwardingAddressesInPagedSpace<MCAllocateFromOldPointerSpace,
                                        ReportDeleteIfNeeded>(
      Heap::old_pointer_space());

  EncodeForwardingAddressesInPagedSpace<MCAllocateFromOldDataSpace,
                                        IgnoreNonLiveObject>(
      Heap::old_data_space());

  EncodeForwardingAddressesInPagedSpace<MCAllocateFromCodeSpace,
                                        ReportDeleteIfNeeded>(
      Heap::code_space());

  EncodeForwardingAddressesInPagedSpace<MCAllocateFromCellSpace,
                                        IgnoreNonLiveObject>(
      Heap::cell_space());


  // Compute new space next to last after the old and code spaces have been
  // compacted.  Objects in new space can be promoted to old or code space.
  EncodeForwardingAddressesInNewSpace();

  // Compute map space last because computing forwarding addresses
  // overwrites non-live objects.  Objects in the other spaces rely on
  // non-live map pointers to get the sizes of non-live objects.
  EncodeForwardingAddressesInPagedSpace<MCAllocateFromMapSpace,
                                        IgnoreNonLiveObject>(
      Heap::map_space());

  // Write relocation info to the top page, so we can use it later.  This is
  // done after promoting objects from the new space so we get the correct
  // allocation top.
  Heap::old_pointer_space()->MCWriteRelocationInfoToPage();
  Heap::old_data_space()->MCWriteRelocationInfoToPage();
  Heap::code_space()->MCWriteRelocationInfoToPage();
  Heap::map_space()->MCWriteRelocationInfoToPage();
  Heap::cell_space()->MCWriteRelocationInfoToPage();
}


class MapIterator : public HeapObjectIterator {
 public:
  MapIterator() : HeapObjectIterator(Heap::map_space(), &SizeCallback) { }

  explicit MapIterator(Address start)
      : HeapObjectIterator(Heap::map_space(), start, &SizeCallback) { }

 private:
  static int SizeCallback(HeapObject* unused) {
    USE(unused);
    return Map::kSize;
  }
};


class MapCompact {
 public:
  explicit MapCompact(int live_maps)
    : live_maps_(live_maps),
      to_evacuate_start_(Heap::map_space()->TopAfterCompaction(live_maps)),
      map_to_evacuate_it_(to_evacuate_start_),
      first_map_to_evacuate_(
          reinterpret_cast<Map*>(HeapObject::FromAddress(to_evacuate_start_))) {
  }

  void CompactMaps() {
    // As we know the number of maps to evacuate beforehand,
    // we stop then there is no more vacant maps.
    for (Map* next_vacant_map = NextVacantMap();
         next_vacant_map;
         next_vacant_map = NextVacantMap()) {
      EvacuateMap(next_vacant_map, NextMapToEvacuate());
    }

#ifdef DEBUG
    CheckNoMapsToEvacuate();
#endif
  }

  void UpdateMapPointersInRoots() {
    Heap::IterateRoots(&map_updating_visitor_, VISIT_ONLY_STRONG);
    GlobalHandles::IterateWeakRoots(&map_updating_visitor_);
  }

  void UpdateMapPointersInPagedSpace(PagedSpace* space) {
    ASSERT(space != Heap::map_space());

    PageIterator it(space, PageIterator::PAGES_IN_USE);
    while (it.has_next()) {
      Page* p = it.next();
      UpdateMapPointersInRange(p->ObjectAreaStart(), p->AllocationTop());
    }
  }

  void UpdateMapPointersInNewSpace() {
    NewSpace* space = Heap::new_space();
    UpdateMapPointersInRange(space->bottom(), space->top());
  }

  void UpdateMapPointersInLargeObjectSpace() {
    LargeObjectIterator it(Heap::lo_space());
    for (HeapObject* obj = it.next(); obj != NULL; obj = it.next())
      UpdateMapPointersInObject(obj);
  }

  void Finish() {
    Heap::map_space()->FinishCompaction(to_evacuate_start_, live_maps_);
  }

 private:
  int live_maps_;
  Address to_evacuate_start_;
  MapIterator vacant_map_it_;
  MapIterator map_to_evacuate_it_;
  Map* first_map_to_evacuate_;

  // Helper class for updating map pointers in HeapObjects.
  class MapUpdatingVisitor: public ObjectVisitor {
  public:
    void VisitPointer(Object** p) {
      UpdateMapPointer(p);
    }

    void VisitPointers(Object** start, Object** end) {
      for (Object** p = start; p < end; p++) UpdateMapPointer(p);
    }

  private:
    void UpdateMapPointer(Object** p) {
      if (!(*p)->IsHeapObject()) return;
      HeapObject* old_map = reinterpret_cast<HeapObject*>(*p);

      // Moved maps are tagged with overflowed map word.  They are the only
      // objects those map word is overflowed as marking is already complete.
      MapWord map_word = old_map->map_word();
      if (!map_word.IsOverflowed()) return;

      *p = GetForwardedMap(map_word);
    }
  };

  static MapUpdatingVisitor map_updating_visitor_;

  static Map* NextMap(MapIterator* it, HeapObject* last, bool live) {
    while (true) {
      HeapObject* next = it->next();
      ASSERT(next != NULL);
      if (next == last)
        return NULL;
      ASSERT(!next->IsOverflowed());
      ASSERT(!next->IsMarked());
      ASSERT(next->IsMap() || FreeListNode::IsFreeListNode(next));
      if (next->IsMap() == live)
        return reinterpret_cast<Map*>(next);
    }
  }

  Map* NextVacantMap() {
    Map* map = NextMap(&vacant_map_it_, first_map_to_evacuate_, false);
    ASSERT(map == NULL || FreeListNode::IsFreeListNode(map));
    return map;
  }

  Map* NextMapToEvacuate() {
    Map* map = NextMap(&map_to_evacuate_it_, NULL, true);
    ASSERT(map != NULL);
    ASSERT(map->IsMap());
    return map;
  }

  static void EvacuateMap(Map* vacant_map, Map* map_to_evacuate) {
    ASSERT(FreeListNode::IsFreeListNode(vacant_map));
    ASSERT(map_to_evacuate->IsMap());

    ASSERT(Map::kSize % 4 == 0);

    Heap::CopyBlockToOldSpaceAndUpdateRegionMarks(vacant_map->address(),
                                                  map_to_evacuate->address(),
                                                  Map::kSize);

    ASSERT(vacant_map->IsMap());  // Due to memcpy above.

    MapWord forwarding_map_word = MapWord::FromMap(vacant_map);
    forwarding_map_word.SetOverflow();
    map_to_evacuate->set_map_word(forwarding_map_word);

    ASSERT(map_to_evacuate->map_word().IsOverflowed());
    ASSERT(GetForwardedMap(map_to_evacuate->map_word()) == vacant_map);
  }

  static Map* GetForwardedMap(MapWord map_word) {
    ASSERT(map_word.IsOverflowed());
    map_word.ClearOverflow();
    Map* new_map = map_word.ToMap();
    ASSERT_MAP_ALIGNED(new_map->address());
    return new_map;
  }

  static int UpdateMapPointersInObject(HeapObject* obj) {
    ASSERT(!obj->IsMarked());
    Map* map = obj->map();
    ASSERT(Heap::map_space()->Contains(map));
    MapWord map_word = map->map_word();
    ASSERT(!map_word.IsMarked());
    if (map_word.IsOverflowed()) {
      Map* new_map = GetForwardedMap(map_word);
      ASSERT(Heap::map_space()->Contains(new_map));
      obj->set_map(new_map);

#ifdef DEBUG
      if (FLAG_gc_verbose) {
        PrintF("update %p : %p -> %p\n", obj->address(),
              map, new_map);
      }
#endif
    }

    int size = obj->SizeFromMap(map);
    obj->IterateBody(map->instance_type(), size, &map_updating_visitor_);
    return size;
  }

  static void UpdateMapPointersInRange(Address start, Address end) {
    HeapObject* object;
    int size;
    for (Address current = start; current < end; current += size) {
      object = HeapObject::FromAddress(current);
      size = UpdateMapPointersInObject(object);
      ASSERT(size > 0);
    }
  }

#ifdef DEBUG
  void CheckNoMapsToEvacuate() {
    if (!FLAG_enable_slow_asserts)
      return;

    for (HeapObject* obj = map_to_evacuate_it_.next();
         obj != NULL; obj = map_to_evacuate_it_.next())
      ASSERT(FreeListNode::IsFreeListNode(obj));
  }
#endif
};

MapCompact::MapUpdatingVisitor MapCompact::map_updating_visitor_;


void MarkCompactCollector::SweepSpaces() {
  GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_SWEEP);

  ASSERT(state_ == SWEEP_SPACES);
  ASSERT(!IsCompacting());
  // Noncompacting collections simply sweep the spaces to clear the mark
  // bits and free the nonlive blocks (for old and map spaces).  We sweep
  // the map space last because freeing non-live maps overwrites them and
  // the other spaces rely on possibly non-live maps to get the sizes for
  // non-live objects.
  SweepSpace(Heap::old_pointer_space(), &DeallocateOldPointerBlock);
  SweepSpace(Heap::old_data_space(), &DeallocateOldDataBlock);
  SweepSpace(Heap::code_space(), &DeallocateCodeBlock);
  SweepSpace(Heap::cell_space(), &DeallocateCellBlock);
  SweepNewSpace(Heap::new_space());
  SweepSpace(Heap::map_space(), &DeallocateMapBlock);

  Heap::IterateDirtyRegions(Heap::map_space(),
                            &Heap::IteratePointersInDirtyMapsRegion,
                            &UpdatePointerToNewGen,
                            Heap::WATERMARK_SHOULD_BE_VALID);

  int live_maps_size = Heap::map_space()->Size();
  int live_maps = live_maps_size / Map::kSize;
  ASSERT(live_map_objects_size_ == live_maps_size);

  if (Heap::map_space()->NeedsCompaction(live_maps)) {
    MapCompact map_compact(live_maps);

    map_compact.CompactMaps();
    map_compact.UpdateMapPointersInRoots();

    PagedSpaces spaces;
    for (PagedSpace* space = spaces.next();
         space != NULL; space = spaces.next()) {
      if (space == Heap::map_space()) continue;
      map_compact.UpdateMapPointersInPagedSpace(space);
    }
    map_compact.UpdateMapPointersInNewSpace();
    map_compact.UpdateMapPointersInLargeObjectSpace();

    map_compact.Finish();
  }
}


// Iterate the live objects in a range of addresses (eg, a page or a
// semispace).  The live regions of the range have been linked into a list.
// The first live region is [first_live_start, first_live_end), and the last
// address in the range is top.  The callback function is used to get the
// size of each live object.
int MarkCompactCollector::IterateLiveObjectsInRange(
    Address start,
    Address end,
    HeapObjectCallback size_func) {
  int live_objects_size = 0;
  Address current = start;
  while (current < end) {
    uint32_t encoded_map = Memory::uint32_at(current);
    if (encoded_map == kSingleFreeEncoding) {
      current += kPointerSize;
    } else if (encoded_map == kMultiFreeEncoding) {
      current += Memory::int_at(current + kIntSize);
    } else {
      int size = size_func(HeapObject::FromAddress(current));
      current += size;
      live_objects_size += size;
    }
  }
  return live_objects_size;
}


int MarkCompactCollector::IterateLiveObjects(NewSpace* space,
                                             HeapObjectCallback size_f) {
  ASSERT(MARK_LIVE_OBJECTS < state_ && state_ <= RELOCATE_OBJECTS);
  return IterateLiveObjectsInRange(space->bottom(), space->top(), size_f);
}


int MarkCompactCollector::IterateLiveObjects(PagedSpace* space,
                                             HeapObjectCallback size_f) {
  ASSERT(MARK_LIVE_OBJECTS < state_ && state_ <= RELOCATE_OBJECTS);
  int total = 0;
  PageIterator it(space, PageIterator::PAGES_IN_USE);
  while (it.has_next()) {
    Page* p = it.next();
    total += IterateLiveObjectsInRange(p->ObjectAreaStart(),
                                       p->AllocationTop(),
                                       size_f);
  }
  return total;
}


// -------------------------------------------------------------------------
// Phase 3: Update pointers

// Helper class for updating pointers in HeapObjects.
class UpdatingVisitor: public ObjectVisitor {
 public:
  void VisitPointer(Object** p) {
    UpdatePointer(p);
  }

  void VisitPointers(Object** start, Object** end) {
    // Mark all HeapObject pointers in [start, end)
    for (Object** p = start; p < end; p++) UpdatePointer(p);
  }

  void VisitCodeTarget(RelocInfo* rinfo) {
    ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
    Object* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    VisitPointer(&target);
    rinfo->set_target_address(
        reinterpret_cast<Code*>(target)->instruction_start());
  }

  void VisitDebugTarget(RelocInfo* rinfo) {
    ASSERT((RelocInfo::IsJSReturn(rinfo->rmode()) &&
            rinfo->IsPatchedReturnSequence()) ||
           (RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
            rinfo->IsPatchedDebugBreakSlotSequence()));
    Object* target = Code::GetCodeFromTargetAddress(rinfo->call_address());
    VisitPointer(&target);
    rinfo->set_call_address(
        reinterpret_cast<Code*>(target)->instruction_start());
  }

 private:
  void UpdatePointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;

    HeapObject* obj = HeapObject::cast(*p);
    Address old_addr = obj->address();
    Address new_addr;
    ASSERT(!Heap::InFromSpace(obj));

    if (Heap::new_space()->Contains(obj)) {
      Address forwarding_pointer_addr =
          Heap::new_space()->FromSpaceLow() +
          Heap::new_space()->ToSpaceOffsetForAddress(old_addr);
      new_addr = Memory::Address_at(forwarding_pointer_addr);

#ifdef DEBUG
      ASSERT(Heap::old_pointer_space()->Contains(new_addr) ||
             Heap::old_data_space()->Contains(new_addr) ||
             Heap::new_space()->FromSpaceContains(new_addr) ||
             Heap::lo_space()->Contains(HeapObject::FromAddress(new_addr)));

      if (Heap::new_space()->FromSpaceContains(new_addr)) {
        ASSERT(Heap::new_space()->FromSpaceOffsetForAddress(new_addr) <=
               Heap::new_space()->ToSpaceOffsetForAddress(old_addr));
      }
#endif

    } else if (Heap::lo_space()->Contains(obj)) {
      // Don't move objects in the large object space.
      return;

    } else {
#ifdef DEBUG
      PagedSpaces spaces;
      PagedSpace* original_space = spaces.next();
      while (original_space != NULL) {
        if (original_space->Contains(obj)) break;
        original_space = spaces.next();
      }
      ASSERT(original_space != NULL);
#endif
      new_addr = MarkCompactCollector::GetForwardingAddressInOldSpace(obj);
      ASSERT(original_space->Contains(new_addr));
      ASSERT(original_space->MCSpaceOffsetForAddress(new_addr) <=
             original_space->MCSpaceOffsetForAddress(old_addr));
    }

    *p = HeapObject::FromAddress(new_addr);

#ifdef DEBUG
    if (FLAG_gc_verbose) {
      PrintF("update %p : %p -> %p\n",
             reinterpret_cast<Address>(p), old_addr, new_addr);
    }
#endif
  }
};


void MarkCompactCollector::UpdatePointers() {
#ifdef DEBUG
  ASSERT(state_ == ENCODE_FORWARDING_ADDRESSES);
  state_ = UPDATE_POINTERS;
#endif
  UpdatingVisitor updating_visitor;
  Heap::IterateRoots(&updating_visitor, VISIT_ONLY_STRONG);
  GlobalHandles::IterateWeakRoots(&updating_visitor);

  int live_maps_size = IterateLiveObjects(Heap::map_space(),
                                          &UpdatePointersInOldObject);
  int live_pointer_olds_size = IterateLiveObjects(Heap::old_pointer_space(),
                                                  &UpdatePointersInOldObject);
  int live_data_olds_size = IterateLiveObjects(Heap::old_data_space(),
                                               &UpdatePointersInOldObject);
  int live_codes_size = IterateLiveObjects(Heap::code_space(),
                                           &UpdatePointersInOldObject);
  int live_cells_size = IterateLiveObjects(Heap::cell_space(),
                                           &UpdatePointersInOldObject);
  int live_news_size = IterateLiveObjects(Heap::new_space(),
                                          &UpdatePointersInNewObject);

  // Large objects do not move, the map word can be updated directly.
  LargeObjectIterator it(Heap::lo_space());
  for (HeapObject* obj = it.next(); obj != NULL; obj = it.next())
    UpdatePointersInNewObject(obj);

  USE(live_maps_size);
  USE(live_pointer_olds_size);
  USE(live_data_olds_size);
  USE(live_codes_size);
  USE(live_cells_size);
  USE(live_news_size);
  ASSERT(live_maps_size == live_map_objects_size_);
  ASSERT(live_data_olds_size == live_old_data_objects_size_);
  ASSERT(live_pointer_olds_size == live_old_pointer_objects_size_);
  ASSERT(live_codes_size == live_code_objects_size_);
  ASSERT(live_cells_size == live_cell_objects_size_);
  ASSERT(live_news_size == live_young_objects_size_);
}


int MarkCompactCollector::UpdatePointersInNewObject(HeapObject* obj) {
  // Keep old map pointers
  Map* old_map = obj->map();
  ASSERT(old_map->IsHeapObject());

  Address forwarded = GetForwardingAddressInOldSpace(old_map);

  ASSERT(Heap::map_space()->Contains(old_map));
  ASSERT(Heap::map_space()->Contains(forwarded));
#ifdef DEBUG
  if (FLAG_gc_verbose) {
    PrintF("update %p : %p -> %p\n", obj->address(), old_map->address(),
           forwarded);
  }
#endif
  // Update the map pointer.
  obj->set_map(reinterpret_cast<Map*>(HeapObject::FromAddress(forwarded)));

  // We have to compute the object size relying on the old map because
  // map objects are not relocated yet.
  int obj_size = obj->SizeFromMap(old_map);

  // Update pointers in the object body.
  UpdatingVisitor updating_visitor;
  obj->IterateBody(old_map->instance_type(), obj_size, &updating_visitor);
  return obj_size;
}


int MarkCompactCollector::UpdatePointersInOldObject(HeapObject* obj) {
  // Decode the map pointer.
  MapWord encoding = obj->map_word();
  Address map_addr = encoding.DecodeMapAddress(Heap::map_space());
  ASSERT(Heap::map_space()->Contains(HeapObject::FromAddress(map_addr)));

  // At this point, the first word of map_addr is also encoded, cannot
  // cast it to Map* using Map::cast.
  Map* map = reinterpret_cast<Map*>(HeapObject::FromAddress(map_addr));
  int obj_size = obj->SizeFromMap(map);
  InstanceType type = map->instance_type();

  // Update map pointer.
  Address new_map_addr = GetForwardingAddressInOldSpace(map);
  int offset = encoding.DecodeOffset();
  obj->set_map_word(MapWord::EncodeAddress(new_map_addr, offset));

#ifdef DEBUG
  if (FLAG_gc_verbose) {
    PrintF("update %p : %p -> %p\n", obj->address(),
           map_addr, new_map_addr);
  }
#endif

  // Update pointers in the object body.
  UpdatingVisitor updating_visitor;
  obj->IterateBody(type, obj_size, &updating_visitor);
  return obj_size;
}


Address MarkCompactCollector::GetForwardingAddressInOldSpace(HeapObject* obj) {
  // Object should either in old or map space.
  MapWord encoding = obj->map_word();

  // Offset to the first live object's forwarding address.
  int offset = encoding.DecodeOffset();
  Address obj_addr = obj->address();

  // Find the first live object's forwarding address.
  Page* p = Page::FromAddress(obj_addr);
  Address first_forwarded = p->mc_first_forwarded;

  // Page start address of forwarded address.
  Page* forwarded_page = Page::FromAddress(first_forwarded);
  int forwarded_offset = forwarded_page->Offset(first_forwarded);

  // Find end of allocation in the page of first_forwarded.
  int mc_top_offset = forwarded_page->AllocationWatermarkOffset();

  // Check if current object's forward pointer is in the same page
  // as the first live object's forwarding pointer
  if (forwarded_offset + offset < mc_top_offset) {
    // In the same page.
    return first_forwarded + offset;
  }

  // Must be in the next page, NOTE: this may cross chunks.
  Page* next_page = forwarded_page->next_page();
  ASSERT(next_page->is_valid());

  offset -= (mc_top_offset - forwarded_offset);
  offset += Page::kObjectStartOffset;

  ASSERT_PAGE_OFFSET(offset);
  ASSERT(next_page->OffsetToAddress(offset) < next_page->AllocationTop());

  return next_page->OffsetToAddress(offset);
}


// -------------------------------------------------------------------------
// Phase 4: Relocate objects

void MarkCompactCollector::RelocateObjects() {
#ifdef DEBUG
  ASSERT(state_ == UPDATE_POINTERS);
  state_ = RELOCATE_OBJECTS;
#endif
  // Relocates objects, always relocate map objects first. Relocating
  // objects in other space relies on map objects to get object size.
  int live_maps_size = IterateLiveObjects(Heap::map_space(),
                                          &RelocateMapObject);
  int live_pointer_olds_size = IterateLiveObjects(Heap::old_pointer_space(),
                                                  &RelocateOldPointerObject);
  int live_data_olds_size = IterateLiveObjects(Heap::old_data_space(),
                                               &RelocateOldDataObject);
  int live_codes_size = IterateLiveObjects(Heap::code_space(),
                                           &RelocateCodeObject);
  int live_cells_size = IterateLiveObjects(Heap::cell_space(),
                                           &RelocateCellObject);
  int live_news_size = IterateLiveObjects(Heap::new_space(),
                                          &RelocateNewObject);

  USE(live_maps_size);
  USE(live_pointer_olds_size);
  USE(live_data_olds_size);
  USE(live_codes_size);
  USE(live_cells_size);
  USE(live_news_size);
  ASSERT(live_maps_size == live_map_objects_size_);
  ASSERT(live_data_olds_size == live_old_data_objects_size_);
  ASSERT(live_pointer_olds_size == live_old_pointer_objects_size_);
  ASSERT(live_codes_size == live_code_objects_size_);
  ASSERT(live_cells_size == live_cell_objects_size_);
  ASSERT(live_news_size == live_young_objects_size_);

  // Flip from and to spaces
  Heap::new_space()->Flip();

  Heap::new_space()->MCCommitRelocationInfo();

  // Set age_mark to bottom in to space
  Address mark = Heap::new_space()->bottom();
  Heap::new_space()->set_age_mark(mark);

  PagedSpaces spaces;
  for (PagedSpace* space = spaces.next(); space != NULL; space = spaces.next())
    space->MCCommitRelocationInfo();

  Heap::CheckNewSpaceExpansionCriteria();
  Heap::IncrementYoungSurvivorsCounter(live_news_size);
}


int MarkCompactCollector::RelocateMapObject(HeapObject* obj) {
  // Recover map pointer.
  MapWord encoding = obj->map_word();
  Address map_addr = encoding.DecodeMapAddress(Heap::map_space());
  ASSERT(Heap::map_space()->Contains(HeapObject::FromAddress(map_addr)));

  // Get forwarding address before resetting map pointer
  Address new_addr = GetForwardingAddressInOldSpace(obj);

  // Reset map pointer.  The meta map object may not be copied yet so
  // Map::cast does not yet work.
  obj->set_map(reinterpret_cast<Map*>(HeapObject::FromAddress(map_addr)));

  Address old_addr = obj->address();

  if (new_addr != old_addr) {
    // Move contents.
    Heap::MoveBlockToOldSpaceAndUpdateRegionMarks(new_addr,
                                                  old_addr,
                                                  Map::kSize);
  }

#ifdef DEBUG
  if (FLAG_gc_verbose) {
    PrintF("relocate %p -> %p\n", old_addr, new_addr);
  }
#endif

  return Map::kSize;
}


static inline int RestoreMap(HeapObject* obj,
                             PagedSpace* space,
                             Address new_addr,
                             Address map_addr) {
  // This must be a non-map object, and the function relies on the
  // assumption that the Map space is compacted before the other paged
  // spaces (see RelocateObjects).

  // Reset map pointer.
  obj->set_map(Map::cast(HeapObject::FromAddress(map_addr)));

  int obj_size = obj->Size();
  ASSERT_OBJECT_SIZE(obj_size);

  ASSERT(space->MCSpaceOffsetForAddress(new_addr) <=
         space->MCSpaceOffsetForAddress(obj->address()));

#ifdef DEBUG
  if (FLAG_gc_verbose) {
    PrintF("relocate %p -> %p\n", obj->address(), new_addr);
  }
#endif

  return obj_size;
}


int MarkCompactCollector::RelocateOldNonCodeObject(HeapObject* obj,
                                                   PagedSpace* space) {
  // Recover map pointer.
  MapWord encoding = obj->map_word();
  Address map_addr = encoding.DecodeMapAddress(Heap::map_space());
  ASSERT(Heap::map_space()->Contains(map_addr));

  // Get forwarding address before resetting map pointer.
  Address new_addr = GetForwardingAddressInOldSpace(obj);

  // Reset the map pointer.
  int obj_size = RestoreMap(obj, space, new_addr, map_addr);

  Address old_addr = obj->address();

  if (new_addr != old_addr) {
    // Move contents.
    if (space == Heap::old_data_space()) {
      Heap::MoveBlock(new_addr, old_addr, obj_size);
    } else {
      Heap::MoveBlockToOldSpaceAndUpdateRegionMarks(new_addr,
                                                    old_addr,
                                                    obj_size);
    }
  }

  ASSERT(!HeapObject::FromAddress(new_addr)->IsCode());

  HeapObject* copied_to = HeapObject::FromAddress(new_addr);
  if (copied_to->IsJSFunction()) {
    PROFILE(FunctionMoveEvent(old_addr, new_addr));
  }

  return obj_size;
}


int MarkCompactCollector::RelocateOldPointerObject(HeapObject* obj) {
  return RelocateOldNonCodeObject(obj, Heap::old_pointer_space());
}


int MarkCompactCollector::RelocateOldDataObject(HeapObject* obj) {
  return RelocateOldNonCodeObject(obj, Heap::old_data_space());
}


int MarkCompactCollector::RelocateCellObject(HeapObject* obj) {
  return RelocateOldNonCodeObject(obj, Heap::cell_space());
}


int MarkCompactCollector::RelocateCodeObject(HeapObject* obj) {
  // Recover map pointer.
  MapWord encoding = obj->map_word();
  Address map_addr = encoding.DecodeMapAddress(Heap::map_space());
  ASSERT(Heap::map_space()->Contains(HeapObject::FromAddress(map_addr)));

  // Get forwarding address before resetting map pointer
  Address new_addr = GetForwardingAddressInOldSpace(obj);

  // Reset the map pointer.
  int obj_size = RestoreMap(obj, Heap::code_space(), new_addr, map_addr);

  Address old_addr = obj->address();

  if (new_addr != old_addr) {
    // Move contents.
    Heap::MoveBlock(new_addr, old_addr, obj_size);
  }

  HeapObject* copied_to = HeapObject::FromAddress(new_addr);
  if (copied_to->IsCode()) {
    // May also update inline cache target.
    Code::cast(copied_to)->Relocate(new_addr - old_addr);
    // Notify the logger that compiled code has moved.
    PROFILE(CodeMoveEvent(old_addr, new_addr));
  }

  return obj_size;
}


int MarkCompactCollector::RelocateNewObject(HeapObject* obj) {
  int obj_size = obj->Size();

  // Get forwarding address
  Address old_addr = obj->address();
  int offset = Heap::new_space()->ToSpaceOffsetForAddress(old_addr);

  Address new_addr =
    Memory::Address_at(Heap::new_space()->FromSpaceLow() + offset);

#ifdef DEBUG
  if (Heap::new_space()->FromSpaceContains(new_addr)) {
    ASSERT(Heap::new_space()->FromSpaceOffsetForAddress(new_addr) <=
           Heap::new_space()->ToSpaceOffsetForAddress(old_addr));
  } else {
    ASSERT(Heap::TargetSpace(obj) == Heap::old_pointer_space() ||
           Heap::TargetSpace(obj) == Heap::old_data_space());
  }
#endif

  // New and old addresses cannot overlap.
  if (Heap::InNewSpace(HeapObject::FromAddress(new_addr))) {
    Heap::CopyBlock(new_addr, old_addr, obj_size);
  } else {
    Heap::CopyBlockToOldSpaceAndUpdateRegionMarks(new_addr,
                                                  old_addr,
                                                  obj_size);
  }

#ifdef DEBUG
  if (FLAG_gc_verbose) {
    PrintF("relocate %p -> %p\n", old_addr, new_addr);
  }
#endif

  HeapObject* copied_to = HeapObject::FromAddress(new_addr);
  if (copied_to->IsJSFunction()) {
    PROFILE(FunctionMoveEvent(old_addr, new_addr));
  }

  return obj_size;
}


void MarkCompactCollector::ReportDeleteIfNeeded(HeapObject* obj) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (obj->IsCode()) {
    PROFILE(CodeDeleteEvent(obj->address()));
  } else if (obj->IsJSFunction()) {
    PROFILE(FunctionDeleteEvent(obj->address()));
  }
#endif
}

} }  // namespace v8::internal
