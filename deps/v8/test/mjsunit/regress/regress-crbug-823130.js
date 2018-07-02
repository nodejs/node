// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

var __v_1 = new Array();
var __v_2 = 0x30;
var __v_4 = "abc";
var __v_3 = "def";

function __f_2(b) {
  [...b];
}
__f_2([1]);
__f_2([3.3]);
__f_2([{}]);

var vars = [__v_1, __v_2, __v_3, __v_4];

for (var j = 0; j < vars.length && j < 7; j++) {
  for (var k = j; k < vars.length && k < 7 + j; k++) {
    var v1 = vars[j];
    var e1, e2;
    try {
      __f_2(v1);
      __f_2();
    } catch (e) {
      e1 = "" + e;
    }
    gc();
    try {
      __f_2(v1);
      __f_2();
    } catch (e) {
      e2 = "" + e;
    }
    assertEquals(e1, e2);
  }
}
