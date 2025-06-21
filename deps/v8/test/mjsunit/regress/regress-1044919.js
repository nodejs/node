// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function main() {
  eval();
  function foo() {
    bla = [];
    bla.__proto__ = '';
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  Object.defineProperty(this, 'bla',
      {value: bla, configurable: false, writable: true});
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
  Object.defineProperty(this, 'bla',
      {value: bla, configurable: false, writable: false});
  foo();
})();
