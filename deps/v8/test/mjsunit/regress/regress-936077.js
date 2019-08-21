// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --allow-natives-syntax
// Flags: --concurrent-inlining --function-context-specialization

function main() {
  var obj = {};
  function foo() {
    return obj[0];
  };
  %PrepareFunctionForOptimization(foo);
  ;
  gc();
  obj.x = 10;
  %OptimizeFunctionOnNextCall(foo);
  foo();
}
main();
main();
