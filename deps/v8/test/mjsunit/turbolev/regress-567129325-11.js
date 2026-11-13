// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

const obj = { prop: {} };
obj.prop = "str";

function foo(x) {
  const v = x + 1;
  obj.prop = v;
  "a".startsWith("a", v);
  return new Array(v, "x");
}

%PrepareFunctionForOptimization(foo);
foo(0x40000000);
%OptimizeFunctionOnNextCall(foo);
foo(0x40000000);
