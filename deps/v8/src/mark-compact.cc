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

#include "v8.h"

#include "compilation-cache.h"
#include "execution.h"
#include "heap-profiler.h"
#include "gdb-jit.h"
#include "global-handles.h"
#include "ic-inl.h"
#include "liveobjectlist-inl.h"
#include "mark-compact.h"
#include "objects-visiting.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// MarkCompactCollector

MarkCompactCollector::MarkCompactCollector() :  // NOLINT
#ifdef DEBUG
      state_(IDLE),
#endif
      force_compaction_(false),
      compacting_collection_(false),
      compact_on_next_gc_(false),
      previous_marked_count_(0),
      tracer_(NULL),
#ifdef DEBUG
      live_young_objects_size_(0),
      live_old_pointer_objects_size_(0),
      live_old_data_objects_size_(0),
      live_code_objects_size_(0),
      live_map_objects_size_(0),
      live_cell_objects_size_(0),
      live_lo_objects_size_(0),
      live_bytes_(0),
#endif
      heap_(NULL),
      code_flusher_(NULL),
      encountered_weak_maps_(NULL) { }


void MarkCompactCollector::CollectGarbage() {
  // Make sure that Prepare() has been called. The individual steps below will
  // update the state as they proceed.
  ASSERT(state_ == PREPARE_GC);
  ASSERT(encountered_weak_maps_ == Smi::FromInt(0));

  // Prepare has selected whether to compact the old generation or not.
  // Tell the tracer.
  if (IsCompacting()) tracer_->set_is_compacting();

  MarkLiveObjects();

  if (FLAG_collect_maps) ClearNonLiveTransitions();

  ClearWeakMaps();

  SweepLargeObjectSpace();

  if (IsCompacting()) {
    GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_COMPACT);
    EncodeForwardingAddresses();

    heap()->MarkMapPointersAsEncoded(true);
    UpdatePointers();
    heap()->MarkMapPointersAsEncoded(false);
    heap()->isolate()->pc_to_code_cache()->Flush();

    RelocateObjects();
  } else {
    SweepSpaces();
    heap()->isolate()->pc_to_code_cache()->Flush();
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
  if (!heap()->map_space()->MapPointersEncodable())
      compacting_collection_ = false;
  if (FLAG_collect_maps) CreateBackPointers();
#ifdef ENABLE_GDB_JIT_INTERFACE
  if (FLAG_gdbjit) {
    // If GDBJIT interface is active disable compaction.
    compacting_collection_ = false;
  }
#endif

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
  heap()->isolate()->stub_cache()->Clear();

  heap()->external_string_table_.CleanUp();

  // If we've just compacted old space there's no reason to check the
  // fragmentation limit. Just return.
  if (HasCompacted()) return;

  // We compact the old generation on the next GC if it has gotten too
  // fragmented (ie, we could recover an expected amount of space by
  // reclaiming the waste and free list blocks).
  static const int kFragmentationLimit = 15;        // Percent.
  static const int kFragmentationAllowed = 1 * MB;  // Absolute.
  intptr_t old_gen_recoverable = 0;
  intptr_t old_gen_used = 0;

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

class CodeFlusher {
 public:
  explicit CodeFlusher(Isolate* isolate)
      : isolate_(isolate),
        jsfunction_candidates_head_(NULL),
        shared_function_info_candidates_head_(NULL) {}

  void AddCandidate(SharedFunctionInfo* shared_info) {
    SetNextCandidate(shared_info, shared_function_info_candidates_head_);
    shared_function_info_candidates_head_ = shared_info;
  }

  void AddCandidate(JSFunction* function) {
    ASSERT(function->unchecked_code() ==
           function->unchecked_shared()->unchecked_code());

    SetNextCandidate(function, jsfunction_candidates_head_);
    jsfunction_candidates_head_ = function;
  }

  void ProcessCandidates() {
    ProcessSharedFunctionInfoCandidates();
    ProcessJSFunctionCandidates();
  }

 private:
  void ProcessJSFunctionCandidates() {
    Code* lazy_compile = isolate_->builtins()->builtin(Builtins::kLazyCompile);

    JSFunction* candidate = jsfunction_candidates_head_;
    JSFunction* next_candidate;
    while (candidate != NULL) {
      next_candidate = GetNextCandidate(candidate);

      SharedFunctionInfo* shared = candidate->unchecked_shared();

      Code* code = shared->unchecked_code();
      if (!code->IsMarked()) {
        shared->set_code(lazy_compile);
        candidate->set_code(lazy_compile);
      } else {
        candidate->set_code(shared->unchecked_code());
      }

      candidate = next_candidate;
    }

    jsfunction_candidates_head_ = NULL;
  }


  void ProcessSharedFunctionInfoCandidates() {
    Code* lazy_compile = isolate_->builtins()->builtin(Builtins::kLazyCompile);

    SharedFunctionInfo* candidate = shared_function_info_candidates_head_;
    SharedFunctionInfo* next_candidate;
    while (candidate != NULL) {
      next_candidate = GetNextCandidate(candidate);
      SetNextCandidate(candidate, NULL);

      Code* code = candidate->unchecked_code();
      if (!code->IsMarked()) {
        candidate->set_code(lazy_compile);
      }

      candidate = next_candidate;
    }

    shared_function_info_candidates_head_ = NULL;
  }

  static JSFunction** GetNextCandidateField(JSFunction* candidate) {
    return reinterpret_cast<JSFunction**>(
        candidate->address() + JSFunction::kCodeEntryOffset);
  }

  static JSFunction* GetNextCandidate(JSFunction* candidate) {
    return *GetNextCandidateField(candidate);
  }

  static void SetNextCandidate(JSFunction* candidate,
                               JSFunction* next_candidate) {
    *GetNextCandidateField(candidate) = next_candidate;
  }

  static SharedFunctionInfo** GetNextCandidateField(
      SharedFunctionInfo* candidate) {
    Code* code = candidate->unchecked_code();
    return reinterpret_cast<SharedFunctionInfo**>(
        code->address() + Code::kNextCodeFlushingCandidateOffset);
  }

  static SharedFunctionInfo* GetNextCandidate(SharedFunctionInfo* candidate) {
    return *GetNextCandidateField(candidate);
  }

  static void SetNextCandidate(SharedFunctionInfo* candidate,
                               SharedFunctionInfo* next_candidate) {
    *GetNextCandidateField(candidate) = next_candidate;
  }

  Isolate* isolate_;
  JSFunction* jsfunction_candidates_head_;
  SharedFunctionInfo* shared_function_info_candidates_head_;

  DISALLOW_COPY_AND_ASSIGN(CodeFlusher);
};


MarkCompactCollector::~MarkCompactCollector() {
  if (code_flusher_ != NULL) {
    delete code_flusher_;
    code_flusher_ = NULL;
  }
}


static inline HeapObject* ShortCircuitConsString(Object** p) {
  // Optimization: If the heap object pointed to by p is a non-symbol
  // cons string whose right substring is HEAP->empty_string, update
  // it in place to its left substring.  Return the updated value.
  //
  // Here we assume that if we change *p, we replace it with a heap object
  // (ie, the left substring of a cons string is always a heap object).
  //
  // The check performed is:
  //   object->IsConsString() && !object->IsSymbol() &&
  //   (ConsString::cast(object)->second() == HEAP->empty_string())
  // except the maps for the object and its possible substrings might be
  // marked.
  HeapObject* object = HeapObject::cast(*p);
  MapWord map_word = object->map_word();
  map_word.ClearMark();
  InstanceType type = map_word.ToMap()->instance_type();
  if ((type & kShortcutTypeMask) != kShortcutTypeTag) return object;

  Object* second = reinterpret_cast<ConsString*>(object)->unchecked_second();
  Heap* heap = map_word.ToMap()->heap();
  if (second != heap->raw_unchecked_empty_string()) {
    return object;
  }

  // Since we don't have the object's start, it is impossible to update the
  // page dirty marks. Therefore, we only replace the string with its left
  // substring when page dirty marks do not change.
  Object* first = reinterpret_cast<ConsString*>(object)->unchecked_first();
  if (!heap->InNewSpace(object) && heap->InNewSpace(first)) return object;

  *p = first;
  return HeapObject::cast(first);
}


class StaticMarkingVisitor : public StaticVisitorBase {
 public:
  static inline void IterateBody(Map* map, HeapObject* obj) {
    table_.GetVisitor(map)(map, obj);
  }

  static void Initialize() {
    table_.Register(kVisitShortcutCandidate,
                    &FixedBodyVisitor<StaticMarkingVisitor,
                                      ConsString::BodyDescriptor,
                                      void>::Visit);

    table_.Register(kVisitConsString,
                    &FixedBodyVisitor<StaticMarkingVisitor,
                                      ConsString::BodyDescriptor,
                                      void>::Visit);

    table_.Register(kVisitSlicedString,
                    &FixedBodyVisitor<StaticMarkingVisitor,
                                      SlicedString::BodyDescriptor,
                                      void>::Visit);

    table_.Register(kVisitFixedArray,
                    &FlexibleBodyVisitor<StaticMarkingVisitor,
                                         FixedArray::BodyDescriptor,
                                         void>::Visit);

    table_.Register(kVisitFixedDoubleArray, DataObjectVisitor::Visit);

    table_.Register(kVisitGlobalContext,
                    &FixedBodyVisitor<StaticMarkingVisitor,
                                      Context::MarkCompactBodyDescriptor,
                                      void>::Visit);

    table_.Register(kVisitByteArray, &DataObjectVisitor::Visit);
    table_.Register(kVisitSeqAsciiString, &DataObjectVisitor::Visit);
    table_.Register(kVisitSeqTwoByteString, &DataObjectVisitor::Visit);

    table_.Register(kVisitJSWeakMap, &VisitJSWeakMap);

    table_.Register(kVisitOddball,
                    &FixedBodyVisitor<StaticMarkingVisitor,
                                      Oddball::BodyDescriptor,
                                      void>::Visit);
    table_.Register(kVisitMap,
                    &FixedBodyVisitor<StaticMarkingVisitor,
                                      Map::BodyDescriptor,
                                      void>::Visit);

    table_.Register(kVisitCode, &VisitCode);

    table_.Register(kVisitSharedFunctionInfo,
                    &VisitSharedFunctionInfoAndFlushCode);

    table_.Register(kVisitJSFunction,
                    &VisitJSFunctionAndFlushCode);

    table_.Register(kVisitJSRegExp,
                    &VisitRegExpAndFlushCode);

    table_.Register(kVisitPropertyCell,
                    &FixedBodyVisitor<StaticMarkingVisitor,
                                      JSGlobalPropertyCell::BodyDescriptor,
                                      void>::Visit);

    table_.RegisterSpecializations<DataObjectVisitor,
                                   kVisitDataObject,
                                   kVisitDataObjectGeneric>();

    table_.RegisterSpecializations<JSObjectVisitor,
                                   kVisitJSObject,
                                   kVisitJSObjectGeneric>();

    table_.RegisterSpecializations<StructObjectVisitor,
                                   kVisitStruct,
                                   kVisitStructGeneric>();
  }

  INLINE(static void VisitPointer(Heap* heap, Object** p)) {
    MarkObjectByPointer(heap, p);
  }

  INLINE(static void VisitPointers(Heap* heap, Object** start, Object** end)) {
    // Mark all objects pointed to in [start, end).
    const int kMinRangeForMarkingRecursion = 64;
    if (end - start >= kMinRangeForMarkingRecursion) {
      if (VisitUnmarkedObjects(heap, start, end)) return;
      // We are close to a stack overflow, so just mark the objects.
    }
    for (Object** p = start; p < end; p++) MarkObjectByPointer(heap, p);
  }

  static inline void VisitCodeTarget(Heap* heap, RelocInfo* rinfo) {
    ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
    Code* code = Code::GetCodeFromTargetAddress(rinfo->target_address());
    if (FLAG_cleanup_code_caches_at_gc && code->is_inline_cache_stub()) {
      IC::Clear(rinfo->pc());
      // Please note targets for cleared inline cached do not have to be
      // marked since they are contained in HEAP->non_monomorphic_cache().
    } else {
      heap->mark_compact_collector()->MarkObject(code);
    }
  }

  static void VisitGlobalPropertyCell(Heap* heap, RelocInfo* rinfo) {
    ASSERT(rinfo->rmode() == RelocInfo::GLOBAL_PROPERTY_CELL);
    Object* cell = rinfo->target_cell();
    Object* old_cell = cell;
    VisitPointer(heap, &cell);
    if (cell != old_cell) {
      rinfo->set_target_cell(reinterpret_cast<JSGlobalPropertyCell*>(cell));
    }
  }

  static inline void VisitDebugTarget(Heap* heap, RelocInfo* rinfo) {
    ASSERT((RelocInfo::IsJSReturn(rinfo->rmode()) &&
            rinfo->IsPatchedReturnSequence()) ||
           (RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
            rinfo->IsPatchedDebugBreakSlotSequence()));
    HeapObject* code = Code::GetCodeFromTargetAddress(rinfo->call_address());
    heap->mark_compact_collector()->MarkObject(code);
  }

  // Mark object pointed to by p.
  INLINE(static void MarkObjectByPointer(Heap* heap, Object** p)) {
    if (!(*p)->IsHeapObject()) return;
    HeapObject* object = ShortCircuitConsString(p);
    if (!object->IsMarked()) {
      heap->mark_compact_collector()->MarkUnmarkedObject(object);
    }
  }


  // Visit an unmarked object.
  INLINE(static void VisitUnmarkedObject(MarkCompactCollector* collector,
                                         HeapObject* obj)) {
#ifdef DEBUG
    ASSERT(Isolate::Current()->heap()->Contains(obj));
    ASSERT(!obj->IsMarked());
#endif
    Map* map = obj->map();
    collector->SetMark(obj);
    // Mark the map pointer and the body.
    if (!map->IsMarked()) collector->MarkUnmarkedObject(map);
    IterateBody(map, obj);
  }

  // Visit all unmarked objects pointed to by [start, end).
  // Returns false if the operation fails (lack of stack space).
  static inline bool VisitUnmarkedObjects(Heap* heap,
                                          Object** start,
                                          Object** end) {
    // Return false is we are close to the stack limit.
    StackLimitCheck check(heap->isolate());
    if (check.HasOverflowed()) return false;

    MarkCompactCollector* collector = heap->mark_compact_collector();
    // Visit the unmarked objects.
    for (Object** p = start; p < end; p++) {
      if (!(*p)->IsHeapObject()) continue;
      HeapObject* obj = HeapObject::cast(*p);
      if (obj->IsMarked()) continue;
      VisitUnmarkedObject(collector, obj);
    }
    return true;
  }

  static inline void VisitExternalReference(Address* p) { }
  static inline void VisitRuntimeEntry(RelocInfo* rinfo) { }

 private:
  class DataObjectVisitor {
   public:
    template<int size>
    static void VisitSpecialized(Map* map, HeapObject* object) {
    }

    static void Visit(Map* map, HeapObject* object) {
    }
  };

  typedef FlexibleBodyVisitor<StaticMarkingVisitor,
                              JSObject::BodyDescriptor,
                              void> JSObjectVisitor;

  typedef FlexibleBodyVisitor<StaticMarkingVisitor,
                              StructBodyDescriptor,
                              void> StructObjectVisitor;

  static void VisitJSWeakMap(Map* map, HeapObject* object) {
    MarkCompactCollector* collector = map->heap()->mark_compact_collector();
    JSWeakMap* weak_map = reinterpret_cast<JSWeakMap*>(object);

    // Enqueue weak map in linked list of encountered weak maps.
    ASSERT(weak_map->next() == Smi::FromInt(0));
    weak_map->set_next(collector->encountered_weak_maps());
    collector->set_encountered_weak_maps(weak_map);

    // Skip visiting the backing hash table containing the mappings.
    int object_size = JSWeakMap::BodyDescriptor::SizeOf(map, object);
    BodyVisitorBase<StaticMarkingVisitor>::IteratePointers(
        map->heap(),
        object,
        JSWeakMap::BodyDescriptor::kStartOffset,
        JSWeakMap::kTableOffset);
    BodyVisitorBase<StaticMarkingVisitor>::IteratePointers(
        map->heap(),
        object,
        JSWeakMap::kTableOffset + kPointerSize,
        object_size);

    // Mark the backing hash table without pushing it on the marking stack.
    ASSERT(!weak_map->unchecked_table()->IsMarked());
    ASSERT(weak_map->unchecked_table()->map()->IsMarked());
    collector->SetMark(weak_map->unchecked_table());
  }

  static void VisitCode(Map* map, HeapObject* object) {
    reinterpret_cast<Code*>(object)->CodeIterateBody<StaticMarkingVisitor>(
        map->heap());
  }

  // Code flushing support.

  // How many collections newly compiled code object will survive before being
  // flushed.
  static const int kCodeAgeThreshold = 5;

  static const int kRegExpCodeThreshold = 5;

  inline static bool HasSourceCode(Heap* heap, SharedFunctionInfo* info) {
    Object* undefined = heap->raw_unchecked_undefined_value();
    return (info->script() != undefined) &&
        (reinterpret_cast<Script*>(info->script())->source() != undefined);
  }


  inline static bool IsCompiled(JSFunction* function) {
    return function->unchecked_code() !=
        function->GetIsolate()->builtins()->builtin(Builtins::kLazyCompile);
  }

  inline static bool IsCompiled(SharedFunctionInfo* function) {
    return function->unchecked_code() !=
        function->GetIsolate()->builtins()->builtin(Builtins::kLazyCompile);
  }

  inline static bool IsFlushable(Heap* heap, JSFunction* function) {
    SharedFunctionInfo* shared_info = function->unchecked_shared();

    // Code is either on stack, in compilation cache or referenced
    // by optimized version of function.
    if (function->unchecked_code()->IsMarked()) {
      shared_info->set_code_age(0);
      return false;
    }

    // We do not flush code for optimized functions.
    if (function->code() != shared_info->unchecked_code()) {
      return false;
    }

    return IsFlushable(heap, shared_info);
  }

  inline static bool IsFlushable(Heap* heap, SharedFunctionInfo* shared_info) {
    // Code is either on stack, in compilation cache or referenced
    // by optimized version of function.
    if (shared_info->unchecked_code()->IsMarked()) {
      shared_info->set_code_age(0);
      return false;
    }

    // The function must be compiled and have the source code available,
    // to be able to recompile it in case we need the function again.
    if (!(shared_info->is_compiled() && HasSourceCode(heap, shared_info))) {
      return false;
    }

    // We never flush code for Api functions.
    Object* function_data = shared_info->function_data();
    if (function_data->IsHeapObject() &&
        (SafeMap(function_data)->instance_type() ==
         FUNCTION_TEMPLATE_INFO_TYPE)) {
      return false;
    }

    // Only flush code for functions.
    if (shared_info->code()->kind() != Code::FUNCTION) {
      return false;
    }

    // Function must be lazy compilable.
    if (!shared_info->allows_lazy_compilation()) {
      return false;
    }

    // If this is a full script wrapped in a function we do no flush the code.
    if (shared_info->is_toplevel()) {
      return false;
    }

    // Age this shared function info.
    if (shared_info->code_age() < kCodeAgeThreshold) {
      shared_info->set_code_age(shared_info->code_age() + 1);
      return false;
    }

    return true;
  }


  static bool FlushCodeForFunction(Heap* heap, JSFunction* function) {
    if (!IsFlushable(heap, function)) return false;

    // This function's code looks flushable. But we have to postpone the
    // decision until we see all functions that point to the same
    // SharedFunctionInfo because some of them might be optimized.
    // That would make the nonoptimized version of the code nonflushable,
    // because it is required for bailing out from optimized code.
    heap->mark_compact_collector()->code_flusher()->AddCandidate(function);
    return true;
  }


  static inline Map* SafeMap(Object* obj) {
    MapWord map_word = HeapObject::cast(obj)->map_word();
    map_word.ClearMark();
    map_word.ClearOverflow();
    return map_word.ToMap();
  }


  static inline bool IsJSBuiltinsObject(Object* obj) {
    return obj->IsHeapObject() &&
        (SafeMap(obj)->instance_type() == JS_BUILTINS_OBJECT_TYPE);
  }


  static inline bool IsValidNotBuiltinContext(Object* ctx) {
    if (!ctx->IsHeapObject()) return false;

    Map* map = SafeMap(ctx);
    Heap* heap = map->heap();
    if (!(map == heap->raw_unchecked_function_context_map() ||
          map == heap->raw_unchecked_catch_context_map() ||
          map == heap->raw_unchecked_with_context_map() ||
          map == heap->raw_unchecked_global_context_map())) {
      return false;
    }

    Context* context = reinterpret_cast<Context*>(ctx);

    if (IsJSBuiltinsObject(context->global())) {
      return false;
    }

    return true;
  }


  static void VisitSharedFunctionInfoGeneric(Map* map, HeapObject* object) {
    SharedFunctionInfo* shared = reinterpret_cast<SharedFunctionInfo*>(object);

    if (shared->IsInobjectSlackTrackingInProgress()) shared->DetachInitialMap();

    FixedBodyVisitor<StaticMarkingVisitor,
                     SharedFunctionInfo::BodyDescriptor,
                     void>::Visit(map, object);
  }


  static void UpdateRegExpCodeAgeAndFlush(Heap* heap,
                                          JSRegExp* re,
                                          bool is_ascii) {
    // Make sure that the fixed array is in fact initialized on the RegExp.
    // We could potentially trigger a GC when initializing the RegExp.
    if (SafeMap(re->data())->instance_type() != FIXED_ARRAY_TYPE) return;

    // Make sure this is a RegExp that actually contains code.
    if (re->TypeTagUnchecked() != JSRegExp::IRREGEXP) return;

    Object* code = re->DataAtUnchecked(JSRegExp::code_index(is_ascii));
    if (!code->IsSmi() && SafeMap(code)->instance_type() == CODE_TYPE) {
      // Save a copy that can be reinstated if we need the code again.
      re->SetDataAtUnchecked(JSRegExp::saved_code_index(is_ascii),
                             code,
                             heap);
      // Set a number in the 0-255 range to guarantee no smi overflow.
      re->SetDataAtUnchecked(JSRegExp::code_index(is_ascii),
                             Smi::FromInt(heap->sweep_generation() & 0xff),
                             heap);
    } else if (code->IsSmi()) {
      int value = Smi::cast(code)->value();
      // The regexp has not been compiled yet or there was a compilation error.
      if (value == JSRegExp::kUninitializedValue ||
          value == JSRegExp::kCompilationErrorValue) {
        return;
      }

      // Check if we should flush now.
      if (value == ((heap->sweep_generation() - kRegExpCodeThreshold) & 0xff)) {
        re->SetDataAtUnchecked(JSRegExp::code_index(is_ascii),
                               Smi::FromInt(JSRegExp::kUninitializedValue),
                               heap);
        re->SetDataAtUnchecked(JSRegExp::saved_code_index(is_ascii),
                               Smi::FromInt(JSRegExp::kUninitializedValue),
                               heap);
      }
    }
  }


  // Works by setting the current sweep_generation (as a smi) in the
  // code object place in the data array of the RegExp and keeps a copy
  // around that can be reinstated if we reuse the RegExp before flushing.
  // If we did not use the code for kRegExpCodeThreshold mark sweep GCs
  // we flush the code.
  static void VisitRegExpAndFlushCode(Map* map, HeapObject* object) {
    Heap* heap = map->heap();
    MarkCompactCollector* collector = heap->mark_compact_collector();
    if (!collector->is_code_flushing_enabled()) {
      VisitJSRegExpFields(map, object);
      return;
    }
    JSRegExp* re = reinterpret_cast<JSRegExp*>(object);
    // Flush code or set age on both ascii and two byte code.
    UpdateRegExpCodeAgeAndFlush(heap, re, true);
    UpdateRegExpCodeAgeAndFlush(heap, re, false);
    // Visit the fields of the RegExp, including the updated FixedArray.
    VisitJSRegExpFields(map, object);
  }


  static void VisitSharedFunctionInfoAndFlushCode(Map* map,
                                                  HeapObject* object) {
    MarkCompactCollector* collector = map->heap()->mark_compact_collector();
    if (!collector->is_code_flushing_enabled()) {
      VisitSharedFunctionInfoGeneric(map, object);
      return;
    }
    VisitSharedFunctionInfoAndFlushCodeGeneric(map, object, false);
  }


  static void VisitSharedFunctionInfoAndFlushCodeGeneric(
      Map* map, HeapObject* object, bool known_flush_code_candidate) {
    Heap* heap = map->heap();
    SharedFunctionInfo* shared = reinterpret_cast<SharedFunctionInfo*>(object);

    if (shared->IsInobjectSlackTrackingInProgress()) shared->DetachInitialMap();

    if (!known_flush_code_candidate) {
      known_flush_code_candidate = IsFlushable(heap, shared);
      if (known_flush_code_candidate) {
        heap->mark_compact_collector()->code_flusher()->AddCandidate(shared);
      }
    }

    VisitSharedFunctionInfoFields(heap, object, known_flush_code_candidate);
  }


  static void VisitCodeEntry(Heap* heap, Address entry_address) {
    Object* code = Code::GetObjectFromEntryAddress(entry_address);
    Object* old_code = code;
    VisitPointer(heap, &code);
    if (code != old_code) {
      Memory::Address_at(entry_address) =
          reinterpret_cast<Code*>(code)->entry();
    }
  }


  static void VisitJSFunctionAndFlushCode(Map* map, HeapObject* object) {
    Heap* heap = map->heap();
    MarkCompactCollector* collector = heap->mark_compact_collector();
    if (!collector->is_code_flushing_enabled()) {
      VisitJSFunction(map, object);
      return;
    }

    JSFunction* jsfunction = reinterpret_cast<JSFunction*>(object);
    // The function must have a valid context and not be a builtin.
    bool flush_code_candidate = false;
    if (IsValidNotBuiltinContext(jsfunction->unchecked_context())) {
      flush_code_candidate = FlushCodeForFunction(heap, jsfunction);
    }

    if (!flush_code_candidate) {
      collector->MarkObject(jsfunction->unchecked_shared()->unchecked_code());

      if (jsfunction->unchecked_code()->kind() == Code::OPTIMIZED_FUNCTION) {
        collector->MarkInlinedFunctionsCode(jsfunction->unchecked_code());
      }
    }

    VisitJSFunctionFields(map,
                          reinterpret_cast<JSFunction*>(object),
                          flush_code_candidate);
  }


  static void VisitJSFunction(Map* map, HeapObject* object) {
    VisitJSFunctionFields(map,
                          reinterpret_cast<JSFunction*>(object),
                          false);
  }


#define SLOT_ADDR(obj, offset) \
  reinterpret_cast<Object**>((obj)->address() + offset)


  static inline void VisitJSFunctionFields(Map* map,
                                           JSFunction* object,
                                           bool flush_code_candidate) {
    Heap* heap = map->heap();
    MarkCompactCollector* collector = heap->mark_compact_collector();

    VisitPointers(heap,
                  SLOT_ADDR(object, JSFunction::kPropertiesOffset),
                  SLOT_ADDR(object, JSFunction::kCodeEntryOffset));

    if (!flush_code_candidate) {
      VisitCodeEntry(heap, object->address() + JSFunction::kCodeEntryOffset);
    } else {
      // Don't visit code object.

      // Visit shared function info to avoid double checking of it's
      // flushability.
      SharedFunctionInfo* shared_info = object->unchecked_shared();
      if (!shared_info->IsMarked()) {
        Map* shared_info_map = shared_info->map();
        collector->SetMark(shared_info);
        collector->MarkObject(shared_info_map);
        VisitSharedFunctionInfoAndFlushCodeGeneric(shared_info_map,
                                                   shared_info,
                                                   true);
      }
    }

    VisitPointers(heap,
                  SLOT_ADDR(object,
                            JSFunction::kCodeEntryOffset + kPointerSize),
                  SLOT_ADDR(object, JSFunction::kNonWeakFieldsEndOffset));

    // Don't visit the next function list field as it is a weak reference.
  }

  static inline void VisitJSRegExpFields(Map* map,
                                         HeapObject* object) {
    int last_property_offset =
        JSRegExp::kSize + kPointerSize * map->inobject_properties();
    VisitPointers(map->heap(),
                  SLOT_ADDR(object, JSRegExp::kPropertiesOffset),
                  SLOT_ADDR(object, last_property_offset));
  }


  static void VisitSharedFunctionInfoFields(Heap* heap,
                                            HeapObject* object,
                                            bool flush_code_candidate) {
    VisitPointer(heap, SLOT_ADDR(object, SharedFunctionInfo::kNameOffset));

    if (!flush_code_candidate) {
      VisitPointer(heap, SLOT_ADDR(object, SharedFunctionInfo::kCodeOffset));
    }

    VisitPointers(heap,
                  SLOT_ADDR(object, SharedFunctionInfo::kScopeInfoOffset),
                  SLOT_ADDR(object, SharedFunctionInfo::kSize));
  }

  #undef SLOT_ADDR

  typedef void (*Callback)(Map* map, HeapObject* object);

  static VisitorDispatchTable<Callback> table_;
};


VisitorDispatchTable<StaticMarkingVisitor::Callback>
  StaticMarkingVisitor::table_;


class MarkingVisitor : public ObjectVisitor {
 public:
  explicit MarkingVisitor(Heap* heap) : heap_(heap) { }

  void VisitPointer(Object** p) {
    StaticMarkingVisitor::VisitPointer(heap_, p);
  }

  void VisitPointers(Object** start, Object** end) {
    StaticMarkingVisitor::VisitPointers(heap_, start, end);
  }

 private:
  Heap* heap_;
};


class CodeMarkingVisitor : public ThreadVisitor {
 public:
  explicit CodeMarkingVisitor(MarkCompactCollector* collector)
      : collector_(collector) {}

  void VisitThread(Isolate* isolate, ThreadLocalTop* top) {
    collector_->PrepareThreadForCodeFlushing(isolate, top);
  }

 private:
  MarkCompactCollector* collector_;
};


class SharedFunctionInfoMarkingVisitor : public ObjectVisitor {
 public:
  explicit SharedFunctionInfoMarkingVisitor(MarkCompactCollector* collector)
      : collector_(collector) {}

  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) VisitPointer(p);
  }

  void VisitPointer(Object** slot) {
    Object* obj = *slot;
    if (obj->IsSharedFunctionInfo()) {
      SharedFunctionInfo* shared = reinterpret_cast<SharedFunctionInfo*>(obj);
      collector_->MarkObject(shared->unchecked_code());
      collector_->MarkObject(shared);
    }
  }

 private:
  MarkCompactCollector* collector_;
};


