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
#include "LIEF/ELF/NoteDetails/AndroidIdent.hpp"
#include "LIEF/Visitor.hpp"

#include <spdlog/fmt/fmt.h>

namespace LIEF {
namespace ELF {

static constexpr auto sdk_version_offset = 0;
static constexpr auto ndk_version_offset = sizeof(uint32_t);
static constexpr auto ndk_build_number_offset = ndk_version_offset + AndroidIdent::ndk_version_size;

uint32_t AndroidIdent::sdk_version() const {
  auto val = read_at<uint32_t>(sdk_version_offset);
  if (!val) {
    return 0;
  }
  return *val;
}

std::string AndroidIdent::ndk_version() const {
  auto val = read_string_at(ndk_version_offset);
  if (!val) {
    return "";
  }
  return *val;
}

std::string AndroidIdent::ndk_build_number() const {
  auto val = read_string_at(ndk_build_number_offset);
  if (!val) {
    return "";
  }
  return *val;
}

void AndroidIdent::sdk_version(uint32_t version) {
  write_at(sdk_version_offset, version);
}

void AndroidIdent::ndk_version(const std::string& ndk_version) {
  std::string sized_nersion = ndk_version;
  sized_nersion.resize(ndk_version_size, 0);
  write_string_at(ndk_version_offset, sized_nersion);
}

void AndroidIdent::ndk_build_number(const std::string& ndk_build_number) {
  std::string sized_nersion = ndk_build_number;
  sized_nersion.resize(ndk_build_number_size);
  write_string_at(ndk_build_number_offset, sized_nersion);
}

void AndroidIdent::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

void AndroidIdent::dump(std::ostream& os) const {
  Note::dump(os);
  os << '\n';
  os << fmt::format("SDK: {} NDK: {} NDK Build: {}",
      sdk_version(), ndk_version(), ndk_build_number()
  );
}

} // namespace ELF
} // namespace LIEF
