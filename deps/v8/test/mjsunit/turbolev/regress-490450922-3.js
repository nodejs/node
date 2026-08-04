// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

// Creating an object with a non-const HeapObject field.
let o = { x : "abc" };
o.x = [];

function foo(c) {
  let phi = c ? 0x7fffffff : 42;
  let truncated = phi | 0;
  o.x = phi;
  return truncated;
}

%PrepareFunctionForOptimization(foo);
foo(true);

o.x = "abc";
%OptimizeFunctionOnNextCall(foo);
foo(true);
assertEquals(0x7fffffff, o.x);
// TODO(dmercadier): re-enable this assertOptimized once Phi untagging has been
// re-enabled.
// assertOptimized(foo);

// Even with a Smi it should be fine because we should ForceHeapObject when
// retagging.
foo(false);
assertEquals(42, o.x);
// TODO(dmercadier): re-enable this assertOptimized once Phi untagging has been
// re-enabled.
//assertOptimized(foo);
