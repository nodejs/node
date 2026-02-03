// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-future

function __f_0(__v_0, __v_1) {
    if (__v_1 != __v_0) 'xxxxxxxxxxxxxxxxx' + __v_1;
}
%PrepareFunctionForOptimization(__f_0);

__f_0("", ".");

(function () {
  try {
    for (__v_2 = 2000; __v_2; --__v_2) {
    }
    __f_0(3, "122".length);
  } catch (e) {}
})();
