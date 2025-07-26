// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --script-context-cells --allow-natives-syntax
// Flags: --turbofan --no-always-turbofan --maglev --no-stress-maglev
// Flags: --sparkplug --no-always-sparkplug

function read() { return x; }
%PrepareFunctionForOptimization(read);
try {
  // This throws because we're reading an uninitialized variable.
  read();
} catch (e) {
}
%OptimizeFunctionOnNextCall(read);
try {
  read();
} catch (e) {
}
%OptimizeFunctionOnNextCall(read);
let x = 1;
