// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-assert-types

// Check that constant-folding of arithmetic results in identical nodes.
(function() {
function foo(x) {
  %TurbofanStaticAssert(1 * x == x + 0);
};
%PrepareFunctionForOptimization(foo);
foo(121);
foo(122);
%OptimizeFunctionOnNextCall(foo);
foo(123);
})();
