// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-typed-optimizations
// Flags: --no-use-std-math-pow

// The Turboshaft typer computes constant sets for `x ** y`. It must use the
// same pow implementation as the runtime, otherwise typed optimizations can
// fold comparisons on the result incorrectly.
//
// Each base below is an odd 18-bit integer divided by 2, so its cube lies
// exactly halfway between two doubles. A correctly rounded pow rounds these
// ties to even (the second column); a pow that is not correctly rounded (such
// as many system libms) picks either neighbor. If the typer's pow rounds
// differently than the runtime's, the typer believes `x ** 3` is strictly
// below or above the rounded value, and one of the two comparisons below gets
// folded to `true` even though the runtime value is equal.

const kCases = [
  [104035.5, 1126016297242739.0],
  [104062.5, 1126893218994140.5],
  [104161.5, 1130112493874283.5],
  [104165.5, 1130242694291086.5],
  [104167.5, 1130307798249422.0],
  [104169.5, 1130372904707777.5],
  [104177.5, 1130633355542359.5],
  [104187.5, 1130958975341797.0],
  [104191.5, 1131089240764736.0],
  [104195.5, 1131219516190059.0],
  [104215.5, 1131871043365874.0],
  [104221.5, 1132066550289288.5],
];

function test(body) {
  const f = new Function('b', body);
  %PrepareFunctionForOptimization(f);
  const expected = f(true);
  f(false);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(expected, f(true));
}

for (const [base, rounded] of kCases) {
  // The other phi input is far below `rounded`, so the comparison can only be
  // folded if the typer thinks `base ** 3` is below `rounded`.
  test(`
    let x = b ? ${base} : 2.5;
    return x ** 3 < ${rounded};
  `);
  // The other phi input is far above `rounded`, so the comparison can only be
  // folded if the typer thinks `base ** 3` is above `rounded`.
  test(`
    let x = b ? ${base} : 1000000.5;
    return x ** 3 > ${rounded};
  `);
}
