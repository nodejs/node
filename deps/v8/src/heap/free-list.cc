// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/free-list.h"

#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/heap/free-list-inl.h"
#include "src/heap/heap.h"
#include "src/heap/mutable-page-inl.h"
#include "src/heap/normal-page-inl.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/heap-object-set-map-inl.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Free lists for old object spaces implementation

void FreeListCategory::Unlink(FreeList* owner) {
  if (is_linked(owner) && !top().is_null()) {
    owner->DecreaseAvailableBytes(available_);
  }
  set_prev(nullptr);
  set_next(nullptr);
}

void FreeListCategory::Reset(FreeList* owner) {
  Unlink(owner);
  set_top(Tagged<FreeSpace>());
  available_ = 0;
}

Tagged<FreeSpace> FreeListCategory::PickNodeFromList(size_t minimum_size,
                                                     size_t* node_size) {
  Tagged<FreeSpace> node = top();
  DCHECK(!node.is_null());
  DCHECK(
      BasePage::FromHeapObject(Isolate::Current(), node)->CanAllocateOnChunk());
  int size = node->Size();
  SBXCHECK_GT(size, 0);
  SBXCHECK_EQ(MemoryChunk::FromHeapObject(node),
              MemoryChunk::FromAddress(node.address() + size - 1));
  if (static_cast<size_t>(size) < minimum_size) {
    *node_size = 0;
    return Tagged<FreeSpace>();
  }
  set_top(node->next());
  *node_size = size;
  UpdateCountersAfterAllocation(size);
  return node;
}

Tagged<FreeSpace> FreeListCategory::SearchForNodeInList(const Heap* heap,
                                                        size_t minimum_size,
                                                        size_t* node_size) {
  Tagged<FreeSpace> prev_non_evac_node;
  for (Tagged<FreeSpace> cur_node = top(); !cur_node.is_null();
       cur_node = cur_node->next()) {
    DCHECK(BasePage::FromHeapObject(Isolate::FromHeap(heap), cur_node)
               ->CanAllocateOnChunk());
    int node_size_int = cur_node->size(kRelaxedLoad);
    SBXCHECK_GT(node_size_int, 0);
    size_t size = static_cast<size_t>(node_size_int);
    SBXCHECK_EQ(MemoryChunk::FromHeapObject(cur_node),
                MemoryChunk::FromAddress(cur_node.address() + size - 1));
    if (size >= minimum_size) {
      DCHECK_GE(available_, size);
      UpdateCountersAfterAllocation(size);
      if (cur_node == top()) {
        set_top(cur_node->next());
      }
      if (!prev_non_evac_node.is_null()) {
        if (MemoryChunk::FromHeapObject(prev_non_evac_node)
                ->Metadata()
                ->is_executable()) {
          WritableJitPage jit_page(prev_non_evac_node->address(),
                                   prev_non_evac_node->Size());
          WritableFreeSpace free_space = jit_page.FreeRange(
              prev_non_evac_node->address(), prev_non_evac_node->Size());
          prev_non_evac_node->SetNext(heap, free_space, cur_node->next());
        } else {
          prev_non_evac_node->SetNext(
              heap,
              WritableFreeSpace::ForNonExecutableMemory(
                  prev_non_evac_node->address(), prev_non_evac_node->Size()),
              cur_node->next());
        }
      }
      *node_size = size;
      return cur_node;
    }

    prev_non_evac_node = cur_node;
  }
  return Tagged<FreeSpace>();
}

void FreeListCategory::Free(const Heap* heap,
                            const WritableFreeSpace& writable_free_space,
                            FreeMode mode, FreeList* owner) {
  Tagged<FreeSpace> free_space =
      Cast<FreeSpace>(HeapObject::FromAddress(writable_free_space.Address()));
  DCHECK_EQ(free_space->Size(), writable_free_space.Size());
  free_space->SetNext(heap, writable_free_space, top());
  set_top(free_space);
  size_t size_in_bytes = writable_free_space.Size();
  available_ += size_in_bytes;
  if (mode == kLinkCategory) {
    if (is_linked(owner)) {
      owner->IncreaseAvailableBytes(size_in_bytes);
    } else {
      owner->AddCategory(this);
    }
  }
}

