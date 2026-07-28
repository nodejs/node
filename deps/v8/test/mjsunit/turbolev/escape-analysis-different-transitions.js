// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis

class C {
  constructor(x) { this.x = x; }
}

function foo(x, b1, b2) {
  let o = new C(x);
  // TODO(dmercadier): support lowering CheckMaps to
  // DeoptimizeIf(!CompareMaps), which is required in order to elide {o}.
  //%AssertEscapeAnalysisElided(o);

  // We'll do 2 transitions in 2 branches.
  if (b1) {
    o.y = 15;
  } else {
    o.z = 19;
  }
  // Now the Map is a Phi of 2 maps.

  // And we'll now need map checks, which will have to lead to dynamic checks
  // since they will fail if b1!=b2.
  if (b2) {
    return o.y;
  } else {
    return o.z;
  }
}

%PrepareFunctionForOptimization(C);
%PrepareFunctionForOptimization(foo);
assertEquals(15, foo(10, true, true));
assertEquals(19, foo(10, false, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(15, foo(10, true, true));
assertEquals(19, foo(10, false, false));
