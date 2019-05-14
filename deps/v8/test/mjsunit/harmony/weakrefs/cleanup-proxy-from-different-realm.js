// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let r = Realm.create();

let cleanup = Realm.eval(r, "var stored_global; let cleanup = new Proxy(function() { stored_global = globalThis;}, {}); cleanup");
let realm_global_this = Realm.eval(r, "globalThis");

let fg = new FinalizationGroup(cleanup);

// Create an object and register it in the FinalizationGroup. The object needs
// to be inside a closure so that we can reliably kill them!
let weak_cell;

(function() {
  let object = {};
  fg.register(object, "holdings");

  // object goes out of scope.
})();

gc();

// Assert that the cleanup function was called in its Realm.
let timeout_func = function() {
  let stored_global = Realm.eval(r, "stored_global;");
  assertNotEquals(stored_global, globalThis);
  assertEquals(stored_global, realm_global_this);
}

setTimeout(timeout_func, 0);
