// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function foo(arr) {
  return arr.at(0x40000000);
}

%PrepareFunctionForOptimization(foo);
foo([]);
%OptimizeMaglevOnNextCall(foo);
foo([]);
