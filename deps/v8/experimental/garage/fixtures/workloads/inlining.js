// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Inlining decisions: small callees that should be inlined, plus one callee
// large enough to be rejected. Feeds the inlining tree (PLAN 7.7) and produces
// nested source positions inside a single Maglev graph.
function tiny(a) {
  return a + 1;
}

function small(a, b) {
  return tiny(a) * tiny(b);
}

function big(a) {
  let r = a;
  r += 1; r *= 3; r -= 7; r ^= 11; r |= 2; r &= 0xffff;
  r += 1; r *= 3; r -= 7; r ^= 11; r |= 2; r &= 0xffff;
  r += 1; r *= 3; r -= 7; r ^= 11; r |= 2; r &= 0xffff;
  r += 1; r *= 3; r -= 7; r ^= 11; r |= 2; r &= 0xffff;
  r += 1; r *= 3; r -= 7; r ^= 11; r |= 2; r &= 0xffff;
  r += 1; r *= 3; r -= 7; r ^= 11; r |= 2; r &= 0xffff;
  return r;
}

function outer(a, b) {
  return small(a, b) + big(a);
}

let acc = 0;
for (let i = 0; i < 100000; i++) {
  acc = outer(i & 0xff, (i >> 3) & 0xff) | 0;
}
print(acc);
