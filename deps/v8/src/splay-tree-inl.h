// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SPLAY_TREE_INL_H_
#define V8_SPLAY_TREE_INL_H_

#include "splay-tree.h"

namespace v8 {
namespace internal {


template<typename Config, class Allocator>
SplayTree<Config, Allocator>::~SplayTree() {
  NodeDeleter deleter;
  ForEachNode(&deleter);
}


template<typename Config, class Allocator>
bool SplayTree<Config, Allocator>::Insert(const Key& key,
                                          Locator* locator) {
  if (is_empty()) {
    // If the tree is empty, insert the new node.
    root_ = new(allocator_) Node(key, Config::NoValue());
  } else {
    // Splay on the key to move the last node on the search path
    // for the key to the root of the tree.
    Splay(key);
    // Ignore repeated insertions with the same key.
    int cmp = Config::Compare(key, root_->key_);
    if (cmp == 0) {
      locator->bind(root_);
      return false;
    }
    // Insert the new node.
    Node* node = new(allocator_) Node(key, Config::NoValue());
    InsertInternal(cmp, node);
  }
  locator->bind(root_);
  return true;
}


template<typename Config, class Allocator>
void SplayTree<Config, Allocator>::InsertInternal(int cmp, Node* node) {
  if (cmp > 0) {
    node->left_ = root_;
    node->right_ = root_->right_;
    root_->right_ = NULL;
  } else {
    node->right_ = root_;
    node->left_ = root_->left_;
    root_->left_ = NULL;
  }
  root_ = node;
}


template<typename Config, class Allocator>
bool SplayTree<Config, Allocator>::FindInternal(const Key& key) {
  if (is_empty())
    return false;
  Splay(key);
  return Config::Compare(key, root_->key_) == 0;
}


template<typename Config, class Allocator>
bool SplayTree<Config, Allocator>::Contains(const Key& key) {
  return FindInternal(key);
}


template<typename Config, class Allocator>
bool SplayTree<Config, Allocator>::Find(const Key& key, Locator* locator) {
  if (FindInternal(key)) {
    locator->bind(root_);
    return true;
  } else {
    return false;
  }
}


template<typename Config, class Allocator>
bool SplayTree<Config, Allocator>::FindGreatestLessThan(const Key& key,
                                                        Locator* locator) {
  if (is_empty())
    return false;
  // Splay on the key to move the node with the given key or the last
  // node on the search path to the top of the tree.
  Splay(key);
  // Now the result is either the root node or the greatest node in
  // the left subtree.
  int cmp = Config::Compare(root_->key_, key);
  if (cmp <= 0) {
    locator->bind(root_);
    return true;
  } else {
    Node* temp = root_;
    root_ = root_->left_;
    bool result = FindGreatest(locator);
    root_ = temp;
    return result;
  }
}


template<typename Config, class Allocator>
bool SplayTree<Config, Allocator>::FindLeastGreaterThan(const Key& key,
                                                        Locator* locator) {
  if (is_empty())
    return false;
  // Splay on the key to move the node with the given key or the last
  // node on the search path to the top of the tree.
  Splay(key);
  // Now the result is either the root node or the least node in
  // the right subtree.
  int cmp = Config::Compare(root_->key_, key);
  if (cmp >= 0) {
    locator->bind(root_);
    return true;
  } else {
    Node* temp = root_;
    root_ = root_->right_;
    bool result = FindLeast(locator);
    root_ = temp;
    return result;
  }
}


template<typename Config, class Allocator>
bool SplayTree<Config, Allocator>::FindGreatest(Locator* locator) {
  if (is_empty())
    return false;
  Node* current = root_;
  while (current->right_ != NULL)
    current = current->right_;
  locator->bind(current);
  return true;
}


template<typename Config, class Allocator>
bool SplayTree<Config, Allocator>::FindLeast(Locator* locator) {
  if (is_empty())
    return false;
  Node* current = root_;
  while (current->left_ != NULL)
    current = current->left_;
  locator->bind(current);
  return true;
}


template<typename Config, class Allocator>
bool SplayTree<Config, Allocator>::Move(const Key& old_key,
                                        const Key& new_key) {
  if (!FindInternal(old_key))
    return false;
  Node* node_to_move = root_;
  RemoveRootNode(old_key);
  Splay(new_key);
  int cmp = Config::Compare(new_key, root_->key_);
  if (cmp == 0) {
    // A node with the target key already exists.
    delete node_to_move;
    return false;
  }
  node_to_move->key_ = new_key;
  InsertInternal(cmp, node_to_move);
  return true;
}


template<typename Config, class Allocator>
bool SplayTree<Config, Allocator>::Remove(const Key& key) {
  if (!FindInternal(key))
    return false;
  Node* node_to_remove = root_;
  RemoveRootNode(key);
  delete node_to_remove;
  return true;
}


template<typename Config, class Allocator>
void SplayTree<Config, Allocator>::RemoveRootNode(const Key& key) {
  if (root_->left_ == NULL) {
    // No left child, so the new tree is just the right child.
    root_ = root_->right_;
  } else {
    // Left child exists.
    Node* right = root_->right_;
    // Make the original left child the new root.
    root_ = root_->left_;
    // Splay to make sure that the new root has an empty right child.
    Splay(key);
    // Insert the original right child as the right child of the new
    // root.
    root_->right_ = right;
  }
}


template<typename Config, class Allocator>
void SplayTree<Config, Allocator>::Splay(const Key& key) {
  if (is_empty())
    return;
  Node dummy_node(Config::kNoKey, Config::NoValue());
  // Create a dummy node.  The use of the dummy node is a bit
  // counter-intuitive: The right child of the dummy node will hold
  // the L tree of the algorithm.  The left child of the dummy node
  // will hold the R tree of the algorithm.  Using a dummy node, left
  // and right will always be nodes and we avoid special cases.
  Node* dummy = &dummy_node;
  Node* left = dummy;
  Node* right = dummy;
  Node* current = root_;
  while (true) {
    int cmp = Config::Compare(key, current->key_);
    if (cmp < 0) {
      if (current->left_ == NULL)
        break;
      if (Config::Compare(key, current->left_->key_) < 0) {
        // Rotate right.
        Node* temp = current->left_;
        current->left_ = temp->right_;
        temp->right_ = current;
        current = temp;
        if (current->left_ == NULL)
          break;
      }
      // Link right.
      right->left_ = current;
      right = current;
      current = current->left_;
    } else if (cmp > 0) {
      if (current->right_ == NULL)
        break;
      if (Config::Compare(key, current->right_->key_) > 0) {
        // Rotate left.
        Node* temp = current->right_;
        current->right_ = temp->left_;
        temp->left_ = current;
        current = temp;
        if (current->right_ == NULL)
          break;
      }
      // Link left.
      left->right_ = current;
      left = current;
      current = current->right_;
    } else {
      break;
    }
  }
  // Assemble.
  left->right_ = current->left_;
  right->left_ = current->right_;
  current->left_ = dummy->right_;
  current->right_ = dummy->left_;
  root_ = current;
}


template <typename Config, class Allocator> template <class Callback>
void SplayTree<Config, Allocator>::ForEach(Callback* callback) {
  NodeToPairAdaptor<Callback> callback_adaptor(callback);
  ForEachNode(&callback_adaptor);
}


template <typename Config, class Allocator> template <class Callback>
void SplayTree<Config, Allocator>::ForEachNode(Callback* callback) {
  if (root_ == NULL) return;
  // Pre-allocate some space for tiny trees.
  List<Node*, Allocator> nodes_to_visit(10, allocator_);
  nodes_to_visit.Add(root_, allocator_);
  int pos = 0;
  while (pos < nodes_to_visit.length()) {
    Node* node = nodes_to_visit[pos++];
    if (node->left() != NULL) nodes_to_visit.Add(node->left(), allocator_);
    if (node->right() != NULL) nodes_to_visit.Add(node->right(), allocator_);
    callback->Call(node);
  }
}


} }  // namespace v8::internal

#endif  // V8_SPLAY_TREE_INL_H_
