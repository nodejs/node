// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function Class(a1, a2) {
  return a1 === a2;
}
function foo(x) {
  const instance = new Class(x, x + 1);
  if (instance) {
  } else {
    fail("ToBoolean of instance should not be false, even though the constructor returns false.")
  }
}

%PrepareFunctionForOptimization(Class);
%PrepareFunctionForOptimization(foo);
foo(0);
foo(0);
%OptimizeMaglevOnNextCall(foo);
foo(0);
