// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function inlineMe(a, b) {
  for (var i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) {
      return a[i];
    }
  }
}
%PrepareFunctionForOptimization(inlineMe);
function foo(array) {
  inlineMe(array, ["Dog"]);
}

%PrepareFunctionForOptimization(foo);

foo(["Dog"]);

// Can't treat holes as undefined
Array.prototype[123] = 42;

// HOLEY_SMI_ELEMENTS
foo([,]);

%OptimizeMaglevOnNextCall(foo);

foo(["abc"]);
