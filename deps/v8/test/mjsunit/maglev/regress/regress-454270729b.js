// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function NotString() {
}

NotString.prototype.slice = String.prototype.slice;

function foo(a) {
  try {
    return a.slice(-1);
  } catch {
  }
}
%PrepareFunctionForOptimization(foo);

foo(new NotString());

%OptimizeMaglevOnNextCall(foo);
foo(new NotString());
