// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony

function foo() {
  return Math.clz32(12.34);
}

foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
