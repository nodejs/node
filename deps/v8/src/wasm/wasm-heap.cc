// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-heap.h"

namespace v8 {
namespace internal {
namespace wasm {

DisjointAllocationPool::DisjointAllocationPool(Address start, Address end) {
  ranges_.push_back({start, end});
}

void DisjointAllocationPool::Merge(DisjointAllocationPool&& other) {
  auto dest_it = ranges_.begin();
  auto dest_end = ranges_.end();

  for (auto src_it = other.ranges_.begin(), src_end = other.ranges_.end();
       src_it != src_end;) {
    if (dest_it == dest_end) {
      // everything else coming from src will be inserted
      // at the back of ranges_ from now on.
      ranges_.push_back(*src_it);
      ++src_it;
      continue;
    }
    // Before or adjacent to dest. Insert or merge, and advance
    // just src.
    if (dest_it->first >= src_it->second) {
      if (dest_it->first == src_it->second) {
        dest_it->first = src_it->first;
      } else {
        ranges_.insert(dest_it, {src_it->first, src_it->second});
      }
      ++src_it;
      continue;
    }
    // Src is strictly after dest. Skip over this dest.
    if (dest_it->second < src_it->first) {
      ++dest_it;
      continue;
    }
    // Src is adjacent from above. Merge and advance
    // just src, because the next src, if any, is bound to be
    // strictly above the newly-formed range.
    DCHECK_EQ(dest_it->second, src_it->first);
    dest_it->second = src_it->second;
    ++src_it;
    // Now that we merged, maybe this new range is adjacent to
    // the next. Since we assume src to have come from the
    // same original memory pool, it follows that the next src
    // must be above or adjacent to the new bubble.
    auto next_dest = dest_it;
    ++next_dest;
    if (next_dest != dest_end && dest_it->second == next_dest->first) {
      dest_it->second = next_dest->second;
      ranges_.erase(next_dest);
    }

    // src_it points now at the next, if any, src
    DCHECK_IMPLIES(src_it != src_end, src_it->first >= dest_it->second);
  }
}

DisjointAllocationPool DisjointAllocationPool::Extract(size_t size,
                                                       ExtractionMode mode) {
  DisjointAllocationPool ret;
  for (auto it = ranges_.begin(), end = ranges_.end(); it != end;) {
    auto current = it;
    ++it;
    DCHECK_LT(current->first, current->second);
    size_t current_size = reinterpret_cast<size_t>(current->second) -
                          reinterpret_cast<size_t>(current->first);
    if (size == current_size) {
      ret.ranges_.push_back(*current);
      ranges_.erase(current);
      return ret;
    }
    if (size < current_size) {
      ret.ranges_.push_back({current->first, current->first + size});
      current->first += size;
      DCHECK(current->first < current->second);
      return ret;
    }
    if (mode != kContiguous) {
      size -= current_size;
      ret.ranges_.push_back(*current);
      ranges_.erase(current);
    }
  }
  if (size > 0) {
    Merge(std::move(ret));
    return {};
  }
  return ret;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
