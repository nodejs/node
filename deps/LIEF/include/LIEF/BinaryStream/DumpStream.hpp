/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
#ifndef LIEF_DUMP_STREAM_H
#define LIEF_DUMP_STREAM_H

#include <memory>
#include <cstdint>

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/visibility.h"
#include "LIEF/BinaryStream/BinaryStream.hpp"

namespace LIEF {
class Binary;

/// This stream interface provides facilities to parse a dumped binary.
///
/// A dump is a contiguous capture of a process memory that starts at a given
/// base address. This stream owns (or weakly references)
/// another BinaryStream that holds the raw dump and translates file offsets
/// into dump offsets according to this base address and the binary layout.
class LIEF_API DumpStream : public BinaryStream {
  public:
  DumpStream() = delete;
  DumpStream(uintptr_t base_address, BinaryStream& ref) :
    BinaryStream(BinaryStream::STREAM_TYPE::DUMP),
    baseaddr_(base_address),
    ref_(&ref) {}

  DumpStream(uintptr_t base_address, std::unique_ptr<BinaryStream> owned) :
    BinaryStream(BinaryStream::STREAM_TYPE::DUMP),
    baseaddr_(base_address),
    owned_(std::move(owned)),
    ref_(owned_.get()) {}

  DumpStream(const DumpStream&) = delete;
  DumpStream& operator=(const DumpStream&) = delete;

  DumpStream(DumpStream&&) noexcept = default;
  DumpStream& operator=(DumpStream&&) noexcept = default;

  uintptr_t base_address() const {
    return baseaddr_;
  }

  Binary* binary() LIEF_LIFETIMEBOUND {
    return binary_;
  }

  uint64_t size() const override {
    return ref_->size();
  }

  bool bind_binary(Binary& bin) override {
    binary_ = &bin;
    return true;
  }

  std::vector<uint8_t> content() const;

  result<const void*> read_at(uint64_t /*offset*/, uint64_t /*size*/,
                              uint64_t /*virtual_address*/ = 0) const override {
    return make_error_code(lief_errors::not_supported);
  }

  ok_error_t peek_in(void* dst, uint64_t offset, uint64_t size,
                     uint64_t virtual_address = 0) const override;

  ~DumpStream() override = default;

  static bool classof(const BinaryStream& stream) {
    return stream.type() == BinaryStream::STREAM_TYPE::DUMP;
  }

  protected:
  uintptr_t baseaddr_ = 0;
  Binary* binary_ = nullptr;
  std::unique_ptr<BinaryStream> owned_;
  BinaryStream* ref_ = nullptr;
};
}

#endif