void MarkCompactCollector::MarkInlinedFunctionsCode(Code* code) {
  // For optimized functions we should retain both non-optimized version
  // of it's code and non-optimized version of all inlined functions.
  // This is required to support bailing out from inlined code.
  DeoptimizationInputData* data =
      reinterpret_cast<DeoptimizationInputData*>(
          code->unchecked_deoptimization_data());

  FixedArray* literals = data->UncheckedLiteralArray();

  for (int i = 0, count = data->InlinedFunctionCount()->value();
       i < count;
       i++) {
    JSFunction* inlined = reinterpret_cast<JSFunction*>(literals->get(i));
    MarkObject(inlined->unchecked_shared()->unchecked_code());
  }
}


void MarkCompactCollector::PrepareThreadForCodeFlushing(Isolate* isolate,
                                                        ThreadLocalTop* top) {
  for (StackFrameIterator it(isolate, top); !it.done(); it.Advance()) {
    // Note: for the frame that has a pending lazy deoptimization
    // StackFrame::unchecked_code will return a non-optimized code object for
    // the outermost function and StackFrame::LookupCode will return
    // actual optimized code object.
    StackFrame* frame = it.frame();
    Code* code = frame->unchecked_code();
    MarkObject(code);
    if (frame->is_optimized()) {
      MarkInlinedFunctionsCode(frame->LookupCode());
    }
  }
}


