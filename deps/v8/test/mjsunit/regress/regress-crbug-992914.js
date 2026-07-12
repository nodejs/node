// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function mainFreeze() {
  const v2 = {foo:1.1};
  Object.freeze(v2);
  const v12 = {foo:2.2};
  Object.preventExtensions(v12);
  Object.freeze(v12);
  const v18 = {foo:Object};
  v12.__proto__ = 0;
  v2[5] = 1;
}
mainFreeze();

function mainSeal() {
  const v2 = {foo:1.1};
  Object.seal(v2);
  const v12 = {foo:2.2};
  Object.preventExtensions(v12);
  Object.seal(v12);
  const v18 = {foo:Object};
  v12.__proto__ = 0;
  v2[5] = 1;
}
mainSeal();

function testSeal() {
  a = new RangeError(null, null, null);
  a.toFixed = () => {};
  e = Object.seal(a);
  a = new RangeError(null, null, null);
  a.toFixed = () => {};
  k = Object.preventExtensions(a);
  l = Object.seal(a);
  a.toFixed = () => {};
  assertThrows(() => {
    Array.prototype.unshift.call(l, false, Infinity);
  }, TypeError);
}
testSeal();

function testFreeze() {
  a = new RangeError(null, null, null);
  a.toFixed = () => {};
  e = Object.freeze(a);
  a = new RangeError(null, null, null);
  a.toFixed = () => {};
  k = Object.preventExtensions(a);
  l = Object.freeze(a);
  a.toFixed = () => {};
  assertThrows(() => {
    Array.prototype.unshift.call(l, false, Infinity);
  }, TypeError);
}
testFreeze();
