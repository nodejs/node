// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function foo(o) { return o.x; }

%PrepareFunctionForOptimization(foo);
assertEquals(undefined, foo({}));
assertEquals(undefined, foo(1));
assertEquals(undefined, foo({}));
assertEquals(undefined, foo(1));
%OptimizeFunctionOnNextCall(foo);
assertEquals(undefined, foo({}));
assertOptimized(foo);
assertEquals(undefined, foo(1));
assertOptimized(foo);
