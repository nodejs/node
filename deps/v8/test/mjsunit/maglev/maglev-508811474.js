// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --allow-natives-syntax

function makeSeqString(bigInt) {
  return bigInt.toString();
}

let x = 2512n;
let s0 = makeSeqString(x);
let s1 = makeSeqString(x);
let s2 = makeSeqString(x);
let s3 = makeSeqString(x);

%InternalizeString(s0);
%InternalizeString(s1);
%InternalizeString(s2);
%InternalizeString(s3);

function opt(x) {
  let o = {};
  o[s0] = -x;
  o[s1] = -x;
  o[s2] = -x;
  o[s3] = -x;
  return o;
}

%PrepareFunctionForOptimization(opt);
opt(1234n);
%OptimizeMaglevOnNextCall(opt, "concurrent");
opt(1234n);

// Use a randomized delay (1-30ms) to scan the timing spectrum.
// This makes the test highly robust and machine-independent, as it has a
// high chance of hitting the microscopic race window on any CPU.
// This delay is intentionally randomized to maximize the chance of flakily
// catching issues on different machines and CI buildbots.
const delay = Math.floor(Math.random() * 30) + 1;
print("Using random delay: " + delay + " ms");

const start = performance.now();
while (performance.now() - start < delay) {}

gc({type: "major"});
