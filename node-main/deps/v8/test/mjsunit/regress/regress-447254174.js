// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let obj = { y: 9007199254740992 };
let arr = [, 2.5];

function foo(n) {
    let x = arr[0];

    for (let i = 0; i < n; i += 1) {
      i += x;
      x = 40;
    }

    try {
      x.meh();
    } catch (e) {}

    obj.y = x;
}

%PrepareFunctionForOptimization(foo);
arr.fill();
foo(1);
%OptimizeMaglevOnNextCall(foo);
foo(0);
assertEquals(undefined, obj.y);
