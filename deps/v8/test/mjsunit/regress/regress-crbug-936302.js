// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

(function() {
'use strict';

function baz() {
  'use asm';
  function f() {}
  return {f: f};
}

function foo(x) {
  baz(x);
  %DeoptimizeFunction(foo);
};
%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
})();
