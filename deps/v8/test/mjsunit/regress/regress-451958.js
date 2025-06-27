// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function k() { throw "e"; }
var a = true;
var a = false;
function foo(a) {
  var i, j;
  if (a) {
    for (i = 0; i < 1; j++) ;
    for (i = 0; i < 1; k()) ;
    for (i = 0; i < 1; i++) ;
  }
}
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
foo();

function bar() {
var __v_45;
  for (__v_45 = 0; __v_45 < 64; __v_63++) {
  }
  for (__v_45 = 0; __v_45 < 128; __v_36++) {
  }
  for (__v_45 = 128; __v_45 < 256; __v_45++) {
  }
}
%PrepareFunctionForOptimization(bar);
%OptimizeFunctionOnNextCall(bar);
assertThrows(bar);
