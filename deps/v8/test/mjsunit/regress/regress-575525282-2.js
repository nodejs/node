// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// 1. Map setup
function f(flag) {
  if (flag) return {a: 1, b: {c: 2}};
  return 0;
}
%PrepareFunctionForOptimization(f);
f(true);
f(true);
const sib = { a: 1, z: 0 };

// Resulting maps:
//-> M1 (a: smi) -> M2 (a: smi, b: HeapObject)
//            \
//             ---> M3 (a: smi, z: smi)

// This deprecates all above maps.
sib.a = 1.5;

// 2. Object.prototype with more properties, one of them "x1" and one element.
//    Populate enough fast properties with ('x1', 'WRONG', 0) triples so that
//    when Object.prototype later transitions back from dictionary mode to fast
//    mode, a stale LoadDictionaryField for "x1" reads 'WRONG' in-bounds from
//    the new PropertyArray instead of 'TRUE'.
const A = Object.prototype;
A.x1 = 'TRUE';
A[0] = 7;
for (let i = 0; i < 80; i++) {
  A['k' + i] = (i % 3 === 0) ? 'x1' : (i % 3 === 1 ? 'WRONG' : 0);
}

// 3. Object.prototype goes to dictionary mode: Object.assign from a source of
// more than 1020 properties normalizes its target first; the first getter
// throws, so nothing is stored.
const big = {};
Object.defineProperty(big, 'g0', { enumerable: true, get() { throw 1; } });
for (let i = 0; i < 1100; i++) big['p' + i] = i;
try { Object.assign(A, big); } catch (e) {}

// 4. The function under test:
function h(A, flag) {
  A[0];         // Map check
  f(flag);      // Object literal
  return A.x1;  // Property load from an object whose map we already checked
}

// 5. Warm-up without the literal
%PrepareFunctionForOptimization(h);
assertEquals('TRUE', h(A, false));
%OptimizeMaglevOnNextCall(h);
assertEquals('TRUE', h(A, false));
assertMaglevved(h);

assertFalse(%HasFastProperties(A));

// 6. One call with the literal.
assertEquals('TRUE', h(A, true));
assertUnoptimized(h);

// Creating the object literal optimized the prototype.
assertTrue(%HasFastProperties(A));
