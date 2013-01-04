// Copyright 2012 the V8 project authors. All rights reserved.
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

#ifndef V8_OBJECTS_VISITING_INL_H_
#define V8_OBJECTS_VISITING_INL_H_


namespace v8 {
namespace internal {

template<typename StaticVisitor>
void StaticNewSpaceVisitor<StaticVisitor>::Initialize() {
  table_.Register(kVisitShortcutCandidate,
                  &FixedBodyVisitor<StaticVisitor,
                  ConsString::BodyDescriptor,
                  int>::Visit);

  table_.Register(kVisitConsString,
                  &FixedBodyVisitor<StaticVisitor,
                  ConsString::BodyDescriptor,
                  int>::Visit);

  table_.Register(kVisitSlicedString,
                  &FixedBodyVisitor<StaticVisitor,
                  SlicedString::BodyDescriptor,
                  int>::Visit);

  table_.Register(kVisitFixedArray,
                  &FlexibleBodyVisitor<StaticVisitor,
                  FixedArray::BodyDescriptor,
                  int>::Visit);

  table_.Register(kVisitFixedDoubleArray, &VisitFixedDoubleArray);

  table_.Register(kVisitNativeContext,
                  &FixedBodyVisitor<StaticVisitor,
                  Context::ScavengeBodyDescriptor,
                  int>::Visit);

  table_.Register(kVisitByteArray, &VisitByteArray);

  table_.Register(kVisitSharedFunctionInfo,
                  &FixedBodyVisitor<StaticVisitor,
                  SharedFunctionInfo::BodyDescriptor,
                  int>::Visit);

  table_.Register(kVisitSeqOneByteString, &VisitSeqOneByteString);

  table_.Register(kVisitSeqTwoByteString, &VisitSeqTwoByteString);

  table_.Register(kVisitJSFunction, &VisitJSFunction);

  table_.Register(kVisitFreeSpace, &VisitFreeSpace);

  table_.Register(kVisitJSWeakMap, &JSObjectVisitor::Visit);

  table_.Register(kVisitJSRegExp, &JSObjectVisitor::Visit);

  table_.template RegisterSpecializations<DataObjectVisitor,
                                          kVisitDataObject,
                                          kVisitDataObjectGeneric>();

  table_.template RegisterSpecializations<JSObjectVisitor,
                                          kVisitJSObject,
                                          kVisitJSObjectGeneric>();
  table_.template RegisterSpecializations<StructVisitor,
                                          kVisitStruct,
                                          kVisitStructGeneric>();
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::Initialize() {
  table_.Register(kVisitShortcutCandidate,
                  &FixedBodyVisitor<StaticVisitor,
                  ConsString::BodyDescriptor,
                  void>::Visit);

  table_.Register(kVisitConsString,
                  &FixedBodyVisitor<StaticVisitor,
                  ConsString::BodyDescriptor,
                  void>::Visit);

  table_.Register(kVisitSlicedString,
                  &FixedBodyVisitor<StaticVisitor,
                  SlicedString::BodyDescriptor,
                  void>::Visit);

  table_.Register(kVisitFixedArray, &FixedArrayVisitor::Visit);

  table_.Register(kVisitFixedDoubleArray, &DataObjectVisitor::Visit);

  table_.Register(kVisitNativeContext, &VisitNativeContext);

  table_.Register(kVisitByteArray, &DataObjectVisitor::Visit);

  table_.Register(kVisitFreeSpace, &DataObjectVisitor::Visit);

  table_.Register(kVisitSeqOneByteString, &DataObjectVisitor::Visit);

  table_.Register(kVisitSeqTwoByteString, &DataObjectVisitor::Visit);

  table_.Register(kVisitJSWeakMap, &StaticVisitor::VisitJSWeakMap);

  table_.Register(kVisitOddball,
                  &FixedBodyVisitor<StaticVisitor,
                  Oddball::BodyDescriptor,
                  void>::Visit);

  table_.Register(kVisitMap, &VisitMap);

  table_.Register(kVisitCode, &VisitCode);

  table_.Register(kVisitSharedFunctionInfo, &VisitSharedFunctionInfo);

  table_.Register(kVisitJSFunction, &VisitJSFunction);

  // Registration for kVisitJSRegExp is done by StaticVisitor.

  table_.Register(kVisitPropertyCell,
                  &FixedBodyVisitor<StaticVisitor,
                  JSGlobalPropertyCell::BodyDescriptor,
                  void>::Visit);

  table_.template RegisterSpecializations<DataObjectVisitor,
                                          kVisitDataObject,
                                          kVisitDataObjectGeneric>();

  table_.template RegisterSpecializations<JSObjectVisitor,
                                          kVisitJSObject,
                                          kVisitJSObjectGeneric>();

  table_.template RegisterSpecializations<StructObjectVisitor,
                                          kVisitStruct,
                                          kVisitStructGeneric>();
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitCodeEntry(
    Heap* heap, Address entry_address) {
  Code* code = Code::cast(Code::GetObjectFromEntryAddress(entry_address));
  heap->mark_compact_collector()->RecordCodeEntrySlot(entry_address, code);
  StaticVisitor::MarkObject(heap, code);
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitEmbeddedPointer(
    Heap* heap, RelocInfo* rinfo) {
  ASSERT(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
  ASSERT(!rinfo->target_object()->IsConsString());
  HeapObject* object = HeapObject::cast(rinfo->target_object());
  heap->mark_compact_collector()->RecordRelocSlot(rinfo, object);
  StaticVisitor::MarkObject(heap, object);
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitGlobalPropertyCell(
    Heap* heap, RelocInfo* rinfo) {
  ASSERT(rinfo->rmode() == RelocInfo::GLOBAL_PROPERTY_CELL);
  JSGlobalPropertyCell* cell = rinfo->target_cell();
  StaticVisitor::MarkObject(heap, cell);
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitDebugTarget(
    Heap* heap, RelocInfo* rinfo) {
  ASSERT((RelocInfo::IsJSReturn(rinfo->rmode()) &&
          rinfo->IsPatchedReturnSequence()) ||
         (RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
          rinfo->IsPatchedDebugBreakSlotSequence()));
  Code* target = Code::GetCodeFromTargetAddress(rinfo->call_address());
  heap->mark_compact_collector()->RecordRelocSlot(rinfo, target);
  StaticVisitor::MarkObject(heap, target);
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitCodeTarget(
    Heap* heap, RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
  Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  // Monomorphic ICs are preserved when possible, but need to be flushed
  // when they might be keeping a Context alive, or when the heap is about
  // to be serialized.
  if (FLAG_cleanup_code_caches_at_gc && target->is_inline_cache_stub()
      && (target->ic_state() == MEGAMORPHIC || heap->flush_monomorphic_ics() ||
          Serializer::enabled() || target->ic_age() != heap->global_ic_age())) {
    IC::Clear(rinfo->pc());
    target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  }
  heap->mark_compact_collector()->RecordRelocSlot(rinfo, target);
  StaticVisitor::MarkObject(heap, target);
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitCodeAgeSequence(
    Heap* heap, RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsCodeAgeSequence(rinfo->rmode()));
  Code* target = rinfo->code_age_stub();
  ASSERT(target != NULL);
  heap->mark_compact_collector()->RecordRelocSlot(rinfo, target);
  StaticVisitor::MarkObject(heap, target);
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitNativeContext(
    Map* map, HeapObject* object) {
  FixedBodyVisitor<StaticVisitor,
                   Context::MarkCompactBodyDescriptor,
                   void>::Visit(map, object);

  MarkCompactCollector* collector = map->GetHeap()->mark_compact_collector();
  for (int idx = Context::FIRST_WEAK_SLOT;
       idx < Context::NATIVE_CONTEXT_SLOTS;
       ++idx) {
    Object** slot =
        HeapObject::RawField(object, FixedArray::OffsetOfElementAt(idx));
    collector->RecordSlot(slot, slot, *slot);
  }
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitMap(
    Map* map, HeapObject* object) {
  Heap* heap = map->GetHeap();
  Map* map_object = Map::cast(object);

  // Clears the cache of ICs related to this map.
  if (FLAG_cleanup_code_caches_at_gc) {
    map_object->ClearCodeCache(heap);
  }

  // When map collection is enabled we have to mark through map's
  // transitions and back pointers in a special way to make these links
  // weak.  Only maps for subclasses of JSReceiver can have transitions.
  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  if (FLAG_collect_maps &&
      map_object->instance_type() >= FIRST_JS_RECEIVER_TYPE) {
    MarkMapContents(heap, map_object);
  } else {
    StaticVisitor::VisitPointers(heap,
        HeapObject::RawField(object, Map::kPointerFieldsBeginOffset),
        HeapObject::RawField(object, Map::kPointerFieldsEndOffset));
  }
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitCode(
    Map* map, HeapObject* object) {
  Heap* heap = map->GetHeap();
  Code* code = Code::cast(object);
  if (FLAG_cleanup_code_caches_at_gc) {
    code->ClearTypeFeedbackCells(heap);
  }
  if (FLAG_age_code && !Serializer::enabled()) {
    code->MakeOlder(heap->mark_compact_collector()->marking_parity());
  }
  code->CodeIterateBody<StaticVisitor>(heap);
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitSharedFunctionInfo(
    Map* map, HeapObject* object) {
  Heap* heap = map->GetHeap();
  SharedFunctionInfo* shared = SharedFunctionInfo::cast(object);
  if (shared->ic_age() != heap->global_ic_age()) {
    shared->ResetForNewContext(heap->global_ic_age());
  }
  if (FLAG_cache_optimized_code) {
    // Flush optimized code map on major GC.
    // TODO(mstarzinger): We may experiment with rebuilding it or with
    // retaining entries which should survive as we iterate through
    // optimized functions anyway.
    shared->ClearOptimizedCodeMap();
  }
  MarkCompactCollector* collector = heap->mark_compact_collector();
  if (collector->is_code_flushing_enabled()) {
    if (IsFlushable(heap, shared)) {
      // This function's code looks flushable. But we have to postpone
      // the decision until we see all functions that point to the same
      // SharedFunctionInfo because some of them might be optimized.
      // That would also make the non-optimized version of the code
      // non-flushable, because it is required for bailing out from
      // optimized code.
      collector->code_flusher()->AddCandidate(shared);
      // Treat the reference to the code object weakly.
      VisitSharedFunctionInfoWeakCode(heap, object);
      return;
    }
  }
  VisitSharedFunctionInfoStrongCode(heap, object);
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitJSFunction(
    Map* map, HeapObject* object) {
  Heap* heap = map->GetHeap();
  JSFunction* function = JSFunction::cast(object);
  MarkCompactCollector* collector = heap->mark_compact_collector();
  if (collector->is_code_flushing_enabled()) {
    if (IsFlushable(heap, function)) {
      // This function's code looks flushable. But we have to postpone
      // the decision until we see all functions that point to the same
      // SharedFunctionInfo because some of them might be optimized.
      // That would also make the non-optimized version of the code
      // non-flushable, because it is required for bailing out from
      // optimized code.
      collector->code_flusher()->AddCandidate(function);
      // Visit shared function info immediately to avoid double checking
      // of its flushability later. This is just an optimization because
      // the shared function info would eventually be visited.
      SharedFunctionInfo* shared = function->unchecked_shared();
      if (StaticVisitor::MarkObjectWithoutPush(heap, shared)) {
        StaticVisitor::MarkObject(heap, shared->map());
        VisitSharedFunctionInfoWeakCode(heap, shared);
      }
      // Treat the reference to the code object weakly.
      VisitJSFunctionWeakCode(heap, object);
      return;
    } else {
      // Visit all unoptimized code objects to prevent flushing them.
      StaticVisitor::MarkObject(heap, function->shared()->code());
      if (function->code()->kind() == Code::OPTIMIZED_FUNCTION) {
        MarkInlinedFunctionsCode(heap, function->code());
      }
    }
  }
  VisitJSFunctionStrongCode(heap, object);
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitJSRegExp(
    Map* map, HeapObject* object) {
  int last_property_offset =
      JSRegExp::kSize + kPointerSize * map->inobject_properties();
  StaticVisitor::VisitPointers(map->GetHeap(),
      HeapObject::RawField(object, JSRegExp::kPropertiesOffset),
      HeapObject::RawField(object, last_property_offset));
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::MarkMapContents(
    Heap* heap, Map* map) {
  // Make sure that the back pointer stored either in the map itself or
  // inside its transitions array is marked. Skip recording the back
  // pointer slot since map space is not compacted.
  StaticVisitor::MarkObject(heap, HeapObject::cast(map->GetBackPointer()));

  // Treat pointers in the transitions array as weak and also mark that
  // array to prevent visiting it later. Skip recording the transition
  // array slot, since it will be implicitly recorded when the pointer
  // fields of this map are visited.
  TransitionArray* transitions = map->unchecked_transition_array();
  if (transitions->IsTransitionArray()) {
    MarkTransitionArray(heap, transitions);
  } else {
    // Already marked by marking map->GetBackPointer() above.
    ASSERT(transitions->IsMap() || transitions->IsUndefined());
  }

  // Mark the pointer fields of the Map. Since the transitions array has
  // been marked already, it is fine that one of these fields contains a
  // pointer to it.
  StaticVisitor::VisitPointers(heap,
      HeapObject::RawField(map, Map::kPointerFieldsBeginOffset),
      HeapObject::RawField(map, Map::kPointerFieldsEndOffset));
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::MarkTransitionArray(
    Heap* heap, TransitionArray* transitions) {
  if (!StaticVisitor::MarkObjectWithoutPush(heap, transitions)) return;

  // Simple transitions do not have keys nor prototype transitions.
  if (transitions->IsSimpleTransition()) return;

  if (transitions->HasPrototypeTransitions()) {
    // Mark prototype transitions array but do not push it onto marking
    // stack, this will make references from it weak. We will clean dead
    // prototype transitions in ClearNonLiveTransitions.
    Object** slot = transitions->GetPrototypeTransitionsSlot();
    HeapObject* obj = HeapObject::cast(*slot);
    heap->mark_compact_collector()->RecordSlot(slot, slot, obj);
    StaticVisitor::MarkObjectWithoutPush(heap, obj);
  }

  for (int i = 0; i < transitions->number_of_transitions(); ++i) {
    StaticVisitor::VisitPointer(heap, transitions->GetKeySlot(i));
  }
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::MarkInlinedFunctionsCode(
    Heap* heap, Code* code) {
  // For optimized functions we should retain both non-optimized version
  // of its code and non-optimized version of all inlined functions.
  // This is required to support bailing out from inlined code.
  DeoptimizationInputData* data =
      DeoptimizationInputData::cast(code->deoptimization_data());
  FixedArray* literals = data->LiteralArray();
  for (int i = 0, count = data->InlinedFunctionCount()->value();
       i < count;
       i++) {
    JSFunction* inlined = JSFunction::cast(literals->get(i));
    StaticVisitor::MarkObject(heap, inlined->shared()->code());
  }
}


inline static bool IsValidNonBuiltinContext(Object* context) {
  return context->IsContext() &&
      !Context::cast(context)->global_object()->IsJSBuiltinsObject();
}


inline static bool HasSourceCode(Heap* heap, SharedFunctionInfo* info) {
  Object* undefined = heap->undefined_value();
  return (info->script() != undefined) &&
      (reinterpret_cast<Script*>(info->script())->source() != undefined);
}


template<typename StaticVisitor>
bool StaticMarkingVisitor<StaticVisitor>::IsFlushable(
    Heap* heap, JSFunction* function) {
  SharedFunctionInfo* shared_info = function->unchecked_shared();

  // Code is either on stack, in compilation cache or referenced
  // by optimized version of function.
  MarkBit code_mark = Marking::MarkBitFrom(function->code());
  if (code_mark.Get()) {
    if (!FLAG_age_code) {
      if (!Marking::MarkBitFrom(shared_info).Get()) {
        shared_info->set_code_age(0);
      }
    }
    return false;
  }

  // The function must have a valid context and not be a builtin.
  if (!IsValidNonBuiltinContext(function->unchecked_context())) {
    return false;
  }

  // We do not (yet) flush code for optimized functions.
  if (function->code() != shared_info->code()) {
    return false;
  }

  // Check age of optimized code.
  if (FLAG_age_code && !function->code()->IsOld()) {
    return false;
  }

  return IsFlushable(heap, shared_info);
}


template<typename StaticVisitor>
bool StaticMarkingVisitor<StaticVisitor>::IsFlushable(
    Heap* heap, SharedFunctionInfo* shared_info) {
  // Code is either on stack, in compilation cache or referenced
  // by optimized version of function.
  MarkBit code_mark = Marking::MarkBitFrom(shared_info->code());
  if (code_mark.Get()) {
    return false;
  }

  // The function must be compiled and have the source code available,
  // to be able to recompile it in case we need the function again.
  if (!(shared_info->is_compiled() && HasSourceCode(heap, shared_info))) {
    return false;
  }

  // We never flush code for API functions.
  Object* function_data = shared_info->function_data();
  if (function_data->IsFunctionTemplateInfo()) {
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

  if (FLAG_age_code) {
    return shared_info->code()->IsOld();
  } else {
    // How many collections newly compiled code object will survive before being
    // flushed.
    static const int kCodeAgeThreshold = 5;

    // Age this shared function info.
    if (shared_info->code_age() < kCodeAgeThreshold) {
      shared_info->set_code_age(shared_info->code_age() + 1);
      return false;
    }
    return true;
  }
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitSharedFunctionInfoStrongCode(
    Heap* heap, HeapObject* object) {
  StaticVisitor::BeforeVisitingSharedFunctionInfo(object);
  Object** start_slot =
      HeapObject::RawField(object,
                           SharedFunctionInfo::BodyDescriptor::kStartOffset);
  Object** end_slot =
      HeapObject::RawField(object,
                           SharedFunctionInfo::BodyDescriptor::kEndOffset);
  StaticVisitor::VisitPointers(heap, start_slot, end_slot);
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitSharedFunctionInfoWeakCode(
    Heap* heap, HeapObject* object) {
  StaticVisitor::BeforeVisitingSharedFunctionInfo(object);
  Object** name_slot =
      HeapObject::RawField(object, SharedFunctionInfo::kNameOffset);
  StaticVisitor::VisitPointer(heap, name_slot);

  // Skip visiting kCodeOffset as it is treated weakly here.
  STATIC_ASSERT(SharedFunctionInfo::kNameOffset + kPointerSize ==
      SharedFunctionInfo::kCodeOffset);
  STATIC_ASSERT(SharedFunctionInfo::kCodeOffset + kPointerSize ==
      SharedFunctionInfo::kOptimizedCodeMapOffset);

  Object** start_slot =
      HeapObject::RawField(object,
                           SharedFunctionInfo::kOptimizedCodeMapOffset);
  Object** end_slot =
      HeapObject::RawField(object,
                           SharedFunctionInfo::BodyDescriptor::kEndOffset);
  StaticVisitor::VisitPointers(heap, start_slot, end_slot);
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitJSFunctionStrongCode(
    Heap* heap, HeapObject* object) {
  Object** start_slot =
      HeapObject::RawField(object, JSFunction::kPropertiesOffset);
  Object** end_slot =
      HeapObject::RawField(object, JSFunction::kCodeEntryOffset);
  StaticVisitor::VisitPointers(heap, start_slot, end_slot);

  VisitCodeEntry(heap, object->address() + JSFunction::kCodeEntryOffset);
  STATIC_ASSERT(JSFunction::kCodeEntryOffset + kPointerSize ==
      JSFunction::kPrototypeOrInitialMapOffset);

  start_slot =
      HeapObject::RawField(object, JSFunction::kPrototypeOrInitialMapOffset);
  end_slot =
      HeapObject::RawField(object, JSFunction::kNonWeakFieldsEndOffset);
  StaticVisitor::VisitPointers(heap, start_slot, end_slot);
}


template<typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitJSFunctionWeakCode(
    Heap* heap, HeapObject* object) {
  Object** start_slot =
      HeapObject::RawField(object, JSFunction::kPropertiesOffset);
  Object** end_slot =
      HeapObject::RawField(object, JSFunction::kCodeEntryOffset);
  StaticVisitor::VisitPointers(heap, start_slot, end_slot);

  // Skip visiting kCodeEntryOffset as it is treated weakly here.
  STATIC_ASSERT(JSFunction::kCodeEntryOffset + kPointerSize ==
      JSFunction::kPrototypeOrInitialMapOffset);

  start_slot =
      HeapObject::RawField(object, JSFunction::kPrototypeOrInitialMapOffset);
  end_slot =
      HeapObject::RawField(object, JSFunction::kNonWeakFieldsEndOffset);
  StaticVisitor::VisitPointers(heap, start_slot, end_slot);
}


void Code::CodeIterateBody(ObjectVisitor* v) {
  int mode_mask = RelocInfo::kCodeTargetMask |
                  RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                  RelocInfo::ModeMask(RelocInfo::GLOBAL_PROPERTY_CELL) |
                  RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
                  RelocInfo::ModeMask(RelocInfo::JS_RETURN) |
                  RelocInfo::ModeMask(RelocInfo::DEBUG_BREAK_SLOT) |
                  RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY);

  // There are two places where we iterate code bodies: here and the
  // templated CodeIterateBody (below).  They should be kept in sync.
  IteratePointer(v, kRelocationInfoOffset);
  IteratePointer(v, kHandlerTableOffset);
  IteratePointer(v, kDeoptimizationDataOffset);
  IteratePointer(v, kTypeFeedbackInfoOffset);

  RelocIterator it(this, mode_mask);
  for (; !it.done(); it.next()) {
    it.rinfo()->Visit(v);
  }
}


template<typename StaticVisitor>
void Code::CodeIterateBody(Heap* heap) {
  int mode_mask = RelocInfo::kCodeTargetMask |
                  RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                  RelocInfo::ModeMask(RelocInfo::GLOBAL_PROPERTY_CELL) |
                  RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
                  RelocInfo::ModeMask(RelocInfo::JS_RETURN) |
                  RelocInfo::ModeMask(RelocInfo::DEBUG_BREAK_SLOT) |
                  RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY);

  // There are two places where we iterate code bodies: here and the
  // non-templated CodeIterateBody (above).  They should be kept in sync.
  StaticVisitor::VisitPointer(
      heap,
      reinterpret_cast<Object**>(this->address() + kRelocationInfoOffset));
  StaticVisitor::VisitPointer(
      heap,
      reinterpret_cast<Object**>(this->address() + kHandlerTableOffset));
  StaticVisitor::VisitPointer(
      heap,
      reinterpret_cast<Object**>(this->address() + kDeoptimizationDataOffset));
  StaticVisitor::VisitPointer(
      heap,
      reinterpret_cast<Object**>(this->address() + kTypeFeedbackInfoOffset));

  RelocIterator it(this, mode_mask);
  for (; !it.done(); it.next()) {
    it.rinfo()->template Visit<StaticVisitor>(heap);
  }
}


} }  // namespace v8::internal

#endif  // V8_OBJECTS_VISITING_INL_H_
