// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for crbug.com/503557995.  The inlined Array.prototype.sort
// must keep the heap consistent when the comparator truncates the receiver,
// allocates a separate "groom" array, and then re-grows the receiver.  A bug
// here surfaces as a heap-verifier failure (Map check) during a later
// scavenge.

'use strict';

let current = null;
let flip = false;
let groom = null;
let seq = 0;

function mkObj(k) { return {k, p: seq++, q: 0x1234}; }

function makePackedObj16() {
  const a = [];
  for (let i = 0; i < 48; ++i) a.push(mkObj(48 - i));
  for (let i = 0; i < 32; ++i) a.pop();
  return a;
}

function cmp(a, b) {
  if (!flip) {
    flip = true;
    current.length = 1;
    groom = new Array(64);
    for (let i = 0; i < groom.length; ++i) {
      groom[i] = [mkObj(i), mkObj(i + 1000), mkObj(i + 2000), mkObj(i + 3000)];
    }
    current.push(
      mkObj(9), mkObj(10), mkObj(11), mkObj(12), mkObj(13),
      mkObj(14), mkObj(15), mkObj(16), mkObj(17), mkObj(18),
      mkObj(19), mkObj(20), mkObj(21), mkObj(22), mkObj(23)
    );
  }
  return a.k - b.k;
}

function trigger(arr) {
  current = arr;
  flip = false;
  arr.sort(cmp);
  let acc = 0;
  if (groom !== null) {
    for (let i = 0; i < groom.length; ++i) {
      const g = groom[i];
      acc += g.length;
      acc += g[0].k;
      g[1] = g[0];
    }
  }
  return acc + arr.length;
}

for (let i = 0; i < 500; ++i) trigger(makePackedObj16());
for (let i = 0; i < 5000; ++i) trigger(makePackedObj16());
