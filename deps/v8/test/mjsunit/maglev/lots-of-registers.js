// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function foo() {
  var x0 = 0;
  var x1 = 1;
  var x2 = 2;
  var x3 = 3;
  var x4 = 4;
  var x5 = 5;
  var x6 = 6;
  var x7 = 7;
  var x8 = 8;
  var x9 = 9;
  var x10 = 10;
  var x11 = 11;
  var x12 = 12;
  var x13 = 13;
  var x14 = 14;
  var x15 = 15;
  var x16 = 16;
  var x17 = 17;
  var x18 = 18;
  var x19 = 19;
  var x20 = 20;
  var x21 = 21;
  var x22 = 22;
  var x23 = 23;
  var x24 = 24;
  var x25 = 25;
  var x26 = 26;
  var x27 = 27;
  var x28 = 28;
  var x29 = 29;
  var x30 = 30;
  var x31 = 31;
  var x32 = 32;
  var x33 = 33;
  var x34 = 34;
  var x35 = 35;
  var x36 = 36;
  var x37 = 37;
  var x38 = 38;
  var x39 = 39;

  return x0 + x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 +
      x10 + x11 + x12 + x13 + x14 + x15 + x16 + x17 + x18 + x19 +
      x20 + x21 + x22 + x23 + x24 + x25 + x26 + x27 + x28 + x29 +
      x30 + x31 + x32 + x33 + x34 + x35 + x36 + x37 + x38 + x39;
}

%PrepareFunctionForOptimization(foo);
print(foo());
print(foo());
%OptimizeMaglevOnNextCall(foo);
print(foo());
