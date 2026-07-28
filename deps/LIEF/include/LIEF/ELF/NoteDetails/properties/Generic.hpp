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
#ifndef LIEF_ELF_NOTE_DETAILS_PROPERTIES_GENERIC_H
#define LIEF_ELF_NOTE_DETAILS_PROPERTIES_GENERIC_H

#include "LIEF/ELF/NoteDetails/NoteGnuProperty.hpp"

namespace LIEF {
namespace ELF {

/// This class represents a property which doesn't have a concrete LIEF
/// implementation.
class Generic : public NoteGnuProperty::Property {
  public:

  /// The original raw type as an integer. This value might depends
  /// on the architecture and/or the file type.
  uint32_t type() const {
    return raw_type_;
  }

  static bool classof(const NoteGnuProperty::Property* prop) {
    return prop->type() == NoteGnuProperty::Property::TYPE::GENERIC;
  }

  static std::unique_ptr<Generic> create(uint32_t raw_type) {
    return std::unique_ptr<Generic>(new Generic(raw_type));
  }

  ~Generic() override = default;

  protected:
  Generic(uint32_t raw_type) :
    NoteGnuProperty::Property(NoteGnuProperty::Property::TYPE::GENERIC),
    raw_type_(raw_type)
  {}
  uint32_t raw_type_ = 0;
};
}
}
#endif
