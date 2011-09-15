// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_SPLAY_TREE_H_
#define V8_SPLAY_TREE_H_

#include "allocation.h"

namespace v8 {
namespace internal {


// A splay tree.  The config type parameter encapsulates the different
// configurations of a concrete splay tree:
//
//   typedef Key: the key type
//   typedef Value: the value type
//   static const kNoKey: the dummy key used when no key is set
//   static const kNoValue: the dummy value used to initialize nodes
//   int (Compare)(Key& a, Key& b) -> {-1, 0, 1}: comparison function
//
// The tree is also parameterized by an allocation policy
// (Allocator). The policy is used for allocating lists in the C free
// store or the zone; see zone.h.

// Forward defined as
// template <typename Config, class Allocator = FreeStoreAllocationPolicy>
//     class SplayTree;
template <typename Config, class Allocator>
class SplayTree {
 public:
  typedef typename Config::Key Key;
  typedef typename Config::Value Value;

  class Locator;

  SplayTree() : root_(NULL) { }
  ~SplayTree();

  INLINE(void* operator new(size_t size)) {
    return Allocator::New(static_cast<int>(size));
  }
  INLINE(void operator delete(void* p, size_t)) { return Allocator::Delete(p); }

  // Inserts the given key in this tree with the given value.  Returns
  // true if a node was inserted, otherwise false.  If found the locator
  // is enabled and provides access to the mapping for the key.
  bool Insert(const Key& key, Locator* locator);

  // Looks up the key in this tree and returns true if it was found,
  // otherwise false.  If the node is found the locator is enabled and
  // provides access to the mapping for the key.
  bool Find(const Key& key, Locator* locator);

  // Finds the mapping with the greatest key less than or equal to the
  // given key.
  bool FindGreatestLessThan(const Key& key, Locator* locator);

  // Find the mapping with the greatest key in this tree.
  bool FindGreatest(Locator* locator);

  // Finds the mapping with the least key greater than or equal to the
  // given key.
  bool FindLeastGreaterThan(const Key& key, Locator* locator);

  // Find the mapping with the least key in this tree.
  bool FindLeast(Locator* locator);

  // Move the node from one key to another.
  bool Move(const Key& old_key, const Key& new_key);

  // Remove the node with the given key from the tree.
  bool Remove(const Key& key);

  bool is_empty() { return root_ == NULL; }

  // Perform the splay operation for the given key. Moves the node with
  // the given key to the top of the tree.  If no node has the given
  // key, the last node on the search path is moved to the top of the
  // tree.
  void Splay(const Key& key);

  class Node {
   public:
    Node(const Key& key, const Value& value)
        : key_(key),
          value_(value),
          left_(NULL),
          right_(NULL) { }

    INLINE(void* operator new(size_t size)) {
      return Allocator::New(static_cast<int>(size));
    }
    INLINE(void operator delete(void* p, size_t)) {
      return Allocator::Delete(p);
    }

    Key key() { return key_; }
    Value value() { return value_; }
    Node* left() { return left_; }
    Node* right() { return right_; }

   private:
    friend class SplayTree;
    friend class Locator;
    Key key_;
    Value value_;
    Node* left_;
    Node* right_;
  };

  // A locator provides access to a node in the tree without actually
  // exposing the node.
  class Locator BASE_EMBEDDED {
   public:
    explicit Locator(Node* node) : node_(node) { }
    Locator() : node_(NULL) { }
    const Key& key() { return node_->key_; }
    Value& value() { return node_->value_; }
    void set_value(const Value& value) { node_->value_ = value; }
    inline void bind(Node* node) { node_ = node; }

   private:
    Node* node_;
  };

  template <class Callback>
  void ForEach(Callback* callback);

 protected:
  // Resets tree root. Existing nodes become unreachable.
  void ResetRoot() { root_ = NULL; }

 private:
  // Search for a node with a given key. If found, root_ points
  // to the node.
  bool FindInternal(const Key& key);

  // Inserts a node assuming that root_ is already set up.
  void InsertInternal(int cmp, Node* node);

  // Removes root_ node.
  void RemoveRootNode(const Key& key);

  template<class Callback>
  class NodeToPairAdaptor BASE_EMBEDDED {
   public:
    explicit NodeToPairAdaptor(Callback* callback)
        : callback_(callback) { }
    void Call(Node* node) {
      callback_->Call(node->key(), node->value());
    }

   private:
    Callback* callback_;

    DISALLOW_COPY_AND_ASSIGN(NodeToPairAdaptor);
  };

  class NodeDeleter BASE_EMBEDDED {
   public:
    NodeDeleter() { }
    void Call(Node* node) { delete node; }

   private:
    DISALLOW_COPY_AND_ASSIGN(NodeDeleter);
  };

  template <class Callback>
  void ForEachNode(Callback* callback);

  Node* root_;

  DISALLOW_COPY_AND_ASSIGN(SplayTree);
};


} }  // namespace v8::internal

#endif  // V8_SPLAY_TREE_H_
