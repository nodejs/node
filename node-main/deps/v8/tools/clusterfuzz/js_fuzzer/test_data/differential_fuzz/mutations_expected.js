// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: differential_fuzz/mutations.js
var __v_0 = [1, 2, 3];
__prettyPrintExtra(__v_0);
for (let __v_1 = 0; __v_1 < 3; __v_1 += 1) {
  let __v_2,
    __v_3 = 0;
  __prettyPrintExtra(__v_2);
  __prettyPrintExtra(__v_3);
  __v_0.foo = undefined;
  __prettyPrintExtra(__v_0);
  __prettyPrintExtra(0);
  try {
    __v_1 += 1;
    __prettyPrintExtra(__v_1);
  } catch (e) {
    __prettyPrintExtra(e);
  }
}
function foo() {
  let __v_4 = 0;
  __prettyPrintExtra(__v_4);
  return __v_4;
}
%OptimizeFunctionOnNextCall(foo);
__prettyPrintExtra(foo());
__prettyPrintExtra(function () {
  let __v_5 = 0;
  __prettyPrintExtra(__v_5);
  return __v_5;
}(0));
__prettyPrintExtra((__v_6 => {
  let __v_7 = __v_6;
  __prettyPrintExtra(__v_7);
  return __v_7;
})(0));
{
  const __v_8 = 0;
  __prettyPrintExtra(__v_8);
  for (let __v_9 = 0; __v_9 < 10; __v_9++) {
    let __v_10 = __v_9;
    __prettyPrintExtra(__v_10);
    if (false) {
      const __v_11 = 0;
      __prettyPrintExtra(__v_11);
      __prettyPrintExtra(foo());
      uninteresting;
      continue;
    } else {
      const __v_12 = 0;
      __prettyPrintExtra(__v_12);
      __prettyPrintExtra(foo());
      uninteresting;
    }
    while (true) {
      let __v_13 = __v_9;
      __prettyPrintExtra(__v_13);
      uninteresting;
      break;
    }
    uninteresting;
    return 0;
  }
}
while (true) {
  function __f_0(__v_14) {
    uninteresting;
  }
  function __f_1(__v_15) {
    if (false) {
      let __v_16 = 0;
      __prettyPrintExtra(__v_16);
      uninteresting;
      throw 'error';
    }
  }
}
try {
  print("Hash: " + __hash);
  print("Caught: " + __caught);
  __prettyPrint(__v_0);
} catch (e) {}
