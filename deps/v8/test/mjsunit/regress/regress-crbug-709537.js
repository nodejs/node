// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  return { 0: {}, x: {} };
}
var ref = foo();
assertEquals(ref, foo());
assertEquals(ref, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(ref, foo());
