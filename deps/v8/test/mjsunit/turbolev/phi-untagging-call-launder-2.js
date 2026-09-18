// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap --turbolev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function bar(c) {
  let non_smi_int32 = 0x7ffffff1;
  return c ? non_smi_int32 : 42;
}

function foo(o, c) {
  let phi = bar(c);
  let v = phi & 1; // Int32 use
  o.x = phi; // Should store a Smi!
  return v;
}

let o = { x : 42 };
o.x = 17; // making non-const, but still Smi.

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo(o, false);
foo(o, false);

%OptimizeFunctionOnNextCall(foo);
foo(o, false);
foo(o, true);
