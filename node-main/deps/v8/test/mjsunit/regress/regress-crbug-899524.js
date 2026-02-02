// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function empty() {}

function baz(expected, found) {
  var start = "";
  found.length, start + 'x';
  if (expected.length === found.length) {
    for (var i = 0; i < expected.length; ++i) {
      empty(found[i]);
    }
  }
}

baz([1], new class A extends Array {}());

(function () {
  "use strict";
  function bar() {
    baz([1, 2], arguments);
  }
  function foo() {
    bar(2147483648, -[]);
  };
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();
