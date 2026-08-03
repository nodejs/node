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

import { ProfileView } from "../../../tools/profile_view.mjs";


function createNode(name, time, opt_parent) {
  var node = new ProfileView.Node(name, time, time, null);
  if (opt_parent) {
    opt_parent.addChild(node);
  }
  return node;
}


(function testSorting() {
   //
   // Build a tree:
   //   root             +--c/5
   //    |               |
   //    +--a/2  +--b/3--+--d/4
   //    |       |       |
   //    +--a/1--+--c/1  +--d/2
   //    |       |
   //    +--c/1  +--b/2
   //
   // So we can sort it using 2 fields: name and time.
   var root = createNode('root', 0);
   createNode('a', 2, root);
   var a1 = createNode('a', 1, root);
   createNode('c', 1, root);
   var b3 = createNode('b', 3, a1);
   createNode('c', 1, a1);
   createNode('b', 2, a1);
   createNode('c', 5, b3);
   createNode('d', 4, b3);
   createNode('d', 2, b3);

   var view = new ProfileView(root);
   var flatTree = [];

   function fillFlatTree(node) {
     flatTree.push(node.internalFuncName);
     flatTree.push(node.selfTime);
   }

   view.traverse(fillFlatTree);
   assertEquals(
     ['root', 0,
      'a', 2, 'a', 1, 'c', 1,
      'b', 3, 'c', 1, 'b', 2,
      'c', 5, 'd', 4, 'd', 2], flatTree);

   function cmpStrs(s1, s2) {
     return s1 == s2 ? 0 : (s1 < s2 ? -1 : 1);
   }

   view.sort(function(n1, n2) {
     return cmpStrs(n1.internalFuncName, n2.internalFuncName) ||
         (n1.selfTime - n2.selfTime);
   });

   flatTree = [];
   view.traverse(fillFlatTree);
   assertEquals(
     ['root', 0,
      'a', 1, 'a', 2, 'c', 1,
      'b', 2, 'b', 3, 'c', 1,
      'c', 5, 'd', 2, 'd', 4], flatTree);
})();
