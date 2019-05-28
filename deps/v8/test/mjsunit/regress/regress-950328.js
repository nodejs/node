// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function NoStoreBecauseReadonlyLength() {
  var a = [];
  Object.defineProperty(a, 'length', { writable: false });


  function f() {
    var o = { __proto__ : a };
    o.push;
  }

  f();
  f();
  %OptimizeFunctionOnNextCall(f);

  a[0] = 1.1;
  f();
  assertEquals(undefined, a[0]);
})();

(function NoStoreBecauseTypedArrayProto() {
  const arr_proto = [].__proto__;
  const arr = [];

  function f() {
    const i32arr = new Int32Array();

    const obj = {};
    obj.__proto__ = arr;
    arr_proto.__proto__ = i32arr;
    obj.__proto__ = arr;
    arr_proto.__proto__ = i32arr;
  }

  f();
  %OptimizeFunctionOnNextCall(f);
  arr[1024] = [];
  f();
  assertEquals(undefined, arr[1024]);
})();
