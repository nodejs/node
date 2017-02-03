// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --ignition --turbo-from-bytecode --turbo-filter=f

function f(x) { return x + 23 }
function g(x) { return f(x) + 42 }

assertEquals(23, f(0));
assertEquals(24, f(1));
assertEquals(67, g(2));
assertEquals(68, g(3));

// Optimize {g} with Crankshaft, causing {f} to be inlined.
%OptimizeFunctionOnNextCall(g);
assertEquals(65, g(0));

// Optimize {f} with Turbofan, after it has been inlined.
%OptimizeFunctionOnNextCall(f);
assertEquals(23, f(0));
