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
#ifndef LIEF_ELF_NOTE_DETAILS_PROPERTIES_X86FEATURES_H
#define LIEF_ELF_NOTE_DETAILS_PROPERTIES_X86FEATURES_H
#include <vector>
#include <utility>

#include "LIEF/ELF/NoteDetails/NoteGnuProperty.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace ELF {

/// This class interfaces the different ``GNU_PROPERTY_X86_FEATURE_*``
/// properties which includes:
/// - ``GNU_PROPERTY_X86_FEATURE_1_AND``
/// - ``GNU_PROPERTY_X86_FEATURE_2_USED``
/// - ``GNU_PROPERTY_X86_FEATURE_2_NEEDED``
class LIEF_API X86Features : public NoteGnuProperty::Property {
  public:

  /// Flag according to the ``_AND``, ``_USED`` or ``_NEEDED`` suffixes
  enum class FLAG {
    NONE = 0, ///< For the original ``GNU_PROPERTY_X86_FEATURE_1_AND`` property
    USED,     ///< For the original ``GNU_PROPERTY_X86_FEATURE_2_USED`` property
    NEEDED,   ///< For the original ``GNU_PROPERTY_X86_FEATURE_2_NEEDED`` property
  };

  /// Features provided by these different properties
  enum class FEATURE {
    UNKNOWN = 0,

    IBT,
    SHSTK,
    LAM_U48,
    LAM_U57,
    X86,
    X87,
    MMX,
    XMM,
    YMM,
    ZMM,
    FXSR,
    XSAVE,
    XSAVEOPT,
    XSAVEC,
    TMM,
    MASK,
  };

  /// List of the features as a pair of FLAG, FEATURE.
  ///
  /// For instance, if the raw property is `GNU_PROPERTY_X86_FEATURE_2_USED`
  /// with a bitmask set to ``GNU_PROPERTY_X86_FEATURE_2_XSAVE``, it generates
  /// the pair: FLAG::USED, FEATURE::XSAVE
  using features_t = std::vector<std::pair<FLAG, FEATURE>>;

  static bool classof(const NoteGnuProperty::Property* prop) {
    return prop->type() == NoteGnuProperty::Property::TYPE::X86_FEATURE;
  }

  static std::unique_ptr<X86Features> create(uint32_t type, BinaryStream& stream);

  /// List of the features
  const features_t& features() const {
    return features_;
  }

  void dump(std::ostream &os) const override;

  ~X86Features() override = default;

  protected:
  inline static std::unique_ptr<X86Features> create_feat1(FLAG flag, BinaryStream& stream);
  inline static std::unique_ptr<X86Features> create_feat2(FLAG flag, BinaryStream& stream);
  X86Features(features_t values) :
    NoteGnuProperty::Property(NoteGnuProperty::Property::TYPE::X86_FEATURE),
    features_(std::move(values))
  {}

  features_t features_;
};

LIEF_API const char* to_string(X86Features::FLAG flag);
LIEF_API const char* to_string(X86Features::FEATURE feat);

}
}

#endif
