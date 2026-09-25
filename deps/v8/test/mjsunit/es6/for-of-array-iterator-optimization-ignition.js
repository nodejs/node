// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var results = [];

function testForOf(iterable) {
  results.length = 0;
  for (var i of iterable) {
    results.push(i);
  }
}

testForOf([1, 2, 3]);
console.log(results);
assertEquals(results, [1, 2, 3]);

testForOf([1, 2, 3].keys());
console.log(results);
assertEquals(results, [0, 1, 2]);

testForOf([1, 2, 3].entries());
console.log(results);
assertEquals(results, [[0, 1], [1, 2], [2, 3]]);

testForOf(new Uint8Array([1, 2, 3, 4, 5]));
console.log(results);
assertEquals(results, [1, 2, 3, 4, 5]);
