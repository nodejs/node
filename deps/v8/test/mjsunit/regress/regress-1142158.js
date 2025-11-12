// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

var __v_0 = {};
var __v_13 = {};
var __v_14 = {};
var __v_15 = {};
var __v_16 = {};
var __v_17 = {};
var __v_18 = {};
function __f_6(x, deopt) {
  var __v_1 = x;
  var __v_2 = 2 * x;
  var __v_3 = 3 * x;
  var __v_4 = 4 * x;
  var __v_5 = 5 * x;
  var __v_6 = 6 * x;
  var __v_7 = 7 * x;
  var __v_9 = 9 * x;
  var __v_10 = 10 * x;
  var __v_11 = 11 * x;
  var __v_12 = 12 * x;
  var __v_20 = 18 * x;
  var __v_19 = 19 * x;
  var __v_8 = 20 * x;
  __v_0 = 1;
  deopt + -2147483648;
  return __v_1 + __v_2 + __v_3 + __v_4 + __v_5 + __v_6 + __v_7 + __v_8 + __v_9 + __v_10 + __v_11 + __v_12 + __v_13 +
      __v_14 + __v_15 + __v_16 + __v_17 + __v_18 + __v_19 + __v_20;
};
%PrepareFunctionForOptimization(__f_6);
__f_6();
%OptimizeFunctionOnNextCall(__f_6);
assertEquals("45[object Object][object Object][object Object][object Object][object Object][object Object]9.59", __f_6(0.5, ""));
