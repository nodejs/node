// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function test_push(o) {
  o.push(1);
}

%PrepareFunctionForOptimization(test_push);
test_push([{}]);
%OptimizeMaglevOnNextCall(test_push);
Object.defineProperty(Array.prototype, 1, {get(){}});
assertThrows(()=>test_push([{}]));
