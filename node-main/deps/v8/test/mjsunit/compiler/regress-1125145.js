// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --stack-limit=100

function foo() {}
for (let i = 0; i < 10000; ++i) {
  foo = foo.bind();
}

function main() {
  foo();
  foo();
}

%PrepareFunctionForOptimization(main);
main();
%OptimizeFunctionOnNextCall(main);
main();
