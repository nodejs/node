// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --interrupt-budget=1000 --no-lazy-feedback-allocation

var __v_0 = "exception";
var __v_1 = 0;
var __v_2 = [, true, true, false, false, true, true, true, false, false, true,
  true, false, false, true, false, true, true, false, false, true, true,
  false, true, true, false, false, true, true, false, false, __v_0, __v_1,
  true, true, __v_0, __v_0, true, true, __v_0, __v_0, false, true, __v_0,
  __v_0, false, true, __v_0, __v_0, true, false, __v_0, __v_0, true, false,
  __v_0, __v_0, false, false, __v_0, __v_0, false, false, false, false, __v_0,
  __v_0, false, false, __v_0, __v_0, true, false, __v_0, __v_0, true, false,
  __v_0, __v_0, false, true, __v_0, __v_0, false, true, __v_0, __v_0, true,
  true, __v_0, __v_0, true, true, __v_0, __v_0, __v_0, __v_0, __v_0, __v_0,
  __v_0, __v_0, __v_0, __v_0, __v_0, __v_0, __v_0, __v_0, __v_0, __v_0, __v_0,
  __v_0, __v_0, __v_0, __v_0, __v_0, __v_1, __v_0, __v_0, __v_0, __v_0, __v_0,
  __v_0, __v_1, __v_0, __v_0, __v_0];

for (var __v_3 = 0; __v_3 < 256; __v_3++) {
  __f_1();
}

function __f_0() { __v_2[__v_1]; }
function __f_3() { __v_2.shift(); }
function __f_1() {
  try {
    var __v_18 = __v_8 ? new __v_9() : new __v_17();
  } catch (e) {}
  try {
    var __v_19 = __v_9 ? new __v_16() : new __v_17();
  } catch (e) {}
    __f_0();
    __f_3();
  try {
    if (__v_14) {}
  } catch (e) {}
}
