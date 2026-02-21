// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var non_const_true = true;

function f() {
  return non_const_true || (f() = this);
}

%PrepareFunctionForOptimization(f);
assertTrue(f());
assertTrue(f());
%OptimizeFunctionOnNextCall(f);
assertTrue(f());
