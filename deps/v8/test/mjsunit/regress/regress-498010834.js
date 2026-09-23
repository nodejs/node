// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Use ~5500 property accesses to exceed the 16384 deopt entries limit
// without causing timeouts on slow builds.
let body = 'var sum = 0;\n';
for (let i = 0; i < 5500; i++) {
  body += `sum += o.p${i};\n`;
}
body += 'return sum;';

// Create the function with many unique property accesses
const f = new Function('o', body);

// Create an object with the properties
const obj = {};
for (let i = 0; i < 5500; i++) {
  obj['p' + i] = 1;
}

// Warm up the function
f(obj);
f(obj);

// Force Maglev compilation
%PrepareFunctionForOptimization(f);
f(obj);
%OptimizeMaglevOnNextCall(f);

try {
  f(obj);
} catch(e) {
  // Ignore error if it fails for other reasons
}
