// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(x, b) {
    if (b) return Math.trunc(+(x))
    else return Math.trunc(Number(x))
}

f("1", true);
f("2", true);
f("2", false);
%OptimizeFunctionOnNextCall(f);
f(3n);
