// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let __v_0 = 0;
let __v_1 = 0;

console.log(__v_0, __v_1);

(function () {
  let __v_2 = 0;
  console.log(__v_0, __v_1, __v_2);
})();

let __v_3 = 0;

console.log(__v_0, __v_1, __v_3);

(function (__v_7) {
  let __v_4 = 0;

  console.log(__v_0, __v_1, __v_3, __v_4, __v_7);
  {
    let __v_5 = 0;
    var __v_6 = 0;
    console.log(__v_0, __v_1, __v_3, __v_4, __v_5, __v_6, __v_7);
  }
  console.log(__v_0, __v_1, __v_3, __v_4, __v_6, __v_7);
})(42);
console.log(__v_0, __v_1, __v_3);
