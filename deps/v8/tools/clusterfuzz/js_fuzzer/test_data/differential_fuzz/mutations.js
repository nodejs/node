// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Print after declaration.
var __v_0 = [1, 2, 3];

// Don't print after declarations or assigments in loops.
for (let __v_1 = 0; __v_1 < 3; __v_1 += 1) {

  // Print after multiple declarations.
  let __v_2, __v_3 = 0;

  // Print after assigning to member.
  __v_0.foo = undefined;

  // Replace with deep printing.
  print(0);

  // Print exception.
  try {
    // Print after assignment.
    __v_1 += 1;
  } catch(e) {}
}

// Print within function.
function foo() {
  let __v_4 = 0;
  return __v_4;
}

// Print on call site.
%OptimizeFunctionOnNextCall(foo);
foo();

// Print within and on call site.
(function () {
  let __v_5 = 0;
  return __v_5;
})(0);

// Print within and on call site.
((__v_6) => {
  let __v_7 = __v_6;
  return __v_7;
})(0);

// Complex blocks.
{
  const __v_8 = 0;
  for (let __v_9 = 0; __v_9 < 10; __v_9++) {
    let __v_10 = __v_9;
    if (false) {
      const __v_11 = 0;
      foo();
      uninteresting;
      continue;
    } else {
      const __v_12 = 0;
      foo();
      uninteresting;
    }
    while (true) {
      let __v_13 = __v_9;
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
      uninteresting;
      throw 'error';
    }
  }
}
