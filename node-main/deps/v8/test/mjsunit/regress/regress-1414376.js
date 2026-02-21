// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noturbo-inlining

(function () {
  assertEquals = function assertEquals() {};
})();
function __f_0() {}
function __f_6() {
  for (var __v_6 = -2147483648; __v_6 < 0; __v_6 += 100000) {
    assertEquals(__f_0(), Math.floor(__v_6 / 0));
  }
};
__f_6();
