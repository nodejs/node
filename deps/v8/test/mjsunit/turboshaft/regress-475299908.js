// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function foo(x) {
  return x[x];
}

%PrepareFunctionForOptimization(foo);
foo("aMszj");

%OptimizeFunctionOnNextCall(foo);
foo(Uint8Array);
