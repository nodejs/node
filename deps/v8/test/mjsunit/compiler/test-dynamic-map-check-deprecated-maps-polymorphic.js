// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop --turbo-dynamic-map-checks --opt
// Flags: --no-always-opt

function f(o) {
  return o.b;
}

var o = {a:10, b:20};
var o1 = {a:10, b:20};
var o2 = {a:10, b:20};
var o3 = {a:10, b:20, c:30};
%PrepareFunctionForOptimization(f);
// Transition IC state to polymorphic.
f(o);
f(o3);
%OptimizeFunctionOnNextCall(f);
f(o);
assertOptimized(f);
f(o);

// Deprecates O's map.
o1.b = 10.23;
// Deoptimizes but retains code.
f(o1);
assertOptimized(f);

// Continues to use optimized code since deprecated map is still in the
// feedback. ICs don't drop deprecated maps in the polymoprhic case.
f(o);
f(o);
assertOptimized(f);
