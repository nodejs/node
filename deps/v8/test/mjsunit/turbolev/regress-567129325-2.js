// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

let ta = new Int8Array();
let o = { x: "a" };

function store(x, w, b) {
  if (b) {
    o.x = x;
    o.x = w;
  }
}

function foo(x, b) {
  ta[x];
  store(x, 0 + x, b);
}

%PrepareFunctionForOptimization(store);
%PrepareFunctionForOptimization(foo);

store("a", "a", true);
foo(0, false);

%OptimizeFunctionOnNextCall(foo);
foo(%AllocateHeapNumber(), true);
%HeapObjectVerify(o);
