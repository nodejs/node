// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var boom = { valueOf: function() { throw "boom" } };

function mult_left_plain(x) {
  try {
    return 2 * x;
  } catch (e) {
    return e;
  }
}

%PrepareFunctionForOptimization(mult_left_plain);
%OptimizeFunctionOnNextCall(mult_left_plain);
assertEquals("boom", mult_left_plain(boom));
assertEquals(46, mult_left_plain(23));

function mult_right_plain(x) {
  try {
    return x * 3;
  } catch (e) {
    return e;
  }
}

%PrepareFunctionForOptimization(mult_right_plain);
%OptimizeFunctionOnNextCall(mult_right_plain);
assertEquals("boom", mult_right_plain(boom));
assertEquals(69, mult_right_plain(23));

function mult_none_plain(x,y) {
  try {
    return x * y;
  } catch (e) {
    return e;
  }
}

%PrepareFunctionForOptimization(mult_none_plain);
%OptimizeFunctionOnNextCall(mult_none_plain);
assertEquals("boom", mult_none_plain(boom, boom));
assertEquals("boom", mult_none_plain(boom, 2));
assertEquals("boom", mult_none_plain(2, boom));
assertEquals(966, mult_none_plain(23, 42));
