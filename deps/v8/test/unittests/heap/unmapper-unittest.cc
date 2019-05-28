// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "src/base/region-allocator.h"
#include "src/heap/heap-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/isolate.h"
#include "src/ostreams.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

// This is a v8::PageAllocator implementation that decorates provided page
// allocator object with page tracking functionality.
class TrackingPageAllocator : public ::v8::PageAllocator {
 public:
  explicit TrackingPageAllocator(v8::PageAllocator* page_allocator)
      : page_allocator_(page_allocator),
        allocate_page_size_(page_allocator_->AllocatePageSize()),
        commit_page_size_(page_allocator_->CommitPageSize()),
        region_allocator_(kNullAddress, size_t{0} - commit_page_size_,
                          commit_page_size_) {
    CHECK_NOT_NULL(page_allocator);
    CHECK(IsAligned(allocate_page_size_, commit_page_size_));
  }
  ~TrackingPageAllocator() override = default;

  size_t AllocatePageSize() override { return allocate_page_size_; }

  size_t CommitPageSize() override { return commit_page_size_; }

  void SetRandomMmapSeed(int64_t seed) override {
    return page_allocator_->SetRandomMmapSeed(seed);
  }

  void* GetRandomMmapAddr() override {
    return page_allocator_->GetRandomMmapAddr();
  }

  void* AllocatePages(void* address, size_t size, size_t alignment,
                      PageAllocator::Permission access) override {
    void* result =
        page_allocator_->AllocatePages(address, size, alignment, access);
    if (result) {
      // Mark pages as used.
      Address current_page = reinterpret_cast<Address>(result);
      CHECK(IsAligned(current_page, allocate_page_size_));
      CHECK(IsAligned(size, allocate_page_size_));
      CHECK(region_allocator_.AllocateRegionAt(current_page, size));
      Address end = current_page + size;
      while (current_page < end) {
        page_permissions_.insert({current_page, access});
        current_page += commit_page_size_;
      }
    }
    return result;
  }

  bool FreePages(void* address, size_t size) override {
    bool result = page_allocator_->FreePages(address, size);
    if (result) {
      // Mark pages as free.
      Address start = reinterpret_cast<Address>(address);
      CHECK(IsAligned(start, allocate_page_size_));
      CHECK(IsAligned(size, allocate_page_size_));
      size_t freed_size = region_allocator_.FreeRegion(start);
      CHECK(IsAligned(freed_size, commit_page_size_));
      CHECK_EQ(RoundUp(freed_size, allocate_page_size_), size);
      auto start_iter = page_permissions_.find(start);
      CHECK_NE(start_iter, page_permissions_.end());
      auto end_iter = page_permissions_.lower_bound(start + size);
      page_permissions_.erase(start_iter, end_iter);
    }
    return result;
  }

  bool ReleasePages(void* address, size_t size, size_t new_size) override {
    bool result = page_allocator_->ReleasePages(address, size, new_size);
    if (result) {
      Address start = reinterpret_cast<Address>(address);
      CHECK(IsAligned(start, allocate_page_size_));
      CHECK(IsAligned(size, commit_page_size_));
      CHECK(IsAligned(new_size, commit_page_size_));
      CHECK_LT(new_size, size);
      CHECK_EQ(region_allocator_.TrimRegion(start, new_size), size - new_size);
      auto start_iter = page_permissions_.find(start + new_size);
      CHECK_NE(start_iter, page_permissions_.end());
      auto end_iter = page_permissions_.lower_bound(start + size);
      page_permissions_.erase(start_iter, end_iter);
    }
    return result;
  }

  bool SetPermissions(void* address, size_t size,
                      PageAllocator::Permission access) override {
    bool result = page_allocator_->SetPermissions(address, size, access);
    if (result) {
      UpdatePagePermissions(reinterpret_cast<Address>(address), size, access);
    }
    return result;
  }

  // Returns true if all the allocated pages were freed.
  bool IsEmpty() { return page_permissions_.empty(); }

  void CheckIsFree(Address address, size_t size) {
    CHECK(IsAligned(address, allocate_page_size_));
    CHECK(IsAligned(size, allocate_page_size_));
    EXPECT_TRUE(region_allocator_.IsFree(address, size));
  }

