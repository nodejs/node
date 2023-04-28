// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --interrupt-budget=100 --function-context-specialization

function __f_0() {
  return function __f_1() {
    __v_0.p = 42;
    for (let __v_2 = 0; __v_2 < 100; __v_2++) {
      try { this.p(); } catch (e) {}
    }
    this.p = __v_0;
  };
}
var __v_0 = __f_0();
var __v_1 = __f_0();
__v_1.prototype = {
  p() {
    this.q = new __v_0();
    for (let __v_3 = 0; __v_3 < 200; __v_3++);
  }
};
__v_0.prototype = {
  p() {}
};
new __v_1();
