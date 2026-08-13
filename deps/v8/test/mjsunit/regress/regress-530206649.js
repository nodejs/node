// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// The inlined localeCompare ASCII fast path (StringLocaleCompareIntl) elides
// the SeqOneByteString type guard for string operands that are
// SeqOneByteString constants. Such a constant can be internalized in place
// after compilation: String::MakeThin rewrites it as a ThinString without
// deopting, and the fast path would then read the ThinString's fields as
// characters (a type-confused, out-of-bounds read producing a wrong comparison
// result). The guard must only be elided for already-internalized constants,
// which are never thinned. On non-Intl builds the inline isn't emitted and the
// comparisons below just take the generic path.

// Warm Intl so localeCompare picks the inlineable fast path for the default
// locale.
assertEquals(0, "warmup".localeCompare("warmup"));

// Distinct 16-byte one-byte content per index (bits of i -> 'a'/'b'), built
// via fromCharCode so each is a fresh, non-internalized SeqOneByteString.
globalThis.mk = function(i) {
  const a = [];
  for (let j = 0; j < 16; j++) a.push(97 + ((i >> j) & 1));
  return String.fromCharCode.apply(null, a);
};

// Each key must be baked as a compile-time constant, which requires a distinct
// function (SFI) closing over a single constant. Generate them.
const N = 200;
globalThis.entries = [];
let src = '';
for (let i = 0; i < N; i++) {
  // Canonical twin, internalized first so `key` thins to it rather than
  // internalizing in place.
  src += `(new Map()).has(mk(${i}));`;
  src += `{ const key = mk(${i}), ref = mk(${i});`;
  src += ` entries.push({ f: () => key.localeCompare(ref), key }); }\n`;
}
eval(src);

for (const e of entries) {
  %PrepareFunctionForOptimization(e.f);
  e.f();
  e.f();
  %OptimizeFunctionOnNextCall(e.f);
  e.f();
  assertOptimized(e.f);
}

// Internalize every baked key in place, turning each into a ThinString.
const m = new Map();
for (const e of entries) m.has(e.key);

// Every comparison is between equal-content strings, so the result must be 0.
for (const e of entries) {
  assertEquals(0, e.f());
}