  void CheckPagePermissions(Address address, size_t size,
                            PageAllocator::Permission access) {
    ForEachPage(address, size, [=](PagePermissionsMap::value_type* value) {
      EXPECT_EQ(access, value->second);
    });
  }

  void Print(const char* comment) const {
    i::StdoutStream os;
    os << "\n========================================="
       << "\nTracingPageAllocator state: ";
    if (comment) os << comment;
    os << "\n-----------------------------------------\n";
    region_allocator_.Print(os);
    os << "-----------------------------------------"
       << "\nPage permissions:";
    if (page_permissions_.empty()) {
      os << " empty\n";
      return;
    }
    os << "\n" << std::hex << std::showbase;

    Address contiguous_region_start = static_cast<Address>(-1);
    Address contiguous_region_end = contiguous_region_start;
    PageAllocator::Permission contiguous_region_access =
        PageAllocator::kNoAccess;
    for (auto& pair : page_permissions_) {
      if (contiguous_region_end == pair.first &&
          pair.second == contiguous_region_access) {
        contiguous_region_end += commit_page_size_;
        continue;
      }
      if (contiguous_region_start != contiguous_region_end) {
        PrintRegion(os, contiguous_region_start, contiguous_region_end,
                    contiguous_region_access);
      }
      contiguous_region_start = pair.first;
      contiguous_region_end = pair.first + commit_page_size_;
      contiguous_region_access = pair.second;
    }
    if (contiguous_region_start != contiguous_region_end) {
      PrintRegion(os, contiguous_region_start, contiguous_region_end,
                  contiguous_region_access);
    }
  }

 private:
  using PagePermissionsMap = std::map<Address, PageAllocator::Permission>;
  using ForEachFn = std::function<void(PagePermissionsMap::value_type*)>;

  static void PrintRegion(std::ostream& os, Address start, Address end,
                          PageAllocator::Permission access) {
    os << "  page: [" << start << ", " << end << "), access: ";
    switch (access) {
      case PageAllocator::kNoAccess:
        os << "--";
        break;
      case PageAllocator::kRead:
        os << "R";
        break;
      case PageAllocator::kReadWrite:
        os << "RW";
        break;
      case PageAllocator::kReadWriteExecute:
        os << "RWX";
        break;
      case PageAllocator::kReadExecute:
        os << "RX";
        break;
    }
    os << "\n";
  }

  void ForEachPage(Address address, size_t size, const ForEachFn& fn) {
    CHECK(IsAligned(address, commit_page_size_));
    CHECK(IsAligned(size, commit_page_size_));
    auto start_iter = page_permissions_.find(address);
    // Start page must exist in page_permissions_.
    CHECK_NE(start_iter, page_permissions_.end());
    auto end_iter = page_permissions_.find(address + size - commit_page_size_);
    // Ensure the last but one page exists in page_permissions_.
    CHECK_NE(end_iter, page_permissions_.end());
    // Now make it point to the next element in order to also process is by the
    // following for loop.
    ++end_iter;
    for (auto iter = start_iter; iter != end_iter; ++iter) {
      PagePermissionsMap::value_type& pair = *iter;
      fn(&pair);
    }
  }

  void UpdatePagePermissions(Address address, size_t size,
                             PageAllocator::Permission access) {
    ForEachPage(address, size, [=](PagePermissionsMap::value_type* value) {
      value->second = access;
    });
  }

  v8::PageAllocator* const page_allocator_;
  const size_t allocate_page_size_;
  const size_t commit_page_size_;
  // Region allocator tracks page allocation/deallocation requests.
  base::RegionAllocator region_allocator_;
  // This map keeps track of allocated pages' permissions.
  PagePermissionsMap page_permissions_;
};

class SequentialUnmapperTest : public TestWithIsolate {
 public:
  SequentialUnmapperTest() = default;
  ~SequentialUnmapperTest() override = default;

  static void SetUpTestCase() {
    CHECK_NULL(tracking_page_allocator_);
    old_page_allocator_ = GetPlatformPageAllocator();
    tracking_page_allocator_ = new TrackingPageAllocator(old_page_allocator_);
    CHECK(tracking_page_allocator_->IsEmpty());
    CHECK_EQ(old_page_allocator_,
             SetPlatformPageAllocatorForTesting(tracking_page_allocator_));
    old_flag_ = i::FLAG_concurrent_sweeping;
    i::FLAG_concurrent_sweeping = false;
    TestWithIsolate::SetUpTestCase();
  }

