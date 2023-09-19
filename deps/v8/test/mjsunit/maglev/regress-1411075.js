// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function foo(__v_6, __v_7) {
  return +__v_6.x;
}

%PrepareFunctionForOptimization(foo);
foo({ x: 42 });
foo(false);
%OptimizeMaglevOnNextCall(foo);
foo(false);

assertEquals(NaN, foo(false));
