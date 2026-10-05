// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev

function freshMap(n) {
  const m = new Map();
  for (let i = 0; i < n; i++) m.set(`k${i}`, i);
  return m;
}

function freshSet(n) {
  const s = new Set();
  for (let i = 0; i < n; i++) s.add(`e${i}`);
  return s;
}

function iterMap(m) {
  let n = 0;
  for (const [k, v] of m) n++;
  return n;
}

function iterMapKeys(m) {
  let n = 0;
  for (const k of m.keys()) n++;
  return n;
}

function iterMapValues(m) {
  let n = 0;
  for (const v of m.values()) n++;
  return n;
}

function iterSet(s) {
  let n = 0;
  for (const v of s) n++;
  return n;
}

function iterSetEntries(s) {
  let n = 0;
  for (const [a, b] of s.entries()) n++;
  return n;
}

// The kind is unknown at the resume merge, so this takes the dynamic dispatch.
function* genIterMap(m) {
  for (const [k, v] of m) yield k;
}

function drainGenIterMap(m) {
  let n = 0;
  for (const x of genIterMap(m)) n++;
  return n;
}

const fns = [
  [iterMap, () => freshMap(9)],
  [iterMapKeys, () => freshMap(9)],
  [iterMapValues, () => freshMap(9)],
  [iterSet, () => freshSet(9)],
  [iterSetEntries, () => freshSet(9)],
  [drainGenIterMap, () => freshMap(9)],
];

for (const [fn, makeArg] of fns) {
  %PrepareFunctionForOptimization(fn);
  assertEquals(9, fn(makeArg()));
  assertEquals(9, fn(makeArg()));
  %OptimizeMaglevOnNextCall(fn);
  assertEquals(9, fn(makeArg()));
  assertEquals(9, fn(makeArg()));
  assertTrue(isMaglevved(fn), fn.name);
}

// A polymorphic iteration kind stays on the dynamic dispatch instead of
// deoptimizing once per kind.
function step(it) {
  return it.next().value;
}

{
  const m = new Map([[1, 10]]);
  %PrepareFunctionForOptimization(step);
  step(m.entries());
  step(m.keys());
  step(m.values());
  %OptimizeMaglevOnNextCall(step);
  step(m.entries());
  step(m.keys());
  step(m.values());
  assertTrue(isMaglevved(step));
}