  static void TearDownTestCase() {
    TestWithIsolate::TearDownTestCase();
    i::FLAG_concurrent_sweeping = old_flag_;
    CHECK(tracking_page_allocator_->IsEmpty());
    delete tracking_page_allocator_;
    tracking_page_allocator_ = nullptr;
  }

  Heap* heap() { return isolate()->heap(); }
  MemoryAllocator* allocator() { return heap()->memory_allocator(); }
  MemoryAllocator::Unmapper* unmapper() { return allocator()->unmapper(); }

  TrackingPageAllocator* tracking_page_allocator() {
    return tracking_page_allocator_;
  }

 private:
  static TrackingPageAllocator* tracking_page_allocator_;
  static v8::PageAllocator* old_page_allocator_;
  static bool old_flag_;

  DISALLOW_COPY_AND_ASSIGN(SequentialUnmapperTest);
};

TrackingPageAllocator* SequentialUnmapperTest::tracking_page_allocator_ =
    nullptr;
v8::PageAllocator* SequentialUnmapperTest::old_page_allocator_ = nullptr;
bool SequentialUnmapperTest::old_flag_;

// See v8:5945.
TEST_F(SequentialUnmapperTest, UnmapOnTeardownAfterAlreadyFreeingPooled) {
  Page* page = allocator()->AllocatePage(
      MemoryChunkLayout::AllocatableMemoryInDataPage(),
      static_cast<PagedSpace*>(heap()->old_space()),
      Executability::NOT_EXECUTABLE);
  EXPECT_NE(nullptr, page);
  const size_t page_size = tracking_page_allocator()->AllocatePageSize();
  tracking_page_allocator()->CheckPagePermissions(page->address(), page_size,
                                                  PageAllocator::kReadWrite);
  allocator()->Free<MemoryAllocator::kPooledAndQueue>(page);
  tracking_page_allocator()->CheckPagePermissions(page->address(), page_size,
                                                  PageAllocator::kReadWrite);
  unmapper()->FreeQueuedChunks();
  tracking_page_allocator()->CheckPagePermissions(page->address(), page_size,
                                                  PageAllocator::kNoAccess);
  unmapper()->TearDown();
  if (i_isolate()->isolate_allocation_mode() ==
      IsolateAllocationMode::kInV8Heap) {
    // In this mode Isolate uses bounded page allocator which allocates pages
    // inside prereserved region. Thus these pages are kept reserved until
    // the Isolate dies.
    tracking_page_allocator()->CheckPagePermissions(page->address(), page_size,
                                                    PageAllocator::kNoAccess);
  } else {
    CHECK_EQ(IsolateAllocationMode::kInCppHeap,
             i_isolate()->isolate_allocation_mode());
    tracking_page_allocator()->CheckIsFree(page->address(), page_size);
  }
}

// See v8:5945.
TEST_F(SequentialUnmapperTest, UnmapOnTeardown) {
  Page* page = allocator()->AllocatePage(
      MemoryChunkLayout::AllocatableMemoryInDataPage(),
      static_cast<PagedSpace*>(heap()->old_space()),
      Executability::NOT_EXECUTABLE);
  EXPECT_NE(nullptr, page);
  const size_t page_size = tracking_page_allocator()->AllocatePageSize();
  tracking_page_allocator()->CheckPagePermissions(page->address(), page_size,
                                                  PageAllocator::kReadWrite);

  allocator()->Free<MemoryAllocator::kPooledAndQueue>(page);
  tracking_page_allocator()->CheckPagePermissions(page->address(), page_size,
                                                  PageAllocator::kReadWrite);
  unmapper()->TearDown();
  if (i_isolate()->isolate_allocation_mode() ==
      IsolateAllocationMode::kInV8Heap) {
    // In this mode Isolate uses bounded page allocator which allocates pages
    // inside prereserved region. Thus these pages are kept reserved until
    // the Isolate dies.
    tracking_page_allocator()->CheckPagePermissions(page->address(), page_size,
                                                    PageAllocator::kNoAccess);
  } else {
    CHECK_EQ(IsolateAllocationMode::kInCppHeap,
             i_isolate()->isolate_allocation_mode());
    tracking_page_allocator()->CheckIsFree(page->address(), page_size);
  }
}

}  // namespace internal
}  // namespace v8
