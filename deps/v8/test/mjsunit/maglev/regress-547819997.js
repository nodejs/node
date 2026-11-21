// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

const boxes = [];

// Never called during warm-up, so this call site has no feedback and Maglev
// emits an unconditional deopt for it. The literal below is therefore only
// used by deopt frames and gets elided by escape analysis.
function sink(o) {
  boxes.push(o);
}

function foo(depth, take) {
  const o = {x: 1.5};
  if (depth > 0) foo(depth - 1, take);
  if (take) sink(o);
}

%PrepareFunctionForOptimization(foo);
foo(3, false);
foo(3, false);
%OptimizeMaglevOnNextCall(foo);
foo(3, false);

// Deopts the innermost activation eagerly and the outer ones lazily, so every
// activation materializes its own object.
foo(3, true);

assertEquals(4, boxes.length);
for (let i = 0; i < boxes.length; i++) {
  boxes[i].x = i;
}
for (let i = 0; i < boxes.length; i++) {
  assertEquals(i, boxes[i].x);
}
