// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev --no-lazy-feedback-allocation

function f0() {
  const v2 = [1];
  function f3(a4, a5, a6) {
    function f7(a8) {
      function F11(a13 = 12, a14, a15, a16) {
        if (!new.target) { throw 'must be called with new'; }
      }
      try { F11(a8, f0, 12, 12); } catch (e) {}
      return f7;
    }
    f7(v2).call();
    return f0;
  }
  try {
    throw 0;
  } catch (e) {
    v2.forEach(f3);
  }
  return f3;
}
f0.call();
%PrepareFunctionForOptimization(f0);
%OptimizeFunctionOnNextCall(f0);
f0();
