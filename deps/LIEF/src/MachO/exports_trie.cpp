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
#include "LIEF/iostream.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/MachO/ExportInfo.hpp"

#include "logging.hpp"

#include "MachO/exports_trie.hpp"
#include "MachO/TrieNode.hpp"

namespace LIEF {
namespace MachO {
void show_trie(std::ostream& output, std::string output_prefix,
               BinaryStream& stream, uint64_t start, uint64_t end, const std::string& prefix) {


  if (stream.pos() >= end) {
    return;
  }

  if (start > stream.pos()) {
    return;
  }

  uint8_t terminal_size = 0;
  if (auto res = stream.read<uint8_t>()) {
    terminal_size = *res;
  } else {
    LIEF_ERR("Can't read terminal size");
    return;
  }

  uint64_t children_offset = stream.pos() + terminal_size;

  if (terminal_size != 0) {
    ExportInfo::FLAGS flags;
    if (auto res = stream.read_uleb128()) {
      flags = ExportInfo::FLAGS(*res);
    } else {
      LIEF_ERR("Can't read flags");
      return;
    }
    uint64_t address = 0;
    const std::string& symbol_name = prefix;
    uint64_t ordinal = 0;
    uint64_t other = 0;
    std::string imported_name;

    // REEXPORT
    // ========
    if (is_true(flags & ExportInfo::FLAGS::REEXPORT)) {
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
    if (is_true(flags & ExportInfo::FLAGS::STUB_AND_RESOLVER)) {
      if (auto res = stream.read_uleb128()) {
        other = *res;
      } else {
        return;
      }
    }

    output << output_prefix;
    output << symbol_name;
    output << "{";
    output << "addr: " << std::showbase << std::hex << address << ", ";
    output << "flags: " << std::showbase << std::hex << static_cast<uint64_t>(flags);
    if (is_true(flags & ExportInfo::FLAGS::REEXPORT)) {
      output << ", ";
      output << "re-exported from #" << std::dec << ordinal << " - " << imported_name;
    }

    if (is_true(flags & ExportInfo::FLAGS::STUB_AND_RESOLVER) && other > 0) {
      output << ", ";
      output << "other:" << std::showbase << std::hex << other;
    }

    //if (!binary_->has_symbol(symbol_name)) {
    //  output << " [NOT REGISTRED]";
    //}

    output << "}";
    output << '\n';

  }
  stream.setpos(children_offset);
  uint32_t nb_children = 0;
  if (auto res = stream.read_uleb128()) {
    nb_children = *res;
  } else {
    return;
  }

  output_prefix += "    ";
  for (size_t i = 0; i < nb_children; ++i) {
    std::string suffix;
    if (auto res = stream.read_string()) {
      suffix = std::move(*res);
    } else {
      break;
    }

    std::string name = prefix + suffix;
    uint32_t child_node_offet = 0;

    if (auto res = stream.read_uleb128()) {
      child_node_offet = *res;
    } else {
      break;
    }

    if (child_node_offet == 0) {
      break;
    }

    output << output_prefix << name << "@off." << std::hex << std::showbase << stream.pos() << '\n';

    size_t current_pos = stream.pos();
    stream.setpos(start + child_node_offet);
    show_trie(output, output_prefix, stream, start, end, name);
    stream.setpos(current_pos);
  }

}

std::vector<uint8_t> create_trie(const exports_list_t& exports, size_t pointer_size) {

  std::unique_ptr<TrieNode> start = TrieNode::create("");
  std::vector<std::unique_ptr<TrieNode>> nodes;

  // Build the tree adding every symbole to the root.
  TrieNode* start_ptr = start.get();

  for (const std::unique_ptr<ExportInfo>& info : exports) {
    start_ptr->add_symbol(*info, nodes);
  }

  // Perform a poor topological sort to have parents before childs in ordered_nodes
  std::vector<TrieNode*> ordered_nodes;
  ordered_nodes.reserve(exports.size() * 2);
  for (const std::unique_ptr<ExportInfo>& info : exports) {
    start_ptr->add_ordered_nodes(*info, ordered_nodes);
  }

  bool more;
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
}
