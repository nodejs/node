// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-fast-api

const fast_c_api = new d8.test.FastCAPI();
let deopt = false;
const obj = {
  valueOf() {
    if (deopt) {
      %DeoptimizeFunction(foo);
      d8.terminateNow();
    }
    return 1;
  }
};
function foo() {
  try {
    fast_c_api.call_to_number(obj);
  } catch (e) {
  }
}
%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
setTimeout(() => {
  Promise.resolve().then(() => {
    deopt = true;
    foo();
    assertUnreachable();
  });
}, 0);
