// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {}
f.prototype = 42;

function g(a) {
  a.stack;
}

%PrepareFunctionForOptimization(g);
Error.captureStackTrace(f);
g(f);
g(f);
g(f);
%OptimizeFunctionOnNextCall(g);
g(f);
