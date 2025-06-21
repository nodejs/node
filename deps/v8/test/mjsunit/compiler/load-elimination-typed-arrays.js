// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-load-elimination

function is_big_endian() {
  const array = new Uint32Array(1);
  array[0] = 5;
  const view = new Uint8Array(array.buffer);
  return view[0] != 5;
}

// Test with 2 arrays aliasing because of different kind sizes.
function f(u32s, u8s) {
  u32s[0] = 0xffffffff;
  // The following store will overlap with the 1st store, and should thus
  // invalidate the Load Elimination state associated with the 1st store.
  u8s[1] = 0;
  // The following load should not be eliminated.
  return u32s[0];
}

const u32s = Uint32Array.of(3, 8);
const u8s = new Uint8Array(u32s.buffer);

const r = is_big_endian() ? 0xff00ffff : 0xffff00ff;
%PrepareFunctionForOptimization(f);
assertEquals(f(u32s, u8s), r);
%OptimizeFunctionOnNextCall(f);
assertEquals(f(u32s, u8s), r);


// Test with 2 arrays aliasing because their bases have different offsets.
function g(ta1, ta2) {
  ta2[0] = 0xff;
  ta1[100] = 42;
  return ta2[0];
}

let buffer = new ArrayBuffer(10000);
let ta1 = new Int32Array(buffer, 0/*offset*/);
let ta2 = new Int32Array(buffer, 100*4/*offset*/);

%PrepareFunctionForOptimization(g);
assertEquals(g(ta1, ta2), 42);
%OptimizeFunctionOnNextCall(g);
assertEquals(g(ta1, ta2), 42);
