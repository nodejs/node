// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let __callGC;
(function () {
  __callGC = function () {
  };
})();
function __wrapTC(f = true) {
    return f();
}
(function () {
  __prettyPrint = function (value = false) {
    let str = value;
    print(str);
  };
})();
this.WScript = new Proxy({}, {
});
assertEquals = (expected, found) => {
  __prettyPrint(found);
};
print("v8-foozzie source: v8/test/mjsunit/maglev/regress-389330329.js");
let __v_0 = () =>{
};
let __v_1 = __wrapTC(() => [, 2.5]);
function __f_0(__v_2) {
    let __v_3 = __v_1[0];
    for (let __v_4 = 0; __v_4 < __v_2; __v_4 += 1) {
      __v_4 += __v_3;
      __v_3 = 40;
    }
    try {
      __v_3.meh();
      __v_3 = __v_2, __callGC();
    } catch (__v_5) {}
    __v_0.y = __v_3;
}
  %PrepareFunctionForOptimization(__f_0);
__f_0(1);
  __f_0();
  %OptimizeMaglevOnNextCall(__f_0);
  __f_0();
  assertEquals(undefined, __v_0.y);
