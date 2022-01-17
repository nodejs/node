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
%PrepareFunctionForOptimization(f);
f(o);
%OptimizeFunctionOnNextCall(f);
f(o);
assertOptimized(f);

// Deprecates o's map.
o1.b = 10.23;

// Bails out but retains code.
f(o1);
assertOptimized(f);

// Passing in original object should not cause any deopts.
f(o);
f(o);
assertOptimized(f);

// o and o2 have the same Map, so there should be no deopts.
f(o2);
f(o2);
assertOptimized(f);
