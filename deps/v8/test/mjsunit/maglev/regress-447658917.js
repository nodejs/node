// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function () {
  const handler = {
    get: function () {
      return __dummy;
    }
  };
  __dummy = new Proxy(function () {
  }, handler);
  Object.freeze(__dummy);
})();

function __wrapTC() {
  return __dummy;
}


let caught = 0;

class C1 {
  constructor(x) {
    return x;
  }
}
class C2 extends C1 {
  field = (() => {})();
  constructor(x) {
    try {
      super(x);
    } catch (e) {
    }
  }
}
function foo() {
  let x = __wrapTC();
  new C2(x);
  try {
    this.test();
  } catch (e) {
    return 42;
  }
}

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(C2);
assertEquals(42, foo());
assertEquals(42, foo());

%OptimizeMaglevOnNextCall(foo);
assertEquals(42, foo());
