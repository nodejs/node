// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function foo() {}
const argCount = 65525;
const args = new Array(argCount);
const bar = Function.prototype.bind.apply(foo, args);
function test() {
    new bar(9, 10);
}

%PrepareFunctionForOptimization(test);
try { test(); } catch {}

%OptimizeFunctionOnNextCall(test);
try { test(); } catch {}
