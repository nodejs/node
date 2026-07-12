// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --gc-interval=439

var __v_3;
function __f_2() {
  var __v_1 = new Array(3);
  __v_1[0] = 10;
  __v_1[1] = 15.5;
  __v_3 = __f_2();
  __v_1[2] = 20;
  return __v_1;
}

try {
  for (var __v_2 = 0; __v_2 < 3; ++__v_2) {
    __v_3 = __f_2();
  }
}
catch (e) { }