void FreeListCategory::RepairFreeList(Heap* heap) {
  Tagged<Map> free_space_map = ReadOnlyRoots(heap).free_space_map();
  Tagged<FreeSpace> n = top();
  while (!n.is_null()) {
    ObjectSlot map_slot = n->map_slot();
    if (map_slot.contains_map_value(kNullAddress)) {
      map_slot.store_map(free_space_map);
    } else {
      DCHECK(map_slot.contains_map_value(free_space_map.ptr()));
    }
    n = n->next();
  }
}

void FreeListCategory::Relink(FreeList* owner) {
  CHECK(!is_linked(owner));
  owner->AddCategory(this);
}

// ------------------------------------------------
// Generic FreeList methods (alloc/free related)

constexpr unsigned int FreeList::categories_min[kNumberOfCategories];

FreeList::FreeList(SmallBlocksMode small_blocks_mode)
    : min_block_size_(kMinBlockSize), small_blocks_mode_(small_blocks_mode) {
  if (small_blocks_mode_ == SmallBlocksMode::kProhibit) {
    min_block_size_ =
        (v8_flags.minor_ms && (v8_flags.minor_ms_min_lab_size_kb > 0))
            ? (v8_flags.minor_ms_min_lab_size_kb * KB)
            : kFastPathStart;
  }
  Reset();
}

std::unique_ptr<FreeList> FreeList::CreateFreeList() {
  return std::make_unique<FreeList>(SmallBlocksMode::kAllow);
}

std::unique_ptr<FreeList> FreeList::CreateFreeListForNewSpace() {
  return std::make_unique<FreeList>(SmallBlocksMode::kProhibit);
}

Tagged<FreeSpace> FreeList::TryFindNodeIn(FreeListCategoryType type,
                                          size_t minimum_size,
                                          size_t* node_size) {
  FreeListCategory* category = categories_[type];
  if (category == nullptr) return Tagged<FreeSpace>();
  Tagged<FreeSpace> node = category->PickNodeFromList(minimum_size, node_size);
  if (!node.is_null()) {
    DecreaseAvailableBytes(*node_size);
    VerifyAvailable();
  }
  if (category->is_empty()) {
    RemoveCategory(category);
  }
  return node;
}

Tagged<FreeSpace> FreeList::SearchForNodeInList(const Heap* heap,
                                                FreeListCategoryType type,
                                                size_t minimum_size,
                                                size_t* node_size) {
  FreeListCategoryIterator it(this, type);
  Tagged<FreeSpace> node;
  while (it.HasNext()) {
    FreeListCategory* current = it.Next();
    node = current->SearchForNodeInList(heap, minimum_size, node_size);
    if (!node.is_null()) {
      DecreaseAvailableBytes(*node_size);
      VerifyAvailable();
      if (current->is_empty()) {
        RemoveCategory(current);
      }
      return node;
    }
  }
  return node;
}

size_t FreeList::Free(const Isolate* isolate,
                      const WritableFreeSpace& free_space, FreeMode mode) {
  Address start = free_space.Address();
  size_t size_in_bytes = free_space.Size();
  NormalPage* page = NormalPage::FromAddress(isolate, start);
  page->DecreaseAllocatedBytes(size_in_bytes);

  // Blocks have to be a minimum size to hold free list items.
  if (size_in_bytes < min_block_size_) {
    page->add_wasted_memory(size_in_bytes);
    return size_in_bytes;
  }

  // Insert other blocks at the head of a free list of the appropriate
  // magnitude.
  FreeListCategoryType type = SelectFreeListCategoryType(size_in_bytes);
  page->free_list_category(type)->Free(isolate->heap(), free_space, mode, this);

  // Updating cache
  if (mode == kLinkCategory) {
    UpdateCacheAfterAddition(type);

#ifdef DEBUG
    CheckCacheIntegrity();
#endif
  }

  DCHECK_EQ(page->AvailableInFreeList(),
            page->AvailableInFreeListFromAllocatedBytes());
  return 0;
}

