// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanedUp = false;
let r = Realm.create();
let FG = Realm.eval(r, "FinalizationRegistry");
Realm.detachGlobal(r);

let fg_not_run = new FG(() => {
  assertUnreachable();
});
(() => {
  fg_not_run.register({});
})();

gc();

// Disposing the realm cancels the already scheduled fg_not_run's finalizer.
Realm.dispose(r);

let fg = new FG(()=> {
  cleanedUp = true;
});

// FGs that are alive after disposal can still schedule tasks.
(() => {
  let object = {};
  fg.register(object, {});

  // object becomes unreachable.
})();

gc();

setTimeout(function() { assertTrue(cleanedUp); }, 0);
