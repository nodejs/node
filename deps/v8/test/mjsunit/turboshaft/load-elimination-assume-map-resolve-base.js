// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test that LateLoadEliminationAnalyzer::ProcessAssumeMap resolves the base
// of a load-eliminated heap object before recording its map in `object_maps_`.

// `holder` is polymorphic (2 maps with the same field offset/representation)
// so TurboFan's LoadElimination (and Maglev) cannot preserve `holder.p` across
// `other.p = 1`, leaving `y = holder.p` for Turboshaft's LateLoadElimination.
// Turboshaft eliminates `y` to `x`, so `AssumeMap(y, ...)` emitted by `y.v`
// must be recorded on `ResolveBase(y) == x` for `z.v = 99` (or `y.v = 99`) to
// know that `x` and `z` have disjoint maps.

function storeThenReload(holder, other, x, z) {
  holder.p = x;
  other.p = 1;
  let y = holder.p;
  let v1 = y.v;
  z.v = 99;
  %TurbofanStaticAssert(v1 === y.v);
}

function loadThenReload(holder, other, z) {
  let x = holder.p;
  other.p = 1;
  let y = holder.p;
  let v1 = y.v;
  z.v = 99;
  %TurbofanStaticAssert(v1 === y.v);
  return x;
}

function storeThroughEliminatedLoad(holder, other, x, z) {
  let v1 = z.v;
  holder.p = x;
  other.p = 1;
  let y = holder.p;
  y.v = 99;
  %TurbofanStaticAssert(v1 === z.v);
}

let x = { v: 1 };
x.v = 2;

let holder1 = { p: 1, a: 1 };
holder1.p = x;
holder1.p = 1;
holder1.p = x;

let holder2 = { p: 1, b: 1 };
holder2.p = x;
holder2.p = 1;
holder2.p = x;

let other = { p: 1, c: 1 };
other.p = 2;

let z = { v: 1, d: 1 };
z.v = 2;

%PrepareFunctionForOptimization(storeThenReload);
storeThenReload(holder1, other, x, z);
storeThenReload(holder2, other, x, z);
%OptimizeFunctionOnNextCall(storeThenReload);
storeThenReload(holder1, other, x, z);

%PrepareFunctionForOptimization(loadThenReload);
assertEquals(x, loadThenReload(holder1, other, z));
assertEquals(x, loadThenReload(holder2, other, z));
%OptimizeFunctionOnNextCall(loadThenReload);
assertEquals(x, loadThenReload(holder1, other, z));

%PrepareFunctionForOptimization(storeThroughEliminatedLoad);
storeThroughEliminatedLoad(holder1, other, x, z);
storeThroughEliminatedLoad(holder2, other, x, z);
%OptimizeFunctionOnNextCall(storeThroughEliminatedLoad);
storeThroughEliminatedLoad(holder1, other, x, z);
