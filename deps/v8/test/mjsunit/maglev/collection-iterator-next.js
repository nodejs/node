// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

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

// Static-maps path: the iterator's map is known at the next() call site.
function iterMap(m) {
  const out = [];
  for (const [k, v] of m) out.push(k, v);
  return out;
}

function iterMapKeys(m) {
  const out = [];
  for (const k of m.keys()) out.push(k);
  return out;
}

function iterMapValues(m) {
  const out = [];
  for (const v of m.values()) out.push(v);
  return out;
}

function iterSet(s) {
  const out = [];
  for (const v of s) out.push(v);
  return out;
}

function iterSetEntries(s) {
  const out = [];
  for (const [a, b] of s.entries()) out.push(a, b);
  return out;
}

// Deleting while iterating shrinks the table mid-iteration and exercises
// the obsolete-table healing path.
function iterMapWithDeletes(m) {
  const out = [];
  for (const [k] of m) {
    out.push(k);
    m.delete(k);
  }
  return out;
}

// Adding while iterating grows/rehashes the table.
function iterMapWithAdds(m, n) {
  const out = [];
  let added = 0;
  for (const [k] of m) {
    out.push(k);
    if (added < n) m.set(`x${added++}`, added);
  }
  return out;
}

// Dynamic-kind path: inside a generator the iterator's map is unknown at
// the resume merge, so the kind is dispatched on the map at runtime.
function* genIterMap(m) {
  for (const [k, v] of m) yield `${k}:${v}`;
}

function drainGenIterMap(m) {
  const out = [];
  for (const x of genIterMap(m)) out.push(x);
  return out;
}

const expected = {
  map: iterMap(freshMap(9)),
  keys: iterMapKeys(freshMap(9)),
  values: iterMapValues(freshMap(9)),
  set: iterSet(freshSet(9)),
  setEntries: iterSetEntries(freshSet(9)),
  deletes: iterMapWithDeletes(freshMap(20)),
  adds: iterMapWithAdds(freshMap(5), 8),
  gen: drainGenIterMap(freshMap(9)),
};

const fns = [
  [iterMap, () => [freshMap(9)], expected.map],
  [iterMapKeys, () => [freshMap(9)], expected.keys],
  [iterMapValues, () => [freshMap(9)], expected.values],
  [iterSet, () => [freshSet(9)], expected.set],
  [iterSetEntries, () => [freshSet(9)], expected.setEntries],
  [iterMapWithDeletes, () => [freshMap(20)], expected.deletes],
  [iterMapWithAdds, () => [freshMap(5), 8], expected.adds],
  [drainGenIterMap, () => [freshMap(9)], expected.gen],
];

for (const [fn, makeArgs, want] of fns) {
  %PrepareFunctionForOptimization(fn);
  assertEquals(want, fn(...makeArgs()));
  assertEquals(want, fn(...makeArgs()));
  %OptimizeMaglevOnNextCall(fn);
  assertEquals(want, fn(...makeArgs()));
  assertEquals(want, fn(...makeArgs()));
}

// An exhausted iterator stays exhausted and keeps returning done.
const it = freshMap(3).entries();
while (!it.next().done) {
}
assertTrue(it.next().done);
assertEquals(undefined, it.next().value);

// A polymorphic site sees more than one iteration kind; each iterator must
// keep the value shape of its own kind.
function step(it) {
  return it.next().value;
}

{
  const m = new Map([[1, 10], [2, 20]]);
  %PrepareFunctionForOptimization(step);
  assertEquals([1, 10], step(m.entries()));
  assertEquals(1, step(m.keys()));
  assertEquals(10, step(m.values()));
  %OptimizeMaglevOnNextCall(step);
  assertEquals([1, 10], step(m.entries()));
  assertEquals(1, step(m.keys()));
  assertEquals(10, step(m.values()));
}

// Mutating the collection while an iterator is live obsoletes its table:
// next() has to transition to the successor table. Growing, clearing and
// shrinking all take that path.
function drainKeys(it) {
  const out = [];
  let r;
  while (!(r = it.next()).done) out.push(r.value[0]);
  return out;
}

function growDuringIteration() {
  const m = new Map();
  for (let i = 0; i < 4; i++) m.set(i, i * 10);
  const it = m.entries();
  it.next();
  for (let i = 100; i < 140; i++) m.set(i, i);  // rehash
  return drainKeys(it).length;
}

function clearDuringIteration() {
  const m = new Map();
  for (let i = 0; i < 8; i++) m.set(i, i * 10);
  const it = m.entries();
  it.next();
  m.clear();
  return drainKeys(it).length;
}

function shrinkDuringIteration() {
  const m = new Map();
  for (let i = 0; i < 32; i++) m.set(i, i);
  const it = m.entries();
  it.next();
  for (let i = 0; i < 30; i++) m.delete(i);
  return drainKeys(it).length;
}

%PrepareFunctionForOptimization(drainKeys);

for (const fn of [growDuringIteration, clearDuringIteration,
                  shrinkDuringIteration]) {
  %PrepareFunctionForOptimization(fn);
  const want = fn();
  assertEquals(want, fn());
  %OptimizeMaglevOnNextCall(drainKeys);
  %OptimizeMaglevOnNextCall(fn);
  assertEquals(want, fn());
  assertEquals(want, fn());
}

// A receiver that is not a collection iterator has to fall back to the
// builtin rather than abort the reduction.
const mapNext = new Map().entries().next;

function callNextOn(o) {
  return mapNext.call(o);
}

