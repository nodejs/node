// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

var __v_5 = {};
var __v_35 = {};
var __v_44 = {};
var __v_43 = {};

try {
__v_1 = 1;
__v_2 = {
  get: function() { return function() {} },
  has() { return true },
  getOwnPropertyDescriptor: function() {
    if (__v_1-- == 0) throw "please die";
    return {value: function() {}, configurable: true};
  }
};
__v_3 = new Proxy({}, __v_2);
__v_30 = Object.create(__v_35);
with (__v_5) { f() }
} catch(e) { print("Caught: " + e); }

function __f_1(asmfunc, expect) {
  var __v_1 = asmfunc.toString();
  var __v_2 = __v_1.replace(new RegExp("use asm"), "");
  var __v_39 = {Math: Math};
  var __v_4 = eval("(" + __v_2 + ")")(__v_3);
  print("Testing " + asmfunc.name + " (js)...");
  __v_44.valueOf = __v_43;
  expect(__v_4);
  print("Testing " + asmfunc.name + " (asm.js)...");
  var __v_5 = asmfunc(__v_3);
  expect(__v_5);
  print("Testing " + asmfunc.name + " (wasm)...");
  var module_func = eval(__v_1);
  var __v_6 = module_func({}, __v_3);
  assertTrue(%IsAsmWasmCode(module_func));
  expect(__v_6);
}
function __f_2() {
  "use asm";
  function __f_3() { return 0; }
  function __f_4() { return 1; }
  function __f_5() { return 4; }
  function __f_6() { return 64; }
  function __f_7() { return 137; }
  function __f_8() { return 128; }
  function __f_9() { return -1; }
  function __f_10() { return 1000; }
  function __f_11() { return 2000000; }
  function __f_12() { return 2147483647; }
  return {__f_3: __f_3, __f_4: __f_4, __f_5: __f_5, __f_6: __f_6, __f_7: __f_7, __f_8: __f_8,
          __f_9: __f_9, __f_10: __f_10, __f_11, __f_12: __f_12};
}
try {
__f_1(__f_2, function(module) {
  assertEquals(0, module.__f_3());
  assertEquals(1, module.__f_4());
  assertEquals(4, module.__f_5());
  assertEquals(64, module.__f_6());
  assertEquals(128, module.__f_8());
  assertEquals(-1, module.__f_9());
  assertEquals(1000, module.__f_10());
  assertEquals(2000000, module.__f_11());
  assertEquals(2147483647, module.__f_12());
});
} catch(e) { print("Caught: " + e); }
