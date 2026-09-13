// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestDisposingActiveParentRealm() {
  const r1 = Realm.create();
  const r2 = Realm.create();

  // code3 tries to dispose r2 (which is a parent in the call stack)
  const code3 = `
    Realm.dispose(${r2});
  `;

  // code2 runs code3 in r1, catches the error, and verifies r2 is still
  // usable. We use standard JS throw instead of mjsunit assertions because
  // mjsunit.js is not loaded in sub-realms.
  const code2 = `
    let threw = false;
    try {
      Realm.eval(${r1}, ${JSON.stringify(code3)});
    } catch (e) {
      threw = true;
      // The error message might be prefixed or contain "Error: ", so we
      // check if it contains the expected string.
      if (!e.message.includes("Invalid realm index")) {
        throw new Error("Expected 'Invalid realm index', got: " + e.message);
      }
    }
    if (!threw) {
      throw new Error("Expected Realm.dispose to throw");
    }

    // Re-entering r2 (which is this realm) should work.
    let res = Realm.eval(${r2}, "1 + 1");
    if (res !== 2) {
      throw new Error("Expected 2, got " + res);
    }
  `;

  const code1 = `
    Realm.eval(${r2}, ${JSON.stringify(code2)});
  `;

  Realm.eval(r1, code1);
})();
