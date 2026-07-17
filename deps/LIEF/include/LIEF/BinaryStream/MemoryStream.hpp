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
#ifndef LIEF_MEMORY_STREAM_H
#define LIEF_MEMORY_STREAM_H

#include <cstdint>

#include "LIEF/errors.hpp"
#include "LIEF/visibility.h"
#include "LIEF/BinaryStream/BinaryStream.hpp"

namespace LIEF {
class Binary;
class LIEF_API MemoryStream : public BinaryStream {
  public:
  using BinaryStream::p;
  using BinaryStream::end;
  using BinaryStream::start;

  MemoryStream() = delete;
  MemoryStream(uintptr_t base_address);
  MemoryStream(uintptr_t base_address, uint64_t size) :
    BinaryStream(BinaryStream::STREAM_TYPE::MEMORY),
    baseaddr_(base_address),
    size_(size)
  {}

  MemoryStream(const MemoryStream&) = delete;
  MemoryStream& operator=(const MemoryStream&) = delete;

  MemoryStream(MemoryStream&&) noexcept = default;
  MemoryStream& operator=(MemoryStream&&) noexcept = default;

  uintptr_t base_address() const {
    return this->baseaddr_;
  }

  const uint8_t* p() const override {
    return start() + pos();
  }

  const uint8_t* start() const override {
    return reinterpret_cast<const uint8_t*>(baseaddr_);
  }

  const uint8_t* end() const override {
    return start() + size_;
  }

  void binary(Binary& bin) {
    this->binary_ = &bin;
  }

  Binary* binary() {
    return this->binary_;
  }

  uint64_t size() const override {
    return size_;
  }

  ~MemoryStream() override = default;

  static bool classof(const BinaryStream& stream) {
    return stream.type() == BinaryStream::STREAM_TYPE::MEMORY;
  }

  protected:
  result<const void*> read_at(uint64_t offset, uint64_t size, uint64_t va) const override;
  uintptr_t baseaddr_ = 0;
  uint64_t size_ = 0;
  Binary* binary_ = nullptr;
};
}

#endif
