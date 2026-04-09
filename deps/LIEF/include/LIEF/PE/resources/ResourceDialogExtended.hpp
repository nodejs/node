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
#ifndef LIEF_PE_RESOURCE_DIALOG_EXTENDED_H
#define LIEF_PE_RESOURCE_DIALOG_EXTENDED_H

#include "LIEF/visibility.h"
#include "LIEF/PE/resources/ResourceDialog.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/iterators.hpp"

namespace LIEF {
class BinaryStream;

namespace PE {

/// Implementation for the new extended dialogbox format.
///
/// See: https://learn.microsoft.com/en-us/windows/win32/dlgbox/dlgtemplateex
class LIEF_API ResourceDialogExtended : public ResourceDialog {
  public:

  /// This class represents a `DLGTEMPLATEEX` item (`DLGITEMTEMPLATEEX`).
  ///
  /// See: https://learn.microsoft.com/en-us/windows/win32/dlgbox/dlgitemtemplateex
  class LIEF_API Item : public ResourceDialog::Item {
    public:
    Item() = default;
    Item(const Item&) = default;
    Item& operator=(const Item&) = default;

    Item(Item&&) = default;
    Item& operator=(Item&&) = default;

    static result<Item> parse(BinaryStream& stream);

    std::string to_string() const override;

    /// The help context identifier for the control. When the system sends a
    /// `WM_HELP` message, it passes the `helpID` value in the `dwContextId`
    /// member of the `HELPINFO` structure.
    uint32_t help_id() const {
      return help_id_;
    }

    Item& help_id(uint32_t value) {
      help_id_ = value;
      return *this;
    }

    ~Item() override = default;

    protected:
    uint32_t help_id_ = 0;
  };

  /// Font information for the font to use for the text in the dialog box and
  /// its controls
  struct LIEF_API font_t {
    /// The point size of the font
    uint16_t point_size = 0;

    /// The weight of the font
    uint16_t weight = 0;

    /// Indicates whether the font is italic
    bool italic = false;

    /// The character set to be used
    uint8_t charset = false;

    /// The name of the typeface for the font.
    std::u16string typeface;

    bool is_defined() const {
      return point_size != 0 || weight != 0;
    }

    operator bool() const {
      return is_defined();
    }

    std::string to_string() const;

    LIEF_API friend std::ostream& operator<<(std::ostream& os, const font_t& font) {
      os << font.to_string();
      return os;
    }
  };

  using items_t = std::vector<Item>;
  using it_items = ref_iterator<items_t&>;
  using it_const_items = const_ref_iterator<const items_t&>;

  ResourceDialogExtended() :
    ResourceDialog(ResourceDialog::TYPE::EXTENDED)
  {}

  ResourceDialogExtended(const ResourceDialogExtended&) = default;
  ResourceDialogExtended& operator=(const ResourceDialogExtended&) = default;

  ResourceDialogExtended(ResourceDialogExtended&&) = default;
  ResourceDialogExtended& operator=(ResourceDialogExtended&&) = default;

  static std::unique_ptr<ResourceDialogExtended> create(BinaryStream& stream);

  std::unique_ptr<ResourceDialog> clone() const override {
    return std::unique_ptr<ResourceDialogExtended>(new ResourceDialogExtended(*this));
  }

  static bool classof(const ResourceDialog* dialog) {
    return dialog->type() == ResourceDialog::TYPE::EXTENDED;
  }

  void accept(Visitor& visitor) const override;

  std::string to_string() const override;

  ~ResourceDialogExtended() override = default;

  /// The version number of the extended dialog box template. This member must
  /// be set to 1.
  uint16_t version() const {
    return version_;
  }

  /// Indicates whether a template is an extended dialog box template.
  /// If signature is 0xFFFF, this is an extended dialog box template.
  /// In this case, the dlgVer member specifies the template version number.
  uint16_t signature() const {
    return signature_;
  }

  /// The help context identifier for the dialog box window. When the system
  /// sends a `WM_HELP` message, it passes the `helpID` value in the `dwContextId`
  /// member of the `HELPINFO` structure.
  uint32_t help_id() const {
    return help_id_;
  }

  /// Iterator over the control items of this dialog box
  it_items items() {
    return items_;
  }

  it_const_items items() const {
    return items_;
  }

  /// Add a new control item to the dialog
  void add_item(const Item& item) {
    items_.push_back(item);
  }

  ResourceDialogExtended& version(uint16_t value) {
    version_ = value;
    return *this;
  }

  ResourceDialogExtended& signature(uint16_t value) {
    signature_ = value;
    return *this;
  }

  ResourceDialogExtended& help_id(uint32_t value) {
    help_id_ = value;
    return *this;
  }

  /// Additional font information.
  const font_t& font() const {
    return font_;
  }

  ResourceDialogExtended& font(uint16_t point_size, uint16_t weight, bool italic,
                               uint8_t charset, std::u16string typeface)
  {
    font_.point_size = point_size;
    font_.weight = weight;
    font_.italic = italic;
    font_.charset = charset;
    font_.typeface = std::move(typeface);
    return *this;
  }

  ResourceDialogExtended& font(font_t f) {
    font_ = std::move(f);
    return *this;
  }

  protected:
  uint16_t version_ = 0;
  uint16_t signature_ = 0;
  uint32_t help_id_ = 0;
  font_t font_;
  items_t items_;
};

}
}


#endif
