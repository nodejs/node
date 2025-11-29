// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function foo() {}
function bar(...args) {
  foo.apply(...args);
}

%PrepareFunctionForOptimization(bar);
assertThrows('bar(2,3,4)');
assertThrows('bar(2,3,4)');
%OptimizeMaglevOnNextCall(bar);
assertThrows('bar(2,3,4)');
