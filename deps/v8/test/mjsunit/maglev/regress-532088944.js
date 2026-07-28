// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-iterator-includes

const arr = [0.5, 1.5];

function* foo(n) {
  %OptimizeOsr();

  for (let i = 0; i < 5; i++) { }

  for (let j = 0; j < 5; j++) {
    function inner() { }
    yield 1;
  }

  arr.at(n);
}

%PrepareFunctionForOptimization(foo);
const x = foo(0);
x.includes();
foo(0).next();
