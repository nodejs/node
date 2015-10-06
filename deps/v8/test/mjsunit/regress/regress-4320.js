// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-debug-as debug

var Debug = debug.Debug;

function f() { g(); }

function g() { }

f();
f();
%OptimizeFunctionOnNextCall(f);
f();

Debug.setListener(function() {});
Debug.setBreakPoint(g, 0);

f();
