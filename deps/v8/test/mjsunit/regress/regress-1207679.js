// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc --interrupt-budget=1000 --no-lazy-feedback-allocation

var __v_5;
function __v_1() {
  var PI = {
    get() {}
  };
  function __v_5() {
    Object.defineProperty(PI, 'func', {
    });
    '𝌆'.match();
  }
  __v_5(...[__v_5]);
  try {
    __v_1();
  } catch (PI) {}
}
__v_1();
gc();
__v_1();
