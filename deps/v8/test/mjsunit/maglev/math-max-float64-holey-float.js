// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function foo(a) {
  return Math.max(a[0], 5.2);
}
%PrepareFunctionForOptimization(foo);

let normalArray = [4.5, 3.2, 4.5, 3.2];
let holeyArray = [ , 4.5, 6.5, 6.5];

foo(holeyArray);
foo(normalArray);

%OptimizeMaglevOnNextCall(foo);

assertEquals(5.2, foo(normalArray));
assertTrue(isNaN(foo(holeyArray)));

// No deopts.
assertTrue(isMaglevved(foo));
