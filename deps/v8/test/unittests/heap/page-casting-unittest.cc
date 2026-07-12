// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/base-page.h"
#include "src/heap/mutable-page.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal {

using PageCastingTest = TestWithContext;

template <typename PageType>
void CheckNullCasts() {
  BasePage* base_page_ptr = nullptr;
  const BasePage* const_base_page_ptr =
      const_cast<const BasePage*>(base_page_ptr);

  EXPECT_FALSE(Is<PageType>(base_page_ptr));
  EXPECT_FALSE(Is<const PageType>(base_page_ptr));
  PageType* out_ro_page;
  const PageType* const_out_ro_page;
  EXPECT_FALSE(TryCast<PageType>(base_page_ptr, &out_ro_page));
  EXPECT_FALSE(TryCast<const PageType>(base_page_ptr, &const_out_ro_page));
  SbxCast<PageType>(base_page_ptr);
  SbxCast<const PageType>(base_page_ptr);
  SbxCast<const PageType>(const_base_page_ptr);
  CheckedCast<PageType>(base_page_ptr);
  CheckedCast<const PageType>(base_page_ptr);
  CheckedCast<const PageType>(const_base_page_ptr);
  Cast<PageType>(base_page_ptr);
  Cast<const PageType>(base_page_ptr);
  Cast<const PageType>(const_base_page_ptr);
}

TEST_F(PageCastingTest, NullCasts) {
  CheckNullCasts<ReadOnlyPage>();
  CheckNullCasts<MutablePage>();
  CheckNullCasts<NormalPage>();
  CheckNullCasts<LargePage>();
}

template <typename PageType>
void ExpectAllowedCast(BasePage* base_page_ptr) {
  const BasePage* const_base_page_ptr =
      const_cast<const BasePage*>(base_page_ptr);
  BasePage& base_page_ref = *base_page_ptr;
  const BasePage& const_base_page_ref = *const_base_page_ptr;

  EXPECT_TRUE(Is<PageType>(base_page_ptr));
  EXPECT_TRUE(Is<PageType>(base_page_ref));
  EXPECT_TRUE(Is<const PageType>(base_page_ptr));
  EXPECT_TRUE(Is<const PageType>(base_page_ref));
  PageType* out_ro_page;
  const PageType* const_out_ro_page;
  EXPECT_TRUE(TryCast<PageType>(base_page_ptr, &out_ro_page));
  EXPECT_TRUE(TryCast<PageType>(base_page_ref, &out_ro_page));
  EXPECT_TRUE(TryCast<const PageType>(base_page_ptr, &const_out_ro_page));
  EXPECT_TRUE(TryCast<const PageType>(base_page_ref, &const_out_ro_page));
  SbxCast<PageType>(base_page_ptr);
  SbxCast<const PageType>(base_page_ptr);
  SbxCast<const PageType>(const_base_page_ptr);
  SbxCast<PageType>(base_page_ref);
  SbxCast<const PageType>(base_page_ref);
  SbxCast<const PageType>(const_base_page_ref);
  CheckedCast<PageType>(base_page_ptr);
  CheckedCast<const PageType>(base_page_ptr);
  CheckedCast<const PageType>(const_base_page_ptr);
  CheckedCast<PageType>(base_page_ref);
  CheckedCast<const PageType>(base_page_ref);
  CheckedCast<const PageType>(const_base_page_ref);
  Cast<PageType>(base_page_ptr);
  Cast<const PageType>(base_page_ptr);
  Cast<const PageType>(const_base_page_ptr);
  Cast<PageType>(base_page_ref);
  Cast<const PageType>(base_page_ref);
  Cast<const PageType>(const_base_page_ref);
}

template <typename PageType>
void ExpectProhibitedCast(BasePage* base_page_ptr) {
  const BasePage* const_base_page_ptr =
      const_cast<const BasePage*>(base_page_ptr);
  BasePage& base_page_ref = *base_page_ptr;
  const BasePage& const_base_page_ref = *const_base_page_ptr;

  EXPECT_FALSE(Is<PageType>(base_page_ptr));
  EXPECT_FALSE(Is<PageType>(base_page_ref));
  EXPECT_FALSE(Is<const PageType>(base_page_ptr));
  EXPECT_FALSE(Is<const PageType>(base_page_ref));
  PageType* out_normal_page;
  const PageType* const_out_normal_page;
  EXPECT_FALSE(TryCast<PageType>(base_page_ptr, &out_normal_page));
  EXPECT_FALSE(TryCast<PageType>(base_page_ref, &out_normal_page));
  EXPECT_FALSE(TryCast<const PageType>(base_page_ptr, &const_out_normal_page));
  EXPECT_FALSE(TryCast<const PageType>(base_page_ref, &const_out_normal_page));
  EXPECT_FALSE(
      TryCast<const PageType>(const_base_page_ptr, &const_out_normal_page));
  EXPECT_FALSE(
      TryCast<const PageType>(const_base_page_ref, &const_out_normal_page));
}

TEST_F(PageCastingTest, ReadOnlyPage) {
  Heap* heap = i_isolate()->heap();
  BasePage* base_page_ptr =
      MemoryChunk::FromAddress(heap->read_only_space()->FirstPageAddress())
          ->Metadata();
  ExpectAllowedCast<ReadOnlyPage>(base_page_ptr);
  ExpectProhibitedCast<MutablePage>(base_page_ptr);
  ExpectProhibitedCast<NormalPage>(base_page_ptr);
  ExpectProhibitedCast<LargePage>(base_page_ptr);
}

TEST_F(PageCastingTest, NewSpacePage) {
  Heap* heap = i_isolate()->heap();
  if (!heap->new_space()) {
    return;
  }
  BasePage* base_page_ptr = heap->new_space()->first_page();
  ExpectProhibitedCast<ReadOnlyPage>(base_page_ptr);
  ExpectAllowedCast<MutablePage>(base_page_ptr);
  ExpectAllowedCast<NormalPage>(base_page_ptr);
  ExpectProhibitedCast<LargePage>(base_page_ptr);
}

TEST_F(PageCastingTest, OldSpacePage) {
  Heap* heap = i_isolate()->heap();
  BasePage* base_page_ptr = heap->old_space()->first_page();
  ExpectProhibitedCast<ReadOnlyPage>(base_page_ptr);
  ExpectAllowedCast<MutablePage>(base_page_ptr);
  ExpectAllowedCast<NormalPage>(base_page_ptr);
  ExpectProhibitedCast<LargePage>(base_page_ptr);
}

}  // namespace v8::internal
