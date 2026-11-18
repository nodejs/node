// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

let ta = new Int8Array();
let o = { x : 42 };

function foo(x) {
  ta[x];
  o.x = 0 + x;
  o.x = x;
}

%PrepareFunctionForOptimization(foo);
foo(0);

%OptimizeFunctionOnNextCall(foo);
foo(%AllocateHeapNumber());
%HeapObjectVerify(o);
