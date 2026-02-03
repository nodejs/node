/* Copyright 2021 - 2025 R. Thomas
 * Copyright 2021 - 2025 Quarkslab
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
#include "TrieNode.hpp"
#include "logging.hpp"

#include <utility>

#include "LIEF/MachO/Symbol.hpp"
#include "LIEF/MachO/ExportInfo.hpp"
#include "LIEF/iostream.hpp"

namespace LIEF {
namespace MachO {


// Mainly inspired from LLVM: lld/lib/ReaderWriter/MachO/MachONormalizedFileBinaryWriter.cpp
TrieNode& TrieNode::add_symbol(const ExportInfo& info, TrieNode::node_list_t& nodes) {
  if (!info.has_symbol()) {
    LIEF_ERR("Missing symbol in the Trie node");
    return *this;
  }
  const Symbol& sym = *info.symbol();
  std::string partial_str = sym.name().substr(cummulative_string_.size());

  for (std::unique_ptr<TrieEdge>& edge : children_) {

    std::string edge_string = edge->substr;

    if (partial_str.find(edge_string) == 0) {
      edge->child->add_symbol(info, nodes);
      return *this;
    }

    for (int n = edge_string.size() - 1; n > 0; --n) {
      if (partial_str.substr(0, n) == edge_string.substr(0, n)) {

        std::string b_node_str = edge->child->cummulative_string_;
        b_node_str = b_node_str.substr(0, b_node_str.size() + n - edge_string.size()); // drop front

        std::unique_ptr<TrieNode> b_new_node = TrieNode::create(b_node_str);

        TrieNode& c_node = *edge->child;

        std::string ab_edge_str = edge_string.substr(0, n);
        std::string bc_edge_str = edge_string.substr(n);

        TrieEdge& ab_edge = *edge;

        ab_edge.substr = ab_edge_str;
        ab_edge.child = b_new_node.get();

        std::unique_ptr<TrieEdge> bc_edge = TrieEdge::create(bc_edge_str, c_node);
        b_new_node->children_.push_back(std::move(bc_edge));
        b_new_node->add_symbol(info, nodes);

        nodes.push_back(std::move(b_new_node));
        return *this;
      }
    }
  }

  if (info.has(ExportInfo::FLAGS::REEXPORT) && info.other() != 0) {
    LIEF_DEBUG("REEXPORT: other={}", info.other());
  }

  if (info.has(ExportInfo::FLAGS::STUB_AND_RESOLVER) && info.other() == 0) {
    LIEF_DEBUG("STUB_AND_RESOLVER: other=0");
  }

  std::unique_ptr<TrieNode> new_node = TrieNode::create(sym.name());
  std::unique_ptr<TrieEdge> new_edge = TrieEdge::create(partial_str, *new_node);

  new_node->address_ = info.address();
  new_node->flags_   = info.flags();
  new_node->other_   = info.other();

  if (info.has(ExportInfo::FLAGS::REEXPORT)) {
    new_node->imported_name_ = "";
    if ((info.alias() != nullptr) && info.alias()->name() != sym.name()) {
      new_node->imported_name_ = info.alias()->name();
    }
  }

  new_node->has_export_info_ = true;

  children_.push_back(std::move(new_edge));
  nodes.push_back(std::move(new_node));
  return *this;
}


// Mainly inspired from LLVM: lld/lib/ReaderWriter/MachO/MachONormalizedFileBinaryWriter.cpp - addOrderedNodes
// Add info in nodes making sure every parents node is inserted before
TrieNode& TrieNode::add_ordered_nodes(const ExportInfo& info, std::vector<TrieNode*>& nodes) {
  if (!ordered_) {
    nodes.push_back(this);
    ordered_ = true;
  }

  if (!info.has_symbol()) {
    LIEF_ERR("Missing symbol can process add_ordered_nodes");
    return *this;
  }

  std::string partial_str = info.symbol()->name().substr(cummulative_string_.size());
  for (std::unique_ptr<TrieEdge>& edge : children_) {
    std::string edge_string = edge->substr;

    if (partial_str.find(edge_string) == 0) {
      edge->child->add_ordered_nodes(info, nodes);
      return *this;
    }
  }
  return *this;
}


// Mainly inspired from LLVM: lld/lib/ReaderWriter/MachO/MachONormalizedFileBinaryWriter.cpp - updateOffset
bool TrieNode::update_offset(uint32_t& offset) {
  uint32_t node_size = 1;
  if (has_export_info_) {
   if ((flags_ & static_cast<uint64_t>(ExportInfo::FLAGS::REEXPORT)) != 0u) {
      node_size = vector_iostream::uleb128_size(flags_);
      node_size += vector_iostream::uleb128_size(other_);
      node_size += imported_name_.size() + 1;
    } else {
      node_size = vector_iostream::uleb128_size(flags_);
      node_size += vector_iostream::uleb128_size(address_);
      if ((flags_ & static_cast<uint64_t>(ExportInfo::FLAGS::STUB_AND_RESOLVER)) != 0u) {
        node_size += vector_iostream::uleb128_size(other_);
      }
    }
    node_size += vector_iostream::uleb128_size(node_size);
  }

  ++node_size;

  for (std::unique_ptr<TrieEdge>& edge : children_) {
    node_size += edge->substr.size() + 1;
    node_size += vector_iostream::uleb128_size(edge->child->trie_offset_);
  }

  bool result = (trie_offset_ != offset);
  trie_offset_ = offset;
  offset += node_size;

  return result;
}

TrieNode& TrieNode::write(vector_iostream& buffer) {
  if (has_export_info_) {
    if ((flags_ & static_cast<uint64_t>(ExportInfo::FLAGS::REEXPORT)) != 0u) {
      if (!imported_name_.empty()) {
        uint32_t node_size = 0;
        node_size += vector_iostream::uleb128_size(flags_);
        node_size += vector_iostream::uleb128_size(other_);
        node_size += imported_name_.size() + 1;

        buffer
          .write<uint8_t>(node_size)
          .write_uleb128(flags_)
          .write_uleb128(other_)
          .write(imported_name_);

      } else {
        uint32_t node_size = 0;
        node_size += vector_iostream::uleb128_size(flags_);
        node_size += vector_iostream::uleb128_size(other_);
        node_size += 1;
        buffer
          .write<uint8_t>(node_size)
          .write_uleb128(flags_)
          .write_uleb128(other_)
          .write<uint8_t>('\0');
      }
    }
    else if ((flags_ & static_cast<uint64_t>(ExportInfo::FLAGS::STUB_AND_RESOLVER)) != 0u) {
      uint32_t node_size = 0;
      node_size += vector_iostream::uleb128_size(flags_);
      node_size += vector_iostream::uleb128_size(address_);
      node_size += vector_iostream::uleb128_size(other_);

      buffer
        .write<uint8_t>(node_size)
        .write_uleb128(flags_)
        .write_uleb128(address_)
        .write_uleb128(other_);
    }
    else {
      uint32_t node_size = 0;
      node_size += vector_iostream::uleb128_size(flags_);
      node_size += vector_iostream::uleb128_size(address_);

      buffer
        .write<uint8_t>(node_size)
        .write_uleb128(flags_)
        .write_uleb128(address_);
    }

  } else { // not has_export_info_
    buffer.write<uint8_t>(0);
  }

  // Number of childs
  if (children_.size() >= 256) {
    LIEF_WARN("Too many children ({:d})", children_.size());
    return *this;
  }

  buffer.write<uint8_t>(children_.size());
  for (std::unique_ptr<TrieEdge>& edge : children_) {
    buffer.write(edge->substr)
          .write_uleb128(edge->child->trie_offset_);
  }
  return *this;
}
}
}
