// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function deopt() {
  %DeoptimizeFunction(fun3);
}

function fun3() {
  var r = { 113: deopt(), 113: 7 };
  return r[113];
}

fun3();
fun3();
%OptimizeFunctionOnNextCall(fun3);
var y = fun3();
assertEquals(7, y);
