// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-turbofan
// Flags: --no-stress-maglev --no-always-sparkplug
// Flags: --no-concurrent-recompilation --no-baseline-batch-compilation
// Flags: --profile-guided-optimization
// Flags: --invocation-count-for-maglev=400
// Flags: --invocation-count-for-early-optimization=30

function runTest() {
  if (!%IsMaglevEnabled() || !%IsSparkplugEnabled()) return;

  // Positive control: inlining a stable callee allows the caller's SFI to be
  // promoted to kEarlyMaglev, so a second realm sharing the SFI tiers up early.
  const srcStable = `
    function sCallee(o) { return o.x + 1; }
    function sCaller(o) { return sCallee(o) + 1; }
  `;
  const rStable1 = Realm.createAllowCrossRealmAccess();
  const rStable2 = Realm.createAllowCrossRealmAccess();
  Realm.eval(rStable1, srcStable);
  Realm.eval(rStable2, srcStable);

  const sc1 = Realm.global(rStable1).sCallee;
  let limit = 10000;
  while (!%ActiveTierIsMaglev(sc1) && --limit > 0) sc1({x: 1});
  assertTrue(%ActiveTierIsMaglev(sc1));

  const sd1 = Realm.global(rStable1).sCaller;
  limit = 10000;
  while (!%ActiveTierIsMaglev(sd1) && --limit > 0) sd1({x: 1});
  assertTrue(%ActiveTierIsMaglev(sd1));

  const sc2 = Realm.global(rStable2).sCallee;
  sc2({x: 1});
  const sd2 = Realm.global(rStable2).sCaller;
  for (let i = 0; i < 60; i++) sd2({x: 1});
  assertTrue(%ActiveTierIsMaglev(sd2));

  // Regression case: inlining a callee that once deoptimized must prevent the
  // caller's SFI from being promoted to kEarlyMaglev across realms.
  const srcDeopt = `
    function callee(o) { return o.x + 1; }
    function caller(o) { return callee(o) + 1; }
  `;
  const rDeopt1 = Realm.createAllowCrossRealmAccess();
  const rDeopt2 = Realm.createAllowCrossRealmAccess();
  Realm.eval(rDeopt1, srcDeopt);
  Realm.eval(rDeopt2, srcDeopt);

  const c1 = Realm.global(rDeopt1).callee;
  limit = 10000;
  while (!%ActiveTierIsMaglev(c1) && --limit > 0) c1({x: 1});
  assertTrue(%ActiveTierIsMaglev(c1));
  c1({y: 1, x: 1.5});
  assertFalse(%ActiveTierIsMaglev(c1));

  const d1 = Realm.global(rDeopt1).caller;
  limit = 10000;
  while (!%ActiveTierIsMaglev(d1) && --limit > 0) d1({x: 1});
  assertTrue(%ActiveTierIsMaglev(d1));

  const c2 = Realm.global(rDeopt2).callee;
  c2({x: 1});
  const d2 = Realm.global(rDeopt2).caller;
  for (let i = 0; i < 60; i++) d2({x: 1});
  assertFalse(%ActiveTierIsMaglev(d2));
}
%NeverOptimizeFunction(runTest);
runTest();
