// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <algorithm>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/repeated_field.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

namespace internal {

void** RepeatedPtrFieldBase::InternalExtend(int extend_amount) {
  int new_size = current_size_ + extend_amount;
  if (total_size_ >= new_size) {
    // N.B.: rep_ is non-NULL because extend_amount is always > 0, hence
    // total_size must be non-zero since it is lower-bounded by new_size.
    return &rep_->elements[current_size_];
  }
  Rep* old_rep = rep_;
  Arena* arena = GetArenaNoVirtual();
  new_size = std::max(kMinRepeatedFieldAllocationSize,
                      std::max(total_size_ * 2, new_size));
  GOOGLE_CHECK_LE(new_size, (std::numeric_limits<size_t>::max() - kRepHeaderSize) /
                         sizeof(old_rep->elements[0]))
      << "Requested size is too large to fit into size_t.";
  size_t bytes = kRepHeaderSize + sizeof(old_rep->elements[0]) * new_size;
  if (arena == NULL) {
    rep_ = reinterpret_cast<Rep*>(::operator new(bytes));
  } else {
    rep_ = reinterpret_cast<Rep*>(Arena::CreateArray<char>(arena, bytes));
  }
#if defined(__GXX_DELETE_WITH_SIZE__) || defined(__cpp_sized_deallocation)
  const int old_total_size = total_size_;
#endif
  total_size_ = new_size;
  if (old_rep && old_rep->allocated_size > 0) {
    memcpy(rep_->elements, old_rep->elements,
           old_rep->allocated_size * sizeof(rep_->elements[0]));
    rep_->allocated_size = old_rep->allocated_size;
  } else {
    rep_->allocated_size = 0;
  }
  if (arena == NULL) {
#if defined(__GXX_DELETE_WITH_SIZE__) || defined(__cpp_sized_deallocation)
    const size_t old_size =
        old_total_size * sizeof(rep_->elements[0]) + kRepHeaderSize;
    ::operator delete(static_cast<void*>(old_rep), old_size);
#else
    ::operator delete(static_cast<void*>(old_rep));
#endif
  }
  return &rep_->elements[current_size_];
}

void RepeatedPtrFieldBase::Reserve(int new_size) {
  if (new_size > current_size_) {
    InternalExtend(new_size - current_size_);
  }
}

void RepeatedPtrFieldBase::CloseGap(int start, int num) {
  if (rep_ == NULL) return;
  // Close up a gap of "num" elements starting at offset "start".
  for (int i = start + num; i < rep_->allocated_size; ++i)
    rep_->elements[i - num] = rep_->elements[i];
  current_size_ -= num;
  rep_->allocated_size -= num;
}

MessageLite* RepeatedPtrFieldBase::AddWeak(const MessageLite* prototype) {
  if (rep_ != NULL && current_size_ < rep_->allocated_size) {
    return reinterpret_cast<MessageLite*>(rep_->elements[current_size_++]);
  }
  if (!rep_ || rep_->allocated_size == total_size_) {
    Reserve(total_size_ + 1);
  }
  ++rep_->allocated_size;
  MessageLite* result = prototype
                            ? prototype->New(arena_)
                            : Arena::CreateMessage<ImplicitWeakMessage>(arena_);
  rep_->elements[current_size_++] = result;
  return result;
}

}  // namespace internal


template class PROTOBUF_EXPORT RepeatedField<bool>;
template class PROTOBUF_EXPORT RepeatedField<int32>;
template class PROTOBUF_EXPORT RepeatedField<uint32>;
template class PROTOBUF_EXPORT RepeatedField<int64>;
template class PROTOBUF_EXPORT RepeatedField<uint64>;
template class PROTOBUF_EXPORT RepeatedField<float>;
template class PROTOBUF_EXPORT RepeatedField<double>;
template class PROTOBUF_EXPORT RepeatedPtrField<std::string>;

}  // namespace protobuf
}  // namespace google
