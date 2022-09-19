// Copyright 2009 the V8 project authors. All rights reserved.
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


/**
 * Constructs a Splay tree.  A splay tree is a self-balancing binary
 * search tree with the additional property that recently accessed
 * elements are quick to access again. It performs basic operations
 * such as insertion, look-up and removal in O(log(n)) amortized time.
 *
 * @constructor
 */
export class SplayTree {

  /**
   * Pointer to the root node of the tree.
   *
   * @type {SplayTreeNode}
   * @private
   */
  root_ = null;


  /**
   * @return {boolean} Whether the tree is empty.
   */
  isEmpty() {
    return this.root_ === null;
  }

  /**
   * Inserts a node into the tree with the specified key and value if
   * the tree does not already contain a node with the specified key. If
   * the value is inserted, it becomes the root of the tree.
   *
   * @param {number} key Key to insert into the tree.
   * @param {*} value Value to insert into the tree.
   */
  insert(key, value) {
    if (this.isEmpty()) {
      this.root_ = new SplayTreeNode(key, value);
      return;
    }
    // Splay on the key to move the last node on the search path for
    // the key to the root of the tree.
    this.splay_(key);
    if (this.root_.key == key) return;

    const node = new SplayTreeNode(key, value);
    if (key > this.root_.key) {
      node.left = this.root_;
      node.right = this.root_.right;
      this.root_.right = null;
    } else {
      node.right = this.root_;
      node.left = this.root_.left;
      this.root_.left = null;
    }
    this.root_ = node;
  }

  /**
   * Removes a node with the specified key from the tree if the tree
   * contains a node with this key. The removed node is returned. If the
   * key is not found, an exception is thrown.
   *
   * @param {number} key Key to find and remove from the tree.
   * @return {SplayTreeNode} The removed node.
   */
  remove(key) {
    if (this.isEmpty()) {
      throw Error(`Key not found: ${key}`);
    }
    this.splay_(key);
    if (this.root_.key != key) {
      throw Error(`Key not found: ${key}`);
    }
    const removed = this.root_;
    if (this.root_.left === null) {
      this.root_ = this.root_.right;
    } else {
      const { right } = this.root_;
      this.root_ = this.root_.left;
      // Splay to make sure that the new root has an empty right child.
      this.splay_(key);
      // Insert the original right child as the right child of the new
      // root.
      this.root_.right = right;
    }
    return removed;
  }

  /**
   * Returns the node having the specified key or null if the tree doesn't contain
   * a node with the specified key.
   *
   * @param {number} key Key to find in the tree.
   * @return {SplayTreeNode} Node having the specified key.
   */
  find(key) {
    if (this.isEmpty()) return null;
    this.splay_(key);
    return this.root_.key == key ? this.root_ : null;
  }

  /**
   * @return {SplayTreeNode} Node having the minimum key value.
   */
  findMin() {
    if (this.isEmpty()) return null;
    let current = this.root_;
    while (current.left !== null) {
      current = current.left;
    }
    return current;
  }

  /**
   * @return {SplayTreeNode} Node having the maximum key value.
   */
  findMax(opt_startNode) {
    if (this.isEmpty()) return null;
    let current = opt_startNode || this.root_;
    while (current.right !== null) {
      current = current.right;
    }
    return current;
  }

  /**
   * @return {SplayTreeNode} Node having the maximum key value that
   *     is less or equal to the specified key value.
   */
  findGreatestLessThan(key) {
    if (this.isEmpty()) return null;
    // Splay on the key to move the node with the given key or the last
    // node on the search path to the top of the tree.
    this.splay_(key);
    // Now the result is either the root node or the greatest node in
    // the left subtree.
    if (this.root_.key <= key) {
      return this.root_;
    } else if (this.root_.left !== null) {
      return this.findMax(this.root_.left);
    } else {
      return null;
    }
  }

  /**
   * @return {Array<*>} An array containing all the values of tree's nodes paired
   *     with keys.
   */
  exportKeysAndValues() {
    const result = [];
    this.traverse_(function(node) { result.push([node.key, node.value]); });
    return result;
  }

  /**
   * @return {Array<*>} An array containing all the values of tree's nodes.
   */
  exportValues() {
    const result = [];
    this.traverse_(function(node) { result.push(node.value) });
    return result;
  }

  /**
   * Perform the splay operation for the given key. Moves the node with
   * the given key to the top of the tree.  If no node has the given
   * key, the last node on the search path is moved to the top of the
   * tree. This is the simplified top-down splaying algorithm from:
   * "Self-adjusting Binary Search Trees" by Sleator and Tarjan
   *
   * @param {number} key Key to splay the tree on.
   * @private
   */
  splay_(key) {
    if (this.isEmpty()) return;
    // Create a dummy node.  The use of the dummy node is a bit
    // counter-intuitive: The right child of the dummy node will hold
    // the L tree of the algorithm.  The left child of the dummy node
    // will hold the R tree of the algorithm.  Using a dummy node, left
    // and right will always be nodes and we avoid special cases.
    let dummy, left, right;
    dummy = left = right = new SplayTreeNode(null, null);
    let current = this.root_;
    while (true) {
      if (key < current.key) {
        if (current.left === null) break;
        if (key < current.left.key) {
          // Rotate right.
          const tmp = current.left;
          current.left = tmp.right;
          tmp.right = current;
          current = tmp;
          if (current.left === null) break;
        }
        // Link right.
        right.left = current;
        right = current;
        current = current.left;
      } else if (key > current.key) {
        if (current.right === null) break;
        if (key > current.right.key) {
          // Rotate left.
          const tmp = current.right;
          current.right = tmp.left;
          tmp.left = current;
          current = tmp;
          if (current.right === null) break;
        }
        // Link left.
        left.right = current;
        left = current;
        current = current.right;
      } else {
        break;
      }
    }
    // Assemble.
    left.right = current.left;
    right.left = current.right;
    current.left = dummy.right;
    current.right = dummy.left;
    this.root_ = current;
  }

  /**
   * Performs a preorder traversal of the tree.
   *
   * @param {function(SplayTreeNode)} f Visitor function.
   * @private
   */
  traverse_(f) {
    const nodesToVisit = [this.root_];
    while (nodesToVisit.length > 0) {
      const node = nodesToVisit.shift();
      if (node === null) continue;
      f(node);
      nodesToVisit.push(node.left);
      nodesToVisit.push(node.right);
    }
  }
}

/**
 * Constructs a Splay tree node.
 *
 * @param {number} key Key.
 * @param {*} value Value.
 */
class SplayTreeNode {
  constructor(key, value) {
    this.key = key;
    this.value = value;
    /**
     * @type {SplayTreeNode}
     */
    this.left = null;
    /**
     * @type {SplayTreeNode}
     */
    this.right = null;
  }
};