void MarkCompactCollector::PrepareForCodeFlushing() {
  ASSERT(heap() == Isolate::Current()->heap());

  if (!FLAG_flush_code) {
    EnableCodeFlushing(false);
    return;
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  if (heap()->isolate()->debug()->IsLoaded() ||
      heap()->isolate()->debug()->has_break_points()) {
    EnableCodeFlushing(false);
    return;
  }
#endif
  EnableCodeFlushing(true);

  // Ensure that empty descriptor array is marked. Method MarkDescriptorArray
  // relies on it being marked before any other descriptor array.
  MarkObject(heap()->raw_unchecked_empty_descriptor_array());

  // Make sure we are not referencing the code from the stack.
  ASSERT(this == heap()->mark_compact_collector());
  PrepareThreadForCodeFlushing(heap()->isolate(),
                               heap()->isolate()->thread_local_top());

  // Iterate the archived stacks in all threads to check if
  // the code is referenced.
  CodeMarkingVisitor code_marking_visitor(this);
  heap()->isolate()->thread_manager()->IterateArchivedThreads(
      &code_marking_visitor);

  SharedFunctionInfoMarkingVisitor visitor(this);
  heap()->isolate()->compilation_cache()->IterateFunctions(&visitor);
  heap()->isolate()->handle_scope_implementer()->Iterate(&visitor);

  ProcessMarkingStack();
}


// Visitor class for marking heap roots.
class RootMarkingVisitor : public ObjectVisitor {
 public:
  explicit RootMarkingVisitor(Heap* heap)
    : collector_(heap->mark_compact_collector()) { }

  void VisitPointer(Object** p) {
    MarkObjectByPointer(p);
  }

  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

 private:
  void MarkObjectByPointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;

    // Replace flat cons strings in place.
    HeapObject* object = ShortCircuitConsString(p);
    if (object->IsMarked()) return;

    Map* map = object->map();
    // Mark the object.
    collector_->SetMark(object);

    // Mark the map pointer and body, and push them on the marking stack.
    collector_->MarkObject(map);
    StaticMarkingVisitor::IterateBody(map, object);

    // Mark all the objects reachable from the map and body.  May leave
    // overflowed objects in the heap.
    collector_->EmptyMarkingStack();
  }

  MarkCompactCollector* collector_;
};


