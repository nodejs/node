// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function () {
  var o = [];
  o.__proto__ = {};

  function store(o, i, v) {
    o[i] = v;
  }

  store(o, 0, 0);
  store(o, 1, 0);
  store(o, 2, 0);
  o.__proto__[10000000] = 1;

  var set = 0;

  Object.defineProperty(o, '3', {
    get: function() {
      return 100;
    },
    set: function(v) {
      set = v;
    }
  });

  store(o, 3, 1000);
  assertEquals(1000, set);
  assertEquals(100, o[3]);
})();

(function () {
  var o = new Int32Array(1);
  assertThrows(
      () => Object.defineProperty(o, '0', {get: function() {}}), TypeError);
  assertEquals(
      {value: 0, writable: true, enumerable: true, configurable: true},
      Object.getOwnPropertyDescriptor(o, '0'));
})();

(function() {
function f() {
  var a = new Array();
  a[1] = 1.5;
  return a;
};
%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
var a = f();
a[2] = 2;
assertEquals(3, a.length);
})();
