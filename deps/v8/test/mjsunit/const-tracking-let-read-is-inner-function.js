// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --script-context-cells --allow-natives-syntax
// Flags: --turbofan --no-always-turbofan --maglev --no-stress-maglev
// Flags: --sparkplug --no-always-sparkplug

let a = 1;

let read;
function outer() {
  // Force this function have a context.
  let b = 0;
  read = function() { return a + b; };
}
outer();

%PrepareFunctionForOptimization(read);
assertEquals(1, read());

%OptimizeFunctionOnNextCall(read);
assertEquals(1, read());
assertOptimized(read);

function write(newA) {
  a = newA;
}

// Change what `a` is. This is executed in interpreter.
write(2);

assertUnoptimized(read);
assertEquals(2, read());
