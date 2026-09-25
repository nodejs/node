// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See FindWorthwhileInterval and the frequency_collator.
const encourage_boyer_moore = "abcdefghijklmnopqrstuvwxyz";

// Unfortunately there's currently no way to verify that we've generated a
// certain bytecode.
// TODO(jgruber): Add one if needed again in the future.

{
  const r = /<script|<style|<link/i;
  r.exec(encourage_boyer_moore);
  assertEquals(0, r.exec("<sTyle").index);
  assertEquals(4, r.exec("....<sTyle").index);
  assertEquals(16, r.exec("................<sTyle").index);
}

{
  const r = /aaabbb|aaaccc|aaddddd/i;
  r.exec(encourage_boyer_moore);
  assertEquals(0, r.exec("aaddddd").index);
  assertEquals(4, r.exec("....aaddddd").index);
  assertEquals(16, r.exec("................aaddddd").index);
}

{
  const r = /aaaabbb|aaaaccc|aaaddddd/i;
  r.exec(encourage_boyer_moore);
  {
    const s = "aaaabbb";
    assertEquals(0, r.exec("" + s).index);
    assertEquals(4, r.exec("...." + s).index);
    assertEquals(16, r.exec("................" + s).index);
  }
  {
    const s = "aaaaccc";
    assertEquals(0, r.exec("" + s).index);
    assertEquals(4, r.exec("...." + s).index);
    assertEquals(16, r.exec("................" + s).index);
  }
  {
    const s = "aaaddddd";
    assertEquals(0, r.exec("" + s).index);
    assertEquals(4, r.exec("...." + s).index);
    assertEquals(16, r.exec("................" + s).index);
  }
}

{
  // TODO(jgruber): Interesting pattern:
  // * A slightly different form causes us to miss the peephole opt.
  // * Inefficient single-char tail comparisons.
  const r = /aaabbbbbbbb|aaacccccccc|aadddddddd/i;
  r.exec(encourage_boyer_moore);
  const s = "aaabbbbbbbb";
  assertEquals(0, r.exec("" + s).index);
  assertEquals(4, r.exec("...." + s).index);
  assertEquals(16, r.exec("................" + s).index);
}
