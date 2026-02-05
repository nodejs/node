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
#include "LIEF/MachO/Stub.hpp"
#include "LIEF/MachO/Section.hpp"
#include "LIEF/utils.hpp"

#include "internal_utils.hpp"
#include "logging.hpp"

namespace LIEF::MachO {

inline uint32_t nb_stubs(const Section& section) {
  return section.size() / section.reserved2();
}

inline uint64_t get_offset(size_t pos, const Section& section) {
  return section.reserved2() * pos;
}

const Section*
  Stub::Iterator::find_section_offset(size_t pos, uint64_t& offset) const
{
  if (stubs_.size() == 1) {
    offset = get_offset(pos, *stubs_.back());
    return stubs_.back();
  }

  if (stubs_.size() == 2) {
    const Section* first = stubs_[0];
    const Section* second = stubs_[1];

    const uint32_t nb_stubs_1 = nb_stubs(*first);
    [[maybe_unused]] const uint32_t nb_stubs_2 = nb_stubs(*second);
    if (pos < nb_stubs_1) {
      offset = get_offset(pos, *first);
      return first;
    }

    assert(nb_stubs_1 <= pos && pos < nb_stubs_1 + nb_stubs_2);

    offset = get_offset(pos - nb_stubs_1, *second);
    return second;
  }

  uint64_t limit = nb_stubs(*stubs_[0]);
  for (size_t idx = 0; idx < stubs_.size(); ++idx) {
    if (pos < limit) {
      offset = idx > 0 ?
               get_offset(pos - (limit - nb_stubs(*stubs_[idx - 1])), *stubs_[idx]) :
               get_offset(pos, *stubs_[idx]);
      return stubs_[idx];
    }
    if (idx < stubs_.size() - 1) {
      limit += nb_stubs(*stubs_[idx + 1]);
    }
  }
  return nullptr;
}

Stub Stub::Iterator::operator*() const {
  uint64_t offset = 0;
  const Section* section = find_section_offset(pos_, offset);
  if (section == nullptr) {
    logging::fatal_error("Can't find section for stub position: {}", pos_);
  }
  LIEF::span<const uint8_t> content = section->content();
  const uint32_t stride = section->reserved2();
  if (offset >= content.size() || offset + stride > content.size()) {
    logging::fatal_error("Stub out of range at pos: {}", pos_);
  }
  LIEF::span<const uint8_t> stub_raw = section->content().subspan(offset, stride);
  const uint64_t addr = section->virtual_address() + offset;
  return Stub(
    target_info_, addr, as_vector(stub_raw)
  );
}

#if !defined(LIEF_EXTENDED)
result<uint64_t> Stub::target() const {
  logging::needs_lief_extended();
  return make_error_code(lief_errors::require_extended_version);
}
#endif

std::ostream& operator<<(std::ostream& os, const Stub& stub) {
  if (is_extended()) {
    os << fmt::format("0x{:010x}: 0x{:010x} ({} bytes)",
                       stub.address(), stub.target().value_or(0),
                       stub.raw().size());
  } else {
    os << fmt::format("0x{:010x} ({} bytes)", stub.address(), stub.raw().size());
  }
  return os;
}

}
