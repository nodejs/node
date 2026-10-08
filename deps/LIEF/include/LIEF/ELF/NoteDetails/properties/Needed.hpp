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
#ifndef LIEF_ELF_NOTE_DETAILS_PROPERTIES_NEEDED_H
#define LIEF_ELF_NOTE_DETAILS_PROPERTIES_NEEDED_H
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/ELF/NoteDetails/NoteGnuProperty.hpp"

namespace LIEF {
class BinaryStream;

namespace ELF {

/// This class represents the ``GNU_PROPERTY_1_NEEDED`` note property.
///
/// This property provides information about additional features that the
/// object file needs at runtime (e.g. indirect external access).
class LIEF_API Needed : public NoteGnuProperty::Property {
  public:
  enum class NEED {
    UNKNOWN = 0,
    INDIRECT_EXTERN_ACCESS, ///< The object needs indirect external access
  };

  /// Return the list of needed features
  const std::vector<NEED>& needs() const {
    return needs_;
  }

  static bool classof(const NoteGnuProperty::Property* prop) {
    return prop->type() == NoteGnuProperty::Property::TYPE::NEEDED;
  }

  static std::unique_ptr<Needed> create(BinaryStream& stream);

  void dump(std::ostream& os) const override;

  ~Needed() override = default;

  protected:
  Needed(std::vector<NEED> needs) :
    NoteGnuProperty::Property(NoteGnuProperty::Property::TYPE::NEEDED),
    needs_(std::move(needs)) {}

  std::vector<NEED> needs_;
};

LIEF_API const char* to_string(Needed::NEED need);

}
}

#endif
