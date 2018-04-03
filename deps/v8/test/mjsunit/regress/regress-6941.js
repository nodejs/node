// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function foo(x) {
  return Symbol.iterator == x;
}

function main() {
  foo(Symbol());
  foo({valueOf() { return Symbol.toPrimitive}});
}

%NeverOptimizeFunction(main);
main();
%OptimizeFunctionOnNextCall(foo);
main();
assertOptimized(foo);
