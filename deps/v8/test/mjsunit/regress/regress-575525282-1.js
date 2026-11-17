// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// The 32-bit word that optimized code hands to script as a tagged pointer (low bit set).
// Cage base + 0xdead0000 lies 3.48 GiB into V8's 4-GiB pointer-compression
// cage, far above everything this page allocates there.
const WORD = 0xdead0001;
// Compressed address of the read-only string "name".
const NAME = 0xe29;

// 1. Map setup
function f(flag) { if (flag) return { a: 1, b: { c: 2 } }; return 0; }
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

// 2. Object.prototype with more properties, one of them "name" and one element.
const A = Object.prototype;
A.name = 'TRUE';
A[0] = 7;

// 3. Object.prototype goes to dictionary mode: Object.assign from a source of
// more than 1020 properties normalizes its target first; the first getter
// throws, so nothing is stored.
const big = {};
Object.defineProperty(big, 'g0', { enumerable: true, get() { throw 1; } });
for (let i = 0; i < 1100; i++) big['p' + i] = i;
try { Object.assign(A, big); } catch (e) {}

// 4. The function under test:
// Every double of the array literal holds the two words NAME (low) and
// WORD (high).
const dv = new DataView(new ArrayBuffer(8));
dv.setUint32(0, NAME, true); dv.setUint32(4, WORD, true);
const h = new Function('A', 'flag',
    'A[0]; ' + // Map check
    'f(flag); ' + // Object literal
    // Big double array
    'var o = [' +
    Array(2048).fill(String(dv.getFloat64(0, true))).join(',') +
    ']; ' +
    'var x = A.name; ' + // Property load from an object whose map we already
                         // checked
    'return [x.length, o];');

// 5. Warm-up without the literal
%PrepareFunctionForOptimization(h);
h(A, false);
%OptimizeMaglevOnNextCall(h);

assertFalse(%HasFastProperties(A));

// 6. One call with the literal.
h(A, true);
assertUnoptimized(h);

// Creating the object literal optimized the prototype.
assertTrue(%HasFastProperties(A));
