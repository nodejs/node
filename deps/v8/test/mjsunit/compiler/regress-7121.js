// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() { %ToLength(42n) }
%PrepareFunctionForOptimization(foo);
assertThrows(foo, TypeError);
%OptimizeFunctionOnNextCall(foo);
assertThrows(foo, TypeError);
