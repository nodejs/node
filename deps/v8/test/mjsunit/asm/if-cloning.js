// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var if0 = (function Module() {
  "use asm";
  function if0(i, j) {
    i = i|0;
    j = j|0;
    if ((i | 0) == 0 ? (j | 0) == 0 : 0) return 1;
    return 0;
  }
  return {if0: if0};
})().if0;
assertEquals(1, if0(0, 0));
assertEquals(0, if0(11, 0));
assertEquals(0, if0(0, -1));
assertEquals(0, if0(-1024, 1));


var if1 = (function Module() {
  "use asm";
  function if1(i, j) {
    i = i|0;
    j = j|0;
    if ((i | 0) == 0 ? (j | 0) == 0 : 1) return 0;
    return 1;
  }
  return {if1: if1};
})().if1;
assertEquals(0, if1(0, 0));
assertEquals(0, if1(11, 0));
assertEquals(1, if1(0, -1));
assertEquals(0, if1(-1024, 9));