// Helper class for pruning the symbol table.
class SymbolTableCleaner : public ObjectVisitor {
 public:
  explicit SymbolTableCleaner(Heap* heap)
    : heap_(heap), pointers_removed_(0) { }

  virtual void VisitPointers(Object** start, Object** end) {
    // Visit all HeapObject pointers in [start, end).
    for (Object** p = start; p < end; p++) {
      if ((*p)->IsHeapObject() && !HeapObject::cast(*p)->IsMarked()) {
        // Check if the symbol being pruned is an external symbol. We need to
        // delete the associated external data as this symbol is going away.

        // Since no objects have yet been moved we can safely access the map of
        // the object.
        if ((*p)->IsExternalString()) {
          heap_->FinalizeExternalString(String::cast(*p));
        }
        // Set the entry to null_value (as deleted).
        *p = heap_->raw_unchecked_null_value();
        pointers_removed_++;
      }
    }
  }

  int PointersRemoved() {
    return pointers_removed_;
  }

 private:
  Heap* heap_;
  int pointers_removed_;
};


// Implementation of WeakObjectRetainer for mark compact GCs. All marked objects
// are retained.
class MarkCompactWeakObjectRetainer : public WeakObjectRetainer {
 public:
  virtual Object* RetainAs(Object* object) {
    MapWord first_word = HeapObject::cast(object)->map_word();
    if (first_word.IsMarked()) {
      return object;
    } else {
      return NULL;
    }
  }
};


void MarkCompactCollector::MarkUnmarkedObject(HeapObject* object) {
  ASSERT(!object->IsMarked());
  ASSERT(HEAP->Contains(object));
  if (object->IsMap()) {
    Map* map = Map::cast(object);
    if (FLAG_cleanup_code_caches_at_gc) {
      map->ClearCodeCache(heap());
    }
    SetMark(map);

    // When map collection is enabled we have to mark through map's transitions
    // in a special way to make transition links weak.
    // Only maps for subclasses of JSReceiver can have transitions.
    STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
    if (FLAG_collect_maps && map->instance_type() >= FIRST_JS_RECEIVER_TYPE) {
      MarkMapContents(map);
    } else {
      marking_stack_.Push(map);
    }
  } else {
    SetMark(object);
    marking_stack_.Push(object);
  }
}


void MarkCompactCollector::MarkMapContents(Map* map) {
  // Mark prototype transitions array but don't push it into marking stack.
  // This will make references from it weak. We will clean dead prototype
  // transitions in ClearNonLiveTransitions.
  FixedArray* prototype_transitions = map->unchecked_prototype_transitions();
  if (!prototype_transitions->IsMarked()) SetMark(prototype_transitions);

  Object* raw_descriptor_array =
      *HeapObject::RawField(map,
                            Map::kInstanceDescriptorsOrBitField3Offset);
  if (!raw_descriptor_array->IsSmi()) {
    MarkDescriptorArray(
        reinterpret_cast<DescriptorArray*>(raw_descriptor_array));
  }

  // Mark the Object* fields of the Map.
  // Since the descriptor array has been marked already, it is fine
  // that one of these fields contains a pointer to it.
  Object** start_slot = HeapObject::RawField(map,
                                             Map::kPointerFieldsBeginOffset);

  Object** end_slot = HeapObject::RawField(map, Map::kPointerFieldsEndOffset);

  StaticMarkingVisitor::VisitPointers(map->heap(), start_slot, end_slot);
}


void MarkCompactCollector::MarkDescriptorArray(
    DescriptorArray* descriptors) {
  if (descriptors->IsMarked()) return;
  // Empty descriptor array is marked as a root before any maps are marked.
  ASSERT(descriptors != HEAP->raw_unchecked_empty_descriptor_array());
  SetMark(descriptors);

  FixedArray* contents = reinterpret_cast<FixedArray*>(
      descriptors->get(DescriptorArray::kContentArrayIndex));
  ASSERT(contents->IsHeapObject());
  ASSERT(!contents->IsMarked());
  ASSERT(contents->IsFixedArray());
  ASSERT(contents->length() >= 2);
  SetMark(contents);
  // Contents contains (value, details) pairs.  If the details say that the type
  // of descriptor is MAP_TRANSITION, CONSTANT_TRANSITION,
  // EXTERNAL_ARRAY_TRANSITION or NULL_DESCRIPTOR, we don't mark the value as
  // live.  Only for MAP_TRANSITION, EXTERNAL_ARRAY_TRANSITION and
  // CONSTANT_TRANSITION is the value an Object* (a Map*).
  for (int i = 0; i < contents->length(); i += 2) {
    // If the pair (value, details) at index i, i+1 is not
    // a transition or null descriptor, mark the value.
    PropertyDetails details(Smi::cast(contents->get(i + 1)));
    if (details.type() < FIRST_PHANTOM_PROPERTY_TYPE) {
      HeapObject* object = reinterpret_cast<HeapObject*>(contents->get(i));
      if (object->IsHeapObject() && !object->IsMarked()) {
        SetMark(object);
        marking_stack_.Push(object);
      }
    }
  }
  // The DescriptorArray descriptors contains a pointer to its contents array,
  // but the contents array is already marked.
  marking_stack_.Push(descriptors);
}


