// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-scoping

'use strict';

// Top-level code
let s = 0;
let f = [undefined, undefined, undefined]
for (const x of [1,2,3]) {
  s += x;
  f[x-1] = function() { return x; }
}
assertEquals(6, s);
assertEquals(1, f[0]());
assertEquals(2, f[1]());
assertEquals(3, f[2]());

let x = 1;
s = 0;
for (const x of [x, x+1, x+2]) {
  s += x;
}
assertEquals(6, s);

s = 0;
var q = 1;
for (const q of [q, q+1, q+2]) {
  s += q;
}
assertEquals(6, s);

let z = 1;
s = 0;
for (const x = 1; z < 2; z++) {
  s += x + z;
}
assertEquals(2, s);


s = "";
for (const x in [1,2,3]) {
  s += x;
}
assertEquals("012", s);

assertThrows(function() { for(const x in [1,2,3]) { x++ } }, SyntaxError);

// Function scope
(function() {
  let s = 0;
  for (const x of [1,2,3]) {
    s += x;
  }
  assertEquals(6, s);

  let x = 1;
  s = 0;
  for (const x of [x, x+1, x+2]) {
    s += x;
  }
  assertEquals(6, s);

  s = 0;
  var q = 1;
  for (const q of [q, q+1, q+2]) {
    s += q;
  }
  assertEquals(6, s);

  s = "";
  for (const x in [1,2,3]) {
    s += x;
  }
  assertEquals("012", s);
}());
