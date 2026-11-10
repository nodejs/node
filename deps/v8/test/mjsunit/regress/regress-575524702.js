// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Case 1: StoreGlobalIC / LoadGlobalIC warmed up BEFORE cross-origin
// Realm.navigate().
(function TestPreNavigationWarmup() {
  const r1 = Realm.createAllowCrossRealmAccess();

  const [setLeak, getLeak, RealmTypeError] = Realm.eval(r1, `
    Object.defineProperty(Object.prototype, "leak", {
      get: version,
      set: version,
      configurable: true
    });
    function setLeak(v) {
      leak = v;
    }
    function getLeak() {
      return leak;
    }
    for (let i = 0; i < 10; i++) {
      setLeak(i);
      getLeak();
    }
    [setLeak, getLeak, TypeError];
  `);

  Realm.navigate(r1);

  assertThrows(() => setLeak(1), RealmTypeError, "no access");
  assertThrows(() => getLeak(), RealmTypeError, "no access");
})();

// Case 2: StoreGlobalIC / LoadGlobalIC warmed up AFTER cross-origin
// Realm.navigate().
(function TestPostNavigationWarmup() {
  const r2 = Realm.createAllowCrossRealmAccess();

  const [setLeak, getLeak, RealmTypeError] = Realm.eval(r2, `
    Object.defineProperty(Object.prototype, "leak2", {
      get: version,
      set: version,
      configurable: true
    });
    function setLeak(v) {
      leak2 = v;
    }
    function getLeak() {
      return leak2;
    }
    [setLeak, getLeak, TypeError];
  `);

  Realm.navigate(r2);

  for (let i = 0; i < 10; i++) {
    assertThrows(() => setLeak(i), RealmTypeError, "no access");
    assertThrows(() => getLeak(), RealmTypeError, "no access");
  }
})();
