// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --expose-externalize-string

// A string constant baked into optimized code can change shape afterwards.
// Externalizing one in place swaps in an external string map and shrinks the
// object, while String::length keeps its old value. Optimized code that
// decided the constant was a SeqOneByteString and stopped re-checking then
// walks the shrunk object as if it still held characters.
//
// Optimize localeCompare against such constants, externalize them, and
// require the optimized answers to keep matching an unoptimized reference.

// Warm Intl so localeCompare picks the inlineable fast path.
assertEquals(0, 'warmup'.localeCompare('warmup'));

// Each constant needs its own function so the literal is baked as a
// HeapConstant. Contents must be distinct, or a later string internalizes to
// an earlier one and can then no longer be externalized on its own.
const N = 16;
globalThis.entries = [];
let src = '';
for (let i = 0; i < N; i++) {
  const s = String.fromCharCode(97 + Math.floor(i / 25), 98 + (i % 25));
  src += `entries.push({ key: "${s}",`;
  src += ` f: function(x) { return "${s}".localeCompare(x); } });\n`;
}
eval(src);

for (const e of entries) {
  %PrepareFunctionForOptimization(e.f);
  e.f('zz');
  e.f('zz');
  %OptimizeFunctionOnNextCall(e.f);
  e.f('zz');
  assertOptimized(e.f);
}

for (const e of entries) externalizeString(e.key);

// Two-character constants keep the misread inside the shrunk object, where it
// hits the resource field rather than memory past the end. With the sandbox
// that field holds (index << 6), so a stale read yields 0x00 or 0x40 followed
// by an index byte; enumerate that space so some argument matches it. Without
// the sandbox the field is a raw pointer and a match is unlikely, which only
// costs detection, never correctness.
const args = [];
for (const first of [0x00, 0x40]) {
  for (let second = 0; second < 128; second++) {
    args.push(String.fromCharCode(first, second));
  }
}

// The unoptimized builtin reads the externalized string correctly, so it is
// the oracle for what every optimized answer has to be.
const reference = (left, right) =>
    String.prototype.localeCompare.call(left, right);

for (const e of entries) {
  for (const arg of args) {
    assertEquals(Math.sign(reference(e.key, arg)), Math.sign(e.f(arg)),
                 `${JSON.stringify(e.key)} vs ${JSON.stringify(arg)}`);
  }
}
