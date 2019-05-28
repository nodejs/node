// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

a = new Int8Array(1);

function f(i) {
  return i in a;
}

assertTrue(f(0));
%OptimizeFunctionOnNextCall(f);
assertFalse(f(-1));
