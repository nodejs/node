// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt


const big = 2n ** BigInt((2 ** 30)-1);


function testAdd(x, y) {
  return x + y;
}


%PrepareFunctionForOptimization(testAdd);
testAdd(3n, 7n);
testAdd(17n, -54n);
%OptimizeFunctionOnNextCall(testAdd);
assertEquals(testAdd(6n, 2n), 8n);
assertOptimized(testAdd);

assertThrows(() => testAdd(big, big), RangeError);
assertUnoptimized(testAdd);

%PrepareFunctionForOptimization(testAdd);
testAdd(30n, -50n);
testAdd(23n, 5n);
%OptimizeFunctionOnNextCall(testAdd);
assertEquals(testAdd(-7n, -12n), -19n);
assertOptimized(testAdd);

assertThrows(() => testAdd(big, big), RangeError);
assertOptimized(testAdd);
assertThrows(() => testAdd(big, big), RangeError);
assertOptimized(testAdd);
