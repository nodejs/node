// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --maglev-verify-dominance

function bar(o) {
  let o1 = { x : 42 };
  let o2 = { y : o1 };

  o.y = "abc"; // Polluting `y` field so that Maglev lightweight
               // load-elimination loses track of `o2.y`.

  let v = o2.y;

  o2.y = undefined;

  // At this point, {v} is {o1}, and returning it makes {o1} escape. The
  // CandidateAnalyzer must not wrongly think that {v} is `undefined` (which was
  // just stored to the property offset above).
  return v;
}

%PrepareFunctionForOptimization(bar);
assertEquals({x : 42}, bar({}));
assertEquals({x : 42}, bar({}));

%OptimizeFunctionOnNextCall(bar);
assertEquals({x : 42}, bar({}));
