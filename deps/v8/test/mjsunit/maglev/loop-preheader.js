// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Shapes where a bytecode merge point lands exactly on a loop header, giving
// the header several forward predecessors. The graph builder has to merge
// those in a pre-header, so that the header keeps exactly two predecessors.

function sideEffect() {
  globalThis.x = (globalThis.x | 0) + 1;
}

// Runs {fn} on every argument list of {cases} (a list of [args, expected])
// interpreted, then in Maglev, then in the top tier.
function testAllTiers(fn, cases) {
  %PrepareFunctionForOptimization(fn);
  for (const [args, expected] of cases) assertEquals(expected, fn(...args));
  %OptimizeMaglevOnNextCall(fn);
  for (const [args, expected] of cases) assertEquals(expected, fn(...args));
  %PrepareFunctionForOptimization(fn);
  %OptimizeFunctionOnNextCall(fn);
  for (const [args, expected] of cases) assertEquals(expected, fn(...args));
}

// An `if` whose join point is the loop header.
function ifJoin(a, n) {
  if (a) sideEffect();
  let sum = 0;
  while (n > 0) {
    sum += n;
    n--;
  }
  return sum;
}
testAllTiers(ifJoin, [[[true, 3], 6], [[false, 3], 6]]);

// Several switch cases joining on the loop header.
function switchJoin(a, n) {
  switch (a) {
    case 0:
      sideEffect();
      break;
    case 1:
      break;
    case 2:
      sideEffect();
      sideEffect();
      break;
  }
  let sum = 0;
  while (n > 0) {
    sum += n;
    n--;
  }
  return sum;
}
testAllTiers(switchJoin,
             [[[0, 3], 6], [[1, 3], 6], [[2, 3], 6], [[3, 3], 6]]);

// Same, but on an outer loop, which is not eagerly peeled.
function ifJoinOuterLoop(a, n, m) {
  if (a) sideEffect();
  let sum = 0;
  while (n > 0) {
    let i = m;
    while (i > 0) {
      sum++;
      i--;
    }
    n--;
  }
  return sum;
}
testAllTiers(ifJoinOuterLoop, [[[true, 3, 2], 6], [[false, 3, 2], 6]]);

// Nested `if`s, so that the header has more than two forward predecessors.
function manyForwardEdges(a, b, n) {
  if (a) {
    sideEffect();
  } else if (b) {
    sideEffect();
    sideEffect();
  }
  let sum = 0;
  while (n > 0) {
    sum += n;
    n--;
  }
  return sum;
}
testAllTiers(
    manyForwardEdges,
    [[[true, false, 3], 6], [[false, true, 3], 6], [[false, false, 3], 6]]);

// Values flowing through the pre-header phis into the loop phis.
function phiThroughPreheader(a, n) {
  let acc;
  if (a) {
    acc = 1;
  } else {
    acc = 2.5;
  }
  while (n > 0) {
    acc += n;
    n--;
  }
  return acc;
}
testAllTiers(phiThroughPreheader, [[[true, 3], 7], [[false, 3], 8.5]]);

// A `break` out of a (peelable) inner loop landing on the next loop's header.
function breakOntoLoopHeader(n, m) {
  let sum = 0;
  for (let i = 0; i < n; i++) {
    if (i == 2) break;
    sum++;
  }
  while (m > 0) {
    sum += m;
    m--;
  }
  return sum;
}
testAllTiers(breakOntoLoopHeader, [[[5, 3], 8], [[1, 3], 7]]);

// The callee's loop header is split after being inlined.
function inlinee(a, n) {
  if (a) sideEffect();
  let sum = 0;
  while (n > 0) {
    sum += n;
    n--;
  }
  return sum;
}
function inliner(a, n) {
  return inlinee(a, n);
}
%PrepareFunctionForOptimization(inlinee);
testAllTiers(inliner, [[[true, 3], 6], [[false, 3], 6]]);

// A forward edge that is dead when the graph is built: the `if` body is never
// executed while collecting feedback, so it deoptimizes unconditionally.
function neverWarm(x) {
  return x.someUnknownProperty;
}
function deadForwardEdge(a, n) {
  if (a) neverWarm(a);
  let sum = 0;
  while (n > 0) {
    sum += n;
    n--;
  }
  return sum;
}
%PrepareFunctionForOptimization(deadForwardEdge);
assertEquals(6, deadForwardEdge(false, 3));
%OptimizeMaglevOnNextCall(deadForwardEdge);
assertEquals(6, deadForwardEdge(false, 3));
assertEquals(6, deadForwardEdge({}, 3));

// Resumable loops are entered through their resume edges, which bypass the
// header, but their forward edges still go through the pre-header.
function* generatorIfJoin(a, n) {
  if (a) sideEffect();
  while (n > 0) {
    yield n;
    n--;
  }
}
function drive(a, n) {
  let sum = 0;
  for (const value of generatorIfJoin(a, n)) sum += value;
  return sum;
}
%PrepareFunctionForOptimization(generatorIfJoin);
testAllTiers(drive, [[[true, 3], 6], [[false, 3], 6]]);

// OSR into a loop header preceded by a join.
function osrIfJoin(a, n) {
  if (a) sideEffect();
  let sum = 0;
  for (let i = 0; i < n; i++) {
    sum += i;
    if (i == 3) %OptimizeOsr();
  }
  return sum;
}
%PrepareFunctionForOptimization(osrIfJoin);
assertEquals(45, osrIfJoin(true, 10));
%PrepareFunctionForOptimization(osrIfJoin);
assertEquals(45, osrIfJoin(false, 10));
