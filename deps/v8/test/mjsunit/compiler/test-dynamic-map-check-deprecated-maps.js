// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop --dynamic-map-checks --opt
// Flags: --no-always-opt

function f(o) {
  return o.b;
}

var o = {a:10, b:20};
var o1 = {a:10, b:20};
var o2 = {a:10, b:20};
%PrepareFunctionForOptimization(f);
f(o);
%OptimizeFunctionOnNextCall(f);
f(o);
assertOptimized(f);
%PrepareFunctionForOptimization(f);
f(o);

// Deprecates O's map.
o1.b = 10.23;
// Deoptimizes but retains code.
f(o1);
assertOptimized(f);

// Deoptimizes and discards code.
f(o);
f(o);
assertUnoptimized(f);

// When we reoptimize we should include code for migrating deprecated maps.
%OptimizeFunctionOnNextCall(f);
f(o);
assertOptimized(f);

f(o2);
f(o2);
assertOptimized(f);
