// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function optimize(crankshaft_test) {
  crankshaft_test();
  crankshaft_test();
  %OptimizeFunctionOnNextCall(crankshaft_test);
  crankshaft_test();
}

function f() {
  var v1 = 0;
  var v2 = -0;
  var t = v2++;
  v2++;
  return Math.max(v2++, v1++);
}

optimize(f);
