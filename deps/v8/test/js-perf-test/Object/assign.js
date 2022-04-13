// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('Assign', [1000], [
  new Benchmark('BasicAssign1', false, false, 0,
                BasicAssign1, BasicAssign1Setup, BasicAssign1TearDown),
  new Benchmark('BasicAssign3', false, false, 0,
                BasicAssign3, BasicAssign3Setup, BasicAssign3TearDown),
  new Benchmark('BasicAssignNull3', false, false, 0,
                BasicAssignNull3, BasicAssignNull3Setup,
                BasicAssignNull3TearDown),
]);

var object;
var src1;
var src2;
var src3;
var obj1;
var obj2;

// ----------------------------------------------------------------------------

function BasicAssign1Setup() {
  object = {};
  obj1 = {};
  obj2 = {};
  src1 = { id: "6930530530", obj1: obj1, obj2: obj2 };
}

function BasicAssign1() {
  Object.assign(object, src1);
}

function BasicAssign1TearDown() {
  return object.id === src1.id &&
         object.obj1 === obj1 &&
         object.obj2 === obj2;
}

// ----------------------------------------------------------------------------

function BasicAssign3Setup() {
  object = {};
  obj1 = {};
  obj2 = {};
  src1 = { id: "6930530530" };
  src2 = { obj1: obj1 };
  src3 = { obj2: obj2 };
}

function BasicAssign3() {
  Object.assign(object, src1, src2, src3);
}

function BasicAssign3TearDown() {
  return object.id === src1.id &&
         object.obj1 === src2 &&
         object.obj2 === src3;
}

// ----------------------------------------------------------------------------

function BasicAssignNull3Setup() {
  object = {};
  obj1 = {};
  obj2 = {};
  src1 = { id: "6930530530" };
  src2 = null;
  src3 = { obj1: obj1, obj2: obj2 };
}

function BasicAssignNull3() {
  Object.assign(object, src1, src2, src3);
}

function BasicAssignNull3TearDown() {
  return object.id === src1.id &&
         object.obj1 === src2 &&
         object.obj2 === src3;
}
