// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/bit-vector.h"

#include <numeric>

#include "src/base/bits.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

#ifdef DEBUG
void BitVector::Print() const {
  bool first = true;
  PrintF("{");
  for (int i = 0; i < length(); i++) {
    if (Contains(i)) {
      if (!first) PrintF(",");
      first = false;
      PrintF("%d", i);
    }
  }
  PrintF("}\n");
}
#endif

int BitVector::Count() const {
  auto accumulate_popcnt = [](int cnt, uintptr_t word) -> int {
    return cnt + base::bits::CountPopulation(word);
  };
  return std::accumulate(data_begin_, data_end_, 0, accumulate_popcnt);
}

}  // namespace internal
}  // namespace v8
