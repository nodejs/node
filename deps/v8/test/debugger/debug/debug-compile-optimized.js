// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --opt --no-always-opt

Debug = debug.Debug;

Debug.setListener(function() {});

function f() {}
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
assertOptimized(f);

var bp = Debug.setBreakPoint(f);
assertUnoptimized(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
assertUnoptimized(f);

Debug.clearBreakPoint(bp);
%OptimizeFunctionOnNextCall(f);
f();
assertOptimized(f);

Debug.setListener(null);
