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
#ifndef LIEF_ELF_ANDROID_IDENT_H
#define LIEF_ELF_ANDROID_IDENT_H

#include <ostream>
#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/ELF/Note.hpp"

namespace LIEF {
namespace ELF {

/// Class representing the ".note.android.ident" section
///
/// @see: https://android.googlesource.com/platform/ndk/+/ndk-release-r16/sources/crt/crtbrand.S#39
class LIEF_API AndroidIdent : public Note {
  public:
  static constexpr size_t sdk_version_size        = sizeof(uint32_t);
  static constexpr size_t ndk_version_size        = 64 * sizeof(char);
  static constexpr size_t ndk_build_number_size   = 64 * sizeof(char);

  public:
  std::unique_ptr<Note> clone() const override {
    return std::unique_ptr<AndroidIdent>(new AndroidIdent(*this));
  }

  /// Target SDK version (or 0 if it can't be resolved)
  uint32_t sdk_version() const;

  /// NDK version used (or an empty string if it can't be parsed)
  std::string ndk_version() const;

  /// NDK build number (or an empty string if it can't be parsed)
  std::string ndk_build_number() const;

  void sdk_version(uint32_t version);
  void ndk_version(const std::string& ndk_version);
  void ndk_build_number(const std::string& ndk_build_number);

  void dump(std::ostream& os) const override;

  void accept(Visitor& visitor) const override;

  static bool classof(const Note* note) {
    return note->type() == Note::TYPE::ANDROID_IDENT;
  }

  ~AndroidIdent() override = default;

  static constexpr size_t description_size() {
    return sdk_version_size + ndk_version_size + ndk_build_number_size;
  }

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const AndroidIdent& note) {
    note.dump(os);
    return os;
  }
  protected:
  using Note::Note;
};


} // namepsace ELF
} // namespace LIEF

#endif
