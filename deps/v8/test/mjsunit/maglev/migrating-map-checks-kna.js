// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-non-eager-inlining
// Flags: --max-maglev-inlined-bytecode-size-small=0

// 1. CheckMapsWithMigration and StoreMap tracking in
// RecomputeKnownNodeAspectsProcessor (with an unstable migration target map)
// and redundant CheckMapsWithMigration elimination in MaglevGraphOptimizer.
function Point(x, y) {
  this.x = x;
  this.y = y;
}

function scalePoint(p, s) {
  p.x *= s;
  p.y *= s;
}

function sumPoint(p) {
  return p.x + p.y;
}

// Create an instance with Smi fields first, then deprecate the initial Smi map
// by transitioning `x` and `y` to double representation so the resulting map
// is a migration target, and transition away from that target map so it is
// also unstable.
let deprecated_point = new Point(1, 2);
(new Point(1, 2)).x = 1.5;
(new Point(1, 2)).y = 2.5;
(new Point(1.5, 2.5)).z = 1;

%PrepareFunctionForOptimization(Point);
%PrepareFunctionForOptimization(scalePoint);
%PrepareFunctionForOptimization(sumPoint);

// Warm up with the migrated double-field map.
scalePoint(deprecated_point, 2.0);
sumPoint(deprecated_point);

function testMigratingMapChecks(p, x, y) {
  // For parameter `p`, the first inlined call emits CheckMapsWithMigration,
  // and the second inlined call's CheckMapsWithMigration is folded via KNA.
  scalePoint(p, 1.5);
  let s1 = sumPoint(p);

  // For freshly allocated `fresh`, StoreMap from `new Point` establishes the
  // unstable target map in KNA across the inlined `scalePoint` call so the
  // subsequent CheckMapsWithMigration in `sumPoint` folds.
  let fresh = new Point(x + 0.25, y + 0.75);
  scalePoint(fresh, 2.0);
  let s2 = sumPoint(fresh);
  return s1 + s2;
}

%PrepareFunctionForOptimization(testMigratingMapChecks);
let p1 = new Point(2.0, 4.0);
assertEquals(17.0, testMigratingMapChecks(p1, 1, 2));
assertEquals(21.5, testMigratingMapChecks(p1, 1, 2));
%OptimizeMaglevOnNextCall(testMigratingMapChecks);
assertEquals(28.25, testMigratingMapChecks(p1, 1, 2));

// 2. CheckMapsWithMigrationAndDeopt tracking in
// RecomputeKnownNodeAspectsProcessor when a deprecated map in feedback has an
// updated counterpart that is not yet a migration target.
function Box(x, y) {
  this.x = x;
  this.y = y;
}

function scaleBox(b, s) {
  b.x *= s;
  b.y *= s;
}

function sumBox(b) {
  return b.x + b.y;
}

%PrepareFunctionForOptimization(scaleBox);
%PrepareFunctionForOptimization(sumBox);

let smi_box = new Box(2, 4);
scaleBox(smi_box, 2);
sumBox(smi_box);

function testMigratingMapChecksAndDeopt(b) {
  scaleBox(b, 1.5);
  return sumBox(b);
}

%PrepareFunctionForOptimization(testMigratingMapChecksAndDeopt);
assertEquals(18, testMigratingMapChecksAndDeopt(new Box(4, 8)));

// Deprecate the Smi Box map without migrating any existing instance so the
// updated double-field map is not marked as a migration target, and make the
// updated map unstable.
(new Box(1, 2)).x = 1.5;
(new Box(1.5, 2)).z = 1;

%OptimizeMaglevOnNextCall(testMigratingMapChecksAndDeopt);
assertEquals(9.0, testMigratingMapChecksAndDeopt(new Box(2.0, 4.0)));

// 3. StoreMap type contradiction in RecomputeKnownNodeAspectsProcessor when an
// inlined function narrows the receiver type to JSArray before a polymorphic
// property store arm whose transitioning StoreMap expects an ordinary JSObject.
function checkArray(a) {
  let s = a.length;
  s += a.length;
  s += a.length;
  s += a.length;
  return s;
}

function TransitionObj() {
  this.a = 1;
}

function testStoreMapContradiction(o) {
  let n = checkArray(o);
  o.x = 1;
  return n;
}

let arr = [1, 2];
arr.x = 1;

%PrepareFunctionForOptimization(testStoreMapContradiction);
// Record the transitioning JSObject store in the polymorphic IC before
// preparing checkArray, so checkArray only collects JSArray feedback.
testStoreMapContradiction(new TransitionObj());

%PrepareFunctionForOptimization(checkArray);
assertEquals(8, testStoreMapContradiction(arr));
assertEquals(8, testStoreMapContradiction(arr));

%OptimizeMaglevOnNextCall(testStoreMapContradiction);
assertEquals(8, testStoreMapContradiction(arr));