{
  const m = new Map([[1, 10]]);
  %PrepareFunctionForOptimization(callNextOn);
  assertEquals({ value: [1, 10], done: false }, callNextOn(m.entries()));
  assertThrows(() => callNextOn({}), TypeError);
  assertThrows(() => callNextOn(new Set().values()), TypeError);
  %OptimizeMaglevOnNextCall(callNextOn);
  assertEquals({ value: [1, 10], done: false }, callNextOn(m.entries()));
  assertThrows(() => callNextOn({}), TypeError);
  assertThrows(() => callNextOn(new Set().values()), TypeError);
}

// When entries()/keys()/values() are inlined, next() gets a freshly allocated
// iterator whose table_ it then overwrites. Obsoleting the table from the loop
// body has to be observed by the transition loop.
function forOfKeysWithDeletes(m) {
  const out = [];
  for (const k of m.keys()) {
    out.push(k);
    m.delete(k);
  }
  return out;
}

function forOfValuesWithDeletes(m) {
  const out = [];
  for (const v of m.values()) {
    out.push(v);
    m.delete(v);
  }
  return out;
}

function forOfWithClear(m) {
  const out = [];
  let first = true;
  for (const [k] of m) {
    out.push(k);
    if (first) {
      m.clear();
      first = false;
    }
  }
  return out;
}

function countingMap(n) {
  const m = new Map();
  for (let i = 0; i < n; i++) m.set(i, i);
  return m;
}

{
  const wantKeys = forOfKeysWithDeletes(countingMap(20));
  const wantValues = forOfValuesWithDeletes(countingMap(20));
  const wantClear = forOfWithClear(countingMap(8));
  %PrepareFunctionForOptimization(forOfKeysWithDeletes);
  %PrepareFunctionForOptimization(forOfValuesWithDeletes);
  %PrepareFunctionForOptimization(forOfWithClear);
  forOfKeysWithDeletes(countingMap(20));
  forOfValuesWithDeletes(countingMap(20));
  forOfWithClear(countingMap(8));
  %OptimizeMaglevOnNextCall(forOfKeysWithDeletes);
  %OptimizeMaglevOnNextCall(forOfValuesWithDeletes);
  %OptimizeMaglevOnNextCall(forOfWithClear);
  assertEquals(wantKeys, forOfKeysWithDeletes(countingMap(20)));
  assertEquals(wantValues, forOfValuesWithDeletes(countingMap(20)));
  assertEquals(wantClear, forOfWithClear(countingMap(8)));
}

// A polymorphic site over set iterator kinds, as above for maps.
function stepSet(it) {
  return it.next().value;
}

{
  const s = new Set([7, 8]);
  %PrepareFunctionForOptimization(stepSet);
  assertEquals(7, stepSet(s.values()));
  assertEquals([7, 7], stepSet(s.entries()));
  assertEquals(7, stepSet(s.keys()));
  %OptimizeMaglevOnNextCall(stepSet);
  assertEquals(7, stepSet(s.values()));
  assertEquals([7, 7], stepSet(s.entries()));
  assertEquals(7, stepSet(s.keys()));
}

// Deleting while iterating a set takes the healing path too.
function iterSetWithDeletes(s) {
  const out = [];
  for (const v of s) {
    out.push(v);
    s.delete(v);
  }
  return out;
}

function* genIterSet(s) {
  for (const v of s) yield v;
}

function drainGenIterSet(s) {
  const out = [];
  for (const x of genIterSet(s)) out.push(x);
  return out;
}

{
  const wantDeletes = iterSetWithDeletes(freshSet(20));
  const wantGen = drainGenIterSet(freshSet(9));
  %PrepareFunctionForOptimization(iterSetWithDeletes);
  %PrepareFunctionForOptimization(drainGenIterSet);
  assertEquals(wantDeletes, iterSetWithDeletes(freshSet(20)));
  assertEquals(wantGen, drainGenIterSet(freshSet(9)));
  %OptimizeMaglevOnNextCall(iterSetWithDeletes);
  %OptimizeMaglevOnNextCall(drainGenIterSet);
  assertEquals(wantDeletes, iterSetWithDeletes(freshSet(20)));
  assertEquals(wantGen, drainGenIterSet(freshSet(9)));
}

// A site that sees both a map and a set iterator matches neither builtin's
// iterator maps, and has to fall back.
function stepEither(it) {
  return it.next().value;
}

{
  const m = new Map([[1, 10]]);
  const s = new Set([7]);
  %PrepareFunctionForOptimization(stepEither);
  assertEquals([1, 10], stepEither(m.entries()));
  assertEquals(7, stepEither(s.values()));
  %OptimizeMaglevOnNextCall(stepEither);
  assertEquals([1, 10], stepEither(m.entries()));
  assertEquals(7, stepEither(s.values()));
}

// The iterator result and the key-value array belong to the realm of the
// next() being called, not to the realm of the caller.
{
  const realm = Realm.create();
  Realm.eval(realm, 'globalThis.m = new Map([[1, 2]]);');
  const otherObjectPrototype = Realm.eval(realm, 'Object.prototype');
  const otherArrayPrototype = Realm.eval(realm, 'Array.prototype');

  function crossRealmNext(it) {
    const result = it.next();
    return [Object.getPrototypeOf(result) === otherObjectPrototype,
            Object.getPrototypeOf(result.value) === otherArrayPrototype,
            result.value[0], result.value[1], result.done];
  }

  const want = [true, true, 1, 2, false];
  %PrepareFunctionForOptimization(crossRealmNext);
  assertEquals(want, crossRealmNext(Realm.eval(realm, 'm.entries()')));
  assertEquals(want, crossRealmNext(Realm.eval(realm, 'm.entries()')));
  %OptimizeMaglevOnNextCall(crossRealmNext);
  assertEquals(want, crossRealmNext(Realm.eval(realm, 'm.entries()')));
  assertEquals(want, crossRealmNext(Realm.eval(realm, 'm.entries()')));
}
