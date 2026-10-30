// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-non-eager-inlining --maglev-licm
// Flags: --max-maglev-inlined-bytecode-size-small=0 --no-turbofan

// 1. Double-field (in-object and out-of-object) load/store KNA tracking
// across non-eager inlining, loops, LICM, and MaglevGraphOptimizer.
function Vec3(x, y, z) {
  this.x = x;
  this.y = y;
  this.z = z;
}

function addVec(a, b) {
  a.x += b.x;
  a.y += b.y;
  a.z += b.z;
}

function dotVec(a, b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

function testDoubleFieldKna(x) {
  let v1 = new Vec3(x + 0.5, x + 1.5, x + 2.5);
  let v2 = new Vec3(1.25, 2.25, 3.25);
  addVec(v1, v2);
  addVec(v1, v2);
  return dotVec(v1, v2);
}

%PrepareFunctionForOptimization(Vec3);
%PrepareFunctionForOptimization(addVec);
%PrepareFunctionForOptimization(dotVec);
%PrepareFunctionForOptimization(testDoubleFieldKna);
assertEquals(53.25, testDoubleFieldKna(1));
assertEquals(60, testDoubleFieldKna(2));
%OptimizeMaglevOnNextCall(testDoubleFieldKna);
assertEquals(53.25, testDoubleFieldKna(1));
assertEquals(60, testDoubleFieldKna(2));

// Loop KNA tracking and LICM of double-field HeapNumber box loads.
function testDoubleFieldLoopKna(x, n) {
  let v1 = new Vec3(x + 0.5, x + 1.5, x + 2.5);
  let v2 = new Vec3(1.25, 2.25, 3.25);
  let sum = 0;
  for (let i = 0; i < n; i++) {
    addVec(v1, v2);
    sum += dotVec(v1, v2);
  }
  return sum;
}

function testDoubleFieldLicm(v1, v2, n) {
  let sum = 0;
  let i = 0;
  do {
    let xy = v1.x * v2.x + v1.y * v2.y;
    for (let j = 0; j < n; j++) {
      sum += xy + v1.z * v2.z;
    }
    i++;
  } while (i < n);
  return sum;
}

%PrepareFunctionForOptimization(testDoubleFieldLoopKna);
%PrepareFunctionForOptimization(testDoubleFieldLicm);
assertEquals(159.75, testDoubleFieldLoopKna(1, 3));
assertEquals(180, testDoubleFieldLoopKna(2, 3));
%OptimizeMaglevOnNextCall(testDoubleFieldLoopKna);
assertEquals(159.75, testDoubleFieldLoopKna(1, 3));
assertEquals(180, testDoubleFieldLoopKna(2, 3));

assertEquals(109.125,
             testDoubleFieldLicm(new Vec3(0.5, 1.5, 2.5),
                                 new Vec3(1.25, 2.25, 3.25), 3));
assertEquals(109.125,
             testDoubleFieldLicm(new Vec3(0.5, 1.5, 2.5),
                                 new Vec3(1.25, 2.25, 3.25), 3));
%OptimizeMaglevOnNextCall(testDoubleFieldLicm);
assertEquals(109.125,
             testDoubleFieldLicm(new Vec3(0.5, 1.5, 2.5),
                                 new Vec3(1.25, 2.25, 3.25), 3));

// Out-of-object double fields.
function makeOutOfObject(x) {
  let o = { a: 1, b: 2, c: 3, d: 4 };
  o.e = x + 0.25;
  o.f = x + 0.75;
  return o;
}

function updateOutOfObject(o, delta) {
  o.e += delta;
  o.f += delta * 2;
  return o.e + o.f;
}

function testOutOfObjectDoubleFieldKna(x) {
  let o = makeOutOfObject(x);
  let r1 = updateOutOfObject(o, 1.5);
  let r2 = updateOutOfObject(o, 0.5);
  return r1 + r2 + o.e * o.f;
}

%PrepareFunctionForOptimization(makeOutOfObject);
%PrepareFunctionForOptimization(updateOutOfObject);
%PrepareFunctionForOptimization(testOutOfObjectDoubleFieldKna);
assertEquals(49.1875, testOutOfObjectDoubleFieldKna(2));
assertEquals(65.1875, testOutOfObjectDoubleFieldKna(3));
%OptimizeMaglevOnNextCall(testOutOfObjectDoubleFieldKna);
assertEquals(49.1875, testOutOfObjectDoubleFieldKna(2));
assertEquals(65.1875, testOutOfObjectDoubleFieldKna(3));

// 2. Ensure Smi/Tagged property values and Double-field HeapNumber boxes with
// the same property name do not collide in KNA or LICM when maps transition or
// become stale.
function MapDouble() {
  this.x = 1.5;
}
// Create a sibling transition on .z so that the .y transition map gets its own
// DescriptorArray. Generalizing .x to Tagged on the .y map then happens
// in-place without generalizing MapDouble itself, allowing an instance of
// MapDouble to transition from a Double .x field to a Tagged .x field via .y.
let branch_z = new MapDouble();
branch_z.x = 2.5;
branch_z.z = 1;

function makeTagged() {
  let o = new MapDouble();
  o.y = 1;
  o.x = 10;
  return o;
}
let branch_y = makeTagged();
branch_y.w = 1;

function transitionToTagged(o) {
  o.y = 1;
}
function transitionOther(o2) {
  o2.w = 1;
}
function writeSmi(o, do_write) {
  if (do_write) {
    o.x = 42;
  }
}
function readSmi(o, do_read) {
  if (do_read) {
    return o.x + 1;
  }
  return 0;
}
function readDouble(o, do_read) {
  if (do_read) {
    return o.x;
  }
  return 0;
}
function writeDouble(o, do_write) {
  if (do_write) {
    o.x = 3.5;
  }
}
function transitionAddDoubleX(o) {
  o.x = 2.5;
}

%PrepareFunctionForOptimization(MapDouble);
%PrepareFunctionForOptimization(makeTagged);
%PrepareFunctionForOptimization(transitionToTagged);
%PrepareFunctionForOptimization(transitionOther);
%PrepareFunctionForOptimization(writeSmi);
%PrepareFunctionForOptimization(readSmi);
%PrepareFunctionForOptimization(readDouble);
%PrepareFunctionForOptimization(writeDouble);
%PrepareFunctionForOptimization(transitionAddDoubleX);

for (let i = 0; i < 3; i++) {
  let d = new MapDouble();
  writeDouble(d, true);
  writeDouble(d, false);
  readDouble(d, true);
  readDouble(d, false);
  transitionToTagged(d);
  writeSmi(d, true);
  writeSmi(d, false);
  readSmi(d, true);
  readSmi(d, false);
  transitionOther(d);
  transitionAddDoubleX({ a: 1 });
}

function testDoubleThenSmi(o, o2, flag) {
  writeDouble(o, true);
  let r1 = readDouble(o, true);
  transitionToTagged(o2);
  let r2 = readSmi(o, flag);
  writeSmi(o, flag);
  let r3 = readSmi(o, flag);
  let r4 = readDouble(o, !flag);
  return r1 + r2 + r3 + r4;
}

function testSmiThenDouble(o, o2, is_smi, is_double) {
  if (is_smi) {
    writeSmi(o, true);
    let r1 = readSmi(o, true);
    transitionOther(o2);
    writeDouble(o, is_double);
    return r1 + readDouble(o, is_double);
  }
  writeDouble(o, is_double);
  return readDouble(o, is_double);
}

function testCrossInvalidationAndLoopClobber(d, s, empty, n) {
  let sum = readDouble(d, true);
  transitionAddDoubleX(empty);
  let i = 0;
  do {
    sum += d.x;
    s.x = 42;
    for (let j = 0; j < n; j++) {
      sum += s.x;
    }
    i++;
  } while (i < n);
  return sum + readSmi(s, true);
}

%PrepareFunctionForOptimization(testDoubleThenSmi);
%PrepareFunctionForOptimization(testSmiThenDouble);
%PrepareFunctionForOptimization(testCrossInvalidationAndLoopClobber);

{
  let d = new MapDouble();
  assertEquals(51, testDoubleThenSmi(d, d, true));
  assertEquals(7, testDoubleThenSmi(new MapDouble(), new MapDouble(), false));
}
%OptimizeMaglevOnNextCall(testDoubleThenSmi);
{
  let d = new MapDouble();
  assertEquals(51, testDoubleThenSmi(d, d, true));
  assertEquals(7, testDoubleThenSmi(new MapDouble(), new MapDouble(), false));
}

assertEquals(43, testSmiThenDouble(makeTagged(), makeTagged(), true, false));
assertEquals(3.5, testSmiThenDouble(new MapDouble(), makeTagged(), false, true));
%OptimizeMaglevOnNextCall(testSmiThenDouble);
assertEquals(43, testSmiThenDouble(makeTagged(), makeTagged(), true, false));
assertEquals(3.5, testSmiThenDouble(new MapDouble(), makeTagged(), false, true));

assertEquals(215.5,
             testCrossInvalidationAndLoopClobber(
                 new MapDouble(), makeTagged(), { a: 1 }, 2));
assertEquals(215.5,
             testCrossInvalidationAndLoopClobber(
                 new MapDouble(), makeTagged(), { a: 1 }, 2));
%OptimizeMaglevOnNextCall(testCrossInvalidationAndLoopClobber);
assertEquals(215.5,
             testCrossInvalidationAndLoopClobber(
                 new MapDouble(), makeTagged(), { a: 1 }, 2));
