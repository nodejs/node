// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


(function TestSmi() {
  var v_0 = {};
  function f_0(constructor, closure) {
    var v_2 = { value: 0 };
    v_4 = closure(constructor, 1073741823, v_0, v_2);
    assertEquals(1, v_2.value);
  }
  function f_1(constructor, val, deopt, v_2) {
    if (!new constructor(val, deopt, v_2)) {
    }
  }
  function f_10(constructor) {
    f_0(constructor, f_1);
    f_0(constructor, f_1);
    f_0(constructor, f_1);
  }
  function f_12(val, deopt, v_2) {
    v_2.value++;
  }
  f_10(f_12);
})();

(function TestHeapNumber() {
  var v_0 = {};
  function f_0(constructor, closure) {
    var v_2 = { value: 1.5 };
    v_4 = closure(constructor, 1073741823, v_0, v_2);
    assertEquals(2.5, v_2.value);
  }
  function f_1(constructor, val, deopt, v_2) {
    if (!new constructor(val, deopt, v_2)) {
    }
  }
  function f_10(constructor) {
    f_0(constructor, f_1);
    f_0(constructor, f_1);
    f_0(constructor, f_1);
  }
  function f_12(val, deopt, v_2) {
    v_2.value++;
  }
  f_10(f_12);
})();
