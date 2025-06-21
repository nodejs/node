// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

(function () {
  async function foo() {
    for (let i = 0; i == 1; i++) {
      780109, { get: function() { } }; await 7;
    }
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  %OptimizeMaglevOnNextCall(foo);
  foo();
})();
