// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt --no-stress-flush-bytecode

// Check that there are no deopt loops for DataView methods.

var buffer = new ArrayBuffer(64);
var dataview = new DataView(buffer, 8, 24);

// Check DataView getters.

function readUint8(offset) {
  return dataview.getUint8(offset);
}

function warmupRead(f) {
  %PrepareFunctionForOptimization(f);
  f(0);
  f(1);
  %OptimizeFunctionOnNextCall(f);
  f(2);
  f(3);
}

warmupRead(readUint8);
assertOptimized(readUint8);
readUint8(0.5); // Deopts.
assertUnoptimized(readUint8);

warmupRead(readUint8);
assertOptimized(readUint8);
readUint8(1.5); // Doesn't deopt because getUint8 didn't get inlined this time.
assertOptimized(readUint8);

// Check DataView setters.

function writeUint8(offset, value) {
  dataview.setUint8(offset, value);
}

function warmupWrite(f) {
  %PrepareFunctionForOptimization(f);
  f(0, 0);
  f(0, 1);
  %OptimizeFunctionOnNextCall(f);
  f(0, 2);
  f(0, 3);
}

warmupWrite(writeUint8);
assertOptimized(writeUint8);
writeUint8(0.5, 0); // Deopts.
assertUnoptimized(writeUint8);

warmupWrite(writeUint8);
assertOptimized(writeUint8);
writeUint8(1.5, 0); // Doesn't deopt.
assertOptimized(writeUint8);
