// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags:

(function () {
  const handler = {
  };
  __dummy = new function () {
  };
})();
function __wrapTC(f, permissive = true) {
  try {
    return f();
  } catch (e) {
    if (permissive) {
      return __dummy;
    }
  }
}
let __v_10 = () => new WebAssembly.Global();
function __f_2() {
    for (let __v_36 = 0; __v_36 < 61; __v_36++) {
      let __v_37 = __wrapTC(() => new WebAssembly.Global(), false);
        [ __v_37];
      let __v_38 = 10;
() => ({
        next() {
            __v_38--;
        }
      });
      function __f_3() {}
    }
}
  __v_10[Symbol.toPrimitive] = __f_2;
const __v_11 = /(?=.)1SU6?/um;
  __v_11.__lookupSetter__(__v_10);
