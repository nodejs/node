// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = [42];
function foo() {
  return a.filter(() => true);
}

%PrepareFunctionForOptimization(foo);
a[25] = undefined;
var result1 = foo();
assertArrayEquals([42, undefined], result1);
%OptimizeFunctionOnNextCall(foo);
var result2 = foo();
assertArrayEquals([42, undefined], result2);
if(%IsExperimentalUndefinedDoubleEnabled()) {
  assertTrue(%HasDoubleElements(result2));
}
