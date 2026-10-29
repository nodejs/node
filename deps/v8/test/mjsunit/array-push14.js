// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function __f_17(__v_9) {
  var __v_10 = 0;
  var count = 100000;
  while (count-- != 0) {
    var l = __v_9.push(0.5);
    if (++__v_10 >= 2) return __v_9;
    __v_10 = {};
  }
  return __v_9;
}

let a = __f_17([2.2]);
assertEquals(a[0], 2.2);
assertEquals(a[10000], 0.5);
