// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const Probe = (function() {
  return {
    probe: () => {},
  };
})();

const foo = (a1) => {
  function bar() {
    for (let i = 0; i < 5; i++) {
      Probe.probe();
      eval();
    }
  }
  %PrepareFunctionForOptimization(bar);
  return bar();
};

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
