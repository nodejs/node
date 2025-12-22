// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __wrapTC(f = true) {
    return f();
}
(function () {
  __prettyPrint = function (value = false) {
    let str = value;
    print(str);
  };
  __prettyPrintExtra = function () {
  };
})();
this.WScript = new Proxy({}, {
});
assertEquals = (expected, found) => {
  __prettyPrint(found);
};
print("v8-foozzie source: v8/test/mjsunit/maglev/phi-untagging-holeyfloat64-float64-input.js");
let __v_10 = __wrapTC(() => [1.1,, 3.3,,]);
function __f_0(__v_11) {
    let __v_12 = 17.25;
    let __v_13 = 3.333;
    for (let __v_14 = 0; __v_14 < __v_11; __v_14++) {
      __v_13 += __v_12;
      let __v_15 = __v_10[__v_14];
      if (__v_14 == 3) {
        __v_15 = -Math.abs(__v_15);
      }
      __v_12 = __v_15;
      __prettyPrintExtra(__v_12);
    }
    return __v_12;
}
  %PrepareFunctionForOptimization(__f_0);
 __f_0(4);
  %OptimizeMaglevOnNextCall(__f_0);
assertEquals(NaN, (/* FunctionCallMutator: Optimizing __f_0 */undefined, __prettyPrintExtra(), __f_0(4), %OptimizeFunctionOnNextCall(__f_0), __f_0(4)));
