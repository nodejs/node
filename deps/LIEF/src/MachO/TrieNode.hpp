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
#ifndef LIEF_MACHO_TRIE_NODE_H_
#define LIEF_MACHO_TRIE_NODE_H_
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#include "LIEF/visibility.h"

namespace LIEF {
class vector_iostream;
namespace MachO {
class TrieNode;
class ExportInfo;

class LIEF_LOCAL TrieEdge {
  public:
  static std::unique_ptr<TrieEdge> create(const std::string& str, TrieNode& node) {
    return std::make_unique<TrieEdge>(str, node);
  }

  TrieEdge() = delete;
  TrieEdge(std::string str, TrieNode& node) :
    substr(std::move(str)),
    child(&node)
  {}

  ~TrieEdge() = default;

  public:
  std::string substr;
  TrieNode* child = nullptr;
};


class LIEF_LOCAL TrieNode {
  public:
  using trie_edge_list_t = std::vector<std::unique_ptr<TrieEdge>>;
  using node_list_t = std::vector<std::unique_ptr<TrieNode>>;

  static std::unique_ptr<TrieNode> create(std::string str) {
    return std::make_unique<TrieNode>(std::move(str));
  }

  TrieNode() = delete;

  TrieNode(std::string str) :
    cummulative_string_(std::move(str))
  {}

  ~TrieNode() = default;

  TrieNode& add_symbol(const ExportInfo& info, node_list_t& nodes);
  TrieNode& add_ordered_nodes(const ExportInfo& info, std::vector<TrieNode*>& nodes);
  bool update_offset(uint32_t& offset);

  TrieNode& write(vector_iostream& buffer);

  private:
  std::string cummulative_string_;
  trie_edge_list_t children_;
  uint64_t address_ = 0;
  uint64_t flags_ = 0;
  uint64_t other_ = 0;
  std::string imported_name_;
  uint32_t trie_offset_ = 0;
  bool has_export_info_ = false;
  bool ordered_ = false;
};

}
}

#endif
