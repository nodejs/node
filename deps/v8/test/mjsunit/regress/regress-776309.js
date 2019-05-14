// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function C() { }

function f(b) {
  var o = new C();
  // Create out-of-object properties only on one branch so that escape
  // analysis does not analyze the property array away.
  if (b) o.t = 1.1;
  %_DeoptimizeNow();
  return o.t;
}

// Finish slack tracking for C.
for (var i = 0; i < 1000; i++) new C();

f(true);
f(true);
f(false);

%OptimizeFunctionOnNextCall(f);

assertEquals(1.1, f(true));
