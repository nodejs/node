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

#include <google/protobuf/stubs/bytestream.h>

#include <string.h>
#include <algorithm>

#include <google/protobuf/stubs/logging.h>

namespace google {
namespace protobuf {
namespace strings {

void ByteSource::CopyTo(ByteSink* sink, size_t n) {
  while (n > 0) {
    StringPiece fragment = Peek();
    if (fragment.empty()) {
      GOOGLE_LOG(DFATAL) << "ByteSource::CopyTo() overran input.";
      break;
    }
    std::size_t fragment_size = std::min<std::size_t>(n, fragment.size());
    sink->Append(fragment.data(), fragment_size);
    Skip(fragment_size);
    n -= fragment_size;
  }
}

void ByteSink::Flush() {}

void UncheckedArrayByteSink::Append(const char* data, size_t n) {
  if (data != dest_) {
    // Catch cases where the pointer returned by GetAppendBuffer() was modified.
    GOOGLE_DCHECK(!(dest_ <= data && data < (dest_ + n)))
        << "Append() data[] overlaps with dest_[]";
    memcpy(dest_, data, n);
  }
  dest_ += n;
}

CheckedArrayByteSink::CheckedArrayByteSink(char* outbuf, size_t capacity)
    : outbuf_(outbuf), capacity_(capacity), size_(0), overflowed_(false) {
}

void CheckedArrayByteSink::Append(const char* bytes, size_t n) {
  size_t available = capacity_ - size_;
  if (n > available) {
    n = available;
    overflowed_ = true;
  }
  if (n > 0 && bytes != (outbuf_ + size_)) {
    // Catch cases where the pointer returned by GetAppendBuffer() was modified.
    GOOGLE_DCHECK(!(outbuf_ <= bytes && bytes < (outbuf_ + capacity_)))
        << "Append() bytes[] overlaps with outbuf_[]";
    memcpy(outbuf_ + size_, bytes, n);
  }
  size_ += n;
}

GrowingArrayByteSink::GrowingArrayByteSink(size_t estimated_size)
    : capacity_(estimated_size),
      buf_(new char[estimated_size]),
      size_(0) {
}

GrowingArrayByteSink::~GrowingArrayByteSink() {
  delete[] buf_;  // Just in case the user didn't call GetBuffer.
}

void GrowingArrayByteSink::Append(const char* bytes, size_t n) {
  size_t available = capacity_ - size_;
  if (bytes != (buf_ + size_)) {
    // Catch cases where the pointer returned by GetAppendBuffer() was modified.
    // We need to test for this before calling Expand() which may reallocate.
    GOOGLE_DCHECK(!(buf_ <= bytes && bytes < (buf_ + capacity_)))
        << "Append() bytes[] overlaps with buf_[]";
  }
  if (n > available) {
    Expand(n - available);
  }
  if (n > 0 && bytes != (buf_ + size_)) {
    memcpy(buf_ + size_, bytes, n);
  }
  size_ += n;
}

char* GrowingArrayByteSink::GetBuffer(size_t* nbytes) {
  ShrinkToFit();
  char* b = buf_;
  *nbytes = size_;
  buf_ = nullptr;
  size_ = capacity_ = 0;
  return b;
}

void GrowingArrayByteSink::Expand(size_t amount) {  // Expand by at least 50%.
  size_t new_capacity = std::max(capacity_ + amount, (3 * capacity_) / 2);
  char* bigger = new char[new_capacity];
  memcpy(bigger, buf_, size_);
  delete[] buf_;
  buf_ = bigger;
  capacity_ = new_capacity;
}

void GrowingArrayByteSink::ShrinkToFit() {
  // Shrink only if the buffer is large and size_ is less than 3/4
  // of capacity_.
  if (capacity_ > 256 && size_ < (3 * capacity_) / 4) {
    char* just_enough = new char[size_];
    memcpy(just_enough, buf_, size_);
    delete[] buf_;
    buf_ = just_enough;
    capacity_ = size_;
  }
}

void StringByteSink::Append(const char* data, size_t n) {
  dest_->append(data, n);
}

size_t ArrayByteSource::Available() const {
  return input_.size();
}

StringPiece ArrayByteSource::Peek() {
  return input_;
}

void ArrayByteSource::Skip(size_t n) {
  GOOGLE_DCHECK_LE(n, input_.size());
  input_.remove_prefix(n);
}

LimitByteSource::LimitByteSource(ByteSource *source, size_t limit)
  : source_(source),
    limit_(limit) {
}

size_t LimitByteSource::Available() const {
  size_t available = source_->Available();
  if (available > limit_) {
    available = limit_;
  }

  return available;
}

StringPiece LimitByteSource::Peek() {
  StringPiece piece(source_->Peek());
  if (piece.size() > limit_) {
    piece.set(piece.data(), limit_);
  }

  return piece;
}

void LimitByteSource::Skip(size_t n) {
  GOOGLE_DCHECK_LE(n, limit_);
  source_->Skip(n);
  limit_ -= n;
}

void LimitByteSource::CopyTo(ByteSink *sink, size_t n) {
  GOOGLE_DCHECK_LE(n, limit_);
  source_->CopyTo(sink, n);
  limit_ -= n;
}

}  // namespace strings
}  // namespace protobuf
}  // namespace google
