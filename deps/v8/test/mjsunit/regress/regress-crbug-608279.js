// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --always-turbofan --no-lazy

function __f_38() {
  try {
    throw 0;
  } catch (e) {
    eval();
    var __v_38 = { a: 'hest' };
    __v_38.m = function () { return __v_38.a; };
  }
  return __v_38;
}
var __v_40 = __f_38();
__v_40.m();
