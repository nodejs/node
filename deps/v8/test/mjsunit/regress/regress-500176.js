// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function __f_0(p) {
  var __v_0 = -2147483642;
  for (var __v_1 = 0; __v_1 < 10; __v_1++) {
    if (__v_1 > 5) { __v_0 = __v_0 + p; break;}
  }
}
for (var __v_2 = 0; __v_2 < 100000; __v_2++) __f_0(42);
__v_2 = { f: function () { return x + y; },
          2: function () { return x - y} };
