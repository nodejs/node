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
#include "LIEF/PE/ResourceData.hpp"
#include "LIEF/PE/resources/ResourceDialog.hpp"
#include "LIEF/PE/resources/ResourceDialogRegular.hpp"
#include "LIEF/PE/resources/ResourceDialogExtended.hpp"

#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/utils.hpp"

#include "logging.hpp"
#include "frozen.hpp"
#include "fmt_formatter.hpp"

#include "PE/resources/styles_array.hpp"

FMT_FORMATTER(LIEF::PE::ResourceDialog::DIALOG_STYLES, LIEF::PE::to_string);
FMT_FORMATTER(LIEF::PE::ResourceDialog::WINDOW_EXTENDED_STYLES, LIEF::PE::to_string);
FMT_FORMATTER(LIEF::PE::ResourceDialog::WINDOW_STYLES, LIEF::PE::to_string);

namespace LIEF::PE {

static bool is_extended(BinaryStream& strm) {
  ScopedStream scope(strm);
  auto version = scope->read<uint16_t>().value_or(0);
  auto signature = scope->read<uint16_t>().value_or(0);
  return version == 1 && signature == 0xFFFF;
}

ResourceDialog::dialogs_t ResourceDialog::parse(const uint8_t* buffer, size_t size) {
  if (size == 0) {
    return {};
  }
  dialogs_t dialogs;

  SpanStream strm(buffer, size);
  LIEF_DEBUG("Parsing dialogs from stream size: 0x{:08x}", size);
  while (strm) {
    std::unique_ptr<ResourceDialog> dia;
    if (is_extended(strm)) {
      dia = ResourceDialogExtended::create(strm);
    } else {
      dia = ResourceDialogRegular::create(strm);
    }

    if (dia != nullptr) {
      dialogs.push_back(std::move(dia));
    }

    break;
  }

  LIEF_DEBUG("{} bytes left", (int64_t)size - (int64_t)strm.pos());
  return dialogs;
}

ResourceDialog::dialogs_t ResourceDialog::parse(const ResourceData& node) {
  span<const uint8_t> content = node.content();
  if (content.empty()) {
    return {};
  }

  return parse(content.data(), content.size());
}

ok_error_t ResourceDialog::parse_menu(ResourceDialog& dialog, BinaryStream& stream) {
  auto menu = stream.read<uint16_t>();
  if (!menu) {
    return make_error_code(menu.error());
  }

  switch (*menu) {
    case 0x0000:
      break;
    case 0xFFFF:
      {
        auto ordinal = stream.read<uint16_t>();
        if (!ordinal) {
          return make_error_code(ordinal.error());
        }
        dialog.menu(*ordinal);
        break;
      }

    default:
      {
        stream.decrement_pos(2); // because of the first read<uint16_t>
        auto name = stream.read_u16string();
        if (!name) {
          return make_error_code(name.error());
        }
        dialog.menu(std::move(*name));
        break;
      }
  }
  stream.align(sizeof(uint16_t));
  return ok();
}

ok_error_t ResourceDialog::parse_class(ResourceDialog& dialog, BinaryStream& stream) {
  auto clazz = stream.read<uint16_t>();
  if (!clazz) {
    return make_error_code(clazz.error());
  }

  switch (*clazz) {
    case 0x0000:
      break;
    case 0xFFFF:
      {
        auto ordinal = stream.read<uint16_t>();
        if (!ordinal) {
          return make_error_code(ordinal.error());
        }
        dialog.window_class(*ordinal);
        break;
      }
    default:
      {
        stream.decrement_pos(2);
        auto name = stream.read_u16string();
        if (!name) {
          return make_error_code(name.error());
        }
        dialog.window_class(std::move(*name));
        break;
      }
  }
  stream.align(sizeof(uint16_t));
  return ok();
}

ok_error_t ResourceDialog::parse_class(ResourceDialog::Item& item,
                                       BinaryStream& stream)
{
  auto clazz = stream.read<uint16_t>();
  if (!clazz) {
    return make_error_code(clazz.error());
  }

  switch (*clazz) {
    case 0xFFFF:
      {
        auto ordinal = stream.read<uint16_t>();
        if (!ordinal) {
          return make_error_code(ordinal.error());
        }
        item.clazz(*ordinal);
        break;
      }
    default:
      {
        stream.decrement_pos(2);
        auto name = stream.read_u16string();
        if (!name) {
          return make_error_code(name.error());
        }
        item.clazz(std::move(*name));
        break;
      }
  }
  stream.align(sizeof(uint16_t));
  return ok();
}


ok_error_t ResourceDialog::parse_title(ResourceDialog& dialog, BinaryStream& stream) {
  auto title = stream.read_u16string();

  if (!title) {
    return make_error_code(title.error());
  }

  dialog.title(std::move(*title));
  stream.align(sizeof(uint16_t));
  return ok();
}

ok_error_t ResourceDialog::parse_title(ResourceDialog::Item& item,
                                       BinaryStream& stream)
{
  auto info = stream.read<uint16_t>();
  if (!info) {
    return make_error_code(info.error());
  }

  switch (*info) {
    case 0xFFFF:
      {
        auto ordinal = stream.read<uint16_t>();
        if (!ordinal) {
          return make_error_code(ordinal.error());
        }
        item.title(*ordinal);
        break;
      }
    default:
      {
        stream.decrement_pos(2);
        auto name = stream.read_u16string();
        if (!name) {
          return make_error_code(name.error());
        }
        item.title(std::move(*name));
        break;
      }
  }
  stream.align(sizeof(uint16_t));
  return ok();
}

ok_error_t ResourceDialog::parse_creation_data(
  ResourceDialog::Item& item, BinaryStream& stream)
{
  auto size = stream.read<uint16_t>();
  if (!size) {
    return make_error_code(size.error());
  }
  std::vector<uint8_t> raw;
  auto is_ok = stream.read_data(raw, *size);

  item.data(std::move(raw));
  return is_ok;
}

std::vector<ResourceDialog::DIALOG_STYLES> ResourceDialog::styles_list() const {
  std::vector<DIALOG_STYLES> flags;
  std::copy_if(std::begin(DIALOG_STYLES_ARRAY), std::end(DIALOG_STYLES_ARRAY),
               std::back_inserter(flags),
               [this] (DIALOG_STYLES f) { return has(f); });
  return flags;
}

std::vector<ResourceDialog::WINDOW_STYLES> ResourceDialog::windows_styles_list() const {
  std::vector<WINDOW_STYLES> flags;
  std::copy_if(std::begin(WINDOW_STYLES_ARRAY), std::end(WINDOW_STYLES_ARRAY),
               std::back_inserter(flags),
               [this] (WINDOW_STYLES f) { return has(f); });
  return flags;
}

std::vector<ResourceDialog::WINDOW_EXTENDED_STYLES> ResourceDialog::windows_ext_styles_list() const {
  std::vector<WINDOW_EXTENDED_STYLES> flags;
  std::copy_if(std::begin(WINDOW_EXTENDED_STYLES_ARRAY), std::end(WINDOW_EXTENDED_STYLES_ARRAY),
               std::back_inserter(flags),
               [this] (WINDOW_EXTENDED_STYLES f) { return has(f); });
  return flags;
}


std::string ResourceDialog::title_utf8() const {
  return u16tou8(title_);
}

std::string ResourceDialog::ordinal_or_str_t::to_string() const {
  if (ordinal) {
    return fmt::format("ord={}", *ordinal);
  }
  return u16tou8(string);
}

std::vector<ResourceDialog::WINDOW_STYLES>
  ResourceDialog::Item::window_styles() const {
  std::vector<WINDOW_STYLES> flags;
  std::copy_if(std::begin(WINDOW_STYLES_ARRAY), std::end(WINDOW_STYLES_ARRAY),
               std::back_inserter(flags),
               [this] (WINDOW_STYLES f) { return has(f); });
  return flags;
}

std::vector<ResourceDialog::CONTROL_STYLES>
  ResourceDialog::Item::control_styles() const
{
  std::vector<CONTROL_STYLES> flags;
  std::copy_if(std::begin(CONTROL_STYLES_ARRAY), std::end(CONTROL_STYLES_ARRAY),
               std::back_inserter(flags),
               [this] (CONTROL_STYLES f) { return has(f); });
  return flags;
}

ResourceDialog& ResourceDialog::title(const std::string& title) {
  if (auto u16 = u8tou16(title)) {
    return this->title(std::move(*u16));
  }
  return *this;
}


const char* to_string(ResourceDialog::DIALOG_STYLES e) {
  #define ENTRY(X) std::pair(ResourceDialog::DIALOG_STYLES::X, #X)
  STRING_MAP enums2str {
    ENTRY(ABSALIGN),
    ENTRY(SYSMODAL),
    ENTRY(LOCALEDIT),
    ENTRY(SETFONT),
    ENTRY(MODALFRAME),
    ENTRY(NOIDLEMSG),
    ENTRY(SETFOREGROUND),
    ENTRY(S3DLOOK),
    ENTRY(FIXEDSYS),
    ENTRY(NOFAILCREATE),
    ENTRY(CONTROL),
    ENTRY(CENTER),
    ENTRY(CENTERMOUSE),
    ENTRY(CONTEXTHELP),
    ENTRY(SHELLFONT),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(ResourceDialog::WINDOW_STYLES e) {
  #define ENTRY(X) std::pair(ResourceDialog::WINDOW_STYLES::X, #X)
  STRING_MAP enums2str {
    ENTRY(OVERLAPPED),
    ENTRY(POPUP),
    ENTRY(CHILD),
    ENTRY(MINIMIZE),
    ENTRY(VISIBLE),
    ENTRY(DISABLED),
    ENTRY(CLIPSIBLINGS),
    ENTRY(CLIPCHILDREN),
    ENTRY(MAXIMIZE),
    ENTRY(CAPTION),
    ENTRY(BORDER),
    ENTRY(DLGFRAME),
    ENTRY(VSCROLL),
    ENTRY(HSCROLL),
    ENTRY(SYSMENU),
    ENTRY(THICKFRAME),
    ENTRY(GROUP),
    ENTRY(TABSTOP),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}


const char* to_string(ResourceDialog::WINDOW_EXTENDED_STYLES e) {
  #define ENTRY(X) std::pair(ResourceDialog::WINDOW_EXTENDED_STYLES::X, #X)
  STRING_MAP enums2str {
    ENTRY(DLGMODALFRAME),
    ENTRY(NOPARENTNOTIFY),
    ENTRY(TOPMOST),
    ENTRY(ACCEPTFILES),
    ENTRY(TRANSPARENT_STY),
    ENTRY(MDICHILD),
    ENTRY(TOOLWINDOW),
    ENTRY(WINDOWEDGE),
    ENTRY(CLIENTEDGE),
    ENTRY(CONTEXTHELP),
    ENTRY(RIGHT),
    ENTRY(LEFT),
    ENTRY(RTLREADING),
    ENTRY(LEFTSCROLLBAR),
    ENTRY(CONTROLPARENT),
    ENTRY(STATICEDGE),
    ENTRY(APPWINDOW),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(ResourceDialog::CONTROL_STYLES e) {
  #define ENTRY(X) std::pair(ResourceDialog::CONTROL_STYLES::X, #X)
  STRING_MAP enums2str {
    ENTRY(TOP),
    ENTRY(NOMOVEY),
    ENTRY(BOTTOM),
    ENTRY(NORESIZE),
    ENTRY(NOPARENTALIGN),
    ENTRY(ADJUSTABLE),
    ENTRY(NODIVIDER),
    ENTRY(VERT),
    ENTRY(LEFT),
    ENTRY(RIGHT),
    ENTRY(NOMOVEX),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}

