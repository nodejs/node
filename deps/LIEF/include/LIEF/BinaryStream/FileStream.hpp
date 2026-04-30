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
#ifndef LIEF_FILE_STREAM_H
#define LIEF_FILE_STREAM_H

#include <vector>
#include <string>
#include <fstream>

#include "LIEF/errors.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/visibility.h"

namespace LIEF {

/// Stream interface over a `std::ifstream`
class LIEF_API FileStream : public BinaryStream {
  public:
  static result<FileStream> from_file(const std::string& file);
  FileStream(std::ifstream fs, uint64_t size) :
    BinaryStream(STREAM_TYPE::FILE),
    ifs_(std::move(fs)),
    size_(size)
  {}

  FileStream() = delete;

  FileStream(const FileStream&) = delete;
  FileStream& operator=(const FileStream&) = delete;

  FileStream(FileStream&& other) noexcept = default;
  FileStream& operator=(FileStream&& other) noexcept = default;

  uint64_t size() const override {
    return size_;
  }

  std::vector<uint8_t> content() const;
  ~FileStream() override = default;

  static bool classof(const BinaryStream& stream) {
    return stream.type() == STREAM_TYPE::FILE;
  }

  ok_error_t peek_in(void* dst, uint64_t offset, uint64_t size,
                     uint64_t /* virtual_address */= 0) const override {
    if (offset > size_ || offset + size > size_) {
      return make_error_code(lief_errors::read_error);
    }
    const auto pos = ifs_.tellg();
    ifs_.seekg(offset);
    ifs_.read(static_cast<char*>(dst), size);
    ifs_.seekg(pos);
    return ok();
  }

  result<const void*> read_at(uint64_t, uint64_t, uint64_t) const override {
    return make_error_code(lief_errors::not_supported);
  }

  protected:
  mutable std::ifstream ifs_;
  uint64_t size_ = 0;
};
}

#endif
