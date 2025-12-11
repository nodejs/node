// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --interrupt-budget=1024 --noturbo-loop-variable

function opt() {
  const array = [-1, 1];
  array.shift();
}

%PrepareFunctionForOptimization(opt);
opt();
opt();
%OptimizeFunctionOnNextCall(opt);
opt();
opt();
