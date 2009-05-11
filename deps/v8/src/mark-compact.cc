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

namespace v8 { namespace internal {

// -------------------------------------------------------------------------
// MarkCompactCollector

bool MarkCompactCollector::compacting_collection_ = false;

int MarkCompactCollector::previous_marked_count_ = 0;
GCTracer* MarkCompactCollector::tracer_ = NULL;


#ifdef DEBUG
MarkCompactCollector::CollectorState MarkCompactCollector::state_ = IDLE;

// Counters used for debugging the marking phase of mark-compact or mark-sweep
// collection.
int MarkCompactCollector::live_bytes_ = 0;
int MarkCompactCollector::live_young_objects_ = 0;
int MarkCompactCollector::live_old_data_objects_ = 0;
int MarkCompactCollector::live_old_pointer_objects_ = 0;
int MarkCompactCollector::live_code_objects_ = 0;
int MarkCompactCollector::live_map_objects_ = 0;
int MarkCompactCollector::live_lo_objects_ = 0;
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

  if (compacting_collection_) {
    EncodeForwardingAddresses();

    UpdatePointers();

    RelocateObjects();

    RebuildRSets();

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

  static const int kFragmentationLimit = 50;  // Percent.
#ifdef DEBUG
  ASSERT(state_ == IDLE);
  state_ = PREPARE_GC;
#endif
  ASSERT(!FLAG_always_compact || !FLAG_never_compact);

  compacting_collection_ = FLAG_always_compact;

  // We compact the old generation if it gets too fragmented (ie, we could
  // recover an expected amount of space by reclaiming the waste and free
  // list blocks).  We always compact when the flag --gc-global is true
  // because objects do not get promoted out of new space on non-compacting
  // GCs.
  if (!compacting_collection_) {
    int old_gen_recoverable = 0;
    int old_gen_used = 0;

    OldSpaces spaces;
    while (OldSpace* space = spaces.next()) {
      old_gen_recoverable += space->Waste() + space->AvailableFree();
      old_gen_used += space->Size();
    }
    int old_gen_fragmentation =
      static_cast<int>((old_gen_recoverable * 100.0) / old_gen_used);
    if (old_gen_fragmentation > kFragmentationLimit) {
      compacting_collection_ = true;
    }
  }

  if (FLAG_never_compact) compacting_collection_ = false;
  if (FLAG_collect_maps) CreateBackPointers();

#ifdef DEBUG
  if (compacting_collection_) {
    // We will write bookkeeping information to the remembered set area
    // starting now.
    Page::set_rset_state(Page::NOT_IN_USE);
  }
#endif

  PagedSpaces spaces;
  while (PagedSpace* space = spaces.next()) {
    space->PrepareForMarkCompact(compacting_collection_);
  }

#ifdef DEBUG
  live_bytes_ = 0;
  live_young_objects_ = 0;
  live_old_pointer_objects_ = 0;
  live_old_data_objects_ = 0;
  live_code_objects_ = 0;
  live_map_objects_ = 0;
  live_lo_objects_ = 0;
#endif
}


void MarkCompactCollector::Finish() {
#ifdef DEBUG
  ASSERT(state_ == SWEEP_SPACES || state_ == REBUILD_RSETS);
  state_ = IDLE;
#endif
  // The stub cache is not traversed during GC; clear the cache to
  // force lazy re-initialization of it. This must be done after the
  // GC, because it relies on the new address of certain old space
  // objects (empty string, illegal builtin).
  StubCache::Clear();
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
  if (reinterpret_cast<String*>(second) != Heap::empty_string()) return object;

  // Since we don't have the object's start, it is impossible to update the
  // remembered set.  Therefore, we only replace the string with its left
  // substring when the remembered set does not change.
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

  void BeginCodeIteration(Code* code) {
    // When iterating over a code object during marking
    // ic targets are derived pointers.
    ASSERT(code->ic_flag() == Code::IC_TARGET_IS_ADDRESS);
  }

  void EndCodeIteration(Code* code) {
    // If this is a compacting collection, set ic targets
    // are pointing to object headers.
    if (IsCompacting()) code->set_ic_flag(Code::IC_TARGET_IS_OBJECT);
  }

  void VisitCodeTarget(RelocInfo* rinfo) {
    ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
    Code* code = CodeFromDerivedPointer(rinfo->target_address());
    if (FLAG_cleanup_ics_at_gc && code->is_inline_cache_stub()) {
      IC::Clear(rinfo->pc());
      // Please note targets for cleared inline cached do not have to be
      // marked since they are contained in Heap::non_monomorphic_cache().
    } else {
      MarkCompactCollector::MarkObject(code);
    }
    if (IsCompacting()) {
      // When compacting we convert the target to a real object pointer.
      code = CodeFromDerivedPointer(rinfo->target_address());
      rinfo->set_target_object(code);
    }
  }

  void VisitDebugTarget(RelocInfo* rinfo) {
    ASSERT(RelocInfo::IsJSReturn(rinfo->rmode()) &&
           rinfo->IsCallInstruction());
    HeapObject* code = CodeFromDerivedPointer(rinfo->call_address());
    MarkCompactCollector::MarkObject(code);
    // When compacting we convert the call to a real object pointer.
    if (IsCompacting()) rinfo->set_call_object(code);
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

  // Retrieves the Code pointer from derived code entry.
  Code* CodeFromDerivedPointer(Address addr) {
    ASSERT(addr != NULL);
    return reinterpret_cast<Code*>(
        HeapObject::FromAddress(addr - Code::kHeaderSize));
  }

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
  void VisitPointers(Object** start, Object** end) {
    // Visit all HeapObject pointers in [start, end).
    for (Object** p = start; p < end; p++) {
      if ((*p)->IsHeapObject() && !HeapObject::cast(*p)->IsMarked()) {
        // Check if the symbol being pruned is an external symbol. We need to
        // delete the associated external data as this symbol is going away.

        // Since the object is not marked we can access its map word safely
        // without having to worry about marking bits in the object header.
        Map* map = HeapObject::cast(*p)->map();
        // Since no objects have yet been moved we can safely access the map of
        // the object.
        uint32_t type = map->instance_type();
        bool is_external = (type & kStringRepresentationMask) ==
                           kExternalStringTag;
        if (is_external) {
          bool is_two_byte = (type & kStringEncodingMask) == kTwoByteStringTag;
          byte* resource_addr = reinterpret_cast<byte*>(*p) +
                                ExternalString::kResourceOffset -
                                kHeapObjectTag;
          if (is_two_byte) {
            v8::String::ExternalStringResource** resource =
                reinterpret_cast<v8::String::ExternalStringResource**>
                (resource_addr);
            delete *resource;
            // Clear the resource pointer in the symbol.
            *resource = NULL;
          } else {
            v8::String::ExternalAsciiStringResource** resource =
                reinterpret_cast<v8::String::ExternalAsciiStringResource**>
                (resource_addr);
            delete *resource;
            // Clear the resource pointer in the symbol.
            *resource = NULL;
          }
        }
        // Set the entry to null_value (as deleted).
        *p = Heap::null_value();
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
  visitor.VisitPointers(HeapObject::RawField(map, Map::kPrototypeOffset),
                        HeapObject::RawField(map, Map::kSize));
}


void MarkCompactCollector::MarkDescriptorArray(
    DescriptorArray *descriptors) {
  if (descriptors->IsMarked()) return;
  // Empty descriptor array is marked as a root before any maps are marked.
  ASSERT(descriptors != Heap::empty_descriptor_array());
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
  while (iterator.has_next()) {
    Object* next_object = iterator.next();
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

  while (it->has_next()) {
    HeapObject* object = it->next();
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


class SymbolMarkingVisitor : public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object** end) {
    MarkingVisitor marker;
    for (Object** p = start; p < end; p++) {
      if (!(*p)->IsHeapObject()) continue;

      HeapObject* object = HeapObject::cast(*p);
      // If the object is marked, we have marked or are in the process
      // of marking subparts.
      if (object->IsMarked()) continue;

      // The object is unmarked, we do not need to unmark to use its
      // map.
      Map* map = object->map();
      object->IterateBody(map->instance_type(),
                          object->SizeFromMap(map),
                          &marker);
    }
  }
};


void MarkCompactCollector::MarkSymbolTable() {
  // Objects reachable from symbols are marked as live so as to ensure
  // that if the symbol itself remains alive after GC for any reason,
  // and if it is a sliced string or a cons string backed by an
  // external string (even indirectly), then the external string does
  // not receive a weak reference callback.
  SymbolTable* symbol_table = SymbolTable::cast(Heap::symbol_table());
  // Mark the symbol table itself.
  SetMark(symbol_table);
  // Explicitly mark the prefix.
  MarkingVisitor marker;
  symbol_table->IteratePrefix(&marker);
  ProcessMarkingStack(&marker);
  // Mark subparts of the symbols but not the symbols themselves
  // (unless reachable from another symbol).
  SymbolMarkingVisitor symbol_marker;
  symbol_table->IterateElements(&symbol_marker);
  ProcessMarkingStack(&marker);
}


void MarkCompactCollector::MarkRoots(RootMarkingVisitor* visitor) {
  // Mark the heap roots including global variables, stack variables,
  // etc., and all objects reachable from them.
  Heap::IterateStrongRoots(visitor);

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
  // symbol table.  Cannot use SymbolTable::cast here because the symbol
  // table is marked.
  SymbolTable* symbol_table =
      reinterpret_cast<SymbolTable*>(Heap::symbol_table());
  SymbolTableCleaner v;
  symbol_table->IterateElements(&v);
  symbol_table->ElementsRemoved(v.PointersRemoved());

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
    live_young_objects_++;
  } else if (Heap::map_space()->Contains(obj)) {
    ASSERT(obj->IsMap());
    live_map_objects_++;
  } else if (Heap::old_pointer_space()->Contains(obj)) {
    live_old_pointer_objects_++;
  } else if (Heap::old_data_space()->Contains(obj)) {
    live_old_data_objects_++;
  } else if (Heap::code_space()->Contains(obj)) {
    live_code_objects_++;
  } else if (Heap::lo_space()->Contains(obj)) {
    live_lo_objects_++;
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
  // All of these actions are carried out only on maps of JSObects
  // and related subtypes.
  while (map_iterator.has_next()) {
    Map* map = reinterpret_cast<Map*>(map_iterator.next());
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
    Object *next;
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
//  31             21 20              10 9               0
// +-----------------+------------------+-----------------+
// |forwarding offset|page offset of map|page index of map|
// +-----------------+------------------+-----------------+
//  11 bits           11 bits            10 bits
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
// strings are promoted to the code space, all others to the old space.
inline Object* MCAllocateFromNewSpace(HeapObject* object, int object_size) {
  OldSpace* target_space = Heap::TargetSpace(object);
  ASSERT(target_space == Heap::old_pointer_space() ||
         target_space == Heap::old_data_space());
  Object* forwarded = target_space->MCAllocateRaw(object_size);

  if (forwarded->IsFailure()) {
    forwarded = Heap::new_space()->MCAllocateRaw(object_size);
  }
  return forwarded;
}


// Allocation functions for the paged spaces call the space's MCAllocateRaw.
inline Object* MCAllocateFromOldPointerSpace(HeapObject* object,
                                             int object_size) {
  return Heap::old_pointer_space()->MCAllocateRaw(object_size);
}


inline Object* MCAllocateFromOldDataSpace(HeapObject* object, int object_size) {
  return Heap::old_data_space()->MCAllocateRaw(object_size);
}


inline Object* MCAllocateFromCodeSpace(HeapObject* object, int object_size) {
  return Heap::code_space()->MCAllocateRaw(object_size);
}


inline Object* MCAllocateFromMapSpace(HeapObject* object, int object_size) {
  return Heap::map_space()->MCAllocateRaw(object_size);
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


// A code deletion event is logged for non-live code objects.
inline void LogNonLiveCodeObject(HeapObject* object) {
  if (object->IsCode()) LOG(CodeDeleteEvent(object->address()));
}


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
        EncodeFreeRegion(free_start, current - free_start);
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
  if (!is_prev_alive) EncodeFreeRegion(free_start, end - free_start);
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


static void SweepSpace(NewSpace* space) {
  HeapObject* object;
  for (Address current = space->bottom();
       current < space->top();
       current += object->Size()) {
    object = HeapObject::FromAddress(current);
    if (object->IsMarked()) {
      object->ClearMark();
      MarkCompactCollector::tracer()->decrement_marked_count();
    } else {
      // We give non-live objects a map that will correctly give their size,
      // since their existing map might not be live after the collection.
      int size = object->Size();
      if (size >= Array::kHeaderSize) {
        object->set_map(Heap::byte_array_map());
        ByteArray::cast(object)->set_length(ByteArray::LengthFor(size));
      } else {
        ASSERT(size == kPointerSize);
        object->set_map(Heap::one_word_filler_map());
      }
      ASSERT(object->Size() == size);
    }
    // The object is now unmarked for the call to Size() at the top of the
    // loop.
  }
}


static void SweepSpace(PagedSpace* space, DeallocateFunction dealloc) {
  PageIterator it(space, PageIterator::PAGES_IN_USE);
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
        if (MarkCompactCollector::IsCompacting() && object->IsCode()) {
          // If this is compacting collection marked code objects have had
          // their IC targets converted to objects.
          // They need to be converted back to addresses.
          Code::cast(object)->ConvertICTargetsFromObjectToAddress();
        }
        if (!is_previous_alive) {  // Transition from free to live.
          dealloc(free_start, current - free_start);
          is_previous_alive = true;
        }
      } else {
        if (object->IsCode()) {
          // Notify the logger that compiled code has been collected.
          LOG(CodeDeleteEvent(Code::cast(object)->address()));
        }
        if (is_previous_alive) {  // Transition from live to free.
          free_start = current;
          is_previous_alive = false;
        }
      }
      // The object is now unmarked for the call to Size() at the top of the
      // loop.
    }

    // If the last region was not live we need to from free_start to the
    // allocation top in the page.
    if (!is_previous_alive) {
      int free_size = p->AllocationTop() - free_start;
      if (free_size > 0) {
        dealloc(free_start, free_size);
      }
    }
  }
}


void MarkCompactCollector::DeallocateOldPointerBlock(Address start,
                                                     int size_in_bytes) {
  Heap::ClearRSetRange(start, size_in_bytes);
  Heap::old_pointer_space()->Free(start, size_in_bytes);
}


void MarkCompactCollector::DeallocateOldDataBlock(Address start,
                                                  int size_in_bytes) {
  Heap::old_data_space()->Free(start, size_in_bytes);
}


void MarkCompactCollector::DeallocateCodeBlock(Address start,
                                               int size_in_bytes) {
  Heap::code_space()->Free(start, size_in_bytes);
}


void MarkCompactCollector::DeallocateMapBlock(Address start,
                                              int size_in_bytes) {
  // Objects in map space are frequently assumed to have size Map::kSize and a
  // valid map in their first word.  Thus, we break the free block up into
  // chunks and free them separately.
  ASSERT(size_in_bytes % Map::kSize == 0);
  Heap::ClearRSetRange(start, size_in_bytes);
  Address end = start + size_in_bytes;
  for (Address a = start; a < end; a += Map::kSize) {
    Heap::map_space()->Free(a);
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
                                        IgnoreNonLiveObject>(
      Heap::old_pointer_space());

  EncodeForwardingAddressesInPagedSpace<MCAllocateFromOldDataSpace,
                                        IgnoreNonLiveObject>(
      Heap::old_data_space());

  EncodeForwardingAddressesInPagedSpace<MCAllocateFromCodeSpace,
                                        LogNonLiveCodeObject>(
      Heap::code_space());

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
}


void MarkCompactCollector::SweepSpaces() {
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
  SweepSpace(Heap::new_space());
  SweepSpace(Heap::map_space(), &DeallocateMapBlock);
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
  int live_objects = 0;
  Address current = start;
  while (current < end) {
    uint32_t encoded_map = Memory::uint32_at(current);
    if (encoded_map == kSingleFreeEncoding) {
      current += kPointerSize;
    } else if (encoded_map == kMultiFreeEncoding) {
      current += Memory::int_at(current + kIntSize);
    } else {
      live_objects++;
      current += size_func(HeapObject::FromAddress(current));
    }
  }
  return live_objects;
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

 private:
  void UpdatePointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;

    HeapObject* obj = HeapObject::cast(*p);
    Address old_addr = obj->address();
    Address new_addr;
    ASSERT(!Heap::InFromSpace(obj));

    if (Heap::new_space()->Contains(obj)) {
      Address f_addr = Heap::new_space()->FromSpaceLow() +
                       Heap::new_space()->ToSpaceOffsetForAddress(old_addr);
      new_addr = Memory::Address_at(f_addr);

#ifdef DEBUG
      ASSERT(Heap::old_pointer_space()->Contains(new_addr) ||
             Heap::old_data_space()->Contains(new_addr) ||
             Heap::code_space()->Contains(new_addr) ||
             Heap::new_space()->FromSpaceContains(new_addr));

      if (Heap::new_space()->FromSpaceContains(new_addr)) {
        ASSERT(Heap::new_space()->FromSpaceOffsetForAddress(new_addr) <=
               Heap::new_space()->ToSpaceOffsetForAddress(old_addr));
      }
#endif

    } else if (Heap::lo_space()->Contains(obj)) {
      // Don't move objects in the large object space.
      return;

    } else {
      ASSERT(Heap::old_pointer_space()->Contains(obj) ||
             Heap::old_data_space()->Contains(obj) ||
             Heap::code_space()->Contains(obj) ||
             Heap::map_space()->Contains(obj));

      new_addr = MarkCompactCollector::GetForwardingAddressInOldSpace(obj);
      ASSERT(Heap::old_pointer_space()->Contains(new_addr) ||
             Heap::old_data_space()->Contains(new_addr) ||
             Heap::code_space()->Contains(new_addr) ||
             Heap::map_space()->Contains(new_addr));

#ifdef DEBUG
      if (Heap::old_pointer_space()->Contains(obj)) {
        ASSERT(Heap::old_pointer_space()->MCSpaceOffsetForAddress(new_addr) <=
               Heap::old_pointer_space()->MCSpaceOffsetForAddress(old_addr));
      } else if (Heap::old_data_space()->Contains(obj)) {
        ASSERT(Heap::old_data_space()->MCSpaceOffsetForAddress(new_addr) <=
               Heap::old_data_space()->MCSpaceOffsetForAddress(old_addr));
      } else if (Heap::code_space()->Contains(obj)) {
        ASSERT(Heap::code_space()->MCSpaceOffsetForAddress(new_addr) <=
               Heap::code_space()->MCSpaceOffsetForAddress(old_addr));
      } else {
        ASSERT(Heap::map_space()->MCSpaceOffsetForAddress(new_addr) <=
               Heap::map_space()->MCSpaceOffsetForAddress(old_addr));
      }
#endif
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
  Heap::IterateRoots(&updating_visitor);
  GlobalHandles::IterateWeakRoots(&updating_visitor);

  int live_maps = IterateLiveObjects(Heap::map_space(),
                                     &UpdatePointersInOldObject);
  int live_pointer_olds = IterateLiveObjects(Heap::old_pointer_space(),
                                             &UpdatePointersInOldObject);
  int live_data_olds = IterateLiveObjects(Heap::old_data_space(),
                                          &UpdatePointersInOldObject);
  int live_codes = IterateLiveObjects(Heap::code_space(),
                                      &UpdatePointersInOldObject);
  int live_news = IterateLiveObjects(Heap::new_space(),
                                     &UpdatePointersInNewObject);

  // Large objects do not move, the map word can be updated directly.
  LargeObjectIterator it(Heap::lo_space());
  while (it.has_next()) UpdatePointersInNewObject(it.next());

  USE(live_maps);
  USE(live_pointer_olds);
  USE(live_data_olds);
  USE(live_codes);
  USE(live_news);

#ifdef DEBUG
  ASSERT(live_maps == live_map_objects_);
  ASSERT(live_data_olds == live_old_data_objects_);
  ASSERT(live_pointer_olds == live_old_pointer_objects_);
  ASSERT(live_codes == live_code_objects_);
  ASSERT(live_news == live_young_objects_);
#endif
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

  // Find end of allocation of in the page of first_forwarded.
  Address mc_top = forwarded_page->mc_relocation_top;
  int mc_top_offset = forwarded_page->Offset(mc_top);

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
  ASSERT(next_page->OffsetToAddress(offset) < next_page->mc_relocation_top);

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
  int live_maps = IterateLiveObjects(Heap::map_space(), &RelocateMapObject);
  int live_pointer_olds = IterateLiveObjects(Heap::old_pointer_space(),
                                             &RelocateOldPointerObject);
  int live_data_olds = IterateLiveObjects(Heap::old_data_space(),
                                          &RelocateOldDataObject);
  int live_codes = IterateLiveObjects(Heap::code_space(), &RelocateCodeObject);
  int live_news = IterateLiveObjects(Heap::new_space(), &RelocateNewObject);

  USE(live_maps);
  USE(live_data_olds);
  USE(live_pointer_olds);
  USE(live_codes);
  USE(live_news);
#ifdef DEBUG
  ASSERT(live_maps == live_map_objects_);
  ASSERT(live_data_olds == live_old_data_objects_);
  ASSERT(live_pointer_olds == live_old_pointer_objects_);
  ASSERT(live_codes == live_code_objects_);
  ASSERT(live_news == live_young_objects_);
#endif

  // Notify code object in LO to convert IC target to address
  // This must happen after lo_space_->Compact
  LargeObjectIterator it(Heap::lo_space());
  while (it.has_next()) { ConvertCodeICTargetToAddress(it.next()); }

  // Flips from and to spaces
  Heap::new_space()->Flip();

  // Sets age_mark to bottom in to space
  Address mark = Heap::new_space()->bottom();
  Heap::new_space()->set_age_mark(mark);

  Heap::new_space()->MCCommitRelocationInfo();
#ifdef DEBUG
  // It is safe to write to the remembered sets as remembered sets on a
  // page-by-page basis after committing the m-c forwarding pointer.
  Page::set_rset_state(Page::IN_USE);
#endif
  PagedSpaces spaces;
  while (PagedSpace* space = spaces.next()) space->MCCommitRelocationInfo();
}


int MarkCompactCollector::ConvertCodeICTargetToAddress(HeapObject* obj) {
  if (obj->IsCode()) {
    Code::cast(obj)->ConvertICTargetsFromObjectToAddress();
  }
  return obj->Size();
}


int MarkCompactCollector::RelocateMapObject(HeapObject* obj) {
  // decode map pointer (forwarded address)
  MapWord encoding = obj->map_word();
  Address map_addr = encoding.DecodeMapAddress(Heap::map_space());
  ASSERT(Heap::map_space()->Contains(HeapObject::FromAddress(map_addr)));

  // Get forwarding address before resetting map pointer
  Address new_addr = GetForwardingAddressInOldSpace(obj);

  // recover map pointer
  obj->set_map(reinterpret_cast<Map*>(HeapObject::FromAddress(map_addr)));

  // The meta map object may not be copied yet.
  Address old_addr = obj->address();

  if (new_addr != old_addr) {
    memmove(new_addr, old_addr, Map::kSize);  // copy contents
  }

#ifdef DEBUG
  if (FLAG_gc_verbose) {
    PrintF("relocate %p -> %p\n", old_addr, new_addr);
  }
#endif

  return Map::kSize;
}


static inline int RelocateOldObject(HeapObject* obj,
                                    OldSpace* space,
                                    Address new_addr,
                                    Address map_addr) {
  // recover map pointer
  obj->set_map(reinterpret_cast<Map*>(HeapObject::FromAddress(map_addr)));

  // This is a non-map object, it relies on the assumption that the Map space
  // is compacted before the Old space (see RelocateObjects).
  int obj_size = obj->Size();
  ASSERT_OBJECT_SIZE(obj_size);

  ASSERT(space->MCSpaceOffsetForAddress(new_addr) <=
         space->MCSpaceOffsetForAddress(obj->address()));

  space->MCAdjustRelocationEnd(new_addr, obj_size);

#ifdef DEBUG
  if (FLAG_gc_verbose) {
    PrintF("relocate %p -> %p\n", obj->address(), new_addr);
  }
#endif

  return obj_size;
}


int MarkCompactCollector::RelocateOldNonCodeObject(HeapObject* obj,
                                                   OldSpace* space) {
  // decode map pointer (forwarded address)
  MapWord encoding = obj->map_word();
  Address map_addr = encoding.DecodeMapAddress(Heap::map_space());
  ASSERT(Heap::map_space()->Contains(map_addr));

  // Get forwarding address before resetting map pointer
  Address new_addr = GetForwardingAddressInOldSpace(obj);

  int obj_size = RelocateOldObject(obj, space, new_addr, map_addr);

  Address old_addr = obj->address();

  if (new_addr != old_addr) {
    memmove(new_addr, old_addr, obj_size);  // copy contents
  }

  ASSERT(!HeapObject::FromAddress(new_addr)->IsCode());

  return obj_size;
}


int MarkCompactCollector::RelocateOldPointerObject(HeapObject* obj) {
  return RelocateOldNonCodeObject(obj, Heap::old_pointer_space());
}


int MarkCompactCollector::RelocateOldDataObject(HeapObject* obj) {
  return RelocateOldNonCodeObject(obj, Heap::old_data_space());
}


int MarkCompactCollector::RelocateCodeObject(HeapObject* obj) {
  // decode map pointer (forwarded address)
  MapWord encoding = obj->map_word();
  Address map_addr = encoding.DecodeMapAddress(Heap::map_space());
  ASSERT(Heap::map_space()->Contains(HeapObject::FromAddress(map_addr)));

  // Get forwarding address before resetting map pointer
  Address new_addr = GetForwardingAddressInOldSpace(obj);

  int obj_size = RelocateOldObject(obj, Heap::code_space(), new_addr, map_addr);

  // convert inline cache target to address using old address
  if (obj->IsCode()) {
    // convert target to address first related to old_address
    Code::cast(obj)->ConvertICTargetsFromObjectToAddress();
  }

  Address old_addr = obj->address();

  if (new_addr != old_addr) {
    memmove(new_addr, old_addr, obj_size);  // copy contents
  }

  HeapObject* copied_to = HeapObject::FromAddress(new_addr);
  if (copied_to->IsCode()) {
    // may also update inline cache target.
    Code::cast(copied_to)->Relocate(new_addr - old_addr);
    // Notify the logger that compiled code has moved.
    LOG(CodeMoveEvent(old_addr, new_addr));
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

  if (Heap::new_space()->FromSpaceContains(new_addr)) {
    ASSERT(Heap::new_space()->FromSpaceOffsetForAddress(new_addr) <=
           Heap::new_space()->ToSpaceOffsetForAddress(old_addr));
  } else {
    OldSpace* target_space = Heap::TargetSpace(obj);
    ASSERT(target_space == Heap::old_pointer_space() ||
           target_space == Heap::old_data_space());
    target_space->MCAdjustRelocationEnd(new_addr, obj_size);
  }

  // New and old addresses cannot overlap.
  memcpy(reinterpret_cast<void*>(new_addr),
         reinterpret_cast<void*>(old_addr),
         obj_size);

#ifdef DEBUG
  if (FLAG_gc_verbose) {
    PrintF("relocate %p -> %p\n", old_addr, new_addr);
  }
#endif

  return obj_size;
}


// -------------------------------------------------------------------------
// Phase 5: rebuild remembered sets

void MarkCompactCollector::RebuildRSets() {
#ifdef DEBUG
  ASSERT(state_ == RELOCATE_OBJECTS);
  state_ = REBUILD_RSETS;
#endif
  Heap::RebuildRSets();
}

} }  // namespace v8::internal
