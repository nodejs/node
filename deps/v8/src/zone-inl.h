// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_ZONE_INL_H_
#define V8_ZONE_INL_H_

#include "zone.h"
#include "v8-counters.h"

namespace v8 {
namespace internal {


inline void* Zone::New(int size) {
  ASSERT(AssertNoZoneAllocation::allow_allocation());
  ASSERT(ZoneScope::nesting() > 0);
  // Round up the requested size to fit the alignment.
  size = RoundUp(size, kAlignment);

  // Check if the requested size is available without expanding.
  Address result = position_;
  if ((position_ += size) > limit_) result = NewExpand(size);

  // Check that the result has the proper alignment and return it.
  ASSERT(IsAddressAligned(result, kAlignment, 0));
  return reinterpret_cast<void*>(result);
}


template <typename T>
T* Zone::NewArray(int length) {
  return static_cast<T*>(Zone::New(length * sizeof(T)));
}


bool Zone::excess_allocation() {
  return segment_bytes_allocated_ > zone_excess_limit_;
}


void Zone::adjust_segment_bytes_allocated(int delta) {
  segment_bytes_allocated_ += delta;
  Counters::zone_segment_bytes.Set(segment_bytes_allocated_);
}


template <typename C>
bool ZoneSplayTree<C>::Insert(const Key& key, Locator* locator) {
  if (is_empty()) {
    // If the tree is empty, insert the new node.
    root_ = new Node(key, C::kNoValue);
  } else {
    // Splay on the key to move the last node on the search path
    // for the key to the root of the tree.
    Splay(key);
    // Ignore repeated insertions with the same key.
    int cmp = C::Compare(key, root_->key_);
    if (cmp == 0) {
      locator->bind(root_);
      return false;
    }
    // Insert the new node.
    Node* node = new Node(key, C::kNoValue);
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
  locator->bind(root_);
  return true;
}


template <typename C>
bool ZoneSplayTree<C>::Find(const Key& key, Locator* locator) {
  if (is_empty())
    return false;
  Splay(key);
  if (C::Compare(key, root_->key_) == 0) {
    locator->bind(root_);
    return true;
  } else {
    return false;
  }
}


template <typename C>
bool ZoneSplayTree<C>::FindGreatestLessThan(const Key& key,
                                            Locator* locator) {
  if (is_empty())
    return false;
  // Splay on the key to move the node with the given key or the last
  // node on the search path to the top of the tree.
  Splay(key);
  // Now the result is either the root node or the greatest node in
  // the left subtree.
  int cmp = C::Compare(root_->key_, key);
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


template <typename C>
bool ZoneSplayTree<C>::FindLeastGreaterThan(const Key& key,
                                            Locator* locator) {
  if (is_empty())
    return false;
  // Splay on the key to move the node with the given key or the last
  // node on the search path to the top of the tree.
  Splay(key);
  // Now the result is either the root node or the least node in
  // the right subtree.
  int cmp = C::Compare(root_->key_, key);
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


template <typename C>
bool ZoneSplayTree<C>::FindGreatest(Locator* locator) {
  if (is_empty())
    return false;
  Node* current = root_;
  while (current->right_ != NULL)
    current = current->right_;
  locator->bind(current);
  return true;
}


template <typename C>
bool ZoneSplayTree<C>::FindLeast(Locator* locator) {
  if (is_empty())
    return false;
  Node* current = root_;
  while (current->left_ != NULL)
    current = current->left_;
  locator->bind(current);
  return true;
}


template <typename C>
bool ZoneSplayTree<C>::Remove(const Key& key) {
  // Bail if the tree is empty
  if (is_empty())
    return false;
  // Splay on the key to move the node with the given key to the top.
  Splay(key);
  // Bail if the key is not in the tree
  if (C::Compare(key, root_->key_) != 0)
    return false;
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
  return true;
}


template <typename C>
void ZoneSplayTree<C>::Splay(const Key& key) {
  if (is_empty())
    return;
  Node dummy_node(C::kNoKey, C::kNoValue);
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
    int cmp = C::Compare(key, current->key_);
    if (cmp < 0) {
      if (current->left_ == NULL)
        break;
      if (C::Compare(key, current->left_->key_) < 0) {
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
      if (C::Compare(key, current->right_->key_) > 0) {
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


template <typename Config> template <class Callback>
void ZoneSplayTree<Config>::ForEach(Callback* callback) {
  // Pre-allocate some space for tiny trees.
  ZoneList<Node*> nodes_to_visit(10);
  nodes_to_visit.Add(root_);
  int pos = 0;
  while (pos < nodes_to_visit.length()) {
    Node* node = nodes_to_visit[pos++];
    if (node == NULL) continue;
    callback->Call(node->key(), node->value());
    nodes_to_visit.Add(node->left());
    nodes_to_visit.Add(node->right());
  }
}


} }  // namespace v8::internal

#endif  // V8_ZONE_INL_H_
