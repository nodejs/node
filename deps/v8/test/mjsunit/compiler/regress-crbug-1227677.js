// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --interrupt-budget=1024

(function() {
  async function foo() {
    for (let j = 0; j < 1; j++) {
      const x = 2n * 3n;
      for (let k = 0; k < 4; k++) {
        function unused() {}
        x >> 5n;
        for (let i = 0; i < 100; i++) ;
        for (let i = 0; i == 6; i++) await 7;
      }
    }
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

(function() {
  async function foo() {
    for (let j = 0; j < 1; j++) {
      const x = 2n * 3n;
      for (let k = 0; k < 4; k++) {
        function unused() {}
        x << 5n;
        for (let i = 0; i < 100; i++) ;
        for (let i = 0; i == 6; i++) await 7;
      }
    }
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

(function() {
  async function foo() {
    for (let j = 0; j < 1; j++) {
      const x = 2n * 3n;
      for (let k = 0; k < 4; k++) {
        function unused() {}
        x / 5n;
        for (let i = 0; i < 100; i++) ;
        for (let i = 0; i == 6; i++) await 7;
      }
    }
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

(function() {
  async function foo() {
    for (let j = 0; j < 1; j++) {
      const x = 2n * 3n;
      for (let k = 0; k < 4; k++) {
        function unused() {}
        x % 5n;
        for (let i = 0; i < 100; i++) ;
        for (let i = 0; i == 6; i++) await 7;
      }
    }
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

(function() {
  async function foo() {
    for (let j = 0; j < 1; j++) {
      const x = 2n * 3n;
      for (let k = 0; k < 4; k++) {
        function unused() {}
        x * 5n;
        for (let i = 0; i < 100; i++) ;
        for (let i = 0; i == 6; i++) await 7;
      }
    }
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

(function() {
  async function foo() {
    for (let j = 0; j < 1; j++) {
      const x = 2n * 3n;
      for (let k = 0; k < 4; k++) {
        function unused() {}
        x ^ 5n;
        for (let i = 0; i < 100; i++) ;
        for (let i = 0; i == 6; i++) await 7;
      }
    }
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

(function() {
  async function foo() {
    for (let j = 0; j < 1; j++) {
      const x = 2n * 3n;
      for (let k = 0; k < 4; k++) {
        function unused() {}
        x & 5n;
        for (let i = 0; i < 100; i++) ;
        for (let i = 0; i == 6; i++) await 7;
      }
    }
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

(function() {
  async function foo() {
    for (let j = 0; j < 1; j++) {
      const x = 2n * 3n;
      for (let k = 0; k < 4; k++) {
        function unused() {}
        x | 5n;
        for (let i = 0; i < 100; i++) ;
        for (let i = 0; i == 6; i++) await 7;
      }
    }
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

(function() {
  async function foo() {
    for (let j = 0; j < 1; j++) {
      const x = 2n * 3n;
      for (let k = 0; k < 4; k++) {
        function unused() {}
        x + 5n;
        for (let i = 0; i < 100; i++) ;
        for (let i = 0; i == 6; i++) await 7;
      }
    }
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

(function() {
  async function foo() {
    for (let j = 0; j < 1; j++) {
      const x = 2n * 3n;
      for (let k = 0; k < 4; k++) {
        function unused() {}
        x < 5n;
        for (let i = 0; i < 100; i++) ;
        for (let i = 0; i == 6; i++) await 7;
      }
    }
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();
