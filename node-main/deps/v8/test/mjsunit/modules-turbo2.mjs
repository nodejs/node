// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --allow-natives-syntax

export let x = 0;

function foo() { return x++ };

function gaga(f) { return f() };

%PrepareFunctionForOptimization(gaga);
assertEquals(0, gaga(foo));
assertEquals(1, gaga(foo));
%OptimizeFunctionOnNextCall(gaga);
assertEquals(2, gaga(foo));
