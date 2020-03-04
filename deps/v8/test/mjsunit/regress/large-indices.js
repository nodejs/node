// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// crbug.com/1026974
(function() {
  function store(obj, key) {
    obj[key] = 10;
  }
  %PrepareFunctionForOptimization(store);
  for (let i = 0; i < 3; i++) {
    let obj = {}
    store(obj, 1152921506754330624);
    assertEquals(["1152921506754330600"], Object.keys(obj));
  }
})();
(function() {
  function store(obj, key) {
    obj[key] = 10;
  }
  %PrepareFunctionForOptimization(store);
  for (let i = 0; i < 3; i++) {
    let obj2 = new Int32Array(0);
    store(obj2, 1152921506754330624);
    assertEquals([], Object.keys(obj2));
    store(obj2, "1152921506754330624");
    assertEquals(["1152921506754330624"], Object.keys(obj2));
  }
})();

// crbug.com/1026729
(function() {
  let key = 0xFFFFFFFF;
  let object = {};
  assertFalse(object.hasOwnProperty(key));
  let proxy = new Proxy({}, {});
  assertFalse(proxy.hasOwnProperty(key));
})();

// crbug.com/1026909
(function() {
  function load(obj, key) {
    return obj[key];
  }
  %PrepareFunctionForOptimization(load);
  const array = new Float64Array();
  assertEquals(undefined, load(array, 'monomorphic'));
  assertEquals(undefined, load(array, '4294967296'));
})();

// crbug.com/1026856
(function() {
  let key = 0xFFFFFFFF;
  let receiver = new Int32Array();
  var value = {};
  var target = {};
  Reflect.set(target, key, value, receiver);
})();

// crbug.com/1028213
(function() {
  function load(obj, key) {
    return obj[key];
  }
  %PrepareFunctionForOptimization(load);
  let obj = function() {};
  obj.__proto__ = new Int8Array(1);
  let key = Object(4294967297);
  for (let i = 0; i < 3; i++) {
    load(obj, key);
  }
})();
(function() {
  function load(obj, key) {
    return obj[key];
  }
  %PrepareFunctionForOptimization(load);
  let obj = new String("abc");
  obj.__proto__ = new Int8Array(1);
  let key = Object(4294967297);
  for (let i = 0; i < 3; i++) {
    load(obj, key);
  }
})();

// crbug.com/1027461#c12
(function() {
  let arr = new Int32Array(2);
  Object.defineProperty(arr, "foo", {get:function() { this.valueOf = 1; }});
  arr[9007199254740991] = 1;
  Object.values(arr);

  let obj = [1, 2, 3];
  Object.defineProperty(obj, 2, {get:function() { this.valueOf = 1; }});
  obj[9007199254740991] = 1;
  Object.values(obj);
})();

// crbug.com/1027461#c18
(function() {
  const v7 = Object(1);
  v7[4294967297] = 1;
  const v8 = Object.assign({}, v7);
})();

// crbug.com/1029369
(function () {
  let obj = {};
  function AddProperty(o, k) {
    Object.defineProperty(o, k, {});
    if (!o.hasOwnProperty(k)) throw "Bug!";
  }
  AddProperty(obj, "1");  // Force dictionary-mode elements.
  AddProperty(obj, 4294967295);
})();

// crbug.com/1029338
(function() {
  var __v_11 = {};
  __v_11.__defineGetter__(4294967295, function () {});
  __v_12 = Object.entries(__v_11);
})();
