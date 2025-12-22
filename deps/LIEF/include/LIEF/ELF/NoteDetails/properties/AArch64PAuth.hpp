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
#ifndef LIEF_ELF_NOTE_DETAILS_PROPERTIES_AARCH64_PAUTH_H
#define LIEF_ELF_NOTE_DETAILS_PROPERTIES_AARCH64_PAUTH_H

#include "LIEF/visibility.h"
#include "LIEF/ELF/NoteDetails/NoteGnuProperty.hpp"

namespace LIEF {
class BinaryStream;

namespace ELF {


/// This class represents the `GNU_PROPERTY_AARCH64_FEATURE_PAUTH` property.
///
/// \note If both: AArch64PAuth::platform and AArch64PAuth::version are set to
/// 0, this means that the binary is incompatible with PAuth ABI extension.
class LIEF_API AArch64PAuth : public NoteGnuProperty::Property {
  public:
  /// 64-bit value that specifies the platform vendor.
  ///
  /// A `0` value is associated with an *invalid* platform while the value `1`
  /// is associated with a baremetal platform.
  uint64_t platform() const {
    return platform_;
  }

  /// 64-bit value that identifies the signing schema used by the ELF file.
  uint64_t version() const {
    return version_;
  }

  static bool classof(const NoteGnuProperty::Property* prop) {
    return prop->type() == NoteGnuProperty::Property::TYPE::AARCH64_PAUTH;
  }

  static std::unique_ptr<AArch64PAuth> create(BinaryStream& stream);

  void dump(std::ostream &os) const override;

  ~AArch64PAuth() override = default;

  protected:
  AArch64PAuth() :
    NoteGnuProperty::Property(NoteGnuProperty::Property::TYPE::AARCH64_PAUTH)
  {}

  AArch64PAuth(uint64_t platform, uint64_t version) :
    NoteGnuProperty::Property(NoteGnuProperty::Property::TYPE::AARCH64_PAUTH),
    platform_(platform),
    version_(version)
  {}

  uint64_t platform_ = 0;
  uint64_t version_ = 0;
};

}
}


#endif
