// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --opt --no-always-opt

////////////////////////////////////////////////////////////////////////////////
// This is a variant of regress-99540-1.js that does not rely on concurrent
// recompilation.
////////////////////////////////////////////////////////////////////////////////

function mkbar() { return function(x) { x.p } }
var bar = mkbar();
function foo(x) { bar(x) }

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(mkbar);

// Create map transitions such that a's final map is not stable.
var dummy = [];
dummy.p = 0;
dummy.q = 0;

var a = [];
a.p = 42;

var b = [];
b.p = 42;

// Warm-up.
foo(a);
foo(a);

// Trigger optimization of bar, based on PACKED_SMI_ELEMENTS feedback.
%OptimizeFunctionOnNextCall(bar);
bar(a);
assertOptimized(bar);
%PrepareFunctionForOptimization(bar);

// Change a's map from PACKED_SMI_ELEMENTS to PACKED_ELEMENTS and run new
// instance of mkbar() in the interpreter s.t. bar's load feedback changes
// accordingly (thanks to feedback vector sharing).
a[0] = {};
mkbar()(a);
assertOptimized(bar);
// If we were to call the optimized bar now, it would deopt.

// Instead we trigger optimization of foo, which will inline bar (this time
// based on the new PACKED_ELEMENTS map.
assertOptimized(bar);
%OptimizeFunctionOnNextCall(foo);
assertOptimized(bar);
foo(a);
assertOptimized(bar);
assertOptimized(foo);
%PrepareFunctionForOptimization(foo);

// Now call the optimized foo on an object that has the old PACKED_SMI_ELEMENTS
// map. This will lead to an eager deopt of foo when the inlined bar sees that
// old map.
foo(b);
assertUnoptimized(foo);
assertOptimized(bar);

// Now ensure there is no deopt-loop. There used to be a deopt-loop because, as
// a result of over-eager checkpoint elimination, we used to deopt into foo
// (right before the call to bar) rather than into bar (right before the load).
%OptimizeFunctionOnNextCall(foo);
foo(b);
assertOptimized(foo);
