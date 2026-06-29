// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stack-size=120

var __v_11 = {};
function __f_2(depth) {
  try {
    __f_5(depth, __v_11);
    return true;
  } catch (e) {
    gc();
  }
}
function __f_5(n, __v_4) {
  if (--n == 0) {
    __f_1(__v_4);
    return;
  }
  __f_5(n, __v_4);
}
function __f_1(__v_4) {
  var __v_5 = new RegExp(__v_4);
}
function __f_4() {
  var __v_1 = 100;
  var __v_8 = 100000;
  while (__v_1 < __v_8 - 1) {
    var __v_3 = Math.floor((__v_1 + __v_8) / 2);
    if (__f_2(__v_3)) {
      __v_1 = __v_3;
    } else {
      __v_8 = __v_3;
    }
  }
}
__f_4();
