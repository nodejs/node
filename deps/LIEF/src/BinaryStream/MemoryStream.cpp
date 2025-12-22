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
#include "LIEF/Abstract/Binary.hpp"
#include "LIEF/BinaryStream/MemoryStream.hpp"
#include "LIEF/utils.hpp"

namespace LIEF {

static constexpr uint64_t MAX_MEM_SIZE = 6_GB;

MemoryStream::MemoryStream(uintptr_t base_address) :
  MemoryStream(base_address, MAX_MEM_SIZE)
{}

result<const void*> MemoryStream::read_at(uint64_t offset, uint64_t size, uint64_t /*va*/) const {
  if (offset > size_ || (offset + size) > size_) {
    return make_error_code(lief_errors::read_out_of_bound);
  }

  const uintptr_t va = baseaddr_ + offset;
  if (binary_ != nullptr) {
    if (auto res = binary_->offset_to_virtual_address(offset, baseaddr_)) {
      return reinterpret_cast<const void*>(*res);
    }
  }
  return reinterpret_cast<const void*>(va);
}


}

