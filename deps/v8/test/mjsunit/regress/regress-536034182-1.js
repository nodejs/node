// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function inlineMe(thing) {
  // Will generate array transition code, PACKED_SMI_ELEMENTS -> PACKED_ELEMENTS
  thing[2] = thing;
}
%PrepareFunctionForOptimization(inlineMe);

function bar() {
  // WeakSet is not an array and won't transition.
  // When `inlineMe` is inlined here, the transition branch is dead.
  inlineMe(WeakSet);

  const array = [0]; // PACKED_SMI_ELEMENTS
  inlineMe(array);
}
%PrepareFunctionForOptimization(bar);

bar();
bar();

%OptimizeMaglevOnNextCall(bar);

bar();
