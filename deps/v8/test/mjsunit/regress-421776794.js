// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

  __getRandomProperty = function () {
  };
let __callGC;
(function () {
  __callGC = function () {
  };
})();
 {
  function* __f_0(...__v_1) {
      for (const __v_2 of __v_1) {
        __v_2[ 788133];
        __v_2[__getRandomProperty()], __callGC(true);
        yield __v_2;
      }
  }
[], [...__f_0(1, /* NumberMutator: Replaced 1.5 with 4.5 */4.5)];
[], [, ...__f_0(/* NumberMutator: Replaced 1 with -8 */-8, 1.5)];
[], [...__f_0(1, 2, 2.5, 3.5)];
[], [...__f_0(2.5, 1, 3.5, 4)];
[], [...__f_0(2.5, 3.5, 1, 4)];
[ {
    }], [...__f_0(0, {
    }, {
    })];
[], [...__f_0(1, 1.5, "2")];
  function __f_6(__v_9) {
      return [...__v_9];
  }
[], __f_6(__f_0(1, 0.1, "1"));
  function __f_11() {
  }
[], /* FunctionCallMutator: Run to stack limit __f_11 */() => {
      return __f_11();
    };
}
