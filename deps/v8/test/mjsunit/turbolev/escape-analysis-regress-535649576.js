// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-assert --turbolev
// Flags: --turbolev-escape-analysis

function createContainer() {
  let registry = [];
  return {
    register() {
      registry.push(1);
    },
    process() {
      this.register();
    }
  };
}
function test() {
    let container = createContainer();
    container.process();
}

%PrepareFunctionForOptimization(test);
for (let i = 0; i < 11; i++) { test(); }
%OptimizeFunctionOnNextCall(test);
test();
