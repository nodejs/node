// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turboprop
var __v_0 = {
  x: 2,
  y: 1
};
function __f_0() {}
function __f_1() {
  for (var __v_1 = 0; __v_1 < 100000; __v_1++) {
    var __v_2 = __v_0.x + __f_0();
  }
  var __v_3 = [{
    x: 2.5,
    y: 1
  }];
}
__f_1();
__f_1();
