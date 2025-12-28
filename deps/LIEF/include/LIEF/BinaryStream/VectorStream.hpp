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
#ifndef LIEF_VECTOR_STREAM_H
#define LIEF_VECTOR_STREAM_H

#include <vector>
#include <string>
#include <memory>

#include "LIEF/errors.hpp"
#include "LIEF/visibility.h"
#include "LIEF/BinaryStream/BinaryStream.hpp"

namespace LIEF {
class SpanStream;
class LIEF_API VectorStream : public BinaryStream {
  public:
  using BinaryStream::p;
  using BinaryStream::end;
  using BinaryStream::start;

  static result<VectorStream> from_file(const std::string& file);
  VectorStream(std::vector<uint8_t> data) :
    BinaryStream(BinaryStream::STREAM_TYPE::VECTOR),
    binary_(std::move(data)),
    size_(binary_.size())
  {}

  VectorStream() = delete;

  // VectorStream should not be copyable for performances reasons
  VectorStream(const VectorStream&) = delete;
  VectorStream& operator=(const VectorStream&) = delete;

  VectorStream(VectorStream&& other) noexcept = default;
  VectorStream& operator=(VectorStream&& other) noexcept = default;

  uint64_t size() const override {
    return size_;
  }

  const std::vector<uint8_t>& content() const {
    return binary_;
  }

  std::vector<uint8_t>&& move_content() {
    size_ = 0;
    return std::move(binary_);
  }

  const uint8_t* p() const override {
    return this->binary_.data() + this->pos();
  }

  const uint8_t* start() const override {
    return this->binary_.data();
  }

  const uint8_t* end() const override {
    return this->binary_.data() + this->binary_.size();
  }


  std::unique_ptr<SpanStream> slice(uint32_t offset, size_t size) const;
  std::unique_ptr<SpanStream> slice(uint32_t offset) const;

  static bool classof(const BinaryStream& stream) {
    return stream.type() == STREAM_TYPE::VECTOR;
  }

  protected:
  result<const void*> read_at(uint64_t offset, uint64_t size, uint64_t /*va*/) const override {
    const uint64_t stream_size = this->size();
    if (offset > stream_size || (offset + size) > stream_size) {
      return make_error_code(lief_errors::read_error);
    }
    return binary_.data() + offset;
  }
  std::vector<uint8_t> binary_;
  uint64_t size_ = 0; // Original size without alignment
};
}

#endif
