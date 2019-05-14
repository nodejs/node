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

// Load the Splay tree implementation from <project root>/tools.
// Files: tools/splaytree.js


(function testIsEmpty() {
  var tree = new SplayTree();
  assertTrue(tree.isEmpty());
  tree.insert(0, 'value');
  assertFalse(tree.isEmpty());
})();


(function testExportValues() {
  var tree = new SplayTree();
  assertArrayEquals([], tree.exportValues());
  tree.insert(0, 'value');
  assertArrayEquals(['value'], tree.exportValues());
  tree.insert(0, 'value');
  assertArrayEquals(['value'], tree.exportValues());
})();


function createSampleTree() {
  // Creates the following tree:
  //           50
  //          /  \
  //         30  60
  //        /  \   \
  //       10  40  90
  //         \    /  \
  //         20  70 100
  //        /      \
  //       15      80
  //
  // We can't use the 'insert' method because it also uses 'splay_'.
  return { key: 50, value: 50,
      left: { key: 30, value: 30,
              left: { key: 10, value: 10, left: null,
                      right: { key: 20, value: 20,
                               left: { key: 15, value: 15,
                                       left: null, right: null },
                               right: null } },
              right: { key: 40, value: 40, left: null, right: null } },
      right: { key: 60, value: 60, left: null,
               right: { key: 90, value: 90,
                        left: { key: 70, value: 70, left: null,
                                right: { key: 80, value: 80,
                                         left: null, right: null } },
                        right: { key: 100, value: 100,
                                 left: null, right: null } } } };
};


(function testSplay() {
  var tree = new SplayTree();
  tree.root_ = createSampleTree();
  assertArrayEquals([50, 30, 60, 10, 40, 90, 20, 70, 100, 15, 80],
                    tree.exportValues());
  tree.splay_(50);
  assertArrayEquals([50, 30, 60, 10, 40, 90, 20, 70, 100, 15, 80],
                    tree.exportValues());
  tree.splay_(80);
  assertArrayEquals([80, 60, 90, 50, 70, 100, 30, 10, 40, 20, 15],
                    tree.exportValues());
})();


(function testInsert() {
  var tree = new SplayTree();
  tree.insert(5, 'root');
  tree.insert(3, 'left');
  assertArrayEquals(['left', 'root'], tree.exportValues());
  tree.insert(7, 'right');
  assertArrayEquals(['right', 'root', 'left'], tree.exportValues());
})();


(function testFind() {
  var tree = new SplayTree();
  tree.insert(5, 'root');
  tree.insert(3, 'left');
  tree.insert(7, 'right');
  assertEquals('root', tree.find(5).value);
  assertEquals('left', tree.find(3).value);
  assertEquals('right', tree.find(7).value);
  assertEquals(null, tree.find(0));
  assertEquals(null, tree.find(100));
  assertEquals(null, tree.find(-100));
})();


(function testFindMin() {
  var tree = new SplayTree();
  assertEquals(null, tree.findMin());
  tree.insert(5, 'root');
  tree.insert(3, 'left');
  tree.insert(7, 'right');
  assertEquals('left', tree.findMin().value);
})();


(function testFindMax() {
  var tree = new SplayTree();
  assertEquals(null, tree.findMax());
  tree.insert(5, 'root');
  tree.insert(3, 'left');
  tree.insert(7, 'right');
  assertEquals('right', tree.findMax().value);
})();


(function testFindGreatestLessThan() {
  var tree = new SplayTree();
  assertEquals(null, tree.findGreatestLessThan(10));
  tree.insert(5, 'root');
  tree.insert(3, 'left');
  tree.insert(7, 'right');
  assertEquals('right', tree.findGreatestLessThan(10).value);
  assertEquals('right', tree.findGreatestLessThan(7).value);
  assertEquals('root', tree.findGreatestLessThan(6).value);
  assertEquals('left', tree.findGreatestLessThan(4).value);
  assertEquals(null, tree.findGreatestLessThan(2));
})();


(function testRemove() {
  var tree = new SplayTree();
  assertThrows('tree.remove(5)');
  tree.insert(5, 'root');
  tree.insert(3, 'left');
  tree.insert(7, 'right');
  assertThrows('tree.remove(1)');
  assertThrows('tree.remove(6)');
  assertThrows('tree.remove(10)');
  assertEquals('root', tree.remove(5).value);
  assertEquals('left', tree.remove(3).value);
  assertEquals('right', tree.remove(7).value);
  assertArrayEquals([], tree.exportValues());
})();
