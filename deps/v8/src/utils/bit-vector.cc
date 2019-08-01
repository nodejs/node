// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/bit-vector.h"

#include "src/base/bits.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

#ifdef DEBUG
void BitVector::Print() {
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

void BitVector::Iterator::Advance() {
  current_++;
  uintptr_t val = current_value_;
  while (val == 0) {
    current_index_++;
    if (Done()) return;
    DCHECK(!target_->is_inline());
    val = target_->data_.ptr_[current_index_];
    current_ = current_index_ << kDataBitShift;
  }
  val = SkipZeroBytes(val);
  val = SkipZeroBits(val);
  current_value_ = val >> 1;
}

int BitVector::Count() const {
  if (data_length_ == 0) {
    return base::bits::CountPopulation(data_.inline_);
  } else {
    int count = 0;
    for (int i = 0; i < data_length_; i++) {
      count += base::bits::CountPopulation(data_.ptr_[i]);
    }
    return count;
  }
}

}  // namespace internal
}  // namespace v8
