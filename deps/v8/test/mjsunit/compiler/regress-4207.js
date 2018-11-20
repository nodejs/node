// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar() { return 0/0 && 1; }
assertEquals(NaN, bar());
%OptimizeFunctionOnNextCall(bar);
assertEquals(NaN, bar());

function foo() { return 0/0 || 1; }
assertEquals(1, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(1, foo());
