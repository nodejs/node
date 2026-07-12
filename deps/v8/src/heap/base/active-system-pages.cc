// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/base/active-system-pages.h"

#include <climits>

#include "src/base/bits.h"
#include "src/base/macros.h"

namespace heap {
namespace base {

size_t ActiveSystemPages::Init(size_t header_size, size_t page_size_bits,
                               size_t user_page_size) {
#if DEBUG
  size_t page_size = 1 << page_size_bits;
  DCHECK_LE(RoundUp(user_page_size, page_size) >> page_size_bits,
            ActiveSystemPages::kMaxPages);
#endif  // DEBUG
  Clear();
  return Add(0, header_size, page_size_bits);
}

size_t ActiveSystemPages::Add(uintptr_t start, uintptr_t end,
                              size_t page_size_bits) {
  const size_t page_size = 1 << page_size_bits;

  DCHECK_LE(start, end);
  DCHECK_LE(end, kMaxPages * page_size);

  // Make sure we actually get the bitcount as argument.
  DCHECK_LT(page_size_bits, sizeof(uintptr_t) * CHAR_BIT);

  const uintptr_t start_page_bit =
      RoundDown(start, page_size) >> page_size_bits;
  const uintptr_t end_page_bit = RoundUp(end, page_size) >> page_size_bits;
  DCHECK_LE(start_page_bit, end_page_bit);

  const uintptr_t bits = end_page_bit - start_page_bit;
  DCHECK_LE(bits, kMaxPages);
  const bitset_t mask = bits == kMaxPages
                            ? int64_t{-1}
                            : ((uint64_t{1} << bits) - 1) << start_page_bit;
  const bitset_t added_pages = ~value_ & mask;
  value_ |= mask;
  return added_pages.count();
}

size_t ActiveSystemPages::Reduce(ActiveSystemPages updated_value) {
  DCHECK_EQ(~value_ & updated_value.value_, 0);
  const bitset_t removed_pages(value_ & ~updated_value.value_);
  value_ = updated_value.value_;
  return removed_pages.count();
}

size_t ActiveSystemPages::Clear() {
  const size_t removed_pages = value_.count();
  value_ = 0;
  return removed_pages;
}

size_t ActiveSystemPages::Size(size_t page_size_bits) const {
  // Make sure we don't get the full page size as argument.
  DCHECK_LT(page_size_bits, sizeof(uintptr_t) * CHAR_BIT);
  return value_.count() * (size_t{1} << page_size_bits);
}

}  // namespace base
}  // namespace heap
