// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Repro for cross-script UniqueIdInScript collision in
// ScopeIterator::HasContext when FindEvalOuterScope resets function_ and
// compares IDs from different scripts.
for (let pad = 0; pad < 40; pad++) {
  const s2 = 'function* gen(){ yield 1 }; gen()';
  const s1 = ' '.repeat(pad) + '({ [(g = eval(' + JSON.stringify(s2) +
      '), "k")]: 1, m() { return super.x } }); g';
  let g;
  const src = 'with ({}) { g = eval(' + JSON.stringify(s1) + '); }';
  eval(src);
  g.next();
  const n = %GetGeneratorScopeCount(g);
  for (let i = 0; i < n; i++) {
    %GetGeneratorScopeDetails(g, i);
  }
}

// Test nested function where inner function has no context, inspecting
// generator scopes.
{
  let g;
  function outer() {
    let captured = 42;
    function inner() {
      const s = 'function* gen(){ yield 1 }; gen()';
      g = eval(s);
    }
    inner();
    return captured;
  }
  outer();
  g.next();
  const n = %GetGeneratorScopeCount(g);
  for (let i = 0; i < n; i++) {
    %GetGeneratorScopeDetails(g, i);
  }
}
