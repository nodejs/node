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
#ifndef LIEF_ELF_NOTE_DETAILS_PROPERTIES_AARCH64_FEATURE_H
#define LIEF_ELF_NOTE_DETAILS_PROPERTIES_AARCH64_FEATURE_H

#include "LIEF/visibility.h"
#include "LIEF/ELF/NoteDetails/NoteGnuProperty.hpp"

namespace LIEF {
class BinaryStream;

namespace ELF {

/// This class represents the `GNU_PROPERTY_AARCH64_FEATURE_1_AND` property.
class LIEF_API AArch64Feature : public NoteGnuProperty::Property {
  public:
  enum class FEATURE {
    UNKNOWN = 0,
    BTI, ///< Support Branch Target Identification (BTI)
    PAC, ///< Support Pointer authentication (PAC)
  };

  /// Return the list of the supported features
  const std::vector<FEATURE>& features() const {
    return features_;
  }

  static bool classof(const NoteGnuProperty::Property* prop) {
    return prop->type() == NoteGnuProperty::Property::TYPE::AARCH64_FEATURES;
  }

  static std::unique_ptr<AArch64Feature> create(BinaryStream& stream);

  void dump(std::ostream &os) const override;

  ~AArch64Feature() override = default;

  protected:
  AArch64Feature() :
    NoteGnuProperty::Property(NoteGnuProperty::Property::TYPE::AARCH64_FEATURES)
  {}

  AArch64Feature(std::vector<FEATURE> features) :
    NoteGnuProperty::Property(NoteGnuProperty::Property::TYPE::AARCH64_FEATURES),
    features_(std::move(features))
  {}

  std::vector<FEATURE> features_;
};


LIEF_API const char* to_string(AArch64Feature::FEATURE feature);
}
}


#endif
