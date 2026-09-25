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
#include "LIEF/Visitor.hpp"
#include "LIEF/utils.hpp"

#include "LIEF/PE/resources/langs.hpp"
#include "LIEF/PE/ResourcesManager.hpp"
#include "LIEF/PE/ResourceNode.hpp"
#include "LIEF/PE/ResourceData.hpp"
#include "LIEF/PE/ResourceDirectory.hpp"

#include "PE/Structures.hpp"

#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/iostream.hpp"

#include "frozen.hpp"
#include "logging.hpp"
#include "internal_utils.hpp"
#include "fmt_formatter.hpp"

FMT_FORMATTER(LIEF::PE::ResourcesManager::TYPE, LIEF::PE::to_string);

namespace LIEF {
namespace PE {

static constexpr auto RESOURCE_TYPES = {
  ResourcesManager::TYPE::CURSOR,         ResourcesManager::TYPE::BITMAP,
  ResourcesManager::TYPE::ICON,           ResourcesManager::TYPE::MENU,
  ResourcesManager::TYPE::DIALOG,         ResourcesManager::TYPE::STRING,
  ResourcesManager::TYPE::FONTDIR,        ResourcesManager::TYPE::FONT,
  ResourcesManager::TYPE::ACCELERATOR,    ResourcesManager::TYPE::RCDATA,
  ResourcesManager::TYPE::MESSAGETABLE,   ResourcesManager::TYPE::GROUP_CURSOR,
  ResourcesManager::TYPE::GROUP_ICON,     ResourcesManager::TYPE::VERSION,
  ResourcesManager::TYPE::DLGINCLUDE,     ResourcesManager::TYPE::PLUGPLAY,
  ResourcesManager::TYPE::VXD,            ResourcesManager::TYPE::ANICURSOR,
  ResourcesManager::TYPE::ANIICON,        ResourcesManager::TYPE::HTML,
  ResourcesManager::TYPE::MANIFEST,
};

std::string ResourcesManager::string_entry_t::string_u8() const {
  return u16tou8(string);
}

const ResourceNode* ResourcesManager::get_node_type(ResourcesManager::TYPE type) const {
  ResourceNode::it_childs nodes = resources_->childs();
  const auto it_node = std::find_if(std::begin(nodes), std::end(nodes),
      [type] (const ResourceNode& node) {
        return TYPE(node.id()) == type;
      });

  if (it_node == std::end(nodes)) {
    return nullptr;
  }

  return &*it_node;
}

std::vector<ResourcesManager::TYPE> ResourcesManager::get_types() const {
  std::vector<TYPE> types;
  for (const ResourceNode& node : resources_->childs()) {
    const auto it = std::find_if(std::begin(RESOURCE_TYPES), std::end(RESOURCE_TYPES),
        [&node] (TYPE t) {
          return t == TYPE(node.id());
        });

    if (it != std::end(RESOURCE_TYPES)) {
      types.push_back(*it);
    }
  }
  return types;
}

std::string ResourcesManager::manifest() const {
  const ResourceNode* root_node = get_node_type(TYPE::MANIFEST);
  if (root_node == nullptr) {
    return "";
  }

  const auto* data_node =
    root_node->safe_get_at(0).safe_get_at(0).cast<ResourceData>();

  if (data_node == nullptr) {
    LIEF_WARN("Manifest node seems corrupted");
    return "";
  }

  span<const uint8_t> content = data_node->content();
  return std::string{std::begin(content), std::end(content)};
}

void ResourcesManager::manifest(const std::string& manifest) {
  ResourceNode* manifest_node = get_node_type(TYPE::MANIFEST);
  if (manifest_node == nullptr) {
    auto L1 = std::make_unique<ResourceDirectory>((int)TYPE::MANIFEST);
    auto L2 = std::make_unique<ResourceDirectory>(1);
    auto L3 = std::make_unique<ResourceData>(manifest);

    (*resources_)
      .add_child(std::move(L1))
      .add_child(std::move(L2))
      .add_child(std::move(L3));

    manifest_node = get_node_type(TYPE::MANIFEST);
  }

  if (manifest_node == nullptr) {
    LIEF_WARN("Manifest node seems corrupted");
    return;
  }

  auto* data_node =
    manifest_node->safe_get_at(0).safe_get_at(0).cast<ResourceData>();

  if (data_node == nullptr) {
    LIEF_WARN("Manifest node seems corrupted");
    return;
  }

  data_node->content(manifest);
  return;
}

std::vector<ResourceVersion> ResourcesManager::version() const {
  std::vector<ResourceVersion> result;
  const ResourceNode* root_node = get_node_type(TYPE::VERSION);
  if (root_node == nullptr) {
    return result;
  }
  for (const ResourceNode& child_l1 : root_node->childs()) {
    if (!child_l1.is_directory()) {
      continue;
    }
    for (const ResourceNode& child_l3 : child_l1.childs()) {
      const auto* data_node = child_l3.cast<ResourceData>();
      if (data_node == nullptr) {
        continue;
      }

      if (auto info = ResourceVersion::parse(*data_node)) {
        result.push_back(std::move(*info));
      }
    }
  }
  return result;
}

ResourcesManager::it_const_icons ResourcesManager::icons() const {
  std::vector<ResourceIcon> icons;
  const ResourceNode* root_icon     = get_node_type(TYPE::ICON);
  const ResourceNode* root_grp_icon = get_node_type(TYPE::GROUP_ICON);
  if (root_icon == nullptr) {
    LIEF_ERR("Missing '{}' entry", to_string(TYPE::ICON));
    return icons;
  }

  if (root_grp_icon == nullptr) {
    LIEF_ERR("Missing '{}' entry", to_string(TYPE::GROUP_ICON));
    return icons;
  }

  for (const ResourceNode& grp_icon_lvl2 : root_grp_icon->childs()) {
    for (const ResourceNode& grp_icon_lvl3 : grp_icon_lvl2.childs()) {
      const auto* icon_group_node = grp_icon_lvl3.cast<ResourceData>();
      if (icon_group_node == nullptr) {
        LIEF_WARN("Expecting a data node for node id: {}", grp_icon_lvl3.id());
        continue;
      }
      const uint32_t id = icon_group_node->id();

      span<const uint8_t> icon_group_content = icon_group_node->content();

      if (icon_group_content.empty()) {
        LIEF_INFO("Group icon is empty");
        continue;
      }

      SpanStream stream(icon_group_content);
      details::pe_resource_icon_dir group_icon_header;
      if (auto res = stream.read<details::pe_resource_icon_dir>()) {
        group_icon_header = *res;
      } else {
        LIEF_WARN("Can't read GRPICONDIR for resource node id: {}", id);
        continue;
      }

      LIEF_DEBUG("GRPICONDIR.idType:  {}", group_icon_header.type);
      LIEF_DEBUG("GRPICONDIR.idCount: {}", group_icon_header.count);

      // Some checks
      if (group_icon_header.type != 1) {
        LIEF_ERR("Group icon type should be equal to 1 (vs {})", group_icon_header.type);
        return icons;
      }

      for (size_t i = 0; i < group_icon_header.count; ++i) {
        details::pe_resource_icon_group entry;
        if (auto res = stream.read<details::pe_resource_icon_group>()) {
          entry = *res;
        } else {
          LIEF_WARN("Can't read GRPICONDIR.idEntries[{}]", i);
          break;
        }

        ResourceIcon icon = entry;
        icon.lang_ = lang_from_id(grp_icon_lvl3.id());
        icon.sublang_ = sublang_from_id(grp_icon_lvl3.id());

        // Find the icon the RESOURCE_TYPES::ICON tree that matched entry.ID
        ResourceNode::it_const_childs sub_nodes_icons = root_icon->childs();
        const auto it = std::find_if(std::begin(sub_nodes_icons), std::end(sub_nodes_icons),
            [&entry] (const ResourceNode& node) {
              return node.id() == entry.ID;
            });
        if (it == std::end(sub_nodes_icons)) {
          LIEF_WARN("Unable to find the icon associated with id: {:d}", entry.ID);
          continue;
        }

        ResourceNode::it_childs icons_childs = it->childs();
        if (icons_childs.empty()) {
          LIEF_WARN("Resources nodes looks corrupted");
          continue;
        }
        const ResourceNode& icon_node = icons_childs[0];
        if (!icon_node.is_data()) {
          LIEF_WARN("Expecting a Data node for node id: {}", icon_node.id());
          continue;
        }
        span<const uint8_t> pixels = static_cast<const ResourceData&>(icon_node).content();
        icon.pixels_ = std::vector<uint8_t>(std::begin(pixels), std::end(pixels));
        icons.push_back(std::move(icon));
      }
    }
  }

  return icons;
}


void ResourcesManager::add_icon(const ResourceIcon& icon) {
  // Root
  // |
  // +--+--- DIR/ICON (ID: 3)
  // |  |
  // |  +---+-- DIR / ID: 1
  // |      |
  // |      +------ DATA / ID: 1033 (LANG) ---> Pixels   <-----------+
  // .                                                               |
  // .                                                               |
  // |                                                               |
  // +---+-- DIR/ICON_GROUP                                          |
  // |  |                                                            |
  // |  +---+-- DIR / ID: 1                                          |
  // |      |                                                        |
  // |      +------ DATA / ID: 1033 (LANG) ---> pe_resource_icon_group

  ResourceNode* icon_node = this->get_node_type(TYPE::ICON);
  ResourceNode* icon_grp_node = this->get_node_type(TYPE::GROUP_ICON);

  if (icon_node == nullptr) {
    LIEF_DEBUG("Missing 'ICON' node: creating");
    auto icon_dir = std::make_unique<ResourceDirectory>((int)TYPE::ICON);
    auto icon_id = std::make_unique<ResourceDirectory>(1);
    icon_dir->add_child(std::move(icon_id));
    icon_node = &resources_->add_child(std::move(icon_dir));
  }

  if (icon_grp_node == nullptr) {
    LIEF_DEBUG("Missing 'GROUP_ICON' node: creating");
    vector_iostream ios;
    ios.reserve(sizeof(details::pe_resource_icon_dir) +
                sizeof(details::pe_icon_header));

    ios
      // pe_resource_icon_dir
      .write<uint16_t>(/*reserved*/0)
      .write<uint16_t>(/*type*/1)
      .write<uint16_t>(/*count*/0)
    ;
    std::vector<uint8_t> raw_header;
    ios.move(raw_header);

    auto icon_grp_dir = std::make_unique<ResourceDirectory>((int)TYPE::GROUP_ICON);
    auto group = std::make_unique<ResourceDirectory>(0);
    auto header_node = std::make_unique<ResourceData>(std::move(raw_header));
    group->add_child(std::move(header_node));
    icon_grp_dir->add_child(*group);
    icon_grp_node = &resources_->add_child(std::move(icon_grp_dir));
  }

  auto new_id = static_cast<uint16_t>(icon.id());

  if ((int32_t)icon.id() < 0) {
    new_id = icon_node->childs().size() + 1;
  }

  auto* icon_group_data =
    icon_grp_node->safe_get_at(0).safe_get_at(0).cast<ResourceData>();

  auto* icon_dir =
    icon_node->safe_get_at(0).cast<ResourceDirectory>();

  if (icon_group_data == nullptr) {
    LIEF_ERR("Can't find data node for the icon group headers");
    return;
  }

  if (icon_dir == nullptr) {
    LIEF_ERR("Can't find data node for the icon headers");
    return;
  }

  vector_iostream icon_group = icon_group_data->edit();

  if (icon_group.size() < /* reserved + type + count */3 * sizeof(uint16_t)) {
    LIEF_ERR("Icon group header too small");
    return;
  }

  ++icon_group.edit_as<details::pe_resource_icon_dir>()->count;
  icon_group
    .seek_end()
    .write<uint8_t>(icon.width())
    .write<uint8_t>(icon.height())
    .write<uint8_t>(icon.color_count())
    .write<uint8_t>(icon.reserved())
    .write<uint16_t>(icon.planes())
    .write<uint16_t>(icon.bit_count())
    .write<uint16_t>(icon.size())
    .write<uint16_t>(new_id)
  ;

  auto pixel_node = std::make_unique<ResourceData>(as_vector(icon.pixels()));
  pixel_node->id(static_cast<int>(icon.sublang()) << 10 | static_cast<int>(icon.lang()));
  icon_dir->add_child(std::move(pixel_node));
}


void ResourcesManager::change_icon(const ResourceIcon& original, const ResourceIcon& newone) {
  ResourceNode* icon_node = get_node_type(TYPE::ICON);
  ResourceNode* icon_grp_node = get_node_type(TYPE::GROUP_ICON);

  if (icon_node == nullptr) {
    LIEF_ERR("Missing 'ICON' node");
    return;
  }

  if (icon_grp_node == nullptr) {
    LIEF_ERR("Missing 'GROUP_ICON' node");
    return;
  }

  // Update group in which the icon is registred
  for (ResourceNode& grp_icon_lvl2 : icon_grp_node->childs()) {
    for (ResourceNode& grp_icon_lvl3 : grp_icon_lvl2.childs()) {
      auto* icon_group_node = grp_icon_lvl3.cast<ResourceData>();

      if (icon_group_node == nullptr) {
        LIEF_WARN("Resource group icon corrupted");
        continue;
      }

      vector_iostream editor = icon_group_node->edit();
      SpanStream stream = icon_group_node->content();

      /* reserved */stream.read<uint16_t>().value_or(0);
      /* type */ stream.read<uint16_t>().value_or(0);
      size_t count = stream.read<uint16_t>().value_or(0);

      for (size_t i = 0; i < count; ++i) {
        const size_t pos = stream.pos();
        auto res = stream.read<details::pe_resource_icon_group>();
        if (!res) {
          LIEF_DEBUG("Icon #{} is corrupted");
          return;
        }
        if (res->ID != original.id()) {
          continue;
        }

        auto* icon_header = editor.edit_as<details::pe_resource_icon_group>(pos);
        icon_header->width = newone.width();
        icon_header->height = newone.height();
        icon_header->color_count = newone.color_count();
        icon_header->reserved = newone.reserved();
        icon_header->planes = newone.planes();
        icon_header->bit_count = newone.bit_count();
        icon_header->size = newone.size();
      }
    }
  }

  // Then the header
  icon_node->delete_child(original.id());

  ResourceDirectory new_icon_dir_node(newone.id());
  ResourceData new_icon_data_node(as_vector(newone.pixels()));
  new_icon_data_node.id((int)encode_lang(newone.lang(), newone.sublang()));
  new_icon_dir_node.add_child(std::move(new_icon_data_node));
  icon_node->add_child(std::move(new_icon_dir_node));
}

ResourcesManager::it_const_dialogs ResourcesManager::dialogs() const {
  dialogs_.clear();
  const ResourceNode* dialog_node = get_node_type(TYPE::DIALOG);
  if (dialog_node == nullptr) {
    return dialogs_;
  }

  const auto* dialog_dir = dialog_node->cast<ResourceDirectory>();

  if (dialog_dir == nullptr) {
    LIEF_INFO("Expecting a Directory node for the Dialog Node");
    return dialogs_;
  }

  auto nodes = dialog_dir->childs();
  for (size_t i = 0; i < nodes.size(); ++i) {
    const auto* dialog = nodes[i].cast<ResourceDirectory>();
    if (dialog == nullptr) {
      continue;
    }

    auto langs = dialog->childs();

    for (size_t j = 0; j < langs.size(); ++j) {
      const auto* data_node = langs[j].cast<ResourceData>();
      if (data_node == nullptr) {
        LIEF_INFO("Expecting a Data node for child #{}->{}", i, j);
        continue;
      }

      dialogs_t lang_dialog = ResourceDialog::parse(*data_node);
      if (lang_dialog.empty()) {
        continue;
      }

      std::move(lang_dialog.begin(), lang_dialog.end(),
                std::back_inserter(dialogs_));
    }
  }
  return dialogs_;
}

ResourcesManager::strings_table_t ResourcesManager::string_table() const {
  std::vector<string_entry_t> string_table;
  const ResourceNode* root_node = get_node_type(TYPE::STRING);
  if (root_node == nullptr) {
    return string_table;
  }

  for (const ResourceNode& child_l1 : root_node->childs()) {
    for (const ResourceNode& child_l2 : child_l1.childs()) {
      const auto* data_node = child_l2.cast<ResourceData>();
      if (data_node == nullptr) {
        LIEF_WARN("Expecting a data not for the string node id {}", child_l2.id());
        continue;
      }
      span<const uint8_t> content = data_node->content();
      if (content.empty()) {
        LIEF_ERR("String table content is empty");
        continue;
      }


      SpanStream stream(content);
      for (size_t i = 0; i < 16; ++i) {
        if (!stream) {
          break;
        }

        auto slen = stream.read<uint16_t>();
        if (!slen) {
          break;
        }

        if (*slen == 0) {
          continue;
        }

        std::vector<char16_t> buffer;
        if (!stream.read_objects(buffer, *slen)) {
          break;
        }
        std::u16string str(buffer.begin(), buffer.end());
        uint32_t id = (child_l1.id() - 1) << 4 | i;
        string_table.push_back(string_entry_t{std::move(str), id});
      }
    }
  }
  return string_table;
}

std::vector<std::string> ResourcesManager::html() const {
  const ResourceNode* root_node = get_node_type(TYPE::HTML);
  if (root_node == nullptr) {
    LIEF_ERR("Missing '{}' entry", to_string(TYPE::HTML));
    return {};
  }
  std::vector<std::string> html;
  for (const ResourceNode& child_l1 : root_node->childs()) {
    for (const ResourceNode& child_l2 : child_l1.childs()) {
      if (!child_l2.is_data()) {
        LIEF_ERR("html node corrupted");
        continue;
      }
      const auto& html_node = static_cast<const ResourceData&>(child_l2);

      span<const uint8_t> content = html_node.content();
      if (content.empty()) {
        LIEF_ERR("html content is empty");
        continue;
      }
      html.push_back(std::string{std::begin(content), std::end(content)});
    }
  }

  return html;
}

ResourcesManager::it_const_accelerators ResourcesManager::accelerator() const {
  std::vector<ResourceAccelerator> accelerator;
  const ResourceNode* root_node = get_node_type(TYPE::ACCELERATOR);
  if (root_node == nullptr) {
    LIEF_ERR("Missing '{}' entry", to_string(TYPE::ACCELERATOR));
    return accelerator;
  }

  for (const ResourceNode& child_l1 : root_node->childs()) {
    for (const ResourceNode& child_l2 : child_l1.childs()) {
      const auto* accelerator_node = child_l2.cast<ResourceData>();
      if (accelerator_node == nullptr) {
        LIEF_ERR("Expecting a Data node for node id: {}", child_l2.id());
        continue;
      }

      span<const uint8_t> content = accelerator_node->content();

      if (content.empty()) {
        LIEF_INFO("Accelerator content is empty");
        continue;
      }

      SpanStream stream(content);

      while (stream) {
        auto res_entry = stream.read<details::pe_resource_acceltableentry>();
        if (!res_entry) {
          LIEF_ERR("Can't read pe_resource_acceltableentry");
          break;
        }
        accelerator.emplace_back(*res_entry);
      }

      if (!accelerator.empty()) {
        ResourceAccelerator& acc = accelerator.back();
        if (!acc.has(ResourceAccelerator::FLAGS::END)) {
          LIEF_ERR("Accelerator resources might be corrupted");
        }
      }
    }
  }

  return accelerator;
}

std::string ResourcesManager::print(uint32_t depth) const {
  std::ostringstream oss;
  uint32_t current_depth = 0;
  print_tree(*resources_, oss, current_depth, depth);
  return oss.str();
}

void ResourcesManager::print_tree(const ResourceNode& node, std::ostringstream& output,
                                  uint32_t current_depth, uint32_t max_depth,
                                  const ResourceNode*/*parent*/, std::string header,
                                  bool is_last) const
{
  static constexpr auto ELBOW = "└──";
  static constexpr auto PIPE = "│  ";
  static constexpr auto TEE = "├──";
  static constexpr auto BLANK = "    ";

  if (max_depth > 0 && max_depth < current_depth) {
    return;
  }

  std::string type = node.is_directory() ? "Directory" : "Data";
  std::string info = fmt::format("{} ID: {:04d} (0x{:04x})",
                                 type, node.id(), node.id());
  if (node.has_name()) {
    info += fmt::format(" name: {}", u16tou8(node.name()));
  } else if (std::string ty = to_string(TYPE(node.id()));
             ty != "UNKNOWN" && node.depth() == 1)
  {
    info += fmt::format(" type: {}", ty);
  }
  else if (current_depth == 3) {
    uint32_t lang = lang_from_id(node.id());
    uint32_t sub_lang = sublang_from_id(node.id());
    info += fmt::format(" Lang: 0x{:02x} / Sublang: 0x{:02x}", lang, sub_lang);
  }

  if (const auto* data = node.cast<ResourceData>()) {
    const size_t size = data->content().size();
    info += fmt::format(" length={} (0x{:06x}), offset: 0x{:04x}",
                        size, size, data->offset());
  }

  if (const auto* dir = node.cast<ResourceDirectory>()) {
    info += fmt::format(" children={}", dir->childs().size());
  }

  output << fmt::format("{}{} {}\n", header, is_last ? ELBOW : TEE , info);
  if (const auto* data = node.cast<ResourceData>()) {
    std::string hex_content;
    std::string str_content;
    span<const uint8_t> content = data->content();
    size_t size = std::min<size_t>(content.size(), 20);
    hex_content.reserve(2 * size);
    for (size_t i = 0; i < size; ++i) {
      hex_content += fmt::format("{:02x}", content[i]);
      if (i != (size - 1)) {
        hex_content += ':';
      }
      if (char c = (char)content[i]; is_printable(c)) {
        str_content += c;
      } else {
        str_content += '.';
      }
    }
    output << fmt::format("{}{}{} Hex: {}\n", header, BLANK, TEE, hex_content);
    output << fmt::format("{}{}{} Str: {}\n", header, BLANK, ELBOW, str_content);
  }

  auto children = node.childs();
  for (size_t i = 0; i < children.size(); ++i) {
    print_tree(children[i], output, current_depth + 1, max_depth,
               &node, header + (is_last ? BLANK : PIPE),
               i == (children.size() - 1));
  }
}

void ResourcesManager::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const ResourcesManager& rsrc) {
  os << rsrc.print(3) << '\n';

  std::vector<ResourcesManager::TYPE> types = rsrc.get_types();

  if (!types.empty()) {
    os << fmt::format("Types: {}\n", types);
  }

  if (const std::string& manifest = rsrc.manifest(); !manifest.empty()) {
    os << fmt::format("Manifest:\n{}\n", manifest);
  }

  for (const ResourceVersion& version : rsrc.version()) {
    os << version << '\n';
  }

  const auto& icons = rsrc.icons();
  for (size_t i = 0; i < icons.size(); ++i) {
    os << fmt::format("Icon #{:02d}:\n{}\n", i, to_string(icons[i]));
  }

  const auto& dialogs = rsrc.dialogs();
  for (size_t i = 0; i < dialogs.size(); ++i) {
    os << fmt::format("Dialog #{:02d}:\n{}\n", i, to_string(dialogs[i]));
  }

  const auto& str_table = rsrc.string_table();
  for (size_t i = 0; i < str_table.size(); ++i) {
    const ResourcesManager::string_entry_t& entry = str_table[i];
    os << fmt::format("StringTable[{}]: {}", i, LIEF::to_string(entry));
  }
  return os;
}

const char* to_string(ResourcesManager::TYPE type) {
  #define ENTRY(X) std::pair(ResourcesManager::TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(CURSOR),
    ENTRY(BITMAP),
    ENTRY(ICON),
    ENTRY(MENU),
    ENTRY(DIALOG),
    ENTRY(STRING),
    ENTRY(FONTDIR),
    ENTRY(FONT),
    ENTRY(ACCELERATOR),
    ENTRY(RCDATA),
    ENTRY(MESSAGETABLE),
    ENTRY(GROUP_CURSOR),
    ENTRY(GROUP_ICON),
    ENTRY(VERSION),
    ENTRY(DLGINCLUDE),
    ENTRY(PLUGPLAY),
    ENTRY(VXD),
    ENTRY(ANICURSOR),
    ENTRY(ANIICON),
    ENTRY(HTML),
    ENTRY(MANIFEST),
  };
  #undef ENTRY

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}


} // namespace PE
} // namespace LIEF
