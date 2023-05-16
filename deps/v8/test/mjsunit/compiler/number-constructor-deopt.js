// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This test writes {} to x to trigger lazy deopt
// from inside the number constructor.
var x = "5";
var b = false;

check = function() {
  if (b) x = {};
  return 0;
}

var obj = {};
obj.valueOf = check;

function f() {
  try {
    return x + Number(obj);
  } catch(e) {
    console.log(e.stack);
  }
}

%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
b = true;
f();
