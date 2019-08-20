// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --code-comments --print-code

(function simple_test() {
  function fib(n) {
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
  }

  // Call a number of times to trigger optimization.
  for (let i = 0; i < 100; ++i) {
    fib(8);
  }
})();

(function test_asm() {
  function asm() {
    'use asm';
    function f() {}
    return f;
  }

  var m = asm();
})();
