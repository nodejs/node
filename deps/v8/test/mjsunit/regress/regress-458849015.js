// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

function __wrapTC(f = true) {
  try {
    return f();
  } catch (e) {
  }
}
var __hash = 0;
(function () {
    switch (typeof value) {
    }
  __prettyPrint = function (value = false) {
    let str = value;
    const hash = str;
    __hash = hash + __hash.toString();
  };
  __prettyPrintExtra = function (value) {
    __prettyPrint(value);
  };
})();
this.WScript = new Proxy({}, {
});
  (function __f_17() {
  }());
print("v8-foozzie source: v8/test/mjsunit/regress/regress-423059192.js");
function __f_19( __v_81) {
  let __v_82 = __wrapTC();
  return /* ArrayMutator: Duplicate an element */ /* ArrayMutator: Duplicate an element (replaced) */[__v_81 - __v_82, __v_82, __v_81 - __v_82];
}
  for (let __v_83 = 0; __v_83 < 1e4; ++__v_83) {
    let __v_84 = __wrapTC(() => __f_19());
    __prettyPrintExtra(__v_84);
  }
  print("Hash: " + __hash);
