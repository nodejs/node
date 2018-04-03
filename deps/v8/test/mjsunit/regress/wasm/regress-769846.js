// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module() {
  "use asm";
  function div_(__v_6) {
    __v_6 = __v_6 | 0;
  }
  return { f: div_}
};
var __f_0 = Module().f;
__v_8 = [0];
__v_8.__defineGetter__(0, function() { return __f_0(__v_8); });
__v_8[0];
