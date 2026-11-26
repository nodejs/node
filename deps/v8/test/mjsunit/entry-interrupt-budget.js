// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --turbofan --sparkplug
// Flags: --no-always-sparkplug --no-stress-maglev
// Flags: --no-concurrent-recompilation --no-concurrent-sparkplug
// Flags: --no-baseline-batch-compilation --no-profile-guided-optimization
// Flags: --invocation-count-for-maglev=100 --invocation-count-for-turbofan=100

// Async functions that suspend and are never resumed only reach
// SuspendGenerator, never Return. Their interrupt budget must still be
// charged so that they tier up.
const never = new Promise(() => {});

function tiersUp(f, isTier) {
  for (let i = 0; i < 5000; i++) {
    f();
    if (isTier(f)) return true;
  }
  return false;
}
%NeverOptimizeFunction(tiersUp);

(function TiersUpOnSuspend() {
  if (!%IsSparkplugEnabled() || !%IsMaglevEnabled()) return;
  async function f() {
    await never;
  }
  %CompileBaseline(f);
  assertTrue(%ActiveTierIsSparkplug(f));
  assertTrue(tiersUp(f, f => %ActiveTierIsMaglev(f)));
  if (!%IsTurbofanEnabled()) return;
  assertTrue(tiersUp(f, f => %ActiveTierIsTurbofan(f)));
})();

// Deeply recursive functions that haven't returned yet should also charge the
// interrupt budget on entry so inner calls tier up.
(function TiersUpOnRecursion() {
  if (!%IsSparkplugEnabled() || !%IsMaglevEnabled()) return;
  let reached_maglev = false;
  let reached_turbofan = false;
  function rec(n) {
    if (n === 0) return 0;
    if (%ActiveTierIsMaglev(rec)) reached_maglev = true;
    if (%ActiveTierIsTurbofan(rec)) reached_turbofan = true;
    return rec(n - 1) + 1;
  }
  %CompileBaseline(rec);
  assertTrue(%ActiveTierIsSparkplug(rec));
  rec(1500);
  assertTrue(reached_maglev);
  if (!%IsTurbofanEnabled()) return;
  assertTrue(reached_turbofan);
})();
