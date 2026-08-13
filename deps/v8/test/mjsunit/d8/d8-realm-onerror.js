// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test onerror in a Realm.

var realm = Realm.createAllowCrossRealmAccess();
var global = Realm.global(realm);

global.onerror = function (msg, source, lineno, colno, ex) {
  global.error_called = true;
  return true; // Suppress
};

// Use setTimeout to throw uncaught exception in the realm context.
Realm.eval(realm, "setTimeout(() => { throw new Error('foo'); }, 0);");

// Wait for the timeout to fire.
setTimeout(function check() {
  if (global.error_called) {
    // Exit using `quit` so that the failure from setTimeout isn't propagated
    // into the exit code.
    quit(0);
  }
  fail("onerror was not called");
}, 0);