void MarkCompactCollector::CreateBackPointers() {
  HeapObjectIterator iterator(heap()->map_space());
  for (HeapObject* next_object = iterator.next();
       next_object != NULL; next_object = iterator.next()) {
    if (next_object->IsMap()) {  // Could also be ByteArray on free list.
      Map* map = Map::cast(next_object);
      STATIC_ASSERT(LAST_TYPE == LAST_CALLABLE_SPEC_OBJECT_TYPE);
      if (map->instance_type() >= FIRST_JS_RECEIVER_TYPE) {
        map->CreateBackPointers();
      } else {
        ASSERT(map->instance_descriptors() == heap()->empty_descriptor_array());
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


class OverflowedObjectsScanner : public AllStatic {
 public:
  // Fill the marking stack with overflowed objects returned by the given
  // iterator.  Stop when the marking stack is filled or the end of the space
  // is reached, whichever comes first.
  template<class T>
  static inline void ScanOverflowedObjects(MarkCompactCollector* collector,
                                           T* it) {
    // The caller should ensure that the marking stack is initially not full,
    // so that we don't waste effort pointlessly scanning for objects.
    ASSERT(!collector->marking_stack_.is_full());

    for (HeapObject* object = it->next(); object != NULL; object = it->next()) {
      if (object->IsOverflowed()) {
        object->ClearOverflow();
        ASSERT(object->IsMarked());
        ASSERT(HEAP->Contains(object));
        collector->marking_stack_.Push(object);
        if (collector->marking_stack_.is_full()) return;
      }
    }
  }
};


bool MarkCompactCollector::IsUnmarkedHeapObject(Object** p) {
  return (*p)->IsHeapObject() && !HeapObject::cast(*p)->IsMarked();
}


void MarkCompactCollector::MarkSymbolTable() {
  SymbolTable* symbol_table = heap()->raw_unchecked_symbol_table();
  // Mark the symbol table itself.
  SetMark(symbol_table);
  // Explicitly mark the prefix.
  MarkingVisitor marker(heap());
  symbol_table->IteratePrefix(&marker);
  ProcessMarkingStack();
}


void MarkCompactCollector::MarkRoots(RootMarkingVisitor* visitor) {
  // Mark the heap roots including global variables, stack variables,
  // etc., and all objects reachable from them.
  heap()->IterateStrongRoots(visitor, VISIT_ONLY_STRONG);

  // Handle the symbol table specially.
  MarkSymbolTable();

  // There may be overflowed objects in the heap.  Visit them now.
  while (marking_stack_.overflowed()) {
    RefillMarkingStack();
    EmptyMarkingStack();
  }
}


void MarkCompactCollector::MarkObjectGroups() {
  List<ObjectGroup*>* object_groups =
      heap()->isolate()->global_handles()->object_groups();

  int last = 0;
  for (int i = 0; i < object_groups->length(); i++) {
    ObjectGroup* entry = object_groups->at(i);
    ASSERT(entry != NULL);

    Object*** objects = entry->objects_;
    bool group_marked = false;
    for (size_t j = 0; j < entry->length_; j++) {
      Object* object = *objects[j];
      if (object->IsHeapObject() && HeapObject::cast(object)->IsMarked()) {
        group_marked = true;
        break;
      }
    }

    if (!group_marked) {
      (*object_groups)[last++] = entry;
      continue;
    }

    // An object in the group is marked, so mark all heap objects in
    // the group.
    for (size_t j = 0; j < entry->length_; ++j) {
      if ((*objects[j])->IsHeapObject()) {
        MarkObject(HeapObject::cast(*objects[j]));
      }
    }

    // Once the entire group has been marked, dispose it because it's
    // not needed anymore.
    entry->Dispose();
  }
  object_groups->Rewind(last);
}


void MarkCompactCollector::MarkImplicitRefGroups() {
  List<ImplicitRefGroup*>* ref_groups =
      heap()->isolate()->global_handles()->implicit_ref_groups();

  int last = 0;
  for (int i = 0; i < ref_groups->length(); i++) {
    ImplicitRefGroup* entry = ref_groups->at(i);
    ASSERT(entry != NULL);

    if (!(*entry->parent_)->IsMarked()) {
      (*ref_groups)[last++] = entry;
      continue;
    }

    Object*** children = entry->children_;
    // A parent object is marked, so mark all child heap objects.
    for (size_t j = 0; j < entry->length_; ++j) {
      if ((*children[j])->IsHeapObject()) {
        MarkObject(HeapObject::cast(*children[j]));
      }
    }

    // Once the entire group has been marked, dispose it because it's
    // not needed anymore.
    entry->Dispose();
  }
  ref_groups->Rewind(last);
}


// Mark all objects reachable from the objects on the marking stack.
// Before: the marking stack contains zero or more heap object pointers.
// After: the marking stack is empty, and all objects reachable from the
// marking stack have been marked, or are overflowed in the heap.
void MarkCompactCollector::EmptyMarkingStack() {
  while (!marking_stack_.is_empty()) {
    while (!marking_stack_.is_empty()) {
      HeapObject* object = marking_stack_.Pop();
      ASSERT(object->IsHeapObject());
      ASSERT(heap()->Contains(object));
      ASSERT(object->IsMarked());
      ASSERT(!object->IsOverflowed());

      // Because the object is marked, we have to recover the original map
      // pointer and use it to mark the object's body.
      MapWord map_word = object->map_word();
      map_word.ClearMark();
      Map* map = map_word.ToMap();
      MarkObject(map);

      StaticMarkingVisitor::IterateBody(map, object);
    }

    // Process encountered weak maps, mark objects only reachable by those
    // weak maps and repeat until fix-point is reached.
    ProcessWeakMaps();
  }
}


// Sweep the heap for overflowed objects, clear their overflow bits, and
// push them on the marking stack.  Stop early if the marking stack fills
// before sweeping completes.  If sweeping completes, there are no remaining
// overflowed objects in the heap so the overflow flag on the markings stack
// is cleared.
void MarkCompactCollector::RefillMarkingStack() {
  ASSERT(marking_stack_.overflowed());

  SemiSpaceIterator new_it(heap()->new_space(), &OverflowObjectSize);
  OverflowedObjectsScanner::ScanOverflowedObjects(this, &new_it);
  if (marking_stack_.is_full()) return;

  HeapObjectIterator old_pointer_it(heap()->old_pointer_space(),
                                    &OverflowObjectSize);
  OverflowedObjectsScanner::ScanOverflowedObjects(this, &old_pointer_it);
  if (marking_stack_.is_full()) return;

  HeapObjectIterator old_data_it(heap()->old_data_space(), &OverflowObjectSize);
  OverflowedObjectsScanner::ScanOverflowedObjects(this, &old_data_it);
  if (marking_stack_.is_full()) return;

  HeapObjectIterator code_it(heap()->code_space(), &OverflowObjectSize);
  OverflowedObjectsScanner::ScanOverflowedObjects(this, &code_it);
  if (marking_stack_.is_full()) return;

  HeapObjectIterator map_it(heap()->map_space(), &OverflowObjectSize);
  OverflowedObjectsScanner::ScanOverflowedObjects(this, &map_it);
  if (marking_stack_.is_full()) return;

  HeapObjectIterator cell_it(heap()->cell_space(), &OverflowObjectSize);
  OverflowedObjectsScanner::ScanOverflowedObjects(this, &cell_it);
  if (marking_stack_.is_full()) return;

  LargeObjectIterator lo_it(heap()->lo_space(), &OverflowObjectSize);
  OverflowedObjectsScanner::ScanOverflowedObjects(this, &lo_it);
  if (marking_stack_.is_full()) return;

  marking_stack_.clear_overflowed();
}


// Mark all objects reachable (transitively) from objects on the marking
// stack.  Before: the marking stack contains zero or more heap object
// pointers.  After: the marking stack is empty and there are no overflowed
// objects in the heap.
void MarkCompactCollector::ProcessMarkingStack() {
  EmptyMarkingStack();
  while (marking_stack_.overflowed()) {
    RefillMarkingStack();
    EmptyMarkingStack();
  }
}


void MarkCompactCollector::ProcessExternalMarking() {
  bool work_to_do = true;
  ASSERT(marking_stack_.is_empty());
  while (work_to_do) {
    MarkObjectGroups();
    MarkImplicitRefGroups();
    work_to_do = !marking_stack_.is_empty();
    ProcessMarkingStack();
  }
}


void MarkCompactCollector::MarkLiveObjects() {
  GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_MARK);
  // The recursive GC marker detects when it is nearing stack overflow,
  // and switches to a different marking system.  JS interrupts interfere
  // with the C stack limit check.
  PostponeInterruptsScope postpone(heap()->isolate());

#ifdef DEBUG
  ASSERT(state_ == PREPARE_GC);
  state_ = MARK_LIVE_OBJECTS;
#endif
  // The to space contains live objects, the from space is used as a marking
  // stack.
  marking_stack_.Initialize(heap()->new_space()->FromSpaceLow(),
                            heap()->new_space()->FromSpaceHigh());

  ASSERT(!marking_stack_.overflowed());

  PrepareForCodeFlushing();

  RootMarkingVisitor root_visitor(heap());
  MarkRoots(&root_visitor);

  // The objects reachable from the roots are marked, yet unreachable
  // objects are unmarked.  Mark objects reachable due to host
  // application specific logic.
  ProcessExternalMarking();

  // The objects reachable from the roots or object groups are marked,
  // yet unreachable objects are unmarked.  Mark objects reachable
  // only from weak global handles.
  //
  // First we identify nonlive weak handles and mark them as pending
  // destruction.
  heap()->isolate()->global_handles()->IdentifyWeakHandles(
      &IsUnmarkedHeapObject);
  // Then we mark the objects and process the transitive closure.
  heap()->isolate()->global_handles()->IterateWeakRoots(&root_visitor);
  while (marking_stack_.overflowed()) {
    RefillMarkingStack();
    EmptyMarkingStack();
  }

  // Repeat host application specific marking to mark unmarked objects
  // reachable from the weak roots.
  ProcessExternalMarking();

  // Object literal map caches reference symbols (cache keys) and maps
  // (cache values). At this point still useful maps have already been
  // marked. Mark the keys for the alive values before we process the
  // symbol table.
  ProcessMapCaches();

  // Prune the symbol table removing all symbols only pointed to by the
  // symbol table.  Cannot use symbol_table() here because the symbol
  // table is marked.
  SymbolTable* symbol_table = heap()->raw_unchecked_symbol_table();
  SymbolTableCleaner v(heap());
  symbol_table->IterateElements(&v);
  symbol_table->ElementsRemoved(v.PointersRemoved());
  heap()->external_string_table_.Iterate(&v);
  heap()->external_string_table_.CleanUp();

  // Process the weak references.
  MarkCompactWeakObjectRetainer mark_compact_object_retainer;
  heap()->ProcessWeakReferences(&mark_compact_object_retainer);

  // Remove object groups after marking phase.
  heap()->isolate()->global_handles()->RemoveObjectGroups();
  heap()->isolate()->global_handles()->RemoveImplicitRefGroups();

  // Flush code from collected candidates.
  if (is_code_flushing_enabled()) {
    code_flusher_->ProcessCandidates();
  }

  // Clean up dead objects from the runtime profiler.
  heap()->isolate()->runtime_profiler()->RemoveDeadSamples();
}


void MarkCompactCollector::ProcessMapCaches() {
  Object* raw_context = heap()->global_contexts_list_;
  while (raw_context != heap()->undefined_value()) {
    Context* context = reinterpret_cast<Context*>(raw_context);
    if (context->IsMarked()) {
      HeapObject* raw_map_cache =
          HeapObject::cast(context->get(Context::MAP_CACHE_INDEX));
      // A map cache may be reachable from the stack. In this case
      // it's already transitively marked and it's too late to clean
      // up its parts.
      if (!raw_map_cache->IsMarked() &&
          raw_map_cache != heap()->undefined_value()) {
        MapCache* map_cache = reinterpret_cast<MapCache*>(raw_map_cache);
        int existing_elements = map_cache->NumberOfElements();
        int used_elements = 0;
        for (int i = MapCache::kElementsStartIndex;
             i < map_cache->length();
             i += MapCache::kEntrySize) {
          Object* raw_key = map_cache->get(i);
          if (raw_key == heap()->undefined_value() ||
              raw_key == heap()->null_value()) continue;
          STATIC_ASSERT(MapCache::kEntrySize == 2);
          Object* raw_map = map_cache->get(i + 1);
          if (raw_map->IsHeapObject() &&
              HeapObject::cast(raw_map)->IsMarked()) {
            ++used_elements;
          } else {
            // Delete useless entries with unmarked maps.
            ASSERT(raw_map->IsMap());
            map_cache->set_null_unchecked(heap(), i);
            map_cache->set_null_unchecked(heap(), i + 1);
          }
        }
        if (used_elements == 0) {
          context->set(Context::MAP_CACHE_INDEX, heap()->undefined_value());
        } else {
          // Note: we don't actually shrink the cache here to avoid
          // extra complexity during GC. We rely on subsequent cache
          // usages (EnsureCapacity) to do this.
          map_cache->ElementsRemoved(existing_elements - used_elements);
          MarkObject(map_cache);
        }
      }
    }
    // Move to next element in the list.
    raw_context = context->get(Context::NEXT_CONTEXT_LINK);
  }
  ProcessMarkingStack();
}


#ifdef DEBUG
void MarkCompactCollector::UpdateLiveObjectCount(HeapObject* obj) {
  live_bytes_ += obj->Size();
  if (heap()->new_space()->Contains(obj)) {
    live_young_objects_size_ += obj->Size();
  } else if (heap()->map_space()->Contains(obj)) {
    ASSERT(obj->IsMap());
    live_map_objects_size_ += obj->Size();
  } else if (heap()->cell_space()->Contains(obj)) {
    ASSERT(obj->IsJSGlobalPropertyCell());
    live_cell_objects_size_ += obj->Size();
  } else if (heap()->old_pointer_space()->Contains(obj)) {
    live_old_pointer_objects_size_ += obj->Size();
  } else if (heap()->old_data_space()->Contains(obj)) {
    live_old_data_objects_size_ += obj->Size();
  } else if (heap()->code_space()->Contains(obj)) {
    live_code_objects_size_ += obj->Size();
  } else if (heap()->lo_space()->Contains(obj)) {
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
  heap()->lo_space()->FreeUnmarkedObjects();
}


// Safe to use during marking phase only.
bool MarkCompactCollector::SafeIsMap(HeapObject* object) {
  MapWord metamap = object->map_word();
  metamap.ClearMark();
  return metamap.ToMap()->instance_type() == MAP_TYPE;
}


void MarkCompactCollector::ClearNonLiveTransitions() {
  HeapObjectIterator map_iterator(heap()->map_space(), &SizeOfMarkedObject);
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
    STATIC_ASSERT(LAST_TYPE == LAST_CALLABLE_SPEC_OBJECT_TYPE);
    if (map->instance_type() < FIRST_JS_RECEIVER_TYPE) continue;

    if (map->IsMarked() && map->attached_to_shared_function_info()) {
      // This map is used for inobject slack tracking and has been detached
      // from SharedFunctionInfo during the mark phase.
      // Since it survived the GC, reattach it now.
      map->unchecked_constructor()->unchecked_shared()->AttachInitialMap(map);
    }

    // Clear dead prototype transitions.
    int number_of_transitions = map->NumberOfProtoTransitions();
    if (number_of_transitions > 0) {
      FixedArray* prototype_transitions =
          map->unchecked_prototype_transitions();
      int new_number_of_transitions = 0;
      const int header = Map::kProtoTransitionHeaderSize;
      const int proto_offset =
          header + Map::kProtoTransitionPrototypeOffset;
      const int map_offset = header + Map::kProtoTransitionMapOffset;
      const int step = Map::kProtoTransitionElementsPerEntry;
      for (int i = 0; i < number_of_transitions; i++) {
        Object* prototype = prototype_transitions->get(proto_offset + i * step);
        Object* cached_map = prototype_transitions->get(map_offset + i * step);
        if (HeapObject::cast(prototype)->IsMarked() &&
            HeapObject::cast(cached_map)->IsMarked()) {
          if (new_number_of_transitions != i) {
            prototype_transitions->set_unchecked(
                heap_,
                proto_offset + new_number_of_transitions * step,
                prototype,
                UPDATE_WRITE_BARRIER);
            prototype_transitions->set_unchecked(
                heap_,
                map_offset + new_number_of_transitions * step,
                cached_map,
                SKIP_WRITE_BARRIER);
          }
          new_number_of_transitions++;
        }
      }

      // Fill slots that became free with undefined value.
      Object* undefined = heap()->raw_unchecked_undefined_value();
      for (int i = new_number_of_transitions * step;
           i < number_of_transitions * step;
           i++) {
        prototype_transitions->set_unchecked(heap_,
                                             header + i,
                                             undefined,
                                             SKIP_WRITE_BARRIER);
      }
      map->SetNumberOfProtoTransitions(new_number_of_transitions);
    }

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
        current->ClearNonLiveTransitions(heap(), real_prototype);
      }
      *HeapObject::RawField(current, Map::kPrototypeOffset) =
          real_prototype;
      current = reinterpret_cast<Map*>(next);
    }
  }
}


void MarkCompactCollector::ProcessWeakMaps() {
  Object* weak_map_obj = encountered_weak_maps();
  while (weak_map_obj != Smi::FromInt(0)) {
    ASSERT(HeapObject::cast(weak_map_obj)->IsMarked());
    JSWeakMap* weak_map = reinterpret_cast<JSWeakMap*>(weak_map_obj);
    ObjectHashTable* table = weak_map->unchecked_table();
    for (int i = 0; i < table->Capacity(); i++) {
      if (HeapObject::cast(table->KeyAt(i))->IsMarked()) {
        Object* value = table->get(table->EntryToValueIndex(i));
        StaticMarkingVisitor::MarkObjectByPointer(heap(), &value);
        table->set_unchecked(heap(),
                             table->EntryToValueIndex(i),
                             value,
                             UPDATE_WRITE_BARRIER);
      }
    }
    weak_map_obj = weak_map->next();
  }
}


void MarkCompactCollector::ClearWeakMaps() {
  Object* weak_map_obj = encountered_weak_maps();
  while (weak_map_obj != Smi::FromInt(0)) {
    ASSERT(HeapObject::cast(weak_map_obj)->IsMarked());
    JSWeakMap* weak_map = reinterpret_cast<JSWeakMap*>(weak_map_obj);
    ObjectHashTable* table = weak_map->unchecked_table();
    for (int i = 0; i < table->Capacity(); i++) {
      if (!HeapObject::cast(table->KeyAt(i))->IsMarked()) {
        table->RemoveEntry(i, heap());
      }
    }
    weak_map_obj = weak_map->next();
    weak_map->set_next(Smi::FromInt(0));
  }
  set_encountered_weak_maps(Smi::FromInt(0));
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


// Encode a free region, defined by the given start address and size, in the
// first word or two of the region.
void EncodeFreeRegion(Address free_start, int free_size) {
  ASSERT(free_size >= kIntSize);
  if (free_size == kIntSize) {
    Memory::uint32_at(free_start) = MarkCompactCollector::kSingleFreeEncoding;
  } else {
    ASSERT(free_size >= 2 * kIntSize);
    Memory::uint32_at(free_start) = MarkCompactCollector::kMultiFreeEncoding;
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
inline MaybeObject* MCAllocateFromNewSpace(Heap* heap,
                                           HeapObject* object,
                                           int object_size) {
  MaybeObject* forwarded;
  if (object_size > heap->MaxObjectSizeInPagedSpace()) {
    forwarded = Failure::Exception();
  } else {
    OldSpace* target_space = heap->TargetSpace(object);
    ASSERT(target_space == heap->old_pointer_space() ||
           target_space == heap->old_data_space());
    forwarded = target_space->MCAllocateRaw(object_size);
  }
  Object* result;
  if (!forwarded->ToObject(&result)) {
    result = heap->new_space()->MCAllocateRaw(object_size)->ToObjectUnchecked();
  }
  return result;
}


// Allocation functions for the paged spaces call the space's MCAllocateRaw.
MUST_USE_RESULT inline MaybeObject* MCAllocateFromOldPointerSpace(
    Heap *heap,
    HeapObject* ignore,
    int object_size) {
  return heap->old_pointer_space()->MCAllocateRaw(object_size);
}


MUST_USE_RESULT inline MaybeObject* MCAllocateFromOldDataSpace(
    Heap* heap,
    HeapObject* ignore,
    int object_size) {
  return heap->old_data_space()->MCAllocateRaw(object_size);
}


MUST_USE_RESULT inline MaybeObject* MCAllocateFromCodeSpace(
    Heap* heap,
    HeapObject* ignore,
    int object_size) {
  return heap->code_space()->MCAllocateRaw(object_size);
}


MUST_USE_RESULT inline MaybeObject* MCAllocateFromMapSpace(
    Heap* heap,
    HeapObject* ignore,
    int object_size) {
  return heap->map_space()->MCAllocateRaw(object_size);
}


MUST_USE_RESULT inline MaybeObject* MCAllocateFromCellSpace(
    Heap* heap, HeapObject* ignore, int object_size) {
  return heap->cell_space()->MCAllocateRaw(object_size);
}


// The forwarding address is encoded at the same offset as the current
// to-space object, but in from space.
inline void EncodeForwardingAddressInNewSpace(Heap* heap,
                                              HeapObject* old_object,
                                              int object_size,
                                              Object* new_object,
                                              int* ignored) {
  int offset =
      heap->new_space()->ToSpaceOffsetForAddress(old_object->address());
  Memory::Address_at(heap->new_space()->FromSpaceLow() + offset) =
      HeapObject::cast(new_object)->address();
}


// The forwarding address is encoded in the map pointer of the object as an
// offset (in terms of live bytes) from the address of the first live object
// in the page.
inline void EncodeForwardingAddressInPagedSpace(Heap* heap,
                                                HeapObject* old_object,
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
inline void IgnoreNonLiveObject(HeapObject* object, Isolate* isolate) {}


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
inline void EncodeForwardingAddressesInRange(MarkCompactCollector* collector,
                                             Address start,
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
      collector->tracer()->decrement_marked_count();
      object_size = object->Size();

      Object* forwarded =
          Alloc(collector->heap(), object, object_size)->ToObjectUnchecked();
      Encode(collector->heap(), object, object_size, forwarded, offset);

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
      ProcessNonLive(object, collector->heap()->isolate());
      if (is_prev_alive) {  // Transition from live to non-live.
        free_start = current;
        is_prev_alive = false;
      }
      LiveObjectList::ProcessNonLive(object);
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
      this,
      heap()->new_space()->bottom(),
      heap()->new_space()->top(),
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
        this,
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
static void MigrateObject(Heap* heap,
                          Address dst,
                          Address src,
                          int size,
                          bool to_old_space) {
  if (to_old_space) {
    heap->CopyBlockToOldSpaceAndUpdateRegionMarks(dst, src, size);
  } else {
    heap->CopyBlock(dst, src, size);
  }

  Memory::Address_at(src) = dst;
}


class StaticPointersToNewGenUpdatingVisitor : public
  StaticNewSpaceVisitor<StaticPointersToNewGenUpdatingVisitor> {
 public:
  static inline void VisitPointer(Heap* heap, Object** p) {
    if (!(*p)->IsHeapObject()) return;

    HeapObject* obj = HeapObject::cast(*p);
    Address old_addr = obj->address();

    if (heap->new_space()->Contains(obj)) {
      ASSERT(heap->InFromSpace(*p));
      *p = HeapObject::FromAddress(Memory::Address_at(old_addr));
    }
  }
};


// Visitor for updating pointers from live objects in old spaces to new space.
// It does not expect to encounter pointers to dead objects.
class PointersToNewGenUpdatingVisitor: public ObjectVisitor {
 public:
  explicit PointersToNewGenUpdatingVisitor(Heap* heap) : heap_(heap) { }

  void VisitPointer(Object** p) {
    StaticPointersToNewGenUpdatingVisitor::VisitPointer(heap_, p);
  }

  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) {
      StaticPointersToNewGenUpdatingVisitor::VisitPointer(heap_, p);
    }
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
  Heap* heap_;
};


// Visitor for updating pointers from live objects in old spaces to new space.
// It can encounter pointers to dead objects in new space when traversing map
// space (see comment for MigrateObject).
static void UpdatePointerToNewGen(HeapObject** p) {
  if (!(*p)->IsHeapObject()) return;

  Address old_addr = (*p)->address();
  ASSERT(HEAP->InFromSpace(*p));

  Address new_addr = Memory::Address_at(old_addr);

  if (new_addr == NULL) {
    // We encountered pointer to a dead object. Clear it so we will
    // not visit it again during next iteration of dirty regions.
    *p = NULL;
  } else {
    *p = HeapObject::FromAddress(new_addr);
  }
}


static String* UpdateNewSpaceReferenceInExternalStringTableEntry(Heap* heap,
                                                                 Object** p) {
  Address old_addr = HeapObject::cast(*p)->address();
  Address new_addr = Memory::Address_at(old_addr);
  return String::cast(HeapObject::FromAddress(new_addr));
}


static bool TryPromoteObject(Heap* heap, HeapObject* object, int object_size) {
  Object* result;

  if (object_size > heap->MaxObjectSizeInPagedSpace()) {
    MaybeObject* maybe_result =
        heap->lo_space()->AllocateRawFixedArray(object_size);
    if (maybe_result->ToObject(&result)) {
      HeapObject* target = HeapObject::cast(result);
      MigrateObject(heap, target->address(), object->address(), object_size,
                    true);
      heap->mark_compact_collector()->tracer()->
          increment_promoted_objects_size(object_size);
      return true;
    }
  } else {
    OldSpace* target_space = heap->TargetSpace(object);

    ASSERT(target_space == heap->old_pointer_space() ||
           target_space == heap->old_data_space());
    MaybeObject* maybe_result = target_space->AllocateRaw(object_size);
    if (maybe_result->ToObject(&result)) {
      HeapObject* target = HeapObject::cast(result);
      MigrateObject(heap,
                    target->address(),
                    object->address(),
                    object_size,
                    target_space == heap->old_pointer_space());
      heap->mark_compact_collector()->tracer()->
          increment_promoted_objects_size(object_size);
      return true;
    }
  }

  return false;
}


static void SweepNewSpace(Heap* heap, NewSpace* space) {
  heap->CheckNewSpaceExpansionCriteria();

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
      heap->mark_compact_collector()->tracer()->decrement_marked_count();

      size = object->Size();
      survivors_size += size;

      // Aggressively promote young survivors to the old space.
      if (TryPromoteObject(heap, object, size)) {
        continue;
      }

      // Promotion failed. Just migrate object to another semispace.
      // Allocation cannot fail at this point: semispaces are of equal size.
      Object* target = space->AllocateRaw(size)->ToObjectUnchecked();

      MigrateObject(heap,
                    HeapObject::cast(target)->address(),
                    current,
                    size,
                    false);
    } else {
      // Process the dead object before we write a NULL into its header.
      LiveObjectList::ProcessNonLive(object);

      size = object->Size();
      Memory::Address_at(current) = NULL;
    }
  }

  // Second pass: find pointers to new space and update them.
  PointersToNewGenUpdatingVisitor updating_visitor(heap);

  // Update pointers in to space.
  Address current = space->bottom();
  while (current < space->top()) {
    HeapObject* object = HeapObject::FromAddress(current);
    current +=
        StaticPointersToNewGenUpdatingVisitor::IterateBody(object->map(),
                                                           object);
  }

  // Update roots.
  heap->IterateRoots(&updating_visitor, VISIT_ALL_IN_SWEEP_NEWSPACE);
  LiveObjectList::IterateElements(&updating_visitor);

  // Update pointers in old spaces.
  heap->IterateDirtyRegions(heap->old_pointer_space(),
                            &Heap::IteratePointersInDirtyRegion,
                            &UpdatePointerToNewGen,
                            heap->WATERMARK_SHOULD_BE_VALID);

  heap->lo_space()->IterateDirtyRegions(&UpdatePointerToNewGen);

  // Update pointers from cells.
  HeapObjectIterator cell_iterator(heap->cell_space());
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

  // Update pointer from the global contexts list.
  updating_visitor.VisitPointer(heap->global_contexts_list_address());

  // Update pointers from external string table.
  heap->UpdateNewSpaceReferencesInExternalStringTable(
      &UpdateNewSpaceReferenceInExternalStringTableEntry);

  // All pointers were updated. Update auxiliary allocation info.
  heap->IncrementYoungSurvivorsCounter(survivors_size);
  space->set_age_mark(space->top());

  // Update JSFunction pointers from the runtime profiler.
  heap->isolate()->runtime_profiler()->UpdateSamplesAfterScavenge();
}


static void SweepSpace(Heap* heap, PagedSpace* space) {
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
        heap->mark_compact_collector()->tracer()->decrement_marked_count();

        if (!is_previous_alive) {  // Transition from free to live.
          space->DeallocateBlock(free_start,
                                 static_cast<int>(current - free_start),
                                 true);
          is_previous_alive = true;
        }
      } else {
        heap->mark_compact_collector()->ReportDeleteIfNeeded(
            object, heap->isolate());
        if (is_previous_alive) {  // Transition from live to free.
          free_start = current;
          is_previous_alive = false;
        }
        LiveObjectList::ProcessNonLive(object);
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
          space->DeallocateBlock(free_start, size_in_bytes, false);
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
        space->DeallocateBlock(last_free_start, last_free_size, true);
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
    space->DeallocateBlock(last_free_start, last_free_size, false);
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


void MarkCompactCollector::EncodeForwardingAddresses() {
  ASSERT(state_ == ENCODE_FORWARDING_ADDRESSES);
  // Objects in the active semispace of the young generation may be
  // relocated to the inactive semispace (if not promoted).  Set the
  // relocation info to the beginning of the inactive semispace.
  heap()->new_space()->MCResetRelocationInfo();

  // Compute the forwarding pointers in each space.
  EncodeForwardingAddressesInPagedSpace<MCAllocateFromOldPointerSpace,
                                        ReportDeleteIfNeeded>(
      heap()->old_pointer_space());

  EncodeForwardingAddressesInPagedSpace<MCAllocateFromOldDataSpace,
                                        IgnoreNonLiveObject>(
      heap()->old_data_space());

  EncodeForwardingAddressesInPagedSpace<MCAllocateFromCodeSpace,
                                        ReportDeleteIfNeeded>(
      heap()->code_space());

  EncodeForwardingAddressesInPagedSpace<MCAllocateFromCellSpace,
                                        IgnoreNonLiveObject>(
      heap()->cell_space());


  // Compute new space next to last after the old and code spaces have been
  // compacted.  Objects in new space can be promoted to old or code space.
  EncodeForwardingAddressesInNewSpace();

  // Compute map space last because computing forwarding addresses
  // overwrites non-live objects.  Objects in the other spaces rely on
  // non-live map pointers to get the sizes of non-live objects.
  EncodeForwardingAddressesInPagedSpace<MCAllocateFromMapSpace,
                                        IgnoreNonLiveObject>(
      heap()->map_space());

  // Write relocation info to the top page, so we can use it later.  This is
  // done after promoting objects from the new space so we get the correct
  // allocation top.
  heap()->old_pointer_space()->MCWriteRelocationInfoToPage();
  heap()->old_data_space()->MCWriteRelocationInfoToPage();
  heap()->code_space()->MCWriteRelocationInfoToPage();
  heap()->map_space()->MCWriteRelocationInfoToPage();
  heap()->cell_space()->MCWriteRelocationInfoToPage();
}


class MapIterator : public HeapObjectIterator {
 public:
  explicit MapIterator(Heap* heap)
    : HeapObjectIterator(heap->map_space(), &SizeCallback) { }

  MapIterator(Heap* heap, Address start)
      : HeapObjectIterator(heap->map_space(), start, &SizeCallback) { }

 private:
  static int SizeCallback(HeapObject* unused) {
    USE(unused);
    return Map::kSize;
  }
};


class MapCompact {
 public:
  explicit MapCompact(Heap* heap, int live_maps)
    : heap_(heap),
      live_maps_(live_maps),
      to_evacuate_start_(heap->map_space()->TopAfterCompaction(live_maps)),
      vacant_map_it_(heap),
      map_to_evacuate_it_(heap, to_evacuate_start_),
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
    MapUpdatingVisitor map_updating_visitor;
    heap()->IterateRoots(&map_updating_visitor, VISIT_ONLY_STRONG);
    heap()->isolate()->global_handles()->IterateWeakRoots(
        &map_updating_visitor);
    LiveObjectList::IterateElements(&map_updating_visitor);
  }

  void UpdateMapPointersInPagedSpace(PagedSpace* space) {
    ASSERT(space != heap()->map_space());

    PageIterator it(space, PageIterator::PAGES_IN_USE);
    while (it.has_next()) {
      Page* p = it.next();
      UpdateMapPointersInRange(heap(),
                               p->ObjectAreaStart(),
                               p->AllocationTop());
    }
  }

  void UpdateMapPointersInNewSpace() {
    NewSpace* space = heap()->new_space();
    UpdateMapPointersInRange(heap(), space->bottom(), space->top());
  }

  void UpdateMapPointersInLargeObjectSpace() {
    LargeObjectIterator it(heap()->lo_space());
    for (HeapObject* obj = it.next(); obj != NULL; obj = it.next())
      UpdateMapPointersInObject(heap(), obj);
  }

  void Finish() {
    heap()->map_space()->FinishCompaction(to_evacuate_start_, live_maps_);
  }

  inline Heap* heap() const { return heap_; }

 private:
  Heap* heap_;
  int live_maps_;
  Address to_evacuate_start_;
  MapIterator vacant_map_it_;
  MapIterator map_to_evacuate_it_;
  Map* first_map_to_evacuate_;

  // Helper class for updating map pointers in HeapObjects.
  class MapUpdatingVisitor: public ObjectVisitor {
  public:
    MapUpdatingVisitor() {}

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

    map_to_evacuate->heap()->CopyBlockToOldSpaceAndUpdateRegionMarks(
        vacant_map->address(), map_to_evacuate->address(), Map::kSize);

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

  static int UpdateMapPointersInObject(Heap* heap, HeapObject* obj) {
    ASSERT(!obj->IsMarked());
    Map* map = obj->map();
    ASSERT(heap->map_space()->Contains(map));
    MapWord map_word = map->map_word();
    ASSERT(!map_word.IsMarked());
    if (map_word.IsOverflowed()) {
      Map* new_map = GetForwardedMap(map_word);
      ASSERT(heap->map_space()->Contains(new_map));
      obj->set_map(new_map);

#ifdef DEBUG
      if (FLAG_gc_verbose) {
        PrintF("update %p : %p -> %p\n",
               obj->address(),
               reinterpret_cast<void*>(map),
               reinterpret_cast<void*>(new_map));
      }
#endif
    }

    int size = obj->SizeFromMap(map);
    MapUpdatingVisitor map_updating_visitor;
    obj->IterateBody(map->instance_type(), size, &map_updating_visitor);
    return size;
  }

  static void UpdateMapPointersInRange(Heap* heap, Address start, Address end) {
    HeapObject* object;
    int size;
    for (Address current = start; current < end; current += size) {
      object = HeapObject::FromAddress(current);
      size = UpdateMapPointersInObject(heap, object);
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


void MarkCompactCollector::SweepSpaces() {
  GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_SWEEP);

  ASSERT(state_ == SWEEP_SPACES);
  ASSERT(!IsCompacting());
  // Noncompacting collections simply sweep the spaces to clear the mark
  // bits and free the nonlive blocks (for old and map spaces).  We sweep
  // the map space last because freeing non-live maps overwrites them and
  // the other spaces rely on possibly non-live maps to get the sizes for
  // non-live objects.
  SweepSpace(heap(), heap()->old_pointer_space());
  SweepSpace(heap(), heap()->old_data_space());
  SweepSpace(heap(), heap()->code_space());
  SweepSpace(heap(), heap()->cell_space());
  { GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_SWEEP_NEWSPACE);
    SweepNewSpace(heap(), heap()->new_space());
  }
  SweepSpace(heap(), heap()->map_space());

  heap()->IterateDirtyRegions(heap()->map_space(),
                             &heap()->IteratePointersInDirtyMapsRegion,
                             &UpdatePointerToNewGen,
                             heap()->WATERMARK_SHOULD_BE_VALID);

  intptr_t live_maps_size = heap()->map_space()->Size();
  int live_maps = static_cast<int>(live_maps_size / Map::kSize);
  ASSERT(live_map_objects_size_ == live_maps_size);

  if (heap()->map_space()->NeedsCompaction(live_maps)) {
    MapCompact map_compact(heap(), live_maps);

    map_compact.CompactMaps();
    map_compact.UpdateMapPointersInRoots();

    PagedSpaces spaces;
    for (PagedSpace* space = spaces.next();
         space != NULL; space = spaces.next()) {
      if (space == heap()->map_space()) continue;
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
    LiveObjectCallback size_func) {
  int live_objects_size = 0;
  Address current = start;
  while (current < end) {
    uint32_t encoded_map = Memory::uint32_at(current);
    if (encoded_map == kSingleFreeEncoding) {
      current += kPointerSize;
    } else if (encoded_map == kMultiFreeEncoding) {
      current += Memory::int_at(current + kIntSize);
    } else {
      int size = (this->*size_func)(HeapObject::FromAddress(current));
      current += size;
      live_objects_size += size;
    }
  }
  return live_objects_size;
}


int MarkCompactCollector::IterateLiveObjects(
    NewSpace* space, LiveObjectCallback size_f) {
  ASSERT(MARK_LIVE_OBJECTS < state_ && state_ <= RELOCATE_OBJECTS);
  return IterateLiveObjectsInRange(space->bottom(), space->top(), size_f);
}


int MarkCompactCollector::IterateLiveObjects(
    PagedSpace* space, LiveObjectCallback size_f) {
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
  explicit UpdatingVisitor(Heap* heap) : heap_(heap) {}

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

  inline Heap* heap() const { return heap_; }

 private:
  void UpdatePointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;

    HeapObject* obj = HeapObject::cast(*p);
    Address old_addr = obj->address();
    Address new_addr;
    ASSERT(!heap()->InFromSpace(obj));

    if (heap()->new_space()->Contains(obj)) {
      Address forwarding_pointer_addr =
          heap()->new_space()->FromSpaceLow() +
          heap()->new_space()->ToSpaceOffsetForAddress(old_addr);
      new_addr = Memory::Address_at(forwarding_pointer_addr);

#ifdef DEBUG
      ASSERT(heap()->old_pointer_space()->Contains(new_addr) ||
             heap()->old_data_space()->Contains(new_addr) ||
             heap()->new_space()->FromSpaceContains(new_addr) ||
             heap()->lo_space()->Contains(HeapObject::FromAddress(new_addr)));

      if (heap()->new_space()->FromSpaceContains(new_addr)) {
        ASSERT(heap()->new_space()->FromSpaceOffsetForAddress(new_addr) <=
               heap()->new_space()->ToSpaceOffsetForAddress(old_addr));
      }
#endif

    } else if (heap()->lo_space()->Contains(obj)) {
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

  Heap* heap_;
};


void MarkCompactCollector::UpdatePointers() {
#ifdef DEBUG
  ASSERT(state_ == ENCODE_FORWARDING_ADDRESSES);
  state_ = UPDATE_POINTERS;
#endif
  UpdatingVisitor updating_visitor(heap());
  heap()->isolate()->runtime_profiler()->UpdateSamplesAfterCompact(
      &updating_visitor);
  heap()->IterateRoots(&updating_visitor, VISIT_ONLY_STRONG);
  heap()->isolate()->global_handles()->IterateWeakRoots(&updating_visitor);

  // Update the pointer to the head of the weak list of global contexts.
  updating_visitor.VisitPointer(&heap()->global_contexts_list_);

  LiveObjectList::IterateElements(&updating_visitor);

  int live_maps_size = IterateLiveObjects(
      heap()->map_space(), &MarkCompactCollector::UpdatePointersInOldObject);
  int live_pointer_olds_size = IterateLiveObjects(
      heap()->old_pointer_space(),
      &MarkCompactCollector::UpdatePointersInOldObject);
  int live_data_olds_size = IterateLiveObjects(
      heap()->old_data_space(),
      &MarkCompactCollector::UpdatePointersInOldObject);
  int live_codes_size = IterateLiveObjects(
      heap()->code_space(), &MarkCompactCollector::UpdatePointersInOldObject);
  int live_cells_size = IterateLiveObjects(
      heap()->cell_space(), &MarkCompactCollector::UpdatePointersInOldObject);
  int live_news_size = IterateLiveObjects(
      heap()->new_space(), &MarkCompactCollector::UpdatePointersInNewObject);

  // Large objects do not move, the map word can be updated directly.
  LargeObjectIterator it(heap()->lo_space());
  for (HeapObject* obj = it.next(); obj != NULL; obj = it.next()) {
    UpdatePointersInNewObject(obj);
  }

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

  ASSERT(heap()->map_space()->Contains(old_map));
  ASSERT(heap()->map_space()->Contains(forwarded));
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
  UpdatingVisitor updating_visitor(heap());
  obj->IterateBody(old_map->instance_type(), obj_size, &updating_visitor);
  return obj_size;
}


int MarkCompactCollector::UpdatePointersInOldObject(HeapObject* obj) {
  // Decode the map pointer.
  MapWord encoding = obj->map_word();
  Address map_addr = encoding.DecodeMapAddress(heap()->map_space());
  ASSERT(heap()->map_space()->Contains(HeapObject::FromAddress(map_addr)));

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
  UpdatingVisitor updating_visitor(heap());
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
  int live_maps_size = IterateLiveObjects(
      heap()->map_space(), &MarkCompactCollector::RelocateMapObject);
  int live_pointer_olds_size = IterateLiveObjects(
      heap()->old_pointer_space(),
      &MarkCompactCollector::RelocateOldPointerObject);
  int live_data_olds_size = IterateLiveObjects(
      heap()->old_data_space(), &MarkCompactCollector::RelocateOldDataObject);
  int live_codes_size = IterateLiveObjects(
      heap()->code_space(), &MarkCompactCollector::RelocateCodeObject);
  int live_cells_size = IterateLiveObjects(
      heap()->cell_space(), &MarkCompactCollector::RelocateCellObject);
  int live_news_size = IterateLiveObjects(
      heap()->new_space(), &MarkCompactCollector::RelocateNewObject);

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
  heap()->new_space()->Flip();

  heap()->new_space()->MCCommitRelocationInfo();

  // Set age_mark to bottom in to space
  Address mark = heap()->new_space()->bottom();
  heap()->new_space()->set_age_mark(mark);

  PagedSpaces spaces;
  for (PagedSpace* space = spaces.next(); space != NULL; space = spaces.next())
    space->MCCommitRelocationInfo();

  heap()->CheckNewSpaceExpansionCriteria();
  heap()->IncrementYoungSurvivorsCounter(live_news_size);
}


int MarkCompactCollector::RelocateMapObject(HeapObject* obj) {
  // Recover map pointer.
  MapWord encoding = obj->map_word();
  Address map_addr = encoding.DecodeMapAddress(heap()->map_space());
  ASSERT(heap()->map_space()->Contains(HeapObject::FromAddress(map_addr)));

  // Get forwarding address before resetting map pointer
  Address new_addr = GetForwardingAddressInOldSpace(obj);

  // Reset map pointer.  The meta map object may not be copied yet so
  // Map::cast does not yet work.
  obj->set_map(reinterpret_cast<Map*>(HeapObject::FromAddress(map_addr)));

  Address old_addr = obj->address();

  if (new_addr != old_addr) {
    // Move contents.
    heap()->MoveBlockToOldSpaceAndUpdateRegionMarks(new_addr,
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
  Address map_addr = encoding.DecodeMapAddress(heap()->map_space());
  ASSERT(heap()->map_space()->Contains(map_addr));

  // Get forwarding address before resetting map pointer.
  Address new_addr = GetForwardingAddressInOldSpace(obj);

  // Reset the map pointer.
  int obj_size = RestoreMap(obj, space, new_addr, map_addr);

  Address old_addr = obj->address();

  if (new_addr != old_addr) {
    // Move contents.
    if (space == heap()->old_data_space()) {
      heap()->MoveBlock(new_addr, old_addr, obj_size);
    } else {
      heap()->MoveBlockToOldSpaceAndUpdateRegionMarks(new_addr,
                                                     old_addr,
                                                     obj_size);
    }
  }

  ASSERT(!HeapObject::FromAddress(new_addr)->IsCode());

  HeapObject* copied_to = HeapObject::FromAddress(new_addr);
  if (copied_to->IsSharedFunctionInfo()) {
    PROFILE(heap()->isolate(),
            SharedFunctionInfoMoveEvent(old_addr, new_addr));
  }
  HEAP_PROFILE(heap(), ObjectMoveEvent(old_addr, new_addr));

  return obj_size;
}


int MarkCompactCollector::RelocateOldPointerObject(HeapObject* obj) {
  return RelocateOldNonCodeObject(obj, heap()->old_pointer_space());
}


int MarkCompactCollector::RelocateOldDataObject(HeapObject* obj) {
  return RelocateOldNonCodeObject(obj, heap()->old_data_space());
}


int MarkCompactCollector::RelocateCellObject(HeapObject* obj) {
  return RelocateOldNonCodeObject(obj, heap()->cell_space());
}


int MarkCompactCollector::RelocateCodeObject(HeapObject* obj) {
  // Recover map pointer.
  MapWord encoding = obj->map_word();
  Address map_addr = encoding.DecodeMapAddress(heap()->map_space());
  ASSERT(heap()->map_space()->Contains(HeapObject::FromAddress(map_addr)));

  // Get forwarding address before resetting map pointer
  Address new_addr = GetForwardingAddressInOldSpace(obj);

  // Reset the map pointer.
  int obj_size = RestoreMap(obj, heap()->code_space(), new_addr, map_addr);

  Address old_addr = obj->address();

  if (new_addr != old_addr) {
    // Move contents.
    heap()->MoveBlock(new_addr, old_addr, obj_size);
  }

  HeapObject* copied_to = HeapObject::FromAddress(new_addr);
  if (copied_to->IsCode()) {
    // May also update inline cache target.
    Code::cast(copied_to)->Relocate(new_addr - old_addr);
    // Notify the logger that compiled code has moved.
    PROFILE(heap()->isolate(), CodeMoveEvent(old_addr, new_addr));
  }
  HEAP_PROFILE(heap(), ObjectMoveEvent(old_addr, new_addr));

  return obj_size;
}


int MarkCompactCollector::RelocateNewObject(HeapObject* obj) {
  int obj_size = obj->Size();

  // Get forwarding address
  Address old_addr = obj->address();
  int offset = heap()->new_space()->ToSpaceOffsetForAddress(old_addr);

  Address new_addr =
    Memory::Address_at(heap()->new_space()->FromSpaceLow() + offset);

#ifdef DEBUG
  if (heap()->new_space()->FromSpaceContains(new_addr)) {
    ASSERT(heap()->new_space()->FromSpaceOffsetForAddress(new_addr) <=
           heap()->new_space()->ToSpaceOffsetForAddress(old_addr));
  } else {
    ASSERT(heap()->TargetSpace(obj) == heap()->old_pointer_space() ||
           heap()->TargetSpace(obj) == heap()->old_data_space());
  }
#endif

  // New and old addresses cannot overlap.
  if (heap()->InNewSpace(HeapObject::FromAddress(new_addr))) {
    heap()->CopyBlock(new_addr, old_addr, obj_size);
  } else {
    heap()->CopyBlockToOldSpaceAndUpdateRegionMarks(new_addr,
                                                   old_addr,
                                                   obj_size);
  }

#ifdef DEBUG
  if (FLAG_gc_verbose) {
    PrintF("relocate %p -> %p\n", old_addr, new_addr);
  }
#endif

  HeapObject* copied_to = HeapObject::FromAddress(new_addr);
  if (copied_to->IsSharedFunctionInfo()) {
    PROFILE(heap()->isolate(),
            SharedFunctionInfoMoveEvent(old_addr, new_addr));
  }
  HEAP_PROFILE(heap(), ObjectMoveEvent(old_addr, new_addr));

  return obj_size;
}


void MarkCompactCollector::EnableCodeFlushing(bool enable) {
  if (enable) {
    if (code_flusher_ != NULL) return;
    code_flusher_ = new CodeFlusher(heap()->isolate());
  } else {
    if (code_flusher_ == NULL) return;
    delete code_flusher_;
    code_flusher_ = NULL;
  }
}


void MarkCompactCollector::ReportDeleteIfNeeded(HeapObject* obj,
                                                Isolate* isolate) {
#ifdef ENABLE_GDB_JIT_INTERFACE
  if (obj->IsCode()) {
    GDBJITInterface::RemoveCode(reinterpret_cast<Code*>(obj));
  }
#endif
  if (obj->IsCode()) {
    PROFILE(isolate, CodeDeleteEvent(obj->address()));
  }
}


int MarkCompactCollector::SizeOfMarkedObject(HeapObject* obj) {
  MapWord map_word = obj->map_word();
  map_word.ClearMark();
  return obj->SizeFromMap(map_word.ToMap());
}


void MarkCompactCollector::Initialize() {
  StaticPointersToNewGenUpdatingVisitor::Initialize();
  StaticMarkingVisitor::Initialize();
}


} }  // namespace v8::internal
