// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --single-threaded

function foo(b) {
  foo.bind();
  if (b) {
    %OptimizeFunctionOnNextCall(foo);
  }
  for (let i = 0; i < 10000; i++) {}
  foo instanceof foo;
}

  %PrepareFunctionForOptimization(foo);
foo(false);
%OptimizeMaglevOnNextCall(foo);
foo(true);
foo.prototype = foo;
foo(true);
