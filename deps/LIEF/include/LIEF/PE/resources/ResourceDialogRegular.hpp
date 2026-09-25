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
#ifndef LIEF_PE_RESOURCE_DIALOG_REGULAR_H
#define LIEF_PE_RESOURCE_DIALOG_REGULAR_H

#include "LIEF/visibility.h"
#include "LIEF/PE/resources/ResourceDialog.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/span.hpp"

namespace LIEF {
class BinaryStream;

namespace PE {

/// Implementation for a regular/legacy dialog box.
///
/// See: https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-dlgtemplate
class LIEF_API ResourceDialogRegular : public ResourceDialog {
  public:
  using ResourceDialog::ordinal_or_str_t;

  /// This class represents a `DLGTEMPLATE` item: `DLGITEMTEMPLATE`
  ///
  /// See: https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-dlgitemtemplate
  class LIEF_API Item : public ResourceDialog::Item {
    public:
    Item() = default;
    Item(const Item&) = default;
    Item& operator=(const Item&) = default;

    Item(Item&&) = default;
    Item& operator=(Item&&) = default;

    static result<Item> parse(BinaryStream& stream);

    std::string to_string() const override;

    ~Item() override = default;
  };

  /// This structure represents additional font information that might be
  /// embedded at the end of the DLGTEMPLATE stream
  struct LIEF_API font_t {
    uint16_t point_size = 0;
    std::u16string name;

    bool is_defined() const {
      return point_size != 0;
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

  ResourceDialogRegular() :
    ResourceDialog(ResourceDialog::TYPE::REGULAR)
  {}

  ResourceDialogRegular(const ResourceDialogRegular&) = default;
  ResourceDialogRegular& operator=(const ResourceDialogRegular&) = default;

  ResourceDialogRegular(ResourceDialogRegular&&) = default;
  ResourceDialogRegular& operator=(ResourceDialogRegular&&) = default;

  static std::unique_ptr<ResourceDialogRegular> create(BinaryStream& stream);

  std::unique_ptr<ResourceDialog> clone() const override {
    return std::unique_ptr<ResourceDialogRegular>(new ResourceDialogRegular(*this));
  }

  static bool classof(const ResourceDialog* dialog) {
    return dialog->type() == ResourceDialog::TYPE::REGULAR;
  }

  std::string to_string() const override;

  /// Number of control items
  uint32_t nb_items() const {
    return items_.size();
  }

  /// Iterator over the different control items
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

  /// Additional font information
  const font_t& font() const {
    return font_;
  }

  ResourceDialogRegular& font(uint16_t pointsize, std::u16string name) {
    font_.name = std::move(name);
    font_.point_size = pointsize;
    return *this;
  }

  ResourceDialogRegular& font(font_t f) {
    font_ = std::move(f);
    return *this;
  }

  void accept(Visitor& visitor) const override;

  ~ResourceDialogRegular() override = default;

  protected:
  font_t font_;
  items_t items_;
};

}
}


#endif
