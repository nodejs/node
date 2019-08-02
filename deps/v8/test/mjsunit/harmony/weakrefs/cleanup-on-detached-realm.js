// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let r = Realm.create();
let FG = Realm.eval(r, "FinalizationGroup");
Realm.detachGlobal(r);

let fg = new FG(()=> {
  assertUnreachable();
});

(() => {
  let object = {};
  fg.register(object, {});

  // object goes out of scope.
})();

gc();
