// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: loop_variables.js
let __v_0 = 0;
function __f_0(__v_8, __v_9) {
  for (const __v_10 = __v_8; __v_10 < 10000; __v_10++) {
    console.log(__v_9);
  }
}
let __v_1 = false;
{
  let __v_11 = 10;
  while (something) {
    __v_0++;
    if (__v_0 > 5) break;
  }
  let __v_12 = "";
  do {
    __v_12 + "...";
  } while (call() || __v_1 > 0);
  console.log(__v_12);
}
let __v_2 = 0;
let __v_3 = 0;
let __v_4 = 0;
let __v_5 = 0;
while (call(__v_2)) {
  console.log(__v_3);
  __v_2 = __v_3;
  try {
    for (; __v_4 < 2; __v_4++) {
      {
        console.log(__v_5);
      }
    }
  } catch (__v_13) {}
  __v_5++;
}
let __v_6 = 10000;
let __v_7 = "foo";
while (__v_6-- > 0) {
  console.log(__v_7);
}
