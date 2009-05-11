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


// Initlialize namespaces
var devtools = devtools || {};
devtools.profiler = devtools.profiler || {};


/**
 * Creates a profile object for processing profiling-related events
 * and calculating function execution times.
 *
 * @constructor
 */
devtools.profiler.Profile = function() {
  this.codeMap_ = new devtools.profiler.CodeMap();
  this.topDownTree_ = new devtools.profiler.CallTree();
  this.bottomUpTree_ = new devtools.profiler.CallTree();
};


/**
 * Returns whether a function with the specified name must be skipped.
 * Should be overriden by subclasses.
 *
 * @param {string} name Function name.
 */
devtools.profiler.Profile.prototype.skipThisFunction = function(name) {
  return false;
};


/**
 * Enum for profiler operations that involve looking up existing
 * code entries.
 *
 * @enum {number}
 */
devtools.profiler.Profile.Operation = {
  MOVE: 0,
  DELETE: 1,
  TICK: 2
};


/**
 * Called whenever the specified operation has failed finding a function
 * containing the specified address. Should be overriden by subclasses.
 * See the devtools.profiler.Profile.Operation enum for the list of
 * possible operations.
 *
 * @param {number} operation Operation.
 * @param {number} addr Address of the unknown code.
 * @param {number} opt_stackPos If an unknown address is encountered
 *     during stack strace processing, specifies a position of the frame
 *     containing the address.
 */
devtools.profiler.Profile.prototype.handleUnknownCode = function(
    operation, addr, opt_stackPos) {
};


/**
 * Registers static (library) code entry.
 *
 * @param {string} name Code entry name.
 * @param {number} startAddr Starting address.
 * @param {number} endAddr Ending address.
 */
devtools.profiler.Profile.prototype.addStaticCode = function(
    name, startAddr, endAddr) {
  var entry = new devtools.profiler.CodeMap.CodeEntry(
      endAddr - startAddr, name);
  this.codeMap_.addStaticCode(startAddr, entry);
  return entry;
};


/**
 * Registers dynamic (JIT-compiled) code entry.
 *
 * @param {string} type Code entry type.
 * @param {string} name Code entry name.
 * @param {number} start Starting address.
 * @param {number} size Code entry size.
 */
devtools.profiler.Profile.prototype.addCode = function(
    type, name, start, size) {
  var entry = new devtools.profiler.Profile.DynamicCodeEntry(size, type, name);
  this.codeMap_.addCode(start, entry);
  return entry;
};


/**
 * Reports about moving of a dynamic code entry.
 *
 * @param {number} from Current code entry address.
 * @param {number} to New code entry address.
 */
devtools.profiler.Profile.prototype.moveCode = function(from, to) {
  try {
    this.codeMap_.moveCode(from, to);
  } catch (e) {
    this.handleUnknownCode(devtools.profiler.Profile.Operation.MOVE, from);
  }
};


/**
 * Reports about deletion of a dynamic code entry.
 *
 * @param {number} start Starting address.
 */
devtools.profiler.Profile.prototype.deleteCode = function(start) {
  try {
    this.codeMap_.deleteCode(start);
  } catch (e) {
    this.handleUnknownCode(devtools.profiler.Profile.Operation.DELETE, start);
  }
};


/**
 * Records a tick event. Stack must contain a sequence of
 * addresses starting with the program counter value.
 *
 * @param {Array<number>} stack Stack sample.
 */
devtools.profiler.Profile.prototype.recordTick = function(stack) {
  var processedStack = this.resolveAndFilterFuncs_(stack);
  this.bottomUpTree_.addPath(processedStack);
  processedStack.reverse();
  this.topDownTree_.addPath(processedStack);
};


/**
 * Translates addresses into function names and filters unneeded
 * functions.
 *
 * @param {Array<number>} stack Stack sample.
 */
