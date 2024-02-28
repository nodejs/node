// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function foo () {
  try {
    [] = true;
  } catch (e) {
    return e;
  }
}

%PrepareFunctionForOptimization(foo);
const error1 = foo();
%OptimizeFunctionOnNextCall(foo);
assertEquals(error1.message, foo().message);
