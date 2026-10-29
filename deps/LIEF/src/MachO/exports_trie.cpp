/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
#include "LIEF/iostream.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/MachO/ExportInfo.hpp"

#include <algorithm>
#include <unordered_set>

#include <spdlog/fmt/fmt.h>

#include "logging.hpp"

#include "MachO/exports_trie.hpp"
#include "MachO/TrieNode.hpp"


namespace LIEF::MachO {

void show_trie_impl(std::ostream& output, std::string output_prefix,
                    BinaryStream& stream, uint64_t start, uint64_t end,
                    const std::string& prefix,
                    std::unordered_set<uint64_t>& visited, size_t depth) {
  constexpr size_t MAX_TRIE_DEPTH = 60;

  if (depth > MAX_TRIE_DEPTH) {
    return;
  }

  if (stream.pos() >= end) {
    return;
  }

  if (start > stream.pos()) {
    return;
  }

  if (!visited.insert(stream.pos()).second) {
    return;
  }

  auto terminal_size = stream.read<uint8_t>();
  if (!terminal_size) {
    LIEF_ERR("Failed to read terminal size");
    return;
  }

  uint64_t children_offset = stream.pos() + *terminal_size;

  if (*terminal_size != 0) {
    auto flags =
        stream.read_uleb128().map([](uint64_t v) { return (ExportInfo::FLAGS)v; });
    if (!flags) {
      LIEF_ERR("Failed to read flags");
      return;
    }
    uint64_t address = 0;
    const std::string& symbol_name = prefix;
    uint64_t ordinal = 0;
    uint64_t other = 0;
    std::string imported_name;

    // REEXPORT
    // ========
    if (is_true(*flags & ExportInfo::FLAGS::REEXPORT)) {
      if (auto res = stream.read_uleb128()) {
        ordinal = *res;
      } else {
        return;
      }
      if (auto res = stream.peek_string()) {
        imported_name = std::move(*res);
      } else {
        return;
      }
      if (imported_name.empty()) {
        imported_name = symbol_name;
      }
    } else {
      if (auto res = stream.read_uleb128()) {
        address = *res;
      } else {
        return;
      }
    }


    // STUB_AND_RESOLVER
    // =================
    if (is_true(*flags & ExportInfo::FLAGS::STUB_AND_RESOLVER)) {
      if (auto res = stream.read_uleb128()) {
        other = *res;
      } else {
        return;
      }
    }

    output << fmt::format("{}{}{{addr: {:#x}, flags: {:#x}", output_prefix,
                          symbol_name, address, (uint64_t)*flags);
    if (is_true(*flags & ExportInfo::FLAGS::REEXPORT)) {
      output << fmt::format(", re-exported from #{} - {}", ordinal, imported_name);
    }

    if (is_true(*flags & ExportInfo::FLAGS::STUB_AND_RESOLVER) && other > 0) {
      output << fmt::format(", other:{:#x}", other);
    }

    output << "}\n";
  }
  stream.setpos(children_offset);
  auto nb_children = stream.read_uleb128();
  if (!nb_children) {
    return;
  }

  if (stream.pos() >= end) {
    return;
  }

  const uint64_t remaining = end - stream.pos();
  const uint64_t children = std::min<uint64_t>(*nb_children, remaining);

  output_prefix += "    ";
  for (uint64_t i = 0; i < children; ++i) {
    auto suffix = stream.read_string();
    if (!suffix) {
      break;
    }

    std::string name = prefix + *suffix;
    auto child_node_offet = stream.read_uleb128();
    if (!child_node_offet || *child_node_offet == 0) {
      break;
    }

    if (*child_node_offet >= end - start) {
      break;
    }

    output << fmt::format("{}{}@off.{:#x}\n", output_prefix, name, stream.pos());

    ScopedStream scope(stream, start + *child_node_offet);
    show_trie_impl(output, output_prefix, *scope, start, end, name, visited,
                   depth + 1);
  }
}

void show_trie(std::ostream& output, std::string output_prefix,
               BinaryStream& stream, uint64_t start, uint64_t end,
               const std::string& prefix) {
  std::unordered_set<uint64_t> visited;
  show_trie_impl(output, std::move(output_prefix), stream, start, end, prefix,
                 visited, /*depth=*/0);
}

std::vector<uint8_t> create_trie(const exports_list_t& exports,
                                 size_t pointer_size) {

  std::unique_ptr<TrieNode> start = TrieNode::create("");
  std::vector<std::unique_ptr<TrieNode>> nodes;

  // Build the tree by adding every symbol to the root.
  TrieNode* start_ptr = start.get();

  for (const std::unique_ptr<ExportInfo>& info : exports) {
    TrieNode::node_list_t sym_node = start_ptr->add_symbol(*info);
    nodes.insert(nodes.end(), std::make_move_iterator(sym_node.begin()),
                 std::make_move_iterator(sym_node.end()));
  }

  // Perform a poor topological sort to have parents before children in
  // ordered_nodes
  std::vector<TrieNode*> ordered_nodes;
  ordered_nodes.reserve(exports.size() * 2);
  for (const std::unique_ptr<ExportInfo>& info : exports) {
    std::vector<TrieNode*> ord_nodes = start_ptr->add_ordered_nodes(*info);
    ordered_nodes.insert(ordered_nodes.end(), ord_nodes.begin(), ord_nodes.end());
  }

  bool more = false;
  do {
    uint32_t offset = 0;
    more = false;
    for (TrieNode* node : ordered_nodes) {
      if (node->update_offset(offset)) {
        more = true;
      }
    }
  } while (more);


  vector_iostream raw_output;
  for (TrieNode* node : ordered_nodes) {
    node->write(raw_output);
  }

  raw_output.align(pointer_size);

  return raw_output.raw();
}
}