devtools.profiler.Profile.prototype.resolveAndFilterFuncs_ = function(stack) {
  var result = [];
  for (var i = 0; i < stack.length; ++i) {
    var entry = this.codeMap_.findEntry(stack[i]);
    if (entry) {
      var name = entry.getName();
      if (!this.skipThisFunction(name)) {
        result.push(name);
      }
    } else {
      this.handleUnknownCode(
          devtools.profiler.Profile.Operation.TICK, stack[i], i);
    }
  }
  return result;
};


/**
 * Performs a BF traversal of the top down call graph.
 *
 * @param {function(devtools.profiler.CallTree.Node)} f Visitor function.
 */
devtools.profiler.Profile.prototype.traverseTopDownTree = function(f) {
  this.topDownTree_.traverse(f);
};


/**
 * Performs a BF traversal of the bottom up call graph.
 *
 * @param {function(devtools.profiler.CallTree.Node)} f Visitor function.
 */
devtools.profiler.Profile.prototype.traverseBottomUpTree = function(f) {
  this.bottomUpTree_.traverse(f);
};


/**
 * Calculates a top down profile for a node with the specified label.
 * If no name specified, returns the whole top down calls tree.
 *
 * @param {string} opt_label Node label.
 */
devtools.profiler.Profile.prototype.getTopDownProfile = function(opt_label) {
  return this.getTreeProfile_(this.topDownTree_, opt_label);
};


/**
 * Calculates a bottom up profile for a node with the specified label.
 * If no name specified, returns the whole bottom up calls tree.
 *
 * @param {string} opt_label Node label.
 */
devtools.profiler.Profile.prototype.getBottomUpProfile = function(opt_label) {
  return this.getTreeProfile_(this.bottomUpTree_, opt_label);
};


/**
 * Helper function for calculating a tree profile.
 *
 * @param {devtools.profiler.Profile.CallTree} tree Call tree.
 * @param {string} opt_label Node label.
 */
devtools.profiler.Profile.prototype.getTreeProfile_ = function(tree, opt_label) {
  if (!opt_label) {
    tree.computeTotalWeights();
    return tree;
  } else {
    var subTree = tree.cloneSubtree(opt_label);
    subTree.computeTotalWeights();
    return subTree;
  }
};


/**
 * Calculates a flat profile of callees starting from a node with
 * the specified label. If no name specified, starts from the root.
 *
 * @param {string} opt_label Starting node label.
 */
devtools.profiler.Profile.prototype.getFlatProfile = function(opt_label) {
  var counters = new devtools.profiler.CallTree();
  var rootLabel = opt_label || devtools.profiler.CallTree.ROOT_NODE_LABEL;
  var precs = {};
  precs[rootLabel] = 0;
  var root = counters.findOrAddChild(rootLabel);

  this.topDownTree_.computeTotalWeights();
  this.topDownTree_.traverseInDepth(
    function onEnter(node) {
      if (!(node.label in precs)) {
        precs[node.label] = 0;
      }
      var nodeLabelIsRootLabel = node.label == rootLabel;
      if (nodeLabelIsRootLabel || precs[rootLabel] > 0) {
        if (precs[rootLabel] == 0) {
          root.selfWeight += node.selfWeight;
          root.totalWeight += node.totalWeight;
        } else {
          var rec = root.findOrAddChild(node.label);
          rec.selfWeight += node.selfWeight;
          if (nodeLabelIsRootLabel || precs[node.label] == 0) {
            rec.totalWeight += node.totalWeight;
          }
        }
        precs[node.label]++;
      }
    },
    function onExit(node) {
      if (node.label == rootLabel || precs[rootLabel] > 0) {
        precs[node.label]--;
      }
    },
    null);

  if (!opt_label) {
    // If we have created a flat profile for the whole program, we don't
    // need an explicit root in it. Thus, replace the counters tree
    // root with the node corresponding to the whole program.
    counters.root_ = root;
  } else {
    // Propagate weights so percents can be calculated correctly.
    counters.getRoot().selfWeight = root.selfWeight;
    counters.getRoot().totalWeight = root.totalWeight;
  }
  return counters;
};


