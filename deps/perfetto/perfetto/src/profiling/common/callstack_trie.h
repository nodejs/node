/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_PROFILING_COMMON_CALLSTACK_TRIE_H_
#define SRC_PROFILING_COMMON_CALLSTACK_TRIE_H_

#include <string>
#include <typeindex>
#include <vector>

#include "perfetto/ext/base/lookup_set.h"
#include "src/profiling/common/interner.h"
#include "src/profiling/common/unwind_support.h"

namespace perfetto {
namespace profiling {

struct Mapping {
  Mapping(Interned<std::string> b) : build_id(std::move(b)) {}

  Interned<std::string> build_id;
  uint64_t exact_offset = 0;
  uint64_t start_offset = 0;
  uint64_t start = 0;
  uint64_t end = 0;
  uint64_t load_bias = 0;
  std::vector<Interned<std::string>> path_components{};

  bool operator<(const Mapping& other) const {
    return std::tie(build_id, exact_offset, start_offset, start, end, load_bias,
                    path_components) <
           std::tie(other.build_id, other.exact_offset, other.start_offset,
                    other.start, other.end, other.load_bias,
                    other.path_components);
  }
  bool operator==(const Mapping& other) const {
    return std::tie(build_id, exact_offset, start_offset, start, end, load_bias,
                    path_components) ==
           std::tie(other.build_id, other.exact_offset, other.start_offset,
                    other.start, other.end, other.load_bias,
                    other.path_components);
  }
};

struct Frame {
  Frame(Interned<Mapping> m, Interned<std::string> fn_name, uint64_t pc)
      : mapping(m), function_name(fn_name), rel_pc(pc) {}
  Interned<Mapping> mapping;
  Interned<std::string> function_name;
  uint64_t rel_pc;

  bool operator<(const Frame& other) const {
    return std::tie(mapping, function_name, rel_pc) <
           std::tie(other.mapping, other.function_name, other.rel_pc);
  }

  bool operator==(const Frame& other) const {
    return std::tie(mapping, function_name, rel_pc) ==
           std::tie(other.mapping, other.function_name, other.rel_pc);
  }
};

// Graph of function callsites. A single instance can be used for callsites from
// different processes. Each call site is represented by a
// GlobalCallstackTrie::Node that is owned by the parent callsite. Each node has
// a pointer to its parent, which means the function call-graph can be
// reconstructed from a GlobalCallstackTrie::Node by walking down the parent
// chain.
//
// For the following two callstacks:
//  * libc_init -> main -> foo -> alloc_buf
//  * libc_init -> main -> bar -> alloc_buf
// The tree looks as following:
//             alloc_buf  alloc_buf
//                   |      |
//                  foo    bar
//                    \    /
//                      main
//                       |
//                   libc_init
//                       |
//                    [root_]
class GlobalCallstackTrie {
 public:
  // Optionally, Nodes can be externally refcounted via |IncrementNode| and
  // |DecrementNode|. In which case, the orphaned nodes are deleted when the
  // last reference is decremented.
  class Node {
   public:
    // This is opaque except to GlobalCallstackTrie.
    friend class GlobalCallstackTrie;

    // Allow building a node out of a frame for base::LookupSet.
    Node(Interned<Frame> frame) : Node(frame, 0, nullptr) {}
    Node(Interned<Frame> frame, uint64_t id)
        : Node(std::move(frame), id, nullptr) {}
    Node(Interned<Frame> frame, uint64_t id, Node* parent)
        : id_(id), parent_(parent), location_(frame) {}

    ~Node() { PERFETTO_DCHECK(!ref_count_); }

    uint64_t id() const { return id_; }

   private:
    Node* GetOrCreateChild(const Interned<Frame>& loc);
    // Deletes all descendant nodes, regardless of |ref_count_|.
    void DeleteChildren() { children_.Clear(); }

    uint64_t ref_count_ = 0;
    uint64_t id_;
    Node* const parent_;
    const Interned<Frame> location_;
    base::LookupSet<Node, const Interned<Frame>, &Node::location_> children_;
  };

  GlobalCallstackTrie() = default;
  ~GlobalCallstackTrie() = default;
  GlobalCallstackTrie(const GlobalCallstackTrie&) = delete;
  GlobalCallstackTrie& operator=(const GlobalCallstackTrie&) = delete;

  // Moving this would invalidate the back pointers to the root_ node.
  GlobalCallstackTrie(GlobalCallstackTrie&&) = delete;
  GlobalCallstackTrie& operator=(GlobalCallstackTrie&&) = delete;

  Interned<Frame> InternCodeLocation(const FrameData& loc);

  Node* CreateCallsite(const std::vector<FrameData>& callstack);
  Node* CreateCallsite(const std::vector<Interned<Frame>>& callstack);

  static void IncrementNode(Node* node);
  static void DecrementNode(Node* node);

  std::vector<Interned<Frame>> BuildInverseCallstack(const Node* node) const;

  // Purges all interned callstacks (and the associated internings), without
  // restarting any interning sequences. Incompatible with external refcounting
  // of nodes (Node.ref_count_).
  void ClearTrie() {
    PERFETTO_DLOG("Clearing trie");
    root_.DeleteChildren();
  }

 private:
  Node* GetOrCreateChild(Node* self, const Interned<Frame>& loc);

  Interned<Frame> MakeRootFrame();

  Interner<std::string> string_interner_;
  Interner<Mapping> mapping_interner_;
  Interner<Frame> frame_interner_;

  uint64_t next_callstack_id_ = 0;

  Node root_{MakeRootFrame(), ++next_callstack_id_};
};

}  // namespace profiling
}  // namespace perfetto

template <>
struct std::hash<::perfetto::profiling::Mapping> {
  using argument_type = ::perfetto::profiling::Mapping;
  using result_type = size_t;
  result_type operator()(const argument_type& mapping) {
    size_t h =
        std::hash<::perfetto::profiling::InternID>{}(mapping.build_id.id());
    h ^= std::hash<uint64_t>{}(mapping.exact_offset);
    h ^= std::hash<uint64_t>{}(mapping.start_offset);
    h ^= std::hash<uint64_t>{}(mapping.start);
    h ^= std::hash<uint64_t>{}(mapping.end);
    h ^= std::hash<uint64_t>{}(mapping.load_bias);
    for (const auto& path : mapping.path_components)
      h ^= std::hash<uint64_t>{}(path.id());
    return h;
  }
};

template <>
struct std::hash<::perfetto::profiling::Frame> {
  using argument_type = ::perfetto::profiling::Frame;
  using result_type = size_t;
  result_type operator()(const argument_type& frame) {
    size_t h = std::hash<::perfetto::profiling::InternID>{}(frame.mapping.id());
    h ^= std::hash<::perfetto::profiling::InternID>{}(frame.function_name.id());
    h ^= std::hash<uint64_t>{}(frame.rel_pc);
    return h;
  }
};

#endif  // SRC_PROFILING_COMMON_CALLSTACK_TRIE_H_
