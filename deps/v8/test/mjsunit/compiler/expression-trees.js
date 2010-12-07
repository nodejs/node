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

// Flags: --always-opt --nocompilation-cache

// Given a binary operation string and an ordered array of leaf
// strings, return an array of all binary tree strings with the leaves
// (in order) as the fringe.
function makeTrees(op, leaves) {
  var len = leaves.length;
  if (len == 1) {
    // One leaf is a leaf.
    return leaves;
  } else {
    // More than one leaf requires an interior node.
    var result = [];
    // Split the leaves into left and right subtrees in all possible
    // ways.  For each split recursively compute all possible subtrees.
    for (var i = 1; i < len; ++i) {
      var leftTrees = makeTrees(op, leaves.slice(0, i));
      var rightTrees = makeTrees(op, leaves.slice(i, len));
      // Adjoin every possible left and right subtree.
      for (var j = 0; j < leftTrees.length; ++j) {
        for (var k = 0; k < rightTrees.length; ++k) {
          var string = "(" + leftTrees[j] + op + rightTrees[k] + ")";
          result.push(string);
        }
      }
    }
    return result;
  }
}

// All 429 possible bitwise OR trees with eight leaves.
var identifiers = ['a','b','c','d','e','f','g','h'];
var or_trees = makeTrees("|", identifiers);
var and_trees = makeTrees("&", identifiers);

// Set up leaf masks to set 8 least-significant bits.
var a = 1 << 0;
var b = 1 << 1;
var c = 1 << 2;
var d = 1 << 3;
var e = 1 << 4;
var f = 1 << 5;
var g = 1 << 6;
var h = 1 << 7;

for (var i = 0; i < or_trees.length; ++i) {
  for (var j = 0; j < 8; ++j) {
    var or_fun = new Function("return " + or_trees[i]);
    if (j == 0) assertEquals(255, or_fun());

    // Set the j'th variable to a string to force a bailout.
    eval(identifiers[j] + "+= ''");
    assertEquals(255, or_fun());
    // Set it back to a number for the next iteration.
    eval(identifiers[j] + "= +" + identifiers[j]);
  }
}

// Set up leaf masks to clear 8 least-significant bits.
a ^= 255;
b ^= 255;
c ^= 255;
d ^= 255;
e ^= 255;
f ^= 255;
g ^= 255;
h ^= 255;

for (i = 0; i < and_trees.length; ++i) {
  for (var j = 0; j < 8; ++j) {
    var and_fun = new Function("return " + and_trees[i]);
    if (j == 0) assertEquals(0, and_fun());

    // Set the j'th variable to a string to force a bailout.
    eval(identifiers[j] + "+= ''");
    assertEquals(0, and_fun());
    // Set it back to a number for the next iteration.
    eval(identifiers[j] + "= +" + identifiers[j]);
  }
}
