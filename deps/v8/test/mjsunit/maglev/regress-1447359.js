// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function f(o) {
  return o.x + o.x;
}

%PrepareFunctionForOptimization(f);
f("a");
f(1);
%OptimizeMaglevOnNextCall(f);
f(1);
