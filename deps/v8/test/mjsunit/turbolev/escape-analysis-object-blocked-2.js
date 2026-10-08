// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function foo(arg, out_obj) {
  let o = { x : arg };
  out_obj.y = o;
  return 42;
}

let o = {};

%PrepareFunctionForOptimization(foo);
foo(42, o);
assertEquals({ y : { x : 42 } }, o);
o = {};

foo(42, o);
assertEquals({ y : { x : 42 } }, o);
o = {};


%OptimizeFunctionOnNextCall(foo);
foo(42, o);
assertEquals({ y : { x : 42 } }, o);
assertOptimized(foo);
