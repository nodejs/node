// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-optimistic-peeled-loops
// Flags: --maglev-non-eager-inlining  --max-maglev-eager-inlined-bytecode-size=0

let o = { x : 42 };
o.x = "abc"; // Making non-const and AnyTagged representation.

function store(v) {
  o.x = v;
}

function foo() {
  for (let phi = 0; phi < 10; phi) {
    ~phi; // Smi feedback, will insert CheckedSmiUntag
    store(phi);
    phi = 0x7fffffff;
    o.x = phi; // Tagging {phi}.
  }
}

%PrepareFunctionForOptimization(store);
%PrepareFunctionForOptimization(foo);
foo();
foo();

%OptimizeMaglevOnNextCall(foo);
foo();