NormalPage* FreeList::GetPageForSize(size_t size_in_bytes) {
  FreeListCategoryType minimum_category =
      SelectFreeListCategoryType(size_in_bytes);
  NormalPage* page = nullptr;
  for (int cat = minimum_category + 1; !page && cat <= kLastCategory; cat++) {
    page = GetPageForCategoryType(cat);
  }
  if (!page) {
    // Might return a page in which |size_in_bytes| will not fit.
    page = GetPageForCategoryType(minimum_category);
  }
  return page;
}

Tagged<FreeSpace> FreeList::Allocate(const Heap* heap, size_t size_in_bytes,
                                     size_t* node_size,
                                     AllocationOrigin origin) {
  DCHECK_GE(kMaxBlockSize, size_in_bytes);
  DCHECK_IMPLIES(small_blocks_mode_ == SmallBlocksMode::kProhibit,
                 origin != AllocationOrigin::kGC);
  Tagged<FreeSpace> node;
  FreeListCategoryType type;

  if (origin == AllocationOrigin::kGC) {
    type = SelectFreeListCategoryType(size_in_bytes);
    type = next_nonempty_category[type];
    for (; type < kLastCategory; type = next_nonempty_category[type + 1]) {
      node = TryFindNodeIn(type, size_in_bytes, node_size);
      if (!node.is_null()) break;
    }

    if (node.is_null()) {
      // Searching each element of the last category.
      type = kLastCategory;
      node = SearchForNodeInList(heap, type, size_in_bytes, node_size);
    }
  } else {
    // Fast path part 1: searching the last categories
    FreeListCategoryType first_category =
        SelectFastAllocationFreeListCategoryType(size_in_bytes);
    type = first_category;
    for (type = next_nonempty_category[type]; type <= kLastCategory;
         type = next_nonempty_category[type + 1]) {
      node = TryFindNodeIn(type, size_in_bytes, node_size);
      if (!node.is_null()) break;
    }

    // Fast path part 2: searching the medium categories for tiny objects
    if (small_blocks_mode_ == SmallBlocksMode::kAllow) {
      if (node.is_null()) {
        if (size_in_bytes <= kTinyObjectMaxSize) {
          DCHECK_EQ(kFastPathFirstCategory, first_category);
          for (type = next_nonempty_category[kFastPathFallBackTiny];
               type < kFastPathFirstCategory;
               type = next_nonempty_category[type + 1]) {
            node = TryFindNodeIn(type, size_in_bytes, node_size);
            if (!node.is_null()) break;
          }
          first_category = kFastPathFallBackTiny;
        }
      }
    }

    // Searching the last category
    if (node.is_null()) {
      // Searching each element of the last category.
      type = kLastCategory;
      node = SearchForNodeInList(heap, type, size_in_bytes, node_size);
    }

    // Finally, search the most precise category
    if (node.is_null()) {
      type = SelectFreeListCategoryType(size_in_bytes);
      for (type = next_nonempty_category[type]; type < first_category;
           type = next_nonempty_category[type + 1]) {
        node = TryFindNodeIn(type, size_in_bytes, node_size);
        if (!node.is_null()) break;
      }
    }
  }

  if (!node.is_null()) {
    if (categories_[type] == nullptr) UpdateCacheAfterRemoval(type);
    NormalPage::FromHeapObject(node)->IncreaseAllocatedBytes(*node_size);
  }

#ifdef DEBUG
  CheckCacheIntegrity();
#endif

  VerifyAvailable();
  return node;
}

// ------------------------------------------------
// Generic FreeList methods (non alloc/free related)

void FreeList::Reset() {
  ResetCache();
  ForAllFreeListCategories(
      [this](FreeListCategory* category) { category->Reset(this); });
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    categories_[i] = nullptr;
  }
  wasted_bytes_ = 0;
  available_ = 0;
}

void FreeList::ResetForNonBlackAllocatedPages() {
  DCHECK(v8_flags.black_allocated_pages);
  ResetCache();
  ForAllFreeListCategories([this](FreeListCategory* category) {
    if (!category->is_empty()) {
      auto* chunk = MemoryChunk::FromHeapObject(category->top());
      if (chunk->IsBlackAllocated()) {
        category->Unlink(this);
        return;
      }
    }
    category->Reset(this);
  });
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    categories_[i] = nullptr;
  }
  wasted_bytes_ = 0;
  available_ = 0;
}