/**
 * Creates a dynamic code entry.
 *
 * @param {number} size Code size.
 * @param {string} type Code type.
 * @param {string} name Function name.
 * @constructor
 */
devtools.profiler.Profile.DynamicCodeEntry = function(size, type, name) {
  devtools.profiler.CodeMap.CodeEntry.call(this, size, name);
  this.type = type;
};


/**
 * Returns node name.
 */
devtools.profiler.Profile.DynamicCodeEntry.prototype.getName = function() {
  var name = this.name;
  if (name.length == 0) {
    name = '<anonymous>';
  } else if (name.charAt(0) == ' ') {
    // An anonymous function with location: " aaa.js:10".
    name = '<anonymous>' + name;
  }
  return this.type + ': ' + name;
};


/**
 * Constructs a call graph.
 *
 * @constructor
 */
devtools.profiler.CallTree = function() {
  this.root_ = new devtools.profiler.CallTree.Node(
      devtools.profiler.CallTree.ROOT_NODE_LABEL);
};


/**
 * The label of the root node.
 */
devtools.profiler.CallTree.ROOT_NODE_LABEL = '';


/**
 * @private
 */
devtools.profiler.CallTree.prototype.totalsComputed_ = false;


/**
 * Returns the tree root.
 */
devtools.profiler.CallTree.prototype.getRoot = function() {
  return this.root_;
};


/**
 * Adds the specified call path, constructing nodes as necessary.
 *
 * @param {Array<string>} path Call path.
 */
devtools.profiler.CallTree.prototype.addPath = function(path) {
  if (path.length == 0) {
    return;
  }
  var curr = this.root_;
  for (var i = 0; i < path.length; ++i) {
    curr = curr.findOrAddChild(path[i]);
  }
  curr.selfWeight++;
  this.totalsComputed_ = false;
};


/**
 * Finds an immediate child of the specified parent with the specified
 * label, creates a child node if necessary. If a parent node isn't
 * specified, uses tree root.
 *
 * @param {string} label Child node label.
 */
devtools.profiler.CallTree.prototype.findOrAddChild = function(label) {
  return this.root_.findOrAddChild(label);
};


/**
 * Creates a subtree by cloning and merging all subtrees rooted at nodes
 * with a given label. E.g. cloning the following call tree on label 'A'
 * will give the following result:
 *
 *           <A>--<B>                                     <B>
 *          /                                            /
 *     <root>             == clone on 'A' ==>  <root>--<A>
 *          \                                            \
 *           <C>--<A>--<D>                                <D>
 *
 * And <A>'s selfWeight will be the sum of selfWeights of <A>'s from the
 * source call tree.
 *
 * @param {string} label The label of the new root node.
 */
devtools.profiler.CallTree.prototype.cloneSubtree = function(label) {
  var subTree = new devtools.profiler.CallTree();
  this.traverse(function(node, parent) {
    if (!parent && node.label != label) {
      return null;
    }
    var child = (parent ? parent : subTree).findOrAddChild(node.label);
    child.selfWeight += node.selfWeight;
    return child;
  });
  return subTree;
};


/**
 * Computes total weights in the call graph.
 */
devtools.profiler.CallTree.prototype.computeTotalWeights = function() {
  if (this.totalsComputed_) {
    return;
  }
  this.root_.computeTotalWeight();
  this.totalsComputed_ = true;
};


/**
 * Traverses the call graph in preorder. This function can be used for
 * building optionally modified tree clones. This is the boilerplate code
 * for this scenario:
 *
 * callTree.traverse(function(node, parentClone) {
 *   var nodeClone = cloneNode(node);
 *   if (parentClone)
 *     parentClone.addChild(nodeClone);
 *   return nodeClone;
 * });
 *
 * @param {function(devtools.profiler.CallTree.Node, *)} f Visitor function.
 *    The second parameter is the result of calling 'f' on the parent node.
 */
