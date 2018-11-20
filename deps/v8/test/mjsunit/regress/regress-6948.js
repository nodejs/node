// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

var o = {};

function foo(s) { return o[s]; }

var s = 'c' + 'c';
foo(s);
foo(s);
%OptimizeFunctionOnNextCall(foo);
assertEquals(undefined, foo(s));
assertOptimized(foo);
assertEquals(undefined, foo('c' + 'c'));
assertOptimized(foo);
assertEquals(undefined, foo('ccddeeffgg'.slice(0, 2)));
assertOptimized(foo);
