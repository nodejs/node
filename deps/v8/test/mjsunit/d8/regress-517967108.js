// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestDisposingWorkerRealm() {
  const r1 = Realm.createAllowCrossRealmAccess();
  const r2 = Realm.createAllowCrossRealmAccess();
  Realm.switch(r1);

  let w = new Worker("postMessage('go');", { type: "string" });
  w.onmessage = function() {
    const r2_Error = Realm.eval(r2, "Error");
    assertThrows(() => {
      Realm.eval(r2, `Realm.dispose(${r1});`);
    }, r2_Error, /Invalid realm index/);
  };
})();