devtools.profiler.CallTree.prototype.traverse = function(f) {
  var pairsToProcess = new ConsArray();
  pairsToProcess.concat([{node: this.root_, param: null}]);
  while (!pairsToProcess.atEnd()) {
    var pair = pairsToProcess.next();
    var node = pair.node;
    var newParam = f(node, pair.param);
    var morePairsToProcess = [];
    node.forEachChild(function (child) {
        morePairsToProcess.push({node: child, param: newParam}); });
    pairsToProcess.concat(morePairsToProcess);
  }
};


/**
 * Performs an indepth call graph traversal.
 *
 * @param {function(devtools.profiler.CallTree.Node)} enter A function called
 *     prior to visiting node's children.
 * @param {function(devtools.profiler.CallTree.Node)} exit A function called
 *     after visiting node's children.
 */
devtools.profiler.CallTree.prototype.traverseInDepth = function(enter, exit) {
  function traverse(node) {
    enter(node);
    node.forEachChild(traverse);
    exit(node);
  }
  traverse(this.root_);
};


/**
 * Constructs a call graph node.
 *
 * @param {string} label Node label.
 * @param {devtools.profiler.CallTree.Node} opt_parent Node parent.
 */
devtools.profiler.CallTree.Node = function(label, opt_parent) {
  this.label = label;
  this.parent = opt_parent;
  this.children = {};
};


/**
 * Node self weight (how many times this node was the last node in
 * a call path).
 * @type {number}
 */
devtools.profiler.CallTree.Node.prototype.selfWeight = 0;


/**
 * Node total weight (includes weights of all children).
 * @type {number}
 */
devtools.profiler.CallTree.Node.prototype.totalWeight = 0;


/**
 * Adds a child node.
 *
 * @param {string} label Child node label.
 */
devtools.profiler.CallTree.Node.prototype.addChild = function(label) {
  var child = new devtools.profiler.CallTree.Node(label, this);
  this.children[label] = child;
  return child;
};


/**
 * Computes node's total weight.
 */
devtools.profiler.CallTree.Node.prototype.computeTotalWeight =
    function() {
  var totalWeight = this.selfWeight;
  this.forEachChild(function(child) {
      totalWeight += child.computeTotalWeight(); });
  return this.totalWeight = totalWeight;
};


/**
 * Returns all node's children as an array.
 */
devtools.profiler.CallTree.Node.prototype.exportChildren = function() {
  var result = [];
  this.forEachChild(function (node) { result.push(node); });
  return result;
};


/**
 * Finds an immediate child with the specified label.
 *
 * @param {string} label Child node label.
 */
devtools.profiler.CallTree.Node.prototype.findChild = function(label) {
  return this.children[label] || null;
};


/**
 * Finds an immediate child with the specified label, creates a child
 * node if necessary.
 *
 * @param {string} label Child node label.
 */
devtools.profiler.CallTree.Node.prototype.findOrAddChild = function(label) {
  return this.findChild(label) || this.addChild(label);
};


/**
 * Calls the specified function for every child.
 *
 * @param {function(devtools.profiler.CallTree.Node)} f Visitor function.
 */
devtools.profiler.CallTree.Node.prototype.forEachChild = function(f) {
  for (var c in this.children) {
    f(this.children[c]);
  }
};


/**
 * Walks up from the current node up to the call tree root.
 *
 * @param {function(devtools.profiler.CallTree.Node)} f Visitor function.
 */
devtools.profiler.CallTree.Node.prototype.walkUpToRoot = function(f) {
  for (var curr = this; curr != null; curr = curr.parent) {
    f(curr);
  }
};


/**
 * Tries to find a node with the specified path.
 *
 * @param {Array<string>} labels The path.
 * @param {function(devtools.profiler.CallTree.Node)} opt_f Visitor function.
 */
devtools.profiler.CallTree.Node.prototype.descendToChild = function(
    labels, opt_f) {
  for (var pos = 0, curr = this; pos < labels.length && curr != null; pos++) {
    var child = curr.findChild(labels[pos]);
    if (opt_f) {
      opt_f(child, pos);
    }
    curr = child;
  }
  return curr;
};
