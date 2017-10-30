// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef V8_REGEXP_JSREGEXP_INL_H_
#define V8_REGEXP_JSREGEXP_INL_H_

#include "src/allocation.h"
#include "src/objects.h"
#include "src/regexp/jsregexp.h"

namespace v8 {
namespace internal {


RegExpImpl::GlobalCache::~GlobalCache() {
  // Deallocate the register array if we allocated it in the constructor
  // (as opposed to using the existing jsregexp_static_offsets_vector).
  if (register_array_size_ > Isolate::kJSRegexpStaticOffsetsVectorSize) {
    DeleteArray(register_array_);
  }
}


int32_t* RegExpImpl::GlobalCache::FetchNext() {
  current_match_index_++;
  if (current_match_index_ >= num_matches_) {
    // Current batch of results exhausted.
    // Fail if last batch was not even fully filled.
    if (num_matches_ < max_matches_) {
      num_matches_ = 0;  // Signal failed match.
      return NULL;
    }

    int32_t* last_match =
        &register_array_[(current_match_index_ - 1) * registers_per_match_];
    int last_end_index = last_match[1];

    if (regexp_->TypeTag() == JSRegExp::ATOM) {
      num_matches_ = RegExpImpl::AtomExecRaw(regexp_,
                                             subject_,
                                             last_end_index,
                                             register_array_,
                                             register_array_size_);
    } else {
      int last_start_index = last_match[0];
      if (last_start_index == last_end_index) {
        // Zero-length match. Advance by one code point.
        last_end_index = AdvanceZeroLength(last_end_index);
      }
      if (last_end_index > subject_->length()) {
        num_matches_ = 0;  // Signal failed match.
        return NULL;
      }
      num_matches_ = RegExpImpl::IrregexpExecRaw(regexp_,
                                                 subject_,
                                                 last_end_index,
                                                 register_array_,
                                                 register_array_size_);
    }

    if (num_matches_ <= 0) return NULL;
    current_match_index_ = 0;
    return register_array_;
  } else {
    return &register_array_[current_match_index_ * registers_per_match_];
  }
}


int32_t* RegExpImpl::GlobalCache::LastSuccessfulMatch() {
  int index = current_match_index_ * registers_per_match_;
  if (num_matches_ == 0) {
    // After a failed match we shift back by one result.
    index -= registers_per_match_;
  }
  return &register_array_[index];
}


}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_JSREGEXP_INL_H_
