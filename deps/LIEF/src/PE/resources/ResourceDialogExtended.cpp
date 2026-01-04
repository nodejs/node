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
#include <sstream>

#include "LIEF/Visitor.hpp"
#include "LIEF/utils.hpp"
#include "LIEF/PE/resources/ResourceDialogExtended.hpp"

#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "logging.hpp"
#include "fmt_formatter.hpp"

FMT_FORMATTER(LIEF::PE::ResourceDialog::DIALOG_STYLES, LIEF::PE::to_string);
FMT_FORMATTER(LIEF::PE::ResourceDialog::WINDOW_EXTENDED_STYLES, LIEF::PE::to_string);
FMT_FORMATTER(LIEF::PE::ResourceDialog::WINDOW_STYLES, LIEF::PE::to_string);
FMT_FORMATTER(LIEF::PE::ResourceDialog::CONTROL_STYLES, LIEF::PE::to_string);

namespace LIEF::PE {

result<ResourceDialogExtended::Item> ResourceDialogExtended::Item::parse(BinaryStream& stream) {
  // typedef struct {
  //   DWORD     helpID;
  //   DWORD     exStyle;
  //   DWORD     style;
  //   short     x;
  //   short     y;
  //   short     cx;
  //   short     cy;
  //   DWORD     id;
  //   sz_Or_Ord windowClass;
  //   sz_Or_Ord title;
  //   WORD      extraCount;
  // } DLGITEMTEMPLATEEX;
  stream.align(sizeof(uint32_t));

  auto helpID = stream.read<uint32_t>();
  if (!helpID) {
    return make_error_code(helpID.error());
  }

  auto exStyle = stream.read<uint32_t>();
  if (!exStyle) {
    return make_error_code(exStyle.error());
  }

  auto style = stream.read<uint32_t>();
  if (!style) {
    return make_error_code(style.error());
  }

  auto x = stream.read<uint16_t>();
  if (!x) {
    return make_error_code(x.error());
  }

  auto y = stream.read<uint16_t>();
  if (!y) {
    return make_error_code(y.error());
  }

  auto cx = stream.read<uint16_t>();
  if (!cx) {
    return make_error_code(cx.error());
  }

  auto cy = stream.read<uint16_t>();
  if (!cy) {
    return make_error_code(cy.error());
  }

  auto id = stream.read<int32_t>();
  if (!id) {
    return make_error_code(id.error());
  }

  ResourceDialogExtended::Item item;
  item
    .help_id(*helpID)
    .style(*style)
    .extended_style(*exStyle)
    .x(*x).y(*y)
    .cx(*cx).cy(*cy)
    .id(*id);

  stream.align(sizeof(uint16_t));

  if (auto is_ok = parse_class(item, stream); !is_ok) {
    return item;
  }

  if (auto is_ok = parse_title(item, stream); !is_ok) {
    return item;
  }

  if (auto is_ok = parse_creation_data(item, stream); !is_ok) {
    return item;
  }

  return item;
}


ok_error_t parse_typeface(ResourceDialogExtended& dialog, BinaryStream& stream) {
  if (!dialog.has(ResourceDialog::DIALOG_STYLES::SETFONT) &&
      !dialog.has(ResourceDialog::DIALOG_STYLES::SHELLFONT))
  {
    return ok();
  }
  stream.align(sizeof(uint16_t));

  auto point_size = stream.read<uint16_t>();
  if (!point_size) {
    return make_error_code(point_size.error());
  }

  auto weight = stream.read<uint16_t>();
  if (!weight) {
    return make_error_code(weight.error());
  }

  auto italic = stream.read<uint8_t>();
  if (!italic) {
    return make_error_code(italic.error());
  }

  auto charset = stream.read<uint8_t>();
  if (!charset) {
    return make_error_code(charset.error());
  }

  auto typeface = stream.read_u16string();
  if (!typeface) {
    return make_error_code(typeface.error());
  }

  dialog.font(*point_size, *weight, (bool)*italic,
              *charset, std::move(*typeface));

  stream.align(sizeof(uint16_t));
  return ok();
}


std::unique_ptr<ResourceDialogExtended>
  ResourceDialogExtended::create(BinaryStream& stream)
{
  auto version = stream.read<uint16_t>();
  if (!version) {
    return nullptr;
  }

  auto signature = stream.read<uint16_t>();

  if (!signature || *signature != 0xFFFF) {
    return nullptr;
  }

  auto help_id = stream.read<uint32_t>();
  if (!help_id) {
    return nullptr;
  }

  auto ext_style = stream.read<uint32_t>();
  if (!ext_style) {
    return nullptr;
  }

  auto style = stream.read<uint32_t>();
  if (!style) {
    return nullptr;
  }

  auto nb_items = stream.read<uint16_t>();
  if (!nb_items) {
    return nullptr;
  }

  auto x = stream.read<int16_t>();
  if (!x) {
    return nullptr;
  }

  auto y = stream.read<int16_t>();
  if (!y) {
    return nullptr;
  }

  auto cx = stream.read<int16_t>();
  if (!cx) {
    return nullptr;
  }

  auto cy = stream.read<int16_t>();
  if (!cy) {
    return nullptr;
  }

  auto dialog = std::make_unique<ResourceDialogExtended>();
  (*dialog)
    .version(*version)
    .signature(*signature)
    .help_id(*help_id)
    .extended_style(*ext_style)
    .style(*style)
    .x(*x).y(*y)
    .cx(*cx).cy(*cy)
  ;

  if (auto is_ok = parse_menu(*dialog, stream); !is_ok) {
    return dialog;
  }

  if (auto is_ok = parse_class(*dialog, stream); !is_ok) {
    return dialog;
  }

  if (auto is_ok = parse_title(*dialog, stream); !is_ok) {
    return dialog;
  }

  if (auto is_ok = parse_typeface(*dialog, stream); !is_ok) {
    return dialog;
  }

  LIEF_DEBUG("Dialog nb item: #{}", *nb_items);
  for (size_t i = 0; i < *nb_items; ++i) {
    auto item = ResourceDialogExtended::Item::parse(stream);
    if (!item) {
      LIEF_INFO("Can't parse (regular) Dialog item #{:02d}", i);
      return dialog;
    }
    dialog->add_item(*item);
  }

  LIEF_DEBUG("#{} items parsed", dialog->items_.size());

  return dialog;
}

std::string ResourceDialogExtended::font_t::to_string() const {
  return fmt::format("{}, {}", point_size, u16tou8(typeface));
}

std::string ResourceDialogExtended::Item::to_string() const {
  std::string win_class_str;
  {
    if (auto ord = clazz().ordinal) {
      switch ((WINDOW_CLASS)*ord) {
        case WINDOW_CLASS::BUTTON:
          win_class_str = "Button";
          break;
        case WINDOW_CLASS::EDIT:
          win_class_str = "Edit";
          break;
        case WINDOW_CLASS::STATIC:
          win_class_str = "Static";
          break;
        case WINDOW_CLASS::LIST_BOX:
          win_class_str = "List box";
          break;
        case WINDOW_CLASS::SCROLL_BAR:
          win_class_str = "Scroll bar";
          break;
        case WINDOW_CLASS::COMBO_BOX:
          win_class_str = "Combo box";
          break;
        default:
          win_class_str = fmt::format("unknown (0x{:04x})", *ord);
          break;
      }
    } else {
      win_class_str = u16tou8(clazz().string);
    }
  }

  return fmt::format("CONTROL '{}', {}, {}, {} {}, {}, {}, {}, {}",
                     title().to_string(), id(), win_class_str,
                     fmt::join(control_styles(), " | "),
                     fmt::join(window_styles(), " | "),
                     x(), y(), cx(), cy());
}

std::string ResourceDialogExtended::to_string() const {
  std::ostringstream oss;
  oss << fmt::format("DIALOGEX {}, {}, {}, {}\n", x(), y(), cx(), cy());

  oss << fmt::format("STYLE: {} {}\n", fmt::join(styles_list(), " | "),
                     fmt::join(windows_styles_list(), " | "));
  if (auto m = menu()) {
    oss << fmt::format("MENU: {}\n", m.to_string());
  }

  if (auto c = window_class()) {
    oss << fmt::format("WINDOWS CLASS: {}\n", c.to_string());
  }

  oss << "CAPTION: \"" << title_utf8() << "\"\n";

  if (const font_t& f = font()) {
    oss << "FONT: " << f.to_string() << '\n';
  }

  if (items_.empty()) {
    return oss.str();
  }

  oss << "{\n";
  for (const Item& item : items()) {
    oss << "  " << item << '\n';
  }
  oss << "}\n";

  return oss.str();
}

void ResourceDialogExtended::accept(Visitor& visitor) const {
  visitor.visit(*this);
}
}
