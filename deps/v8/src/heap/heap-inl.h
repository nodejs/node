// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_INL_H_
#define V8_HEAP_HEAP_INL_H_

#include <cmath>

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap other than src/heap/heap.h and its
// write barrier here!
#include "src/heap/heap-write-barrier.h"
#include "src/heap/heap.h"

#include "src/base/atomic-utils.h"
#include "src/base/platform/platform.h"
#include "src/counters-inl.h"
#include "src/feedback-vector.h"

// TODO(mstarzinger): There is one more include to remove in order to no longer
// leak heap internals to users of this interface!
#include "src/heap/spaces-inl.h"
#include "src/isolate.h"
#include "src/log.h"
#include "src/msan.h"
#include "src/objects-inl.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/api-callbacks-inl.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/literal-objects.h"
#include "src/objects/microtask-queue-inl.h"
#include "src/objects/scope-info.h"
#include "src/objects/script-inl.h"
#include "src/profiler/heap-profiler.h"
#include "src/string-hasher.h"
#include "src/zone/zone-list-inl.h"

// The following header includes the write barrier essentials that can also be
// used stand-alone without including heap-inl.h.
// TODO(mlippautz): Remove once users of object-macros.h include this file on
// their own.
#include "src/heap/heap-write-barrier-inl.h"

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

