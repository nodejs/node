// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

Debug = debug.Debug;

function f() { return %_DebugIsActive() != 0; }

assertFalse(f());
assertFalse(f());
Debug.setListener(function() {});
assertTrue(f());
Debug.setListener(null);
assertFalse(f());

%OptimizeFunctionOnNextCall(f);
assertFalse(f());
assertOptimized(f);

Debug.setListener(function() {});
assertTrue(f());
assertOptimized(f);

Debug.setListener(null);
assertFalse(f());
assertOptimized(f);
