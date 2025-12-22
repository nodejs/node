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
#include <algorithm>

#include "logging.hpp"
#include "LIEF/Visitor.hpp"

#include "LIEF/utils.hpp"

#include "LIEF/PE/Binary.hpp"
#include "LIEF/PE/ResourceNode.hpp"
#include "LIEF/PE/ResourceDirectory.hpp"
#include "LIEF/PE/ResourceData.hpp"
#include "LIEF/PE/ResourcesManager.hpp"

#include "LIEF/BinaryStream/SpanStream.hpp"

#include "internal_utils.hpp"
#include "PE/Structures.hpp"

namespace LIEF {
namespace PE {

class TreeParser {
  public:
  TreeParser() = delete;
  TreeParser(BinaryStream& stream, uint64_t base_rva) :
    stream_(stream),
    base_offset_(stream.pos()),
    base_rva_(base_rva)
  {}

  TreeParser(BinaryStream& stream, Binary& pe) :
    stream_(stream),
    base_offset_(stream.pos()),
    base_rva_(0),
    pe_(&pe)
  {}

  std::unique_ptr<ResourceNode> parse() {
    const auto res_directory_table = stream_.peek<details::pe_resource_directory_table>();
    if (!res_directory_table) {
      return nullptr;
    }
    const size_t pos = stream_.pos();
    return parse_resource_node(*res_directory_table, pos, pos);
  }

  std::unique_ptr<ResourceNode> parse_resource_node(
      const details::pe_resource_directory_table& directory_table,
      uint32_t base_offset, uint32_t current_offset, uint32_t depth = 0);

  result<uint64_t> rva_to_offset(uint64_t rva) const {
    if (pe_ != nullptr) {
      return pe_->rva_to_offset(rva);
    }
    if (base_rva_ <= rva && rva < (base_rva_ + stream_.size())) {
      return (rva - base_rva_) + base_offset_;
    }
    return make_error_code(lief_errors::conversion_error);
  }

