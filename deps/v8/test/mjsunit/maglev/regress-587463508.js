// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function isFoo(str, should_compare) {
  if (should_compare) return str === 'foo';
}
%PrepareFunctionForOptimization(isFoo);
isFoo('foo', true);
isFoo('bar', true);

const kStringsPerRound = 30;
const kRounds = 80;
const old_space_strings = [];
for (let round = 0; round < kRounds; round++) {
  const batch = [];
  for (let i = 0; i < kStringsPerRound; i++) {
    batch.push(['non_internalized', round, i].join('_'));
  }
  old_space_strings.push(batch);
}
// Promote the flat SeqOneByteStrings to old space so StringTable internalizes
// them in-place by transitioning their map word directly.
gc();

const intern_sink = {};
for (let round = 0; round < kRounds; round++) {
  const batch = old_space_strings[round];
  const params = ['should_compare'];
  const stores = [];
  const compares = [];
  for (let i = 0; i < kStringsPerRound; i++) {
    const global_name = `const_cell_${round}_${i}`;
    globalThis[global_name] = batch[i];
    params.push(`s${i}`);
    // Storing to a kConstant global PropertyCell sets s_i's checked_value to
    // HeapConstant(batch[i]) while batch[i] is still non-internalized.
    stores.push(`${global_name} = s${i};`);
    // Inlining isFoo calls GetInternalizedString(s_i), which checks batch[i]'s
    // live map, emits CheckedInternalizedString, and updates checked_value.
    compares.push(`isFoo(s${i}, should_compare);`);
  }

  const fn = new Function(...params, stores.concat(compares).join('\n'));
  %PrepareFunctionForOptimization(fn);
  fn(false, ...batch);
  %OptimizeMaglevOnNextCall(fn, 'concurrent');
  fn(false, ...batch);

  // Race main-thread in-place string internalization against background Maglev
  // graph building.
  for (let i = kStringsPerRound - 1; i >= 0; i--) {
    intern_sink[batch[i]] = 1;
  }
}
