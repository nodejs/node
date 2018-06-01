// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_INL_H_
#define V8_HEAP_HEAP_INL_H_

#include <cmath>

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap other than src/heap/heap.h here!
#include "src/heap/heap.h"

#include "src/base/platform/platform.h"
#include "src/counters-inl.h"
#include "src/feedback-vector.h"
// TODO(mstarzinger): There are 3 more includes to remove in order to no longer
// leak heap internals to users of this interface!
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/heap/store-buffer.h"
#include "src/isolate.h"
#include "src/log.h"
#include "src/msan.h"
#include "src/objects-inl.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/literal-objects.h"
#include "src/objects/scope-info.h"
#include "src/objects/script-inl.h"
#include "src/profiler/heap-profiler.h"
#include "src/string-hasher.h"
#include "src/zone/zone-list-inl.h"

namespace v8 {
namespace internal {

AllocationSpace AllocationResult::RetrySpace() {
  DCHECK(IsRetry());
  return static_cast<AllocationSpace>(Smi::ToInt(object_));
}

HeapObject* AllocationResult::ToObjectChecked() {
  CHECK(!IsRetry());
  return HeapObject::cast(object_);
}

#define ROOT_ACCESSOR(type, name, camel_name) \
  type* Heap::name() { return type::cast(roots_[k##camel_name##RootIndex]); }
ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

#define STRUCT_MAP_ACCESSOR(NAME, Name, name) \
  Map* Heap::name##_map() { return Map::cast(roots_[k##Name##MapRootIndex]); }
STRUCT_LIST(STRUCT_MAP_ACCESSOR)
#undef STRUCT_MAP_ACCESSOR

#define DATA_HANDLER_MAP_ACCESSOR(NAME, Name, Size, name)  \
  Map* Heap::name##_map() {                                \
    return Map::cast(roots_[k##Name##Size##MapRootIndex]); \
  }
DATA_HANDLER_LIST(DATA_HANDLER_MAP_ACCESSOR)
#undef DATA_HANDLER_MAP_ACCESSOR

#define STRING_ACCESSOR(name, str) \
  String* Heap::name() { return String::cast(roots_[k##name##RootIndex]); }
INTERNALIZED_STRING_LIST(STRING_ACCESSOR)
#undef STRING_ACCESSOR

#define SYMBOL_ACCESSOR(name) \
  Symbol* Heap::name() { return Symbol::cast(roots_[k##name##RootIndex]); }
PRIVATE_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define SYMBOL_ACCESSOR(name, description) \
  Symbol* Heap::name() { return Symbol::cast(roots_[k##name##RootIndex]); }
PUBLIC_SYMBOL_LIST(SYMBOL_ACCESSOR)
WELL_KNOWN_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define ACCESSOR_INFO_ACCESSOR(accessor_name, AccessorName)                \
  AccessorInfo* Heap::accessor_name##_accessor() {                         \
    return AccessorInfo::cast(roots_[k##AccessorName##AccessorRootIndex]); \
  }
ACCESSOR_INFO_LIST(ACCESSOR_INFO_ACCESSOR)
#undef ACCESSOR_INFO_ACCESSOR

#define ROOT_ACCESSOR(type, name, camel_name)                                 \
  void Heap::set_##name(type* value) {                                        \
    /* The deserializer makes use of the fact that these common roots are */  \
    /* never in new space and never on a page that is being compacted.    */  \
    DCHECK(!deserialization_complete() ||                                     \
           RootCanBeWrittenAfterInitialization(k##camel_name##RootIndex));    \
    DCHECK(k##camel_name##RootIndex >= kOldSpaceRoots || !InNewSpace(value)); \
    roots_[k##camel_name##RootIndex] = value;                                 \
  }
ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

PagedSpace* Heap::paged_space(int idx) {
  DCHECK_NE(idx, LO_SPACE);
  DCHECK_NE(idx, NEW_SPACE);
  return static_cast<PagedSpace*>(space_[idx]);
}

Space* Heap::space(int idx) { return space_[idx]; }

Address* Heap::NewSpaceAllocationTopAddress() {
  return new_space_->allocation_top_address();
}

Address* Heap::NewSpaceAllocationLimitAddress() {
  return new_space_->allocation_limit_address();
}

Address* Heap::OldSpaceAllocationTopAddress() {
  return old_space_->allocation_top_address();
}

Address* Heap::OldSpaceAllocationLimitAddress() {
  return old_space_->allocation_limit_address();
}

void Heap::UpdateNewSpaceAllocationCounter() {
  new_space_allocation_counter_ = NewSpaceAllocationCounter();
}

size_t Heap::NewSpaceAllocationCounter() {
  return new_space_allocation_counter_ + new_space()->AllocatedSinceLastGC();
}

AllocationResult Heap::AllocateRaw(int size_in_bytes, AllocationSpace space,
                                   AllocationAlignment alignment) {
  DCHECK(AllowHandleAllocation::IsAllowed());
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK(gc_state_ == NOT_IN_GC);
#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  if (FLAG_random_gc_interval > 0 || FLAG_gc_interval >= 0) {
    if (!always_allocate() && Heap::allocation_timeout_-- <= 0) {
      return AllocationResult::Retry(space);
    }
  }
#endif
#ifdef DEBUG
  isolate_->counters()->objs_since_last_full()->Increment();
  isolate_->counters()->objs_since_last_young()->Increment();
#endif

  bool large_object = size_in_bytes > kMaxRegularHeapObjectSize;
  HeapObject* object = nullptr;
  AllocationResult allocation;
  if (NEW_SPACE == space) {
    if (large_object) {
      space = LO_SPACE;
    } else {
      allocation = new_space_->AllocateRaw(size_in_bytes, alignment);
      if (allocation.To(&object)) {
        OnAllocationEvent(object, size_in_bytes);
      }
      return allocation;
    }
  }

  // Here we only allocate in the old generation.
  if (OLD_SPACE == space) {
    if (large_object) {
      allocation = lo_space_->AllocateRaw(size_in_bytes, NOT_EXECUTABLE);
    } else {
      allocation = old_space_->AllocateRaw(size_in_bytes, alignment);
    }
  } else if (CODE_SPACE == space) {
    if (size_in_bytes <= code_space()->AreaSize()) {
      allocation = code_space_->AllocateRawUnaligned(size_in_bytes);
    } else {
      allocation = lo_space_->AllocateRaw(size_in_bytes, EXECUTABLE);
    }
  } else if (LO_SPACE == space) {
    DCHECK(large_object);
    allocation = lo_space_->AllocateRaw(size_in_bytes, NOT_EXECUTABLE);
  } else if (MAP_SPACE == space) {
    allocation = map_space_->AllocateRawUnaligned(size_in_bytes);
  } else if (RO_SPACE == space) {
#ifdef V8_USE_SNAPSHOT
    DCHECK(isolate_->serializer_enabled());
#endif
    DCHECK(!large_object);
    allocation = read_only_space_->AllocateRaw(size_in_bytes, alignment);
  } else {
    // NEW_SPACE is not allowed here.
    UNREACHABLE();
  }

  if (allocation.To(&object)) {
    if (space == CODE_SPACE) {
      // Unprotect the memory chunk of the object if it was not unprotected
      // already.
      UnprotectAndRegisterMemoryChunk(object);
      ZapCodeObject(object->address(), size_in_bytes);
    }
    OnAllocationEvent(object, size_in_bytes);
  }

  return allocation;
}

void Heap::OnAllocationEvent(HeapObject* object, int size_in_bytes) {
  for (auto& tracker : allocation_trackers_) {
    tracker->AllocationEvent(object->address(), size_in_bytes);
  }

  if (FLAG_verify_predictable) {
    ++allocations_count_;
    // Advance synthetic time by making a time request.
    MonotonicallyIncreasingTimeInMs();

    UpdateAllocationsHash(object);
    UpdateAllocationsHash(size_in_bytes);

    if (allocations_count_ % FLAG_dump_allocations_digest_at_alloc == 0) {
      PrintAllocationsHash();
    }
  } else if (FLAG_fuzzer_gc_analysis) {
    ++allocations_count_;
  } else if (FLAG_trace_allocation_stack_interval > 0) {
    ++allocations_count_;
    if (allocations_count_ % FLAG_trace_allocation_stack_interval == 0) {
      isolate()->PrintStack(stdout, Isolate::kPrintStackConcise);
    }
  }
}


void Heap::OnMoveEvent(HeapObject* target, HeapObject* source,
                       int size_in_bytes) {
  HeapProfiler* heap_profiler = isolate_->heap_profiler();
  if (heap_profiler->is_tracking_object_moves()) {
    heap_profiler->ObjectMoveEvent(source->address(), target->address(),
                                   size_in_bytes);
  }
  for (auto& tracker : allocation_trackers_) {
    tracker->MoveEvent(source->address(), target->address(), size_in_bytes);
  }
  if (target->IsSharedFunctionInfo()) {
    LOG_CODE_EVENT(isolate_, SharedFunctionInfoMoveEvent(source->address(),
                                                         target->address()));
  }

  if (FLAG_verify_predictable) {
    ++allocations_count_;
    // Advance synthetic time by making a time request.
    MonotonicallyIncreasingTimeInMs();

    UpdateAllocationsHash(source);
    UpdateAllocationsHash(target);
    UpdateAllocationsHash(size_in_bytes);

    if (allocations_count_ % FLAG_dump_allocations_digest_at_alloc == 0) {
      PrintAllocationsHash();
    }
  } else if (FLAG_fuzzer_gc_analysis) {
    ++allocations_count_;
  }
}

void Heap::UpdateAllocationsHash(HeapObject* object) {
  Address object_address = object->address();
  MemoryChunk* memory_chunk = MemoryChunk::FromAddress(object_address);
  AllocationSpace allocation_space = memory_chunk->owner()->identity();

  STATIC_ASSERT(kSpaceTagSize + kPageSizeBits <= 32);
  uint32_t value =
      static_cast<uint32_t>(object_address - memory_chunk->address()) |
      (static_cast<uint32_t>(allocation_space) << kPageSizeBits);

  UpdateAllocationsHash(value);
}


void Heap::UpdateAllocationsHash(uint32_t value) {
  uint16_t c1 = static_cast<uint16_t>(value);
  uint16_t c2 = static_cast<uint16_t>(value >> 16);
  raw_allocations_hash_ =
      StringHasher::AddCharacterCore(raw_allocations_hash_, c1);
  raw_allocations_hash_ =
      StringHasher::AddCharacterCore(raw_allocations_hash_, c2);
}


void Heap::RegisterExternalString(String* string) {
  external_string_table_.AddString(string);
}


void Heap::FinalizeExternalString(String* string) {
  DCHECK(string->IsExternalString());
  v8::String::ExternalStringResourceBase** resource_addr =
      reinterpret_cast<v8::String::ExternalStringResourceBase**>(
          reinterpret_cast<byte*>(string) + ExternalString::kResourceOffset -
          kHeapObjectTag);

  // Dispose of the C++ object if it has not already been disposed.
  if (*resource_addr != nullptr) {
    (*resource_addr)->Dispose();
    *resource_addr = nullptr;
  }
}

Address Heap::NewSpaceTop() { return new_space_->top(); }

bool Heap::InNewSpace(Object* object) {
  DCHECK(!HasWeakHeapObjectTag(object));
  return object->IsHeapObject() && InNewSpace(HeapObject::cast(object));
}

bool Heap::InNewSpace(MaybeObject* object) {
  HeapObject* heap_object;
  return object->ToStrongOrWeakHeapObject(&heap_object) &&
         InNewSpace(heap_object);
}

bool Heap::InNewSpace(HeapObject* heap_object) {
  // Inlined check from NewSpace::Contains.
  bool result = Page::FromAddress(heap_object->address())->InNewSpace();
  DCHECK(!result ||                 // Either not in new space
         gc_state_ != NOT_IN_GC ||  // ... or in the middle of GC
         InToSpace(heap_object));   // ... or in to-space (where we allocate).
  return result;
}

bool Heap::InFromSpace(Object* object) {
  DCHECK(!HasWeakHeapObjectTag(object));
  return object->IsHeapObject() && InFromSpace(HeapObject::cast(object));
}

bool Heap::InFromSpace(MaybeObject* object) {
  HeapObject* heap_object;
  return object->ToStrongOrWeakHeapObject(&heap_object) &&
         InFromSpace(heap_object);
}

bool Heap::InFromSpace(HeapObject* heap_object) {
  return MemoryChunk::FromAddress(heap_object->address())
      ->IsFlagSet(Page::IN_FROM_SPACE);
}

bool Heap::InToSpace(Object* object) {
  DCHECK(!HasWeakHeapObjectTag(object));
  return object->IsHeapObject() && InToSpace(HeapObject::cast(object));
}

bool Heap::InToSpace(MaybeObject* object) {
  HeapObject* heap_object;
  return object->ToStrongOrWeakHeapObject(&heap_object) &&
         InToSpace(heap_object);
}

bool Heap::InToSpace(HeapObject* heap_object) {
  return MemoryChunk::FromAddress(heap_object->address())
      ->IsFlagSet(Page::IN_TO_SPACE);
}

bool Heap::InOldSpace(Object* object) { return old_space_->Contains(object); }

bool Heap::InNewSpaceSlow(Address address) {
  return new_space_->ContainsSlow(address);
}

bool Heap::InOldSpaceSlow(Address address) {
  return old_space_->ContainsSlow(address);
}

bool Heap::ShouldBePromoted(Address old_address) {
  Page* page = Page::FromAddress(old_address);
  Address age_mark = new_space_->age_mark();
  return page->IsFlagSet(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK) &&
         (!page->ContainsLimit(age_mark) || old_address < age_mark);
}

void Heap::RecordWrite(Object* object, Object** slot, Object* value) {
  DCHECK(!HasWeakHeapObjectTag(*slot));
  DCHECK(!HasWeakHeapObjectTag(value));
  if (!InNewSpace(value) || !object->IsHeapObject() || InNewSpace(object)) {
    return;
  }
  store_buffer()->InsertEntry(reinterpret_cast<Address>(slot));
}

void Heap::RecordWrite(Object* object, MaybeObject** slot, MaybeObject* value) {
  if (!InNewSpace(value) || !object->IsHeapObject() || InNewSpace(object)) {
    return;
  }
  store_buffer()->InsertEntry(reinterpret_cast<Address>(slot));
}

void Heap::RecordWriteIntoCode(Code* host, RelocInfo* rinfo, Object* value) {
  if (InNewSpace(value)) {
    RecordWriteIntoCodeSlow(host, rinfo, value);
  }
}

void Heap::RecordFixedArrayElements(FixedArray* array, int offset, int length) {
  if (InNewSpace(array)) return;
  for (int i = 0; i < length; i++) {
    if (!InNewSpace(array->get(offset + i))) continue;
    store_buffer()->InsertEntry(
        reinterpret_cast<Address>(array->RawFieldOfElementAt(offset + i)));
  }
}

Address* Heap::store_buffer_top_address() {
  return store_buffer()->top_address();
}

void Heap::CopyBlock(Address dst, Address src, int byte_size) {
  CopyWords(reinterpret_cast<Object**>(dst), reinterpret_cast<Object**>(src),
            static_cast<size_t>(byte_size / kPointerSize));
}

template <Heap::FindMementoMode mode>
AllocationMemento* Heap::FindAllocationMemento(Map* map, HeapObject* object) {
  Address object_address = object->address();
  Address memento_address = object_address + object->SizeFromMap(map);
  Address last_memento_word_address = memento_address + kPointerSize;
  // If the memento would be on another page, bail out immediately.
  if (!Page::OnSamePage(object_address, last_memento_word_address)) {
    return nullptr;
  }
  HeapObject* candidate = HeapObject::FromAddress(memento_address);
  Map* candidate_map = candidate->map();
  // This fast check may peek at an uninitialized word. However, the slow check
  // below (memento_address == top) ensures that this is safe. Mark the word as
  // initialized to silence MemorySanitizer warnings.
  MSAN_MEMORY_IS_INITIALIZED(&candidate_map, sizeof(candidate_map));
  if (candidate_map != allocation_memento_map()) {
    return nullptr;
  }

  // Bail out if the memento is below the age mark, which can happen when
  // mementos survived because a page got moved within new space.
  Page* object_page = Page::FromAddress(object_address);
  if (object_page->IsFlagSet(Page::NEW_SPACE_BELOW_AGE_MARK)) {
    Address age_mark =
        reinterpret_cast<SemiSpace*>(object_page->owner())->age_mark();
    if (!object_page->Contains(age_mark)) {
      return nullptr;
    }
    // Do an exact check in the case where the age mark is on the same page.
    if (object_address < age_mark) {
      return nullptr;
    }
  }

  AllocationMemento* memento_candidate = AllocationMemento::cast(candidate);

  // Depending on what the memento is used for, we might need to perform
  // additional checks.
  Address top;
  switch (mode) {
    case Heap::kForGC:
      return memento_candidate;
    case Heap::kForRuntime:
      if (memento_candidate == nullptr) return nullptr;
      // Either the object is the last object in the new space, or there is
      // another object of at least word size (the header map word) following
      // it, so suffices to compare ptr and top here.
      top = NewSpaceTop();
      DCHECK(memento_address == top ||
             memento_address + HeapObject::kHeaderSize <= top ||
             !Page::OnSamePage(memento_address, top - 1));
      if ((memento_address != top) && memento_candidate->IsValid()) {
        return memento_candidate;
      }
      return nullptr;
    default:
      UNREACHABLE();
  }
  UNREACHABLE();
}

void Heap::UpdateAllocationSite(Map* map, HeapObject* object,
                                PretenuringFeedbackMap* pretenuring_feedback) {
  DCHECK_NE(pretenuring_feedback, &global_pretenuring_feedback_);
  DCHECK(InFromSpace(object) ||
         (InToSpace(object) &&
          Page::FromAddress(object->address())
              ->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION)) ||
         (!InNewSpace(object) &&
          Page::FromAddress(object->address())
              ->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION)));
  if (!FLAG_allocation_site_pretenuring ||
      !AllocationSite::CanTrack(map->instance_type()))
    return;
  AllocationMemento* memento_candidate =
      FindAllocationMemento<kForGC>(map, object);
  if (memento_candidate == nullptr) return;

  // Entering cached feedback is used in the parallel case. We are not allowed
  // to dereference the allocation site and rather have to postpone all checks
  // till actually merging the data.
  Address key = memento_candidate->GetAllocationSiteUnchecked();
  (*pretenuring_feedback)[reinterpret_cast<AllocationSite*>(key)]++;
}

Isolate* Heap::isolate() {
  return reinterpret_cast<Isolate*>(
      reinterpret_cast<intptr_t>(this) -
      reinterpret_cast<size_t>(reinterpret_cast<Isolate*>(16)->heap()) + 16);
}

void Heap::ExternalStringTable::AddString(String* string) {
  DCHECK(string->IsExternalString());
  if (heap_->InNewSpace(string)) {
    new_space_strings_.push_back(string);
  } else {
    old_space_strings_.push_back(string);
  }
}

Oddball* Heap::ToBoolean(bool condition) {
  return condition ? true_value() : false_value();
}

uint32_t Heap::HashSeed() {
  uint32_t seed = static_cast<uint32_t>(hash_seed()->value());
  DCHECK(FLAG_randomize_hashes || seed == 0);
  return seed;
}


int Heap::NextScriptId() {
  int last_id = last_script_id()->value();
  if (last_id == Smi::kMaxValue) last_id = v8::UnboundScript::kNoScriptId;
  last_id++;
  set_last_script_id(Smi::FromInt(last_id));
  return last_id;
}

int Heap::NextDebuggingId() {
  int last_id = last_debugging_id()->value();
  if (last_id == SharedFunctionInfo::DebuggingIdBits::kMax) {
    last_id = SharedFunctionInfo::kNoDebuggingId;
  }
  last_id++;
  set_last_debugging_id(Smi::FromInt(last_id));
  return last_id;
}

int Heap::GetNextTemplateSerialNumber() {
  int next_serial_number = next_template_serial_number()->value() + 1;
  set_next_template_serial_number(Smi::FromInt(next_serial_number));
  return next_serial_number;
}

AlwaysAllocateScope::AlwaysAllocateScope(Isolate* isolate)
    : heap_(isolate->heap()) {
  heap_->always_allocate_scope_count_.Increment(1);
}

AlwaysAllocateScope::~AlwaysAllocateScope() {
  heap_->always_allocate_scope_count_.Decrement(1);
}

CodeSpaceMemoryModificationScope::CodeSpaceMemoryModificationScope(Heap* heap)
    : heap_(heap) {
  if (heap_->write_protect_code_memory()) {
    heap_->increment_code_space_memory_modification_scope_depth();
    heap_->code_space()->SetReadAndWritable();
    LargePage* page = heap_->lo_space()->first_page();
    while (page != nullptr) {
      if (page->IsFlagSet(MemoryChunk::IS_EXECUTABLE)) {
        CHECK(heap_->memory_allocator()->IsMemoryChunkExecutable(page));
        page->SetReadAndWritable();
      }
      page = page->next_page();
    }
  }
}

CodeSpaceMemoryModificationScope::~CodeSpaceMemoryModificationScope() {
  if (heap_->write_protect_code_memory()) {
    heap_->decrement_code_space_memory_modification_scope_depth();
    heap_->code_space()->SetReadAndExecutable();
    LargePage* page = heap_->lo_space()->first_page();
    while (page != nullptr) {
      if (page->IsFlagSet(MemoryChunk::IS_EXECUTABLE)) {
        CHECK(heap_->memory_allocator()->IsMemoryChunkExecutable(page));
        page->SetReadAndExecutable();
      }
      page = page->next_page();
    }
  }
}

CodePageCollectionMemoryModificationScope::
    CodePageCollectionMemoryModificationScope(Heap* heap)
    : heap_(heap) {
  if (heap_->write_protect_code_memory() &&
      !heap_->code_space_memory_modification_scope_depth()) {
    heap_->EnableUnprotectedMemoryChunksRegistry();
  }
}

CodePageCollectionMemoryModificationScope::
    ~CodePageCollectionMemoryModificationScope() {
  if (heap_->write_protect_code_memory() &&
      !heap_->code_space_memory_modification_scope_depth()) {
    heap_->ProtectUnprotectedMemoryChunks();
    heap_->DisableUnprotectedMemoryChunksRegistry();
  }
}

CodePageMemoryModificationScope::CodePageMemoryModificationScope(
    MemoryChunk* chunk)
    : chunk_(chunk),
      scope_active_(chunk_->heap()->write_protect_code_memory() &&
                    chunk_->IsFlagSet(MemoryChunk::IS_EXECUTABLE)) {
  if (scope_active_) {
    DCHECK(chunk_->owner()->identity() == CODE_SPACE ||
           (chunk_->owner()->identity() == LO_SPACE &&
            chunk_->IsFlagSet(MemoryChunk::IS_EXECUTABLE)));
    chunk_->SetReadAndWritable();
  }
}

CodePageMemoryModificationScope::~CodePageMemoryModificationScope() {
  if (scope_active_) {
    chunk_->SetReadAndExecutable();
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_INL_H_
