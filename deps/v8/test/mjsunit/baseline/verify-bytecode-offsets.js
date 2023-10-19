// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-sparkplug --allow-natives-syntax

// This test mainly exists to make ClusterFuzz aware of
// d8.test.verifySourcePositions.

globalValue = false;

function foo(param1, ...param2) {
  try {
    for (let key in param1) { param2.push(key); }
    for (let a of param1) { param2.push(a); }
    let [a, b] = param2;
    let copy = [{literal:1}, {}, [], [1], 1, ...param2];
    return a + b + copy.length;
  } catch (e) {
    return e.toString().match(/[a-zA-Z]+/g);
  } finally {
    globalValue = new String(23);
  }
  return Math.min(Math.random(), 0.5);
}

var obj = [...Array(10).keys()];
obj.foo = 'bar';
foo(obj, obj);

d8.test.verifySourcePositions(foo);

// Make sure invalid calls throw.
assertThrows(() => {d8.test.verifySourcePositions(0)});
assertThrows(() => {d8.test.verifySourcePositions(obj)});
assertThrows(() => {d8.test.verifySourcePositions(new Proxy(foo, {}))});
assertThrows(() => {d8.test.verifySourcePositions(%GetUndetectable())});
