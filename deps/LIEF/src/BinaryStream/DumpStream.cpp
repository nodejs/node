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
#include "LIEF/BinaryStream/DumpStream.hpp"
#include "LIEF/Abstract/Binary.hpp"

namespace LIEF {

std::vector<uint8_t> DumpStream::content() const {
  std::vector<uint8_t> res;
  res.resize(size());
  ref_->peek_in(res.data(), /*offset=*/0, ref_->size());
  return res;
}

ok_error_t DumpStream::peek_in(void* dst, uint64_t offset, uint64_t size,
                               uint64_t /*virtual_address*/) const {
  if (binary_ != nullptr) {
    if (auto va = binary_->offset_to_virtual_address(offset, baseaddr_)) {
      if (*va >= baseaddr_) {
        return ref_->peek_in(dst, *va - baseaddr_, size,
                             /*virtual_address=*/0);
      }
      return make_error_code(lief_errors::read_out_of_bound);
    }
  }
  return ref_->peek_in(dst, offset, size, /*virtual_address=*/0);
}

}
