// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-turbo-inlining --opt --no-always-opt


function bar() { return foo(); }
function foo() { return %IsBeingInterpreted(); }

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);

assertTrue(bar());
assertTrue(bar());
%OptimizeFunctionOnNextCall(bar);
assertTrue(bar());
