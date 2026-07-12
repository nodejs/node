// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev
let a = new Float32Array(1000);
function foo(i) {
    return a[i];
}

%PrepareFunctionForOptimization(foo);
assertEquals(0, foo(0));
assertEquals(0, foo(0));

%OptimizeMaglevOnNextCall(foo);
assertEquals(undefined, foo(0x40000000)); // On 32 bits, this will overflow the array index.
