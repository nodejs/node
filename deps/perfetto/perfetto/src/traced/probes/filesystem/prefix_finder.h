/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SRC_TRACED_PROBES_FILESYSTEM_PREFIX_FINDER_H_
#define SRC_TRACED_PROBES_FILESYSTEM_PREFIX_FINDER_H_

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/lookup_set.h"

namespace perfetto {

// PrefixFinder allows to find prefixes for filenames that ensure that
// they can be found again within a provided limit of steps when searching
// within that prefix in the same order.
//
// For instance, let the limit be 4 and the filesystem be.
// /a/1
// /a/2
// /a/3
// /b/4
// /b/5
//
// The prefix for /a/1, /a/2 and /a/3/ is /, the one for /b/4 and /b/5 is /b/.
class PrefixFinder {
 public:
  // Opaque placeholder for a prefix that can be turned into a string
  // using ToString.
  // Can not be constructed outside of PrefixFinder.
  class Node {
   public:
    friend class PrefixFinder;
    Node(std::string name) : Node(std::move(name), nullptr) {}
    Node(std::string name, Node* parent)
        : name_(std::move(name)), parent_(parent) {}

    Node(const Node& that) = delete;
    Node& operator=(const Node&) = delete;

    // Return string representation of prefix, e.g. /foo/bar.
    // Does not include a trailing /.
    std::string ToString() const;

   private:
    // Add a new child to this node.
    Node* AddChild(std::string name);

    // Get existing child for this node. Return nullptr if it
    // does not exist.
    Node* MaybeChild(const std::string& name);

    const std::string name_;
    const Node* parent_;
    base::LookupSet<Node, const std::string, &Node::name_> children_;
  };

  PrefixFinder(size_t limit);

  // Add path to prefix mapping.
  // Must be called in DFS order.
  // Must be called before GetPrefix(path) for the same path.
  // Must not be called after Finalize.
  void AddPath(std::string path);

  // Return identifier for prefix. Ownership remains with the PrefixFinder.
  // Must be called after AddPath(path) for the same path.
  // Must not be before after Finalize.
  Node* GetPrefix(std::string path);

  // Call this after the last AddPath and before the first GetPrefix.
  void Finalize();

 private:
  // We're about to remove the suffix of state from i onwards,
  // if necessary add a prefix for anything in that suffix.
  void Flush(size_t i);
  void InsertPrefix(size_t len);
  const size_t limit_;
  // (path element, count) tuples for last path seen.
  std::vector<std::pair<std::string, size_t>> state_{{"", 0}};
  Node root_{"", nullptr};
#if PERFETTO_DCHECK_IS_ON()
  bool finalized_ = false;
#endif
};

}  // namespace perfetto
#endif  // SRC_TRACED_PROBES_FILESYSTEM_PREFIX_FINDER_H_
