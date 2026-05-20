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

#include "LIEF/utils.hpp"
#include "LIEF/Visitor.hpp"
#include "LIEF/PE/resources/ResourceDialogRegular.hpp"

#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "logging.hpp"
#include "fmt_formatter.hpp"

FMT_FORMATTER(LIEF::PE::ResourceDialog::DIALOG_STYLES, LIEF::PE::to_string);
FMT_FORMATTER(LIEF::PE::ResourceDialog::WINDOW_EXTENDED_STYLES, LIEF::PE::to_string);
FMT_FORMATTER(LIEF::PE::ResourceDialog::WINDOW_STYLES, LIEF::PE::to_string);
FMT_FORMATTER(LIEF::PE::ResourceDialog::CONTROL_STYLES, LIEF::PE::to_string);

namespace LIEF::PE {

ok_error_t parse_font_info(ResourceDialogRegular& dialog, BinaryStream& stream) {
  if (!dialog.has(ResourceDialog::DIALOG_STYLES::SETFONT)) {
    return ok();
  }

  auto point_size = stream.read<uint16_t>();

  if (!point_size) {
    return make_error_code(point_size.error());
  }

  auto typeface = stream.read_u16string();

  if (!typeface) {
    return make_error_code(typeface.error());
  }

  dialog.font(*point_size, std::move(*typeface));

  stream.align(sizeof(uint16_t));
  return ok();
}

result<ResourceDialogRegular::Item>
  ResourceDialogRegular::Item::parse(BinaryStream& stream)
{
  stream.align(sizeof(uint32_t));

  auto style = stream.read<uint32_t>();
  if (!style) {
    return make_error_code(style.error());
  }

  auto dwExtendedStyle = stream.read<uint32_t>();
  if (!dwExtendedStyle) {
    return make_error_code(dwExtendedStyle.error());
  }

  auto x = stream.read<int16_t>();
  if (!x) {
    return make_error_code(x.error());
  }

  auto y = stream.read<int16_t>();
  if (!y) {
    return make_error_code(y.error());
  }

  auto cx = stream.read<int16_t>();
  if (!cx) {
    return make_error_code(cx.error());
  }

  auto cy = stream.read<int16_t>();
  if (!cy) {
    return make_error_code(cy.error());
  }

  auto id = stream.read<int16_t>();
  if (!id) {
    return make_error_code(cy.error());
  }

  ResourceDialogRegular::Item item;
  item
    .style(*style)
    .extended_style(*dwExtendedStyle)
    .x(*x).y(*y)
    .cx(*cx).cy(*cy)
    .id((int32_t)*id)
  ;
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

std::unique_ptr<ResourceDialogRegular>
  ResourceDialogRegular::create(BinaryStream& stream)
{
  LIEF_DEBUG("Parsing regular dialog");
  auto style = stream.read<uint32_t>();
  if (!style) {
    return nullptr;
  }

  auto dwExtendedStyle = stream.read<uint32_t>();

  if (!dwExtendedStyle) {
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

  auto dialog = std::make_unique<ResourceDialogRegular>();
  (*dialog)
    .style(*style)
    .extended_style(*dwExtendedStyle)
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

  if (auto is_ok = parse_font_info(*dialog, stream); !is_ok) {
    return dialog;
  }

  LIEF_DEBUG("Dialog nb item: #{}", *nb_items);
  for (size_t i = 0; i < *nb_items; ++i) {
    auto item = ResourceDialogRegular::Item::parse(stream);
    if (!item) {
      LIEF_INFO("Can't parse (regular) Dialog item #{:02d}", i);
      return dialog;
    }

    dialog->add_item(*item);
  }
  LIEF_DEBUG("#{} items parsed", dialog->items_.size());

  return dialog;
}

std::string ResourceDialogRegular::Item::to_string() const {
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

std::string ResourceDialogRegular::font_t::to_string() const {
  return std::to_string(point_size) + ",  " + u16tou8(name);
}

std::string ResourceDialogRegular::to_string() const {
  std::ostringstream oss;
  oss << fmt::format("DIALOG {}, {}, {}, {}\n", x(), y(), cx(), cy());
  oss << fmt::format("STYLE: {} {}\n", fmt::join(styles_list(), " | "),
                     fmt::join(windows_styles_list(), " | "));
  if (auto m = menu()) {
    oss << fmt::format("MENU: {}\n", m.to_string());
  }

  if (auto c = window_class()) {
    oss << fmt::format("WINDOWS CLASS: {}\n", c.to_string());
  }

  oss << "CAPTION: \"" << title_utf8() << "\"\n";

  if (auto f = font()) {
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

void ResourceDialogRegular::accept(Visitor& visitor) const {
  visitor.visit(*this);
}
}
