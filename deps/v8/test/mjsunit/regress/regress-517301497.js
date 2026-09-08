// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

function __wrapTC(f = true) {
    return f();
}
const __v_0 = 10;
const __v_5 = __wrapTC(() => function () {
  function __f_32() {
    try {
      __f_31();
    } catch (__v_123) {
    }
  }
  return {
    probe: __f_32,
  };
}());

function __f_1(__v_124, __v_125) {
  if (__v_124 instanceof Array) {
    for (let __v_126 = 0; (() => {
      __v_5.probe( __v_126);
      return __v_126 < __v_124.length;
    })(); __v_126++) {
      __v_125[__v_0];
    }
  }
}

function __f_2() {
  const __v_127 = __f_43();

  __f_1(__v_127, __v_127);
}

__f_2();
const __v_7 = __wrapTC(() => [,]);
__f_1(__v_7, [__v_7]);
%PrepareFunctionForOptimization(__f_2);
const __v_8 = __wrapTC(() => %OptimizeFunctionOnNextCall(__f_2));
__f_2();
%OptimizeFunctionOnNextCall(__f_2);
__f_2();

function __f_43() {
  const __v_160 = () => -31644 >>> false;
    String.prototype.startsWith.call( "QR", __v_160);

  const __v_161 = /jHfoo(?<=bar)baz/ysgmv;
    for (let __v_162 =  __v_163 = __v_161; __v_163; __v_163--) {}
}
