/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_TRACE_BLOB_VIEW_H_
#define SRC_TRACE_PROCESSOR_TRACE_BLOB_VIEW_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>

#include "perfetto/base/logging.h"

namespace perfetto {
namespace trace_processor {

// This class is an equivalent of std::string_view for trace binary data.
// The main difference is that this class has also shared ownership of a portion
// of the raw trace.
// The underlying buffer will be freed once all the TraceBlobViews that refer
// to the same buffer have passed through the pipeline and been parsed.
class TraceBlobView {
 public:
  TraceBlobView(std::unique_ptr<uint8_t[]> buffer, size_t offset, size_t length)
      : shbuf_(SharedBuf(std::move(buffer))),
        offset_(static_cast<uint32_t>(offset)),
        length_(static_cast<uint32_t>(length)) {
    PERFETTO_DCHECK(offset <= std::numeric_limits<uint32_t>::max());
    PERFETTO_DCHECK(length <= std::numeric_limits<uint32_t>::max());
  }

  // Allow std::move().
  TraceBlobView(TraceBlobView&&) noexcept = default;
  TraceBlobView& operator=(TraceBlobView&&) = default;

  // Disable implicit copy.
  TraceBlobView(const TraceBlobView&) = delete;
  TraceBlobView& operator=(const TraceBlobView&) = delete;

  TraceBlobView slice(size_t offset, size_t length) const {
    PERFETTO_DCHECK(offset + length <= offset_ + length_);
    return TraceBlobView(shbuf_, offset, length);
  }

  bool operator==(const TraceBlobView& rhs) const {
    return (shbuf_ == rhs.shbuf_) && (offset_ == rhs.offset_) &&
           (length_ == rhs.length_);
  }
  bool operator!=(const TraceBlobView& rhs) const { return !(*this == rhs); }

  inline const uint8_t* data() const { return start() + offset_; }

  size_t offset_of(const uint8_t* data) const {
    // When a field is size 0, data can be equal to start() + offset_ + length_.
    PERFETTO_DCHECK(data >= start() && data <= (start() + offset_ + length_));
    return static_cast<size_t>(data - start());
  }

  size_t length() const { return length_; }
  size_t offset() const { return offset_; }

 private:
  // An equivalent to std::shared_ptr<uint8_t>, with the differnce that:
  // - Supports array types, available for shared_ptr only in C++17.
  // - Is not thread safe, which is not needed for our purposes.
  class SharedBuf {
   public:
    explicit SharedBuf(std::unique_ptr<uint8_t[]> mem) {
      rcbuf_ = new RefCountedBuf(std::move(mem));
    }

    SharedBuf(const SharedBuf& copy) : rcbuf_(copy.rcbuf_) {
      PERFETTO_DCHECK(rcbuf_->refcount > 0);
      rcbuf_->refcount++;
    }

    ~SharedBuf() {
      if (!rcbuf_)
        return;
      PERFETTO_DCHECK(rcbuf_->refcount > 0);
      if (--rcbuf_->refcount == 0) {
        RefCountedBuf* rcbuf = rcbuf_;
        rcbuf_ = nullptr;
        delete rcbuf;
      }
    }

    SharedBuf(SharedBuf&& other) noexcept {
      rcbuf_ = other.rcbuf_;
      other.rcbuf_ = nullptr;
    }

    SharedBuf& operator=(SharedBuf&& other) {
      if (this != &other) {
        // A bit of a ugly but pragmatic pattern to implement move assignment.
        // First invoke the distructor and then invoke the move constructor
        // inline via placement-new.
        this->~SharedBuf();
        new (this) SharedBuf(std::move(other));
      }
      return *this;
    }

    bool operator==(const SharedBuf& x) const { return x.rcbuf_ == rcbuf_; }
    bool operator!=(const SharedBuf& x) const { return !(x == *this); }
    const uint8_t* data() const { return rcbuf_->mem.get(); }

   private:
    struct RefCountedBuf {
      explicit RefCountedBuf(std::unique_ptr<uint8_t[]> buf)
          : refcount(1), mem(std::move(buf)) {}
      int refcount;
      std::unique_ptr<uint8_t[]> mem;
    };

    RefCountedBuf* rcbuf_ = nullptr;
  };

  inline const uint8_t* start() const { return shbuf_.data(); }

  TraceBlobView(SharedBuf b, size_t o, size_t l)
      : shbuf_(b),
        offset_(static_cast<uint32_t>(o)),
        length_(static_cast<uint32_t>(l)) {}

  SharedBuf shbuf_;
  uint32_t offset_;
  uint32_t length_;  // Measured from |offset_|, not from |data()|.
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TRACE_BLOB_VIEW_H_
