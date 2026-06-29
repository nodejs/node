// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/page-metadata.h"

#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/page-metadata-inl.h"
#include "src/heap/paged-spaces.h"

namespace v8::internal {

PageMetadata::PageMetadata(Heap* heap, BaseSpace* space, size_t size,
                           Address area_start, Address area_end,
                           VirtualMemory reservation,
                           Executability executability,
                           MemoryChunk::MainThreadFlags* trusted_flags)
    : MutablePageMetadata(heap, space, size, area_start, area_end,
                          std::move(reservation), PageSize::kRegular,
                          executability) {
  DCHECK(!is_large());
  trusted_main_thread_flags_ = ComputeInitialFlags(executability);
  *trusted_flags = trusted_main_thread_flags_;
}

void PageMetadata::AllocateFreeListCategories() {
  DCHECK_NULL(categories_);
  categories_ =
      new FreeListCategory*[owner()->free_list()->number_of_categories()]();
  for (int i = kFirstCategory; i <= owner()->free_list()->last_category();
       i++) {
    DCHECK_NULL(categories_[i]);
    categories_[i] = new FreeListCategory();
  }
}

void PageMetadata::InitializeFreeListCategories() {
  for (int i = kFirstCategory; i <= owner()->free_list()->last_category();
       i++) {
    categories_[i]->Initialize(static_cast<FreeListCategoryType>(i));
  }
}

void PageMetadata::ReleaseFreeListCategories() {
  if (categories_ != nullptr) {
    for (int i = kFirstCategory; i <= owner()->free_list()->last_category();
         i++) {
      if (categories_[i] != nullptr) {
        delete categories_[i];
        categories_[i] = nullptr;
      }
    }
    delete[] categories_;
    categories_ = nullptr;
  }
}

PageMetadata* PageMetadata::ConvertNewToOld(PageMetadata* old_page,
                                            FreeMode free_mode) {
  DCHECK(old_page);
  DCHECK(old_page->Chunk()->InNewSpace());
  old_page->ResetAgeInNewSpace();
  OldSpace* old_space = old_page->heap()->old_space();
  old_page->set_owner(old_space);
  old_page->ClearFlagsNonExecutable(MemoryChunk::kAllFlagsMask);
  DCHECK_NE(old_space->identity(), SHARED_SPACE);
  old_page->SetOldGenerationPageFlags(
      old_page->heap()->incremental_marking()->marking_mode());
  PageMetadata* new_page = old_space->InitializePage(old_page);
  old_space->AddPromotedPage(new_page, free_mode);
  return new_page;
}

size_t PageMetadata::AvailableInFreeList() {
  size_t sum = 0;
  ForAllFreeListCategories(
      [&sum](FreeListCategory* category) { sum += category->available(); });
  return sum;
}

void PageMetadata::MarkNeverAllocateForTesting() {
  DCHECK(this->owner_identity() != NEW_SPACE);
  DCHECK(!never_allocate_on_chunk());
  set_never_allocate_on_chunk(true);
  set_never_evacuate();
  reinterpret_cast<PagedSpace*>(owner())->free_list()->EvictFreeListItems(this);
}

void PageMetadata::CreateBlackArea(Address start, Address end) {
  DCHECK(!v8_flags.black_allocated_pages);
  DCHECK_NE(NEW_SPACE, owner_identity());
  DCHECK(v8_flags.sticky_mark_bits ||
         heap()->incremental_marking()->black_allocation());
  DCHECK_EQ(PageMetadata::FromAddress(start), this);
  DCHECK_LT(start, end);
  DCHECK_EQ(PageMetadata::FromAddress(end - 1), this);
  marking_bitmap()->SetRange<AccessMode::ATOMIC>(
      MarkingBitmap::AddressToIndex(start),
      MarkingBitmap::LimitAddressToIndex(end));
  IncrementLiveBytesAtomically(static_cast<intptr_t>(end - start));
  owner()->NotifyBlackAreaCreated(end - start);
}

void PageMetadata::DestroyBlackArea(Address start, Address end) {
  DCHECK(!v8_flags.black_allocated_pages);
  DCHECK_NE(NEW_SPACE, owner_identity());
  DCHECK(v8_flags.sticky_mark_bits ||
         heap()->incremental_marking()->black_allocation());
  DCHECK_EQ(PageMetadata::FromAddress(start), this);
  DCHECK_LT(start, end);
  DCHECK_EQ(PageMetadata::FromAddress(end - 1), this);
  marking_bitmap()->ClearRange<AccessMode::ATOMIC>(
      MarkingBitmap::AddressToIndex(start),
      MarkingBitmap::LimitAddressToIndex(end));
  IncrementLiveBytesAtomically(-static_cast<intptr_t>(end - start));
  owner()->NotifyBlackAreaDestroyed(end - start);
}

void PageMetadata::MarkEvacuationCandidate() {
  DCHECK(!never_evacuate());
  DCHECK_NULL(slot_set<OLD_TO_OLD>());
  DCHECK_NULL(typed_slot_set<OLD_TO_OLD>());
  set_is_evacuation_candidate(true);
  SetFlagMaybeExecutable(MemoryChunk::EVACUATION_CANDIDATE);
  reinterpret_cast<PagedSpace*>(owner())->free_list()->EvictFreeListItems(this);
}

void PageMetadata::ClearEvacuationCandidate() {
  CHECK(evacuation_was_aborted());
  set_evacuation_was_aborted(false);
  set_is_evacuation_candidate(false);
  ClearFlagMaybeExecutable(MemoryChunk::EVACUATION_CANDIDATE);
  InitializeFreeListCategories();
}

void PageMetadata::AbortEvacuation() {
  DCHECK(!evacuation_was_aborted());
  set_evacuation_was_aborted(true);
}

}  // namespace v8::internal