  private:
  BinaryStream& stream_;
  uint32_t base_offset_ = 0;
  uint32_t base_rva_ = 0;
  Binary* pe_ = nullptr;
  std::set<uint32_t> visited_;
};

std::unique_ptr<ResourceNode> TreeParser::parse_resource_node(
  const details::pe_resource_directory_table& directory_table,
  uint32_t base_offset, uint32_t current_offset, uint32_t depth)
{
  const uint32_t numberof_ID_entries   = directory_table.NumberOfIDEntries;
  const uint32_t numberof_name_entries = directory_table.NumberOfNameEntries;

  size_t directory_array_offset = current_offset + sizeof(details::pe_resource_directory_table);
  details::pe_resource_directory_entries entries_array;

  if (auto res_entries_array = stream_.peek<details::pe_resource_directory_entries>(directory_array_offset)) {
    entries_array = *res_entries_array;
  } else {
    return nullptr;
  }

  auto directory = std::make_unique<ResourceDirectory>(directory_table);
  directory->set_depth(depth);

  // Iterate over the childs
  for (size_t idx = 0; idx < (numberof_name_entries + numberof_ID_entries); ++idx) {

    uint32_t data_rva = entries_array.RVA;
    uint32_t id       = entries_array.NameID.IntegerID;

    directory_array_offset += sizeof(details::pe_resource_directory_entries);
    if (auto res_entries_array = stream_.peek<details::pe_resource_directory_entries>(directory_array_offset)) {
      entries_array = *res_entries_array;
    } else {
      break;
    }

    result<std::u16string> name;

    // Get the resource name
    if ((id & 0x80000000) != 0u) {
      uint32_t offset        = id & (~ 0x80000000);
      uint32_t string_offset = base_offset + offset;
      LIEF_DEBUG("base_offset=0x{:04x}, string_offset=0x{:04x}",
                 base_offset, string_offset);

      auto res_length = stream_.peek<uint16_t>(string_offset);
      if (res_length && *res_length <= 100) {
        name = stream_.peek_u16string_at(string_offset + sizeof(uint16_t), *res_length);
        if (!name) {
          LIEF_ERR("Node's name for the node id: {} is corrupted", id);
        }
      }
    }

    if ((0x80000000 & data_rva) == 0) { // We are on a leaf
      uint32_t offset = base_offset + data_rva;
      details::pe_resource_data_entry data_entry;

      if (!visited_.insert(offset).second) {
        if (visited_.size() == 1) {
          // Only print once
          LIEF_WARN("Infinite loop detected on resources");
        }
        break;
      }

      if (auto res_data_entry = stream_.peek<details::pe_resource_data_entry>(offset)) {
        data_entry = *res_data_entry;
      } else {
        break;
      }

      auto content_offset = rva_to_offset(data_entry.DataRVA);

      uint32_t content_size   = data_entry.Size;
      uint32_t code_page      = data_entry.Codepage;

      std::vector<uint8_t> leaf_data;
      if (content_offset &&
          stream_.peek_data(leaf_data, *content_offset, content_size,
                            data_entry.DataRVA))
      {
        auto node = std::make_unique<ResourceData>(std::move(leaf_data), code_page);

        node->set_depth(depth + 1);
        node->id(id);
        node->set_offset(*content_offset);
        if (name) {
          node->name(*name);
        }

        directory->push_child(std::move(node));
      } else {
        LIEF_DEBUG("The leaf of the node id {} is corrupted", id);
        break;
      }
    } else { // We are on a directory
      const uint32_t directory_rva = data_rva & (~ 0x80000000);
      const uint32_t offset        = base_offset + directory_rva;
      if (!visited_.insert(offset).second) {
        if (visited_.size() == 1) {
          // Only print once
          LIEF_WARN("Infinite loop detected on resources");
        }
        break;
      }

      if (auto res_next_dir_table = stream_.peek<details::pe_resource_directory_table>(offset)) {
        if (auto node = parse_resource_node(*res_next_dir_table, base_offset, offset, depth + 1)) {
          if (name) {
            node->name(*name);
          }
          node->id(id);
          directory->push_child(std::move(node));
        } else {
          // node is a nullptr
          continue;
        }
      } else {
        LIEF_WARN("The directory of the node id {} is corrupted", id);
        break;
      }
    }
  }
  return directory;
}


std::unique_ptr<ResourceNode>
  ResourceNode::parse(BinaryStream& stream, uint64_t rva)
{
  TreeParser parser(stream, rva);
  return parser.parse();
}

std::unique_ptr<ResourceNode>
  ResourceNode::parse(const uint8_t* buffer, size_t size, uint64_t rva)
{
  SpanStream stream(buffer, size);
  return parse(stream, rva);
}

std::unique_ptr<ResourceNode>
  ResourceNode::parse(BinaryStream& stream, const Binary& bin)
{
  TreeParser parser(stream, const_cast<Binary&>(bin));
  return parser.parse();
}

ResourceNode::~ResourceNode() = default;

ResourceNode::ResourceNode(const ResourceNode& other) :
  Object{other},
  type_{other.type_},
  id_{other.id_},
  name_{other.name_},
  depth_{other.depth_}
{
  childs_.reserve(other.childs_.size());
  for (const std::unique_ptr<ResourceNode>& node : other.childs_) {
    childs_.push_back(node->clone());
  }
}

ResourceNode& ResourceNode::operator=(const ResourceNode& other) {
  if (this == &other) {
    return *this;
  }
  type_   = other.type_;
  id_     = other.id_;
  name_   = other.name_;
  depth_  = other.depth_;

  childs_.reserve(other.childs_.size());
  for (const std::unique_ptr<ResourceNode>& node : other.childs_) {
    childs_.push_back(node->clone());
  }
  return *this;
}

void ResourceNode::swap(ResourceNode& other) {
  std::swap(type_,   other.type_);
  std::swap(id_,     other.id_);
  std::swap(name_,   other.name_);
  std::swap(childs_, other.childs_);
  std::swap(depth_,  other.depth_);
}

std::string ResourceNode::utf8_name() const {
  return u16tou8(name());
}

ResourceNode& ResourceNode::add_child(std::unique_ptr<ResourceNode> child) {
  child->depth_ = depth_ + 1;

  if (auto* dir = cast<ResourceDirectory>()) {
    child->has_name() ? dir->numberof_name_entries(dir->numberof_name_entries() + 1) :
                        dir->numberof_id_entries(dir->numberof_id_entries() + 1);
    return **insert_child(std::move(child));
  }

  push_child(std::move(child));
  return *childs_.back();
}

void ResourceNode::delete_child(uint32_t id) {
  const auto it_node = std::find_if(std::begin(childs_), std::end(childs_),
      [id] (const std::unique_ptr<ResourceNode>& node) {
        return node->id() == id;
      });

  if (it_node == std::end(childs_)) {
    LIEF_ERR("Unable to find the node with the id {:d}", id);
    return;
  }

  delete_child(**it_node);
}

void ResourceNode::delete_child(const ResourceNode& node) {
  const auto it_node = std::find_if(std::begin(childs_), std::end(childs_),
      [&node] (const std::unique_ptr<ResourceNode>& intree_node) {
        return *intree_node == node;
      });

  if (it_node == std::end(childs_)) {
    LIEF_ERR("Unable to find the node with id: {}", node.id());
    return;
  }

  std::unique_ptr<ResourceNode>& inode = *it_node;

  if (auto* dir = cast<ResourceDirectory>()) {
    if (inode->has_name()) {
      dir->numberof_name_entries(dir->numberof_name_entries() - 1);
    } else {
      dir->numberof_id_entries(dir->numberof_id_entries() - 1);
    }
  }

  childs_.erase(it_node);
}

void ResourceNode::name(const std::string& name) {
  if (auto res = u8tou16(name)) {
    return this->name(std::move(*res));
  }
  LIEF_WARN("{} can't be converted to a UTF-16 string", name);
}


// This logic follows the description from the Microsoft documentation at
// https://docs.microsoft.com/en-us/windows/win32/debug/pe-format#resource-directory-table
//
// "(remember that all the Name entries precede all the ID entries for the table). All entries for the table
// "are sorted in ascending order: the Name entries by case-sensitive string and the ID entries by numeric value."
ResourceNode::childs_t::iterator ResourceNode::insert_child(std::unique_ptr<ResourceNode> child) {
  const auto it = std::upper_bound(childs_.begin(), childs_.end(), child,
      [] (const std::unique_ptr<ResourceNode>& lhs, const std::unique_ptr<ResourceNode>& rhs) {
        if (lhs->has_name() && rhs->has_name()) {
          // Case-sensitive string sort
          return std::lexicographical_compare(
              lhs->name().begin(), lhs->name().end(),
              rhs->name().begin(), rhs->name().end());
        } else if (!lhs->has_name() && !rhs->has_name()) {
          return lhs->id() < rhs->id();
        } else {
          // Named entries come first
          return lhs->has_name();
        }
      });

  return childs_.insert(it, std::move(child));
}

const ResourceNode& ResourceNode::safe_get_at(size_t idx) const {
  class InvalidNode : public ResourceNode {
    public:
    InvalidNode() :
      ResourceNode(ResourceNode::TYPE::UNKNOWN)
    {}

    std::unique_ptr<ResourceNode> clone() const {
      return nullptr;
    }
  };

  static InvalidNode INVALID;

  if (idx < childs_.size()) {
    return *childs_[idx];
  }
  return INVALID;
}

void ResourceNode::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

bool operator==(const ResourceNode& LHS, const ResourceNode& RHS) {
  if (LHS.id() != RHS.id()) {
    return false;
  }

  if (LHS.utf8_name() != RHS.utf8_name()) {
    return false;
  }

  if (const auto* lhs_data = LHS.cast<ResourceData>()) {
    const auto* rhs_data = RHS.cast<ResourceData>();
    if (rhs_data == nullptr) {
      return false;
    }

    if (lhs_data->code_page() != rhs_data->code_page()) {
      return false;
    }

    if (lhs_data->reserved() != rhs_data->reserved()) {
      return false;
    }

    if (lhs_data->raw_content() != rhs_data->raw_content()) {
      return false;
    }
  }

  if (const auto* lhs_dir = LHS.cast<ResourceDirectory>()) {
    const auto* rhs_dir = RHS.cast<ResourceDirectory>();
    if (rhs_dir == nullptr) {
      return false;
    }

    if (lhs_dir->characteristics() != rhs_dir->characteristics()) {
      return false;
    }

    if (lhs_dir->time_date_stamp() != rhs_dir->time_date_stamp()) {
      return false;
    }

    if (lhs_dir->major_version() != rhs_dir->major_version()) {
      return false;
    }

    if (lhs_dir->minor_version() != rhs_dir->minor_version()) {
      return false;
    }

    if (lhs_dir->numberof_name_entries() != rhs_dir->numberof_name_entries()) {
      return false;
    }

    if (lhs_dir->numberof_id_entries() != rhs_dir->numberof_id_entries()) {
      return false;
    }

    auto children_lhs = lhs_dir->childs();
    auto children_rhs = rhs_dir->childs();
    if (children_lhs.size() != children_rhs.size()) {
      return false;
    }

    for (size_t i = 0; i < children_lhs.size(); ++i) {
      if (children_lhs[i] != children_rhs[i]) {
        return false;
      }
    }
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const ResourceNode& node) {
  ResourcesManager manager(const_cast<ResourceNode&>(node));
  os << manager.print();
  return os;

}


}
}
