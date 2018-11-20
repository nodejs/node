// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-natives-as=builtins --expose-gc

var __v_7 = [];
var __v_8 = {};
var __v_10 = {};
var __v_11 = this;
var __v_12 = {};
var __v_13 = {};
var __v_14 = "";
var __v_15 = {};
try {
__v_1 = {x:0};
%OptimizeFunctionOnNextCall(__f_1);
assertEquals("good", __f_1());
delete __v_1.x;
assertEquals("good", __f_1());
} catch(e) { print("Caught: " + e); }
try {
__v_3 = new Set();
__v_5 = new builtins.SetIterator(__v_3, -12);
__v_4 = new Map();
__v_6 = new builtins.MapIterator(__v_4, 2);
__f_3(Array);
} catch(e) { print("Caught: " + e); }
function __f_4(__v_8, filter) {
  function __f_6(v) {
    for (var __v_4 in v) {
      for (var __v_4 in v) {}
    }
    %OptimizeFunctionOnNextCall(filter);
    return filter(v);
  }
  var __v_7 = eval(__v_8);
  gc();
  return __f_6(__v_7);
}
function __f_5(__v_6) {
  var __v_5 = new Array(__v_6);
  for (var __v_4 = 0; __v_4 < __v_6; __v_4++) __v_5.push('{}');
  return __v_5;
}
try {
try {
  __v_8.test("\x80");
  assertUnreachable();
} catch (e) {
}
gc();
} catch(e) { print("Caught: " + e); }