#define ROOT_ACCESSOR(type, name, CamelName) \
  type* Heap::name() { return type::cast(roots_[RootIndex::k##CamelName]); }
MUTABLE_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

#define ROOT_ACCESSOR(type, name, CamelName)                                   \
  void Heap::set_##name(type* value) {                                         \
    /* The deserializer makes use of the fact that these common roots are */   \
    /* never in new space and never on a page that is being compacted.    */   \
    DCHECK(!deserialization_complete() ||                                      \
           RootCanBeWrittenAfterInitialization(RootIndex::k##CamelName));      \
    DCHECK_IMPLIES(static_cast<int>(RootIndex::k##CamelName) < kOldSpaceRoots, \
                   !InNewSpace(value));                                        \
    roots_[RootIndex::k##CamelName] = value;                                   \
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
  bool new_large_object = FLAG_young_generation_large_objects &&
                          size_in_bytes > kMaxNewSpaceHeapObjectSize;
  HeapObject* object = nullptr;
  AllocationResult allocation;
  if (NEW_SPACE == space) {
    if (large_object) {
      space = LO_SPACE;
    } else {
      if (new_large_object) {
        allocation = new_lo_space_->AllocateRaw(size_in_bytes);
      } else {
        allocation = new_space_->AllocateRaw(size_in_bytes, alignment);
      }
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
    DCHECK(CanAllocateInReadOnlySpace());
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

bool Heap::CanAllocateInReadOnlySpace() {
  return !deserialization_complete_ &&
         (isolate()->serializer_enabled() ||
          !isolate()->initialized_from_snapshot());
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
  DCHECK(string->IsExternalString());
  DCHECK(!string->IsThinString());
  external_string_table_.AddString(string);
}

void Heap::UpdateExternalString(String* string, size_t old_payload,
                                size_t new_payload) {
  DCHECK(string->IsExternalString());
  Page* page = Page::FromHeapObject(string);

  if (old_payload > new_payload)
    page->DecrementExternalBackingStoreBytes(
        ExternalBackingStoreType::kExternalString, old_payload - new_payload);
  else
    page->IncrementExternalBackingStoreBytes(
        ExternalBackingStoreType::kExternalString, new_payload - old_payload);
}

void Heap::FinalizeExternalString(String* string) {
  DCHECK(string->IsExternalString());
  Page* page = Page::FromHeapObject(string);
  ExternalString* ext_string = ExternalString::cast(string);

  page->DecrementExternalBackingStoreBytes(
      ExternalBackingStoreType::kExternalString,
      ext_string->ExternalPayloadSize());

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

// static
bool Heap::InNewSpace(Object* object) {
  DCHECK(!HasWeakHeapObjectTag(object));
  return object->IsHeapObject() && InNewSpace(HeapObject::cast(object));
}

// static
bool Heap::InNewSpace(MaybeObject* object) {
  HeapObject* heap_object;
  return object->GetHeapObject(&heap_object) && InNewSpace(heap_object);
}

// static
bool Heap::InNewSpace(HeapObject* heap_object) {
  // Inlined check from NewSpace::Contains.
  bool result = MemoryChunk::FromHeapObject(heap_object)->InNewSpace();
#ifdef DEBUG
  // If in NEW_SPACE, then check we're either not in the middle of GC or the
  // object is in to-space.
  if (result) {
    // If the object is in NEW_SPACE, then it's not in RO_SPACE so this is safe.
    Heap* heap = Heap::FromWritableHeapObject(heap_object);
    DCHECK(heap->gc_state_ != NOT_IN_GC || InToSpace(heap_object));
  }
#endif
  return result;
}

// static
bool Heap::InFromSpace(Object* object) {
  DCHECK(!HasWeakHeapObjectTag(object));
  return object->IsHeapObject() && InFromSpace(HeapObject::cast(object));
}

// static
bool Heap::InFromSpace(MaybeObject* object) {
  HeapObject* heap_object;
  return object->GetHeapObject(&heap_object) && InFromSpace(heap_object);
}

// static
bool Heap::InFromSpace(HeapObject* heap_object) {
  return MemoryChunk::FromHeapObject(heap_object)
      ->IsFlagSet(Page::IN_FROM_SPACE);
}

// static
bool Heap::InToSpace(Object* object) {
  DCHECK(!HasWeakHeapObjectTag(object));
  return object->IsHeapObject() && InToSpace(HeapObject::cast(object));
}

// static
bool Heap::InToSpace(MaybeObject* object) {
  HeapObject* heap_object;
  return object->GetHeapObject(&heap_object) && InToSpace(heap_object);
}

// static
bool Heap::InToSpace(HeapObject* heap_object) {
  return MemoryChunk::FromHeapObject(heap_object)->IsFlagSet(Page::IN_TO_SPACE);
}

bool Heap::InOldSpace(Object* object) { return old_space_->Contains(object); }

bool Heap::InReadOnlySpace(Object* object) {
  return read_only_space_->Contains(object);
}

bool Heap::InNewSpaceSlow(Address address) {
  return new_space_->ContainsSlow(address);
}

bool Heap::InOldSpaceSlow(Address address) {
  return old_space_->ContainsSlow(address);
}

// static
Heap* Heap::FromWritableHeapObject(const HeapObject* obj) {
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(obj);
  // RO_SPACE can be shared between heaps, so we can't use RO_SPACE objects to
  // find a heap. The exception is when the ReadOnlySpace is writeable, during
  // bootstrapping, so explicitly allow this case.
  SLOW_DCHECK(chunk->owner()->identity() != RO_SPACE ||
              static_cast<ReadOnlySpace*>(chunk->owner())->writable());
  Heap* heap = chunk->heap();
  SLOW_DCHECK(heap != nullptr);
  return heap;
}

bool Heap::ShouldBePromoted(Address old_address) {
  Page* page = Page::FromAddress(old_address);
  Address age_mark = new_space_->age_mark();
  return page->IsFlagSet(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK) &&
         (!page->ContainsLimit(age_mark) || old_address < age_mark);
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
  if (candidate_map != ReadOnlyRoots(this).allocation_memento_map()) {
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
  DCHECK(
      InFromSpace(object) ||
      (InToSpace(object) && Page::FromAddress(object->address())
                                ->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION)) ||
      (!InNewSpace(object) && Page::FromAddress(object->address())
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
  DCHECK(!Contains(string));

  if (InNewSpace(string)) {
    new_space_strings_.push_back(string);
  } else {
    old_space_strings_.push_back(string);
  }
}

Oddball* Heap::ToBoolean(bool condition) {
  ReadOnlyRoots roots(this);
  return condition ? roots.true_value() : roots.false_value();
}

uint64_t Heap::HashSeed() {
  uint64_t seed;
  hash_seed()->copy_out(0, reinterpret_cast<byte*>(&seed), kInt64Size);
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
  if (last_id == DebugInfo::DebuggingIdBits::kMax) {
    last_id = DebugInfo::kNoDebuggingId;
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

int Heap::MaxNumberToStringCacheSize() const {
  // Compute the size of the number string cache based on the max newspace size.
  // The number string cache has a minimum size based on twice the initial cache
  // size to ensure that it is bigger after being made 'full size'.
  size_t number_string_cache_size = max_semi_space_size_ / 512;
  number_string_cache_size =
      Max(static_cast<size_t>(kInitialNumberStringCacheSize * 2),
          Min<size_t>(0x4000u, number_string_cache_size));
  // There is a string and a number per entry so the length is twice the number
  // of entries.
  return static_cast<int>(number_string_cache_size * 2);
}

void Heap::IncrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                              size_t amount) {
  base::CheckedIncrement(&backing_store_bytes_, amount);
  // TODO(mlippautz): Implement interrupt for global memory allocations that can
  // trigger garbage collections.
}

void Heap::DecrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                              size_t amount) {
  base::CheckedDecrement(&backing_store_bytes_, amount);
}

AlwaysAllocateScope::AlwaysAllocateScope(Isolate* isolate)
    : heap_(isolate->heap()) {
  heap_->always_allocate_scope_count_++;
}

AlwaysAllocateScope::~AlwaysAllocateScope() {
  heap_->always_allocate_scope_count_--;
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
