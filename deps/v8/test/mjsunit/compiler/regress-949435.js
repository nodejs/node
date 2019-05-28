// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  const v6 = new String();
  v6.POSITIVE_INFINITY = 1337;
  const v8 = Object.seal(v6);
  v8.POSITIVE_INFINITY = Object;
}

f();
%OptimizeFunctionOnNextCall(f)
f();
