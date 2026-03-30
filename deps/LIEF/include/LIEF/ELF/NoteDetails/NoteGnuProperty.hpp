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
#ifndef LIEF_ELF_NOTE_GNU_PROPERTY_H
#define LIEF_ELF_NOTE_GNU_PROPERTY_H

#include <vector>
#include <ostream>
#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/ELF/Note.hpp"

namespace LIEF {
namespace ELF {

/// Class that wraps the `NT_GNU_PROPERTY_TYPE_0` note
class LIEF_API NoteGnuProperty : public Note {
  public:

  /// This class wraps the different properties that can be used in a
  /// `NT_GNU_PROPERTY_TYPE_0` note
  class LIEF_API Property {
    public:

    /// LIEF's mirror types of the original `GNU_PROPERTY_` values
    enum class TYPE {
      UNKNOWN = 0,
      GENERIC,              ///< Property that dont' have special implementation
      AARCH64_FEATURES,     ///< Mirror of `GNU_PROPERTY_AARCH64_FEATURE_1_AND`
      AARCH64_PAUTH,        ///< Mirror of `GNU_PROPERTY_AARCH64_FEATURE_PAUTH`
      STACK_SIZE,           ///< Mirror of `GNU_PROPERTY_STACK_SIZE`
      NO_COPY_ON_PROTECTED, ///< Mirror of `GNU_PROPERTY_NO_COPY_ON_PROTECTED`
      X86_ISA,              ///< Mirror of `GNU_PROPERTY_X86_ISA_1_*` and `GNU_PROPERTY_X86_COMPAT_*`
      X86_FEATURE,          ///< Mirror of `GNU_PROPERTY_X86_FEATURE_*`
      NEEDED,
    };

    /// Return the LIEF's mirror type of the note.
    TYPE type() const {
      return type_;
    }

    virtual void dump(std::ostream& os) const;

    virtual ~Property() = default;

    LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const Property& prop) {
      prop.dump(os);
      return os;
    }

    protected:
    Property() = delete;
    Property(TYPE type) :
      type_(type) {}
    TYPE type_ = TYPE::UNKNOWN;
  };

  using properties_t = std::vector<std::unique_ptr<NoteGnuProperty::Property>>;

  NoteGnuProperty(ARCH arch, Header::CLASS cls, std::string name,
                  uint32_t type, description_t description,
                  std::string secname) :
    Note(std::move(name), TYPE::GNU_PROPERTY_TYPE_0, type, std::move(description),
         std::move(secname)),
    arch_(arch), class_(cls)
  {}

  std::unique_ptr<Note> clone() const override {
    return std::unique_ptr<Note>(new NoteGnuProperty(*this));
  }

  /// Find the property with the given type or return a `nullptr`
  std::unique_ptr<NoteGnuProperty::Property> find(Property::TYPE type) const;

  /// Return the properties as a list of Property
  properties_t properties() const;

  void dump(std::ostream& os) const override;

  void accept(Visitor& visitor) const override;

  static bool classof(const Note* note) {
    return note->type() == Note::TYPE::GNU_PROPERTY_TYPE_0;
  }

  ~NoteGnuProperty() override = default;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const NoteGnuProperty& note) {
    note.dump(os);
    return os;
  }

  protected:
  ARCH arch_ = ARCH::NONE;
  Header::CLASS class_ = Header::CLASS::NONE;
};

LIEF_API const char* to_string(NoteGnuProperty::Property::TYPE type);

} // namepsace ELF
} // namespace LIEF

#endif
