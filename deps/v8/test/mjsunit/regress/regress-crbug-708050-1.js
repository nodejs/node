// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var v = {}

function foo() {
  v[0] = 5;
  v[-0] = 27;
  return v[0];
}

assertEquals(27, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(27, foo());
