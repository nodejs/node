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
#include <map>

#include "LIEF/BinaryStream/FileStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/ART/utils.hpp"
#include "ART/Structures.hpp"

namespace LIEF {
namespace ART {

inline bool is_art(BinaryStream& stream) {
  using magic_t = std::array<char, sizeof(details::art_magic)>;
  if (auto magic_res = stream.peek<magic_t>(0)) {
    const auto magic = *magic_res;
    return std::equal(std::begin(magic), std::end(magic),
                      std::begin(details::art_magic));
  }
  return false;
}

inline art_version_t version(BinaryStream& stream) {
  using version_t = std::array<char, 4>;
  stream.setpos(0);
  if (!is_art(stream)) {
    return 0;
  }
  stream.increment_pos(sizeof(details::art_magic));
  if (auto ver_res = stream.peek<version_t>()) {
    const auto version = *ver_res;
    const bool are_digits = std::all_of(std::begin(version), std::end(version),
        [] (char c) { return c == 0 || ::isdigit(c); });
    if (!are_digits) {
      return 0;
    }
    std::string version_str(std::begin(version), std::end(version));
    return static_cast<art_version_t>(std::stoul(version_str));
  }
  return 0;
}



bool is_art(const std::string& file) {
  if (auto stream = FileStream::from_file(file)) {
    return is_art(*stream);
  }
  return false;
}

bool is_art(const std::vector<uint8_t>& raw) {
  if (auto stream = SpanStream::from_vector(raw)) {
    return is_art(*stream);
  }
  return false;
}

art_version_t version(const std::string& file) {
  if (auto stream = FileStream::from_file(file)) {
    return version(*stream);
  }
  return 0;
}

art_version_t version(const std::vector<uint8_t>& raw) {
  if (auto stream = SpanStream::from_vector(raw)) {
    return version(*stream);
  }
  return 0;
}

LIEF::Android::ANDROID_VERSIONS android_version(art_version_t version) {
  static const std::map<art_version_t, LIEF::Android::ANDROID_VERSIONS> oat2android {
    { 17, LIEF::Android::ANDROID_VERSIONS::VERSION_601 },
    { 29, LIEF::Android::ANDROID_VERSIONS::VERSION_700 },
    { 30, LIEF::Android::ANDROID_VERSIONS::VERSION_712 },
    { 44, LIEF::Android::ANDROID_VERSIONS::VERSION_800 },
    { 46, LIEF::Android::ANDROID_VERSIONS::VERSION_810 },
    { 56, LIEF::Android::ANDROID_VERSIONS::VERSION_900 },

  };
  auto   it  = oat2android.lower_bound(version);
  return it == oat2android.end() ? LIEF::Android::ANDROID_VERSIONS::VERSION_UNKNOWN : it->second;
}


}
}
