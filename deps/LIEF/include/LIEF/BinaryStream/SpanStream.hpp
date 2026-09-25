/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_SPAN_STREAM_H
#define LIEF_SPAN_STREAM_H

#include <cstdint>
#include <vector>
#include <array>
#include <cstddef>
#include <memory>

#include "LIEF/errors.hpp"
#include "LIEF/span.hpp"
#include "LIEF/visibility.h"
#include "LIEF/BinaryStream/BinaryStream.hpp"

namespace LIEF {
class VectorStream;
class LIEF_API SpanStream : public BinaryStream {
  public:
  using BinaryStream::p;
  using BinaryStream::end;
  using BinaryStream::start;

  static result<SpanStream> from_vector(const std::vector<uint8_t>& data) {
    return SpanStream(data);
  }

  template<size_t N>
  static result<SpanStream> from_array(const std::array<uint8_t, N>& data) {
    return SpanStream(data.data(), N);
  }

  SpanStream(span<const uint8_t> data) :
    SpanStream(data.data(), data.size())
  {}

  SpanStream(span<uint8_t> data) :
    SpanStream(data.data(), data.size())
  {}

  SpanStream(const uint8_t* p, size_t size) :
    BinaryStream(BinaryStream::STREAM_TYPE::SPAN),
    data_{p, p + size}
  {}

  SpanStream(const std::vector<uint8_t>& data) :
    SpanStream(data.data(), data.size())
  {}

  std::unique_ptr<SpanStream> clone() const {
    return std::unique_ptr<SpanStream>(new SpanStream(*this));
  }

  SpanStream() = delete;

  SpanStream(const SpanStream& other) = default;
  SpanStream& operator=(const SpanStream& other) = default;

  SpanStream(SpanStream&& other) noexcept = default;
  SpanStream& operator=(SpanStream&& other) noexcept = default;

  uint64_t size() const override {
    return data_.size();
  }

  const uint8_t* p() const override {
    return data_.data() + this->pos();
  }

  const uint8_t* start() const override {
    return data_.data();
  }

  const uint8_t* end() const override {
    return data_.data() + size();
  }

  std::vector<uint8_t> content() const {
    return {data_.begin(), data_.end()};
  }

  result<SpanStream> slice(size_t offset, size_t size) const {
    if (offset > data_.size() || (offset + size) > data_.size()) {
      return make_error_code(lief_errors::read_out_of_bound);
    }
    return data_.subspan(offset, size);
  }
  result<SpanStream> slice(size_t offset) const {
    if (offset > data_.size()) {
      return make_error_code(lief_errors::read_out_of_bound);
    }
    return data_.subspan(offset, data_.size() - offset);
  }

  std::unique_ptr<VectorStream> to_vector() const;

  static bool classof(const BinaryStream& stream) {
    return stream.type() == BinaryStream::STREAM_TYPE::SPAN;
  }

  ~SpanStream() override = default;

  protected:
  result<const void*> read_at(uint64_t offset, uint64_t size, uint64_t /*va*/) const override {
    const uint64_t stream_size = this->size();
    if (offset > stream_size || (offset + size) > stream_size) {
      return make_error_code(lief_errors::read_error);
    }
    return data_.data() + offset;
  }
  span<const uint8_t> data_;
};
}

#endif
