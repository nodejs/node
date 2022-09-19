// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var o = {}
var p = {foo: 1.5}

function g(x) { return x.foo === +x.foo; }

%PrepareFunctionForOptimization(g);
assertEquals(false, g(o));
assertEquals(false, g(o));
%OptimizeFunctionOnNextCall(g);
assertEquals(false, g(o));  // Still fine here.
assertEquals(true, g(p));
%PrepareFunctionForOptimization(g);
%OptimizeFunctionOnNextCall(g);
assertEquals(false, g(o));  // Confused by type feedback.
