// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log(42);

try {
  console.log(42);
} catch (e) {}

function foo() {
  return 0;
}

for (let i = 0; i < 10000; i++) {
  console.log(42);
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
