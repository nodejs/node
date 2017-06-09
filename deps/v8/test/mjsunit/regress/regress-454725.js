// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc

var __v_9 = {};
var depth = 15;
var current = 0;

function __f_15(__v_3) {
  if ((__v_3 % 50) != 0) {
    return __v_3;
  } else {
    return __v_9 + 0.5;
  }
}
function __f_13(a) {
  a[100000 - 2] = 1;
  for (var __v_3= 0; __v_3 < 70000; ++__v_3 ) {
    a[__v_3] = __f_15(__v_3);
  }
}
function __f_2(size) {

}
var tmp;
function __f_18(allocator) {
  current++;
  if (current == depth) return;
  var __v_7 = new allocator(100000);
  __f_13(__v_7);
  var __v_4 = 6;
  for (var __v_3= 0; __v_3 < 70000; __v_3 += 501 ) {
    tmp += __v_3;
  }
  __f_18(Array);
  current--;
}

gc();
__f_18(__f_2);
