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
#include <map>

#include "LIEF/BinaryStream/FileStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/VDEX/utils.hpp"
#include "VDEX/Structures.hpp"

#include "internal_utils.hpp"


namespace LIEF::VDEX {

inline bool is_vdex(BinaryStream& stream) {
  using magic_t = std::array<char, sizeof(details::magic)>;
  if (auto magic_res = stream.peek<magic_t>(0)) {
    const auto magic = *magic_res;
    return std::equal(std::begin(magic), std::end(magic),
                      std::begin(details::magic));
  }
  return false;
}

inline vdex_version_t version(BinaryStream& stream) {
  using version_t = std::array<char, 4>;
  stream.setpos(0);
  if (!is_vdex(stream)) {
    return 0;
  }
  stream.increment_pos(sizeof(details::magic));
  if (auto ver_res = stream.peek<version_t>()) {
    const auto version = *ver_res;
    return static_cast<vdex_version_t>(parse_android_version(version.data(),
                                                             version.size()));
  }
  return 0;
}

bool is_vdex(const std::string& file) {
  if (auto stream = FileStream::from_file(file)) {
    return is_vdex(*stream);
  }
  return false;
}

bool is_vdex(const std::vector<uint8_t>& raw) {
  if (auto stream = SpanStream::from_vector(raw)) {
    return is_vdex(*stream);
  }
  return false;
}

vdex_version_t version(const std::string& file) {
  if (auto stream = FileStream::from_file(file)) {
    return version(*stream);
  }
  return 0;
}

vdex_version_t version(const std::vector<uint8_t>& raw) {
  if (auto stream = SpanStream::from_vector(raw)) {
    return version(*stream);
  }
  return 0;
}

LIEF::Android::ANDROID_VERSIONS android_version(vdex_version_t version) {
  static const std::map<vdex_version_t, LIEF::Android::ANDROID_VERSIONS>
      oat2android{
          {6, LIEF::Android::ANDROID_VERSIONS::VERSION_800},
          {10, LIEF::Android::ANDROID_VERSIONS::VERSION_810},

  };
  auto it = oat2android.lower_bound(version);
  return it == oat2android.end() ?
             LIEF::Android::ANDROID_VERSIONS::VERSION_UNKNOWN :
             it->second;
}


}
