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
#ifndef LIEF_ELF_NOTE_DETAILS_PROPERTIES_NO_COP_H
#define LIEF_ELF_NOTE_DETAILS_PROPERTIES_NO_COP_H

#include "LIEF/ELF/NoteDetails/NoteGnuProperty.hpp"

namespace LIEF {
namespace ELF {

/// This class provides an interface over the `GNU_PROPERTY_NO_COPY_ON_PROTECTED`
/// property. This property indicates that the linker shouldn't copy relocations
/// against protected symbols.
class NoteNoCopyOnProtected : public NoteGnuProperty::Property {
  public:
  static bool classof(const NoteGnuProperty::Property* prop) {
    return prop->type() == NoteGnuProperty::Property::TYPE::NO_COPY_ON_PROTECTED;
  }

  static std::unique_ptr<NoteNoCopyOnProtected> create() {
    return std::unique_ptr<NoteNoCopyOnProtected>(new NoteNoCopyOnProtected());
  }

  ~NoteNoCopyOnProtected() override = default;

  protected:
  NoteNoCopyOnProtected() :
    NoteGnuProperty::Property(NoteGnuProperty::Property::TYPE::NO_COPY_ON_PROTECTED)
  {}
};
}
}

#endif
