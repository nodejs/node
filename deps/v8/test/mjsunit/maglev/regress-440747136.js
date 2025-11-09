// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-concurrent-inlining

function __wrapTC(f, permissive = true) {
  try {
    return f();
  } catch (e) {
  }
}
const __v_1 = 256;
const __v_2 = 4;
class __c_0 {
  constructor(   __v_25) {
      this.offset = __v_25 != undefined ? __v_25 : __c_0.offsetForSize();
  }
  static offsetForSize() {
    let __v_32 = __wrapTC(() => Math.floor(Math.random() * 8));
    let __v_33 = () => __v_31 - 2;
    let __v_34 = __wrapTC(() => __v_33 - 1);
    return __v_32 & __v_34;
  }
}
function __f_0(__v_52) {
    let __v_53 = new Array();
    for (let __v_54 = 0; __v_54 < __v_52; __v_54++) {
      __v_53[__v_54] = new __c_0();
    }
}
let __v_15 = () => [];
  for (let __v_114 = 0; __v_114 < __v_2; __v_114++) {
    __v_15[__v_114] = __f_0(__v_1);
  }
