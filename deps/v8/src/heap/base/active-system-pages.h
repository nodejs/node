// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_BASE_ACTIVE_SYSTEM_PAGES_H_
#define V8_HEAP_BASE_ACTIVE_SYSTEM_PAGES_H_

#include <bitset>
#include <cstdint>

#include "src/base/macros.h"

namespace heap {
namespace base {

// Class implements a bitset of system pages on a heap page.
class ActiveSystemPages final {
 public:
  // Defines the maximum number of system pages that can be tracked in one
  // instance.
  static constexpr size_t kMaxPages = 64;

  // Initializes the set of active pages to the system pages for the header.
  V8_EXPORT_PRIVATE size_t Init(size_t header_size, size_t page_size_bits,
                                size_t user_page_size);

  // Adds the pages for this memory range. Returns the number of freshly added
  // pages.
  V8_EXPORT_PRIVATE size_t Add(size_t start, size_t end, size_t page_size_bits);

  // Replaces the current bitset with the given argument. The new bitset needs
  // to be a proper subset of the current pages, which means this operation
  // can't add pages. Returns the number of removed pages.
  V8_EXPORT_PRIVATE size_t Reduce(ActiveSystemPages updated_value);

  // Removes all pages. Returns the number of removed pages.
  V8_EXPORT_PRIVATE size_t Clear();

  // Returns the memory used with the given page size.
  V8_EXPORT_PRIVATE size_t Size(size_t page_size_bits) const;

 private:
  using bitset_t = std::bitset<kMaxPages>;

  bitset_t value_;
};

}  // namespace base
}  // namespace heap

#endif  // V8_HEAP_BASE_ACTIVE_SYSTEM_PAGES_H_