void FreeList::EvictFreeListItems(NormalPage* page) {
  size_t sum = 0;
  page->ForAllFreeListCategories([this, &sum](FreeListCategory* category) {
    sum += category->available();
    RemoveCategory(category);
    category->Reset(this);
  });
  page->add_wasted_memory(sum);
}

void FreeList::RepairLists(Heap* heap) {
  ForAllFreeListCategories(
      [heap](FreeListCategory* category) { category->RepairFreeList(heap); });
}

bool FreeList::AddCategory(FreeListCategory* category) {
  FreeListCategoryType type = category->type_;
  DCHECK_LT(type, kNumberOfCategories);
  FreeListCategory* top = categories_[type];

  if (category->is_empty()) return false;
  DCHECK_NE(top, category);

  // Common double-linked list insertion.
  if (top != nullptr) {
    top->set_prev(category);
  }
  category->set_next(top);
  categories_[type] = category;

  IncreaseAvailableBytes(category->available());

  // Updating cache
  UpdateCacheAfterAddition(type);

#ifdef DEBUG
  CheckCacheIntegrity();
#endif

  return true;
}

void FreeList::RemoveCategory(FreeListCategory* category) {
  FreeListCategoryType type = category->type_;
  DCHECK_LT(type, kNumberOfCategories);
  FreeListCategory* top = categories_[type];

  if (category->is_linked(this)) {
    DecreaseAvailableBytes(category->available());
  }

  // Common double-linked list removal.
  if (top == category) {
    categories_[type] = category->next();
  }
  if (category->prev() != nullptr) {
    category->prev()->set_next(category->next());
  }
  if (category->next() != nullptr) {
    category->next()->set_prev(category->prev());
  }
  category->set_next(nullptr);
  category->set_prev(nullptr);

  // Updating cache
  if (categories_[type] == nullptr) {
    UpdateCacheAfterRemoval(type);
  }

#ifdef DEBUG
  CheckCacheIntegrity();
#endif
}

void FreeList::PrintCategories(FreeListCategoryType type) {
  FreeListCategoryIterator it(this, type);
  PrintF("FreeList[%p, top=%p, %d] ", static_cast<void*>(this),
         static_cast<void*>(categories_[type]), type);
  while (it.HasNext()) {
    FreeListCategory* current = it.Next();
    PrintF("%p -> ", static_cast<void*>(current));
  }
  PrintF("null\n");
}

size_t FreeListCategory::SumFreeList() {
  size_t sum = 0;
  Tagged<FreeSpace> cur = top();
  while (!cur.is_null()) {
    // We can't use "cur->map()" here because both cur's map and the
    // root can be null during bootstrapping.
    DCHECK(cur->map_slot().contains_map_value(
        Isolate::FromHeap(NormalPage::FromHeapObject(cur)->heap())
            ->root(RootIndex::kFreeSpaceMap)
            .ptr()));
    sum += cur->size(kRelaxedLoad);
    cur = cur->next();
  }
  return sum;
}
int FreeListCategory::FreeListLength() {
  int length = 0;
  Tagged<FreeSpace> cur = top();
  while (!cur.is_null()) {
    length++;
    cur = cur->next();
  }
  return length;
}

#ifdef DEBUG
bool FreeList::IsVeryLong() {
  int len = 0;
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    FreeListCategoryIterator it(this, static_cast<FreeListCategoryType>(i));
    while (it.HasNext()) {
      len += it.Next()->FreeListLength();
      if (len >= FreeListCategory::kVeryLongFreeList) return true;
    }
  }
  return false;
}

// This can take a very long time because it is linear in the number of entries
// on the free list, so it should not be called if FreeListLength returns
// kVeryLongFreeList.
size_t FreeList::SumFreeLists() {
  size_t sum = 0;
  ForAllFreeListCategories(
      [&sum](FreeListCategory* category) { sum += category->SumFreeList(); });
  return sum;
}
#endif

}  // namespace internal
}  // namespace v8
