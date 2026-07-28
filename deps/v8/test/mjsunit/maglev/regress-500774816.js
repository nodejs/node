// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const N = 33000;
let params = [];
for (let i = 0; i < N; i++) params.push("p" + i.toString(36));

const src =
  "function bigargs(" + params.join(",") + "){ return arguments[0]; }" +
  " function caller(){ return bigargs(1); }";
(0, eval)(src);

%PrepareFunctionForOptimization(bigargs);
bigargs(1);
bigargs(1);

%PrepareFunctionForOptimization(caller);
caller();
caller();
%OptimizeFunctionOnNextCall(caller);
caller();
