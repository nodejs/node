// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: differential_fuzz/mutations.js
var __v_0 = [1, 2, 3];
for (let __v_1 = 0; __v_1 < 3; __v_1 += 1) {
  let __v_2,
    __v_3 = 0;
  __v_0.foo = undefined;
  __prettyPrintExtra(0);
  try {
    __v_1 += 1;
  } catch (e) {
    __caught++;
  }
  __prettyPrintExtra(__v_2);
  __prettyPrintExtra(__v_3);
}
function foo() {
  let __v_4 = 0;
  __prettyPrintExtra(__v_4);
  return __v_4;
}
%OptimizeFunctionOnNextCall(foo);
foo();
(function () {
  let __v_5 = 0;
  __prettyPrintExtra(__v_5);
  return __v_5;
})(0);
(__v_6 => {
  let __v_7 = __v_6;
  __prettyPrintExtra(__v_6);
  __prettyPrintExtra(__v_7);
  return __v_7;
})(0);
{
  const __v_8 = 0;
  for (let __v_9 = 0; __v_9 < 10; __v_9++) {
    let __v_10 = __v_9;
    if (false) {
      const __v_11 = 0;
      foo();
      uninteresting;
      __prettyPrintExtra(__v_11);
      continue;
    } else {
      const __v_12 = 0;
      foo();
      uninteresting;
      __prettyPrintExtra(__v_12);
    }
    while (true) {
      let __v_13 = __v_9;
      uninteresting;
      __prettyPrintExtra(__v_13);
      break;
    }
    uninteresting;
    __prettyPrintExtra(__v_10);
    return 0;
  }
  __prettyPrintExtra(__v_8);
}
while (true) {
  function __f_0(__v_14) {
    uninteresting;
  }
  function __f_1(__v_15) {
    if (false) {
      let __v_16 = 0;
      uninteresting;
      __prettyPrintExtra(__v_16);
      throw 'error';
    }
  }
}
try {
  print("Hash: " + __hash);
  print("Caught: " + __caught);
  __prettyPrint(__v_0);
} catch (e) {}
