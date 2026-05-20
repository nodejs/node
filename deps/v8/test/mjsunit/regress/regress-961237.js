// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const a = 1.1;
const b = null;

function f(x) { return -0 == (x ? a : b); }
%PrepareFunctionForOptimization(f);
assertEquals(false, f(true));
assertEquals(false, f(true));
%OptimizeFunctionOnNextCall(f);
assertEquals(false, f(false));
