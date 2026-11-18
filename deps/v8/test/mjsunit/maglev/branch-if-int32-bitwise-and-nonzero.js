// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --turbolev --turbofan

// Branches on `x & mask` fuse into a single test-and-branch.

function oneBit(x) { return (x & 1) ? "odd" : "even"; }
function oneBitNegated(x) { return !(x & 1) ? "even" : "odd"; }
function multiBit(x) { return (x & 0x30) ? 1 : 0; }
// The bytecode generator commutes the Smi literal to the right.
function constantOnTheLeft(x) { return (0x40 & x) ? 1 : 0; }
// 0x80000000 is not an int32, so the mask reaches Maglev as -2147483648.
function signBit(x) { return (x & 0x80000000) ? 1 : 0; }
// The and result is also used elsewhere, so it must stay alive.
function usedTwice(x) { let m = x & 6; return m ? m : -1; }
// The and result is live in a deopt frame in the taken branch.
function deoptInBranch(x, o) { let m = x & 6; return m ? o.y + m : -1; }

function check() {
  assertEquals("odd", oneBit(1));
  assertEquals("even", oneBit(2));
  assertEquals("odd", oneBit(-1));
  assertEquals("even", oneBitNegated(2));
  assertEquals("odd", oneBitNegated(3));
  assertEquals(1, multiBit(0x10));
  assertEquals(1, multiBit(0x20));
  assertEquals(0, multiBit(0x40));
  assertEquals(0, multiBit(0));
  assertEquals(1, constantOnTheLeft(0x40));
  assertEquals(0, constantOnTheLeft(0x20));
  assertEquals(1, signBit(-1));
  assertEquals(1, signBit(-2147483648));
  assertEquals(0, signBit(0x7fffffff));
  assertEquals(0, signBit(0));
  assertEquals(2, usedTwice(2));
  assertEquals(6, usedTwice(7));
  assertEquals(-1, usedTwice(1));
  assertEquals(3, deoptInBranch(2, {y: 1}));
  assertEquals(-1, deoptInBranch(1, {y: 1}));
}

const functions = [oneBit, oneBitNegated, multiBit, constantOnTheLeft, signBit,
                   usedTwice, deoptInBranch];
for (const f of functions) %PrepareFunctionForOptimization(f);
check();
for (const f of functions) %OptimizeMaglevOnNextCall(f);
check();
for (const f of functions) assertMaglevved(f);

// A different map in the taken branch deoptimizes with the and result live.
assertEquals(8, deoptInBranch(6, {z: 0, y: 2}));
assertUnoptimized(deoptInBranch);

// Turbolev builds the same fused branch and lowers it back to an and and a
// branch for Turboshaft.
for (const f of functions) %PrepareFunctionForOptimization(f);
check();
for (const f of functions) %OptimizeFunctionOnNextCall(f);
check();
for (const f of functions) assertOptimized(f);

// A third map in the taken branch deoptimizes the Turbolev code too.
assertEquals(8, deoptInBranch(6, {w: 0, z: 0, y: 2}));
assertUnoptimized(deoptInBranch);
