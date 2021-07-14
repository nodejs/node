// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

function f() {
  let r = [];
  let o1 = { x: 5 };
  let o2 = { y: 5, inner: { n: 5 } };
  r.push(o1);
  r.push(o2);
  return r;
}

%PrepareFunctionForOptimization(f);
{
  let r = f();
  f();

  // Deprecate all the maps.
  r[0].x = 5.5;
  r[1].y = 5.5;
  r[1].inner.n = 5.5;
}
// We will optimize despite deprecated maps in the boilerplates.
// until code finalization.
%OptimizeFunctionOnNextCall(f);
f();
assertOptimized(f);
