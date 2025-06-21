// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function* h() { try {yield 42} finally {yield 43} };
function* g() { yield* h(); };

%PrepareFunctionForOptimization(g);
let x = g();
x.next();
%OptimizeFunctionOnNextCall(g);
x.throw(670);
try { x.next() } catch (e) { }
