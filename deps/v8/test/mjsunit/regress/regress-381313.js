// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var g = 0;

function f(x, deopt) {
  var a0 = x;
  var a1 = 2 * x;
  var a2 = 3 * x;
  var a3 = 4 * x;
  var a4 = 5 * x;
  var a5 = 6 * x;
  var a6 = 7 * x;
  var a7 = 8 * x;
  var a8 = 9 * x;
  var a9 = 10 * x;
  var a10 = 11 * x;
  var a11 = 12 * x;
  var a12 = 13 * x;
  var a13 = 14 * x;
  var a14 = 15 * x;
  var a15 = 16 * x;
  var a16 = 17 * x;
  var a17 = 18 * x;
  var a18 = 19 * x;
  var a19 = 20 * x;

  g = 1;

  deopt + 0;

  return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 +
         a10 + a11 + a12 + a13 + a14 + a15 + a16 + a17 + a18 + a19;
}

f(0.5, 0);
f(0.5, 0);
%OptimizeFunctionOnNextCall(f);
print(f(0.5, ""));
