// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan --maglev-assert-types

function foo() {
  for (let i = 0; i < 5; i++) {
    function bar(v) {
      try {
        (v ?? i).abc;
      } catch (e) {}
      return {};
    }
    bar();
    bar(bar());
  }
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
