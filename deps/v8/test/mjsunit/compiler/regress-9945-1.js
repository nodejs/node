// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt
// Flags: --concurrent-recompilation --block-concurrent-recompilation

function foo(x) { bar(x) }
function bar(x) { x.p }

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(bar);

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

// Trigger optimization of bar but don't yet complete it.
%OptimizeFunctionOnNextCall(bar, "concurrent");
foo(a);
%PrepareFunctionForOptimization(bar);

// Change a's map from PACKED_SMI_ELEMENTS to PACKED_ELEMENTS and run bar in the
// interpreter (via foo) s.t. bar's load feedback changes accordingly.
a[0] = {};
foo(a);
assertUnoptimized(bar, "no sync");

// Now finish the optimization of bar, which was based on the old
// PACKED_SMI_ELEMENTS feedback.
%UnblockConcurrentRecompilation();
assertOptimized(bar);
// If we were to call the optimized bar now, it would deopt.

// Instead we trigger optimization of foo, which will inline bar (this time
// based on the new PACKED_ELEMENTS map.
%OptimizeFunctionOnNextCall(foo);
foo(a);
assertOptimized(foo);
%PrepareFunctionForOptimization(foo);
assertOptimized(bar);

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
