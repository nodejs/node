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
#include "src/heap/third-party/heap-api.h"

#include "src/base/atomic-utils.h"
#include "src/base/platform/platform.h"
#include "src/objects/feedback-vector.h"

// TODO(mstarzinger): There is one more include to remove in order to no longer
// leak heap internals to users of this interface!
#include "src/execution/isolate-data.h"
#include "src/execution/isolate.h"
#include "src/heap/spaces-inl.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/api-callbacks-inl.h"
#include "src/objects/cell-inl.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/property-cell.h"
#include "src/objects/scope-info.h"
#include "src/objects/script-inl.h"
#include "src/objects/slots-inl.h"
#include "src/objects/struct-inl.h"
#include "src/profiler/heap-profiler.h"
#include "src/sanitizer/msan.h"
#include "src/strings/string-hasher.h"
#include "src/zone/zone-list-inl.h"

namespace v8 {
namespace internal {

AllocationSpace AllocationResult::RetrySpace() {
  DCHECK(IsRetry());
  return static_cast<AllocationSpace>(Smi::ToInt(object_));
}

HeapObject AllocationResult::ToObjectChecked() {
  CHECK(!IsRetry());
  return HeapObject::cast(object_);
}

Isolate* Heap::isolate() {
  return reinterpret_cast<Isolate*>(
      reinterpret_cast<intptr_t>(this) -
      reinterpret_cast<size_t>(reinterpret_cast<Isolate*>(16)->heap()) + 16);
}

int64_t Heap::external_memory() {
  return isolate()->isolate_data()->external_memory_;
}

void Heap::update_external_memory(int64_t delta) {
  isolate()->isolate_data()->external_memory_ += delta;
}

void Heap::update_external_memory_concurrently_freed(uintptr_t freed) {
  external_memory_concurrently_freed_ += freed;
}

void Heap::account_external_memory_concurrently_freed() {
  isolate()->isolate_data()->external_memory_ -=
      external_memory_concurrently_freed_;
  external_memory_concurrently_freed_ = 0;
}

RootsTable& Heap::roots_table() { return isolate()->roots_table(); }

#define ROOT_ACCESSOR(Type, name, CamelName)                           \
  Type Heap::name() {                                                  \
    return Type::cast(Object(roots_table()[RootIndex::k##CamelName])); \
  }
MUTABLE_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

#define ROOT_ACCESSOR(type, name, CamelName)                                   \
  void Heap::set_##name(type value) {                                          \
    /* The deserializer makes use of the fact that these common roots are */   \
    /* never in new space and never on a page that is being compacted.    */   \
    DCHECK_IMPLIES(deserialization_complete(),                                 \
                   !RootsTable::IsImmortalImmovable(RootIndex::k##CamelName)); \
    DCHECK_IMPLIES(RootsTable::IsImmortalImmovable(RootIndex::k##CamelName),   \
                   IsImmovable(HeapObject::cast(value)));                      \
    roots_table()[RootIndex::k##CamelName] = value.ptr();                      \
  }
ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

void Heap::SetRootMaterializedObjects(FixedArray objects) {
  roots_table()[RootIndex::kMaterializedObjects] = objects.ptr();
}

void Heap::SetRootScriptList(Object value) {
  roots_table()[RootIndex::kScriptList] = value.ptr();
}

void Heap::SetRootStringTable(StringTable value) {
  roots_table()[RootIndex::kStringTable] = value.ptr();
}

void Heap::SetMessageListeners(TemplateList value) {
  roots_table()[RootIndex::kMessageListeners] = value.ptr();
}

void Heap::SetPendingOptimizeForTestBytecode(Object hash_table) {
  DCHECK(hash_table.IsObjectHashTable() || hash_table.IsUndefined(isolate()));
  roots_table()[RootIndex::kPendingOptimizeForTestBytecode] = hash_table.ptr();
}

PagedSpace* Heap::paged_space(int idx) {
  DCHECK_NE(idx, LO_SPACE);
  DCHECK_NE(idx, NEW_SPACE);
  DCHECK_NE(idx, CODE_LO_SPACE);
  DCHECK_NE(idx, NEW_LO_SPACE);
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

AllocationResult Heap::AllocateRaw(int size_in_bytes, AllocationType type,
                                   AllocationOrigin origin,
                                   AllocationAlignment alignment) {
  DCHECK(AllowHandleAllocation::IsAllowed());
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK_EQ(gc_state_, NOT_IN_GC);
#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  if (FLAG_random_gc_interval > 0 || FLAG_gc_interval >= 0) {
    if (!always_allocate() && Heap::allocation_timeout_-- <= 0) {
      return AllocationResult::Retry();
    }
  }
#endif
#ifdef DEBUG
  IncrementObjectCounters();
#endif

  bool large_object = size_in_bytes > kMaxRegularHeapObjectSize;

  HeapObject object;
  AllocationResult allocation;

  if (FLAG_single_generation && type == AllocationType::kYoung) {
    type = AllocationType::kOld;
  }

  if (AllocationType::kYoung == type) {
    if (large_object) {
      if (FLAG_young_generation_large_objects) {
        allocation = new_lo_space_->AllocateRaw(size_in_bytes);
      } else {
        // If young generation large objects are disalbed we have to tenure the
        // allocation and violate the given allocation type. This could be
        // dangerous. We may want to remove FLAG_young_generation_large_objects
        // and avoid patching.
        allocation = lo_space_->AllocateRaw(size_in_bytes);
      }
    } else {
      allocation = new_space_->AllocateRaw(size_in_bytes, alignment, origin);
    }
  } else if (AllocationType::kOld == type && !V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    if (large_object) {
      allocation = lo_space_->AllocateRaw(size_in_bytes);
    } else {
      allocation = old_space_->AllocateRaw(size_in_bytes, alignment, origin);
    }
  } else if (AllocationType::kCode == type) {
    if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
      allocation = tp_heap_->AllocateCode(size_in_bytes);
    } else if (size_in_bytes <= code_space()->AreaSize() && !large_object) {
      allocation = code_space_->AllocateRawUnaligned(size_in_bytes);
    } else {
      allocation = code_lo_space_->AllocateRaw(size_in_bytes);
    }
  } else if (AllocationType::kMap == type && !V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    allocation = map_space_->AllocateRawUnaligned(size_in_bytes);
  } else if (AllocationType::kReadOnly == type &&
             !V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    DCHECK(isolate_->serializer_enabled());
    DCHECK(!large_object);
    DCHECK(CanAllocateInReadOnlySpace());
    DCHECK_EQ(AllocationOrigin::kRuntime, origin);
    allocation =
        read_only_space_->AllocateRaw(size_in_bytes, alignment, origin);
  } else if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    allocation = tp_heap_->Allocate(size_in_bytes);
  } else {
    UNREACHABLE();
  }

  if (allocation.To(&object)) {
    if (AllocationType::kCode == type && !V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
      // Unprotect the memory chunk of the object if it was not unprotected
      // already.
      UnprotectAndRegisterMemoryChunk(object);
      ZapCodeObject(object.address(), size_in_bytes);
      if (!large_object) {
        MemoryChunk::FromHeapObject(object)
            ->GetCodeObjectRegistry()
            ->RegisterNewlyAllocatedCodeObject(object.address());
      }
    }
    OnAllocationEvent(object, size_in_bytes);
  }

  return allocation;
}

template <Heap::AllocationRetryMode mode>
HeapObject Heap::AllocateRawWith(int size, AllocationType allocation,
                                 AllocationOrigin origin,
                                 AllocationAlignment alignment) {
  DCHECK(AllowHandleAllocation::IsAllowed());
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK_EQ(gc_state_, NOT_IN_GC);
  Heap* heap = isolate()->heap();
  Address* top = heap->NewSpaceAllocationTopAddress();
  Address* limit = heap->NewSpaceAllocationLimitAddress();
  if (allocation == AllocationType::kYoung &&
      alignment == AllocationAlignment::kWordAligned &&
      size <= kMaxRegularHeapObjectSize &&
      (*limit - *top >= static_cast<unsigned>(size)) &&
      V8_LIKELY(!FLAG_single_generation && FLAG_inline_new &&
                FLAG_gc_interval == 0)) {
    DCHECK(IsAligned(size, kTaggedSize));
    HeapObject obj = HeapObject::FromAddress(*top);
    *top += size;
    heap->CreateFillerObjectAt(obj.address(), size, ClearRecordedSlots::kNo);
    MSAN_ALLOCATED_UNINITIALIZED_MEMORY(obj.address(), size);
    return obj;
  }
  switch (mode) {
    case kLightRetry:
      return AllocateRawWithLightRetrySlowPath(size, allocation, origin,
                                               alignment);
    case kRetryOrFail:
      return AllocateRawWithRetryOrFailSlowPath(size, allocation, origin,
                                                alignment);
  }
  UNREACHABLE();
}

void Heap::OnAllocationEvent(HeapObject object, int size_in_bytes) {
  for (auto& tracker : allocation_trackers_) {
    tracker->AllocationEvent(object.address(), size_in_bytes);
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

bool Heap::CanAllocateInReadOnlySpace() {
  return read_only_space()->writable();
}

void Heap::UpdateAllocationsHash(HeapObject object) {
  Address object_address = object.address();
  MemoryChunk* memory_chunk = MemoryChunk::FromAddress(object_address);
  AllocationSpace allocation_space = memory_chunk->owner_identity();

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

void Heap::RegisterExternalString(String string) {
  DCHECK(string.IsExternalString());
  DCHECK(!string.IsThinString());
  external_string_table_.AddString(string);
}

void Heap::FinalizeExternalString(String string) {
  DCHECK(string.IsExternalString());
  Page* page = Page::FromHeapObject(string);
  ExternalString ext_string = ExternalString::cast(string);

  page->DecrementExternalBackingStoreBytes(
      ExternalBackingStoreType::kExternalString,
      ext_string.ExternalPayloadSize());

  ext_string.DisposeResource();
}

Address Heap::NewSpaceTop() { return new_space_->top(); }

bool Heap::InYoungGeneration(Object object) {
  DCHECK(!HasWeakHeapObjectTag(object));
  return object.IsHeapObject() && InYoungGeneration(HeapObject::cast(object));
}

// static
bool Heap::InYoungGeneration(MaybeObject object) {
  HeapObject heap_object;
  return object->GetHeapObject(&heap_object) && InYoungGeneration(heap_object);
}

// static
bool Heap::InYoungGeneration(HeapObject heap_object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return false;
  bool result = MemoryChunk::FromHeapObject(heap_object)->InYoungGeneration();
#ifdef DEBUG
  // If in the young generation, then check we're either not in the middle of
  // GC or the object is in to-space.
  if (result) {
    // If the object is in the young generation, then it's not in RO_SPACE so
    // this is safe.
    Heap* heap = Heap::FromWritableHeapObject(heap_object);
    DCHECK_IMPLIES(heap->gc_state_ == NOT_IN_GC, InToPage(heap_object));
  }
#endif
  return result;
}

// static
bool Heap::InFromPage(Object object) {
  DCHECK(!HasWeakHeapObjectTag(object));
  return object.IsHeapObject() && InFromPage(HeapObject::cast(object));
}

// static
bool Heap::InFromPage(MaybeObject object) {
  HeapObject heap_object;
  return object->GetHeapObject(&heap_object) && InFromPage(heap_object);
}

// static
bool Heap::InFromPage(HeapObject heap_object) {
  return MemoryChunk::FromHeapObject(heap_object)->IsFromPage();
}

// static
bool Heap::InToPage(Object object) {
  DCHECK(!HasWeakHeapObjectTag(object));
  return object.IsHeapObject() && InToPage(HeapObject::cast(object));
}

// static
bool Heap::InToPage(MaybeObject object) {
  HeapObject heap_object;
  return object->GetHeapObject(&heap_object) && InToPage(heap_object);
}

// static
bool Heap::InToPage(HeapObject heap_object) {
  return MemoryChunk::FromHeapObject(heap_object)->IsToPage();
}

bool Heap::InOldSpace(Object object) { return old_space_->Contains(object); }

// static
Heap* Heap::FromWritableHeapObject(HeapObject obj) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    return Heap::GetIsolateFromWritableObject(obj)->heap();
  }
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(obj);
  // RO_SPACE can be shared between heaps, so we can't use RO_SPACE objects to
  // find a heap. The exception is when the ReadOnlySpace is writeable, during
  // bootstrapping, so explicitly allow this case.
  SLOW_DCHECK(chunk->IsWritable());
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
  DCHECK(IsAligned(byte_size, kTaggedSize));
  CopyTagged(dst, src, static_cast<size_t>(byte_size / kTaggedSize));
}

template <Heap::FindMementoMode mode>
AllocationMemento Heap::FindAllocationMemento(Map map, HeapObject object) {
  Address object_address = object.address();
  Address memento_address = object_address + object.SizeFromMap(map);
  Address last_memento_word_address = memento_address + kTaggedSize;
  // If the memento would be on another page, bail out immediately.
  if (!Page::OnSamePage(object_address, last_memento_word_address)) {
    return AllocationMemento();
  }
  HeapObject candidate = HeapObject::FromAddress(memento_address);
  ObjectSlot candidate_map_slot = candidate.map_slot();
  // This fast check may peek at an uninitialized word. However, the slow check
  // below (memento_address == top) ensures that this is safe. Mark the word as
  // initialized to silence MemorySanitizer warnings.
  MSAN_MEMORY_IS_INITIALIZED(candidate_map_slot.address(), kTaggedSize);
  if (!candidate_map_slot.contains_value(
          ReadOnlyRoots(this).allocation_memento_map().ptr())) {
    return AllocationMemento();
  }

  // Bail out if the memento is below the age mark, which can happen when
  // mementos survived because a page got moved within new space.
  Page* object_page = Page::FromAddress(object_address);
  if (object_page->IsFlagSet(Page::NEW_SPACE_BELOW_AGE_MARK)) {
    Address age_mark =
        reinterpret_cast<SemiSpace*>(object_page->owner())->age_mark();
    if (!object_page->Contains(age_mark)) {
      return AllocationMemento();
    }
    // Do an exact check in the case where the age mark is on the same page.
    if (object_address < age_mark) {
      return AllocationMemento();
    }
  }

  AllocationMemento memento_candidate = AllocationMemento::cast(candidate);

  // Depending on what the memento is used for, we might need to perform
  // additional checks.
  Address top;
  switch (mode) {
    case Heap::kForGC:
      return memento_candidate;
    case Heap::kForRuntime:
      if (memento_candidate.is_null()) return AllocationMemento();
      // Either the object is the last object in the new space, or there is
      // another object of at least word size (the header map word) following
      // it, so suffices to compare ptr and top here.
      top = NewSpaceTop();
      DCHECK(memento_address == top ||
             memento_address + HeapObject::kHeaderSize <= top ||
             !Page::OnSamePage(memento_address, top - 1));
      if ((memento_address != top) && memento_candidate.IsValid()) {
        return memento_candidate;
      }
      return AllocationMemento();
    default:
      UNREACHABLE();
  }
  UNREACHABLE();
}

void Heap::UpdateAllocationSite(Map map, HeapObject object,
                                PretenuringFeedbackMap* pretenuring_feedback) {
  DCHECK_NE(pretenuring_feedback, &global_pretenuring_feedback_);
#ifdef DEBUG
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  DCHECK_IMPLIES(chunk->IsToPage(),
                 chunk->IsFlagSet(MemoryChunk::PAGE_NEW_NEW_PROMOTION));
  DCHECK_IMPLIES(!chunk->InYoungGeneration(),
                 chunk->IsFlagSet(MemoryChunk::PAGE_NEW_OLD_PROMOTION));
#endif
  if (!FLAG_allocation_site_pretenuring ||
      !AllocationSite::CanTrack(map.instance_type())) {
    return;
  }
  AllocationMemento memento_candidate =
      FindAllocationMemento<kForGC>(map, object);
  if (memento_candidate.is_null()) return;

  // Entering cached feedback is used in the parallel case. We are not allowed
  // to dereference the allocation site and rather have to postpone all checks
  // till actually merging the data.
  Address key = memento_candidate.GetAllocationSiteUnchecked();
  (*pretenuring_feedback)[AllocationSite::unchecked_cast(Object(key))]++;
}

void Heap::ExternalStringTable::AddString(String string) {
  DCHECK(string.IsExternalString());
  DCHECK(!Contains(string));

  if (InYoungGeneration(string)) {
    young_strings_.push_back(string);
  } else {
    old_strings_.push_back(string);
  }
}

Oddball Heap::ToBoolean(bool condition) {
  ReadOnlyRoots roots(this);
  return condition ? roots.true_value() : roots.false_value();
}

int Heap::NextScriptId() {
  int last_id = last_script_id().value();
  if (last_id == Smi::kMaxValue) last_id = v8::UnboundScript::kNoScriptId;
  last_id++;
  set_last_script_id(Smi::FromInt(last_id));
  return last_id;
}

int Heap::NextDebuggingId() {
  int last_id = last_debugging_id().value();
  if (last_id == DebugInfo::DebuggingIdBits::kMax) {
    last_id = DebugInfo::kNoDebuggingId;
  }
  last_id++;
  set_last_debugging_id(Smi::FromInt(last_id));
  return last_id;
}

int Heap::GetNextTemplateSerialNumber() {
  int next_serial_number = next_template_serial_number().value() + 1;
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

AlwaysAllocateScope::AlwaysAllocateScope(Heap* heap) : heap_(heap) {
  heap_->always_allocate_scope_count_++;
}

AlwaysAllocateScope::AlwaysAllocateScope(Isolate* isolate)
    : AlwaysAllocateScope(isolate->heap()) {}

AlwaysAllocateScope::~AlwaysAllocateScope() {
  heap_->always_allocate_scope_count_--;
}

CodeSpaceMemoryModificationScope::CodeSpaceMemoryModificationScope(Heap* heap)
    : heap_(heap) {
  if (heap_->write_protect_code_memory()) {
    heap_->increment_code_space_memory_modification_scope_depth();
    heap_->code_space()->SetReadAndWritable();
    LargePage* page = heap_->code_lo_space()->first_page();
    while (page != nullptr) {
      DCHECK(page->IsFlagSet(MemoryChunk::IS_EXECUTABLE));
      CHECK(heap_->memory_allocator()->IsMemoryChunkExecutable(page));
      page->SetReadAndWritable();
      page = page->next_page();
    }
  }
}

CodeSpaceMemoryModificationScope::~CodeSpaceMemoryModificationScope() {
  if (heap_->write_protect_code_memory()) {
    heap_->decrement_code_space_memory_modification_scope_depth();
    heap_->code_space()->SetDefaultCodePermissions();
    LargePage* page = heap_->code_lo_space()->first_page();
    while (page != nullptr) {
      DCHECK(page->IsFlagSet(MemoryChunk::IS_EXECUTABLE));
      CHECK(heap_->memory_allocator()->IsMemoryChunkExecutable(page));
      page->SetDefaultCodePermissions();
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

#ifdef V8_ENABLE_THIRD_PARTY_HEAP
CodePageMemoryModificationScope::CodePageMemoryModificationScope(Code code)
    : chunk_(nullptr), scope_active_(false) {}
#else
CodePageMemoryModificationScope::CodePageMemoryModificationScope(Code code)
    : CodePageMemoryModificationScope(MemoryChunk::FromHeapObject(code)) {}
#endif

CodePageMemoryModificationScope::CodePageMemoryModificationScope(
    MemoryChunk* chunk)
    : chunk_(chunk),
      scope_active_(chunk_->heap()->write_protect_code_memory() &&
                    chunk_->IsFlagSet(MemoryChunk::IS_EXECUTABLE)) {
  if (scope_active_) {
    DCHECK(chunk_->owner_identity() == CODE_SPACE ||
           (chunk_->owner_identity() == CODE_LO_SPACE));
    chunk_->SetReadAndWritable();
  }
}

CodePageMemoryModificationScope::~CodePageMemoryModificationScope() {
  if (scope_active_) {
    chunk_->SetDefaultCodePermissions();
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_INL_H_
