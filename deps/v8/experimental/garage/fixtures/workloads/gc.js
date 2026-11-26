// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Allocation churn to force several scavenges (and ideally one major GC) so
// --trace-gc lines land interleaved with compiler output.
function makeGarbage(n) {
  let last = null;
  for (let i = 0; i < n; i++) {
    last = {a: i, b: [i, i + 1, i + 2], c: 'x' + (i & 0x3ff)};
  }
  return last;
}

const kept = [];
for (let round = 0; round < 40; round++) {
  const o = makeGarbage(20000);
  if (round % 8 === 0) kept.push(o);
}
print(kept.length);
