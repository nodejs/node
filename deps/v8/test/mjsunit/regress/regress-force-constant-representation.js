// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test push double as tagged.
var a = [{}];
function f(a) {
  a.push(Infinity);
};
%PrepareFunctionForOptimization(f);
f(a);
f(a);
f(a);
%OptimizeFunctionOnNextCall(f);
f(a);
assertEquals([{}, Infinity, Infinity, Infinity, Infinity], a);
